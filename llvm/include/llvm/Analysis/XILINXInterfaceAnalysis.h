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
//===----------------------------------------------------------------------===//
//
// This file define the InterfaceAnalysis class. InterfaceAnalysis gets
// interface type for each objects
//
//===----------------------------------------------------------------------===//

#ifndef REFLOW_INTERFACE_ANALYSIS_H
#define REFLOW_INTERFACE_ANALYSIS_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/Optional.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/XILINXAggregateUtil.h"
#include "llvm/Pass.h"

namespace llvm {
class InterfaceAnalysis {
public:
  InterfaceAnalysis(Module &M) : M(M) {}
  ~InterfaceAnalysis() { ValueHwTyMap.clear(); }
  Optional<InterfaceInfo> getInterfaceInfo(Value *V);
  void getInterfaceInfo(Value *V, SmallVectorImpl<InterfaceInfo> &InfoList);
  void print(raw_ostream &OS) const;
  void print(raw_ostream &OS, Value *V,
             SmallVectorImpl<InterfaceInfo> &InfoList) const;

private:
  /// A CallbackVH to arrange for InterfaceAnalysis to be notified whenever a
  /// Value is deleted.
  class IACallbackVH final : public CallbackVH {
    InterfaceAnalysis *IA;

    void deleted() override;
    void allUsesReplacedWith(Value *New) override;

  public:
    IACallbackVH(Value *V, InterfaceAnalysis *IA = nullptr)
        : CallbackVH(V), IA(IA) {}
  };

  friend class IACallbackVH;
  void eraseValueFromMap(Value *V) {
    auto I = ValueHwTyMap.find_as(V);
    if (I != ValueHwTyMap.end())
      ValueHwTyMap.erase(V);
  }

private:
  Module &M;
  using ValueMapType = DenseMap<IACallbackVH, SmallVector<InterfaceInfo, 1>,
                                DenseMapInfo<Value *>>;
  /// This is a cache of the values we have analyzed so far.
  ValueMapType ValueHwTyMap;
};

class InterfaceAnalysisWrapperPass : public ModulePass {
  std::unique_ptr<InterfaceAnalysis> IA;

public:
  static char ID;

  InterfaceAnalysisWrapperPass() : ModulePass(ID) {
    initializeInterfaceAnalysisWrapperPassPass(
        *PassRegistry::getPassRegistry());
  }

  InterfaceAnalysis &getIA() { return *IA; }
  const InterfaceAnalysis &getIA() const { return *IA; }

  bool runOnModule(Module &M) override {
    IA.reset(new InterfaceAnalysis(M));
    return false;
  }
  void releaseMemory() override { IA.reset(); }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }
  void print(raw_ostream &OS, const Module * = nullptr) const override;
};

// merge interface info (collected from intrinsics) about one object
void mergeInterfaceInfoIntoList(InterfaceInfo From,
                                SmallVectorImpl<InterfaceInfo> &To);
// merge inferterface info about 2 or more objects
void mergeTwoInterfaceInfoLists(SmallVectorImpl<InterfaceInfo> &From,
                                SmallVectorImpl<InterfaceInfo> &To);
// get all related interface info.
void findInterfaceInfoOnTop(Value *V, SmallVectorImpl<InterfaceInfo> &InfoList);
Optional<InterfaceInfo> findInterfaceInfoOnTop(Value *V);
Optional<InterfaceInfo>
pickMainInterfaceInfo(SmallVectorImpl<InterfaceInfo> &InfoList);
// check if it's AXI-Stream with side channels
bool isAXISWithSideChannel(const InterfaceInfo &Info);
// check if it's array-to-stream
bool isArray2Stream(const InterfaceInfo &Info);
// check if it's array-to-stream
bool isArray2Stream(const SmallVectorImpl<InterfaceInfo> &IFInfoList);
// check if it's array-to-stream
bool isArray2Stream(Value *V, InterfaceAnalysis *IA);
// check if the interface is with AXI protocol
bool isAXIProtocolInterface(const InterfaceInfo &IFInfo);
// check if the interface is with AXI protocol
bool isAXIProtocolInterface(const SmallVectorImpl<InterfaceInfo> &IFInfoList);

} // namespace llvm

#endif // REFLOW_INTERFACE_ANALYSIS_H
