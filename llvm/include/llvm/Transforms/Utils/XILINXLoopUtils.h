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
// This file declares common functions for manipulating XILINX HLS Loop.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_UTILS_XILINXLOOPINFOUTILS_H
#define LLVM_TRANSFORMS_UTILS_XILINXLOOPINFOUTILS_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/XILINXLoopInfoUtils.h"

namespace llvm {

/// Add metadata \p Attr + \p Option to Loop \p L.
void addLoopMetadata(Loop *L, StringRef Attr,
                     ArrayRef<Metadata *> Options = None);

/// Add loop trip count metadata to Loop \p L.
/// \p Min: minimum number of loop iterations
/// \p Max: maximum number of loop iterations
/// \p Avg: average number of loop iterations
void addLoopTripCount(Loop *L, uint32_t Min, uint32_t Max, uint32_t Avg, 
                      StringRef Source = "", DILocation *DL = nullptr);

/// Add dataflow metadata to Loop \p L.
void addDataFlow(Loop *L, StringRef Source = "");

/// Add pipeline metadata to Loop \p L.
void addPipeline(Loop *L, int32_t II = -1, bool IsRewind = false,
                 PipelineStyle Style = PipelineStyle::Default, StringRef Source = "", DILocation *Loc = nullptr);

/// Add pipeline off(force not to pipeline) metadata to Loop \p L.
void addPipelineOff(Loop *L, StringRef Source = "");

/// Add unroll full metadata to Loop \p L.
void addFullyUnroll(Loop *L, StringRef Source = "", DILocation *Loc = nullptr);

/// Add partial unroll with \p Factor metadata to Loop \p L.
void addPartialUnroll(Loop *L, uint32_t Factor, bool SkipExitCheck, StringRef Source = "", DILocation *Loc = nullptr);

/// Add unroll off(force not to unroll) metadata to Loop \p L.
void addUnrollOff(Loop *L, StringRef Source = "");

/// Add flatten metadata to Loop \p L.
void addFlatten(Loop *L, StringRef Source = "infer-from-design", DILocation *Loc = nullptr);

/// Add flatten off(force not to flatten) metadata to Loop \p L.
void addFlattenOff(Loop *L, StringRef Source = "infer-from-design", DILocation *Loc = nullptr);

} // end namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_XILINXLOOPINFOUTILS_H
