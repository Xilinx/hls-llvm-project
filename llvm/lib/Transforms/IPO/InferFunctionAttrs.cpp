//===- InferFunctionAttrs.cpp - Infer implicit function attributes --------===//
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

#include "llvm/Transforms/IPO/InferFunctionAttrs.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BuildLibCalls.h"
using namespace llvm;

#define DEBUG_TYPE "inferattrs"

STATISTIC(NumInaccessibleMemOrArgMemOnly,
          "Number of functions marked as inaccessiblemem_or_argmemonly");

static bool inferAllPrototypeAttributes(Function &F,
                                        const TargetLibraryInfo &TLI,
                                        const TargetTransformInfo &TTI) {
  // We only infer things using the prototype and the name; we don't need
  // definitions.
  if (F.isDeclaration() && !F.hasFnAttribute((Attribute::OptimizeNone))) {
    if (F.getName() == "__assert_fail" &&
        TTI.isLegalToInferAttributeForFunction(&F)) {
      // Infer inaccessiblemem_or_argmemonly attribute for __assert_fail if
      // we're with supported target.
      if (F.onlyAccessesArgMemory() || F.onlyAccessesInaccessibleMemory() ||
          F.onlyAccessesInaccessibleMemOrArgMem())
        return false;

      F.setOnlyAccessesInaccessibleMemOrArgMem();
      ++NumInaccessibleMemOrArgMemOnly;
      return true;
    }

    return inferLibFuncAttributes(F, TLI);
  }

  return false;
}

PreservedAnalyses InferFunctionAttrsPass::run(Module &M,
                                              ModuleAnalysisManager &AM) {
  auto &TLI = AM.getResult<TargetLibraryAnalysis>(M);
  bool Changed = false;

  auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  for (Function &F : M.functions()) {
    auto &TTI = FAM.getResult<TargetIRAnalysis>(F);
    Changed |= inferAllPrototypeAttributes(F, TLI, TTI);
  }

  // If we didn't infer anything, preserve all analyses.
  if (!Changed)
    return PreservedAnalyses::all();

  // Otherwise, we may have changed fundamental function attributes, so clear
  // out all the passes.
  return PreservedAnalyses::none();
}

namespace {
struct InferFunctionAttrsLegacyPass : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  InferFunctionAttrsLegacyPass() : ModulePass(ID) {
    initializeInferFunctionAttrsLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
  }

  bool runOnModule(Module &M) override {
    if (skipModule(M))
      return false;

    auto &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
    bool Changed = false;

    for (Function &F : M.functions()) {
      auto &TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
      Changed |= inferAllPrototypeAttributes(F, TLI, TTI);
    }

    return Changed;
  }
};
}

char InferFunctionAttrsLegacyPass::ID = 0;
INITIALIZE_PASS_BEGIN(InferFunctionAttrsLegacyPass, "inferattrs",
                      "Infer set function attributes", false, false)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_END(InferFunctionAttrsLegacyPass, "inferattrs",
                    "Infer set function attributes", false, false)

Pass *llvm::createInferFunctionAttrsLegacyPass() {
  return new InferFunctionAttrsLegacyPass();
}
