//===--- ExprHLS.h - Classes for representing expressions ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// And has the following additional copyright:
//
// (C) Copyright 2016-2022 Xilinx, Inc.
// All Rights Reserved.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Expr interface and subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_EXPRHLS_H
#define LLVM_CLANG_AST_EXPRHLS_H

#include "clang/AST/Expr.h"

namespace clang {

/// To specify a whole array in HLS pragma variable exprssion, array subscript expressions
/// are extended with the following syntax:
/// \code
/// [*]
/// \endcode
/// the whole array expr represent the whole array of the original array.

class HLSWholeArrayExpr : public Expr {
    Stmt *Base;
    SourceLocation StarLoc;
    SourceLocation RBracketLoc;

public:
    HLSWholeArrayExpr(Expr* Base, QualType Type, ExprValueKind VK, ExprObjectKind OK,
                      SourceLocation StarLoc, SourceLocation RBracketLoc) 
        : Expr(
                 HLSWholeArrayExprClass, Type, VK, OK, 
                 Base->isTypeDependent(),
                 Base->isValueDependent(),
                 Base->isInstantiationDependent(),
                 Base->containsUnexpandedParameterPack()),
                 Base(Base), StarLoc(StarLoc), RBracketLoc(RBracketLoc) { }

    /// Create an empty whole array expression.
    explicit HLSWholeArrayExpr(EmptyShell Shell) : Expr(HLSWholeArrayExprClass, Shell) { }

    /// Get base of the whole array expression.
    Expr* getBase() { return cast<Expr>(Base); }
    const Expr* getBase() const { return cast<Expr>(Base); }

    /// Set base of the whole array expression.
    void setBase(Expr* E) { Base = E; }

    /// Return original type of the base expression for whole array expr.
    static QualType getBaseOriginalType(const Expr *Base);

    SourceLocation getLocStart() const LLVM_READONLY { return getBeginLoc(); }
    SourceLocation getBeginLoc() const LLVM_READONLY { return getBase()->getLocStart(); }

    SourceLocation getLocEnd() const LLVM_READONLY { return getEndLoc(); }
    SourceLocation getEndLoc() const LLVM_READONLY { return RBracketLoc; }
    
    SourceLocation getStarLoc()  const { return StarLoc; }
    void setStarLoc(SourceLocation L) { StarLoc = L; }

    SourceLocation getRBracketLoc()  const { return RBracketLoc; }
    void setRBracketLoc(SourceLocation L) { RBracketLoc = L; }
    
    SourceLocation getExprLoc()  const { return getBase()->getExprLoc(); }

    static bool classof(const Stmt *T) {
        return T->getStmtClass() == HLSWholeArrayExprClass;
    }
    
    /// Iterators
    child_range children() { return child_range(&Base, &Base + 1); }
    const_child_range children() const { return const_child_range(&Base, &Base + 1); }
};

} // end namespace clang

 #endif

