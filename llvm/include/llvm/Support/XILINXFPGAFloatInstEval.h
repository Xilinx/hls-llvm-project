// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
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
// This file defines functions to evaluate FPGA floating point intrinsics.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_XILINXFPGAFLOATINSTEVAL_H
#define LLVM_SUPPORT_XILINXFPGAFLOATINSTEVAL_H

#include "llvm/ADT/APInt.h"

namespace fpga {
llvm::APInt EvalFloatAdd(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                         int ExpWidth);
llvm::APInt EvalFloatSub(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                         int ExpWidth);
llvm::APInt EvalFloatMul(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                         int ExpWidth);
llvm::APInt EvalFloatDiv(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                         int ExpWidth);
llvm::APInt EvalFloatFMA(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                         const llvm::APInt &Add, int ExpWidth);
llvm::APInt EvalFloatSqrt(const llvm::APInt &Val, int ExpWidth);

llvm::APInt EvalFloatFromFixed(const llvm::APInt &Val, int FixedExp,
                               int ExpWidth, int DestWidth);
llvm::APInt EvalFloatToFixed(const llvm::APInt &Val, int ExpWidth,
                             int FixedExp, int DestWidth);
llvm::APInt EvalFloatToFloat(const llvm::APInt &Val, int SrcExpWidth,
                             int DestExpWidth, int DestWidth);

bool EvalFloatCompareEQ(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                               int ExpWidth);
bool EvalFloatCompareLT(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                               int ExpWidth);
bool EvalFloatCompareLE(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                               int ExpWidth);
bool EvalFloatCompareNE(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                               int ExpWidth);
bool EvalFloatCompareUO(const llvm::APInt &Lhs, const llvm::APInt &Rhs,
                               int ExpWidth);
}

#endif
