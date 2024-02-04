// (c) Copyright 2016-2022 Xilinx, Inc.
// Copyright (C) 2023, Advanced Micro Devices, Inc.
// All Rights Reserved.
//
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//===----------------------------------------------------------------------===//
/// \file
/// \brief This file implements parsing of all Xilinx directives and clauses.
/// '#pragma HLS|AP|AUTOPILOT kind named-arguments'.
///  kind:
///    identifer
///  named-arguments:
///    named-argument named-arguments[opt]
///
///  named-argument:
///    identifer
///    identifer '=' identifer-list
///    identifer '=' integer
///
///  identifer-list:
///    identifer
///    identifer ',' identifer-list[opt]
///
///
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/Lex/LexDiagnostic.h"
#include "clang/Lex/LiteralSupport.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/Parser.h"
#include "clang/Sema/Scope.h"
#include "clang/Sema/SemaInternal.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/XILINXFPGAPlatformBasic.h"
#include "llvm/Support/XILINXSystemInfo.h"
#include "clang/Basic/HLSDiagnostic.h"
#include "clang/Basic/HLSPragmaParser.inc"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>
#include <sstream>

using namespace clang;
using namespace llvm;

#define TABLEGEN_HLS 1

// TODO, following code is very ugly , FIXME
template <typename Pred>
static void TransferAttributes(ParsedAttributes &To, ParsedAttributes &From,
                               Pred ShouldTransfer) {
  AttributeList *CheckedList = nullptr;
  while (auto *Cur = From.getList()) {
    // Take out Cur from attrs
    auto *Next = Cur->getNext();
    From.set(Next);

    if (!ShouldTransfer(Cur)) {
      // Put Cur to CheckedList
      Cur->setNext(CheckedList);
      CheckedList = Cur;
      continue;
    }

    // Otherwise add to the To
    Cur->setNext(nullptr);
    auto *newAttr = To.clone(*Cur);
    newAttr->setPragmaContext(Cur->getPragmaContext()); 
  }

  // Put the checked list back to From.
  From.set(CheckedList);
}

static bool ShouldSinkFromLabel(AttributeList *A) {
  switch (A->getKind()) {
  default:
    return false;
  case AttributeList::AT_OpenCLUnrollHint:
  case AttributeList::AT_XCLPipelineLoop:
  case AttributeList::AT_XCLDataFlow:
  case AttributeList::AT_XCLFlattenLoop:
  case AttributeList::AT_XCLLoopTripCount:
  case AttributeList::AT_XCLLatency:
  case AttributeList::AT_XlxDependence:
  case AttributeList::AT_XlxArrayStencil:
  case AttributeList::AT_XlxStable:
  case AttributeList::AT_XlxStableContent:
  case AttributeList::AT_XlxShared:
  case AttributeList::AT_XlxDisaggr:
  case AttributeList::AT_XlxAggregate:
    return true;
  }
}
/*
 * sink attribute wrapping the label to the stmt
 * __attribute__((xlx_latency))
 * label_name:
 * {
 *   a = b * c;
 *   b = a * d;
 * }
 *
 * will sink xlx_latency to CompoundStmt after "label_name"
 *
 */
void Parser::SinkLabelAttributes(ParsedAttributesWithRange &To,
                                 ParsedAttributesWithRange &From,
                                 const Token &IdentTok) {
  TransferAttributes(To, From, ShouldSinkFromLabel);

  auto *AttrName = getPreprocessor().getIdentifierInfo("xcl_region");
  auto Loc = IdentTok.getLocation();
  ArgsUnion Arg[] = {IdentifierLoc::create(
      Actions.Context, IdentTok.getLocation(), IdentTok.getIdentifierInfo())};
  To.addNew(AttrName, Loc, nullptr, Loc, Arg, array_lengthof(Arg),
            AttributeList::AS_GNU);
}

// Only for internal debugging usage:
static bool enableXilinxPragmaChecker() {
  char *tmp = std::getenv("XILINX_SCOUT_HLS_DISABLE_PRAGMA_CHECKER");
  if (tmp == NULL)
    return true;
  std::string getEnvInfo = "";
  getEnvInfo = tmp;
  if (getEnvInfo == "yes")
    return false;
  else
    return true;
}

static bool HasDataflowAttributeInternal(AttributeList *List) {
  bool HasDataflow = false;
  for (auto *A = List; A; A = A->getNext())
    HasDataflow |= A->isXCLDataflowAttribute();
  return HasDataflow;
}

bool Parser::HasDataflowAttribute(AttributeList *List) {
  return HasDataflowAttributeInternal(List);
}

static bool ShouldHoistFromScope(AttributeList *A) {
  switch (A->getKind()) {
  default:
    return false;
  case AttributeList::AT_OpenCLUnrollHint:
  case AttributeList::AT_XCLDataFlow:
  case AttributeList::AT_XCLPipelineLoop:
  case AttributeList::AT_XlxDependence:
  case AttributeList::AT_XlxArrayStencil:
  case AttributeList::AT_XCLFlattenLoop:
  case AttributeList::AT_XCLLoopTripCount:
    return true;
  }
}

void Parser::RemoveDataflowAttribute(ParsedAttributes &From) {
  ParsedAttributes EmptyAttrs(From.getPool().getFactory());
  TransferAttributes(EmptyAttrs, From, [](AttributeList *A) {
    return A->getKind() == AttributeList::AT_XCLDataFlow ? true : false;
  });
  auto A = EmptyAttrs.getList();
  while (A) {
    this->Diag(A->getLoc(), diag::warn_remove_xcl_dataflow_attr)
        << FixItHint::CreateRemoval(A->getRange());
    A = A->getNext();
  }
}

void Scope::hoistParsedHLSPragmas(ParsedAttributes &Dst) {
  TransferAttributes(Dst, ParsedHLSPragmas, ShouldHoistFromScope);
}

static bool IsContinue(Scope *S) {
  return S->getFlags() & Scope::ContinueScope;
}

static bool IsHoistScope(Scope *&S) {
  if (!S->isCompoundStmtScope())
    return false;

  // Hoist to continue scope, e.g. the "for/while/do" statement
  S = S->getParent();
  while (S && !IsContinue(S) && !S->isCompoundStmtScope())
    S = S->getParent();

  // Do not hoist across compound statement scope
  if (!S || S->isCompoundStmtScope())
    return false;

  return true;
}

void Scope::hoistParsedHLSPragmas() {
  auto P = this;
  if (IsHoistScope(P)) {
    hoistParsedHLSPragmas(P->getParsedHLSPragmasRef());
  }
}

static AttributeList *IsRegionUnrollScope(AttributeList *A) {
  while (A) {
    if (A->getKind() == AttributeList::AT_XlxUnrollRegionHint)
      return A;
    A = A->getNext();
  }
  return nullptr;
}

/// SinkParsedHLSUnrollPragmas - Sink parsed HLS region unroll pragmas from
/// parent scope to subloops
void Parser::SinkParsedHLSUnrollPragmas(ParsedAttributesWithRange &To,
                                        Scope *P) {
  auto *RegionUnrollAttr = IsRegionUnrollScope(P->getParsedHLSPragmas());
  if (RegionUnrollAttr && IsHoistScope(P)) {
    auto ArgNum = RegionUnrollAttr->getNumArgs();
    ArgsVector Args;
    for (unsigned i = 0; i < ArgNum; i++)
      Args.emplace_back(RegionUnrollAttr->getArg(i));
    auto *AttrName = getPreprocessor().getIdentifierInfo("opencl_unroll_hint");

    To.addNew(AttrName, RegionUnrollAttr->getLoc(), nullptr,
              RegionUnrollAttr->getScopeLoc(), &Args[0], ArgNum,
              AttributeList::AS_GNU);
  }
}

//===----------------------------------------------------------------------===//
// Xilinx declarative directives.
//===----------------------------------------------------------------------===//

class XlxPragmaArgParser;

typedef ArgsUnion  (*CallBackParserFunc)(XlxPragmaArgParser& PAP, Parser &P, SourceLocation PragmaLoc);
/// it is used to hold pragma param value
struct XlxPragmaParam {
  enum Type {
    Unknown = 0,
    Id,
    Enum,
    ICEExpr,
    VarRefExpr,
    PresentID,
    CallBackParser,
    DCEExpr,
  };

  Type T = Unknown;
  bool Required = false;

  union {
    StringRef S;
    int64_t Int;
    // TODO, rename it to iceValue,  it is only used to restore ICE option
    Expr *VarRef;

    // it is used to help check error , if pragma param  can be "value_a,
    // value_b, and value_c, ...." these values are exclusive for each other,
    // given these presentId("value_x" , exlcuded_gorup_id ) XlxParser::parse will
    // check it , avoid some error such as #pragma name,  value_a, value_c
    unsigned PresentGroup;
    CallBackParserFunc callback;
    double Double;
  };
  SmallVector<StringRef, 4> EnumVals;

  XlxPragmaParam():T(Unknown) {}

  XlxPragmaParam(bool Required, StringRef S)
      : T(Id), Required(Required), S(S) {}

  XlxPragmaParam(bool Required, int64_t Int)
      : T(ICEExpr), Required(Required), Int(Int) {}

  XlxPragmaParam(bool Required, std::initializer_list<StringRef> Vals)
      : T(Enum), Required(Required), EnumVals(Vals) {}

  XlxPragmaParam(bool Required, Expr *expr)
      : T(VarRefExpr), Required(Required), VarRef(expr) {}

  XlxPragmaParam(unsigned Group) : T(PresentID), PresentGroup(Group) {}

  XlxPragmaParam(bool Required, CallBackParserFunc callback) 
      : T(CallBackParser), Required(Required), callback(callback) { }

  XlxPragmaParam(bool Required, double Double)
      : T(DCEExpr), Required(Required), Double(Double) {}
};

static std::pair<StringRef, XlxPragmaParam> reqId(StringRef Name) {
  return {Name, XlxPragmaParam(true, "")};
}

static std::pair<StringRef, XlxPragmaParam> optId(StringRef Name,
                                                  StringRef S = "") {
  return {Name, XlxPragmaParam(false, S)};
}

static std::pair<StringRef, XlxPragmaParam> reqICEExpr(StringRef Name,
                                                       int64_t i = 0) {
  return {Name, XlxPragmaParam(true, i)};
}

static std::pair<StringRef, XlxPragmaParam> optICEExpr(StringRef Name,
                                                       int64_t i = 0) {
  return {Name, XlxPragmaParam(false, i)};
}

static std::pair<StringRef, XlxPragmaParam> reqVarRefExpr(StringRef Name,
                                                          Expr *e = nullptr) {
  return {Name, XlxPragmaParam(true, (Expr *)NULL)};
}

static std::pair<StringRef, XlxPragmaParam> optVarRefExpr(StringRef Name,
                                                          Expr *e = nullptr) {
  return {Name, XlxPragmaParam(false, (Expr *)NULL)};
}

static std::pair<StringRef, XlxPragmaParam> presentId(StringRef Name,
                                                      unsigned Group = 0) {
  return {Name, XlxPragmaParam(Group)};
}

static std::pair<StringRef, XlxPragmaParam> optCallBackParser(StringRef Name, CallBackParserFunc callback) { 
  return {Name, XlxPragmaParam(false, callback)};
}
static std::pair<StringRef, XlxPragmaParam> reqCallBackParser(StringRef Name, CallBackParserFunc callback) { 
  return {Name, XlxPragmaParam(true,  callback)};
}

static std::pair<StringRef, XlxPragmaParam> optDCEExpr(StringRef Name, double Double = 0.0) { 
  return {Name, XlxPragmaParam(false, Double)};
}
static std::pair<StringRef, XlxPragmaParam> reqDCEExpr(StringRef Name, double Double = 0.0) { 
  return {Name, XlxPragmaParam(true,  Double)};
}

static std::pair<StringRef, XlxPragmaParam>
optEnum(StringRef Name, std::initializer_list<StringRef> Vals) {
  return {Name, XlxPragmaParam(false, Vals)};
}

typedef SmallVector<std::pair<Decl *, SourceLocation>, 4> SubjectListTy;

class XlxPragmaArgParser {
  ParsedAttributes &ScopeAttrs;
  ParsedAttributes &DependenceAttrs;

  // TODO, SubjectList is very ugly design, we should not do Sematic action :
  // binding attribute with variable by process
  // now, it is only used by Interface pragma parsing
  SubjectListTy &SubjectList;

  bool ApplyToFunction;

  StringMap<XlxPragmaParam> NamedParams;
  SmallVector<StringRef, 4> ParamList;

  // this is used to help parse  some special pragma param
  // some pragma param will impact the pragma parse behavior
  // currently example"interface::mode" pragma param will affect
  // the set of valid param values for other pragma params
  StringRef SubjectParam;
  Expr* ifCond; 
  StringMap<ArgsUnion> ArgMap;
  StringMap<IdentifierLoc *> PresentedID;

  SourceLocation PragmaLoc;
  SourceRange PragmaRange;

  VarDecl *parseSubject();

public:
  Parser &P;
  Preprocessor &PP;
  IdentifierInfo *pragmaContext; 

  XlxPragmaArgParser(
      Parser &P, Scope *CurScope, SubjectListTy &Subjects, 
      IdentifierInfo* pragmaContext, 
      Expr* ifCond
      )
      : P(P), PP(P.getPreprocessor()), pragmaContext(pragmaContext), ifCond(ifCond),  SubjectList(Subjects), 
        ScopeAttrs(CurScope->getParsedHLSPragmasRef()),
        DependenceAttrs(CurScope->getDependencePragmasRef())
  { 

  }

  void setPragmaOption(
    StringRef SubjectParam, bool ApplyToFunction,
      std::initializer_list<std::pair<StringRef, XlxPragmaParam>> List,
      SourceLocation PragmaLoc) { 

    this->ApplyToFunction = ApplyToFunction; 
    this->NamedParams = List; 
    this->SubjectParam = SubjectParam; 
    this->PragmaLoc = PragmaLoc ; 

    for (const auto &P : List)
      ParamList.push_back(P.first);
  }

  AttributeList *createAttribute(StringRef Name,
                                 MutableArrayRef<ArgsUnion> Args,
                                 AttributeList *List = nullptr) {

    auto *II = PP.getIdentifierInfo(Name);
    auto &Pool = ScopeAttrs.getPool();
    auto *A = Pool.create(II, PragmaRange, nullptr, PragmaLoc, Args.data(),
                          Args.size(), AttributeList::AS_GNU);
    //is the pragma from 'ext'(external tool auto generating) or from 'user'
    //this is used to mark the source of the pragma 
    A->setPragmaContext( pragmaContext); 
    A->setHLSIfCond(ifCond); 
    A->setNext(List);
    return A;
  }

  void addAttribute(StringRef Name, MutableArrayRef<ArgsUnion> Args) {
    ScopeAttrs.add(createAttribute(Name, Args));
  }

  void addDependenceAttribute(StringRef Name, MutableArrayRef<ArgsUnion> Args) {
    DependenceAttrs.add(createAttribute(Name, Args));
  }

  bool parse();

  IdentifierLoc *parseIdentifierLoc();
  IdentifierLoc *parseEnumIdentifier(const XlxPragmaParam &param);
  Expr *parseICEExpression();
  Expr *parseVarRefExpression(StringRef optionName);

  ArrayRef<std::pair<Decl *, SourceLocation>> subjects() const {
    return SubjectList;
  }

  IdentifierLoc *createIdentLoc(StringRef S,
                                SourceLocation Loc = SourceLocation()) const {
    auto &Ctx = P.getActions().getASTContext();

    auto Ident = PP.getIdentifierInfo(S);
    return IdentifierLoc::create(Ctx, Loc, Ident);
  }

  IdentifierLoc *createIdentLoc(IdentifierInfo *II,
                                SourceLocation Loc = SourceLocation()) const {
    auto &Ctx = P.getActions().getASTContext();
    return IdentifierLoc::create(Ctx, Loc, II);
  }

  IntegerLiteral *
  createIntegerLiteral(int64_t i, SourceLocation Loc = SourceLocation()) const {
    auto &Ctx = P.getActions().getASTContext();
    auto IntTy = Ctx.IntTy;
    auto Width = Ctx.getIntWidth(IntTy);
    auto Int = APInt(Width, i);
    return IntegerLiteral::Create(Ctx, Int, IntTy, Loc);
  }

  clang::StringLiteral *
  createStringLiteral(StringRef S, 
                      SourceLocation Loc = SourceLocation()) const {
    auto &Ctx = P.getActions().getASTContext();
    QualType CharTy = Ctx.CharTy;
    CharTy.addConst();

    // Get an array type for the string, according to C99 6.4.5.  This includes
    // the nul terminator character as well as the string length for pascal
    // strings.
    QualType StrTy = Ctx.getConstantArrayType(CharTy,
                         llvm::APInt(32, S.size()+1),
                         clang::ArrayType::Normal, 0);


    return clang::StringLiteral::Create(
               Ctx, S, clang::StringLiteral::Ascii, false, StrTy, &Loc, 1);
  }

  FloatingLiteral *
  createFloatingLiteral(double Val, SourceLocation Loc = SourceLocation()) const {
    auto &Ctx = P.getActions().getASTContext();
    return FloatingLiteral::Create(Ctx, APFloat(Val), true, Ctx.DoubleTy, Loc);
  }

  ArgsUnion lookup(StringRef Param) const { return ArgMap.lookup(Param); }

  ArgsUnion operator[](StringRef Param) const {

    auto ParamInfo = NamedParams.lookup(Param);

    if (auto Arg = ArgMap.lookup(Param)) { 
      assert(ParamInfo.T != XlxPragmaParam:: Unknown && "Unexpected" ) ;
      if (ParamInfo.T == XlxPragmaParam::PresentID) { 
        return createIntegerLiteral(1, Arg.get<IdentifierLoc*>()->Loc);
      }
      else { 
        return Arg;
      }
    }

    // TODO, report diagnostic error message
    assert(!ParamInfo.Required && "Missing required argument!");

    auto Loc = PragmaLoc;
    // Create the default value
    switch (ParamInfo.T) {
    case XlxPragmaParam::Id:
      return createIdentLoc(ParamInfo.S, Loc);
    case XlxPragmaParam::Enum:
      return createIdentLoc(ParamInfo.EnumVals[0], Loc);
    case XlxPragmaParam::ICEExpr:
      return createIntegerLiteral(ParamInfo.Int, Loc);
    case XlxPragmaParam::DCEExpr:
      return createFloatingLiteral(ParamInfo.Double, Loc);
    case XlxPragmaParam::VarRefExpr:
      return ArgsUnion((Expr *)nullptr);
    case XlxPragmaParam::PresentID: 
      // TODO, "presentId" class of option should be evaluated as "IdentifierLoc"
      // because Sema need IdentifierLoc to report precise source location, and
      // name
      return createIntegerLiteral(0, Loc);

    default:
      llvm_unreachable("unexpected  ParamInfo.Type");
    }
  }

  bool parseSubjectList();

  IdentifierLoc *presentedId(StringRef Name) const {
    return PresentedID.lookup(Name);
  }
  bool CheckAndFilter(StringMap<XlxPragmaParam> ParamMap);

  ArgsUnion parseOptionalEnumVal(
    StringRef Option, ArrayRef<StringRef> EnumVals, StringRef Implicit)
  {
    if (P.TryConsumeToken(tok::equal)) {
      const Token &Tok = P.getCurToken();
      if (Tok.is(tok::annot_pragma_XlxHLS_end)) {
        P.Diag(Tok.getLocation(), diag::warn_pragma_named_argument_missing) << Option;
        return ArgsUnion();
      }
      else {
        IdentifierInfo *II = Tok.getIdentifierInfo();
        SourceLocation Loc = P.ConsumeToken();
        return createIdentLoc(II, Loc);
      }
    }
    else {
      return createIdentLoc(Implicit);
    }
  }

  bool getOff()
  {
    return !lookup("off").isNull();
  }
};


static void getSubExprOfVariable(SmallVector<Expr *, 4> &subExprs, Expr *var_expr,
                                 Parser &P) {
  // parse "expr1, expr2, expr3"
  while (isa<BinaryOperator>(var_expr) &&
         dyn_cast<BinaryOperator>(var_expr)->getOpcode() == BO_Comma) {

    auto bin_expr = dyn_cast<BinaryOperator>(var_expr);
    assert(!isa<BinaryOperator>(bin_expr->getRHS()) && "unexpected, it is not valid variable expressions format");

    Expr *leaf = bin_expr->getRHS();
    // stupid test case using  "variable = &stream_variable", I don't know why
    if (isa<UnaryOperator>(leaf) &&
        dyn_cast<UnaryOperator>(leaf)->getOpcode() == UO_AddrOf) {
      UnaryOperator *up = dyn_cast<UnaryOperator>(leaf);
      leaf = up->getSubExpr();
      P.Diag(up->getExprLoc(), diag::warn_extra_tokens_before_variable_expression)
        << FixItHint::CreateRemoval(up->getExprLoc());
    }
    if (isa<ImplicitCastExpr>(leaf)) {
      ImplicitCastExpr *cast = dyn_cast<ImplicitCastExpr>(leaf);
      leaf = cast->getSubExpr();
    }
    subExprs.push_back(leaf);
    var_expr = bin_expr->getLHS();
  }

  if (isa<UnaryOperator>(var_expr) &&
      dyn_cast<UnaryOperator>(var_expr)->getOpcode() == UO_AddrOf) {
    UnaryOperator *up = dyn_cast<UnaryOperator>(var_expr);
    P.Diag(up->getExprLoc(), diag::warn_extra_tokens_before_variable_expression)
      << FixItHint::CreateRemoval(up->getExprLoc());
    var_expr = up->getSubExpr();
  }

  if (isa<ImplicitCastExpr>(var_expr)) {
    ImplicitCastExpr *cast = dyn_cast<ImplicitCastExpr>(var_expr);
    var_expr = cast->getSubExpr();
  }
  subExprs.push_back(var_expr);
  //reverse the order of subExprs , because COMMA_ operator  is Left Hand Side first 
  for (int i = 0; i < subExprs.size() / 2; i++) { 
    std::swap(subExprs[i], subExprs[ subExprs.size() - 1 - i ]);
  }
}

static StringRef str(const IdentifierLoc *Id) { return Id->Ident->getName(); }

static StringRef str(const IdentifierLoc &Id) { return Id.Ident->getName(); }

static IdentifierLoc ParseXlxPragmaArgument(Parser &P) {
  const auto &Tok = P.getCurToken();
  auto &PP = P.getPreprocessor();

  // Return null when hit pragma end
  if (Tok.is(tok::annot_pragma_XlxHLS_end))
    return {SourceLocation(), nullptr};

  // Do not fail on #pragma HLS inline
  if (!Tok.isOneOf(tok::identifier, tok::kw_inline, tok::kw_register,
                   tok::kw_auto, tok::kw_false, tok::kw_true, tok::kw_class)) {
    P.Diag(Tok, diag::warn_unexpected_token_in_pragma_argument)
        << PP.getSpelling(Tok) << tok::identifier;
    return {SourceLocation(), nullptr};
  }

  auto Name = Tok.getIdentifierInfo();
  auto Loc = P.ConsumeToken();

  return {Loc, Name};
}

static IdentifierLoc TryConsumeWords(Parser &P, ArrayRef<StringRef> Words) {
  auto &Tok = P.getCurToken();
  if (Tok.isNot(tok::identifier))
    return {SourceLocation(), nullptr};

  auto *II = Tok.getIdentifierInfo();

  if (llvm::any_of(Words,
                   [II](StringRef S) { return S.equals_lower(II->getName()); }))
    return {P.ConsumeToken(), II};

  return {SourceLocation(), nullptr};
}

Expr *XlxPragmaArgParser::parseVarRefExpression(StringRef optionName) {
  auto &Actions = P.getActions();

  const auto &Tok = P.getCurToken();
  // ExprResult ArgExpr = P.ParseAssignmentExpression();
  // ExprResult ArgExpr = P.ParseExpression();

  ExprResult ArgExpr = P.ParseHLSVariableExpression(optionName);

  ArgExpr = Actions.CorrectDelayedTyposInExpr(ArgExpr);
  if (!ArgExpr.isInvalid()) {
    Expr *expr = ArgExpr.get();

    return expr;
  } else {
    return nullptr;
  }
}

VarDecl *XlxPragmaArgParser::parseSubject() {
  // Skip address_of or dererference
  SourceLocation Loc;
  bool SkippedSomthing =
      P.TryConsumeToken(tok::amp, Loc) || P.TryConsumeToken(tok::star, Loc);

  auto &Actions = P.getActions();
  ExprResult ArgExpr = P.ParseAssignmentExpression();
  ArgExpr = Actions.CorrectDelayedTyposInExpr(ArgExpr);
  auto *DeclRef = dyn_cast_or_null<DeclRefExpr>(ArgExpr.get());
  if (!DeclRef)
    return nullptr;

  if (auto *D = dyn_cast<VarDecl>(DeclRef->getDecl())) {
    if (SkippedSomthing)
      P.Diag(Loc, diag::warn_extra_tokens_before_variable_expression)
          << FixItHint::CreateRemoval(Loc);
    return D;
  }

  return nullptr;
}

IdentifierLoc *
XlxPragmaArgParser::parseEnumIdentifier(const XlxPragmaParam &Param) {
  auto &Tok = P.getCurToken();
  if (!Tok.isOneOf(tok::identifier, tok::string_literal, tok::kw_auto, tok::kw_true, tok::kw_false)) {
    P.Diag(Tok, diag::warn_unexpected_token_in_pragma_argument)
        << PP.getSpelling(Tok) << tok::identifier;
    return nullptr;
  }

  IdentifierInfo *Ident;
  if (Tok.is(tok::string_literal)) {
    StringLiteralParser X(Tok, P.getPreprocessor());
    StringRef str = X.hadError ? "" : X.GetString();
    Ident = P.getPreprocessor().getIdentifierInfo(str);
  } else if (Tok.isOneOf(tok::kw_auto, tok::kw_true, tok::kw_false)) {
    Ident = Tok.getIdentifierInfo(); 
  } else
    Ident = Tok.getIdentifierInfo();

  auto *IdLoc = createIdentLoc(Ident, Tok.getLocation());
  P.ConsumeAnyToken();

  auto EnumVal = str(IdLoc);
  std::string EnumStr;
  if (llvm::none_of(Param.EnumVals, [EnumVal, &EnumStr](StringRef S) {
        EnumStr += S.str() + '/';
        return S.equals_lower(EnumVal);
      })) {
    P.Diag(IdLoc->Loc, diag::warn_unexpected_token_in_pragma_argument)
        << EnumVal << EnumStr;
    return nullptr;
  }
  return IdLoc;
}

IdentifierLoc *XlxPragmaArgParser::parseIdentifierLoc() {
  auto &Tok = P.getCurToken();
  if (!Tok.isOneOf(tok::identifier, tok::string_literal, tok::kw_auto)) {
    P.Diag(Tok, diag::warn_unexpected_token_in_pragma_argument)
        << PP.getSpelling(Tok) << tok::identifier;
    return nullptr;
  }

  IdentifierInfo *Ident;
  if (Tok.is(tok::string_literal)) {
    StringLiteralParser X(Tok, P.getPreprocessor());
    StringRef str = X.hadError ? "" : X.GetString();
    Ident = P.getPreprocessor().getIdentifierInfo(str);
  } else if (Tok.is(tok::kw_auto)) {
    Ident = P.getPreprocessor().getIdentifierInfo("auto");
  } else
    Ident = Tok.getIdentifierInfo();

  auto *IL = createIdentLoc(Ident, Tok.getLocation());
  P.ConsumeAnyToken();

  return IL;
}

// following function is important  to assume that
// attribute's option is constant scalar in CodeGen
Expr *XlxPragmaArgParser::parseICEExpression() {
  auto &Actions = P.getActions();
  ExprResult ArgExpr = P.ParseConstantExpression();
  ArgExpr = Actions.CheckOrBuildPartialConstExpr(ArgExpr.get());
  ArgExpr = Actions.CorrectDelayedTyposInExpr(ArgExpr);
  return ArgExpr.get();
}

bool XlxPragmaArgParser::parseSubjectList() {
  auto &Actions = P.getActions();

  auto &Tok = P.getCurToken();
  auto Loc = Tok.getLocation();

  // Return means the parent function
  if (P.TryConsumeToken(tok::kw_return)) {
    if (!ApplyToFunction) {
      P.Diag(Tok, diag::warn_unexpected_token_in_pragma_argument)
          << tok::kw_return << tok::identifier;
      return false;
    }

    auto *FD = Actions.getCurFunctionDecl();
    if (!FD)
      return false;

    SubjectList.emplace_back(FD, Loc);
  } 
  else if (P.TryConsumeToken(tok::kw_void))
    /*P.Diag(Loc, diag::warn_extra_pragma_hls_token_ignored)*/;
  else if (auto *Var = parseSubject())
    SubjectList.emplace_back(Var, Loc);
  else
    return false;

  if (P.TryConsumeToken(tok::comma))
    return parseSubjectList();

  return true;
}

bool XlxPragmaArgParser::CheckAndFilter(StringMap<XlxPragmaParam> ParamMap) 
{
  for( auto &kv : ArgMap) {
    StringRef name = kv.getKey();
    ArgsUnion arg = kv.getValue();
    if (arg.isNull()){
      //it is prsentedId
      continue;
    }
    auto parm = ParamMap.lookup( name);
    switch(parm.T) 
    {
      case XlxPragmaParam::Unknown:
      {
        SourceLocation loc ; 
        if (arg.is<IdentifierLoc*>()) 
          loc = arg.get<IdentifierLoc*>()->Loc;
        else if (arg.is<Expr*>()) { 
          loc = arg.get<Expr*>()->getExprLoc();
        }
        P.Diag(loc, diag::warn_unexpected_pragma_parameter) << name;
      //Diagnostic
        return false;
      }
      case XlxPragmaParam::Id: 
      case XlxPragmaParam::Enum:
      case XlxPragmaParam::ICEExpr:
      case XlxPragmaParam::VarRefExpr:
      break;
    }
  }
  for( auto &kv : PresentedID){
    StringRef name = kv.getKey();
    IdentifierLoc* arg = kv.getValue();
    auto parm = ParamMap.lookup( name);
    switch(parm.T) 
    {
      case XlxPragmaParam::Unknown:
      //Diagnostic
        P.Diag(arg->Loc, diag::warn_unexpected_pragma_parameter) << name;
        return false;
      case XlxPragmaParam::Id: 
      case XlxPragmaParam::Enum:
      case XlxPragmaParam::ICEExpr:
      case XlxPragmaParam::VarRefExpr:
        //Diagnostic
        return true;
      case XlxPragmaParam::PresentID:
        continue;
        break;
    }
  }
  return true;
}

// parse all pragma according NamedParams
// and save result into  ArgMap
// FYI,  Subject, Enum, ICEExpr, VarRefExpr  using synatx grammar: name = value
// while present is used  value, no  tok::equal
bool XlxPragmaArgParser::parse() {
  DenseMap<unsigned, IdentifierLoc> PresentGroups;
  while (P.getCurToken().isNot(tok::annot_pragma_XlxHLS_end)) {
    // Parse each named argument:
    ///  named-argument:
    ///    identifer
    ///    identifer '=' identifer
    ///    identifer '=' integer
    auto MaybeArg = ParseXlxPragmaArgument(P);
    if (!MaybeArg.Ident)
      return false;

    auto ArgName = str(MaybeArg);

    if (ArgName.equals_lower(SubjectParam)) {
      if (!P.TryConsumeToken(tok::equal)) {
        const auto &Tok = P.getCurToken();
        P.Diag(Tok, diag::warn_unexpected_token_in_pragma_argument)
            << PP.getSpelling(Tok) << tok::equal;
        return false;
      }

      if (!parseSubjectList())
        return false;

      continue;
    }

    const auto &Param = NamedParams.lookup(ArgName.lower());
    auto T = Param.T;
    if (T == XlxPragmaParam::Unknown) {
      P.Diag(MaybeArg.Loc, diag::warn_unexpected_pragma_parameter) << ArgName;
      return false;
    }

    auto &Arg = ArgMap[ArgName.lower()];
    if (!Arg.isNull()) {
      P.Diag(MaybeArg.Loc, diag::warn_repeated_pragma_parameter) << ArgName;
      return false;
    }

    if (T == XlxPragmaParam::CallBackParser) {
      Arg = Param.callback(*this, P, PragmaLoc);
      if (Arg.isNull())
        return false;
    }

    else if (T == XlxPragmaParam::PresentID) {
      if (PresentedID.count(ArgName.lower())) {
        P.Diag(MaybeArg.Loc, diag::warn_repeated_pragma_parameter) << ArgName;
        return false;
      }
      PresentedID.insert(std::make_pair(
        ArgName.lower(), createIdentLoc(MaybeArg.Ident, MaybeArg.Loc)));

      // Check mutual exclusive options
      if (Param.PresentGroup) {
        auto r = PresentGroups.insert({Param.PresentGroup, MaybeArg});
        if (!r.second) {
          P.Diag(MaybeArg.Loc, diag::warn_confilict_pragma_parameter)
              << ArgName << str(PresentGroups[Param.PresentGroup]);
          return false;
        }
      }

      if (P.TryConsumeToken(tok::equal)) {
        const Token &Tok = P.getCurToken();
        if (Tok.is(tok::annot_pragma_XlxHLS_end)) {
          P.Diag(Tok.getLocation(), diag::warn_pragma_named_argument_missing) << ArgName;
          return false;
        }
        else {
          StringRef s(Tok.isLiteral() ? Tok.getLiteralData()
                                      : Tok.getIdentifierInfo()->getNameStart(),
                      Tok.getLength());
          if (s.equals_lower("false") || s.equals("0")) {
            P.ConsumeToken();
          }
          else if (s.equals_lower("true") || s.equals("1")) {
            Arg = createIdentLoc(MaybeArg.Ident, MaybeArg.Loc);
            P.ConsumeToken();
          }
          else {
            P.Diag(Tok.getLocation(), diag::err_invalid_option_value) << s << ArgName;
            return false;
          }
        }
      }
      else {
        Arg = createIdentLoc(MaybeArg.Ident, MaybeArg.Loc);
      }
    }

    else {
      //// ID, Enum, Expression Parameter are all with following uniform format
      ///    identifer '=' identifer
      ///    identifer '=' integer
      /// We should see a '='
      if (!P.TryConsumeToken(tok::equal)) {
        const auto &Tok = P.getCurToken();
        if (Tok.getKind() == tok::annot_pragma_XlxHLS_end) { 
          P.Diag(Tok, diag::warn_unexpected_token_in_pragma_argument)
             << "End Of Pramga Line" << tok::equal;
        }
        else { 
          P.Diag(Tok, diag::warn_unexpected_token_in_pragma_argument)
             << PP.getSpelling(Tok) << tok::equal;
        }
        return false;
      }

      switch (T) {
      case XlxPragmaParam::Enum:
        Arg = parseEnumIdentifier(Param);
        break;
      case XlxPragmaParam::Id:
        Arg = parseIdentifierLoc();
        break;
      case XlxPragmaParam::ICEExpr:
        Arg = parseICEExpression();
        break;
      case XlxPragmaParam::VarRefExpr:
        Arg = parseVarRefExpression(ArgName);
        break;
      case XlxPragmaParam::DCEExpr:
        Arg = parseICEExpression();
        break;
      default:
        llvm_unreachable("Unexpected type");
        break;
      }
      if (Arg.isNull())
        return false;
    }
  }

  // Check the subject
  if (SubjectList.empty() && !SubjectParam.empty()) {
    P.Diag(P.getCurToken(), diag::warn_pragma_named_argument_missing)
        << SubjectParam;
    return false;
  }

  // Check required arguments
  for (const auto &Param : NamedParams) {
    if (Param.second.Required && !ArgMap.count(Param.first())) {
      P.Diag(PragmaLoc, diag::warn_pragma_named_argument_missing)
          << Param.first();
      return false;
    }
  }

  PragmaRange = SourceRange(PragmaLoc, P.getEndOfPreviousToken());
  return true;
}


static bool IsHLSStreamType(const Expr *VarExpr) {
  if (!VarExpr)
    return false;

  auto Ty = VarExpr->getType(); 
  if (isa<DecayedType>(Ty)) { 
    Ty = cast<DecayedType>(Ty)->getOriginalType();
  }
  Ty = Ty.getCanonicalType();

  auto *BTy = Ty->isReferenceType() ? Ty->getPointeeType().getTypePtr()
                                    : Ty->getPointeeOrArrayElementType();

  if (BTy->isClassType() && !BTy->getAsCXXRecordDecl()
                                 ->getCanonicalDecl()
                                 ->getQualifiedNameAsString()
                                 .compare("hls::stream"))
    return true;

  // hls::stream<type> with template in type
  if (dyn_cast<TemplateSpecializationType>(BTy)) {
    TemplateName name =
        dyn_cast<TemplateSpecializationType>(BTy)->getTemplateName();
    if (TemplateDecl *decl = name.getAsTemplateDecl()) {
      std::string base_name = decl->getQualifiedNameAsString();
      if (base_name == "hls::stream")
        return true;
    }
  }
  return false;
}

static bool HandleXlxDataflowPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                    SourceLocation PragmaLoc) {
  SubjectListTy SubjectList;
  PAP.setPragmaOption(
      "", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_DATAFLOW, 
#else 
      {optICEExpr("interval", -1), presentId("disable_start_propagation", 2)},
#endif 
      PragmaLoc); 
  if (!PAP.parse())
    return false;

  if (PAP.lookup("interval")) {
    PAP.P.Diag(PAP.P.getCurToken().getLocation(), diag::warn_deprecated_pragma_option)
      << "interval"
      << "dataflow";
  }

  auto PropagationType = "start_propagation";
  if (PAP.presentedId("disable_start_propagation"))
    PropagationType = "disable_start_propagation";

  ArgsUnion Type = PAP.createIdentLoc(PropagationType);
  PAP.addDependenceAttribute("xcl_dataflow", Type);

  return true;
}

static ArgsUnion ParserewindOptionForpipeline(XlxPragmaArgParser &PAP, Parser &P, SourceLocation PragmaLoc)
{
  return PAP.parseOptionalEnumVal("rewind", {"true", "false"}, "true");
}

static bool HandleXlxPipelinePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                    SourceLocation PragmaLoc) {
  SubjectListTy SubjectList;
  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_PIPELINE, 
#else 
                         {optICEExpr("ii", -1),
                          optCallBackParser("rewind", ParserewindOptionForpipeline)
                          presentId("enable_flush", 2),
                          presentId("off", 3),
                          optEnum("style", {"stp", "flp", "frp"})},
#endif
                         PragmaLoc);
  if (!PAP.parse())
    return false;

  bool off = PAP.getOff();
  // Obtain style option value
  IdentifierLoc *StyleMode = nullptr;
  if (auto Sty = PAP.lookup("style"))
    StyleMode = Sty.get<IdentifierLoc *>();

  if (StyleMode) {
    // Checkers for conflict
    if (PAP.presentedId("enable_flush")) {
      // error out
      PAP.P.Diag(PAP.P.getCurToken().getLocation(), diag::error_pipeline_style_conflict)
          << "'enable_flush' option.";
    }
#if 0
    if (PAP.presentedId("rewind") && StyleMode->Ident->getName().equals_lower("stp") == false) {
      // error out
      PAP.P.Diag(PAP.P.getCurToken().getLocation(), diag::error_pipeline_style_conflict)
          << "'rewind' option.";
    }
#endif 
  }

  // following with use style ID instead
  int64_t styleID = -1; // default value, which will be changed in LLVM
  if (PAP.presentedId("enable_flush")) {
    // enable_flush is same with style=flp, so we make them together
    styleID = 1; //same with style=flp
    // warning message to deprecate
    PAP.P.Diag(PAP.P.getCurToken().getLocation(),
           diag::warn_pipeline_enable_flush_deprecate);
  }

  if (StyleMode) {
    if (StyleMode->Ident->getName().equals_lower("stp"))
      styleID = 0;
    else if (StyleMode->Ident->getName().equals_lower("flp"))
      styleID = 1;
    else if (StyleMode->Ident->getName().equals_lower("frp"))
      styleID = 2;
  }

  ArgsUnion II = PAP["ii"];

  if (off)
    II = ArgsUnion(PAP.createIntegerLiteral(0));

  int64_t rewind = 0;
  if (ArgsUnion Arg = PAP.lookup("rewind")) {
    auto Val = Arg.get<IdentifierLoc*>();
    StringRef name {Val->Ident->getName()};
    if (name.equals_lower("false")) {
      rewind = 2;
    }
    else if (name.equals_lower("true")) {
      rewind = 1;
    }
    else {
      PAP.P.Diag(Val->Loc, diag::err_invalid_option_value) << name << "rewind";
      return false;
    }
  }
  ArgsUnion args[] = {II, PAP.createIntegerLiteral(styleID),
                      PAP.createIntegerLiteral(rewind)};
  PAP.addDependenceAttribute("xlx_pipeline", args);
  return true;
}

static bool HandleXlxUnrollPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                  SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_UNROLL,
#else
                         {optICEExpr("factor", 0), presentId("region", 1),
                          presentId("skip_exit_check", 2), presentId("complete", 1),
                          presentId("partial", 1), presentId("off", 3)
                         },
#endif
                         PragmaLoc);
  if (!PAP.parse())
    return false;

  // complete/partial is no longed supported
  if (enableXilinxPragmaChecker()) {
    if (PAP.presentedId("complete") || PAP.presentedId("partial") ||
        PAP.presentedId("region")) {
      PAP.P.Diag(PAP.P.getCurToken().getLocation(),
             diag::err_xlx_pragma_option_not_supported_by_HLS_WarnOut)
          << "Unroll"
          << "complete/partial/region";
    }
  }

  if (auto Arg = PAP.lookup("factor")) {
    if (auto *E = Arg.get<Expr *>()) {
      if (auto *I = dyn_cast<IntegerLiteral>(E)) {
        llvm::APInt Factor = I->getValue();
        if (Factor.getSExtValue() < 0 || Factor.getSExtValue() > 4294967295) {
          PAP.P.Diag(E->getExprLoc(), diag::err_xlx_pragma_invalid_unroll_factor)
              << "Option 'factor' is too big, the valid range is 0 ~ 2^32-1";
          return false;
        }
      }
    }
  }

  bool off = PAP.getOff();
  if (off) {
    for (auto s : {"factor", "skip_exit_check"}) {
      if (PAP.lookup(s)) {
        PAP.P.Diag(PAP.P.getCurToken().getLocation(), diag::err_confilict_pragma_parameter)
          << s
          << "off=true";
        return false;
      }
    }
  }

  ArgsUnion factor;

  if (PAP.lookup("factor")) {
    factor = PAP["factor"];
  }
  else if (off) {
    factor = PAP.createIntegerLiteral(1);
  }

  ArgsUnion Args[] = {factor, PAP["skip_exit_check"]};
  PAP.addDependenceAttribute("xlx_unroll_hint", Args);
  return true;
}

static bool HandleXlxFlattenPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                   SourceLocation PragmaLoc) {
  auto *S = CurScope;

  PAP.setPragmaOption("", false, {presentId("off", 1)}, PragmaLoc);
  if (!PAP.parse())
    return false;

  bool off = PAP.getOff();
  ArgsUnion Args[] = {PAP.createIntegerLiteral(off)};
  PAP.addDependenceAttribute("xlx_flatten_loop", Args);

  return true;
}

static bool HandleXlxMergePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                 SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_LOOP_MERGE, 
#else 
      {presentId("force", 1)}, 
#endif
      PragmaLoc);
  if (!PAP.parse())
    return false;

  auto Force = PAP["force"];
  PAP.addAttribute("xlx_merge_loop", Force);

  return true;
}

static bool HandleLoopTripCountPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                      SourceLocation PragmaLoc) {
  auto *S = CurScope;

  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_LOOP_TRIPCOUNT,
#else 
      {optICEExpr("min", 0), reqICEExpr("max"), optICEExpr("avg", 0)},
#endif
      PragmaLoc);

  if (!PAP.parse())
    return false;

  ArgsVector Args = {PAP["min"], PAP["max"]};
  if (PAP.lookup("avg"))
    Args.emplace_back(PAP["avg"]);
  PAP.addDependenceAttribute("xlx_loop_tripcount", Args);
  return true;
}

static bool
IsInvalidCore(Parser &P, 
              StringRef &CoreStr,
              const Expr* expr,
              SourceLocation PragmaLoc) {

  // Ignore axi related resource

  // we should check HLSStream type after "TempalteInstantitation" 
  // so, only  dsiable follwoing code, and  check IsHLSStreamType in 
  // HoistXlxAttr
#if 0
  // FIFO core only work on hls::stream
  if (CoreStr.startswith_lower("fifo")){
    bool IsStream = IsHLSStreamType(expr);
    if (!IsStream) {
      P.Diag(expr->getExprLoc(), diag::warn_implicit_hls_stream) 
        << "resource"
        << "hls::stream";
      return true;
    }
  }
#endif
  return false;
}

static bool HandleResourcePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                 SourceLocation PragmaLoc) {
  // Just warning to user this old resource pragma will be deleted in future
  PAP.P.Diag(PragmaLoc,
         diag::warn_deprecated_pragma_ignored_by_scout_skip_strict_mode)
      << "Resource pragma"
      << "bind_op/bind_storage pragma";

  PAP.setPragmaOption("", true,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_RESOURCE,
#else
      {reqVarRefExpr("variable"), reqId("core"), optICEExpr("latency", -1),
       optEnum("ecc_mode", {"none", "encode", "decode", "both"}),
       presentId("auto", 1), presentId("distribute", 1), presentId("block", 1),
       presentId("uram", 1), optId("metadata")},
#endif
      PragmaLoc);
  if (!PAP.parse())
    return false;

  IdentifierLoc *coreII = NULL;
  StringRef core_name;
  SourceLocation core_loc;

  coreII = PAP.lookup("core").get<IdentifierLoc *>();
  core_name = str(coreII);
  core_loc = coreII->Loc;

  SmallString<32> Name("xpm_memory");
  // Deal with old feature: XPM_MEMORY
  if (core_name.equals_lower("xpm_memory")) {
    // Just warning to user this old option will be deleted in future
    PAP.P.Diag(PAP.P.getCurToken().getLocation(),
           diag::warn_deprecated_pragma_option_ignored_by_scout)
        << "xpm_memory"
        << "Resource"
        << "Bind_Storage Pragma";

    unsigned bitselect = 0;
    if (PAP.presentedId("auto")) { /* do nothing */
    }
    if (PAP.presentedId("distribute"))
      bitselect = bitselect | 1;
    if (PAP.presentedId("block"))
      bitselect = bitselect | 2;
    if (PAP.presentedId("uram"))
      bitselect = bitselect | 4;

    switch (bitselect) {
    case 0: /* do nothing */
      break;
    case 1:
      Name += "_distribute";
      break;
    case 2:
      Name += "_block";
      break;
    case 4:
      Name += "_uram";
      break;
    default:
      PAP.P.Diag(PragmaLoc, diag::err_resource_pragma_xpm_memory_option_conflict);
    }
    core_name = Name;
  }
  // Ignore axi related resource
  // we need to check HLSStream type after "TemplateInstantiation" , during parsing 
  // TemplateInstantiation haven't happened 
  // so,   check IsHLSStreamType during  HoistXlxAttr

  if (core_name.startswith_lower("axi")) {
    PAP.P.Diag(PragmaLoc, diag::warn_obsolete_pragma_replaced)
      << "#pragma HLS RESOURCE core=axi"
      << "#pragma HLS INTERFACE axi";
    return false;
  }

  Expr* var_expr = PAP["variable"].get<Expr*>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);

  // Obtain op+impl based on platform API via core_name
  const platform::PlatformBasic *xilinxPlatform =
      platform::PlatformBasic::getInstance();
  std::vector<std::pair<platform::PlatformBasic::OP_TYPE,
                        platform::PlatformBasic::IMPL_TYPE>>
      OpImpls = xilinxPlatform->getOpImplFromCoreNameInCompleteRepository(core_name.str());
  bool isValid = true;
  if (!OpImpls.size()) { 
    SmallString<32> msg( "Resource with core_name: " ); 
    msg.append(core_name);
      PAP.P.Diag(PragmaLoc, diag::err_xlx_attribute_ignore_because_invalid_option)
        << msg << "this core doesn't exist at all";  
    isValid = false; 
    return isValid; 
  }
  for (auto om : OpImpls) {
    if (om.first == platform::PlatformBasic::OP_MEMORY) {

      for (auto e : subExprs) {
        if (isa<clang::FunctionType>(e->getType().getTypePtr())) {
          PAP.P.getActions().Diag(e->getExprLoc(),
                              diag::err_xlx_attribute_invalid_option)
              << "return"
              << "variable";
          isValid = false;
          continue;
        }

        ArgsUnion Args[] = {e, PAP.createIntegerLiteral(om.first, coreII->Loc),
                            PAP.createIntegerLiteral(om.second, coreII->Loc),
                            PAP["latency"]};
        PAP.addDependenceAttribute("xlx_bind_storage", Args);
      }
    } else {
      Expr* var_expr = PAP["variable"].get<Expr*>();
      // parse "expr1, expr2, expr3"
      SmallVector<Expr *, 4> subExprs;
      getSubExprOfVariable(subExprs, var_expr, PAP.P);
      for (auto e : subExprs) {
        FunctionDecl *func_decl = nullptr;
        if (isa<DeclRefExpr>(e)) {
          Decl *decl = dyn_cast<DeclRefExpr>(e)->getDecl();
          if (isa<FunctionDecl>(decl)) {
            func_decl = dyn_cast<FunctionDecl>(decl);
          }
        }

        if (func_decl) {
          PAP.P.getActions().Diag(e->getExprLoc(),
                              diag::err_xlx_attribute_invalid_option)
              << "return"
              << "variable";
          isValid = false;
          continue;
        } else {
          ArgsUnion Args[] = {
              e, PAP.createIntegerLiteral(om.first, coreII->Loc),
              PAP.createIntegerLiteral(om.second, coreII->Loc), PAP["latency"]};
          PAP.addDependenceAttribute("xlx_bind_op", Args);
        }
      }
    }
  }
  return isValid;
}

static bool HandleTopFunctionPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                    SourceLocation PragmaLoc) {
  // Do not use class method as top function
  auto CurFn = PAP.P.getActions().getCurFunctionDecl();
  if (CurFn->isCXXClassMember()) {
    PAP.P.Diag(PragmaLoc, diag::err_top_pragma_appiled_in_wrong_scope);
    return false;
  }

  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_TOP,
#else 
      {optId("name")}, 
#endif
      PragmaLoc); 

  if (!PAP.parse())
    return false;
  
  ArgsUnion args[] = {PAP["name"]};

  PAP.addAttribute("sdx_kernel", args); 
  return true;
}

static bool HandleUpwardInlineFunctionPragma(XlxPragmaArgParser &PAP, 
                                             bool NoInline) {
  //SemaXlxAttr, translate 'xlx_inline' to noinline or always_inline that will only apply for function
  ArgsUnion args[] = {PAP.createIntegerLiteral( !NoInline /*0, means off, 1 means on */)}; 
  PAP.addDependenceAttribute("xlx_inline", args); 
  return true;
}

// IsRecursive:
// 0: region
// 1: recursive
// 2: region off
static bool HandleDownwardInlineFunctionPragma(XlxPragmaArgParser &PAP,
                                               int IsRecursive) {


  ArgsUnion recurInfo = PAP.createIntegerLiteral(IsRecursive);
  PAP.addDependenceAttribute("xcl_inline", {recurInfo}); 
  return true;
}

static bool HandleInlinePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                               SourceLocation PragmaLoc) {
  // Parse Self|Region|All as scope
  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_INLINE,
#else 
      //TODO, check
                         {presentId("self", 1),presentId("all", 1), 
                          presentId("region", 1),
                          presentId("recursive", 2),
                          presentId("off", 3)
                         },
#endif
                         PragmaLoc);
  if (!PAP.parse())
    return false;

  bool typeAll = PAP.presentedId("all");
  bool typeSelf = PAP.presentedId("self");
  bool typeRegion = PAP.presentedId("region");
  if (auto Sty = PAP.lookup("type")) {
      IdentifierLoc * TypeMode = Sty.get<IdentifierLoc *>();
      if (TypeMode->Ident->getName().equals_lower("all")) typeAll = true;
      else if (TypeMode->Ident->getName().equals_lower("self")) typeSelf = true;
      else if (TypeMode->Ident->getName().equals_lower("region")) typeRegion = true;
  }
  bool InlineScope = (typeAll || typeSelf || typeRegion);
  bool RecursiveInline = PAP.presentedId("recursive");
  bool InlineOff = PAP.getOff();

  //0. Pre-explanations:
  //HandleUpwardInlineFunctionPragma: 
  //    handle "current callee function". So it only cares "off" or not
  //HandleUpwardInlineFunctionPragma:
  //    handle "recursive/region inline". So it only cares sub-functions inline or not;
  //Difference between "recursive" and "region":
  //    recursive: inline all sub-calls to bottom level
  //    region:    inline all sub-calls in 1-level

  //1. Handle without "self/all/region"
  //   E.g. #pragma HLS INLINE 
  //   E.g. #pragma HLS INLINE off
  //   E.g. #pragma HLS INLINE recursive
  if (!InlineScope) {
    // #pragma HLS INLINE recursive
    if (RecursiveInline) {
      // Inline everything in the compound statement
      HandleDownwardInlineFunctionPragma(PAP, 1);
      if (InlineOff)
        HandleUpwardInlineFunctionPragma(PAP, true);
      return true;
    }

    // Inline the current function, make sure we are in a function scope
    HandleUpwardInlineFunctionPragma(PAP, InlineOff);
    return true;
  }

  //2. Several warning checkers for deprecated options
  if (enableXilinxPragmaChecker()) {
    // self/all is no longed supported, error out
    if (PAP.presentedId("self") || PAP.presentedId("all")) {
      PAP.P.Diag(PAP.P.getCurToken().getLocation(),
             diag::err_xlx_pragma_option_not_supported_by_HLS_WarnOut)
          << "Inline"
          << "self/all";
    }

    // region option is warn, maybe ignored in future
    if (typeRegion) {
      PAP.P.Diag(PAP.P.getCurToken().getLocation(),
             diag::warn_deprecated_pragma_option_ignored_by_scout)
          << "region"
          << "Inline"
          << "Inline Pragma";
    }
  }

  //3. Handle "self" specially
  //   #pragma HLS INLINE self
  //   #pragma HLS INLINE self off
  //   #pragma HLS INLINE self recursive
  if (typeSelf) {
    if (RecursiveInline) {
      // recursively inline everything in the compound statement
      HandleDownwardInlineFunctionPragma(PAP, 1);
      if (InlineOff)
        HandleUpwardInlineFunctionPragma(PAP, true);
      return true;
    }

    // Inline the current function, make sure we are in a function scope
    HandleUpwardInlineFunctionPragma(PAP, InlineOff);
    return true;
  }

  //4. Handle "region" specially (other scenarios will return above)
  //   #pragma HLS INLINE region
  //   #pragma HLS INLINE region off
  //   #pragma HLS INLINE region recursive
  //   #pragma HLS INLINE region recursive off
  {
    if (InlineOff)
      HandleUpwardInlineFunctionPragma(PAP, true);

    if (RecursiveInline) {
      // Recursively inline everything in the compound statement
      HandleDownwardInlineFunctionPragma(PAP, 1);
      return true;
    }

    // Inline callsite in a none recursive way
    if (InlineOff)
      HandleDownwardInlineFunctionPragma(PAP, 2);
    else
      HandleDownwardInlineFunctionPragma(PAP, 0);
    return true;
  }
}

static bool HandleResetPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                              SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_RESET,
#else
      {reqVarRefExpr("variable"),
       presentId("off", 1)
      },
#endif
                         PragmaLoc);

  if (!PAP.parse())
    return false;

  bool off = PAP.getOff();
  //auto ResetOff = PAP["off"];
  //auto *A = PAP.createAttribute("xlx_var_reset", ResetOff);

  //auto &Actions = P.getActions();
  //for (auto &S : PAP.subjects()) {
  //  auto Var = dyn_cast<VarDecl>(S.first);
  //  if (!Var->hasGlobalStorage()) {
  //    P.Diag(S.second, diag::warn_invalid_pragma_variable)
  //        << "reset"
  //        << "static or global";
  //    continue;
  //  }
  //  //Actions.ProcessDeclAttributeList(CurScope, S.first, A, false);
  //}

  Expr *var_expr = PAP.lookup("variable").get<Expr*>();
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
  for (auto port_ref : subExprs) {
    ArgsUnion args[] = {port_ref, PAP.createIntegerLiteral(off)};
    PAP.addDependenceAttribute("xlx_reset", args);
  }
  return true;
}
static bool HandleFunctionAllocationPragma(IdentifierLoc *allocType, 
                                           XlxPragmaArgParser &PAP
                                           ) {

  Expr *func = PAP["instances"].get<Expr *>();
  ArgsUnion Args[] = {func, PAP["limit"]};
  PAP.addDependenceAttribute("xlx_function_allocation", Args);

  return true;
}

static bool HandleOperationAllocationPragma(IdentifierLoc *allocType,
                                            XlxPragmaArgParser &PAP) {


  ArgsUnion Args[] = {allocType, PAP["instances"], PAP["limit"]};
  PAP.addDependenceAttribute("fpga_resource_limit_hint", Args);
  return true;
}

static ArgsUnion ParseinstancesOptionForallocation(XlxPragmaArgParser &PAP, Parser &P, 
                                                  SourceLocation PragmaLoc) { 
  P.TryConsumeToken(tok::equal);
  auto &Ctx = PAP.P.getActions().getASTContext();
  auto &PP = PAP.P.getPreprocessor();
  auto &Actions = PAP.P.getActions();

  IdentifierLoc *AllocType = nullptr;
  auto allocToken = PAP.P.getCurToken();

  if (ArgsUnion arg = PAP.lookup("type")) {
    AllocType = arg.get<IdentifierLoc*>();
    StringRef s { AllocType->Ident->getName() };
    if (s.equals_lower("function")) {
      Expr *instances =  PAP.parseVarRefExpression("instances"); 
      return instances;
    }
    else if (s.equals_lower("operation")) {
      return PAP.parseIdentifierLoc();
    }
  }
  if (auto Sty = PAP.presentedId("function")) {
    Expr *instances =  PAP.parseVarRefExpression("instances"); 
    return instances;
  }
  if (auto Sty = PAP.presentedId("operation")) {
    return PAP.parseIdentifierLoc();
  } else {
    // Error out, we expect first option is "allocationype"
    PAP.P.Diag(allocToken.getLocation(),
           diag::warn_unexpected_token_in_pragma_argument)
        << allocToken.getIdentifierInfo()->getName() << "function/operation";
    return ArgsUnion(); 
  }
}

static bool HandleAllocationPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                   SourceLocation PragmaLoc) {

  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
    PRAGMA_HLS_ALLOCATION,
#else 
    {presentId("function", 1), presentId("operation", 1), reqId("instances"), reqICEExpr("limit"),
     optEnum("type", {"operation", "function"})},
#endif
    PragmaLoc);

  if (!PAP.parse())
    return false;

  IdentifierLoc *allocType = nullptr;
  if (ArgsUnion arg = PAP.lookup("type")) {
    allocType = arg.get<IdentifierLoc*>();
  }
  else if (ArgsUnion arg = PAP.lookup("function")) { 
    allocType = arg.get<IdentifierLoc*>();
  }
  else if (ArgsUnion arg = PAP.lookup("operation")) { 
    allocType = arg.get<IdentifierLoc*>();
  }

  if (allocType) {
    StringRef s { allocType->Ident->getName() };
    if (s.equals_lower("function")) {
      return HandleFunctionAllocationPragma(allocType, PAP);
    }
    else if (s.equals_lower("operation")) {
      return HandleOperationAllocationPragma(allocType, PAP);
    }
  }
  return false;
}

static bool HandleExpressionBanlancePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                           SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_EXPRESSION_BALANCE, 
#else 
      {
       presentId("off", 1)
      },
#endif
                          PragmaLoc); 

  if (!PAP.parse())
    return false;

  bool off = PAP.getOff();
  ArgsUnion Args[] = {PAP.createIntegerLiteral(off)};
  PAP.addAttribute("xlx_expr_balance", Args);

  return true;
}

static bool HandleClockPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                              SourceLocation PragmaLoc) {

  if (enableXilinxPragmaChecker()) {
    PAP.P.Diag(PragmaLoc, diag::warn_deprecated_pragma_ignored_by_scout) << "clock";
  }
  return false; // deprecated
}

static bool HandleDataPackPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                              SourceLocation PragmaLoc) {

  char *tmp = std::getenv("XILINX_VITIS_HLS_TRANSLATE_DATA_PACK_PRAGMA_TO_AGGREGATE");
  if (tmp == nullptr ) { 
    if (enableXilinxPragmaChecker()) {
      PAP.P.Diag(PragmaLoc, diag::err_xlx_pragma_not_supported_by_scout_HLS_WarnOut);
      return false;
    }
  }
  else { 
    PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
        PRAGMA_HLS_DATA_PACK, 
#else 
        {reqVarRefExpr("variable"), optId("instance"), presentId("struct_level"), presentId("field_level")}, 
#endif
        PragmaLoc); 
    if (!PAP.parse()) { 
      return false;
    } 
  
    ArgsUnion byte_pad;
    if (auto id = PAP.presentedId("struct_level")) { 
      //errout , can not support it now
      PAP.P.Diag(id->Loc, diag::err_xlx_attribute_invalid_option_and_because)
        << "'struct_level'" << "Vitis HLS doesn't support it";
      // TODO: ETP return false?
      //return false;
    }
    else if (auto id = PAP.presentedId("field_level")) { 
      byte_pad = id;
    } else if (auto Sty = PAP.lookup("byte_pad")) {
      byte_pad = Sty.get<IdentifierLoc *>();
    }
  
    Expr *var_expr = PAP["variable"].get<Expr *>();
    SmallVector<Expr *, 4> subExprs;
    getSubExprOfVariable(subExprs, var_expr, PAP.P);
  
    for (auto var : subExprs) {
      ArgsUnion Args[] = {var, byte_pad};
      PAP.addDependenceAttribute("xlx_data_pack", Args);
    }
  }
  return true;
}

// static void HandleArrayMapPragma(Parser &P, Scope *CurScope,
//                                 SourceLocation PragmaLoc) {
//
//   P.Diag( PragmaLoc, diag::warn_deprecated_pragma_ignored_by_scout)
//      <<"array_map";
//   return;
//}

static bool HandleFunctionInstantiatePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                            SourceLocation PragmaLoc) {
#if 0
  if (enableXilinxPragmaChecker()) {
    P.Diag(PragmaLoc, diag::warn_deprecated_pragma_ignored_by_scout)
        << "function_instantiate";
    return false;
  }
#endif

  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_FUNCTION_INSTANTIATE,
#else 
      {reqVarRefExpr("variable")}, 
#endif

            PragmaLoc); 

  if (!PAP.parse())
    return false;

  Expr *expr = PAP["variable"].get<Expr*>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> varExprs;
  getSubExprOfVariable(varExprs, expr, PAP.P);

  auto isParameterDecl = [](Expr *E) -> bool {
    if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
      Decl *D = DRE->getDecl();
      if(ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(D)) {
        return true;
      }
    }
    return false;
  };

  SmallVector<ArgsUnion, 8> args;
  for(Expr *E : varExprs) {
    if(isParameterDecl(E)) {
      ArgsUnion args[] = {E};
      PAP.addDependenceAttribute("xlx_func_instantiate", args);
    } else {
      PAP.P.Diag(E->getExprLoc(), diag::warn_invalid_pragma_variable)
          << "function_instantiate"
          << "function parameter";
    }
  }

  return true;
}


static bool HandleOccurrencePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                   SourceLocation PragmaLoc) {
  // occurrence can not be inserted in function scope
  if (CurScope->isFunctionScope()) {
    PAP.P.Diag(PragmaLoc, diag::warn_xlx_pragma_applied_in_wrong_scope)
        << "occurrence" << 2;
    return false;
  }

  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_OCCURRENCE, 
#else 
      {optICEExpr("cycle", 1),
       presentId("off", 2)
      },
#endif
                         PragmaLoc);

  if (!PAP.parse())
    return false;

  bool off = PAP.getOff();
  if (off) {
    if (PAP.lookup("cycle")) {
      PAP.P.Diag(PAP.P.getCurToken().getLocation(), diag::err_confilict_pragma_parameter)
        << "cycle"
        << "off=true";
      return false;
    }
  }

  ArgsUnion Cycle;
  if (PAP.lookup("cycle")) {
    Cycle = PAP["cycle"];
  }
  else {
    Cycle = PAP.createIntegerLiteral(1);
  }
  PAP.addAttribute("xlx_occurrence", {Cycle});
  return true;
}

static bool HandleProtocolPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                 SourceLocation PragmaLoc) {
  // Remove warning for protocol for temp
  // P.Diag( PragmaLoc, diag::warn_deprecated_pragma_ignored_by_scout )
  //   <<"protocol";

  // protocol can not be inserted in function scope
  if (CurScope->isFunctionScope()) {
    PAP.P.Diag(PragmaLoc, diag::warn_xlx_pragma_applied_in_wrong_scope)
        << "protocol" << 2;
    return false;
  }

  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_PROTOCOL, 
#else 
                         {presentId("floating", 1), presentId("fixed", 1),
                          optEnum("mode", {"floating", "fixed"})},
#endif
                         PragmaLoc);

  if (!PAP.parse())
    return false;

  IdentifierLoc *TypeMode = nullptr;
  if (auto Sty = PAP.lookup("mode"))
    TypeMode = Sty.get<IdentifierLoc *>();

  auto Mode = "floating";
  if (PAP.presentedId("fixed"))
    Mode = "fixed";
  else if (PAP.presentedId("floating"))
    Mode = "floating";
  else if (TypeMode) {
    if (TypeMode->Ident->getName().equals_lower("fixed"))
      Mode = "fixed";
    else if (TypeMode->Ident->getName().equals_lower("floating"))
      Mode = "floating";
  }

  ArgsUnion Arg = PAP.createIdentLoc(Mode);
  PAP.addAttribute("xlx_protocol", Arg);
  return true;
}

static bool HandlePerformancePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                SourceLocation PragmaLoc) {

  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
                         PRAGMA_HLS_PERFORMANCE,
#else
                         {
                           optDCEExpr("target_ti", 0.0),
                           optDCEExpr("target_tl", 0.0),
                           optDCEExpr("assume_ti", 0.0),
                           optDCEExpr("assume_tl", 0.0),
                           optEnum("unit", {"cycle", "sec"}),
                           optEnum("scope", {"loop", "region"})                          
                         },
#endif
                         PragmaLoc);

  if (!PAP.parse())
    return false;

  /// retrive scope value
  IdentifierLoc *ScopeId = nullptr;
  if (ArgsUnion ScopeValue = PAP.lookup("scope"))
    ScopeId = ScopeValue.get<IdentifierLoc *>();
  
  StringRef ScopeKind = "loop";

  if (ScopeId) {
    ScopeKind = StringSwitch<StringRef>(ScopeId->Ident->getName())
                    .CaseLower("loop", "loop")
                    .CaseLower("region", "region")
                    .Default(ScopeKind);
  }

  ArgsUnion TargetTi = PAP.lookup("target_ti");
  ArgsUnion TargetTl = PAP.lookup("target_tl");
  ArgsUnion AssumeTi = PAP.lookup("assume_ti");
  ArgsUnion AssumeTl = PAP.lookup("assume_tl");

  if (!TargetTi && !TargetTl && !AssumeTi && !AssumeTl) {
    PAP.P.Diag(PragmaLoc, diag::err_performance_at_least_one_parameter_required);
    return false;
  }

  ArgsUnion Args[] = {
      PAP["target_ti"],
      PAP["target_tl"],
      PAP["assume_ti"],
      PAP["assume_tl"],
      PAP["unit"],
      PAP.createIdentLoc(ScopeKind)
  };

  PAP.addDependenceAttribute("xlx_performance", Args);
  return true;
}

static bool HandleLatencyPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                SourceLocation PragmaLoc) {

  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_LATENCY, 
#else 
                         {optICEExpr("min", 0), optICEExpr("max", 65535)},
#endif 

                         PragmaLoc);

  if (!PAP.parse())
    return false;

  if (!PAP.lookup("min") && !PAP.lookup("max"))
    return false;

  ArgsUnion Arg[] = {PAP["min"], PAP["max"]};
  PAP.addDependenceAttribute("xcl_latency", Arg);
  return true;
}

static bool HandleArrayPartitionPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                       SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_ARRAY_PARTITION,
#else 
                         {reqVarRefExpr("variable"), optICEExpr("factor", 0),
                          optICEExpr("dim", 1), presentId("cyclic", 1),
                          presentId("block", 1), presentId("complete", 1),
                          optEnum("type", {"cyclic", "block", "complete"}), 
                          presentId("dynamic", 0), presentId("off", 2)
                         },
#endif
                         PragmaLoc);

  if (!PAP.parse())
    return false;

  IdentifierLoc *TypeMode = nullptr;
  if (auto Sty = PAP.lookup("type"))
    TypeMode = Sty.get<IdentifierLoc *>();

  StringRef Type = "complete";
  if (PAP.presentedId("block"))
    Type = "block";
  else if (PAP.presentedId("cyclic"))
    Type = "cyclic";
  else if (PAP.presentedId("complete"))
    Type = "complete";
  else if (TypeMode) {
    if (TypeMode->Ident->getName().equals_lower("block"))
      Type = "block";
    else if (TypeMode->Ident->getName().equals_lower("cyclic"))
      Type = "cyclic";
    else if (TypeMode->Ident->getName().equals_lower("complete"))
      Type = "complete";
  }

  ArgsUnion AType = PAP.createIdentLoc(Type);
  ArgsUnion Dim = PAP["dim"];
  ArgsUnion Factor;

  bool off = PAP.getOff();
  if (off) {
    for (auto s : {"type", "block", "cyclic", "complete", "factor"}) {
      if (PAP.lookup(s)) {
        PAP.P.Diag(PAP.P.getCurToken().getLocation(), diag::err_confilict_pragma_parameter)
          << s
          << "off=true";
        return false;
      }
    }
  }

  if (Type.equals_lower("complete")) {
    if (auto Arg = PAP.lookup("factor")) {
      auto *E = Arg.get<Expr *>();
      PAP.P.Diag(E->getExprLoc(), diag::warn_extra_pragma_hls_token_ignored)
          << "factor"
          << "array_partition";
    }
  } else {
    if (!PAP.lookup("factor")) {
      PAP.P.Diag(PragmaLoc, diag::warn_pragma_named_argument_missing) << "factor";
      return false;
    }

    // Have to insert the factor next to AType as the format of attribute
    // has a werid definition e.g. xcl_array_partition(block, 2, 1)
    // where the second parameter is the factor.
    Factor = PAP["factor"];
  }

  Expr *var_expr = PAP["variable"].get<Expr *>();
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);

  for (auto var : subExprs) {
    ArgsUnion Args[] = {var, AType, Factor, Dim, PAP.presentedId("dynamic"),
                        PAP.createIntegerLiteral(off)
                       };
    PAP.addDependenceAttribute("xlx_array_partition", Args);
  }

  return true;
}

static bool HandleArrayReshapePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                     SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_ARRAY_RESHAPE,
#else 
                         {reqVarRefExpr("variable"), optICEExpr("factor", 0),
                          optICEExpr("dim", 1), presentId("cyclic", 1),
                          presentId("block", 1), presentId("complete", 1),
                          presentId("object", 2),
                          optEnum("type", {"cyclic", "block", "complete"}),
                          presentId("off", 3)
                         },
#endif
                         PragmaLoc);

  if (!PAP.parse())
    return false;

  bool off = PAP.getOff();
  if (off) {
    for (auto s : {"type", "block", "cyclic", "complete", "factor"}) {
      if (PAP.lookup(s)) {
        PAP.P.Diag(PAP.P.getCurToken().getLocation(), diag::err_confilict_pragma_parameter)
          << s
          << "off=true";
        return false;
      }
    }
  }

  IdentifierLoc *TypeMode = nullptr;
  if (auto Sty = PAP.lookup("type"))
    TypeMode = Sty.get<IdentifierLoc *>();

  StringRef Type = "complete";
  if (PAP.presentedId("block"))
    Type = "block";
  else if (PAP.presentedId("cyclic"))
    Type = "cyclic";
  else if (PAP.presentedId("complete"))
    Type = "complete";
  else if (TypeMode) {
    if (TypeMode->Ident->getName().equals_lower("block"))
      Type = "block";
    else if (TypeMode->Ident->getName().equals_lower("cyclic"))
      Type = "cyclic";
    else if (TypeMode->Ident->getName().equals_lower("complete"))
      Type = "complete";
  }

  ArgsUnion AType = PAP.createIdentLoc(Type);
  ArgsUnion Dim = PAP["dim"];

  // ignore all other options
  if (PAP.presentedId("object")) {
    Type = "complete";
    AType = PAP.createIdentLoc(Type);
    Dim = PAP.createIntegerLiteral(0);
  }

  ArgsUnion factor;

  if (Type.equals_lower("complete")) {
    // don't expect factor option
    if (auto Arg = PAP.lookup("factor")) {
      auto *E = Arg.get<Expr *>();
      PAP.P.Diag(E->getExprLoc(), diag::warn_extra_pragma_hls_token_ignored)
          << "factor"
          << "array_reshape";
    }
  } else {
    // expect factor option
    if (!PAP.lookup("factor")) {
      // TODO, add error out message
      return false;
    }

    // Have to insert the factor next to AType as the format of attribute
    // has a werid definition e.g. xlx_array_reshape(block, 2, 1)
    // where the second parameter is the factor.
    factor = PAP["factor"];
  }

  Expr *var_expr = PAP["variable"].get<Expr *>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
  for (auto var : subExprs) {
    ArgsUnion args[] = {var, AType, factor, Dim,
                        PAP.createIntegerLiteral(off)};

    PAP.addDependenceAttribute("xlx_array_reshape", args);
  }

  return true;
}

static bool HandleStreamPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                               SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_STREAM, 
#else 
                         {reqVarRefExpr("variable"), optICEExpr("depth", 0),
                          optICEExpr("dim", 0), presentId("off", 1),
                          optEnum("type", {"fifo", "pipo", "shared", "unsync"})},
#endif
                         PragmaLoc);

  if (!PAP.parse())
    return false;

  // Obtain type option value
  IdentifierLoc *TypeMode = nullptr;
  if (auto Sty = PAP.lookup("type"))
    TypeMode = Sty.get<IdentifierLoc *>();

  if (TypeMode) {
    // Checkers for conflict
    if (PAP.presentedId("off") && TypeMode->Ident->getName().equals_lower("pipo") == false) {
      // error out
      PAP.P.Diag(PAP.P.getCurToken().getLocation(), diag::error_stream_type_conflict)
          << "'off' option.";
    }
  }

  // following with use type ID instead
  int64_t typeID = 0; // default value means "fifo"
  if (PAP.presentedId("off")) {
    // off is same with type=pipo, so we make them together
    typeID = 1; //same with type=pipo
    // warning message to deprecate
    PAP.P.Diag(PAP.P.getCurToken().getLocation(),
           diag::warn_stream_off_deprecate);
  }

  if (TypeMode) {
    if (TypeMode->Ident->getName().equals_lower("fifo"))
      typeID = 0;
    else if (TypeMode->Ident->getName().equals_lower("pipo"))
      typeID = 1;
    else if (TypeMode->Ident->getName().equals_lower("shared"))
      typeID = 2;
    else if (TypeMode->Ident->getName().equals_lower("unsync"))
      typeID = 3;
  }


  ArgsUnion Depth, /*Dim,*/ Off;
  Depth = PAP["depth"];
#if 0
  if (PAP.presentedId("off")) {
    Off = PAP.createIntegerLiteral(1);
  } else {
    Off = PAP.createIntegerLiteral(0);
  }
#endif

  // Error out for 'dim' option
  if (enableXilinxPragmaChecker()) {
    if (PAP.lookup("dim"))
      PAP.P.Diag(PAP.P.getCurToken().getLocation(),
             diag::err_xlx_pragma_option_not_supported_by_HLS_WarnOut)
          << "Stream"
          << "dim";
  }
  Expr *var_expr = PAP["variable"].get<Expr *>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);

  for (auto expr : subExprs) {
    ArgsUnion args[] = {expr, Depth, PAP.createIntegerLiteral(typeID)};
    PAP.addDependenceAttribute("xlx_reqd_pipe_depth", args);
  }
  return true;
}

static bool isBurstMAXIStruct(QualType type) { 

  return (type->isClassType() && !type->getAsCXXRecordDecl()
                                 ->getCanonicalDecl()
                                 ->getQualifiedNameAsString()
                                 .compare("hls::burst_maxi"));
}

static bool CheckInterfacePort(Parser &P,
                                   const Expr *port,
                                   IdentifierLoc Mode) {
  /*TODO, we need check the VarExpr origin is from  function Parameter
  if (!Var || !isa<ParmVarDecl>(Var)) {
    P.Diag(S.second, diag::warn_invalid_interface_port);
    return nullptr;
  }
  */

  auto ModeName = Mode.Ident->getName();
  // Check hls::stream type first
  const bool StreamMode =
      ModeName.equals_lower("axis") || ModeName.equals_lower("ap_fifo");
  const bool StreamType = IsHLSStreamType(port);
  if (StreamType && !StreamMode) {
    P.Diag(port->getExprLoc(), diag::warn_unsupported_interface_port_data_type) << ModeName;
    return false;
  }

  auto Type = port->getType(); 
  if (isa<DecayedType>(Type)) { 
    Type = cast<DecayedType>(Type)->getOriginalType();
  }

  Type = Type.getCanonicalType();
  bool Handle =
      StringSwitch<bool>(ModeName)
          // AXI
          .CaseLower("m_axi", Type->isArrayType() || Type->isPointerType() ||
                                  Type->isReferenceType() || isBurstMAXIStruct(Type))
          .CaseLower("axis", true)
          .CaseLower("s_axilite", true)
          // RAM/FIFO
          .CaseLower("ap_memory", Type->isArrayType() ||
                                      Type->isPointerType() ||
                                      Type->isReferenceType())
          .CaseLower("bram", Type->isArrayType() || Type->isPointerType() ||
                                 Type->isReferenceType())
          .CaseLower("ap_fifo", true)
          // Scalars
          .CaseLower("ap_none", true)
          // Handshake
          .CaseLower("ap_hs", true)
          .CaseLower("ap_ack", true)
          .CaseLower("ap_vld", true)
          .CaseLower("ap_ovld", true)
          // Stable
          .CaseLower("ap_stable", true)
          // Calling conventions
          .CaseLower("ap_ctrl_none", true)
          .CaseLower("ap_ctrl_hs", true)
          .CaseLower("ap_ctrl_chain", true)
          .Default(false);

  if (!Handle) {
    P.Diag(port->getExprLoc(), diag::warn_unsupported_interface_port_data_type) << ModeName;
    return false;
  }

  // Array of size one is not supported and treated as pointer to scalar.
  if (ModeName.equals_lower("bram") || ModeName.equals_lower("ap_memory")) {
    if (auto *AT = dyn_cast<ConstantArrayType>(Type)) {
      auto EleTy = AT->getElementType().getCanonicalType();
      // If the array element is another array, it's legal to have size one.
      // FIXME: what if the element array is also of size one?
      if (!EleTy->isArrayType() && AT->getSize().isOneValue()) {
        P.Diag(port->getExprLoc(), diag::warn_unsupported_interface_bram_type) << ModeName;
        return false;
      }
    }
  }

  return true;
}

 static bool HandleApBusInterfacePragma(Parser &P, Scope *CurScope,
                                        IdentifierLoc Mode,
                                        SourceLocation PragmaLoc,
                                        XlxPragmaArgParser &PAP) {
  //error out and do nothing
  PAP.P.Diag(PAP.P.getCurToken().getLocation(),
         diag::err_xlx_pragma_option_not_supported_by_HLS)
      << "Interface"
      << "ap_bus mode"
      << "m_axi";
  return false;

}

static IntegerLiteral* GetInterfaceDirectIO(XlxPragmaArgParser &PAP)
{
  IntegerLiteral *res {PAP.createIntegerLiteral(0)};
  if (auto Arg = PAP.lookup("direct_io")) {
    auto Val {Arg.get<IdentifierLoc*>()};
    auto name {Val->Ident->getName()};
    res = name.equals_lower("false") ? PAP.createIntegerLiteral(0) :
          name.equals_lower("true")  ? PAP.createIntegerLiteral(1) :
          res;
  }
  return res;
}

static bool CheckInterfaceDirectIO(XlxPragmaArgParser &PAP, IdentifierLoc &Mode)
{
  static StringRef validModes[] {
    "ap_none", "ap_stable", "ap_vld", "ap_ack", "ap_hs", "ap_ovld"
  };
  static StringRef option {"direct_io"};

  if (auto Arg = PAP.lookup(option)) {
    Parser &P = PAP.P;
    SourceLocation Loc {P.getCurToken().getLocation()};
    IdentifierLoc *Val {Arg.get<IdentifierLoc*>()};
    StringRef modeName {Mode.Ident->getName()};
    for (StringRef s : validModes) {
      if (modeName.equals_lower(s)) {
        StringRef vs {Val->Ident->getName()};
        if (vs.equals_lower("true") || vs.equals_lower("false")) {
          return true;
        }
        else {
          P.Diag(Loc, diag::err_invalid_option_value) << vs << option;
          return false;
        }
      }
    }
    P.Diag(Loc, diag::err_confilict_pragma_parameter) << option << modeName;
    return false;
  }
  else {
    return true;
  }
}

static bool HandleGenericInterfacePragma(Parser &P, Scope *CurScope,
                                         IdentifierLoc Mode,
                                         SourceLocation PragmaLoc,
                                         XlxPragmaArgParser &PAP) {

                         
  StringMap<XlxPragmaParam> ParmMap = {

      presentId("ap_stable", 6),
      presentId("ap_fifo", 6), 

      presentId("ap_none", 6),

      presentId("ap_ack", 6), 
      presentId("ap_ovld", 6),

    optVarRefExpr("port"), presentId("register"), optId("name"),
    optICEExpr("depth"), optICEExpr("latency"),
    optEnum("direct_io", {"true", "false"}),
    optEnum("mode", {"ap_fifo", "ap_stable", "ap_none", "ap_ack", "ap_ovld"})};

  if (!PAP.CheckAndFilter(ParmMap))
    return false;

  // Latency is not documented
  if (auto Arg = PAP.lookup("latency")) {
    auto *E = Arg.get<Expr *>();
    PAP.P.Diag(E->getExprLoc(), diag::warn_extra_pragma_hls_token_ignored)
        << "latency"
        << "INTERFACE";
  }

  auto *ModeId = PAP.createIdentLoc(Mode.Ident, Mode.Loc);
  auto ModeStr = ModeId->Ident->getName();
  StringRef InterfaceMode = "fpga_scalar_interface";
  if (ModeStr.equals_lower("ap_fifo"))
    InterfaceMode = "fpga_address_interface";

  auto *AdaptorName = PAP.createIdentLoc("");
  ArgsUnion InterfaceArgs[] = {ModeId, AdaptorName};
  auto *A = PAP.createAttribute(InterfaceMode, InterfaceArgs);

  if (auto Arg = PAP.lookup("name"))
    A = PAP.createAttribute("fpga_signal_name", Arg, A);

  if (auto Arg = PAP.lookup("depth"))
    A = PAP.createAttribute("fpga_foot_print_hint", Arg, A);

  if (PAP.presentedId("register")) {
    A = PAP.createAttribute("fpga_register", None, A);
  }

  auto &Actions = PAP.P.getActions();

  Expr *var_expr = PAP["port"].get<Expr*>();
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, P);
 
  for (auto port_ref : subExprs) {
    if (ModeStr == "ap_fifo") { 
      ArgsUnion args[] = { port_ref, PAP.presentedId("register"),  PAP.lookup("depth"), PAP.lookup("name")};
      PAP.addDependenceAttribute("ap_fifo", args) ;
    }
    else { 
      ArgsUnion args[] = {
        port_ref, ModeId, PAP.presentedId("register"), PAP.lookup("name"),
        GetInterfaceDirectIO(PAP)};
      PAP.addDependenceAttribute("ap_scalar", args) ;
    }
  }
  return true;
}

// Add interrupt option for ap_hs/ap_vld
// This option enables the I/O to be managed in interrupt, by creating the 
// corresponding bits in the ISR and IER SAXILite file. Value N=[16,31] 
// specifies the bit position in both registers
static bool HandleInterruptInterfacePragma(Parser &P, Scope *CurScope,
                                         IdentifierLoc Mode,
                                         SourceLocation PragmaLoc,
                                         XlxPragmaArgParser &PAP) {

                         
  StringMap<XlxPragmaParam> ParmMap = {

      presentId("ap_hs", 6), 
      presentId("ap_vld", 6), 

    optVarRefExpr("port"), presentId("register"), optId("name"),
    optICEExpr("depth"), optICEExpr("latency"), optICEExpr("interrupt"),
    optEnum("direct_io", {"true", "false"}),
    optEnum("mode", {"ap_hs", "ap_vld"})};

  if (!PAP.CheckAndFilter(ParmMap))
    return false;

  // Latency is not documented
  if (auto Arg = PAP.lookup("latency")) {
    auto *E = Arg.get<Expr *>();
    PAP.P.Diag(E->getExprLoc(), diag::warn_extra_pragma_hls_token_ignored)
        << "latency"
        << "INTERFACE";
  }

  auto *ModeId = PAP.createIdentLoc(Mode.Ident, Mode.Loc);
  auto ModeStr = ModeId->Ident->getName();
  StringRef InterfaceMode = "fpga_scalar_interface";

  auto *AdaptorName = PAP.createIdentLoc("");
  ArgsUnion InterfaceArgs[] = {ModeId, AdaptorName};
  auto *A = PAP.createAttribute(InterfaceMode, InterfaceArgs);

  if (auto Arg = PAP.lookup("name"))
    A = PAP.createAttribute("fpga_signal_name", Arg, A);

  if (auto Arg = PAP.lookup("depth"))
    A = PAP.createAttribute("fpga_foot_print_hint", Arg, A);

  if (PAP.presentedId("register")) {
    A = PAP.createAttribute("fpga_register", None, A);
  }

  auto &Actions = PAP.P.getActions();

  Expr *var_expr = PAP["port"].get<Expr*>();
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
 
  for (auto port_ref : subExprs) {
    ArgsUnion args[] = {
      port_ref, ModeId, PAP.presentedId("register"), PAP.lookup("name"),
      PAP.lookup("interrupt"), GetInterfaceDirectIO(PAP)};
    PAP.addDependenceAttribute("ap_scalar_interrupt", args) ;
  }
  return true;
}


static bool HandleBRAMInterfacePragma(Parser &P, Scope *CurScope,
                                      IdentifierLoc Mode,
                                      SourceLocation PragmaLoc,
                                      XlxPragmaArgParser &PAP) {
  StringMap<XlxPragmaParam> ParmMap = 
                         {
                          presentId("ap_memory", 6),
                          presentId("bram", 6),

                          optVarRefExpr("port"), optICEExpr("depth"), optICEExpr("latency"),
                          optId("storage_type", "default"), optId("name"),
                          optEnum("mode", {"ap_memory", "bram"})};

  if (!PAP.CheckAndFilter(ParmMap)) { 
    return false;
  }

  auto *ModeId = PAP.createIdentLoc(Mode.Ident, Mode.Loc);
  auto ModeStr = ModeId->Ident->getName();

  // Default storage_type configuration is "ram_2p" if "stroage_type" is not
  // specified.
  Expr *ram_type = nullptr;
  Expr *ram_impl = nullptr;
  if (auto C = PAP.lookup("storage_type")) {
    auto storage_type_id = C.get<IdentifierLoc *>();
    const platform::PlatformBasic *xilinxPlatform =
        platform::PlatformBasic::getInstance();
    std::pair<platform::PlatformBasic::OP_TYPE,
              platform::PlatformBasic::IMPL_TYPE>
        mem_impl_type;
    if (!xilinxPlatform->verifyInterfaceStorage(
            storage_type_id->Ident->getName().str(), &mem_impl_type)) {
      P.Diag(storage_type_id->Loc, diag::err_xlx_attribute_invalid_option)
          << storage_type_id->Ident->getName()
          << "interface BRAM's option 'storage_type'";
    }
    ram_type =
        PAP.createIntegerLiteral(mem_impl_type.first, storage_type_id->Loc);
    ram_impl =
        PAP.createIntegerLiteral(mem_impl_type.second, storage_type_id->Loc);
  } else {
    ram_type = PAP.createIntegerLiteral(platform::PlatformBasic::OP_UNSUPPORTED,
                                        Mode.Loc);
    ram_impl = PAP.createIntegerLiteral(platform::PlatformBasic::UNSUPPORTED,
                                        Mode.Loc);
  }

  // verify latency after template Instantiation
  auto Latency = PAP.lookup("latency");
  if (!Latency) { 
    Latency = PAP.createIntegerLiteral(-1);
  }

  AttributeList *A = nullptr;
  if (auto Arg = PAP.lookup("name"))
    A = PAP.createAttribute("fpga_signal_name", Arg, A);
  if (auto Arg = PAP.lookup("depth"))
    A = PAP.createAttribute("fpga_foot_print_hint", Arg, A);

  auto &Actions = P.getActions();

  Expr *var_expr = PAP["port"].get<Expr*>();
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, P);
 
  for (auto port_ref : subExprs) {
    ArgsUnion args[] = { port_ref, ModeId, PAP.lookup("storage_type"), PAP.lookup("latency"), PAP.lookup("name"), PAP.lookup("depth")};
    PAP.addDependenceAttribute( "xlx_memory", args);
  }
  return true;
}

static bool HandleSAXIInterfacePragma(Parser &P, Scope *CurScope,
                                      IdentifierLoc Mode,
                                      SourceLocation PragmaLoc,
                                      XlxPragmaArgParser &PAP) {
  
  StringMap<XlxPragmaParam> ParamMap = 
                         {
                          presentId("s_axilite", 6),
                          optVarRefExpr("port"), optId("bundle", "0"), presentId("register"),
                          optICEExpr("offset"), optId("clock"), optId("name"), optId("storage_impl"),
                          optEnum("mode", {"s_axilite"})};
  if (!PAP.CheckAndFilter(ParamMap))
    return false;

  auto &Attrs = CurScope->getParsedHLSPragmasRef();

  // Create the attribute for the adaptor
  auto *AdaptorName = PAP["bundle"].get<IdentifierLoc *>();

  ArgsUnion AdaptorArgs[] = {AdaptorName, PAP["clock"]};

  ArgsVector InterfaceArgs = {PAP.createIdentLoc(Mode.Ident, Mode.Loc),
                              AdaptorName};
  if (auto Offset = PAP.lookup("offset"))
    InterfaceArgs.push_back(Offset);

  // Create the interface attribute for the ParamDecl
  auto *A = PAP.createAttribute("fpga_interface_wrapper", InterfaceArgs);

  if (PAP.presentedId("register"))
    A = PAP.createAttribute("fpga_register", None, A);

  if (auto Arg = PAP.lookup("name"))
    A = PAP.createAttribute("fpga_signal_name", Arg, A);

  auto &Actions = P.getActions();
  ArgsUnion bundleName = PAP.lookup("bundle");
  ArgsUnion offset = PAP.lookup("offset");
  ArgsUnion clockName = PAP.lookup("clock");
  ArgsUnion implName = PAP.lookup("storage_impl");
  ArgsUnion isRegister;
  if (PAP.presentedId("register")) { 
    isRegister = PAP.createIntegerLiteral(1);
  }

  Expr *var_expr = PAP["port"].get<Expr*>();
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, P);
 
  for (auto port_ref : subExprs) {
    ArgsUnion args[] = {port_ref, bundleName, offset, isRegister, PAP.lookup("name"), clockName, implName};
    PAP.addDependenceAttribute("s_axilite", args);
  }
  return true;
}

static bool HandleMAXIInterfacePragma(Parser &P, Scope *CurScope,
                                      IdentifierLoc Mode,
                                      SourceLocation PragmaLoc,
                                      XlxPragmaArgParser &PAP) {

  StringMap<XlxPragmaParam> ParamList = 
  {
      presentId("m_axi", 6), 
      optVarRefExpr("port"), optId("bundle"), optICEExpr("depth"), optId("offset"), optId("name"),
       optICEExpr("num_read_outstanding"), optICEExpr("num_write_outstanding"),
       optICEExpr("max_read_burst_length"),
       optICEExpr("max_write_burst_length"), optICEExpr("latency"),
       optICEExpr("max_widen_bitwidth"),
       optICEExpr("channel"),
       optEnum("mode", {"m_axi"})};

  if (!PAP.CheckAndFilter(ParamList)) {
    return false;
  }

  Expr *channel = PAP.lookup("channel").get<Expr*>(); 
  if (!channel) {
    channel = PAP.createStringLiteral("", PragmaLoc);
  } else {
    llvm::APSInt Value;
    SourceLocation Loc;
    if (!channel->isIntegerConstantExpr(Value, P.getActions().getASTContext(), &Loc)) {
      P.Diag(Loc, diag::err_ice_not_integral) << channel->getType();
      return false;
    }

    Value.setIsUnsigned(true);
    channel = PAP.createStringLiteral(Value.toString(10), Loc);
  }

  Expr *var_expr = PAP.lookup("port").get<Expr*>();
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, P);

  for (auto port_ref : subExprs) {
    ArgsUnion args [] = { port_ref, PAP.lookup("bundle"), PAP.lookup("depth"), PAP.lookup("offset"), 
                        PAP.lookup("name"), PAP.lookup("num_read_outstanding"), PAP.lookup("num_write_outstanding"), 
                        PAP.lookup("max_read_burst_length"), PAP.lookup("max_write_burst_length"), PAP.lookup("latency"),
                        PAP.lookup("max_widen_bitwidth"), channel };
    PAP.addDependenceAttribute("m_axi", args);
  }
  return true;
}

static bool HandleAXISInterfacePragma(Parser &P, Scope *CurScope,
                                      IdentifierLoc Mode,
                                      SourceLocation PragmaLoc,
                                      XlxPragmaArgParser &PAP) {
  StringMap<XlxPragmaParam> ParamMap = 
      {
       presentId("axis", 6),
       optVarRefExpr("port"), presentId("register", 0),
       optEnum("register_mode", {"forward", "reverse", "both", "off"}),
       presentId("forward", 2), presentId("reverse", 2), presentId("both", 2),
       presentId("off", 2), optICEExpr("depth"), optId("name"),
       optEnum("mode", {"axis"})};

  if (!PAP.CheckAndFilter(ParamMap)) { 
    return false;
  }

  auto *ModeId = PAP.createIdentLoc(Mode.Ident, Mode.Loc);

  AttributeList *A = nullptr;
  if (auto Arg = PAP.lookup("name"))
    A = PAP.createAttribute("fpga_signal_name", Arg);

  if (auto Arg = PAP.lookup("depth"))
    A = PAP.createAttribute("fpga_foot_print_hint", Arg, A);

  // register default apply to axis
  // register_mode only apply to axis
  IdentifierLoc *RegMode;
  if (auto Reg = PAP.lookup("register_mode"))
    RegMode = Reg.get<IdentifierLoc *>();
  else if (auto id_loc = PAP.presentedId("both")) {
    RegMode = id_loc;
  } else if (auto id_loc = PAP.presentedId("forward")) {
    RegMode = id_loc;
  } else if (auto id_loc = PAP.presentedId("reverse")) {
    RegMode = id_loc;
  } else if (auto id_loc = PAP.presentedId("off")) {
    RegMode = id_loc;
  } else {
    RegMode = PAP.createIdentLoc("both");
  }

  auto &Actions = P.getActions();

  Expr *var_expr = PAP.lookup("port").get<Expr*>();
  SmallVector<Expr *, 4> subExprs;
  ArgsUnion bundleName = PAP.lookup("bundle"); //only used internal, user will not use
  getSubExprOfVariable(subExprs, var_expr, P);
  for (auto port_ref : subExprs) {
    ArgsUnion args[] = {port_ref, PAP.presentedId("register"), RegMode, PAP.lookup("depth"), PAP.lookup("name"), bundleName};
    PAP.addDependenceAttribute("axis", args);
  }
  return true;
}

static bool HandleAPStableInterfacePragma(Parser &P, Scope *CurScope,
                                        IdentifierLoc Mode,
                                        SourceLocation PragmaLoc,
                                        XlxPragmaArgParser &PAP) 
{
  P.Diag(Mode.Loc, diag::warn_deprecated_pragma_option_ignored_by_scout)
    << "Ap_stable" 
    << "INTERFACE" 
    << "Stable Pragma" ;
  return HandleGenericInterfacePragma(P, CurScope, Mode, PragmaLoc, PAP);
}

static bool HandleUnknownInterfacePragma(Parser &P, Scope *CurScope,
                                         IdentifierLoc Mode,
                                         SourceLocation PragmaLoc, 
                                         XlxPragmaArgParser &PAP){ 
  P.Diag(Mode.Loc, diag::error_unknown_interface_mode);
  return false;
}

static bool HandleCallingConvInInterfacePragma(Parser &P, Scope *CurScope,
                                               IdentifierLoc Mode,
                                               SourceLocation PragmaLoc,
                                               XlxPragmaArgParser &PAP) {
  StringMap<XlxPragmaParam> ParamMap = 
                         {
                           presentId("ap_ctrl_chain"),
                           presentId("ap_ctrl_hs"),
                           presentId("ap_ctrl_none"),
                           reqVarRefExpr("port"), optId("name"),
                           optEnum("mode", {"ap_ctrl_chain", "ap_ctrl_hs", "ap_ctrl_none"})};
  if (!PAP.CheckAndFilter(ParamMap))
    return false;

  Expr *var_expr = PAP.lookup("port").get<Expr*>();
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
  for (auto port_ref : subExprs) {
    if (isa<DeclRefExpr>(port_ref)) {
      Decl * decl = cast<DeclRefExpr>(port_ref)->getDecl();
      if (isa<FunctionDecl>(decl)) {
        ArgsUnion InterfaceArgs[] = {
                                   PAP.createIdentLoc(Mode.Ident, Mode.Loc),
                                   PAP["name"]
                                  };
        PAP.addDependenceAttribute("fpga_function_ctrl_interface", InterfaceArgs);
        continue;
      }
      PAP.P.Diag(port_ref->getExprLoc(), diag::warn_invalid_funtion_level_interface_port);
    }
    else {
      assert(false &&"unexpected, only port = return is expected");
    }
  }
  return true;
}

static bool HandleInterfacePragmaWithPAP(Parser &P, Scope *CurScope,
                                              IdentifierLoc Mode,
                                              SourceLocation PragmaLoc,
                                              XlxPragmaArgParser &PAP) {

  Preprocessor &PP = P.getPreprocessor();
  std::string ModeName = Mode.Ident->getName().lower();
  Mode.Ident = PP.getIdentifierInfo(ModeName);


  // TODO: Generate a warning to say "register" is repaced by "ap_none"
  if (str(Mode).equals_lower("register"))
    return false;

  typedef bool (*Handler)(Parser &, Scope *, IdentifierLoc, SourceLocation, XlxPragmaArgParser &);

  // NOTE: ap_bus is not supported now. Not yet have a protocol spec
  auto *Handle =
      StringSwitch<Handler>(str(Mode))
          // AXI
          .CaseLower("m_axi", HandleMAXIInterfacePragma)
          .CaseLower("axis", HandleAXISInterfacePragma)
          .CaseLower("s_axilite", HandleSAXIInterfacePragma)
          // RAM/FIFO
          .CaseLower("ap_memory", HandleBRAMInterfacePragma)
          .CaseLower("bram", HandleBRAMInterfacePragma)
          .CaseLower("ap_fifo", HandleGenericInterfacePragma)
          // Scalars
          .CaseLower("ap_none", HandleGenericInterfacePragma)
          // Handshake
          .CaseLower("ap_hs", HandleInterruptInterfacePragma)
          .CaseLower("ap_ack", HandleGenericInterfacePragma)
          .CaseLower("ap_vld", HandleInterruptInterfacePragma)
          .CaseLower("ap_ovld", HandleGenericInterfacePragma)
          // Stable
          .CaseLower("ap_stable", HandleAPStableInterfacePragma)
          // Calling conventions
          .CaseLower("ap_ctrl_none", HandleCallingConvInInterfacePragma)
          .CaseLower("ap_ctrl_hs", HandleCallingConvInInterfacePragma)
          .CaseLower("ap_ctrl_chain", HandleCallingConvInInterfacePragma)
          // empty and do noting, i.e. ap_auto instead
          .CaseLower("ap_bus", HandleApBusInterfacePragma)
          // unknown
          .Default(HandleUnknownInterfacePragma);

  return (*Handle)(P, CurScope, Mode, PragmaLoc, PAP);
}

static ArgsUnion ParseoffsetOptionForinterface(XlxPragmaArgParser &PAP, Parser &P, SourceLocation PragmaLoc) 
{
  P.TryConsumeToken(tok::equal);
  IdentifierLoc *TypeMode = nullptr;
  if (auto Sty = PAP.lookup("mode"))
    TypeMode = Sty.get<IdentifierLoc *>();

  if (auto id = PAP.presentedId("s_axilite")
   || (TypeMode && TypeMode->Ident->getName().equals_lower("s_axilite"))) {
    //parse offset as ICEExpression
    return PAP.parseICEExpression();
  }

  else if (auto id = PAP.presentedId("m_axi") 
   || (TypeMode && TypeMode->Ident->getName().equals_lower("m_axi"))) { 
    auto TypeOffset = PAP.parseIdentifierLoc();
    // following code is used to lower the "offset" value set by user
    auto OffsetStr = "empty"; // will error out in sema
    if (TypeOffset && TypeOffset->Ident->getName().equals_lower("slave"))
      OffsetStr = "slave";
    else if (TypeOffset && TypeOffset->Ident->getName().equals_lower("direct"))
      OffsetStr = "direct";
    else if (TypeOffset && TypeOffset->Ident->getName().equals_lower("off"))
      OffsetStr = "off";
    
    if(TypeOffset)
        return PAP.createIdentLoc(OffsetStr, TypeOffset->Loc);
    else 
        P.Diag(P.getCurToken(), diag::err_xlx_invalid_offset_value)
            << P.getPreprocessor().getSpelling(P.getCurToken());
  }

  else if (auto id = PAP.presentedId("ap_bus")
   || (TypeMode && TypeMode->Ident->getName().equals_lower("ap_bus"))) { 
    // This function is only to verify offset option
    // However, if ap_bus mode together with offset by mistake,
    // the checker of error_unknown_interface_mode brings confusing message.
    // So for ap_bus, do nothing here, will error out
    P.Diag(P.getCurToken().getLocation(),
           diag::err_xlx_pragma_option_not_supported_by_HLS)
        << "Interface"
        << "ap_bus mode"
        << "m_axi";
  }

  else { 
    P.Diag(PragmaLoc, diag::error_unknown_interface_mode);
  }
  return ArgsUnion();
}

static bool HandleInterfacePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                  SourceLocation PragmaLoc) {
  // FIXME Interface pragma can only be applied in function scope
  if (!CurScope->isFunctionScope()) {
    PAP.P.Diag(PragmaLoc, diag::warn_xlx_pragma_applied_in_wrong_scope)
        << "interface" << 0;
    return false;
  }

  PAP.setPragmaOption("",  true, 
#ifdef  TABLEGEN_HLS 
      PRAGMA_HLS_INTERFACE, 
#else 
      {reqVarRefExpr("port"), 

      presentId("m_axi", 6), 

      presentId("axis", 6),

      presentId("s_axilite", 6),

      presentId("ap_memory", 6),
      presentId("bram", 6),

      presentId("ap_none", 6), 

      presentId("ap_fifo", 6), 
      presentId("ap_hs", 6), 
      presentId("ap_ack", 6), 
      presentId("ap_vld", 6), 
      presentId("ap_ovld", 6),

      presentId("ap_stable", 6), 
      presentId("ap_bus", 6), 

      presentId("ap_ctrl_none", 6),
      presentId("ap_ctrl_hs", 6),
      presentId("ap_ctrl_chain", 6),

      //MAXI
       optId("bundle", "0"), optICEExpr("depth"), optCallBackParser("offset", ParseoffsetOptionForinterface), optId("name"),
       optICEExpr("num_read_outstanding"), optICEExpr("num_write_outstanding"),
       optICEExpr("max_read_burst_length"),
       optICEExpr("max_write_burst_length"), optICEExpr("latency"),
       optICEExpr("max_widen_bitwidth"), optICEExpr("channel"),

     //SAXI
       optId("bundle", "0"), presentId("register"),
       optCallBackParser("offset", ParseoffsetOptionForinterface), optId("clock"), optId("name"), optId("storage_impl"),

     //AXIS
       presentId("register"),
       optEnum("register_mode", {"forward", "reverse", "both", "off"}),
       presentId("forward", 2), presentId("reverse", 2), presentId("both", 2),
       presentId("off", 2), optICEExpr("depth"), optId("name"),


     //BRAM, AP_MEMORY
       optICEExpr("depth"), optICEExpr("latency"),
       optId("storage_type", "default"), optId("name"), 

     //AP_NONE, AP_STABLE, AP_FIFO, AP_HS, AP_VLD, AP_OVLD, AP_ACK 
       presentId("register"), optId("name"),
       optICEExpr("depth"), optICEExpr("latency"),
       optICEExpr("interrupt"), 
       optEnum("direct_io", {"true", "false"}),

    //AP_CTRL_HS, AP_CTRL_CHAIN, AP_CTRL_NONE
       optId("name"),

    //New feature to support 'mode=value':
       optEnum("mode", {"m_axi", "axis", "s_axilite", "ap_memory", "bram", "ap_fifo", "ap_none", "ap_ack", "ap_hs", "ap_vld", "ap_ovld", "ap_stable", "ap_bus", "ap_ctrl_none", "ap_ctrl_chain", "ap_ctrl_hs"})}, 
#endif
       PragmaLoc);
      
  if (!PAP.parse()) { 
    return false;
  }

  // obtain enum value
  IdentifierLoc *TypeMode = nullptr;
  if (auto Sty = PAP.lookup("mode")) {
    TypeMode = Sty.get<IdentifierLoc *>();
  }

  IdentifierLoc *Mode = nullptr ;
  if (auto id = PAP.presentedId("m_axi")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("axis")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("s_axilite")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_memory")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("bram")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_none")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_fifo")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_hs")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_ack")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_vld")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_ovld")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_stable")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_bus")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_ctrl_none")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_ctrl_hs")) { 
    Mode = id;
  }
  else if (auto id = PAP.presentedId("ap_ctrl_chain")) { 
    Mode = id;
  }

  if (TypeMode && Mode) { 
    SmallString<128> tmp; 
    PAP.P.Diag(PragmaLoc, diag::err_xlx_attribute_invalid_option_and_because)
      <<llvm::Twine(TypeMode->Ident->getName() + " and " +  Mode->Ident->getName()).toStringRef(tmp)
      <<"can not support more than one interface modes in one pragma "; 
    return false; 
  }

  if (!TypeMode && !Mode) { 
    //for "$pragma HLS  register port = return " 
    //or  "pramga HLS register port = xxx " 
    //we need warning and ignore , use need use "$pramga HLS Latency min = 1 max = 1"
    if (PAP.presentedId("register") && PAP.lookup("port")) { 
      PAP.P.Diag(PragmaLoc, diag::warn_obsolete_pragma_replaced)
          << "#pragma HLS INTERFACE port=return register"
          << "#pragma HLS LATENCY min=1 max=1";
      return false;
    }
    else { 
      PAP.P.Diag(PragmaLoc, diag::error_unknown_interface_mode);
    }
    return false;
  }

  if(TypeMode) { 
    Mode = TypeMode;
  }
  bool ValidPragma = true;
  ValidPragma &= CheckInterfacePort(PAP.P, PAP.lookup("port").get<Expr*>(), *Mode);
  ValidPragma &= CheckInterfaceDirectIO(PAP, *Mode);
  if (!ValidPragma)
    return false;
  return HandleInterfacePragmaWithPAP(PAP.P, CurScope, *Mode, PragmaLoc, PAP);
}

static bool HandleMAXIAliasPragma(XlxPragmaArgParser &PAP, Scope *CurScope, SourceLocation PragmaLoc) { 
  // FIXME Interface pragma can only be applied in function scope
  if (!CurScope->isFunctionScope()) {
    PAP.P.Diag(PragmaLoc, diag::warn_xlx_pragma_applied_in_wrong_scope)
        << "alias" << 0;
    return false;
  }

  PAP.setPragmaOption("", true,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_ALIAS, 
#else

                         { reqVarRefExpr("ports"), 
                           optVarRefExpr("offset"), optVarRefExpr("distance")},
#endif

                         PragmaLoc);

  if (!PAP.parse())
    return false;

  Expr* port_expr = PAP["ports"].get<Expr*>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> portExprs;
  getSubExprOfVariable(portExprs, port_expr, PAP.P);

  ArgsUnion offset_arg = PAP.lookup("offset");
  ArgsUnion distance_arg = PAP.lookup("distance");

  if(offset_arg && distance_arg) { 
    PAP.P.Diag(PragmaLoc, diag::warn_confilict_pragma_parameter)
        << "offset" << "distance" ;
    return false;
  } else if (!offset_arg && !distance_arg) { 
    PAP.P.Diag(PragmaLoc, diag::warn_at_least_one_parameter_required)
      <<"offset" << "distance";
    return false;
  }
  
  llvm::SmallVector<Expr*, 4> offsets;
  if (PAP.lookup("offset")) { 
    //TODO, this is ugly, we need rewrite pragma args parser
    Expr* var_expr = PAP.lookup("offset").get<Expr*>();

    SmallVector<Expr *, 4> subExprs;
    getSubExprOfVariable(subExprs, var_expr, PAP.P);

    for (auto sub : subExprs) {
      offsets.push_back(sub);
    }

    if (offsets.size() != portExprs.size()) {
      PAP.P.Diag(var_expr->getExprLoc(), diag::err_xlx_attribute_invalid_option_and_because)
          << "'Offset'"
          << "The number of offset values should match the number of ports";
      return false;
    }
  }
  else { 
    auto distance = PAP.lookup("distance").get<Expr*>();
    auto Loc = distance->getExprLoc();
    auto& Ctx = PAP.P.getActions().getASTContext();
    
    if(distance->isLValue()) {
        distance = ImplicitCastExpr::Create(Ctx, distance->getType(), CK_LValueToRValue, 
                        distance, nullptr, VK_RValue);
    }
    for(unsigned i = 0; i < portExprs.size(); ++i) {
        auto Mul = new (Ctx) BinaryOperator(distance, PAP.createIntegerLiteral(i), BO_Mul, distance->getType(), 
                                VK_RValue, OK_Ordinary, Loc, PAP.P.getActions().getFPOptions()); 
        offsets.push_back(Mul);
    }
  }

  SmallVector<ArgsUnion, 8> args;  
  args.append(portExprs.begin(), portExprs.end());
  args.append(offsets.begin(), offsets.end());
  PAP.addDependenceAttribute("xlx_maxi_alias", args);

  return true;
}


static bool HandleXlxStableContentPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                         SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS 
      PRAGMA_HLS_STABLE_CONTENT, 
#else 
      {reqVarRefExpr("variable")},
#endif
                         PragmaLoc);
  if (!PAP.parse()) {
    return false;
  }
  Expr* var_expr = PAP["variable"].get<Expr*>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
  for (auto var: subExprs) {
    ArgsUnion args[] = {var};
    PAP.addDependenceAttribute("xlx_stable_content", args);
  }
  return true;
}

static bool HandleXlxStablePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                  SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_STABLE, 
#else 
      {reqVarRefExpr("variable")},
#endif
                         PragmaLoc);
  if (!PAP.parse())
    return false;
  Expr* var_expr = PAP["variable"].get<Expr*>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
  for (auto var: subExprs) { 
    ArgsUnion args[] = {var};
    PAP.addDependenceAttribute("xlx_stable", args);
  }
  return true;
}

static bool HandleXlxSharedPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                  SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_SHARED, 
#else
      {reqVarRefExpr("variable")},
#endif
                         PragmaLoc);
  if (!PAP.parse()) {
    return false;
  }

  if (enableXilinxPragmaChecker()) {
    PAP.P.Diag(PragmaLoc, diag::warn_deprecated_pragma_ignored_by_scout)
        << "shared";
  }

  Expr* var_expr = PAP["variable"].get<Expr*>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
  for (auto var: subExprs) { 
    ArgsUnion args[] = {var};
    PAP.addDependenceAttribute("xlx_shared", args);
  }
  return true;
}

static bool HandleXlxDisaggrPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                   SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_DISAGGREGATE, 
#else 
      {reqVarRefExpr("variable")},
#endif 
                         PragmaLoc);
  if (!PAP.parse()) {
    return false;
  }
  Expr* var_expr = PAP["variable"].get<Expr*>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
  for (auto var: subExprs) { 
    ArgsUnion args[] = {var};
    PAP.addDependenceAttribute("xlx_disaggregate", args);
  }
  return true;
}

static bool HandleXlxAggregatePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                     SourceLocation PragmaLoc) {
  PAP.setPragmaOption("", false, 
#ifdef TABLEGEN_HLS 
      PRAGMA_HLS_AGGREGATE, 
#else 
      {reqVarRefExpr("variable"), presentId("bit", 1), presentId("byte", 1), presentId("none", 1), presentId("auto", 1), optEnum("compact", {"none", "bit", "byte", "auto"})},
#endif
                         PragmaLoc);
  if (!PAP.parse()) {
    return false;
  }

  IdentifierLoc *CompactMode = nullptr;
  if (auto Compact = PAP.lookup("compact"))
    CompactMode = Compact.get<IdentifierLoc *>();

  IdentifierLoc *data_pack_compact = nullptr;
  if (auto id = PAP.presentedId("none")) {
    data_pack_compact = id;
  }
  else if (auto id = PAP.presentedId("bit")) { 
    data_pack_compact = id;
  }
  else if (auto id = PAP.presentedId("byte")) { 
    data_pack_compact = id;
  }
  else if (auto id = PAP.presentedId("auto")){ 
    data_pack_compact = id;
  }
  else if (CompactMode) {
    data_pack_compact = CompactMode;
  }

  Expr* var_expr = PAP["variable"].get<Expr*>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
  for( auto var: subExprs) { 
    ArgsUnion args[] = {var, data_pack_compact};
    PAP.addDependenceAttribute("xlx_aggregate", args);
  }
  return true;
}

static bool HandleXlxBindStoragePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                       SourceLocation PragmaLoc) {

  const platform::PlatformBasic *xilinxPlatform =
      platform::PlatformBasic::getInstance();

  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_BIND_STORAGE, 
#else 
                         {reqVarRefExpr("variable"), reqId("type"), optId("impl"),
                          optICEExpr("latency", -1)},
#endif
                         PragmaLoc);

  if (!PAP.parse()) {
    return false;
  }

  auto type_ii = PAP["type"].get<IdentifierLoc *>();
  auto impl_ii = PAP["impl"].get<IdentifierLoc *>();

  std::pair<platform::PlatformBasic::OP_TYPE,
            platform::PlatformBasic::IMPL_TYPE>
      mem_impl_type = xilinxPlatform->getStorageTypeImplFromStr(
          type_ii->Ident->getName().str(), impl_ii->Ident->getName().str());

  if (mem_impl_type.first == platform::PlatformBasic::OP_UNSUPPORTED ||
      mem_impl_type.second == platform::PlatformBasic::UNSUPPORTED) {
    PAP.P.Diag(type_ii->Loc, diag::err_xlx_invalid_resource_option)
        << type_ii->Ident->getName().str() + " + " +
               impl_ii->Ident->getName().str()
        << "BIND_STORAGE's option 'type + impl'";
    return false;
  }

  auto mem_enum_expr = PAP.createIntegerLiteral(mem_impl_type.first);
  auto impl_enum_expr = PAP.createIntegerLiteral(mem_impl_type.second);

  Expr* var_expr = PAP["variable"].get<Expr*>();
  // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);

  for (auto &e : subExprs) {
    // non-functional resource can not used in function
    ArgsUnion args[] = {e, mem_enum_expr, impl_enum_expr, PAP["latency"]};
    PAP.addDependenceAttribute("xlx_bind_storage", args);
  }
  return true;
}

static bool HandleXlxBindOpPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                  SourceLocation PragmaLoc) {

  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_BIND_OP, 
#else 
                         {reqVarRefExpr("variable"), reqId("op"), optId("impl"),
                          optICEExpr("latency", -1)},
#endif
                         PragmaLoc);

  if (!PAP.parse())
    return false;

  const platform::PlatformBasic *xlxPlatform =
      platform::PlatformBasic::getInstance();
  auto opName = PAP["op"].get<IdentifierLoc *>()->Ident->getName().str();
  auto implName = PAP["impl"].get<IdentifierLoc *>()->Ident->getName().str();
  std::pair<platform::PlatformBasic::OP_TYPE,
            platform::PlatformBasic::IMPL_TYPE>
      op_impl = xlxPlatform->getOpImplFromStr(opName, implName);
  if (op_impl.first == platform::PlatformBasic::OP_UNSUPPORTED ||
      op_impl.second == platform::PlatformBasic::UNSUPPORTED) {
    PAP.P.Diag(PragmaLoc, diag::err_xlx_invalid_resource_option)
        << opName + " + " + implName 
        << "bind_op" ;

    return false;
  }
  auto op_enum_expr = PAP.createIntegerLiteral(op_impl.first);
  auto impl_enum_expr = PAP.createIntegerLiteral(op_impl.second);

  Expr* var_expr = PAP["variable"].get<Expr*>();
   // parse "expr1, expr2, expr3"
  SmallVector<Expr *, 4> subExprs;
  getSubExprOfVariable(subExprs, var_expr, PAP.P);
  for (Expr *expr : subExprs) {
    ArgsUnion args[] = {expr, op_enum_expr, impl_enum_expr, PAP["latency"]};
    if (isa<DeclRefExpr>(expr) && isa<FunctionDecl>(cast<DeclRefExpr>(expr)->getDecl())) { 
      PAP.P.Diag(PragmaLoc, diag::err_xlx_attribute_invalid_option_and_because)
        << "'Variable'" 
        << "Bind_op should not be applied on return/function"; 
      return false;
    }
    PAP.addDependenceAttribute("xlx_bind_op", args);
  }
  return true;
}

// TODO, rewrite it support template argument expression in Distance option
static bool HandleXlxDependencePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                      SourceLocation PragmaLoc) {


  PAP.setPragmaOption("", false,
#ifdef  TABLEGEN_HLS
      PRAGMA_HLS_DEPENDENCE, 
#else 
      {optVarRefExpr("cross_variables"), optVarRefExpr("variable"), presentId("pointer", 1),
       presentId("array", 1), presentId("intra", 2), presentId("inter", 2),
       presentId("raw", 3), presentId("war", 3), presentId("waw", 3),
       optICEExpr("distance", -1), presentId("false", 0), presentId("true", 0),
       optEnum("class", {"array", "pointer"}),
       optEnum("type", {"intra", "inter"}),
       optEnum("dependent", {"true", "false"}),
       optEnum("direction", {"raw", "war", "waw"})},
#endif 
      PragmaLoc);

  if (!PAP.parse())
    return false;

  // obtain multi enum values
  IdentifierLoc *ClassMode = nullptr;
  IdentifierLoc *TypeMode = nullptr;
  IdentifierLoc *DependentMode = nullptr;
  IdentifierLoc *DirectionMode = nullptr;
  if (auto Sty = PAP.lookup("class"))
    ClassMode = Sty.get<IdentifierLoc *>();
  if (auto Sty = PAP.lookup("type"))
    TypeMode = Sty.get<IdentifierLoc *>();
  if (auto Sty = PAP.lookup("dependent"))
    DependentMode = Sty.get<IdentifierLoc *>();
  if (auto Sty = PAP.lookup("direction"))
    DirectionMode = Sty.get<IdentifierLoc *>();

  // deal with true/false
  IntegerLiteral *isDep = NULL;
  if (auto false_id = PAP.presentedId("false")) {
    // Just warning to user this old option will be deleted in future
    isDep = PAP.createIntegerLiteral(0, false_id->Loc);
  } 
  else if (auto true_id = PAP.presentedId("true")) {
    isDep = PAP.createIntegerLiteral(1);
  }
  else if (DependentMode) {
    if (DependentMode->Ident->getName().equals_lower("false"))
      isDep = PAP.createIntegerLiteral(0);
    else if (DependentMode->Ident->getName().equals_lower("true"))
      isDep = PAP.createIntegerLiteral(1);
  }

  IdentifierLoc *dep_class = nullptr, *dep_type = nullptr, *dep_direction = nullptr;
  if (auto pointer_id = PAP.presentedId("pointer")) {
    dep_class = pointer_id;
  } 
  else if (auto array_id = PAP.presentedId("array")) {
    dep_class = array_id;
  }
  else if (ClassMode) {
    dep_class = ClassMode;
  }
  else {
    dep_class = nullptr;
  }

  if (auto intra_id = PAP.presentedId("intra")) {
    dep_type = intra_id;
  } 
  else if (auto inter_id = PAP.presentedId("inter")) {
    dep_type = inter_id;
  }
  else if (TypeMode) {
    dep_type = TypeMode;
  }
  else {
    dep_type = nullptr;
  }

  if (auto raw_id = PAP.presentedId("raw")) {
    dep_direction = PAP.createIdentLoc("RAW");
  } 
  else if (auto war_id = PAP.presentedId("war")) {
    dep_direction = PAP.createIdentLoc("WAR");
  }
  else if (auto raw_id = PAP.presentedId("waw")) {
    dep_direction = PAP.createIdentLoc("WAW");
  }
  else if (DirectionMode) {
    if (DirectionMode->Ident->getName().equals_lower("raw"))
      dep_direction = PAP.createIdentLoc("RAW");
    else if (DirectionMode->Ident->getName().equals_lower("war"))
      dep_direction = PAP.createIdentLoc("WAR");
    else if (DirectionMode->Ident->getName().equals_lower("waw"))
      dep_direction = PAP.createIdentLoc("WAW");
  }
  else {
    dep_direction = nullptr;
  }

  //distance -1 meaning, there is no distance  option  appear 
  //
  Expr *distance = nullptr;
  if ( PAP.lookup("distance")) { 
    auto distance_arg = PAP.lookup("distance");
    distance = distance_arg.get<Expr*>();
    //we checked distance is more than '0' in parser 
    if (isa<IntegerLiteral>(distance) && cast<IntegerLiteral>(distance)->getValue().getZExtValue() < 0 ) { 
      PAP.P.Diag(PragmaLoc, diag::err_xlx_attribute_invalid_option_and_because)
        << "'Distance'" 
        << "Valid value for 'Distance' option should be greater than '0'";
      return false ;
    }
  }
  else { 
    //use default distance value "-1" to stand for  "distance option is missing"
    distance = nullptr;
  }


  auto cross_variables = PAP["cross_variables"].get<Expr*>();
  auto var_expr = PAP["variable"].get<Expr*>();
  if (cross_variables && var_expr)  {
    PAP.P.Diag(PragmaLoc, diag::warn_confilict_pragma_parameter)
        << "cross_variable" << "variable" ;
    return false;
  }

  if (var_expr) {
    SmallVector<Expr *, 4> subExprs;
    getSubExprOfVariable(subExprs, var_expr, PAP.P);
    for (unsigned i = 0; i < subExprs.size(); i++) {
      ArgsUnion args[] = {subExprs[i],   dep_class, dep_type,
                          dep_direction, distance,  isDep};
      PAP.addDependenceAttribute("xlx_dependence", args);
    }
  } 
  else if (cross_variables) { 
    // parse "expr1, expr2, expr3"
    SmallVector<Expr *, 4> subExprs;
    getSubExprOfVariable(subExprs, cross_variables, PAP.P);
    if (subExprs.size() != 2) { 
      //TODO, warning + ignore
      return false;
    }
    ArgsUnion args[] = { subExprs[0], subExprs[1], dep_class, dep_type, dep_direction, distance, isDep };
    PAP.addDependenceAttribute("xlx_cross_dependence", args );
  }
  else {
    ArgsUnion args[] = {(Expr*)nullptr, dep_class, dep_type,
                        dep_direction,   distance,  isDep};
    PAP.addDependenceAttribute("xlx_dependence", args);
  }
  return true;
}

static bool HandleXlxArrayStencilPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                                    SourceLocation PragmaLoc) {
  SubjectListTy SubjectList;
  PAP.setPragmaOption("", false,
#ifdef TABLEGEN_HLS
      PRAGMA_HLS_ARRAY_STENCIL, 
#else 
                         {optVarRefExpr("variable"), presentId("off", 1)},
#endif 
                         PragmaLoc);
  if (!PAP.parse())
    return false;

  auto var_expr = PAP["variable"].get<Expr*>();
  if (var_expr) {
    SmallVector<Expr *, 4> subExprs;
    getSubExprOfVariable(subExprs, var_expr, PAP.P);
    for (unsigned i = 0; i < subExprs.size(); i++) {
      ArgsUnion args[] = {subExprs[i], PAP["off"]};
      PAP.addDependenceAttribute("fpga_array_stencil", args);
    }
  } 

  return true;
}

static bool HandleCachePragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                              SourceLocation PragmaLoc) {
  // FIXME cache pragma can only be applied in function scope
  if (!CurScope->isFunctionScope()) {
    PAP.P.Diag(PragmaLoc, diag::warn_xlx_pragma_applied_in_wrong_scope)
        << "cache" << 0;
    return false;
  }

  PAP.setPragmaOption("",  true,
#ifdef  TABLEGEN_HLS 
      PRAGMA_HLS_CACHE, 
#else 
      {reqVarRefExpr("port"),
       optICEExpr("lines"), optICEExpr("depth"), /*optICEExpr("ways"),*/
       /*optICEExpr("users"),*/
     //optEnum("burst", {"off", "on"/*, "adaptive"*/}),
       optEnum("write_mode", {"write_back", "write_through"})},
#endif
       PragmaLoc);
      
  if (!PAP.parse()) { 
    return false;
  }

  IdentifierLoc *burst;
  if (auto b = PAP.lookup("burst")) {
    burst = b.get<IdentifierLoc *>();
  } else {
    burst = PAP.createIdentLoc("on");
  }

  IdentifierLoc *write_mode;
  if (auto w = PAP.lookup("write_mode")) {
    write_mode = w.get<IdentifierLoc *>();
  } else {
    write_mode = PAP.createIdentLoc("write_back");
  }

  Expr *var_expr = PAP.lookup("port").get<Expr*>();
  if (var_expr) {
    SmallVector<Expr *, 4> subExprs;
    getSubExprOfVariable(subExprs, var_expr, PAP.P);
    for (auto port_ref : subExprs) {
      ArgsUnion args[] = { port_ref, PAP.lookup("lines"), PAP.lookup("depth"),
                           /*PAP.lookup("ways"), PAP.lookup("users"),*/
                           PAP.createIntegerLiteral(1), PAP.createIntegerLiteral(1),
                          /*burst,*/
                          PAP.createIdentLoc("off"),
                          write_mode
                         };
      PAP.addDependenceAttribute("xlx_cache", args);
    }
    return true;
  } else {
    return false;
  }
}

static bool UnsupportPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                            SourceLocation PragmaLoc) {
  if (enableXilinxPragmaChecker()) {
    PAP.P.Diag(PragmaLoc, diag::err_xlx_pragma_not_supported_by_scout_HLS_WarnOut);
    return false;
  }
  return false; // not valid
}

static bool HandleUnknownPragma(XlxPragmaArgParser &PAP, Scope *CurScope,
                               SourceLocation PragmaLoc) {
  PAP.P.Diag(PragmaLoc, diag::warn_xlx_pragma_ignored) ;
  return false; // not valid
}

/// \brief Handle the annotation token produced for #pragma HLS|AP|AUTOPILOT ...
void Parser::HandleXlxPragma() {
  getActions().EnterHLSParsing();
  auto RAII = llvm::make_scope_exit([this]() {
      // Ensure that we consume all tokens of the pragma, even when we exit early
      // due to errors.
      // Don't use SkipUntil because it has additional logic for braces etc.
      while (!Tok.is(tok::eof)) {
        if (Tok.is(tok::annot_pragma_XlxHLS_end)) {
          ConsumeAnyToken();
          break;
        }
        ConsumeAnyToken();
      }
    });


  assert((Tok.is(tok::annot_pragma_XlxHLS_directive) || 
      Tok.is(tok::annot_pragma_XlxHLS) || 
      Tok.is(tok::annot_pragma_XlxHLS_old) || 
      Tok.is(tok::annot_pragma_XlxSLX_directive))&& 
      "unexpected, only support #pragma HLS/SLXDIRECTIVE/HLSDIRECTIVE/AP/AUTOPILOT" );

  std::string contextStr = "";

  if (Tok.is(tok::annot_pragma_XlxHLS_directive))
    contextStr = "directive";
  else if (Tok.is(tok::annot_pragma_XlxSLX_directive))
    contextStr = "slx_directive"; 
  else if (Tok.is(tok::annot_pragma_XlxHLS_old))
    contextStr = "deprecated";


  IdentifierInfo* annotation = (IdentifierInfo*)Tok.getAnnotationValue(); 

  SourceLocation PragmaLoc =
      ConsumeAnnotationToken(); // Consume tok::annot_pragma_XlxHLS.

  if (getLangOpts().OpenCL) {
    Diag(PragmaLoc, diag::warn_xlx_pragma_ignored);
    return;
  }


  // Return when hit pragma end
  if (Tok.is(tok::annot_pragma_XlxHLS_end))
    return; 

  // Do not fail on #pragma HLS inline
  Expr *ifCond = nullptr; 
  if (Tok.is(tok::kw_if)) { 
    auto Name = Tok.getIdentifierInfo();
    //consume 'if' keyworkd
    auto IfLoc = ConsumeToken();
    // Parse the condition.
    StmtResult InitStmt;
    Sema::ConditionResult Cond;
    if (ParseHLSIfCond(&InitStmt, Cond, IfLoc)){ 

      Diag(IfLoc, diag::err_xlx_pragma_if_cond_constexpr) ; 
    }
    else { 
      ifCond = Cond.get().second; 
      if (!InitStmt.isUnset()) { 
        Diag(IfLoc, diag::err_xlx_pragma_if_cond_init_var) ; 
      }
    }
  }

  // Do not fail if the pragma end right after the beginning
  if (Tok.is(tok::annot_pragma_XlxHLS_end))
    return;

  auto CurScope = getCurScope();

  auto MaybeName = ParseXlxPragmaArgument(*this);
  if (!MaybeName.Ident)
    return;

  if (!getLangOpts().HLSSLX && contextStr == "slx_directive") { 
    if (!MaybeName.Ident->getName().equals_lower("performance") && 
        !MaybeName.Ident->getName().equals_lower("loop_tripcount") && 
        !MaybeName.Ident->getName().equals_lower("dependence")) { 
      return ; 
    }
  }


  typedef bool (*Handler)(XlxPragmaArgParser & PAP, Scope * CurScope,
                          SourceLocation PragmaLoc);

  auto *Handle =
      StringSwitch<Handler>(str(MaybeName))
          .CaseLower("dataflow", HandleXlxDataflowPragma)
          .CaseLower("pipeline", HandleXlxPipelinePragma)
          .CaseLower("unroll", HandleXlxUnrollPragma)
          .CaseLower("loop_flatten", HandleXlxFlattenPragma)
          .CaseLower("loop_merge", HandleXlxMergePragma)
          .CaseLower("loop_tripcount", HandleLoopTripCountPragma)
          .CaseLower("inline", HandleInlinePragma)
          .CaseLower("interface", HandleInterfacePragma)
          .CaseLower("resource", HandleResourcePragma)
          .CaseLower("stream", HandleStreamPragma)
          .CaseLower("reset", HandleResetPragma)
          .CaseLower("allocation", HandleAllocationPragma)
          .CaseLower("expression_balance", HandleExpressionBanlancePragma)
          .CaseLower("function_instantiate", HandleFunctionInstantiatePragma)
          .CaseLower("array_partition", HandleArrayPartitionPragma)
          .CaseLower("array_reshape", HandleArrayReshapePragma)
          .CaseLower("top", HandleTopFunctionPragma)
          .CaseLower("occurrence", HandleOccurrencePragma)
          .CaseLower("protocol", HandleProtocolPragma)
          .CaseLower("latency", HandleLatencyPragma)
          .CaseLower("dependence", HandleXlxDependencePragma)
          .CaseLower("stable", HandleXlxStablePragma)
          .CaseLower("stable_content", HandleXlxStableContentPragma)
          .CaseLower("shared", HandleXlxSharedPragma)
          .CaseLower("disaggregate", HandleXlxDisaggrPragma)
          .CaseLower("aggregate", HandleXlxAggregatePragma)
          .CaseLower("bind_op", HandleXlxBindOpPragma)
          .CaseLower("bind_storage", HandleXlxBindStoragePragma)
          .CaseLower("extract", UnsupportPragma)
          .CaseLower("REGION", UnsupportPragma)
          .CaseLower("array_map", UnsupportPragma)
          .CaseLower("clock", HandleClockPragma)
          .CaseLower("alias", HandleMAXIAliasPragma)
          .CaseLower("data_pack", HandleDataPackPragma)
          .CaseLower("array_stencil", HandleXlxArrayStencilPragma)
          .CaseLower("performance", HandlePerformancePragma)
          .CaseLower("cache", HandleCachePragma)
          .Default(HandleUnknownPragma);

  SubjectListTy subjects; 
  XlxPragmaArgParser PAP( *this, CurScope, subjects, annotation, ifCond ); 

  SourceLocation startLoc = Tok.getLocation();
  const clang::FunctionDecl * FD = getActions().getCurFunctionDecl();
  bool isValid = (*Handle)(PAP, CurScope, PragmaLoc);
  SourceLocation endLoc = Tok.getLocation();
  StringRef allOptions = Lexer::getSourceText(CharSourceRange::getTokenRange(startLoc, endLoc), PP.getSourceManager(), LangOptions());
        
  std::ostringstream pragmaDetails;
  pragmaDetails 
    << "_XLX_SEP_ PragmaIsValid=" << isValid
    << "_XLX_SEP_ PragmaType=" << str(MaybeName).lower()
    << "_XLX_SEP_ PragmaContext=" << contextStr 
    << "_XLX_SEP_ PragmaFunction=" << (FD ? FD->getNameAsString() : "")
    << "_XLX_SEP_ PragmaOptions=" << allOptions.str()
    << "_XLX_SEP_";

  FullSourceLoc fullLoc(PragmaLoc, getPreprocessor().getSourceManager()); 

  if (!XilinxSystemInfo::isSystemHLSHeaderFile(fullLoc.getPresumedLoc().getFilename())) { 
    Diag(PragmaLoc, diag::dump_xlx_pragma)  
      << pragmaDetails.str();
  }
    
  getActions().ExitHLSParsing();

  // if don't consume left tokens in pramga decl, parser will automatically
  // eating left tokens in pragma until to  pragma end token ( general, pragma
  // end  is a special line change char without prefix "\" ? )
  //
  // following code will cause  parse failed
  // SkipUntil(tok::annot_pragma_XlxHLS_end,
  //        Parser::StopBeforeMatch);
  // ConsumeAnnotationToken();
  //
}
