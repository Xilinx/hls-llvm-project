//===- BasicBlockUtils.cpp - Unit tests for BasicBlockUtils ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// (C) Copyright 2016-2023 Xilinx, Inc.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/SourceMgr.h"
#include "gtest/gtest.h"

using namespace llvm;

static std::unique_ptr<Module> parseIR(LLVMContext &C, const char *IR) {
  SMDiagnostic Err;
  std::unique_ptr<Module> Mod = parseAssemblyString(IR, Err, C);
  if (!Mod)
    Err.print("BasicBlockUtilsTests", errs());
  return Mod;
}

static void
run(Module &M, StringRef FuncName,
    function_ref<void(Function &F, DominatorTree &DT, PostDominatorTree &PDT)>
        Test) {
  auto *F = M.getFunction(FuncName);
  DominatorTree DT(*F);
  PostDominatorTree PDT(*F);
  Test(*F, DT, PDT);
}

static BasicBlock *getBasicBlockByName(Function &F, StringRef Name) {
  for (BasicBlock &BB : F)
    if (BB.getName() == Name)
      return &BB;
  llvm_unreachable("Expected to find basic block!");
}

static Instruction *getInstructionByName(Function &F, StringRef Name) {
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (I.getName() == Name)
        return &I;
  llvm_unreachable("Expected to find instruction!");
}

TEST(BasicBlockUtils, SplitBlockUpdateDTAndPDTSingleBBTest) {
  LLVMContext C;

  std::unique_ptr<Module> M = parseIR(C, R"(define void @foo(i64 %N) {
                 entry:
                   %add = add i64 100, %N
                   %add2 = add i64 %add, %N
                   ret void
                 })");

  run(*M, "foo", [&](Function &F, DominatorTree &DT, PostDominatorTree &PDT) {
    Instruction *AddSndInst = getInstructionByName(F, "add2");
    BasicBlock *BB = getBasicBlockByName(F, "entry");

    auto SplitBB =
        SplitBlock(BB, AddSndInst, &DT, /*LoopInfo *=*/nullptr, &PDT);

    // Verify dominator tree.
    EXPECT_TRUE(DT.dominates(BB, SplitBB));

    // Verify post dominator tree.
    EXPECT_TRUE(PDT.dominates(SplitBB, BB));
  });
}

TEST(BasicBlockUtils, SplitBlockUpdateDTAndPDTControlFlowTest) {
  LLVMContext C;

  std::unique_ptr<Module> M = parseIR(C, R"(define void @foo(i64 %N) {
                 entry:
                   br label %for.body
                 for.body:                          ; preds = %for.inc, %entry
                   %i.02 = phi i64 [ 0, %entry ], [ %inc, %for.inc ]
                   %add = add i64 100, %N
                   %add2 = add i64 %add, %N
                   %and = and i64 %i.02, 3
                   %cmp1 = icmp eq i64 %and, 0
                   br i1 %cmp1, label %if.then, label %for.inc
                 if.then:                           ; preds = %for.body
                   br label %for.inc
                 for.inc:                           ; preds = %if.then, %for.body
                   %inc = add nuw nsw i64 %i.02, 1
                   %exitcond = icmp ne i64 %inc, 16
                   br i1 %exitcond, label %for.body, label %for.end
                 for.end:                           ; preds = %for.inc
                   ret void
                 })");

  run(*M, "foo", [&](Function &F, DominatorTree &DT, PostDominatorTree &PDT) {
    Instruction *AddSndInst = getInstructionByName(F, "add2");
    BasicBlock *BB = getBasicBlockByName(F, "for.body");

    auto SplitBB =
        SplitBlock(BB, AddSndInst, &DT, /*LoopInfo *=*/nullptr, &PDT);

    // Verify dominator tree.
    EXPECT_TRUE(DT.dominates(BB, SplitBB));
    BasicBlock *ThenBB = getBasicBlockByName(F, "if.then");
    BasicBlock *IncBB = getBasicBlockByName(F, "for.inc");
    BasicBlock *EndBB = getBasicBlockByName(F, "for.end");
    EXPECT_TRUE(DT.dominates(SplitBB, ThenBB));
    EXPECT_TRUE(DT.dominates(SplitBB, IncBB));
    EXPECT_TRUE(DT.dominates(SplitBB, EndBB));

    // Verify post dominator tree.
    EXPECT_TRUE(PDT.dominates(EndBB, SplitBB));
    EXPECT_TRUE(PDT.dominates(IncBB, SplitBB));
    EXPECT_TRUE(PDT.dominates(SplitBB, BB));
    BasicBlock *EntryBB = getBasicBlockByName(F, "entry");
    EXPECT_TRUE(PDT.dominates(SplitBB, EntryBB));
  });
}

TEST(BasicBlockUtils, SplitBlockUpdatePDTMultipleExitsTest) {
  LLVMContext C;

  std::unique_ptr<Module> M = parseIR(C, R"(define void @foo(i64 %N) {
                 entry:
                   %add = add i64 100, %N
                   %add2 = add i64 %add, %N
                   %cmp1 = icmp eq i64 %N, 0
                   br i1 %cmp1, label %if.then, label %if.end
                 if.then:
                   br label %next
                 next:
                   ret void
                 if.end:
                   ret void
                 })");

  run(*M, "foo", [&](Function &F, DominatorTree &DT, PostDominatorTree &PDT) {
    Instruction *AddSndInst = getInstructionByName(F, "add2");
    BasicBlock *BB = getBasicBlockByName(F, "entry");

    auto BBIDom = PDT.getNode(BB)->getIDom();
    auto SplitBB =
        SplitBlock(BB, AddSndInst, &DT, /*LoopInfo *=*/nullptr, &PDT);

    // Verify post dominator tree.
    EXPECT_EQ(PDT.getNode(SplitBB)->getIDom(), BBIDom);
    EXPECT_TRUE(PDT.dominates(SplitBB, BB));
  });
}
