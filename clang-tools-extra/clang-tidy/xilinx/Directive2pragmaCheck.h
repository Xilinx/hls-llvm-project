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

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_XILINX_DIRECTIVE2PRAGMA_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_XILINX_DIRECTIVE2PRAGMA_H

#include "clang/Basic/HLSDirective.h"
#include "../ClangTidy.h"
#include "llvm/ADT/MapVector.h"

namespace clang {
namespace tidy {
namespace xilinx {

/// FIXME: Write a short description.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/xilinx-directive2pragma.html


enum ValidVariableKind {
  VAR_None,
  VAR_Has,
  VAR_Invalid,
};

enum PargmaVariableKind {
  VAR_No,
  VAR_NoResource,
  VAR_Resource,
  VAR_ResourceCore,
};

enum VarScope {
  Inv_Scope,
  VAR_Local,
  VAR_Param,
  VAR_Global,
  VAR_Field,
};

enum ErrorType {
  ERR_None,
  WARN_Ambiguous_Function,
  WARN_Ambiguous_Variable,
  WARN_Ambiguous_Label,
  WARN_RESOURCE_ASSIGNMENT_ISSUE,
  WARN_END,
  ERR_CONTAINS_GOTO_STMT,
  ERR_CALSS_MEMBER_ACCESS,
  ERR_Variable_Invalid,
  ERR_Variable_Not_Exist,
  ERR_Function_Not_Exist,
  ERR_Label_Not_Exist,
  Err_Location_Missed
};
class VariableInfo {
public:
  VariableInfo()
      : Scope(Inv_Scope), VarorMemDecl(nullptr), VariableDeclStmt(nullptr),
        AssignStmt(nullptr) {}

  enum VarScope Scope;
  std::string VarName;
  NamedDecl *VarorMemDecl;
  DeclStmt *VariableDeclStmt;
  Stmt *AssignStmt;
};

class DirectiveInfo {
public:
  DirectiveInfo()
      : Type(ERR_None), Kind(VAR_No), FunDecl(nullptr), LabDecl(nullptr),
        TmpLoc(), Context(nullptr), SM(nullptr), VarInfo(nullptr) {}

  enum ErrorType Type;
  SourceLocation ErrLoc;
  enum PargmaVariableKind Kind;
  FunctionDecl *FunDecl;
  LabelDecl *LabDecl;
  SourceLocation TmpLoc;
  ASTContext *Context;
  SourceManager *SM;
  VariableInfo *VarInfo;
  StringRef FromSLX; 
};

class Directive2pragmaCheck : public ClangTidyCheck {
public:
  Directive2pragmaCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) { 
    if (auto fileOpt = Context->getOptions().ImportDirective ) { 
      FileName = fileOpt.getValue(); 
    }
  }
  ast_matchers::DeclarationMatcher
  FieldMatchers(ast_matchers::MatchFinder *Finder, std::string Fieldname,
                std::string Recordname);
  void AssignMatchers(ast_matchers::MatchFinder *Finder,
                      ast_matchers::DeclarationMatcher &vardecl,
                      DirectiveDesc *Directived);
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  // void onStartOfTranslationUnit() override;
  void onEndOfTranslationUnit() override;
  void eliminateDirective();
  ValidVariableKind checkVariableValidity(DirectiveDesc *Directived,
                                          const FunctionDecl *FuncDecl,
                                          std::string &Name);
  void SetPragma(DirectiveDesc *Directived, SourceLocation Loc,
                 SourceManager *SM, ASTContext *Context, StringRef Name,
                 bool before = false);
  void SetPragmawithResource(DirectiveDesc *Directived, DirectiveInfo *DInfo);
  void insertPragmaWithVar();
  bool hasPragmaOptionCore(DirectiveDesc *Directived, std::string &core);
  void collectVarInfo(DirectiveDesc *Directived, ASTContext *Context,
                      const NamedDecl *VariableDecl,
                      const DeclStmt *VariableDeclStmt, const Stmt *AssignStmt);
  bool hasInvalidDataMember(std::string OrignalName,
                            const VarDecl *VariableDecl,
                            DirectiveDesc *Directived);
  void collectFunctionInfo(DirectiveDesc *Directived, SourceManager *SM,
                           ASTContext *Context,
                           const FunctionDecl *MatchedFunction,
                           const LabelDecl *MatchedLabel, SourceLocation &Loc);
  int insertPragmaintoFunction(
      DirectiveDesc *Directived, const FunctionDecl *MatchedFunction,
      const ast_matchers::MatchFinder::MatchResult &Result);
  int insertPragmaintoLabel(
      DirectiveDesc *Directived, const LabelDecl *MatchedLabel,
      const ast_matchers::MatchFinder::MatchResult &Result);
  bool containGotoStmt(Stmt *stmt);
  Stmt *
  needInsertIntoBrace(Stmt *stmt,
                      const ast_matchers::MatchFinder::MatchResult &Result,
                      SourceLocation &Loc);
  std::string dumpSetDirectiveLineNo(DirectiveDesc *Directived);
  std::string dumpPragma(DirectiveDesc *Directived);
  void setDiagMsg(DirectiveDesc *Pragma, DirectiveInfo *Info);

private:
  bool tryReadDirectiveFile();
  std::error_code parseDirective(StringRef DirectiveBuff);

  std::string FileName;
  std::vector<DirectiveDesc> DirectiveList;

  // store default func unit;
  std::vector<std::string> ListCore;
  // DirectiveLists index  map to successful inserted pragma for each tu
  llvm::MapVector<DirectiveDesc *, std::string> InsertedPragma;
  // for each directive collect info
  llvm::MapVector<DirectiveDesc *, DirectiveInfo *> PragmaInfo;
};

} // namespace xilinx
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_XILINX_DIRECTIVE2PRAGMA_H
