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
#include "llvm/Transforms/Utils/XILINXLoopUtils.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"

using namespace llvm;

static LLVMContext &GetContext(const Loop *L) {
  return L->getHeader()->getContext();
}

static bool IsUnrollLoopMetadata(StringRef Attr) {
  return Attr.equals("llvm.loop.unroll.full") ||
         Attr.equals("llvm.loop.unroll.withoutcheck") ||
         Attr.equals("llvm.loop.unroll.count") ||
         Attr.equals("llvm.loop.unroll.disable");
}

/// NOTE: If there is already \p Attr existed for Loop \p L, drops it and
///       appends with the new one.
void llvm::addLoopMetadata(Loop *L, StringRef Attr,
                           ArrayRef<Metadata *> Options) {
  bool IsUnrollLoopMD = IsUnrollLoopMetadata(Attr);

  // Reserve first location for self reference to the LoopID metadata node.
  SmallVector<Metadata *, 4> MDs(1);

  // Check if there's already LoopID.
  if (MDNode *LoopID = L->getLoopID())
    for (unsigned i = 1, ie = LoopID->getNumOperands(); i < ie; ++i) {
      MDNode *MD = cast<MDNode>(LoopID->getOperand(i));

      // Already have given Attr? Drop it.
      if (MD->getNumOperands() > 0) {
        if (const MDString *S = dyn_cast<MDString>(MD->getOperand(0)))
          if ((S->getString() == Attr) ||
              (IsUnrollLoopMD && IsUnrollLoopMetadata(S->getString())))
            continue;
      }

      MDs.push_back(LoopID->getOperand(i));
    }

  // Appends the Attr + Options and creates new loop id.
  LLVMContext &Context = GetContext(L);
  SmallVector<Metadata *, 8> Vec;
  Vec.push_back(MDString::get(Context, Attr));
  if (!Options.empty())
    Vec.append(Options.begin(), Options.end());
  MDs.push_back(MDNode::get(Context, Vec));
  MDNode *NewLoopID = MDNode::get(Context, MDs);

  // Set operand 0 to refer to the loop id itself.
  NewLoopID->replaceOperandWith(0, NewLoopID);
  L->setLoopID(NewLoopID);
}

/// NOTE: If there is already \p Attr existed for Loop \p L, drops it 
void llvm::removeLoopMetadata(Loop *L, StringRef Attr) {
  bool IsUnrollLoopMD = IsUnrollLoopMetadata(Attr);

  // Reserve first location for self reference to the LoopID metadata node.
  SmallVector<Metadata *, 4> MDs(1);

  // Check if there's already LoopID.
  if (MDNode *LoopID = L->getLoopID())
    for (unsigned i = 1, ie = LoopID->getNumOperands(); i < ie; ++i) {
      MDNode *MD = cast<MDNode>(LoopID->getOperand(i));

      // Already have given Attr? Drop it.
      if (MD->getNumOperands() > 0) {
        if (const MDString *S = dyn_cast<MDString>(MD->getOperand(0)))
          if (S->getString() == Attr)
            continue;
      }

      MDs.push_back(LoopID->getOperand(i));
    }

  // Appends the Attr + Options and creates new loop id.
  LLVMContext &Context = GetContext(L);
  MDNode *NewLoopID = MDNode::get(Context, MDs);

  // Set operand 0 to refer to the loop id itself.
  NewLoopID->replaceOperandWith(0, NewLoopID);
  L->setLoopID(NewLoopID);
}

void llvm::addLoopTripCount(Loop *L, uint32_t Min, uint32_t Max, uint32_t Avg, 
                            StringRef Source, DILocation *DL) {
  Type *Int32Ty = Type::getInt32Ty(GetContext(L));
  SmallVector<Metadata *, 4> MD(
      {ConstantAsMetadata::get(ConstantInt::get(Int32Ty, Min)),
       ConstantAsMetadata::get(ConstantInt::get(Int32Ty, Max)),
       ConstantAsMetadata::get(ConstantInt::get(Int32Ty, Avg))});
  if (Source != "") 
    MD.push_back(MDString::get(GetContext(L), Source));
  
  if (DL)
    MD.push_back(DL);

  addLoopMetadata(L, "llvm.loop.tripcount", MD);
}

void llvm::addDataFlow(Loop *L, StringRef Source) {
  SmallVector<Metadata *, 1> MD;
  if (Source != "") {
    MD.push_back(MDString::get(GetContext(L), Source));
  }
  addLoopMetadata(L, "llvm.loop.dataflow.enable", MD);
}

/// II = -1 : default "II" value
/// II = 0  : force no pipeline. Query with isPipelineOff instead.
/// II > 0  : customized "II" value
void llvm::addPipeline(Loop *L, int32_t II, bool IsRewind,
                       PipelineStyle Style, StringRef Source, DILocation *Loc) {
  LLVMContext &Context = GetContext(L);
  SmallVector<Metadata*, 4> MDs{ConstantAsMetadata::get(
                       ConstantInt::getSigned(Type::getInt32Ty(Context), II)),
                   ConstantAsMetadata::get(
                       ConstantInt::get(Type::getInt1Ty(Context), IsRewind)),
                   ConstantAsMetadata::get(ConstantInt::get(
                       Type::getInt8Ty(Context), static_cast<int>(Style)))};
  if (Source != "") {
    MDs.push_back(MDString::get(GetContext(L), Source));
  }
  if (Loc)
    MDs.push_back(Loc);
  addLoopMetadata(L, "llvm.loop.pipeline.enable", MDs);
}

void llvm::addPipelineOff(Loop *L, StringRef Source) {
  addPipeline(L, /* No pipeline */ 0, false, PipelineStyle::Default, Source);
}

void llvm::removePipeline(Loop *L) {
  removeLoopMetadata(L, "llvm.loop.pipeline.enable");
}

void llvm::addFullyUnroll(Loop *L, StringRef Source, DILocation *Loc) {
  SmallVector<Metadata *, 2> MD;
  if (Source != "") {
    MD.push_back(MDString::get(GetContext(L), Source));
  }
  if (Loc) {
    MD.push_back(Loc);
  }
  addLoopMetadata(L, "llvm.loop.unroll.full", MD);
}

void llvm::addPartialUnroll(Loop *L, uint32_t Factor, bool SkipExitCheck, StringRef Source, DILocation *Loc) {
  SmallVector<Metadata *, 3> MD{ConstantAsMetadata::get(
             ConstantInt::get(Type::getInt32Ty(GetContext(L)), Factor))};
  if (Source != "") {
    MD.push_back(MDString::get(GetContext(L), Source));
  }
  if (Loc)
    MD.push_back(Loc);
  if (SkipExitCheck) {
    addLoopMetadata(L, "llvm.loop.unroll.withoutcheck", MD);
    return;
  }
  addLoopMetadata(L, "llvm.loop.unroll.count", MD);
}

void llvm::addUnrollOff(Loop *L, StringRef Source) {
  SmallVector<Metadata *, 1> MD;
  if (Source != "") {
    MD.push_back(MDString::get(GetContext(L), Source));
  }
  addLoopMetadata(L, "llvm.loop.unroll.disable", MD);
}

void llvm::addFlatten(Loop *L, StringRef Source, DILocation *Loc) {
  SmallVector<Metadata *, 3> MD{ConstantAsMetadata::get(
                      ConstantInt::get(Type::getInt1Ty(GetContext(L)), 1)),
                      MDString::get(GetContext(L), Source)};
  if (Loc)
    MD.push_back(Loc);
  addLoopMetadata(L, "llvm.loop.flatten.enable", MD);
}

void llvm::addFlattenOff(Loop *L, StringRef Source, DILocation *Loc) {
  SmallVector<Metadata *, 3> MD{ConstantAsMetadata::get(
                      ConstantInt::get(Type::getInt1Ty(GetContext(L)), 0 /*No Flatten*/)),
                      MDString::get(GetContext(L), Source)};
  if (Loc)
    MD.push_back(Loc);
  addLoopMetadata(L, "llvm.loop.flatten.enable", MD);
}

void llvm::addFlattenCheckerEncode(Loop *L, int Encode, int BitNum) {
  SmallVector<Metadata *, 1> MD{ConstantAsMetadata::get(
      ConstantInt::get(Type::getIntNTy(GetContext(L), BitNum), Encode))};

  addLoopMetadata(L, "llvm.loop.flatten.checker", MD);
}
