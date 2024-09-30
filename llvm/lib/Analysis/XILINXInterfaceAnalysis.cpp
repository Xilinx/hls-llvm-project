// (C) Copyright 2016-2022 Xilinx, Inc.
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc.
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

#include "llvm/Analysis/XILINXInterfaceAnalysis.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/XILINXAggregateUtil.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "reflow-interface-analysis"

using namespace llvm;

static cl::opt<unsigned> MaxLookup("reflow-lookup-threshold", cl::init(64),
                                   cl::Hidden,
                                   cl::desc("Threshold for value tracing"));

void InterfaceAnalysis::IACallbackVH::deleted() {

  DEBUG(dbgs() << "Remove value from InterfaceAnalysis cache as the value "
                  "is deleted:\n  "
               << *getValPtr() << "\n";);
  IA->eraseValueFromMap(getValPtr());
}
void InterfaceAnalysis::IACallbackVH::allUsesReplacedWith(Value *New) {
  DEBUG(dbgs() << "Remove value from InterfaceAnalysis cache as the value "
                  "is RAUWed:\n  "
               << *getValPtr() << "\n";);
  IA->eraseValueFromMap(getValPtr());
}

void InterfaceAnalysisWrapperPass::print(raw_ostream &OS,
                                         const Module *) const {
  IA->print(OS);
}
// get interface info. form intrinsics
InterfaceInfo getInterfaceInfoFromIntrinsic(Use &U, IntrinsicInst *II) {
  if (auto *PI = dyn_cast<PragmaInst>(II)) {
    if (PI->getVariable() != U.get())
      return InterfaceInfo();
    // FIXME: more sideeffect intrinsic
    if (isa<StreamPragmaInst>(PI))
      return InterfaceInfo(InterfaceMode::Fifo);
    else if (isa<PipoPragmaInst>(PI))
      return InterfaceInfo(InterfaceMode::Bram);
  } else if (isa<FPGAFIFOInst>(II)) {
    if (cast<FPGAFIFOInst>(II)->getFIFOOperand() == U.get())
      return InterfaceInfo(InterfaceMode::Fifo, ImplementType::HLSStream);
  } else if (isa<AXISIntrinsicInst>(II)) {
    if (U.getOperandNo() < AXISIntrinsicInst::NumChannels)
      return InterfaceInfo(InterfaceMode::AXIS,
                           ImplementType::StreamSideChannel);
  } else if (isa<FPGAPIPOInst>(II)) {
    if (cast<FPGAPIPOInst>(II)->getPIPOOperand() == U.get())
      return InterfaceInfo(InterfaceMode::MemFifo);
  }

  return InterfaceInfo();
}
// get corresponding function argument for value at callsite
static void getFunctionArgs(CallInst *CI, Value *V,
                            SetVector<Argument *> &ArgSet) {
  CallSite CS(CI);
  auto *F = CS.getCalledFunction();
  if (!F)
    return;
  size_t ArgNo = 0;
  for (auto &A : CS.args()) {
    if (A.get() == V)
      ArgSet.insert(F->arg_begin() + ArgNo);
    ArgNo++;
  }
}

static void findInterfaceInfoFromIntrinsic(
    Value *V, SmallVectorImpl<InterfaceInfo> &InfoList,
    SmallPtrSetImpl<Value *> &Visited, bool &Returned, unsigned Depth = 0) {
  // avoid stack overflow
  if (MaxLookup != 0 && ++Depth > MaxLookup)
    return;

  if (!Visited.insert(V).second)
    return;
  // For constant, infer as Auto
  if (isa<ConstantData>(V))
    return;

  for (auto &U : V->uses()) {
    if (auto *CI = dyn_cast<CallInst>(U.getUser())) {
      if (auto *II = dyn_cast<IntrinsicInst>(CI)) {
        if (II->getFunction()->hasFnAttribute("fpga.wrapper.func"))
          continue;
        mergeInterfaceInfoIntoList(getInterfaceInfoFromIntrinsic(U, II),
                                   InfoList);
      } else {
        auto *F = CI->getCalledFunction();
        if (!F || F->isDeclaration() || F->isVarArg() ||
            F->hasFnAttribute("fpga.wrapper.func"))
          continue;
        // get corresponding function argument NO.
        SetVector<Argument *> ArgSet;
        getFunctionArgs(CI, V, ArgSet);
        for (auto *Arg : ArgSet) {
          bool LocalReturned = false;
          findInterfaceInfoFromIntrinsic(Arg, InfoList, Visited, LocalReturned, Depth);
          if (LocalReturned)
            findInterfaceInfoFromIntrinsic(CI, InfoList, Visited, Returned, Depth);
        }
      }
    } else if (auto *GEP = dyn_cast<GEPOperator>(U.getUser())) {
      findInterfaceInfoFromIntrinsic(GEP, InfoList, Visited, Returned, Depth);
    } else if (auto *BC = dyn_cast<BitCastOperator>(U.getUser())) {
      findInterfaceInfoFromIntrinsic(BC, InfoList, Visited, Returned, Depth);
    } else if (isa<ReturnInst>(U.getUser())) {
      Returned = true;
    }
  }
}
static uint64_t getArgAttributeValue(const Argument *Arg, StringRef AttrName) {
  AttributeList Attrs = Arg->getParent()->getAttributes();
  auto ArgIdx = Arg->getArgNo();
  const auto &Attr = Attrs.getParamAttr(ArgIdx, AttrName);
  auto Str = Attr.getValueAsString();
  if (Str.empty())
    return 0;

  unsigned Size;
  if (to_integer(Str, Size))
    return Size;

  return 0;
}

uint64_t getDecayedDimSize(const Argument *Arg) {
  return getArgAttributeValue(Arg, "fpga.decayed.dim.hint");
}
//
static bool isArrayType(Value *V) {
  if (!V->getType()->isPointerTy())
    return false;
  if (V->getType()->getPointerElementType()->isArrayTy() ||
      (isa<Argument>(V) && getDecayedDimSize(cast<Argument>(V))))
    return true;
  if (isa<CallInst>(V) &&
      cast<CallInst>(V)->getCalledFunction()->getName() == "malloc") {
    for (auto *U : V->users())
      if (isa<BitCastOperator>(U) &&
          U->getType()->getPointerElementType()->isArrayTy())
        return true;
  }
  return false;
}
// only do when there's no other interface info,
static void
getInterfaceInfoFromValueType(Value *V,
                              SmallVectorImpl<InterfaceInfo> &InfoList) {
  if (isArrayType(V)) {
    InfoList.push_back(InterfaceInfo(InterfaceMode::Memory));
    return;
  }
  InfoList.push_back(InterfaceInfo(InterfaceMode::Auto));
  return;
}

static void GetRealUnderlyingObjects(Value *V, const DataLayout &DL,
                                     SmallPtrSetImpl<Value *> &Visited,
                                     SetVector<Value *> &Set) {
  auto P = Visited.insert(V);
  if (!P.second)
    return;

  if (!V->getType()->isPointerTy()) {
    Set.insert(V);
    return;
  }

  SmallVector<Value *, 2> Objs;
  GetUnderlyingObjects(V, Objs, DL, nullptr, 0);

  for (auto *Obj : Objs) {
    if (isa<GlobalVariable>(Obj) || isa<AllocaInst>(Obj) ||
        isa<MallocInst>(Obj)) {
      Set.insert(Obj);
    } else if (auto *Arg = dyn_cast<Argument>(Obj)) {
      auto *F = Arg->getParent();
      if (F->use_empty()) { // Arrive wrapper function
        Set.insert(Arg);
        continue;
      }

      for (auto *U : F->users()) {
        // Skip when the function is attached in directive scope intrinsic.
        // Commonly see with allocation pragmas.
        if (isa<PragmaInst>(U) || isa<ScopeEntry>(U))
          continue;

        if (auto *CI = dyn_cast<CallInst>(U))
          GetRealUnderlyingObjects(CI->getArgOperand(Arg->getArgNo()), DL,
                                   Visited, Set);
      }
    }
  }

  return;
}
// get corresponding top argument if \v V is from cosim wrapper.
static Argument *getTopArgument(Value *V) {
  for (auto *U : V->users()) {
    if (auto *CI = dyn_cast<CallInst>(U)) {
      // get corresponding function argument NO.
      SetVector<Argument *> ArgSet;
      getFunctionArgs(CI, V, ArgSet);
      auto *F = CI->getCalledFunction();
      if (!F)
        continue;
      if (F->hasFnAttribute("fpga.top.func"))
        return *ArgSet.begin();
    } else if (auto *GEP = dyn_cast<GEPOperator>(U)) {
      if (auto *A = getTopArgument(GEP))
        return A;
    } else if (auto *BC = dyn_cast<BitCastOperator>(U)) {
      if (auto *A = getTopArgument(BC))
        return A;
    }
  }
  return nullptr;
}
// get corresponding top argument if \v V is from cosim wrapper.
static Argument *getTheTopArgument(Value *V) {
  auto *I = dyn_cast<Instruction>(V);
  if (!I)
    return nullptr;
  auto *F = I->getFunction();
  if (!F->hasFnAttribute("fpga.wrapper.func"))
    return nullptr;
  return getTopArgument(I);
}
// get all related interface info.
void InterfaceAnalysis::getInterfaceInfo(
    Value *V, SmallVectorImpl<InterfaceInfo> &InfoList) {
  SmallPtrSet<Value *, 5> Visited;
  SetVector<Value *> Set;
  GetRealUnderlyingObjects(V, M.getDataLayout(), Visited, Set);
  for (auto *Obj : Set) {
    auto I = ValueHwTyMap.find_as(Obj);
    if (I != ValueHwTyMap.end()) {
      mergeTwoInterfaceInfoLists(I->second, InfoList);
      continue;
    } else {
      SmallVector<InterfaceInfo, 1> OneInfoList;
      if (isa<Argument>(Obj) &&
          (cast<Argument>(Obj)->getParent()->hasFnAttribute("fpga.top.func") ||
           cast<Argument>(Obj)->getParent()->use_empty()))
        findInterfaceInfoOnTop(Obj, OneInfoList);
      else if (isa<Instruction>(Obj) &&
               cast<Instruction>(Obj)->getFunction()->hasFnAttribute(
                   "fpga.wrapper.func"))
        findInterfaceInfoOnTop(getTheTopArgument(Obj), OneInfoList);

      SmallPtrSet<Value *, 5> Visited;
      bool Returned = false;
      findInterfaceInfoFromIntrinsic(Obj, OneInfoList, Visited, Returned);
      // s_axilite is only a wrapper, so the object under it can be other
      // interface type.
      if (OneInfoList.size() == 0 ||
          (OneInfoList.size() == 1 &&
           OneInfoList[0].IM == InterfaceMode::SAXILite))
        getInterfaceInfoFromValueType(Obj, OneInfoList);
      mergeTwoInterfaceInfoLists(OneInfoList, InfoList);
      // cache the search result.
      // only cache real underlying object, bec. after duplication non-real
      // underlying object info might be invalidated.
      auto Pair = ValueHwTyMap.insert({IACallbackVH(Obj, this), OneInfoList});
      assert(Pair.second && "Already in the map?!");
    }
  }
  if (InfoList.size() == 0)
    InfoList.push_back(InterfaceInfo());
}
// return interface info. when there's only one interface setting
Optional<InterfaceInfo> InterfaceAnalysis::getInterfaceInfo(Value *V) {
  SmallVector<InterfaceInfo, 1> InfoList;
  getInterfaceInfo(V, InfoList);
  return pickMainInterfaceInfo(InfoList);
}

void InterfaceAnalysis::print(raw_ostream &OS, Value *V,
                              SmallVectorImpl<InterfaceInfo> &InfoList) const {
  OS << *V << "\n";
  OS << "  -->  ";
  for (auto &Info : InfoList)
    OS << getInterfaceModeStr(Info) << ", ";
  OS << "\n";
  OS << "  -->  main: "
     << getInterfaceModeStr(pickMainInterfaceInfo(InfoList).getValue()) << "\n";
}

static Function *getTopFunction(const Module &M) {
  for (auto &F : M) {
    if (F.hasFnAttribute("fpga.top.func"))
      return const_cast<Function *>(&F);
    // OpenCL case
    if (F.getCallingConv() == CallingConv::SPIR_KERNEL)
      return const_cast<Function *>(&F);
  }

  // In case no top function attr;
  for (auto &F : M)
    if (F.use_empty() && !F.isDeclaration())
      return const_cast<Function *>(&F);

  return nullptr;
}

void InterfaceAnalysis::print(raw_ostream &OS) const {
  auto &IA = *const_cast<InterfaceAnalysis *>(this);
  // print top function argument
  OS << "Print Interface Analysis result for top function arguments:\n";
  auto *Top = getTopFunction(M);
  for (auto &A : Top->args()) {
    SmallVector<InterfaceInfo, 1> IFInfoList;
    IA.getInterfaceInfo(&A, IFInfoList);
    print(OS, &A, IFInfoList);
  }
  // print internal object
  OS << "Print Interface Analysis result for internal objects:\n";
  for (auto &F : M.functions()) {
    // skip cosim wrapper functions
    if (F.isDeclaration() || F.hasFnAttribute("fpga.wrapper.func"))
      continue;
    for (auto &I : F.getEntryBlock())
      if (isa<AllocaInst>(&I)) {
        SmallVector<InterfaceInfo, 1> IFInfoList;
        IA.getInterfaceInfo(&I, IFInfoList);
        print(OS, &I, IFInfoList);
      }
  }
}

INITIALIZE_PASS_BEGIN(InterfaceAnalysisWrapperPass, DEBUG_TYPE,
                      "Interface Analysis", false, true)
INITIALIZE_PASS_END(InterfaceAnalysisWrapperPass, DEBUG_TYPE,
                    "Interface Analysis", false, true)

char InterfaceAnalysisWrapperPass::ID = 0;

//=========------ public APIs ------========

// merge interface info (collected from intrinsics) about one object
void llvm::mergeInterfaceInfoIntoList(InterfaceInfo From,
                                      SmallVectorImpl<InterfaceInfo> &To) {
  if (From.isNull())
    return;
  // ignore FIFO if there's already AXIS in the list
  if (From.IM == InterfaceMode::Fifo) {
    auto It = std::find_if(To.begin(), To.end(), [](InterfaceInfo &Info) {
      return Info.IM == InterfaceMode::AXIS;
    });
    if (It != To.end()) {
      if (It->IT == ImplementType::None && From.IT != ImplementType::None)
        It->IT = From.IT;
      return;
    }
  } else if (From.IM == InterfaceMode::AXIS) {
    auto It = std::find_if(To.begin(), To.end(), [](InterfaceInfo &Info) {
      return Info.IM == InterfaceMode::Fifo;
    });
    if (It != To.end()) {
      if (It->IT != ImplementType::None && From.IT == ImplementType::None)
        From.IT = It->IT;
      To.erase(It);
      To.push_back(From);
      return;
    }
  }

  auto It = std::find(To.begin(), To.end(), From);
  if (It == To.end())
    To.push_back(From);
  else {
    if (It->IT == ImplementType::None && From.IT != ImplementType::None)
      It->IT = From.IT;
    if (!It->Spec && From.Spec)
      It->Spec = From.Spec;
  }
}
// merge inferterface info about 2 or more objects
void llvm::mergeTwoInterfaceInfoLists(SmallVectorImpl<InterfaceInfo> &From,
                                      SmallVectorImpl<InterfaceInfo> &To) {
  for (auto &Info : From)
    if (std::find(To.begin(), To.end(), Info) == To.end())
      To.push_back(Info);
}

static bool isSsdmInterfaceSpec(User *U) {
  return isa<CallInst>(U) && cast<CallInst>(U)->getCalledFunction() &&
         cast<CallInst>(U)->getCalledFunction()->getName() ==
             "_ssdm_op_SpecInterface";
}

static InterfaceInfo getInterfaceInfoFromInterfaceSpec(CallInst *SpecI) {
  auto Op1 = SpecI->getOperand(1);
  if (!isa<GlobalVariable>(Op1) || !cast<GlobalVariable>(Op1)->hasInitializer())
    return InterfaceInfo(InterfaceMode::Auto);
  auto *CDA =
      dyn_cast<ConstantDataArray>(cast<GlobalVariable>(Op1)->getInitializer());
  if (!CDA)
    return InterfaceInfo(InterfaceMode::Auto);
  auto IMStr = CDA->getAsCString();
  return StringSwitch<InterfaceInfo>(IMStr)
      .CaseLower("ap_auto", InterfaceInfo(InterfaceMode::Auto, SpecI))
      .CaseLower("ap_hs", InterfaceInfo(InterfaceMode::HS, SpecI))
      .CaseLower("ap_ovld", InterfaceInfo(InterfaceMode::OVld, SpecI))
      .CaseLower("ap_none", InterfaceInfo(InterfaceMode::None, SpecI))
      .CaseLower("ap_vld", InterfaceInfo(InterfaceMode::Vld, SpecI))
      .CaseLower("ap_stable", InterfaceInfo(InterfaceMode::Stable, SpecI))
      .CaseLower("ap_memory", InterfaceInfo(InterfaceMode::Memory, SpecI))
      .CaseLower("ap_fifo", InterfaceInfo(InterfaceMode::Fifo, SpecI))
      .CaseLower("bram", InterfaceInfo(InterfaceMode::Bram, SpecI))
      .CaseLower("axis", InterfaceInfo(InterfaceMode::AXIS, SpecI))
      .CaseLower("m_axi", InterfaceInfo(InterfaceMode::MAXI, SpecI))
      .CaseLower("s_axilite", InterfaceInfo(InterfaceMode::SAXILite, SpecI))
      .Default(InterfaceInfo(InterfaceMode::Auto));
}
// get all related interface info.
void llvm::findInterfaceInfoOnTop(Value *V,
                                  SmallVectorImpl<InterfaceInfo> &InfoList) {
  if (!V)
    return;
  for (auto *U : V->users()) {
    if (auto *CI = dyn_cast<InterfaceInst>(U)) {
      if (isa<ApNoneInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::None, CI),
                                   InfoList);
      else if (isa<ApStableInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::Stable, CI),
                                   InfoList);
      else if (isa<ApVldInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::Vld, CI),
                                   InfoList);
      else if (isa<ApOvldInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::OVld, CI),
                                   InfoList);
      else if (isa<ApAckInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::Ack, CI),
                                   InfoList);
      else if (isa<ApHsInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::HS, CI),
                                   InfoList);
      else if (isa<ApFifoInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::Fifo, CI),
                                   InfoList);
      else if (isa<ApMemoryInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::Memory, CI),
                                   InfoList);
      else if (isa<BRAMInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::Bram, CI),
                                   InfoList);
      else if (isa<AxiSInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::AXIS, CI),
                                   InfoList);
      else if (isa<SAXIInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::SAXILite, CI),
                                   InfoList);
      else if (isa<MaxiInst>(CI))
        mergeInterfaceInfoIntoList(InterfaceInfo(InterfaceMode::MAXI, CI),
                                   InfoList);
    } else if (isSsdmInterfaceSpec(U)) {
      mergeInterfaceInfoIntoList(
          getInterfaceInfoFromInterfaceSpec(cast<CallInst>(U)), InfoList);
    } else if (auto *GEP = dyn_cast<GEPOperator>(U)) {
      findInterfaceInfoOnTop(GEP, InfoList);
    } else if (auto *BC = dyn_cast<BitCastOperator>(U)) {
      findInterfaceInfoOnTop(BC, InfoList);
    }
  }
}

Optional<InterfaceInfo>
llvm::pickMainInterfaceInfo(SmallVectorImpl<InterfaceInfo> &InfoList) {
  // return MAXI when there's MAXI interface type in the list, because there
  // might be 3 or more interface types applied on the same object. For e.g.,
  // MAXI with SAXILite and ap_none (later two are actually applied on the
  // offset)
  for (auto IFInfo : InfoList)
    if (IFInfo.IM == InterfaceMode::MAXI)
      return IFInfo;
  // if there're two, and the other one is s_axilite, then return the main one.
  if (InfoList.size() == 2) {
    if (InfoList[0].IM == InterfaceMode::SAXILite)
      return InfoList[1];
    else if (InfoList[1].IM == InterfaceMode::SAXILite)
      return InfoList[0];
  }
  if (InfoList.size() > 0)
    return InfoList[0];
  return None;
}
// return interface info. when there's only one interface setting
Optional<InterfaceInfo> llvm::findInterfaceInfoOnTop(Value *V) {
  if (!V)
    return None;
  SmallVector<InterfaceInfo, 1> InfoList;
  findInterfaceInfoOnTop(V, InfoList);
  return pickMainInterfaceInfo(InfoList);
}
// check if it's array-to-stream
bool llvm::isAXISWithSideChannel(const InterfaceInfo &Info) {
  return Info.IM == InterfaceMode::AXIS &&
         Info.IT == ImplementType::StreamSideChannel;
}
// check if it's array-to-stream
bool llvm::isArray2Stream(const InterfaceInfo &Info) {
  return ((Info.IM == InterfaceMode::AXIS || Info.IM == InterfaceMode::Fifo) &&
          Info.IT != ImplementType::HLSStream &&
          Info.IT != ImplementType::StreamSideChannel);
}
// check if it's array-to-stream
bool llvm::isArray2Stream(const SmallVectorImpl<InterfaceInfo> &IFInfoList) {
  return std::any_of(
      IFInfoList.begin(), IFInfoList.end(),
      [](const InterfaceInfo &IFInfo) { return isArray2Stream(IFInfo); });
}
// check if it's array-to-stream
bool llvm::isArray2Stream(Value *V, InterfaceAnalysis *IA) {
  SmallVector<InterfaceInfo, 1> InfoList;
  IA->getInterfaceInfo(V, InfoList);
  return isArray2Stream(InfoList);
}
// check if the interface is with AXI protocol
bool llvm::isAXIProtocolInterface(const InterfaceInfo &IFInfo) {
  return IFInfo.IM == InterfaceMode::MAXI || IFInfo.IM == InterfaceMode::AXIS ||
         IFInfo.IM == InterfaceMode::SAXILite;
}
// check if the interface is with AXI protocol
bool llvm::isAXIProtocolInterface(
    const SmallVectorImpl<InterfaceInfo> &IFInfoList) {
  return std::any_of(IFInfoList.begin(), IFInfoList.end(),
                     [](const InterfaceInfo &IFInfo) {
                       return isAXIProtocolInterface(IFInfo);
                     });
}
