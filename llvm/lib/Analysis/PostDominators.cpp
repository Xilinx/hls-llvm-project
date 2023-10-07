//===- PostDominators.cpp - Post-Dominator Calculation --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// And has the following additional copyright:
//
// Copyright (C) 2023, Advanced Micro Devices, Inc.
// All Rights Reserved.
//
//===----------------------------------------------------------------------===//
//
// This file implements the post-dominator construction algorithms.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "postdomtree"

//===----------------------------------------------------------------------===//
//  PostDominatorTree Implementation
//===----------------------------------------------------------------------===//

char PostDominatorTreeWrapperPass::ID = 0;

INITIALIZE_PASS(PostDominatorTreeWrapperPass, "postdomtree",
                "Post-Dominator Tree Construction", true, true)

bool PostDominatorTree::invalidate(Function &F, const PreservedAnalyses &PA,
                                   FunctionAnalysisManager::Invalidator &) {
  // Check whether the analysis, all analyses on functions, or the function's
  // CFG have been preserved.
  auto PAC = PA.getChecker<PostDominatorTreeAnalysis>();
  return !(PAC.preserved() || PAC.preservedSet<AllAnalysesOn<Function>>() ||
           PAC.preservedSet<CFGAnalyses>());
}

bool PostDominatorTree::dominates(const Instruction *I1,
                                  const Instruction *I2) const {
  assert(I1 && I2 && "Expecting valid I1 and I2");

  const BasicBlock *BB1 = I1->getParent();
  const BasicBlock *BB2 = I2->getParent();

  if (BB1 != BB2)
    return Base::dominates(BB1, BB2);

  // PHINodes in a block are unordered.
  if (isa<PHINode>(I1) && isa<PHINode>(I2))
    return false;

  return I2->comesBefore(I1);
}

bool PostDominatorTreeWrapperPass::runOnFunction(Function &F) {
  DT.recalculate(F);
  return false;
}

void PostDominatorTreeWrapperPass::print(raw_ostream &OS, const Module *) const {
  DT.print(OS);
}

FunctionPass* llvm::createPostDomTree() {
  return new PostDominatorTreeWrapperPass();
}

AnalysisKey PostDominatorTreeAnalysis::Key;

PostDominatorTree PostDominatorTreeAnalysis::run(Function &F,
                                                 FunctionAnalysisManager &) {
  PostDominatorTree PDT(F);
  return PDT;
}

PostDominatorTreePrinterPass::PostDominatorTreePrinterPass(raw_ostream &OS)
  : OS(OS) {}

PreservedAnalyses
PostDominatorTreePrinterPass::run(Function &F, FunctionAnalysisManager &AM) {
  OS << "PostDominatorTree for function: " << F.getName() << "\n";
  AM.getResult<PostDominatorTreeAnalysis>(F).print(OS);

  return PreservedAnalyses::all();
}
