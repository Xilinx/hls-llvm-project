/*
Copyright (C) 2023, Advanced Micro Devices, Inc.
SPDX-License-Identifier: X11

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of Advanced Micro Devices
shall not be used in advertising or otherwise to promote the sale,
use or other dealings in this Software without prior written authorization
from Advanced Micro Devices, Inc.
*/

#include "llvm/Support/XILINXSystemInfo.h"
bool XilinxSystemInfo::isSystemHLSHeaderFile(const std::string FileName) {
  if (FileName.empty())
    return true;

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
    "cmath",
    "hls_math.h",
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

  std::string NameWithoutPath = llvm::sys::path::filename(FileName, llvm::sys::path::Style::posix);
  return HLSHeaders.find(NameWithoutPath) != HLSHeaders.end();
}
