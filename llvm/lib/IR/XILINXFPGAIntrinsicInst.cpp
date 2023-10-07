// (C) Copyright 2016-2022 Xilinx, Inc.
// Copyright (C) 2023, Advanced Micro Devices, Inc.
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
// This file implements classes that make it really easy to deal with intrinsic
// functions in FPGA with the isa/dyncast family of functions.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/IR/XILINXFPGAIntrinsicInst.h"

using namespace llvm;
using namespace PatternMatch;

static bool isValidPragmaSource(StringRef S) {
  return S == "user" || S == "infer-from-pragma" || S == "infer-from-design";
}

StringRef llvm::getPragmaSourceFromMDNode(MDNode * MD) {
  if (!MD) {
    return StringRef();
  }

  unsigned Index = MD->getNumOperands() - 1;
  if (Index < 1) {
    return StringRef();
  }

  Metadata *M = MD->getOperand(Index).get();
  if (!M || isa<DILocation>(M)) {
    Index--;
    if (Index < 1) {
      return StringRef();
    }

    M = MD->getOperand(Index).get();
  }

  auto *S = dyn_cast<MDString>(M);
  if (!S) {
    return StringRef();
  }

  auto Source = S->getString();
  if (!isValidPragmaSource(Source)) {
    return StringRef();
  }

  return Source;
}

static Value *StripBitOrPointerCast(Value *V) {
  if (auto Cast = dyn_cast<BitCastInst>(V))
    return StripBitOrPointerCast(Cast->getOperand(0));

  if (auto Cast = dyn_cast<IntToPtrInst>(V))
    return StripBitOrPointerCast(Cast->getOperand(0));

  if (auto Cast = dyn_cast<PtrToIntInst>(V))
    return StripBitOrPointerCast(Cast->getOperand(0));

  return V;
}

Value *BitConcatInst::getBits(unsigned Hi, unsigned Lo) const {
  unsigned CurHi = getType()->getBitWidth();
  for (auto &V : arg_operands()) {
    if (Hi > CurHi)
      break;

    unsigned SizeInBits = V->getType()->getIntegerBitWidth();
    if (Hi + 1 == CurHi && Lo == CurHi - SizeInBits)
      return V;

    assert(CurHi >= SizeInBits && "Bad size!");
    CurHi -= SizeInBits;
  }

  return nullptr;
}

Value *BitConcatInst::getElement(unsigned Hi, unsigned Lo) const {
  return StripBitOrPointerCast(getBits(Hi, Lo));
}

IntegerType *PartSelectInst::getRetTy() const {
  return cast<IntegerType>(getType());
}

Value *PartSelectInst::getSrc() const { return getOperand(0); }

IntegerType *PartSelectInst::getSrcTy() const {
  return cast<IntegerType>(getSrc()->getType());
}

Value *PartSelectInst::getOffset() const { return getOperand(1); }

IntegerType *PartSelectInst::getOffsetTy() const {
  return cast<IntegerType>(getOffset()->getType());
}

IntegerType *LegacyPartSelectInst::getRetTy() const {
  return cast<IntegerType>(getType());
}

Value *LegacyPartSelectInst::getSrc() const { return getOperand(0); }

IntegerType *LegacyPartSelectInst::getSrcTy() const {
  return cast<IntegerType>(getSrc()->getType());
}

Value *LegacyPartSelectInst::getLo() const { return getOperand(1); }

Value *LegacyPartSelectInst::getHi() const { return getOperand(2); }

IntegerType *PartSetInst::getRetTy() const {
  return cast<IntegerType>(getType());
}

Value *PartSetInst::getSrc() const { return getOperand(0); }

IntegerType *PartSetInst::getSrcTy() const {
  return cast<IntegerType>(getSrc()->getType());
}

Value *PartSetInst::getRep() const { return getOperand(1); }

IntegerType *PartSetInst::getRepTy() const {
  return cast<IntegerType>(getRep()->getType());
}

Value *PartSetInst::getOffset() const { return getOperand(2); }

IntegerType *PartSetInst::getOffsetTy() const {
  return cast<IntegerType>(getOffset()->getType());
}

IntegerType *LegacyPartSetInst::getRetTy() const {
  return cast<IntegerType>(getType());
}

Value *LegacyPartSetInst::getSrc() const { return getOperand(0); }

IntegerType *LegacyPartSetInst::getSrcTy() const {
  return cast<IntegerType>(getSrc()->getType());
}

Value *LegacyPartSetInst::getRep() const { return getOperand(1); }

IntegerType *LegacyPartSetInst::getRepTy() const {
  return cast<IntegerType>(getRep()->getType());
}

Value *LegacyPartSetInst::getLo() const { return getOperand(2); }

Value *LegacyPartSetInst::getHi() const { return getOperand(3); }

Value *UnpackNoneInst::getOperand() const { return getArgOperand(0); }

Value *PackNoneInst::getOperand() const { return getArgOperand(0); }

Value *UnpackBytesInst::getOperand() const { return getArgOperand(0); }

Value *PackBytesInst::getOperand() const { return getArgOperand(0); }

Value *UnpackBitsInst::getOperand() const { return getArgOperand(0); }

Value *PackBitsInst::getOperand() const { return getArgOperand(0); }

Value *FPGALoadStoreInst::getPointerOperand() {
  if (auto *LD = dyn_cast<FPGALoadInst>(this))
    return LD->getPointerOperand();
  if (auto *LD = dyn_cast<FPGAPPPOLoadInst>(this))
    return LD->getPointerOperand();
  if (auto *ST = dyn_cast<FPGAPPPOStoreInst>(this))
    return ST->getPointerOperand();

  return cast<FPGAStoreInst>(this)->getPointerOperand();
}

const Value *FPGALoadStoreInst::getPointerOperand() const {
  if (auto *LD = dyn_cast<FPGALoadInst>(this))
    return LD->getPointerOperand();
  if (auto *LD = dyn_cast<FPGAPPPOLoadInst>(this))
    return LD->getPointerOperand();
  if (auto *ST = dyn_cast<FPGAPPPOStoreInst>(this))
    return ST->getPointerOperand();

  return cast<FPGAStoreInst>(this)->getPointerOperand();
}

unsigned FPGALoadStoreInst::getPointerAddressSpace() const {
  return getPointerOperand()->getType()->getPointerAddressSpace();
}

PointerType *FPGALoadStoreInst::getPointerType() const {
  return cast<PointerType>(getPointerOperand()->getType());
}

unsigned FPGALoadStoreInst::getAlignment() const {
  if (auto *LD = dyn_cast<FPGALoadInst>(this))
    return LD->getAlignment();
  if (auto *LD = dyn_cast<FPGAPPPOLoadInst>(this))
    return LD->getAlignment();
  if (auto *ST = dyn_cast<FPGAPPPOStoreInst>(this))
    return ST->getAlignment();

  return cast<FPGAStoreInst>(this)->getAlignment();
}

Type *FPGALoadStoreInst::getDataType() const {
  return getPointerType()->getElementType();
}

bool FPGALoadStoreInst::isVolatile() const {
  if (isa<FPGAPPPOLoadStoreInst>(this))
    return true;

  unsigned NumArgs = getNumArgOperands();
  return cast<ConstantInt>(getArgOperand(NumArgs - 1))->isOne();
}

Value *SeqBeginInst::getPointerOperand() const { return getArgOperand(0); }

PointerType *SeqBeginInst::getPointerType() const {
  return cast<PointerType>(getPointerOperand()->getType());
}

void SeqBeginInst::updatePointer(Value *V) { getArgOperandUse(0).set(V); }

uint64_t SeqBeginInst::getSmallConstantSize() const {
  uint64_t Size = 0;
  match(getSize(), m_ConstantInt(Size));
  return Size;
}

uint64_t SeqBeginInst::getSmallConstantSizeInBytes(const DataLayout &DL) const {
  return getSmallConstantSize() * DL.getTypeAllocSize(getDataType());
}

void SeqBeginInst::updateSize(Value *V) { return getArgOperandUse(1).set(V); }

SeqBeginInst *SeqEndInst::getBegin() const {
  return cast<SeqBeginInst>(getArgOperand(0));
}

void SeqEndInst::updateSize(Value *V) { return getArgOperandUse(1).set(V); }

Type *SeqAccessInst::getDataType() const {
  if (auto *Ld = dyn_cast<SeqLoadInst>(this))
    return Ld->getDataType();

  return cast<SeqStoreInst>(this)->getDataType();
}

SeqBeginInst *SeqAccessInst::getPointerOperand() const {
  if (auto *Ld = dyn_cast<SeqLoadInst>(this))
    return Ld->getPointerOperand();

  return cast<SeqStoreInst>(this)->getPointerOperand();
}

Value *SeqAccessInst::getIndex() const {
  if (auto *Ld = dyn_cast<SeqLoadInst>(this))
    return Ld->getIndex();

  return cast<SeqStoreInst>(this)->getIndex();
}

void SeqAccessInst::updateIndex(Value *V) {
  if (auto *Ld = dyn_cast<SeqLoadInst>(this))
    return Ld->updateIndex(V);

  return cast<SeqStoreInst>(this)->updateIndex(V);
}

SeqBeginInst *SeqLoadInst::getPointerOperand() const {
  return cast<SeqBeginInst>(getArgOperand(0));
}

void SeqLoadInst::updateIndex(Value *V) { getArgOperandUse(1).set(V); }

SeqBeginInst *SeqStoreInst::getPointerOperand() const {
  return cast<SeqBeginInst>(getArgOperand(1));
}

void SeqStoreInst::updateIndex(Value *V) { getArgOperandUse(2).set(V); }

Value *SeqStoreInst::getValueOperand() const { return getArgOperand(0); }

Value *SeqStoreInst::getByteEnable() const { return getArgOperand(3); }

bool SeqStoreInst::isMasked() const {
  return !match(getByteEnable(), m_AllOnes());
}

Value *ShiftRegInst::getPointerOperand() const {
  if (auto *Shift = dyn_cast<ShiftRegShiftInst>(this))
    return Shift->getPointerOperand();

  return cast<ShiftRegPeekInst>(this)->getPointerOperand();
}

Type *ShiftRegInst::getDataType() const {
  if (auto *Shift = dyn_cast<ShiftRegShiftInst>(this))
    return Shift->getDataType();

  return cast<ShiftRegPeekInst>(this)->getType();
}

Value *ShiftRegShiftInst::getValueOperand() const { return getArgOperand(0); }
Value *ShiftRegShiftInst::getPointerOperand() const { return getArgOperand(1); }
Value *ShiftRegShiftInst::getPredicate() const { return getArgOperand(2); }
Value *ShiftRegPeekInst::getPointerOperand() const { return getArgOperand(0); }
Value *ShiftRegPeekInst::getIndex() const { return getArgOperand(1); }

DirectiveScopeExit *
DirectiveScopeEntry::BuildDirectiveScope(ArrayRef<OperandBundleDef> ScopeAttrs,
                                         Instruction &Entry,
                                         Instruction &Exit) {
  IRBuilder<> Builder(&Entry);
  auto *ScopeEntry = llvm::Intrinsic::getDeclaration(
      Entry.getModule(), llvm::Intrinsic::directive_scope_entry);

  auto *Token = Builder.CreateCall(ScopeEntry, None, ScopeAttrs);

  Builder.SetInsertPoint(&Exit);
  auto *ScopeExit = llvm::Intrinsic::getDeclaration(
      Entry.getModule(), llvm::Intrinsic::directive_scope_exit);
  return cast<DirectiveScopeExit>(Builder.CreateCall(ScopeExit, Token));
}

DirectiveScopeExit *DirectiveScopeEntry::BuildDirectiveScope(
    StringRef Tag, ArrayRef<Value *> Operands, Instruction &Entry,
    Instruction &Exit) {
  SmallVector<OperandBundleDef, 4> ScopeAttrs;
  ScopeAttrs.emplace_back(Tag, Operands);
  return BuildDirectiveScope(ScopeAttrs, Entry, Exit);
}

// FIXME GEPOperator don't have good API...
static Value *getGEPIndex(GEPOperator *GEP, unsigned Idx) {
  assert(Idx < GEP->getNumIndices() &&
         "Idx out of range of GEPOperand's operands");
  auto I = GEP->idx_begin();
  while (Idx-- > 0)
    ++I;
  return I->get();
}

// FIXME GEPOperator don't have good API...
static Type *getGEPIndexType(GEPOperator *GEP, unsigned Idx) {
  assert(Idx < GEP->getNumIndices() &&
         "Idx out of range of GEPOperand's operands");
  auto T = gep_type_begin(GEP);
  while (Idx-- > 0)
    ++T;
  return T.getIndexedType();
}

// FIXME This is necessary because our pragmas on arrays use the "decayed" form
//       In the future we should clean that up...
static Type *getUndecayedVariableType(Value *Var) {
  // Clang generates a bit cast that removes exactly one dimension:
  //
  //     %decayed = bitcast [10 x i8]* %var to i8*
  //
  if (auto *Cast = dyn_cast<BitCastOperator>(Var)) {
    Type *SrcTy = Cast->getSrcTy();
    Type *DestTy = Cast->getDestTy();
    if (!SrcTy->isPointerTy() || !DestTy->isPointerTy())
      return nullptr;

    SrcTy = SrcTy->getPointerElementType();
    DestTy = DestTy->getPointerElementType();

    if (SrcTy->isArrayTy() && SrcTy->getArrayElementType() == DestTy)
      return SrcTy;
  }

  // LLVM optimize it into a gep into its first element
  //
  //     %decayed = getelementptr [10 x i8], [10 x i8]* %var, i64 0, i64 0
  //
  if (auto *GEP = dyn_cast<GEPOperator>(Var)) {
    unsigned LastIdx = GEP->getNumIndices() - 1;
    if (!match(getGEPIndex(GEP, LastIdx), m_Zero()))
      return nullptr;

    if (LastIdx == 0)
      return getUndecayedVariableType(GEP->getPointerOperand());

    Type *SrcTy = getGEPIndexType(GEP, LastIdx-1);
    Type *DestTy = GEP->getResultElementType();

    if (SrcTy->isArrayTy() && SrcTy->getArrayElementType() == DestTy)
      return SrcTy;
  }

  return nullptr;
}

uint64_t PragmaInst::guessPragmaVarAllocaSizeInBits(const DataLayout &DL) const {
  // If the pragma has a VarAllocSize, use that
  if (getPragmaVarAllocaSizeInBits() > 0)
    return getPragmaVarAllocaSizeInBits();

  // Else we try to guess from the type
  Value *V = getVariable();
  Type *VTy = V->getType();

  // If it is a pointer, get the element type
  if (isa<PointerType>(VTy))
    VTy = VTy->getPointerElementType();

  // If it is a "array" pragma, the argument is probably decayed
  if (Type *Ty = getUndecayedVariableType(V))
    VTy = Ty;

  // And we are done
  return DL.getTypeAllocSizeInBits(VTy);
}

// Return true if this pragma should be applied on variable declaration site.
bool PragmaInst::ShouldBeOnDeclaration() const {
  if (isa<DisaggrInst>(this) || isa<AggregateInst>(this) ||
      isa<ArrayPartitionInst>(this) || isa<ArrayReshapeInst>(this) ||
      isa<StreamPragmaInst>(this) || isa<StreamOfBlocksPragmaInst>(this) ||
      isa<PipoPragmaInst>(this) || isa<BindStoragePragmaInst>(this) ||
      isa<StreamLabelInst>(this) || isa<StreamOfBlocksLabelInst>(this) ||
      isa<ShiftRegLabelInst>(this)) {
    return true;
  } else {
    // Use assert to make sure we cover all pragmas on variables.
    assert((isa<DependenceInst>(this) || isa<StableInst>(this) ||
            isa<StableContentInst>(this) || isa<SharedInst>(this) ||
            isa<BindOpPragmaInst>(this) || isa<ConstSpecInst>(this) ||
            isa<CrossDependenceInst>(this) || isa<SAXIInst>(this) ||
            isa<MaxiInst>(this) || isa<AxiSInst>(this) || 
            isa<ApFifoInst>(this) || isa<ApMemoryInst>(this) ||
            isa<BRAMInst>(this) || isa<ApStableInst>(this) ||
            isa<ApNoneInst>(this) || isa<ApAckInst>(this) ||
            isa<ApVldInst>(this) || isa<ApOvldInst>(this) ||
            isa<ApHsInst>(this)  || isa<StreamLabelInst>(this) ||
            isa<StreamOfBlocksLabelInst>(this) || isa<ShiftRegLabelInst>(this) ||
            isa<ApCtrlNoneInst>(this)  || isa<ApCtrlChainInst>(this) ||
            isa<ApCtrlHsInst>(this) || isa<FPGAResourceLimitInst>(this) ||
            isa<XlxFunctionAllocationInst>(this) || isa<ResetPragmaInst>(this) ||
            isa<MAXIAliasInst>(this) || isa<NPortChannelInst>(this) ||
            isa<FuncInstantiateInst>(this) || isa<ArrayStencilInst>(this) ||
            isa<XlxIPInst>(this) || isa<MaxiCacheInst>(this)
            ) && "Unexpected pragma");
    return false;
  }
}

Value *PragmaInst::getVariable() const {
  if (const DependenceInst *DepInst = dyn_cast<DependenceInst>(this))
    return DepInst->getVariable();
  assert(getNumOperandBundles() == 1 &&
         "PragmaInst is invalid and its bundle num should be 1");
  OperandBundleUse Bundle = getOperandBundleAt(0);
  return Bundle.Inputs[0];
}

bool PragmaInst::isUserPragma() const {
  return "user" == getPragmaSource();
}

bool InterfaceInst::classof(const PragmaInst *I) {
  return (isa<SAXIInst>(I)
       || isa<MaxiInst>(I)
       || isa<AxiSInst>(I)
       || isa<ApFifoInst>(I)
       || isa<ApMemoryInst>(I)
       || isa<BRAMInst>(I)
       || isa<ApStableInst>(I)
       || isa<ApNoneInst>(I)
       || isa<ApAckInst>(I)
       || isa<ApVldInst>(I)
       || isa<ApOvldInst>(I)
       || isa<ApHsInst>(I)
       || isa<ApCtrlNoneInst>(I)
       || isa<ApCtrlChainInst>(I)
       || isa<ApCtrlHsInst>(I)
      );
}

// collect allocation pragmas (in corresponding region if it's specified)
void XlxFunctionAllocationInst::getAll(
    Function *F, SmallVectorImpl<XlxFunctionAllocationInst *> &Allocations,
    Function *RegionF) {

  for (auto *U : F->users()) {
    auto *CI = cast<CallInst>(U);
    // only consider this region scope if the region is specified
    if (RegionF && CI->getFunction() != RegionF)
      continue;
    if (isa<XlxFunctionAllocationInst>(U))
      Allocations.push_back(cast<XlxFunctionAllocationInst>(U));
  }
}

const std::string AXISChannelInst::BundleTagName = "fpga.axis.channel";
const std::string DependenceInst::BundleTagName = "fpga.dependence";
const std::string CrossDependenceInst::BundleTagName = "fpga.cross.dependence";
const std::string MAXIAliasInst::BundleTagName = "fpga.maxi.alias";
const std::string FuncInstantiateInst::BundleTagName = "fpga.func.instantiate";
const std::string ArrayStencilInst::BundleTagName = "fpga_array_stencil";
const std::string StableInst::BundleTagName = "stable";
const std::string StableContentInst::BundleTagName = "stable_content";
const std::string SharedInst::BundleTagName = "shared";
const std::string DisaggrInst::BundleTagName = "disaggr";
const std::string AggregateInst::BundleTagName = "aggregate";
const std::string ArrayPartitionInst::BundleTagName = "xlx_array_partition";
const std::string ArrayReshapeInst::BundleTagName = "xlx_array_reshape";
const std::string StreamPragmaInst::BundleTagName = "xlx_reqd_pipe_depth";
const std::string StreamOfBlocksPragmaInst::BundleTagName = "xlx_reqd_sob_depth";
const std::string PipoPragmaInst::BundleTagName = "xcl_fpga_pipo_depth";
const std::string BindStoragePragmaInst::BundleTagName = "xlx_bind_storage";
const std::string BindOpPragmaInst::BundleTagName = "xlx_bind_op";
const std::string ConstSpecInst::BundleTagName = "const";
const std::string FPGAResourceLimitInst::BundleTagName = "fpga_resource_limit_hint";
const std::string XlxFunctionAllocationInst::BundleTagName = "xlx_function_allocation";
const std::string SAXIInst::BundleTagName = "xlx_s_axilite";
const std::string MaxiInst::BundleTagName = "xlx_m_axi";
const std::string AxiSInst::BundleTagName = "xlx_axis";
const std::string ApFifoInst::BundleTagName = "xlx_ap_fifo";
const std::string ApMemoryInst::BundleTagName = "xlx_ap_memory";
const std::string BRAMInst::BundleTagName = "xlx_bram";
const std::string ApStableInst::BundleTagName = "xlx_ap_stable";
const std::string ApNoneInst::BundleTagName = "xlx_ap_none";
const std::string ApAckInst::BundleTagName = "xlx_ap_ack";
const std::string ApVldInst::BundleTagName = "xlx_ap_vld";
const std::string ApOvldInst::BundleTagName = "xlx_ap_ovld";
const std::string ApHsInst::BundleTagName = "xlx_ap_hs";
const std::string ApCtrlNoneInst::BundleTagName = "xlx_ap_ctrl_none";
const std::string ApCtrlChainInst::BundleTagName = "xlx_ap_ctrl_chain";
const std::string ApCtrlHsInst::BundleTagName = "xlx_ap_ctrl_hs";
const std::string ResetPragmaInst::BundleTagName = "xlx_reset";
const std::string StreamLabelInst::BundleTagName = "stream_interface";
const std::string ShiftRegLabelInst::BundleTagName = "shift_reg_interface";
const std::string StreamOfBlocksLabelInst::BundleTagName = "stream_of_blocks_interface";
const std::string NPortChannelInst::BundleTagName = "nport_channel";
const std::string XlxIPInst::BundleTagName = "xlx_ip";
const std::string MaxiCacheInst::BundleTagName = "xlx_cache";

