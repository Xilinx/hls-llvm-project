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
// This file defines aggregate related APIs
//
//===----------------------------------------------------------------------===//

#ifndef AGGREGATE_INFO_H
#define AGGREGATE_INFO_H

#include "llvm/IR/XILINXFPGAIntrinsicInst.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Type.h"
#include <cstdint>

namespace llvm {

enum class InterfaceMode {
  Auto = 0, // Normal mode, not set
  None,     // None mode
  Stable,   // Stable mode
  Vld,      // Valid mode
  OVld,     // Output valid mode
  Ack,      // Acknowledge mode
  HS,       // Handshaking mode
  Fifo,     // Fifo mode
  MemFifo,  // Memory fifo mode, stream_of_blocks
  Memory,   // Memory mode
  Bram,     // Bram mode
  AXIS,     // AXI-stream mode
  MAXI,     // AXI master mode
  SAXILite, // AXI lite slave mode
  ModeSize
};

enum class ImplementType {
  None = 0,
  Array = 1 << 0,            // C builtin array
  HLSStream = 1 << 1,        // hls::stream
  StreamSideChannel = 1 << 2 // hls::stream with side-channel
};

struct InterfaceInfo {
  InterfaceMode IM;
  ImplementType IT;
  // related interface spec
  // it's located in top function, and should not deleted/invalidated. Otherwise
  // we need to use WeakVH instead
  Instruction *Spec; // can be InterfaceInst or ssdm_InterfaceSpec


  InterfaceInfo()
      : IM(InterfaceMode::Auto), IT(ImplementType::None), Spec(nullptr) {}
  InterfaceInfo(const InterfaceInfo &Info)
      : IM(Info.IM), IT(Info.IT), Spec(Info.Spec) {}
  InterfaceInfo(InterfaceMode Mode)
      : IM(Mode), IT(ImplementType::None), Spec(nullptr) {}
  InterfaceInfo(InterfaceMode Mode, Instruction *Spec)
      : IM(Mode), IT(ImplementType::None), Spec(Spec) {}
  InterfaceInfo(InterfaceMode Mode, ImplementType IT)
      : IM(Mode), IT(IT), Spec(nullptr) {}

  // useless interface info
  bool isNull() {
    return IM == InterfaceMode::Auto && IT == ImplementType::None && !Spec;
  }
  bool operator==(const InterfaceInfo &Info) {
    return this->IM == Info.IM;
  }
  bool operator!=(const InterfaceInfo &Info) {
    return !(this->IM == Info.IM);
  }
};

enum class AggregateType {
  NoSpec = 0,   // no aggregate pragma at all
  Default,      // aggregate pragma without compact option
  Bit,          // -compact bit, field bit alignment
  Byte,         // -compact byte, field byte alignment
  NoCompact     // -compact none, no padding removal
};

struct AggregateInfo {
  AggregateType AggrTy;
  uint64_t WordSize; // In bits

  AggregateInfo(AggregateType AggrTy = AggregateType::NoSpec,
                uint64_t WordSize = 0 /*0 means no spec*/)
      : AggrTy(AggrTy), WordSize(WordSize){};
  AggregateInfo(uint64_t WordSize /*0 means no spec*/,
                AggregateType AggrTy = AggregateType::NoSpec)
      : AggrTy(AggrTy), WordSize(WordSize){};
  AggregateType getAggregateType() { return AggrTy; }
  uint64_t getWordSize() { return WordSize; }
};

uint64_t getAggregatedBitwidthInBitLevel(const DataLayout &DL, Type *Ty);

uint64_t getAggregatedBitwidthInByteLevel(const DataLayout &DL,
                                          Type *Ty);

uint64_t getAggregatedBitwidth(const DataLayout &DL, Type *Ty,
                               AggregateType AggrTy);

uint64_t getAggregatedBitwidth(const DataLayout &DL, Type *Ty,
                               AggregateType AggrTy, InterfaceInfo IFInfo);

void getStructFieldOffsetSizePairsInBits(
    const DataLayout &DL, Type *Ty, uint64_t Start,
    SmallVectorImpl<std::pair<uint64_t, uint64_t>> &PairVec);

std::string getAggregateTypeStr(AggregateType AggrTy);

std::string getInterfaceModeStr(const InterfaceInfo &Info);
// for compatible with original aggregation/disaggregation msg dump
std::string getHwTypeStr(const InterfaceInfo &Info);


} // namespace llvm
#endif // !AGGREGATE_INFO_H
