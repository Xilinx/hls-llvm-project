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
// This file defines aggregate related APIs
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Type.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/XILINXAggregateUtil.h"
#include "llvm/Support/MathExtras.h"

namespace llvm {

uint64_t getAggregatedBitwidthInBitLevel(const DataLayout &DL, Type *Ty) {
  if (Ty->isArrayTy())
    return Ty->getArrayNumElements() *
           getAggregatedBitwidthInBitLevel(DL, Ty->getArrayElementType());
  else if (Ty->isStructTy()) {
    uint64_t BW = 0;
    for (unsigned i = 0; i < Ty->getStructNumElements(); i++) {
      BW += getAggregatedBitwidthInBitLevel(DL, Ty->getStructElementType(i));
    }
    return BW;
    // Maybe VectorTy?
  } else
    return DL.getTypeSizeInBits(Ty);
}

uint64_t getAggregatedBitwidthInByteLevel(const DataLayout &DL, Type *Ty) {
  if (Ty->isArrayTy())
    return Ty->getArrayNumElements() *
           getAggregatedBitwidthInByteLevel(DL, Ty->getArrayElementType());
  else if (Ty->isStructTy()) {
    uint64_t BW = 0;
    for (unsigned i = 0; i < Ty->getStructNumElements(); i++) {
      BW += getAggregatedBitwidthInByteLevel(DL, Ty->getStructElementType(i));
    }
    return BW;
    // Maybe VectorTy?
  } else
    return alignTo(DL.getTypeSizeInBits(Ty), 8);
}

uint64_t getAggregatedBitwidth(const DataLayout &DL, Type *Ty,
                               AggregateType AggrTy) {
  switch (AggrTy) {
  case AggregateType::Bit:
    return getAggregatedBitwidthInBitLevel(DL, Ty);
  case AggregateType::Byte:
    return getAggregatedBitwidthInByteLevel(DL, Ty);
  case AggregateType::NoCompact:
    return DL.getTypeSizeInBits(Ty);
  default:
    llvm_unreachable("Get aggregate bitwidth for NoSpec?!");
  }
  return 0;
}

// For AXIS 'bit' aggregation, need to align the final bitwidth to 8-bits
uint64_t getAggregatedBitwidth(const DataLayout &DL, Type *Ty,
                               AggregateType AggrTy, InterfaceInfo IFInfo) {
  auto BW = getAggregatedBitwidth(DL, Ty, AggrTy);
  if (IFInfo.IM == InterfaceMode::AXIS && AggrTy == AggregateType::Bit)
    BW = alignTo(BW, 8);
  return BW;
}

void getStructFieldOffsetSizePairsInBits(
    const DataLayout &DL, Type *Ty, uint64_t Start,
    SmallVectorImpl<std::pair<uint64_t, uint64_t>> &PairVec) {
  if (Ty->isStructTy()) {
    auto *SL = DL.getStructLayout(cast<StructType>(Ty));
    for (unsigned i = 0; i < Ty->getStructNumElements(); i++)
      getStructFieldOffsetSizePairsInBits(DL, Ty->getStructElementType(i),
                                          Start + SL->getElementOffsetInBits(i),
                                          PairVec);
  } else if (Ty->isArrayTy()) {
    auto *EleTy = Ty->getArrayElementType();
    auto EleSize = DL.getTypeSizeInBits(EleTy);
    for (unsigned i = 0; i < Ty->getArrayNumElements(); i++)
      getStructFieldOffsetSizePairsInBits(DL, EleTy, Start + EleSize * i,
                                          PairVec);
  } else { // arrive at leaf
    PairVec.push_back(std::make_pair(Start, DL.getTypeSizeInBits(Ty)));
  }
}

std::string getAggregateTypeStr(AggregateType AggrTy) {
  switch(AggrTy) {
  case AggregateType::Default:
    return "default";
  case AggregateType::Bit:
    return "compact=bit";
  case AggregateType::Byte:
    return "compact=byte";
  case AggregateType::NoCompact:
    return "compact=none";
  default:
    return "default";
  }
  return "default";
}

std::string getInterfaceModeStr(const InterfaceInfo &Info) {
  std::string InfoStr;
  switch (Info.IM) {
  case InterfaceMode::Auto:
    InfoStr = "auto";
    break;
  case InterfaceMode::None:
    InfoStr = "none";
    break;
  case InterfaceMode::Stable:
    InfoStr = "stable";
    break;
  case InterfaceMode::Vld:
    InfoStr = "vld";
    break;
  case InterfaceMode::OVld:
    InfoStr = "ovld";
    break;
  case InterfaceMode::Ack:
    InfoStr = "ack";
    break;
  case InterfaceMode::HS:
    InfoStr = "hs";
    break;
  case InterfaceMode::Fifo:
    InfoStr = "fifo";
    break;
  case InterfaceMode::MemFifo:
    InfoStr = "memfifo";
    break;
  case InterfaceMode::Memory:
    InfoStr = "ap_memory";
    break;
  case InterfaceMode::Bram:
    InfoStr = "bram";
    break;
  case InterfaceMode::AXIS:
    InfoStr = "axis";
    break;
  case InterfaceMode::MAXI:
    InfoStr = "maxi";
    break;
  case InterfaceMode::SAXILite:
    InfoStr = "s_axilite";
    break;
  default:
    llvm_unreachable("other interface mode?!");
  }
  switch (Info.IT) {
  case ImplementType::Array:
    InfoStr += ": array";
    break;
  case ImplementType::HLSStream:
    InfoStr += ": hls::stream";
    break;
  case ImplementType::StreamSideChannel:
    InfoStr += ": hls::stream with side-channels";
    break;
  default:
    break;
  }
  return InfoStr;
}

std::string getHwTypeStr(const InterfaceInfo &Info) {
  std::string InfoStr;
  switch (Info.IM) {
  case InterfaceMode::Auto:
  case InterfaceMode::None:
  case InterfaceMode::Stable:
  case InterfaceMode::Vld:
  case InterfaceMode::OVld:
  case InterfaceMode::Ack:
  case InterfaceMode::HS:
  case InterfaceMode::SAXILite:
    InfoStr = "scalar";
    break;
  case InterfaceMode::Fifo:
    if (Info.IT == ImplementType::HLSStream)
      InfoStr = "fifo (hls::stream)";
    else
      InfoStr = "fifo (array-to-stream)";
    break;
  case InterfaceMode::MemFifo:
    InfoStr = "stream-of-blocks";
    break;
  case InterfaceMode::Memory:
  case InterfaceMode::Bram:
    InfoStr = "bram";
    break;
  case InterfaceMode::AXIS:
    if (Info.IT == ImplementType::HLSStream)
      InfoStr = "axis (hls::stream)";
    else if (Info.IT == ImplementType::StreamSideChannel)
      InfoStr = "axis (with side-channel)";
    else
      InfoStr = "axis (array-to-stream)";
    break;
  case InterfaceMode::MAXI:
    InfoStr = "maxi";
    break;
  default:
    llvm_unreachable("other interface mode?!");
  }
  return InfoStr;
}

} // namespace llvm
