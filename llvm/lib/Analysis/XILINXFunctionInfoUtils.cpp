// (C) Copyright 2016-2021 Xilinx, Inc.
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
#include <cassert>
#include <set>

using namespace llvm;

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

bool llvm::isIPCore(const Function *F) {
  if (!F->hasFnAttribute("fpga.resource.hint"))
    return false;
  auto AVal = F->getFnAttribute("fpga.resource.hint").getValueAsString();
  auto P = AVal.split('.');
  if (P.first == "vivado_ip")
    return true;
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
  });

  std::string NameWithoutPath = filename(FileName, sys::path::Style::posix);
  return HLSHeaders.find(NameWithoutPath) != HLSHeaders.end();
}

bool llvm::isSystemHLSHeaderFunc(const Function *F) {
  std::string FileName = getFuncSourceFileName(F);
  return isSystemHLSHeaderFile(FileName);
}

