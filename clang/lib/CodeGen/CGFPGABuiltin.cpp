// (c) Copyright 2016-2022 Xilinx, Inc.
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
// Generates code for built-in FPGA calls.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/XILINXFPGAIntrinsicInst.h"
#include "llvm/Support/MathExtras.h"
#include <cassert>

using namespace clang;
using namespace CodeGen;
using namespace llvm;

Value *CodeGenFunction::EmitFPGABuiltinExpr(unsigned BuiltinID,
                                            const CallExpr *E) {
  switch (BuiltinID) {
  case FPGA::BI__fpga_fifo_not_empty:
  case FPGA::BI__fpga_fifo_not_full:
    return EmitBuiltinFPGAFifoStatus(BuiltinID, E);
  case FPGA::BI__fpga_fifo_size:
  case FPGA::BI__fpga_fifo_capacity:
    return EmitBuiltinFPGAFifoLength(BuiltinID, E);
  case FPGA::BI__fpga_fifo_pop:
  case FPGA::BI__fpga_fifo_push:
    return EmitBuiltinFPGAFifoBlocking(BuiltinID, E);
  case FPGA::BI__fpga_fifo_nb_pop:
  case FPGA::BI__fpga_fifo_nb_push:
    return EmitBuiltinFPGAFifoNonBlocking(BuiltinID, E);
  case FPGA::BI__fpga_pipo_not_empty:
  case FPGA::BI__fpga_pipo_not_full:
    return EmitBuiltinFPGAPipoStatus(BuiltinID, E);
  case FPGA::BI__fpga_pipo_pop_acquire:
  case FPGA::BI__fpga_pipo_pop_release:
  case FPGA::BI__fpga_pipo_push_acquire:
  case FPGA::BI__fpga_pipo_push_release:
    return EmitBuiltinFPGAPipoBlocking(BuiltinID, E);
  case FPGA::BI__fpga_set_stream_depth:
    return EmitBuiltinFPGASetStreamDepth(BuiltinID, E);
  case FPGA::BI__fpga_set_stream_of_blocks_depth:
    return EmitBuiltinFPGASetStreamOfBlocksDepth(BuiltinID, E);
  case FPGA::BI__fpga_maxi_read_req:
  case FPGA::BI__fpga_maxi_read:
  case FPGA::BI__fpga_maxi_write_req:
  case FPGA::BI__fpga_maxi_write:
  case FPGA::BI__fpga_maxi_write_resp:
    return EmitBuiltinFPGAMAXIBurst(BuiltinID, E);
  case FPGA::BI__fpga_add_task:
    return EmitBuiltinFPGAAddTask(BuiltinID, E);
  case FPGA::BI__fpga_add_infinite_task:
    return EmitBuiltinFPGAAddInfiniteTask(BuiltinID, E);
  case FPGA::BI__fpga_nport_channel:
    return EmitBuiltinFPGANPortChannel(E);
  case FPGA::BI__fpga_ip:
    return EmitBuiltinFPGAIP(E);
  case FPGA::BI__fpga_fence:
    return EmitBuiltinFPGAFence(E);
  }
  return nullptr;
}


Value *CodeGenFunction::EmitBuiltinFPGAAddTask(unsigned BuiltinID, const CallExpr* E) 
{
  assert(BuiltinID == FPGA::BI__fpga_add_task && "unexpected");
  const Expr *region_id = E->getArg(0);
  const Expr *task_id = E->getArg(1);
  Value * region = EmitScalarExpr( region_id );
  Value * task = EmitScalarExpr( task_id );

  auto *F = Intrinsic::getDeclaration(
      &CGM.getModule(), Intrinsic::fpga_add_task, { Builder.getInt32Ty()->getPointerTo(), Builder.getInt32Ty()->getPointerTo()});

  return Builder.CreateCall(F, {region, task});
}

Value *CodeGenFunction::EmitBuiltinFPGAAddInfiniteTask(unsigned BuiltinID, const CallExpr* E) 
{
  assert(BuiltinID == FPGA::BI__fpga_add_infinite_task && "unexpected");
  const Expr *region_id = E->getArg(0);
  const Expr *task_id = E->getArg(1);
  Value * region = EmitScalarExpr( region_id );
  Value * task = EmitScalarExpr( task_id );

  auto *F = Intrinsic::getDeclaration(
      &CGM.getModule(), Intrinsic::fpga_add_infinite_task, {Builder.getInt32Ty()->getPointerTo(), Builder.getInt32Ty()->getPointerTo()});

  return Builder.CreateCall(F, {region, task});
}

Value *CodeGenFunction::EmitBuiltinFPGAMAXIBurst(unsigned BuiltinID,
                                                 const CallExpr *E) {
  Intrinsic::ID ID = Intrinsic::not_intrinsic;
  switch (BuiltinID) {
  case FPGA::BI__fpga_maxi_read_req:
    ID = Intrinsic::fpga_maxi_read_req;
    break;
  case FPGA::BI__fpga_maxi_read:
    ID = Intrinsic::fpga_maxi_read;
    break;
  case FPGA::BI__fpga_maxi_write_req:
    ID = Intrinsic::fpga_maxi_write_req;
    break;
  case FPGA::BI__fpga_maxi_write:
    ID = Intrinsic::fpga_maxi_write;
    break;
  case FPGA::BI__fpga_maxi_write_resp:
    ID = Intrinsic::fpga_maxi_write_resp;
    break;
  default:
    llvm_unreachable("Bad Builtin");
  }

  auto *PtrAst = E->getArg(0);
  auto *Ptr = EmitScalarExpr(PtrAst);

  if (ID == Intrinsic::fpga_maxi_read_req ||
      ID == Intrinsic::fpga_maxi_write_req) {
    auto *LengthAst = E->getArg(1);
    auto *Length = EmitScalarExpr(LengthAst);
    auto *F = Intrinsic::getDeclaration(&CGM.getModule(), ID, Ptr->getType());
    return Builder.CreateCall(F, {Ptr, Length});
  } else if (ID == Intrinsic::fpga_maxi_read) {
    auto *ValPtrAst = E->getArg(1);
    auto ValPtr = EmitPointerWithAlignment(ValPtrAst);
    auto *F = Intrinsic::getDeclaration(
        &CGM.getModule(), ID, {ValPtr.getElementType(), Ptr->getType()});
    auto *Val = Builder.CreateCall(F, Ptr);
    return Builder.CreateStore(Val, ValPtr);
  } else if (ID == Intrinsic::fpga_maxi_write) {
    auto *ValPtrAst = E->getArg(1);
    auto ValPtr = EmitPointerWithAlignment(ValPtrAst);
    auto *Val = Builder.CreateLoad(ValPtr);
    uint64_t Size = CGM.getDataLayout().getTypeAllocSize(Val->getType());
    // This is byte enable bits
    auto BEPtr = EmitPointerWithAlignment(E->getArg(2));
    auto *BETy = BEPtr.getElementType();
    std::vector<Value *> IdxList;
    IdxList.push_back(Builder.getInt64(0));
    while (StructType *ST = dyn_cast<StructType>(BETy)) {
      if (ST->getNumElements() != 1)
        break;
      IdxList.push_back(Builder.getInt32(0));
      BETy =  ST->getElementType(0);
    }

    auto *BE = Builder.CreateAlignedLoad(
                   Builder.CreateGEP(BEPtr.getPointer(), IdxList),
                   BEPtr.getAlignment().getQuantity());

    auto *F = Intrinsic::getDeclaration(
        &CGM.getModule(), ID,
        {ValPtr.getElementType(), Ptr->getType(), BE->getType()});
    return Builder.CreateCall(F, {Val, Ptr, BE});
  } else {
    assert(ID == Intrinsic::fpga_maxi_write_resp && "Bad Intrinsic ID");
    auto *F = Intrinsic::getDeclaration(&CGM.getModule(), ID, Ptr->getType());
    return Builder.CreateCall(F, Ptr);
  }
}

Value *CodeGenFunction::EmitBuiltinFPGASetStreamDepth(unsigned BuiltinID,
                                                      const CallExpr *E) {

  auto *streamIdAst = E->getArg(0);
  auto *streamId = EmitScalarExpr(streamIdAst);
  auto *depthAst = E->getArg(1);
  auto *depth = EmitScalarExpr(depthAst);
  auto *F = Intrinsic::getDeclaration(&CGM.getModule(),
                                      Intrinsic::fpga_set_stream_depth);

  return Builder.CreateCall(F, {streamId, depth});
}

Value *CodeGenFunction::EmitBuiltinFPGASetStreamOfBlocksDepth(unsigned BuiltinID,
                                                              const CallExpr *E) {

  auto *streamIdAst = E->getArg(0);
  auto *streamId = EmitScalarExpr(streamIdAst);
  auto *depthAst = E->getArg(1);
  auto *depth = EmitScalarExpr(depthAst);
  auto *valType = streamId->getType()->getPointerElementType();
  uint64_t bitSize = CGM.getDataLayout().getTypeAllocSizeInBits(valType);

  auto *F = llvm::Intrinsic::getDeclaration(CurFn->getParent(),
                                            llvm::Intrinsic::sideeffect);
  llvm::Value *args[] = {streamId, depth};
  SmallVector<llvm::OperandBundleDef, 2> bundleDefs;
  bundleDefs.emplace_back(StreamOfBlocksPragmaInst::BundleTagName, args);
  auto* call = Builder.CreateCall(F, None, bundleDefs);

  std::pair<unsigned, llvm::Attribute> attrs = {
    llvm::AttributeList::FunctionIndex,
    llvm::Attribute::get(getLLVMContext(), "xlx.port.bitwidth", std::to_string(bitSize))};
  llvm::AttributeList attr_list = llvm::AttributeList::get(getLLVMContext(),
                                                           attrs);
  call->setAttributes(attr_list);
  call->setOnlyAccessesInaccessibleMemory();
  call->setDoesNotThrow();

  return call;

  return PragmaInst::Create<StreamOfBlocksPragmaInst>({streamId, depth},
                                                      &*Builder.GetInsertPoint(),
                                                      nullptr, bitSize);
}

static Intrinsic::ID getFIFOIntrID(unsigned BI) {
  switch (BI) {
  case FPGA::BI__fpga_fifo_not_empty:
    return Intrinsic::fpga_fifo_not_empty;
  case FPGA::BI__fpga_fifo_not_full:
    return Intrinsic::fpga_fifo_not_full;
  case FPGA::BI__fpga_fifo_size:
    return Intrinsic::fpga_fifo_size;
  case FPGA::BI__fpga_fifo_capacity:
    return Intrinsic::fpga_fifo_capacity;
  case FPGA::BI__fpga_fifo_pop:
    return Intrinsic::fpga_fifo_pop;
  case FPGA::BI__fpga_fifo_push:
    return Intrinsic::fpga_fifo_push;
  case FPGA::BI__fpga_fifo_nb_pop:
    return Intrinsic::fpga_fifo_nb_pop;
  case FPGA::BI__fpga_fifo_nb_push:
    return Intrinsic::fpga_fifo_nb_push;
  default:
    return Intrinsic::not_intrinsic;
  }
}

Value *CodeGenFunction::EmitBuiltinFPGAFifoStatus(unsigned BuiltinID,
                                                  const CallExpr *E) {
  assert((BuiltinID == FPGA::BI__fpga_fifo_not_empty ||
          BuiltinID == FPGA::BI__fpga_fifo_not_full) &&
         "Not a FIFO status intrinsics?");

  auto *FIFOAst = E->getArg(0);
  auto *FIFO = EmitScalarExpr(FIFOAst);

  auto *F = Intrinsic::getDeclaration(
      &CGM.getModule(), getFIFOIntrID(BuiltinID), {FIFO->getType()});

  return Builder.CreateCall(F, {FIFO});
}

Value *CodeGenFunction::EmitBuiltinFPGAFifoLength(unsigned BuiltinID,
                                                const CallExpr *E) {
  assert((BuiltinID == FPGA::BI__fpga_fifo_size ||
          BuiltinID == FPGA::BI__fpga_fifo_capacity) &&
         "Not a FIFO size intrinsics?");

  auto *FIFOAst = E->getArg(0);
  auto *FIFO = EmitScalarExpr(FIFOAst);

  auto *F = Intrinsic::getDeclaration(
      &CGM.getModule(), getFIFOIntrID(BuiltinID), {FIFO->getType()});

  return Builder.CreateCall(F, {FIFO});
}

Value *CodeGenFunction::EmitBuiltinFPGAFifoBlocking(unsigned BuiltinID,
                                                    const CallExpr *E) {
  assert((BuiltinID == FPGA::BI__fpga_fifo_pop ||
          BuiltinID == FPGA::BI__fpga_fifo_push) &&
         "Not a FIFO blocking intrinsics?");

  auto *FIFOAst = E->getArg(0);
  auto *FIFO = EmitScalarExpr(FIFOAst);

  auto *ValAst = E->getArg(1);
  auto ValAddr = EmitPointerWithAlignment(ValAst);

  // Those intrinsics are parametrized by the same types!
  auto *F =
      Intrinsic::getDeclaration(&CGM.getModule(), getFIFOIntrID(BuiltinID),
                                {ValAddr.getElementType(), FIFO->getType()});

  switch (BuiltinID) {
  case FPGA::BI__fpga_fifo_pop: {
    auto *Val = Builder.CreateCall(F, {FIFO});
    return Builder.CreateStore(Val, ValAddr);
  }
  case FPGA::BI__fpga_fifo_push: {
    auto *Val = Builder.CreateLoad(ValAddr);
    return Builder.CreateCall(F, {Val, FIFO});
  }
  default:
    llvm_unreachable("Unsupported intrinsics?");
  }
}

Value *CodeGenFunction::EmitBuiltinFPGAFifoNonBlocking(unsigned BuiltinID,
                                                       const CallExpr *E) {
  assert((BuiltinID == FPGA::BI__fpga_fifo_nb_pop ||
          BuiltinID == FPGA::BI__fpga_fifo_nb_push) &&
         "Not a FIFO non-blocking intrinsics?");

  auto *FIFOAst = E->getArg(0);
  auto *FIFO = EmitScalarExpr(FIFOAst);

  auto *ValAst = E->getArg(1);
  auto ValAddr = EmitPointerWithAlignment(ValAst);

  // Those intrinsics are parametrized by the same types!
  auto *F =
      Intrinsic::getDeclaration(&CGM.getModule(), getFIFOIntrID(BuiltinID),
                                {ValAddr.getElementType(), FIFO->getType()});

  switch (BuiltinID) {
  case FPGA::BI__fpga_fifo_nb_pop: {
    auto *DoneVal = Builder.CreateCall(F, {FIFO});
    auto *Done = Builder.CreateExtractValue(DoneVal, {0});
    auto *Val = Builder.CreateExtractValue(DoneVal, {1});
    Builder.CreateStore(Val, ValAddr);
    return Done;
  }
  case FPGA::BI__fpga_fifo_nb_push: {
    auto *Val = Builder.CreateLoad(ValAddr);
    auto *Done = Builder.CreateCall(F, {Val, FIFO});
    return Done;
  }
  default:
    llvm_unreachable("Unsupported intrinsics?");
  }
}

static Intrinsic::ID getPIPOIntrID(unsigned BI) {
  switch (BI) {
  case FPGA::BI__fpga_pipo_not_empty:
    return Intrinsic::fpga_pipo_not_empty;
  case FPGA::BI__fpga_pipo_not_full:
    return Intrinsic::fpga_pipo_not_full;
  case FPGA::BI__fpga_pipo_pop_acquire:
    return Intrinsic::fpga_pipo_pop_acquire;
  case FPGA::BI__fpga_pipo_pop_release:
    return Intrinsic::fpga_pipo_pop_release;
  case FPGA::BI__fpga_pipo_push_acquire:
    return Intrinsic::fpga_pipo_push_acquire;
  case FPGA::BI__fpga_pipo_push_release:
    return Intrinsic::fpga_pipo_push_release;
  default:
    return Intrinsic::not_intrinsic;
  }
}

Value *CodeGenFunction::EmitBuiltinFPGAPipoStatus(unsigned BuiltinID,
                                                  const CallExpr *E) {
  assert((BuiltinID == FPGA::BI__fpga_pipo_not_empty ||
          BuiltinID == FPGA::BI__fpga_pipo_not_full) &&
         "Not a PIPO status intrinsics?");

  auto *PIPOAst = E->getArg(0);
  auto *PIPO = EmitScalarExpr(PIPOAst);

  auto *F = Intrinsic::getDeclaration(
      &CGM.getModule(), getPIPOIntrID(BuiltinID), {PIPO->getType()});

  return Builder.CreateCall(F, {PIPO});
}

Value *CodeGenFunction::EmitBuiltinFPGAPipoBlocking(unsigned BuiltinID,
                                                    const CallExpr *E) {
  assert((BuiltinID == FPGA::BI__fpga_pipo_pop_acquire ||
          BuiltinID == FPGA::BI__fpga_pipo_pop_release ||
          BuiltinID == FPGA::BI__fpga_pipo_push_acquire ||
          BuiltinID == FPGA::BI__fpga_pipo_push_release) &&
         "Not a PIPO blocking intrinsics?");

  auto *PIPOAst = E->getArg(0);
  auto *PIPO = EmitScalarExpr(PIPOAst);

  auto *F = Intrinsic::getDeclaration(
      &CGM.getModule(), getPIPOIntrID(BuiltinID), {PIPO->getType()});

  return Builder.CreateCall(F, {PIPO});
}

RValue CodeGenFunction::EmitBuiltinOCLPipeReadWrite(unsigned BuiltinID,
                                                    Value *Pipe,
                                                    Address ValPtr) {
  auto ID = Intrinsic::not_intrinsic;
  switch (BuiltinID) {
  case Builtin::BIread_pipe:
    ID = Intrinsic::spir_read_pipe_2;
    break;
  case Builtin::BIwrite_pipe:
    ID = Intrinsic::spir_write_pipe_2;
    break;
  case Builtin::BIread_pipe_block: {
    auto *PipeTy = Pipe->getType();
    auto *F = Intrinsic::getDeclaration(
        &CGM.getModule(), Intrinsic::spir_read_pipe_block_2,
        {PipeTy->getPointerElementType(), PipeTy});
    auto *Val = Builder.CreateCall(F, {Pipe});
    Builder.CreateStore(Val, ValPtr);
    return RValue::getIgnored();
  }
  case Builtin::BIwrite_pipe_block: {
    auto *Val = Builder.CreateLoad(ValPtr);
    auto *F = Intrinsic::getDeclaration(&CGM.getModule(),
                                        Intrinsic::spir_write_pipe_block_2,
                                        {Val->getType(), Pipe->getType()});
    Builder.CreateCall(F, {Val, Pipe});
    return RValue::getIgnored();
  }
  default:
    llvm_unreachable("Unexpected builtin!");
    break;
  }
  auto *F = Intrinsic::getDeclaration(
      &CGM.getModule(), ID,
      {Pipe->getType()->getPointerElementType(), ValPtr.getElementType()});
  return RValue::get(Builder.CreateCall(F, {Pipe, ValPtr.getPointer()}));
}

/// Legacy FPGA related builtins
/// Convert the __builtin_bit_concat builtin.
/// See IEEE 1666-2005, System C, Section 7.2.7, pg 176.
RValue CodeGenFunction::EmitBuiltinBitConcat(const CallExpr *E) {
  llvm::Value *RsltPtr = EmitScalarExpr(E->getArg(0));
  llvm::Value *HighPtr = EmitScalarExpr(E->getArg(1));
  llvm::Value *LowPtr = EmitScalarExpr(E->getArg(2));

  if (BitCastInst *BC = dyn_cast<BitCastInst>(RsltPtr))
    RsltPtr = BC->getOperand(0);
  if (BitCastInst *BC = dyn_cast<BitCastInst>(HighPtr))
    HighPtr = BC->getOperand(0);
  if (BitCastInst *BC = dyn_cast<BitCastInst>(LowPtr))
    LowPtr = BC->getOperand(0);

  llvm::IntegerType *RsltTy = 0, *HighTy = 0, *LowTy = 0;

  if (llvm::PointerType *RsltPtrTy =
          dyn_cast<llvm::PointerType>(RsltPtr->getType()))
    RsltTy = dyn_cast<llvm::IntegerType>(RsltPtrTy->getElementType());

  if (llvm::PointerType *HighPtrTy =
          dyn_cast<llvm::PointerType>(HighPtr->getType()))
    HighTy = dyn_cast<llvm::IntegerType>(HighPtrTy->getElementType());

  if (llvm::PointerType *LowPtrTy =
          dyn_cast<llvm::PointerType>(LowPtr->getType()))
    LowTy = dyn_cast<llvm::IntegerType>(LowPtrTy->getElementType());

  if (!HighTy || !LowTy || !RsltTy) {
    CGM.Error(E->getExprLoc(),
              "All three arguments to __builtin_bit_concat must be int* typed");
    return RValue::get(0);
  }

  if (RsltTy->getBitWidth() != HighTy->getBitWidth() + LowTy->getBitWidth()) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "Invalid bit width for __builtin_bit_concat result");
    return RValue::get(0);
  }

  // Get the shift amount for the high bits
  llvm::ConstantInt *Shift =
      llvm::ConstantInt::get(RsltTy, LowTy->getBitWidth());

  // Load the two values being concatenated
  llvm::Value *High = ((CGBuilderBaseTy &)Builder).CreateLoad(HighPtr, "");
  llvm::Value *Low = ((CGBuilderBaseTy &)Builder).CreateLoad(LowPtr, "");

  // Extend both values to the concatenated bit width
  llvm::Value *HighZExt = Builder.CreateZExt(High, RsltTy, "");
  llvm::Value *LowZExt = Builder.CreateZExt(Low, RsltTy, "");

  // Shift the high bits up
  llvm::Value *New = Builder.CreateShl(HighZExt, Shift, "");

  // Or in the low bits to form the result.
  llvm::Value *Concat = Builder.CreateOr(New, LowZExt, "bit_concat");

  ((CGBuilderBaseTy &)Builder).CreateStore(Concat, RsltPtr);

  return RValue::get(RsltPtr);
}

/// given a string, this method returns the specific radix of the integer
/// 0 represents oct, 0x hex 0b binary
/// if the return value is less than 0, indicates the radix can not be
/// processed
static uint32_t getRadix(std::string &s) {
  std::string prefix = "";
  if (s[0] == '-' || s[0] == '+') {
    prefix += s[0];
    s = s.substr(1);
  }
  int len = s.length() - 1;
  if (len == 1) {
    if (s[0] <= '9' && s[0] >= '0') {
      s = prefix + s;
      return 10;
    } else {
      return 0; // indicate there is only one char and it is not dec
    }
  }

  if (s[0] != '0') {
    // no need to check valid char here
    s = prefix + s;
    return 10;
  }

  if (s[1] == 'x' || s[1] == 'X') {
    if (len == 2) {
      assert(0 && "There is nothing following the Radix");
      return 0;
    }
    s = s.substr(2);
    s = prefix + s;
    return 16;
  }

  if (s[1] == 'b' || s[1] == 'B') {
    if (len == 2) {
      assert(0 && "There is nothing following the Radix");
      return 0;
    }
    s = s.substr(2);
    s = prefix + s;
    return 2;
  }

  s = s.substr(1);
  s = prefix + s;
  return 8;
}

RValue CodeGenFunction::EmitBuiltinBitFromString(const CallExpr *E) {
  llvm::Value *RsltPtr = EmitScalarExpr(E->getArg(0));
  llvm::Value *Str = EmitScalarExpr(E->getArg(1));
  llvm::Value *Radix = EmitScalarExpr(E->getArg(2));

  if (BitCastInst *BC = dyn_cast<BitCastInst>(RsltPtr))
    RsltPtr = BC->getOperand(0);

  llvm::IntegerType *RsltTy = 0;
  if (llvm::PointerType *RsltPtrTy =
          dyn_cast<llvm::PointerType>(RsltPtr->getType()))
    RsltTy = dyn_cast<llvm::IntegerType>(RsltPtrTy->getElementType());
  if (!RsltTy) {
    CGM.Error(E->getExprLoc(),
              "First argument to __builtin_bit_from_string must be int* typed");
    return RValue::get(0);
  }

  GlobalVariable *StrGV = 0;
  if (GetElementPtrInst *StrPtr = dyn_cast<GetElementPtrInst>(Str))
    StrGV = dyn_cast<GlobalVariable>(StrPtr->getOperand(0));
  else if (ConstantExpr *StrCE = dyn_cast<ConstantExpr>(Str))
    if (StrCE->getOpcode() == Instruction::GetElementPtr)
      StrGV = dyn_cast<GlobalVariable>(StrCE->getOperand(0));
  ConstantDataArray *CA = 0;
  if (StrGV && StrGV->hasInitializer() && StrGV->isConstant())
    CA = dyn_cast<ConstantDataArray>(StrGV->getInitializer());

  if (!CA || !CA->isCString()) {
    CGM.Error(E->getArg(1)->getExprLoc(),
              "Second argument to __builtin_bit_from_string must be "
              "a constant C string");
    return RValue::get(0);
  }

  uint32_t bits = RsltTy->getBitWidth();

  std::string str(CA->getAsString());

  llvm::ConstantInt *RadixC = dyn_cast<llvm::ConstantInt>(Radix);
  if (!RadixC || RadixC->isZero()) {
    CGM.Error(E->getArg(2)->getExprLoc(),
              "Third argument to __builtin_bit_from_string must be "
              "a constant int");
    return RValue::get(0);
  }
  uint32_t radix = RadixC->getLimitedValue();
  if (radix != 2 && radix != 8 && radix != 10 && radix != 16) {
    CGM.Error(E->getArg(2)->getExprLoc(),
              "Third argument to __builtin_bit_from_string must be "
              "2, 8, 10 or 16");
    return RValue::get(0);
  }

  if (radix == 10)
    radix = getRadix(str);
  if (radix == 0) {
    CGM.Error(E->getArg(1)->getExprLoc(),
              "Can not process radix for second arguments of "
              "__builtin_bit_from_string");
    return RValue::get(0);
  }

  uint32_t strLen = str.length() - 1;
  const char *strp = str.c_str();
  StringRef strpref(strp, strLen);
  uint32_t bitsNeeded = APInt::getBitsNeeded(strpref, radix);
  APInt APIntVal(bitsNeeded, strpref, radix);

  llvm::Value *ConstInt = 0;
  if (bits < bitsNeeded) {
    ConstInt =
        llvm::ConstantInt::get(CGM.getLLVMContext(), APIntVal.trunc(bits));
    CGM.getDiags().Report(
        CGM.getContext().getFullLoc(E->getArg(0)->getExprLoc()),
        diag::warn_builtin_bit_from_string_small_bitwidth)
        << bitsNeeded << bits;
  } else if (bits > bitsNeeded) {
    const char *extension = "zero";
    if (strp[0] == '-') {
      ConstInt =
          llvm::ConstantInt::get(CGM.getLLVMContext(), APIntVal.sext(bits));
      extension = "sign";
    } else
      ConstInt =
          llvm::ConstantInt::get(CGM.getLLVMContext(), APIntVal.zext(bits));
    CGM.getDiags().Report(
        CGM.getContext().getFullLoc(E->getArg(0)->getExprLoc()),
        diag::warn_builtin_bit_from_string_large_bitwidth)
        << bitsNeeded << extension << bits;
  } else
    ConstInt = llvm::ConstantInt::get(CGM.getLLVMContext(), APIntVal);

  ((CGBuilderBaseTy &)Builder).CreateStore(ConstInt, RsltPtr);

  return RValue::get(RsltPtr);
}

/// Convert the __builtin_bit_select pseudo-builtin. The result is an i1 value.
/// See IEEE 1666-2005, System C, Section 7.2.5, pg 175.
RValue CodeGenFunction::EmitBuiltinBitSelect(const CallExpr *E) {
  // Get the Value and bit operands
  Value *ValPtr = EmitScalarExpr(E->getArg(0));
  Value *Bit = EmitScalarExpr(E->getArg(1));

  if (BitCastInst *BC = dyn_cast<BitCastInst>(ValPtr))
    ValPtr = BC->getOperand(0);

  llvm::IntegerType *ValTy = 0;
  // Make sure they are pointers to integers.
  if (llvm::PointerType *ValPtrTy =
          dyn_cast<llvm::PointerType>(ValPtr->getType()))
    ValTy = dyn_cast<llvm::IntegerType>(ValPtrTy->getElementType());

  if (!ValTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "First argument to __builtin_bit_select must be int* typed");
    return RValue::get(0);
  }

  llvm::IntegerType *BitTy = dyn_cast<llvm::IntegerType>(Bit->getType());

  if (!BitTy) {
    CGM.Error(
        E->getArg(1)->getExprLoc(),
        "Second argument to __builtin_bit_select must be an integer type");
    return RValue::get(0);
  }

  // Load the Val from which we select
  Value *Val = ((CGBuilderBaseTy &)Builder).CreateLoad(ValPtr, "");

  // If the Bit index argument isn't the same width as the Val, make it so.
  if (ValTy != BitTy)
    Bit = Builder.CreateIntCast(Bit, ValTy, false, "");

  // Get a mask for the bit of interest
  llvm::ConstantInt *One = llvm::ConstantInt::get(ValTy, 1);
  Value *Mask = Builder.CreateShl(One, Bit, "");

  // And the value with the mask
  Value *And = Builder.CreateAnd(Val, Mask, "");

  // Truncate down to an i1
  llvm::ConstantInt *Zero = llvm::ConstantInt::get(ValTy, 0);
  llvm::Value *Result = Builder.CreateICmpNE(And, Zero, "bit_select");

  llvm::Type *DestTy = ConvertType(E->getType());
  Result = Builder.CreateIntCast(Result, DestTy, false, "cast");

  return RValue::get(Result);
}

/// Convert the __builtin_bit_set builtin.
/// See IEEE 1666-2005, System C, Section 7.2.5, pg 175.
RValue CodeGenFunction::EmitBuiltinBitSet(const CallExpr *E) {
  // Get the Value and bit operands
  Value *RsltPtr = EmitScalarExpr(E->getArg(0));
  Value *ValPtr = EmitScalarExpr(E->getArg(1));
  Value *ReplPtr = EmitScalarExpr(E->getArg(2));
  Value *Bit = EmitScalarExpr(E->getArg(3));

  if (BitCastInst *BC = dyn_cast<BitCastInst>(RsltPtr))
    RsltPtr = BC->getOperand(0);
  if (BitCastInst *BC = dyn_cast<BitCastInst>(ValPtr))
    ValPtr = BC->getOperand(0);
  if (BitCastInst *BC = dyn_cast<BitCastInst>(ReplPtr))
    ReplPtr = BC->getOperand(0);

  llvm::IntegerType *RsltTy = 0, *ValTy = 0, *BitTy = 0, *ReplTy = 0;

  // Make sure they are pointers to integers.
  if (llvm::PointerType *RsltPtrTy =
          dyn_cast<llvm::PointerType>(RsltPtr->getType()))
    RsltTy = dyn_cast<llvm::IntegerType>(RsltPtrTy->getElementType());
  if (!RsltTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "First argument to __builtin_bit_set must be int* type.");
    return RValue::get(0);
  }

  if (llvm::PointerType *ValPtrTy =
          dyn_cast<llvm::PointerType>(ValPtr->getType()))
    ValTy = dyn_cast<llvm::IntegerType>(ValPtrTy->getElementType());
  if (!ValTy) {
    CGM.Error(E->getArg(1)->getExprLoc(),
              "Second argument to __builtin_bit_set must be int* type.");
    return RValue::get(0);
  }

  if (llvm::PointerType *ReplPtrTy =
          dyn_cast<llvm::PointerType>(ReplPtr->getType()))
    ReplTy = dyn_cast<llvm::IntegerType>(ReplPtrTy->getElementType());
  if (!ReplTy) {
    CGM.Error(E->getArg(2)->getExprLoc(),
              "Third argument to __builtin_bit_set must be int* type.");
    return RValue::get(0);
  }

  BitTy = dyn_cast<llvm::IntegerType>(Bit->getType());
  if (!BitTy) {
    CGM.Error(E->getArg(3)->getExprLoc(),
              "Fourth argument to __builtin_bit_set must be int type.");
    return RValue::get(0);
  }

  // If the Bit index argument isn't the same width as the Val, make it so.
  if (ValTy != BitTy)
    Bit = Builder.CreateIntCast(Bit, ValTy, false, "");

  // Load the value and and replacement bits
  Value *Val = ((CGBuilderBaseTy &)Builder).CreateLoad(ValPtr, "");
  Value *Repl = ((CGBuilderBaseTy &)Builder).CreateLoad(ReplPtr, "");

  llvm::Constant *Zero = ConstantInt::getNullValue(ReplTy);
  llvm::Value *ICmp = Builder.CreateICmpNE(Repl, Zero, "");

  llvm::Constant *One = llvm::ConstantInt::get(ValTy, 1);
  Value *BitClear = Builder.CreateShl(One, Bit, "");
  BitClear = Builder.CreateNot(BitClear, "");
  Value *Result = Builder.CreateAnd(Val, BitClear, "");

  Value *ZExt = Builder.CreateZExt(ICmp, ValTy, "");
  Value *BitSet = Builder.CreateShl(ZExt, Bit, "");

  // Truncate down to an i1
  Value *Select = Builder.CreateOr(Result, BitSet, "bit_set");

  // Save the result.
  ((CGBuilderBaseTy &)Builder).CreateStore(Select, RsltPtr);

  return RValue::get(RsltPtr);
}

/// Convert the __builtin_bit_part_select pseudo-builtin.
/// See IEEE 1666-2005, System C, Section 7.2.6, pg 175.
/// NOTE: The implementation accept non-constant Lo and Hi now
RValue CodeGenFunction::EmitBuiltinBitPartSelect(const CallExpr *E) {
  // Get the operands to the function
  Value *RsltPtr = EmitScalarExpr(E->getArg(0));
  Value *ValPtr = EmitScalarExpr(E->getArg(1));
  Value *Lo = EmitScalarExpr(E->getArg(2));
  Value *Hi = EmitScalarExpr(E->getArg(3));

  if (BitCastInst *BC = dyn_cast<BitCastInst>(RsltPtr))
    RsltPtr = BC->getOperand(0);
  if (BitCastInst *BC = dyn_cast<BitCastInst>(ValPtr))
    ValPtr = BC->getOperand(0);

  llvm::IntegerType *RsltTy = 0, *ValTy = 0, *LoTy = 0, *HiTy = 0;

  // Make sure they are pointers to integers.
  if (llvm::PointerType *RsltPtrTy =
          dyn_cast<llvm::PointerType>(RsltPtr->getType()))
    RsltTy = dyn_cast<llvm::IntegerType>(RsltPtrTy->getElementType());
  if (!RsltTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "First argument to __builtin_bit_part_select must be int* typed");
    return RValue::get(0);
  }

  if (llvm::PointerType *ValPtrTy =
          dyn_cast<llvm::PointerType>(ValPtr->getType()))
    ValTy = dyn_cast<llvm::IntegerType>(ValPtrTy->getElementType());
  if (!ValTy) {
    CGM.Error(
        E->getArg(1)->getExprLoc(),
        "Second argument to __builtin_bit_part_select must be int* typed");
    return RValue::get(0);
  }

  LoTy = dyn_cast<llvm::IntegerType>(Lo->getType());
  if (!LoTy) {
    CGM.Error(E->getArg(2)->getExprLoc(),
              "Third argument to __builtin_bit_part_select must be int typed");
    return RValue::get(0);
  }

  HiTy = dyn_cast<llvm::IntegerType>(Hi->getType());
  if (!HiTy) {
    CGM.Error(E->getArg(3)->getExprLoc(),
              "Fourth argument to __builtin_bit_part_select must be int typed");
    return RValue::get(0);
  }

  LLVMContext &Context = CGM.getLLVMContext();
  if (LoTy != llvm::Type::getInt32Ty(Context))
    Lo = Builder.CreateIntCast(Lo, llvm::Type::getInt32Ty(Context), false, "");
  if (HiTy != llvm::Type::getInt32Ty(Context))
    Hi = Builder.CreateIntCast(Hi, llvm::Type::getInt32Ty(Context), false, "");

  auto *PartSelectDecl = Intrinsic::getDeclaration(
      &CGM.getModule(), Intrinsic::fpga_legacy_part_select, {ValTy});
  Value *Args[3] = {((CGBuilderBaseTy &)Builder).CreateLoad(ValPtr, ""), Lo,
                    Hi};
  Value *PartSelect = Builder.CreateCall(PartSelectDecl, Args, "part_select");
  ((CGBuilderBaseTy &)Builder).CreateStore(PartSelect, RsltPtr);

  return RValue::get(RsltPtr);
}

/// Convert the __builtin_bit_part_set pseudo-builtin.
RValue CodeGenFunction::EmitBuiltinBitPartSet(const CallExpr *E) {
  // Get the operands to the function
  Value *RsltPtr = EmitScalarExpr(E->getArg(0));
  Value *ValPtr = EmitScalarExpr(E->getArg(1));
  Value *RepPtr = EmitScalarExpr(E->getArg(2));
  Value *Lo = EmitScalarExpr(E->getArg(3));
  Value *Hi = EmitScalarExpr(E->getArg(4));

  if (BitCastInst *BC = dyn_cast<BitCastInst>(RsltPtr))
    RsltPtr = BC->getOperand(0);
  if (BitCastInst *BC = dyn_cast<BitCastInst>(ValPtr))
    ValPtr = BC->getOperand(0);
  if (BitCastInst *BC = dyn_cast<BitCastInst>(RepPtr))
    RepPtr = BC->getOperand(0);

  llvm::IntegerType *RsltTy = 0, *ValTy = 0, *LoTy = 0, *HiTy = 0, *RepTy = 0;

  // Make sure they are pointers to integers.
  if (llvm::PointerType *RsltPtrTy =
          dyn_cast<llvm::PointerType>(RsltPtr->getType()))
    RsltTy = dyn_cast<llvm::IntegerType>(RsltPtrTy->getElementType());
  if (!RsltTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "First argument to __builtin_bit_part_set must be int* typed");
    return RValue::get(0);
  }

  if (llvm::PointerType *ValPtrTy =
          dyn_cast<llvm::PointerType>(ValPtr->getType()))
    ValTy = dyn_cast<llvm::IntegerType>(ValPtrTy->getElementType());
  if (!ValTy) {
    CGM.Error(E->getArg(1)->getExprLoc(),
              "Second argument to __builtin_bit_part_set must be int* typed");
    return RValue::get(0);
  }
  if (llvm::PointerType *RepPtrTy =
          dyn_cast<llvm::PointerType>(RepPtr->getType()))
    RepTy = dyn_cast<llvm::IntegerType>(RepPtrTy->getElementType());
  if (!RepTy) {
    CGM.Error(E->getArg(2)->getExprLoc(),
              "Third argument to __builtin_bit_part_set must be int* typed");
    return RValue::get(0);
  }

  LoTy = dyn_cast<llvm::IntegerType>(Lo->getType());
  if (!LoTy) {
    CGM.Error(E->getArg(3)->getExprLoc(),
              "Fourth argument to __builtin_bit_part_set must be int typed");
    return RValue::get(0);
  }

  HiTy = dyn_cast<llvm::IntegerType>(Hi->getType());
  if (!HiTy) {
    CGM.Error(E->getArg(4)->getExprLoc(),
              "Fifth argument to __builtin_bit_part_set must be int typed");
    return RValue::get(0);
  }

  LLVMContext &Context = CGM.getLLVMContext();
  if (LoTy != llvm::Type::getInt32Ty(Context))
    Lo = Builder.CreateIntCast(Lo, llvm::Type::getInt32Ty(Context), false, "");
  if (HiTy != llvm::Type::getInt32Ty(Context))
    Hi = Builder.CreateIntCast(Hi, llvm::Type::getInt32Ty(Context), false, "");

  auto *PartSetDecl = Intrinsic::getDeclaration(
      &CGM.getModule(), Intrinsic::fpga_legacy_part_set, {ValTy, RepTy});

  Value *Args[4] = {((CGBuilderBaseTy &)Builder).CreateLoad(ValPtr, ""),
                    ((CGBuilderBaseTy &)Builder).CreateLoad(RepPtr, ""), Lo,
                    Hi};
  Value *PartSet = Builder.CreateCall(PartSetDecl, Args, "part_set");
  ((CGBuilderBaseTy &)Builder).CreateStore(PartSet, RsltPtr);

  return RValue::get(RsltPtr);
}

/// Convert the __builtin_bit_and_reduce builtin. This builtin requires one
/// integer operand (of arbitrary bit width). It sequentially ands the bits
/// together and returns the resulting bit. This is equivalent to counting the
/// zeros and returning 0 if any are found or 1 otherwise.
/// See IEEE 1666-2005, System C, Section 7.2.8, pg 178.
RValue CodeGenFunction::EmitBuiltinBitAndReduce(const CallExpr *E) {
  // Get the operand to the function
  Value *Operand = EmitScalarExpr(E->getArg(0));
  if (BitCastInst *BC = dyn_cast<BitCastInst>(Operand))
    Operand = BC->getOperand(0);
  llvm::IntegerType *OpTy = 0;
  if (llvm::PointerType *OpPtr =
          dyn_cast<llvm::PointerType>(Operand->getType()))
    OpTy = dyn_cast<llvm::IntegerType>(OpPtr->getElementType());

  if (!OpTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "Argument to __builtin_bit_and_reduce must be of int* type");
    return RValue::get(0);
  }

  Operand = ((CGBuilderBaseTy &)Builder).CreateLoad(Operand, "");
  llvm::Constant *AllOnes = llvm::Constant::getAllOnesValue(OpTy);
  Value *Result = Builder.CreateICmpEQ(Operand, AllOnes, "and_reduce");

  llvm::Type *DestTy = ConvertType(E->getType());
  Result = Builder.CreateIntCast(Result, DestTy, false, "cast");

  return RValue::get(Result);
}

// Convert the __builtin_bit_nand_reduce builtin. This builtin requires one
// integer operand (of arbitrary bit width). It sequentially ands the bits
// together and returns the resulting bit. This is equivalent to counting the
// zeros and returning 0 if any are found or 1 otherwise.
// See IEEE 1666-2005, System C, Section 7.2.8, pg 178.
RValue CodeGenFunction::EmitBuiltinBitNAndReduce(const CallExpr *E) {
  // Get the operand to the function
  Value *Operand = EmitScalarExpr(E->getArg(0));
  if (BitCastInst *BC = dyn_cast<BitCastInst>(Operand))
    Operand = BC->getOperand(0);
  llvm::IntegerType *OpTy = 0;
  if (llvm::PointerType *OpPtr =
          dyn_cast<llvm::PointerType>(Operand->getType()))
    OpTy = dyn_cast<llvm::IntegerType>(OpPtr->getElementType());
  if (!OpTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "Argument to __builtin_bit_nand_reduce must be of int* type");
    return RValue::get(0);
  }

  Operand = ((CGBuilderBaseTy &)Builder).CreateLoad(Operand, "");
  llvm::Constant *AllOnes = llvm::Constant::getAllOnesValue(OpTy);
  Value *Result = Builder.CreateICmpNE(Operand, AllOnes, "nand_reduce");

  llvm::Type *DestTy = ConvertType(E->getType());
  Result = Builder.CreateIntCast(Result, DestTy, false, "cast");

  return RValue::get(Result);
}

// Convert the __builtin_bit_or_reduce builtin. This builtin requires one
// integer operand (of arbitrary bit width). It sequentially ors the bits
// together and returns the resulting bit. This is equivalent to returning
// 0 if the operand is all zeros, 1 others.
// value against 0 and returning 1 if it is not zero, 0 otherwise.
// See IEEE 1666-2005, System C, Section 7.2.8, pg 178.
RValue CodeGenFunction::EmitBuiltinBitOrReduce(const CallExpr *E) {
  // Get the operand to the function
  Value *Operand = EmitScalarExpr(E->getArg(0));
  if (BitCastInst *BC = dyn_cast<BitCastInst>(Operand))
    Operand = BC->getOperand(0);
  llvm::IntegerType *OpTy = 0;
  if (llvm::PointerType *OpPtr =
          dyn_cast<llvm::PointerType>(Operand->getType()))
    OpTy = dyn_cast<llvm::IntegerType>(OpPtr->getElementType());
  if (!OpTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "Argument to __builtin_bit_or_reduce must be of int* type");
    return RValue::get(0);
  }

  Operand = ((CGBuilderBaseTy &)Builder).CreateLoad(Operand, "");
  llvm::Constant *Zero = llvm::ConstantInt::getNullValue(OpTy);
  Value *Result = Builder.CreateICmpNE(Operand, Zero, "or_reduce");

  llvm::Type *DestTy = ConvertType(E->getType());
  Result = Builder.CreateIntCast(Result, DestTy, false, "cast");

  return RValue::get(Result);
}

/// Convert the _builtint_bit_nor_reduce builtin. This builtin requires one
/// integer operand (of arbitrary bit width). It sequentially nor's the bits
/// together and returns the resulting bit of the last nor. This is equivalent
/// to returning 1 if the operand is all 0s, 0 otherwise. It is also the inverse
/// of the __builtin_bit_reduce_nor builtin.
/// See IEEE 1666-2005, System C, Section 7.2.8, pg 178.
RValue CodeGenFunction::EmitBuiltinBitNOrReduce(const CallExpr *E) {
  // Get the operand to the function
  Value *Operand = EmitScalarExpr(E->getArg(0));
  if (BitCastInst *BC = dyn_cast<BitCastInst>(Operand))
    Operand = BC->getOperand(0);
  llvm::IntegerType *OpTy = 0;
  if (llvm::PointerType *OpPtr =
          dyn_cast<llvm::PointerType>(Operand->getType()))
    OpTy = dyn_cast<llvm::IntegerType>(OpPtr->getElementType());
  if (!OpTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "Argument to __builtin_bit_nor_reduce must be of int* type");
    return RValue::get(0);
  }

  Operand = ((CGBuilderBaseTy &)Builder).CreateLoad(Operand, "");
  llvm::Constant *Zero = llvm::ConstantInt::getNullValue(OpTy);
  Value *Result = Builder.CreateICmpEQ(Operand, Zero, "nor_reduce");

  llvm::Type *DestTy = ConvertType(E->getType());
  Result = Builder.CreateIntCast(Result, DestTy, false, "cast");

  return RValue::get(Result);
}

/// Convert the __builtin_bit_xor_reduce builtin. This builtin requires one
/// integer operand (of arbitrary bit width). It sequentially xor's the bits
/// together and returns the resulting bit of the last xor.
/// See IEEE 1666-2005, System C, Section 7.2.8, pg 178.
RValue CodeGenFunction::EmitBuiltinBitXorReduce(const CallExpr *E) {
  // Get the operand to the function
  Value *Operand = EmitScalarExpr(E->getArg(0));
  if (BitCastInst *BC = dyn_cast<BitCastInst>(Operand))
    Operand = BC->getOperand(0);

  llvm::IntegerType *OpTy = 0;
  if (llvm::PointerType *OpPtr =
          dyn_cast<llvm::PointerType>(Operand->getType()))
    OpTy = dyn_cast<llvm::IntegerType>(OpPtr->getElementType());
  if (!OpTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "Argument to __builtin_bit_xor_reduce must be of integer type");
    return RValue::get(0);
  }

  Operand = ((CGBuilderBaseTy &)Builder).CreateLoad(Operand, "");

  uint32_t OpBits = OpTy->getBitWidth();
  std::string IntrName("llvm.ctpop.i" + llvm::utostr(OpBits));

  std::vector<llvm::Type *> args;
  args.push_back(OpTy);

  LLVMContext &Context = CGM.getLLVMContext();
  llvm::FunctionType *FT = llvm::FunctionType::get(
      llvm::IntegerType::get(Context, OpBits), args, false);

  llvm::Function *F =
      cast<llvm::Function>(CGM.CreateRuntimeFunction(FT, IntrName));

  Value *Result = Builder.CreateCall(F, Operand, "");
  Result =
      Builder.CreateTrunc(Result, llvm::Type::getInt1Ty(Context), "xor_reduce");

  llvm::Type *DestTy = ConvertType(E->getType());
  Result = Builder.CreateIntCast(Result, DestTy, false, "cast");
  return RValue::get(Result);
}

/// Convert the __builtin_bit_nxor_reduce builtin. This builtin requires one
/// integer operand (of arbitrary bit width). It sequentially nxor's the bits
/// together and returns the resulting bit of the last nxor.
/// See IEEE 1666-2005, System C, Section 7.2.8, pg 178.
RValue CodeGenFunction::EmitBuiltinBitNXorReduce(const CallExpr *E) {
  // Get the operand to the function
  Value *Operand = EmitScalarExpr(E->getArg(0));
  if (BitCastInst *BC = dyn_cast<BitCastInst>(Operand))
    Operand = BC->getOperand(0);

  llvm::IntegerType *OpTy = 0;
  if (llvm::PointerType *OpPtr =
          dyn_cast<llvm::PointerType>(Operand->getType()))
    OpTy = dyn_cast<llvm::IntegerType>(OpPtr->getElementType());
  if (!OpTy) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "Argument to __builtin_bit_nxor_reduce must be of integer type");
    return RValue::get(0);
  }

  Operand = ((CGBuilderBaseTy &)Builder).CreateLoad(Operand, "");

  uint32_t OpBits = OpTy->getBitWidth();
  std::string IntrName("llvm.ctpop.i" + llvm::utostr(OpBits));

  std::vector<llvm::Type *> args;
  args.push_back(OpTy);

  LLVMContext &Context = CGM.getLLVMContext();
  llvm::FunctionType *FT = llvm::FunctionType::get(
      llvm::IntegerType::get(Context, OpBits), args, false);

  llvm::Function *F =
      cast<llvm::Function>(CGM.CreateRuntimeFunction(FT, IntrName));

  Value *Result = Builder.CreateCall(F, Operand, "");
  Result = Builder.CreateTrunc(Result, llvm::Type::getInt1Ty(Context), "");
  Result = Builder.CreateNot(Result, "nxor_reduce");

  llvm::Type *DestTy = ConvertType(E->getType());
  Result = Builder.CreateIntCast(Result, DestTy, false, "cast");
  return RValue::get(Result);
}

Value* CodeGenFunction::EmitBuiltinFPGANPortChannel(const CallExpr* E) { 
  Value* inPort = EmitPointerWithAlignment(E->getArg(0)).getPointer();
  Value* outPort = EmitPointerWithAlignment(E->getArg(1)).getPointer();
  Value* inPortNum = EmitScalarExpr(E->getArg(2));
  Value* outPortNum = EmitScalarExpr(E->getArg(3));
  Value* depth = EmitScalarExpr(E->getArg(4));
  Value* Alg = EmitScalarExpr(E->getArg(5));
  Value* depth_1 = EmitScalarExpr(E->getArg(6));

  auto *sideeffect = llvm::Intrinsic::getDeclaration(
        CurFn->getParent(), llvm::Intrinsic::sideeffect);

  llvm::Value* args[] = {
    inPort,
    outPort,
    inPortNum,
    outPortNum, 
    depth, 
    Alg,
    depth_1
  };

  SmallVector<llvm::OperandBundleDef, 7> bundleDefs;
  bundleDefs.emplace_back( "nport_channel", args );

  auto* call = Builder.CreateCall(sideeffect, None, bundleDefs );

  //std::pair<unsigned, llvm::Attribute> attrs = { std::make_pair(llvm::AttributeList::FunctionIndex, llvm::Attribute::get(getLLVMContext(), "xlx.port.bitwidth", std::to_string(port_width))) };
  //llvm::AttributeList attr_list = llvm::AttributeList::get( getLLVMContext(), attrs);
  //call->setAttributes(attr_list);

  call->setOnlyAccessesInaccessibleMemory();
  call->setDoesNotThrow();
  return call;
}

Value* CodeGenFunction::EmitBuiltinFPGAIP(const CallExpr* E) {
  unsigned ArgNum = E->getNumArgs();
  if (ArgNum % 2 != 1) {
    CGM.Error(E->getExprLoc(),
              "Argument number mismatch: need IP name followed by pairs of parameter name and value");
    return nullptr;
  }

  SmallVector<Value *, 8> args;

  const StringLiteral *Literal = dyn_cast<StringLiteral>(E->getArg(0));
  if (!Literal) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "This argument should be string literal");
    return nullptr;
  }
  StringRef name = Literal->getString();

  auto xilinxPlatform = platform::PlatformBasic::getInstance();
  auto op = xilinxPlatform->getOpFromName("vivado_ip");
  auto impl = xilinxPlatform->getImplFromName(name);
  if (impl == -1) {
    CGM.Error(E->getArg(0)->getExprLoc(),
              "There is no such IP");
    return nullptr;
  }

  args.push_back(Builder.getInt64(op));
  args.push_back(Builder.getInt64(impl));

  for (unsigned i = 1; i < ArgNum; i++) {
    const Expr *Arg = E->getArg(i);
    Value *Ptr = Arg->getType()->isArrayType() ?
                     EmitLValue(Arg).getPointer() :
                     EmitScalarExpr(Arg);
    if (auto v = dyn_cast<ConstantInt>(Ptr)) {
      if (v->getBitWidth() == 1) {
        Ptr = Builder.getInt32(v->getZExtValue());
      }
    }
    args.push_back(Ptr);
  }

  auto *decl = Intrinsic::getDeclaration(
        CurFn->getParent(), Intrinsic::sideeffect);
  auto *call = Builder.CreateCall(decl, None, OperandBundleDef(XlxIPInst::BundleTagName, args));

  call->setOnlyAccessesInaccessibleMemory();
  call->setDoesNotThrow();

  return call;
}

Value* CodeGenFunction::EmitBuiltinFPGAFence(const CallExpr* E) { 
  std::vector<llvm::Value *> args;
  unsigned ArgNum = E->getNumArgs();
  assert(ArgNum >= 2);

  llvm::ConstantInt *BeforeNumC = 
      cast<llvm::ConstantInt>(EmitScalarExpr(E->getArg(0)));
  llvm::ConstantInt *AfterNumC = 
      cast<llvm::ConstantInt>(EmitScalarExpr(E->getArg(1)));
  unsigned BeforeNum = BeforeNumC->getZExtValue();
  unsigned AfterNum = AfterNumC->getZExtValue();
  if (BeforeNum + AfterNum != ArgNum - 2) {
    if (BeforeNum + AfterNum > ArgNum - 2) {
      CGM.Error(E->getExprLoc(),
                "Argument number mismatch: too less pointer arguments passed to __fpga_fence");
    } else { 
      CGM.Error(E->getExprLoc(),
                "Argument number mismatch: too many pointer arguments passed to __fpga_fence");
    }
    return nullptr;
  }

  for (unsigned i = 0; i < BeforeNum; i++) {
    const Expr *Arg = E->getArg(i+2);
    llvm::Value *Ptr = Arg->getType()->isPointerType() ? 
                           EmitScalarExpr(Arg) :
                           EmitLValue(Arg).getPointer();
    args.push_back(Ptr);
  }

  args.push_back(Builder.getInt32(-1));

  for (unsigned i = 0; i < AfterNum; i++) {
    const Expr *Arg = E->getArg(i+BeforeNum+2);
    llvm::Value *Ptr = Arg->getType()->isPointerType() ? 
                           EmitScalarExpr(Arg) :
                           EmitLValue(Arg).getPointer();
    args.push_back(Ptr);
  }

  auto *fence = llvm::Intrinsic::getDeclaration(
        CurFn->getParent(), Intrinsic::fpga_fence);
  
  auto *call = Builder.CreateCall(fence, args);
  return call;
}

