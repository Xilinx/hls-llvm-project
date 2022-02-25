// (c) Copyright 2016-2021 Xilinx, Inc.
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

#include "AggregateOnHLSVectorCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "xilinx-aggregate-on-hls-vector"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace xilinx {

static bool IsHLSVectorType(QualType Ty) {
  auto *BTy = Ty.getCanonicalType().getTypePtr();

  if (Ty->isReferenceType() || Ty->isPointerType())
    BTy = Ty->getPointeeType().getTypePtr();

  if (BTy->isStructureOrClassType() && !BTy->getAsTagDecl()
                                           ->getCanonicalDecl()
                                           ->getQualifiedNameAsString()
                                           .compare("hls::vector"))
    return true;

  return false;
}


void AggregateOnHLSVectorCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(functionDecl(isDefinition(), hasAttr(attr::SDxKernel),
                                  unless(cxxMethodDecl()))
                         .bind("top"),
                     this);
}

void AggregateOnHLSVectorCheck::check(const MatchFinder::MatchResult &Result) {

  // Get top function declaration.
  const auto *FuncDecl = Result.Nodes.getNodeAs<FunctionDecl>("top");

  if (FuncDecl->getNumParams() == 0u)
    return;

  SourceLocation TopBodyStart = Lexer::getLocForEndOfToken(
      cast<CompoundStmt>(FuncDecl->getBody())->getLBracLoc(), 0,
      *Result.SourceManager, Result.Context->getLangOpts());

  for (ParmVarDecl *VD : FuncDecl->parameters()) {
    QualType Ty = VD->getType();
    if (!Ty->isReferenceType() && !Ty->isPointerType())
      continue;

    if (IsHLSVectorType(Ty)) {
      // Apply aggregate pragma to the parameter of top if it's hls::vector
      // type.
      std::string Param = VD->getQualifiedNameAsString();
      std::string Aggr = "\n#pragma HLS aggregate variable = " + Param + "\n";
      diag(VD->getLocation(),
           " hls::vector type top function variable %0 requires aggregation")
          << Param << FixItHint::CreateInsertion(TopBodyStart, Aggr);

      // Add align attribute to hls_vector type parameter
      SourceLocation ParamEnd = Lexer::getLocForEndOfToken(
          VD->getLocEnd(), 0,
          *Result.SourceManager, Result.Context->getLangOpts());
      std::string Align = Ty->getPointeeType().getAsString();
      std::string Anno = " __attribute__((align_value(alignof(" + Align
                           + ")))) ";
      diag(VD->getLocation(),
           " hls::vector type top function variable %0 requires align attribute")
          << Param << FixItHint::CreateInsertion(ParamEnd, Anno);

    }
  }
}

} // namespace xilinx
} // namespace tidy
} // namespace clang
