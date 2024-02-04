//===- TreeHeightReduction.h - Minimize the height of an operation tree ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_TREEHEIGHTREDUCTION_H
#define LLVM_TRANSFORMS_SCALAR_TREEHEIGHTREDUCTION_H

#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Analysis/TargetTransformInfo.h"

namespace llvm {

namespace thr LLVM_LIBRARY_VISIBILITY {
  class LegacyTreeHeightReductionPass;
}
class TreeHeightReductionPass : public PassInfoMixin<TreeHeightReductionPass> {
public:
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM,
                        LoopStandardAnalysisResults &AR, LPMUpdater &U);
  friend class llvm::thr::LegacyTreeHeightReductionPass;
private:  
  bool runImpl(Loop &L, TargetTransformInfo *TTI);
};

Pass* createLegacyTreeHeightReductionPass();

} // namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_TREEHEIGHTREDUCTION_H
