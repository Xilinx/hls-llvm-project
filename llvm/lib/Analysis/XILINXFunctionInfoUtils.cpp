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

#include "llvm/Analysis/XILINXFunctionInfoUtils.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/XILINXFPGAIntrinsicInst.h"
#include "llvm/Support/XILINXSystemInfo.h"
#include <cassert>
#include <set>

using namespace llvm;

bool llvm::hasFunctionInstantiate(const Function *F) {
  if(!F)
    return false;

  for(const Instruction &I : instructions(F)) {
    if(const PragmaInst *PI = dyn_cast<PragmaInst>(&I)) {
      if(isa<FuncInstantiateInst>(PI)) 
        return true;
    }
  } 
  
  return false;
}

bool llvm::isDataFlow(const Function *F) {
  return F->hasFnAttribute("fpga.dataflow.func");
}

/// \brief Captures function pipeline information.
class PipelineInfo {
  long long II;        // target II
  PipelineStyle Style; // pipeline style

public:
  PipelineInfo(long long II, PipelineStyle Style) : II(II), Style(Style) {}

  long long getII() const { return II; }
  PipelineStyle getStyle() const { return Style; }
};

/// Get function \p F pipeline inforamtion: II, Style.
static Optional<PipelineInfo> GetPipelineInfo(const Function *F) {
  if (!F->hasFnAttribute("fpga.static.pipeline"))
    return None;

  auto P = F->getFnAttribute("fpga.static.pipeline");
  std::pair<StringRef, StringRef> PipeLineInfoStr =
      P.getValueAsString().split(".");
  long long II;
  // https://reviews.llvm.org/D24778 indicates "getAsSignedInteger" returns true
  // for failure, false for success because of convention.
  if (getAsSignedInteger(PipeLineInfoStr.first, 10, II))
    return None;

  long long StyleCode;
  if (getAsSignedInteger(PipeLineInfoStr.second, 10, StyleCode))
    return None;

  assert((StyleCode >= -1) && (StyleCode <= 2) && "unexpected pipeline style!");
  PipelineStyle Style = static_cast<PipelineStyle>(StyleCode);
  return PipelineInfo(II, Style);
}

bool llvm::isPipeline(const Function *F) {
  Optional<PipelineInfo> PInfo = GetPipelineInfo(F);
  if (!PInfo.hasValue())
    return false;

  return PInfo.getValue().getII();
}

Optional<PipelineStyle> llvm::getPipelineStyle(const Function *F) {
  Optional<PipelineInfo> PInfo = GetPipelineInfo(F);
  if (!PInfo.hasValue())
    return None;

  return PInfo.getValue().getStyle();
}

bool llvm::dropPipeline(Function *F) {
  if (F->hasFnAttribute("fpga.static.pipeline")) {
    F->removeFnAttr("fpga.static.pipeline");
    return true;
  }

  return false;
}

bool llvm::isPipelineOff(const Function *F) {
  Optional<PipelineInfo> PInfo = GetPipelineInfo(F);
  if (!PInfo.hasValue())
    return false;

  return (PInfo.getValue().getII() == 0);
}

Optional<long long> llvm::getPipelineII(const Function *F) {
  Optional<PipelineInfo> PInfo = GetPipelineInfo(F);
  if (!PInfo.hasValue())
    return None;

  auto II = PInfo.getValue().getII();
  if (II == 0)
    return None;

  return II;
}

/// NOTE: #pragma HLS inline marks function with Attribute::AlwaysInline
bool llvm::isAlwaysInline(const Function *F) {
  return F->hasFnAttribute(Attribute::AlwaysInline);
}

/// NOTE: #pragma HLS inline marks function with Attribute::AlwaysInline
bool llvm::isAlwaysInline(const CallSite CS) {
  return CS.hasFnAttr(Attribute::AlwaysInline);
}

/// NOTE: #pragma HLS inline off marks function with Attribute::NoInline
bool llvm::isNoInline(const Function *F) {
  return F->hasFnAttribute(Attribute::NoInline);
}

/// NOTE: #pragma HLS inline off marks function with Attribute::NoInline
bool llvm::isNoInline(const CallSite CS) {
  return CS.hasFnAttr(Attribute::NoInline);
}

bool llvm::isTop(const Function *F) {
  return F->hasFnAttribute("fpga.top.func");
}

bool llvm::hasFunctionLatency(const Function *F) {
  return(F && F->hasFnAttribute("fpga.latency"));
}

bool llvm::hasFunctionProtocol(const Function *F) {
  return F && F->hasFnAttribute("fpga.protocol");
}

bool llvm::hasFunctionExpressBalance(const Function *F) {
  return F && F->hasFnAttribute("fpga.exprbalance.func");
}

bool llvm::hasFunctionLoopMerge(const Function *F) {
  return F && F->hasFnAttribute("fpga.mergeloop");
}

bool llvm::hasFunctionOccurrence(const Function *F) {
  return F && F->hasFnAttribute("fpga.occurrence");
}

Optional<const std::string> llvm::getTopFunctionName(const Function *F) {
  if (!isTop(F))
    return None;

  auto TFName = F->getFnAttribute("fpga.top.func").getValueAsString();
  if (!TFName.empty())
    return TFName.str();

  return None;
}

bool llvm::HasVivadoIP(const Function *F) {
  for (const_inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    if (isa<XlxIPInst>(&*I)) {
      return true;
    }
  }
  
  return false;
}

std::string llvm::getFuncSourceFileName(const Function *F) {
  DISubprogram *SP = F->getSubprogram();
  if (!SP) return "";
  return SP->getFilename();
}


bool llvm::isSystemHLSHeaderFunc(const Function *F) {
  std::string FileName = getFuncSourceFileName(F);
  return XilinxSystemInfo::isSystemHLSHeaderFile(FileName);
}

// Judge if HLS intrinsic "llvm.fpga.any()"
bool llvm::isHlsFpgaAnyIntrinsic(const Value *V) {
  if (!V)
    return false;
  if (auto *II = dyn_cast<IntrinsicInst>(V)) {
    switch (II->getIntrinsicID()) {
      case Intrinsic::fpga_any:
        return true;
      default:
        return false;
    }
  }
  return false;
}

MDTuple *llvm::getFuncPragmaInfo(Function *F, StringRef PragmaName) {
  Metadata *M = F->getMetadata("fpga.function.pragma");
  if (!M) {
    return nullptr;
  }
  assert(isa<MDTuple>(M) && "unexpected Metadata type from clang codegen");
  for (auto &Op : cast<MDTuple>(M)->operands()) {
    MDTuple *OnePragma = cast<MDTuple>(Op.get());
    Metadata *Name = OnePragma->getOperand(0).get();
    assert(isa<MDString>(Name) && "unexpected MDType");
    if (cast<MDString>(Name)->getString().equals(PragmaName)) {
      return OnePragma;
    }
  }
  return nullptr;
}

StringRef llvm::getFuncPragmaSource(Function *F, StringRef PragmaName) {
  auto *OnePragma = getFuncPragmaInfo(F, PragmaName);
  return getPragmaSourceFromMDNode(OnePragma);
}

DebugLoc llvm::getFuncPragmaLoc(Function *F, StringRef PragmaName) {
  auto *OnePragma = getFuncPragmaInfo(F, PragmaName);
  if (!OnePragma) {
    return DebugLoc();
  }
  Metadata *Loc =
      OnePragma->getOperand(OnePragma->getNumOperands() - 1).get();
  // If function is mared with 'nodebug' attr, then debugloc will be missing
  if (!Loc)
    return DebugLoc();
  assert(isa<DILocation>(Loc) && "unexpected MDType");
  return DebugLoc(cast<DILocation>(Loc));
}

/// If the given instruction references a specific memory location, fill in
/// Loc with the details, otherwise set Loc.Ptr to null.
///
/// Returns a ModRefInfo value describing the general behavior of the
/// instruction.
/// copied from MemoryDependenceAnalysis.cpp
static ModRefInfo GetLocation(const Instruction *Inst, MemoryLocation &Loc,
                              const TargetLibraryInfo &TLI) {
  if (const LoadInst *LI = dyn_cast<LoadInst>(Inst)) {
    if (LI->isUnordered()) {
      Loc = MemoryLocation::get(LI);
      return ModRefInfo::Ref;
    }
    if (LI->getOrdering() == AtomicOrdering::Monotonic) {
      Loc = MemoryLocation::get(LI);
      return ModRefInfo::ModRef;
    }
    Loc = MemoryLocation();
    return ModRefInfo::ModRef;
  }

  if (const StoreInst *SI = dyn_cast<StoreInst>(Inst)) {
    if (SI->isUnordered()) {
      Loc = MemoryLocation::get(SI);
      return ModRefInfo::Mod;
    }
    if (SI->getOrdering() == AtomicOrdering::Monotonic) {
      Loc = MemoryLocation::get(SI);
      return ModRefInfo::ModRef;
    }
    Loc = MemoryLocation();
    return ModRefInfo::ModRef;
  }

  if (const VAArgInst *V = dyn_cast<VAArgInst>(Inst)) {
    Loc = MemoryLocation::get(V);
    return ModRefInfo::ModRef;
  }

  if (const CallInst *CI = isFreeCall(Inst, &TLI)) {
    // calls to free() deallocate the entire structure
    Loc = MemoryLocation(CI->getArgOperand(0));
    return ModRefInfo::Mod;
  }

  if (const IntrinsicInst *II = dyn_cast<IntrinsicInst>(Inst)) {
    switch (II->getIntrinsicID()) {
    case Intrinsic::lifetime_start:
    case Intrinsic::lifetime_end:
    case Intrinsic::invariant_start:
      Loc = MemoryLocation::getForArgument(II, 1, TLI);
      // These intrinsics don't really modify the memory, but returning Mod
      // will allow them to be handled conservatively.
      return ModRefInfo::Mod;
    case Intrinsic::invariant_end:
      Loc = MemoryLocation::getForArgument(II, 2, TLI);
      // These intrinsics don't really modify the memory, but returning Mod
      // will allow them to be handled conservatively.
      return ModRefInfo::Mod;
    default:
      break;
    }
  }

  // Otherwise, just do the coarse-grained thing that always works.
  if (Inst->mayWriteToMemory())
    return ModRefInfo::ModRef;

  // mayReadFromMemory is conservative for function call, port upstream fix
  auto MayReadMemory = [](const Instruction *I) -> bool {
    if (const CallInst *CI = dyn_cast<CallInst>(I))
      return !CI->doesNotReadMemory();
    return I->mayReadFromMemory();
  };

  if (MayReadMemory(Inst))
    return ModRefInfo::Ref;
  return ModRefInfo::NoModRef;
}

MemDepResult llvm::getDependency(Instruction *QueryInst, Instruction *ScanPos,
                                 MemoryDependenceResults *MDR,
                                 AliasAnalysis *AA, TargetLibraryInfo *TLI) {

  BasicBlock *BB = ScanPos->getParent();
  assert(QueryInst->mayReadOrWriteMemory() &&
         "Instruction should be memory instruction");

  MemoryLocation MemLoc;
  ModRefInfo MR = GetLocation(QueryInst, MemLoc, *TLI);
  if (MemLoc.Ptr) {

    bool isLoad = !isModSet(MR);
    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(QueryInst))
      isLoad |= II->getIntrinsicID() == Intrinsic::lifetime_start;

    return MDR->getPointerDependencyFrom(MemLoc, isLoad, ScanPos->getIterator(),
                                         BB, QueryInst);
  } else if (isa<CallInst>(QueryInst) || isa<InvokeInst>(QueryInst)) {
    CallSite QueryCS(QueryInst);
    bool isReadOnly = AA->onlyReadsMemory(QueryCS);
    return MDR->getCallSiteDependencyFrom(QueryCS, isReadOnly,
                                          ScanPos->getIterator(), BB);
  }

  return MemDepResult::getUnknown();
}
