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
// Test supported HLS direct input/output APIs.
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

/// \brief Basic routine for launching HLSDirectIOTest.
class HLSDirectIOTest : public testing::Test {
protected:
  void SetUp() override {
    M.reset(new Module("MyModule", Ctx));
    auto EltTy = IntegerType::get(Ctx, 32);
    auto PtrTy = PointerType::getUnqual(EltTy);
    FunctionType *FTy = FunctionType::get(Type::getVoidTy(Ctx), {PtrTy, PtrTy},
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

TEST_F(HLSDirectIOTest, HLSIntrinsic) {
  HLSIRBuilder IB(BB);

  auto In = F->getArg(0);
  auto Out = F->getArg(1);

  // Check if succeed to create fpga direct input/output status intrinsics.
  Value *VLd = IB.CreateDirectIOValid(In, "ap_none");
  Value *RSt = IB.CreateDirectIOReady(Out, "ap_hs");
  EXPECT_TRUE(isa<FPGADirectIOInst>(VLd) &&
              isa<FPGADirectIOInst>(RSt));
  EXPECT_TRUE(isa<FPGADirectIOStatusInst>(VLd) &&
              isa<FPGADirectIOStatusInst>(RSt));
  EXPECT_TRUE(isa<FPGADirectIOValidInst>(VLd) &&
              isa<FPGADirectIOReadyInst>(RSt));

  // Check if get correct information from the fpga direct input/output status
  // intrinsics.
  auto SLd = cast<FPGADirectIOStatusInst>(VLd);
  auto SSt = cast<FPGADirectIOStatusInst>(RSt);
  EXPECT_EQ(SLd->getDirectIOOperand(), In);
  EXPECT_EQ(SSt->getDirectIOOperand(), Out);
  EXPECT_EQ(SLd->getDirectIOType(), In->getType());
  EXPECT_EQ(SSt->getDirectIOType(), Out->getType());
  EXPECT_EQ(SLd->getDataType(),
            cast<PointerType>(In->getType())->getElementType());
  EXPECT_EQ(SSt->getDataType(),
            cast<PointerType>(Out->getType())->getElementType());
  EXPECT_EQ(SLd->getHandShake(), "ap_none");
  EXPECT_EQ(SSt->getHandShake(), "ap_hs");

  // Check if succeed to create fpga direct input/output access intrinsics.
  Value *Ld = IB.CreateFPGADirectLoadInst(In, ""/*HandShake*/, 4);
  Value *St = IB.CreateFPGADirectStoreInst(Ld, Out, "ap_ack");
  EXPECT_TRUE(isa<FPGADirectIOInst>(Ld) &&
              isa<FPGADirectIOInst>(St));
  EXPECT_TRUE(isa<FPGADirectLoadStoreInst>(Ld) &&
              isa<FPGADirectLoadStoreInst>(St));
  EXPECT_TRUE(isa<FPGADirectLoadInst>(Ld) &&
              isa<FPGADirectStoreInst>(St));

  // Check if get correct information from the fpga direct input/output access
  // intrinsics.
  auto DirectLd = cast<FPGADirectLoadInst>(Ld);
  auto DirectSt = cast<FPGADirectStoreInst>(St);
  EXPECT_EQ(DirectLd->getDirectIOOperand(), In);
  EXPECT_EQ(DirectLd->getAlignment(), 4);
  EXPECT_EQ(DirectSt->getDirectIOOperand(), Out);
  EXPECT_EQ(DirectSt->getDataOperand(), Ld);
  EXPECT_EQ(DirectSt->getAlignment(), 0);
  EXPECT_FALSE(DirectLd->willReturn());
  EXPECT_FALSE(DirectSt->willReturn());
  EXPECT_EQ(DirectLd->getHandShake(), "");
  EXPECT_EQ(DirectSt->getHandShake(), "ap_ack");
}

