//===-- ValueLatticeUtils.cpp - Utils for solving lattices ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
// And has the following additional copyright:
// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
//
//===----------------------------------------------------------------------===//
//
// This file implements common functions useful for performing data-flow
// analyses that propagate values across function boundaries.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/ValueLatticeUtils.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/XILINXFunctionInfoUtils.h"
using namespace llvm;

bool llvm::canTrackArgumentsInterprocedurally(Function *F) {
  // HLS begin
  // Don't touch Vivado IP
  if (HasVivadoIP(F))
    return false;
  // HLS end

  return F->hasLocalLinkage() && !F->hasAddressTaken();
}

bool llvm::canTrackReturnsInterprocedurally(Function *F) {
  // HLS begin
  // Don't touch Vivado IP
  if (HasVivadoIP(F))
    return false;
  // HLS end
  return F->hasExactDefinition() && !F->hasFnAttribute(Attribute::Naked);
}

bool llvm::canTrackGlobalVariableInterprocedurally(GlobalVariable *GV) {
  if (GV->isConstant() || !GV->hasLocalLinkage() ||
      !GV->hasDefinitiveInitializer())
    return false;
  return !any_of(GV->users(), [&](User *U) {
    if (auto *Store = dyn_cast<StoreInst>(U)) {
      if (Store->getValueOperand() == GV || Store->isVolatile())
        return true;
    } else if (auto *Load = dyn_cast<LoadInst>(U)) {
      if (Load->isVolatile())
        return true;
    } else {
      return true;
    }
    return false;
  });
}
