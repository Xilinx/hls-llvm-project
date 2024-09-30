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
//===----------------------------------------------------------------------===//
//
// This file declares common functions useful for getting information of a
// XILINX HLS Loop.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_XILINXLOOPINFOUTILS_H
#define LLVM_ANALYSIS_XILINXLOOPINFOUTILS_H

#include "llvm/ADT/None.h"
#include "llvm/ADT/Optional.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"

namespace llvm {

class ReflowDiagnostic;

/// Returns true if \p L is a for loop.
bool isForLoop(const Loop *L);

/// Returns true if \p L is a rotated loop.
bool isRotatedLoop(const Loop *L);

/// Returns induction variable or auxiliary induction variable for Loop \p L.
PHINode *getIndVarOrAuxiliaryIndVar(const Loop *L, ScalarEvolution &SE);

struct LoopIndexInfoTy {
  PHINode *PN;
  Value *Upper;
  Value *Init;
  Value *ExitIdx;
  bool IdxZExt;
  CmpInst::Predicate Pred;
  int64_t Step;
};

/// Collect loop index info for the given loop \p L.
/// \returns true on success.
bool getLoopIndexInfo(Loop *L, bool ExitFromHeader, LoopIndexInfoTy &Info,
                      bool HandleXor = false);

/// Returns MDNode that contains the Metadata \p Attr for Loop \p L.
MDNode *getLoopMetadata(const Loop *L, StringRef Attr);

/// Returns true if Loop \p L has Metadata \P Attr.
bool hasLoopMetadata(const Loop *L, StringRef Attr);

/// \brief Captures loop trip count information with HLS loop_tripcount pragma.
class LoopTripCountMDInfo {
  uint64_t Min; // minimum number of loop iterations
  uint64_t Max; // maximum number of loop iterations
  uint64_t Avg; // average number of loop iterations
  std::string Source;
  DILocation *DL;
public:
  LoopTripCountMDInfo(uint64_t Min, uint64_t Max, uint64_t Avg, 
                      StringRef Source, DILocation *DL)
      : Min(Min), Max(Max), Avg(Avg), Source(Source), DL(DL) {}

  uint64_t getMin() const { return Min; }
  uint64_t getMax() const { return Max; }
  uint64_t getAvg() const { return Avg; }
  StringRef getSource() const { return Source; }
  DILocation *getDILocation() const { return DL; }
};

/// Get loop trip count from HLS loop_tripcount pragma.
Optional<LoopTripCountMDInfo> getLoopTripCount(const Loop *L);

/// Returns true if Loop \p L is a dataflow loop.
bool isDataFlow(const Loop *L);

/// Returns true if Loop \p L is a parallel loop.
bool isParallel(const Loop *L);

/// Returns true if Loop \p L is a pipeline loop.
bool isPipeline(const Loop *L);

/// Returns true if Loop \p L is a pipeline rewind loop.
bool isPipelineRewind(const Loop *L);

/// Returns true if Loop \p L is a loop that must not be pipelined.
bool isPipelineOff(const Loop *L);
/// Drop pipeline pragma if any
bool dropPipeline(Loop *L);

/// Get target II for pipeline Loop \p L. Retrun None if Loop \p L is not a
/// pipeline loop.
Optional<ConstantInt *> getPipelineII(const Loop *L);

/// Get target II for pipeline Loop \p L. Retrun 0 if Loop \p L is not a
/// pipeline loop.
int64_t getPipelineIIInt64(const Loop *L);

/// \brief Captures pipeline style.
enum class PipelineStyle {
  Default = -1,
  STP = 0, // stp: stalling pipeline.
  FLP = 1, // flp: flushable pipeline.
  FRP = 2  // frp: free run pipeline.
};

/// Returns pipeline style of pipeline Loop \p L.
Optional<PipelineStyle> getPipelineStyle(const Loop *L);

/// Returns true if Loop \p L is a loop that must not be rotated.
bool isRotateOff(const Loop *L);

/// Returns true if Loop \p L is a flatten loop.
bool isFlatten(const Loop *L);

/// Returns true if Loop \p L is a loop that must not be flattened.
bool isFlattenOff(const Loop *L);

/// Returns true if Loop \p L has unroll enable metadata.
bool hasUnrollEnableMetadata(const Loop *L);

/// Returns true if Loop \p L is marked as unroll full.
bool isFullyUnroll(const Loop *L);

/// Returns true if partial unroll Loop \p L is asked to skip exit check when
/// unrolling the loop.
bool isWithoutExitCheckUnroll(const Loop *L);

/// Returns true if Loop \p L is a loop that must not be unrolled.
bool isUnrollOff(const Loop *L);

/// Get unroll factor for unroll Loop \p L. Retrun None if Loop \p L doesn't
/// have unroll metadata attached or is marked as unroll full.
Optional<ConstantInt *> getUnrollFactor(const Loop *L);

/// Get unroll factor for unroll Loop \p L. Retrun 0 if Loop \p L doesn't
/// have unroll metadata attached or is marked as unroll full.
uint64_t getUnrollFactorUInt64(const Loop *L);

/// Returns true if the loop \p L may be fully unrolled.
/// \p LTC is the loop trip count of loop \p L.
bool mayFullyUnroll(const Loop *L, const SCEV *LTC);

/// Returns true if the loop \p L may be partially unrolled
bool hasLoopPartialUnroll(const Loop *L);

/// Returns true if the loop \p L may contain trip count pragma
bool hasLoopTripCount(const Loop *L);
 
/// Returns true if the do-while loop \p L may be exposed to the dataflow
/// region.
bool mayExposeInDataFlowRegion(ScalarEvolution &SE, const Loop *L);

/// Returns Loop \p L 's name.
Optional<const std::string> getLoopName(const Loop *L);

StringRef getLoopPragmaSource(StringRef pragma, const Loop *L);

DebugLoc getLoopPragmaLoc( StringRef pragma, const Loop *L );

DebugLoc getLoopFlattenPragmaLoc( const Loop *L );
DebugLoc getLoopTripCountPragmaLoc( const Loop* L);
DebugLoc getLoopPipelinePragmaLoc( const Loop *L );
DebugLoc getLoopUnrollPragmaLoc( const Loop *L );
DebugLoc getLoopDataflowPragmaLoc( const Loop *L );

MDNode *GetUnrollMetadata(MDNode *LoopID, StringRef Name);

struct ReflowUnrollOption {
  unsigned Count;  /// unroll factor
  unsigned TripCount; /// positive for constant tripcount, otherwise 0
  unsigned TripMultiple; /// TripCount % TripMultiple == 0 when TripCount != 0
  bool HandleFlatten;
};

LoopUnrollResult ReflowUnrollLoop(Loop *L, ReflowUnrollOption RUO, LoopInfo *LI,
                                  ScalarEvolution *SE, DominatorTree *DT,
                                  AssumptionCache *AC, bool PreserveLCSSA,
                                  bool AllowUnsafeClone = false);

BasicBlock *getExitingBlock(const Loop *L, ScalarEvolution *SE);
// Find trip count and trip multiple if count is not available
void ReflowCalculateTripCountAndMultiple(Loop *L, ScalarEvolution *SE,
                                         unsigned &TripCount,
                                         unsigned &TripMultiple);

ReflowUnrollOption populateUnrollOption(Loop *L, bool WithoutCheck,
                                        unsigned Count, unsigned TripCount,
                                        unsigned TripMultiple,
                                        ScalarEvolution *SE);

Optional<int> getFlattenCheckerEncode(Loop *L);
} // end namespace llvm

#endif // LLVM_ANALYSIS_XILINXLOOPINFOUTILS_H
