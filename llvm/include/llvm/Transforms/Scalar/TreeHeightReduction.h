// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
//===- TreeHeightReduction.h - Minimize the height of an operation tree ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_TREEHEIGHTREDUCTION_H
#define LLVM_TRANSFORMS_SCALAR_TREEHEIGHTREDUCTION_H

#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

namespace thr LLVM_LIBRARY_VISIBILITY {
  class LegacyTreeHeightReductionPass;
}
class TreeHeightReductionPass : public PassInfoMixin<TreeHeightReductionPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
  friend class llvm::thr::LegacyTreeHeightReductionPass;
private:  
  bool runImpl(Function &F, TargetTransformInfo *TTI);
};

Pass* createLegacyTreeHeightReductionPass();

} // namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_TREEHEIGHTREDUCTION_H
