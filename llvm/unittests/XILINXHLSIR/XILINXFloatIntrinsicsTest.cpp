// (C) Copyright 2016-2023 Xilinx, Inc.
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
//===----------------------------------------------------------------------===//
//
// Test Abitrary Precision Floating Point intrinsics and associated APIs.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/XILINXFPGAIntrinsicInst.h"
#include "llvm/IR/XILINXHLSIRBuilder.h"
#include "gtest/gtest.h"

using namespace llvm;

/// \brief Basic routine for launching HLSFloatTest.
class HLSFloatTest : public testing::Test {
protected:
  void SetUp() override {
    M.reset(new Module("MyModule", Ctx));
    auto EltTy = IntegerType::get(Ctx, 32);
    FunctionType *FTy = FunctionType::get(Type::getVoidTy(Ctx), {EltTy},
                                          /*isVarArg=*/false);
    F = Function::Create(FTy, Function::ExternalLinkage, "", M.get());
    BB = BasicBlock::Create(Ctx, "", F);
  }

  void TearDown() override {
    BB = nullptr;
    M.reset();
  }

  LLVMContext Ctx;
  std::unique_ptr<Module> M;
  Function *F;
  BasicBlock *BB;
};

TEST_F(HLSFloatTest, HLSFloatIntrinsic) {
  HLSIRBuilder IB(BB);

  auto In = F->getArg(0);
  auto Exp = IB.getInt32(8);

  // Check if we succeed to create float compute intrinsics.
  Value *Sqrt = IB.CreateFloatSqrt(In, Exp, "sqrt");
  Value *Mul = IB.CreateFloatMul(Sqrt, Sqrt, Exp, "mul");
  Value *Add = IB.CreateFloatAdd(In, Mul, Exp, "add");
  Value *Sub = IB.CreateFloatSub(In, Mul, Exp, "sub");
  Value *Div = IB.CreateFloatDiv(Add, Sub, Exp, "div");
  Value *FMA = IB.CreateFloatFMA(Div, Sub, Mul, Exp, "fma");

  EXPECT_TRUE(isa<FloatInst>(Sqrt) && isa<FloatComputeInst>(Sqrt) &&
              isa<FloatUnaryInst>(Sqrt) && isa<FloatSqrtInst>(Sqrt));
  EXPECT_TRUE(isa<FloatInst>(Mul) && isa<FloatComputeInst>(Mul) &&
              isa<FloatBinaryInst>(Mul) && isa<FloatMulInst>(Mul));
  EXPECT_TRUE(isa<FloatInst>(Add) && isa<FloatComputeInst>(Add) &&
              isa<FloatBinaryInst>(Add) && isa<FloatAddInst>(Add));
  EXPECT_TRUE(isa<FloatInst>(Sub) && isa<FloatComputeInst>(Sub) &&
              isa<FloatBinaryInst>(Sub) && isa<FloatSubInst>(Sub));
  EXPECT_TRUE(isa<FloatInst>(Div) && isa<FloatComputeInst>(Div) &&
              isa<FloatBinaryInst>(Div) && isa<FloatDivInst>(Div));
  EXPECT_TRUE(isa<FloatInst>(FMA) && isa<FloatComputeInst>(FMA) &&
              isa<FloatTernaryInst>(FMA) && isa<FloatFMAInst>(FMA));

  auto *MulI = cast<FloatComputeInst>(Mul);
  auto *DivI = cast<FloatComputeInst>(Div);
  EXPECT_TRUE(MulI->isCommutative());
  EXPECT_FALSE(DivI->isCommutative());

  EXPECT_EQ(MulI->getExpBitwidth(), 8u);
  EXPECT_EQ(DivI->getMantBitwidth(), 24u);

  // Check if we succeed to create float conversion intrinsics.
  auto Ty = In->getType();
  Type *NewTy = IB.getIntNTy(16);
  ConstantInt *NewExp = IB.getInt32(5);
  Value *Flt2Fix = IB.CreateFloatToFixed(NewTy, In, Exp, NewExp);
  Value *Fix2Flt = IB.CreateFloatFromFixed(Ty, Flt2Fix, NewExp, Exp);
  Value *Flt2Flt = IB.CreateFloatToFloat(NewTy, In, Exp, NewExp);

  EXPECT_TRUE(isa<FloatCastInst>(Flt2Flt) && isa<FloatToFloatInst>(Flt2Flt));
  EXPECT_TRUE(isa<FloatCastInst>(Flt2Fix) && isa<FloatToFixedInst>(Flt2Fix));
  EXPECT_TRUE(isa<FloatCastInst>(Fix2Flt) && isa<FloatFromFixedInst>(Fix2Flt));

  EXPECT_EQ(Flt2Fix->getType(), NewTy);
  EXPECT_EQ(Fix2Flt->getType(), Ty);
  EXPECT_EQ(Flt2Flt->getType(), NewTy);

  auto *Flt2FixI = cast<FloatCastInst>(Flt2Fix);
  auto *Flt2FltI = cast<FloatCastInst>(Flt2Flt);
  EXPECT_FALSE(Flt2FixI->isSrcFixed());
  EXPECT_TRUE(Flt2FixI->isDestFixed());
  EXPECT_EQ(Flt2FltI->getSrcExpBitwidth(), 8u);
  EXPECT_EQ(Flt2FltI->getDestExpBitwidth(), 5u);

  // Check if we succeed to create float comparison intrinsics.
  Value *EQ = IB.CreateFloatCmpEQ(Add, Sub, Exp);
  Value *LE = IB.CreateFloatCmpLE(Add, Sub, Exp);
  Value *LT = IB.CreateFloatCmpLT(Add, Sub, Exp);
  Value *NE = IB.CreateFloatCmpNE(Add, Sub, Exp);
  Value *UO = IB.CreateFloatCmpUO(Add, Sub, Exp);

  EXPECT_TRUE(isa<FloatCmpInst>(EQ) && isa<FloatCmpEQInst>(EQ));
  EXPECT_TRUE(isa<FloatCmpInst>(LE) && isa<FloatCmpLEInst>(LE));
  EXPECT_TRUE(isa<FloatCmpInst>(LT) && isa<FloatCmpLTInst>(LT));
  EXPECT_TRUE(isa<FloatCmpInst>(NE) && isa<FloatCmpNEInst>(NE));
  EXPECT_TRUE(isa<FloatCmpInst>(UO) && isa<FloatCmpUOInst>(UO));

  auto *EQInst = cast<FloatCmpInst>(EQ);
  auto *LTInst = cast<FloatCmpInst>(LT);
  EXPECT_TRUE(EQInst->isCommutative());
  EXPECT_FALSE(LTInst->isCommutative());

  EXPECT_EQ(EQInst->getInputType(), Ty);
  EXPECT_EQ(EQInst->getExpBitwidth(), 8u);
  EXPECT_EQ(LTInst->getMantBitwidth(), 24u);
}

