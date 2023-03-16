// (C) Copyright 2016-2022 Xilinx, Inc.
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
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/XILINXFPGAIntrinsicInst.h"
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

bool llvm::isSystemHLSHeaderFile(const std::string FileName) {
  if (FileName.empty())
    return false;

//#include "ap_headers.h"
  static const std::set<std::string> HLSHeaders({
    "AXI4_if.h",
    "algorithm.h",
    "ap_axi_sdata.h",
    "ap_cint.h",
    "ap_decl.h",
    "ap_common.h",
    "ap_fixed.h",
    "ap_fixed_base.h",
    "ap_fixed_ref.h",
    "ap_fixed_special.h",
    "ap_int.h",
    "ap_int_base.h",
    "ap_int_ref.h",
    "ap_int_special.h",
    "ap_mem_if.h",
    "ap_private.h",
    "ap_sc_core.h",
    "ap_sc_dt.h",
    "ap_sc_ext.h",
    "ap_sc_extras.h",
    "ap_shift_reg.h",
    "ap_stream.h",
    //"ap_systemc.h",
    "ap_tlm.h",
    "ap_utils.h",
    "autoesl_tech.h",
    "autopilot_apint.h",
    "autopilot_dt.h",
    //"autopilot_enum.h",
    "autopilot_ssdm_bits.h",
    "autopilot_ssdm_op.h",
    "c_ap_int_sim.h",
    "deque.h",
    "hls_bus_if.h",
    "hls_design.h",
    "hls_fpo.h",
    "hls_stream.h",
    "hls_stream_39.h",
    "hls_streamofblocks.h",
    "hls_util.h",
    "hls_task.h",
    "hls_burst_maxi.h",
    "hls_np_channel.h",
    "iterator.h",
    "list.h",
    "set.h",
    "stdafx.h",
    "string.h",
    //"systemc.h",
    "targetver.h",
    "tlm.h",
    "vector.h",
    "vhls_sim.h",
    //"systemc",
    "complex",
    "dsp48e1_builtins.h",
    "dsp48e2_builtins.h",
    "hls_cordic.h",
  });

  std::string NameWithoutPath = filename(FileName, sys::path::Style::posix);
  return HLSHeaders.find(NameWithoutPath) != HLSHeaders.end();
}

bool llvm::isSystemHLSHeaderFunc(const Function *F) {
  std::string FileName = getFuncSourceFileName(F);
  return isSystemHLSHeaderFile(FileName);
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
