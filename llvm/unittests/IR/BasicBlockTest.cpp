//===- llvm/unittest/IR/BasicBlockTest.cpp - BasicBlock unit tests --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/Support/SourceMgr.h"
#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"
#include <memory>

namespace llvm {
namespace {

TEST(BasicBlockTest, PhiRange) {
  LLVMContext Context;

  // Create the main block.
  std::unique_ptr<BasicBlock> BB(BasicBlock::Create(Context));

  // Create some predecessors of it.
  std::unique_ptr<BasicBlock> BB1(BasicBlock::Create(Context));
  BranchInst::Create(BB.get(), BB1.get());
  std::unique_ptr<BasicBlock> BB2(BasicBlock::Create(Context));
  BranchInst::Create(BB.get(), BB2.get());

  // Make sure this doesn't crash if there are no phis.
  for (auto &PN : BB->phis()) {
    (void)PN;
    EXPECT_TRUE(false) << "empty block should have no phis";
  }

  // Make it a cycle.
  auto *BI = BranchInst::Create(BB.get(), BB.get());

  // Now insert some PHI nodes.
  auto *Int32Ty = Type::getInt32Ty(Context);
  auto *P1 = PHINode::Create(Int32Ty, /*NumReservedValues*/ 3, "phi.1", BI);
  auto *P2 = PHINode::Create(Int32Ty, /*NumReservedValues*/ 3, "phi.2", BI);
  auto *P3 = PHINode::Create(Int32Ty, /*NumReservedValues*/ 3, "phi.3", BI);

  // Some non-PHI nodes.
  auto *Sum = BinaryOperator::CreateAdd(P1, P2, "sum", BI);

  // Now wire up the incoming values that are interesting.
  P1->addIncoming(P2, BB.get());
  P2->addIncoming(P1, BB.get());
  P3->addIncoming(Sum, BB.get());

  // Finally, let's iterate them, which is the thing we're trying to test.
  // We'll use this to wire up the rest of the incoming values.
  for (auto &PN : BB->phis()) {
    PN.addIncoming(UndefValue::get(Int32Ty), BB1.get());
    PN.addIncoming(UndefValue::get(Int32Ty), BB2.get());
  }

  // Test that we can use const iterators and generally that the iterators
  // behave like iterators.
  BasicBlock::const_phi_iterator CI;
  CI = BB->phis().begin();
  EXPECT_NE(CI, BB->phis().end());

  // And iterate a const range.
  for (const auto &PN : const_cast<const BasicBlock *>(BB.get())->phis()) {
    EXPECT_EQ(BB.get(), PN.getIncomingBlock(0));
    EXPECT_EQ(BB1.get(), PN.getIncomingBlock(1));
    EXPECT_EQ(BB2.get(), PN.getIncomingBlock(2));
  }
}

TEST(BasicBlockTest, ComesBefore) {
  const char *ModuleString = R"(define i32 @f(i32 %x) {
                                  %add = add i32 %x, 42
                                  ret i32 %add
                                })";
  LLVMContext Ctx;
  SMDiagnostic Err;
  auto M = parseAssemblyString(ModuleString, Err, Ctx);
  ASSERT_TRUE(M.get());

  Function *F = M->getFunction("f");
  BasicBlock &BB = F->front();
  BasicBlock::iterator I = BB.begin();
  Instruction *Add = &*I++;
  Instruction *Ret = &*I++;

  // Intentionally duplicated to verify cached and uncached are the same.
  EXPECT_FALSE(BB.isInstrOrderValid());
  EXPECT_FALSE(Add->comesBefore(Add));
  EXPECT_TRUE(BB.isInstrOrderValid());
  EXPECT_FALSE(Add->comesBefore(Add));
  BB.invalidateOrders();
  EXPECT_FALSE(BB.isInstrOrderValid());
  EXPECT_TRUE(Add->comesBefore(Ret));
  EXPECT_TRUE(BB.isInstrOrderValid());
  EXPECT_TRUE(Add->comesBefore(Ret));
  BB.invalidateOrders();
  EXPECT_FALSE(Ret->comesBefore(Add));
  EXPECT_FALSE(Ret->comesBefore(Add));
  BB.invalidateOrders();
  EXPECT_FALSE(Ret->comesBefore(Ret));
  EXPECT_FALSE(Ret->comesBefore(Ret));
}

class InstrOrderInvalidationTest : public ::testing::Test {
protected:
  void SetUp() override {
    M.reset(new Module("MyModule", Ctx));
    Nop = Intrinsic::getDeclaration(M.get(), Intrinsic::donothing);
    FunctionType *FT = FunctionType::get(Type::getVoidTy(Ctx), {}, false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, "foo", M.get());
    BB = BasicBlock::Create(Ctx, "entry", F);

    IRBuilder<> Builder(BB);
    I1 = Builder.CreateCall(Nop);
    I2 = Builder.CreateCall(Nop);
    I3 = Builder.CreateCall(Nop);
    Ret = Builder.CreateRetVoid();
  }

  LLVMContext Ctx;
  std::unique_ptr<Module> M;
  Function *Nop = nullptr;
  BasicBlock *BB = nullptr;
  Instruction *I1 = nullptr;
  Instruction *I2 = nullptr;
  Instruction *I3 = nullptr;
  Instruction *Ret = nullptr;
};

TEST_F(InstrOrderInvalidationTest, InsertInvalidation) {
  EXPECT_FALSE(BB->isInstrOrderValid());
  EXPECT_TRUE(I1->comesBefore(I2));
  EXPECT_TRUE(BB->isInstrOrderValid());
  EXPECT_TRUE(I2->comesBefore(I3));
  EXPECT_TRUE(I3->comesBefore(Ret));
  EXPECT_TRUE(BB->isInstrOrderValid());

  // Invalidate orders.
  IRBuilder<> Builder(BB, I2->getIterator());
  Instruction *I1a = Builder.CreateCall(Nop);
  EXPECT_FALSE(BB->isInstrOrderValid());
  EXPECT_TRUE(I1->comesBefore(I1a));
  EXPECT_TRUE(BB->isInstrOrderValid());
  EXPECT_TRUE(I1a->comesBefore(I2));
  EXPECT_TRUE(I2->comesBefore(I3));
  EXPECT_TRUE(I3->comesBefore(Ret));
  EXPECT_TRUE(BB->isInstrOrderValid());
}

TEST_F(InstrOrderInvalidationTest, SpliceInvalidation) {
  EXPECT_TRUE(I1->comesBefore(I2));
  EXPECT_TRUE(I2->comesBefore(I3));
  EXPECT_TRUE(I3->comesBefore(Ret));
  EXPECT_TRUE(BB->isInstrOrderValid());

  // Use Instruction::moveBefore, which uses splice.
  I2->moveBefore(I1);
  EXPECT_FALSE(BB->isInstrOrderValid());

  EXPECT_TRUE(I2->comesBefore(I1));
  EXPECT_TRUE(I1->comesBefore(I3));
  EXPECT_TRUE(I3->comesBefore(Ret));
  EXPECT_TRUE(BB->isInstrOrderValid());
}

TEST_F(InstrOrderInvalidationTest, RemoveNoInvalidation) {
  // Cache the instruction order.
  EXPECT_FALSE(BB->isInstrOrderValid());
  EXPECT_TRUE(I1->comesBefore(I2));
  EXPECT_TRUE(BB->isInstrOrderValid());

  // Removing does not invalidate instruction order.
  I2->removeFromParent();
  I2->deleteValue();
  I2 = nullptr;
  EXPECT_TRUE(BB->isInstrOrderValid());
  EXPECT_TRUE(I1->comesBefore(I3));
  EXPECT_EQ(std::next(I1->getIterator()), I3->getIterator());
}

TEST_F(InstrOrderInvalidationTest, EraseNoInvalidation) {
  // Cache the instruction order.
  EXPECT_FALSE(BB->isInstrOrderValid());
  EXPECT_TRUE(I1->comesBefore(I2));
  EXPECT_TRUE(BB->isInstrOrderValid());

  // Removing does not invalidate instruction order.
  I2->eraseFromParent();
  I2 = nullptr;
  EXPECT_TRUE(BB->isInstrOrderValid());
  EXPECT_TRUE(I1->comesBefore(I3));
  EXPECT_EQ(std::next(I1->getIterator()), I3->getIterator());
}

} // End anonymous namespace.
} // End llvm namespace.
