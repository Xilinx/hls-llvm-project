//===---- CGLoopInfo.cpp - LLVM CodeGen for loop metadata -*- C++ -*-------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// And has the following additional copyright:
//
// (C) Copyright 2016-2022 Xilinx, Inc.
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc.
// All Rights Reserved.
//
//===----------------------------------------------------------------------===//

#include "CGLoopInfo.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/Basic/HLSDiagnostic.h"
#include "clang/Sema/LoopHint.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "CodeGenFunction.h"
#include "CGValue.h"
using namespace clang::CodeGen;
using namespace llvm;

extern StringRef getPragmaContext(const clang::Attr* A) ;

static MDNode *createMetadata(LLVMContext &Ctx, const LoopAttributes &Attrs,
                              const llvm::DebugLoc &StartLoc,
                              const llvm::DebugLoc &EndLoc) {

  if (!Attrs.IsParallel && Attrs.VectorizeWidth == 0 &&
      Attrs.InterleaveCount == 0 && Attrs.UnrollCount == 0 &&
      Attrs.UnrollWithoutCheck == -1 &&
      Attrs.VectorizeEnable == LoopAttributes::Unspecified &&
      Attrs.UnrollEnable == LoopAttributes::Unspecified &&
      Attrs.DistributeEnable == LoopAttributes::Unspecified &&
      Attrs.FlattenEnable == LoopAttributes::Unspecified &&
      !Attrs.PipelineII.hasValue() && 
      Attrs.TripCount.empty() &&
      Attrs.MinMax.empty() && Attrs.LoopName.empty() && !Attrs.IsDataflow &&
      !StartLoc && !EndLoc)
    return nullptr;

  SmallVector<Metadata *, 4> Args;
  // Reserve operand 0 for loop id self reference.
  auto TempNode = MDNode::getTemporary(Ctx, None);
  Args.push_back(TempNode.get());

  // If we have a valid start debug location for the loop, add it.
  if (StartLoc) {
    Args.push_back(StartLoc.getAsMDNode());

    // If we also have a valid end debug location for the loop, add it.
    if (EndLoc)
      Args.push_back(EndLoc.getAsMDNode());
  }

  // Setting vectorize.width
  if (Attrs.VectorizeWidth > 0) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.vectorize.width"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.VectorizeWidth))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // Setting interleave.count
  if (Attrs.InterleaveCount > 0) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.interleave.count"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.InterleaveCount))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // Setting interleave.count
  if (Attrs.UnrollCount > 0) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.unroll.count"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.UnrollCount)),
                        MDString::get(Ctx, Attrs.UnrollPragmaContext),
                        Attrs.UnrollPragmaLoc.getAsMDNode()};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // UnrollNoCheckCount = 0 means full unroll
  if (Attrs.UnrollWithoutCheck != -1) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.unroll.withoutcheck"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.UnrollWithoutCheck)),
                        MDString::get(Ctx, Attrs.UnrollPragmaContext), 
                        Attrs.UnrollPragmaLoc.getAsMDNode()};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // Setting vectorize.enable
  if (Attrs.VectorizeEnable != LoopAttributes::Unspecified) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.vectorize.enable"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt1Ty(Ctx), (Attrs.VectorizeEnable ==
                                                   LoopAttributes::Enable)))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // Setting unroll.full or unroll.disable
  if (Attrs.UnrollEnable != LoopAttributes::Unspecified) {
    std::string Name;
    if (Attrs.UnrollEnable == LoopAttributes::Enable)
      Name = "llvm.loop.unroll.enable";
    else if (Attrs.UnrollEnable == LoopAttributes::Full)
      Name = "llvm.loop.unroll.full";
    else
      Name = "llvm.loop.unroll.disable";
    Metadata *Vals[] = {MDString::get(Ctx, Name), MDString::get(Ctx, Attrs.UnrollPragmaContext), Attrs.UnrollPragmaLoc.getAsMDNode()};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  if (Attrs.PipelineII) {
    Metadata *Vals[] = {
        MDString::get(Ctx, "llvm.loop.pipeline.enable"),
        ConstantAsMetadata::get(ConstantInt::get(Type::getInt32Ty(Ctx),
                                                 Attrs.PipelineII.getValue())),
        ConstantAsMetadata::get(
            ConstantInt::get(Type::getInt8Ty(Ctx), Attrs.Rewind)),
        ConstantAsMetadata::get(
            ConstantInt::get(Type::getInt8Ty(Ctx), Attrs.PipelineStyle)), 
        MDString::get(Ctx, Attrs.PipelinePragmaContext),
        Attrs.PipelinePragmaLoc.getAsMDNode()};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  if (!Attrs.LoopName.empty()) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.name"),
                        MDString::get(Ctx, Attrs.LoopName)};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  if (Attrs.DistributeEnable != LoopAttributes::Unspecified) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.distribute.enable"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt1Ty(Ctx), (Attrs.DistributeEnable ==
                                                   LoopAttributes::Enable)))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  if (Attrs.FlattenEnable != LoopAttributes::Unspecified) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.flatten.enable"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt1Ty(Ctx),
                            (Attrs.FlattenEnable == LoopAttributes::Enable))),
                        MDString::get(Ctx, Attrs.FlattenPragmaContext),
                        Attrs.FlattenPragmaLoc.getAsMDNode()};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  if (Attrs.MinMax.size() == 2) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.latency"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.MinMax[0])),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.MinMax[1]))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  if (Attrs.TripCount.size() == 3) {
    Metadata *Vals[] = {
        MDString::get(Ctx, "llvm.loop.tripcount"),
        ConstantAsMetadata::get(
            ConstantInt::get(Type::getInt32Ty(Ctx), Attrs.TripCount[0])),
        ConstantAsMetadata::get(
            ConstantInt::get(Type::getInt32Ty(Ctx), Attrs.TripCount[1])),
        ConstantAsMetadata::get(
            ConstantInt::get(Type::getInt32Ty(Ctx), Attrs.TripCount[2])),
        MDString::get(Ctx, Attrs.TripCountPragmaContext),
        Attrs.TripCountPragmaLoc.getAsMDNode()
    };
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  if (Attrs.IsDataflow) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.dataflow.enable"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt1Ty(Ctx), Attrs.DisableDFPropagation)),
                        MDString::get(Ctx, Attrs.DataflowPragmaContext),
                        Attrs.DataflowPragmaLoc.getAsMDNode()};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // Set the first operand to itself.
  MDNode *LoopID = MDNode::get(Ctx, Args);
  LoopID->replaceOperandWith(0, LoopID);
  return LoopID;
}

LoopAttributes::LoopAttributes(bool IsParallel)
    : IsParallel(IsParallel), VectorizeEnable(LoopAttributes::Unspecified),
      UnrollEnable(LoopAttributes::Unspecified), VectorizeWidth(0),
      InterleaveCount(0), UnrollCount(0), UnrollWithoutCheck(-1),
      DistributeEnable(LoopAttributes::Unspecified),
      FlattenEnable(LoopAttributes::Unspecified), Rewind(0), PipelineStyle(-1),
      IsDataflow(false), DisableDFPropagation(true) {}

void LoopAttributes::clear() {
  IsParallel = false;
  IsDataflow = false;
  DisableDFPropagation = true;
  VectorizeWidth = 0;
  InterleaveCount = 0;
  UnrollCount = 0;
  UnrollWithoutCheck = -1;
  VectorizeEnable = LoopAttributes::Unspecified;
  UnrollEnable = LoopAttributes::Unspecified;
  DistributeEnable = LoopAttributes::Unspecified;
  FlattenEnable = LoopAttributes::Unspecified;
  PipelineII.reset();
  PipelineStyle = -1;
  Rewind = 0;
  TripCount.clear();
  MinMax.clear();
  LoopName = "";
}

LoopInfo::LoopInfo(BasicBlock *Header, const LoopAttributes &Attrs,
                   const llvm::DebugLoc &StartLoc, const llvm::DebugLoc &EndLoc)
    : LoopID(nullptr), Header(Header), Attrs(Attrs) {
  LoopID = createMetadata(Header->getContext(), Attrs, StartLoc, EndLoc);
}

void LoopInfoStack::push(BasicBlock *Header, const llvm::DebugLoc &StartLoc,
                         const llvm::DebugLoc &EndLoc) {
  Active.push_back(LoopInfo(Header, StagedAttrs, StartLoc, EndLoc));
  // Clear the attributes so nested loops do not inherit them.
  StagedAttrs.clear();
}

//FIXME, following code is very ugly, TODO, trim it 
void LoopInfoStack::push(CodeGenFunction* CGF, BasicBlock *Header, clang::ASTContext &Ctx,
                         ArrayRef<const clang::Attr *> Attrs,
                         const llvm::DebugLoc &StartLoc,
                         const llvm::DebugLoc &EndLoc) {

  // Identify loop hint attributes from Attrs.
  for (const auto *Attr : Attrs) {
    if (auto *N = dyn_cast<XCLRegionNameAttr>(Attr)) {
      setLoopName(N->getName());
      continue;
    }

    bool hls_ifcond = CGF->EvaluateHLSIFCond(Attr->getHLSIfCond()); 

    if (!hls_ifcond )
      continue; 

    if (isa<XlxPipelineAttr>(Attr)) {
      setRewind(cast<XlxPipelineAttr>(Attr)->getRewind());
    }

    const LoopHintAttr *LH = dyn_cast<LoopHintAttr>(Attr);
    const OpenCLUnrollHintAttr *OpenCLHint =
        dyn_cast<OpenCLUnrollHintAttr>(Attr);

    const XlxUnrollHintAttr *XlxUnrollHint = 
        dyn_cast<XlxUnrollHintAttr>(Attr);

    //now, XCLPipelineLoop is different with XlxlPipeline
    //XCLPipelineLoopAttr is used by OpenCL kernel 
    //while, XlxPipelineAttr is for HLS kernel 
    const XCLPipelineLoopAttr *XCLPipeline =
        dyn_cast<XCLPipelineLoopAttr>(Attr);

    const XlxPipelineAttr *XlxPipeline = 
        dyn_cast<XlxPipelineAttr>(Attr);

    const XCLFlattenLoopAttr *XCLFlatten = dyn_cast<XCLFlattenLoopAttr>(Attr);
    const XlxFlattenLoopAttr *XlxFlatten = dyn_cast<XlxFlattenLoopAttr>(Attr); 

    const XCLLoopTripCountAttr *XCLTripCount =
        dyn_cast<XCLLoopTripCountAttr>(Attr);

    const XlxLoopTripCountAttr *XlxTripCount = 
        dyn_cast<XlxLoopTripCountAttr>(Attr);

    const XCLDataFlowAttr *XCLDataflow = dyn_cast<XCLDataFlowAttr>(Attr);

    const XCLLatencyAttr *XCLLatency = dyn_cast<XCLLatencyAttr>(Attr);

    const XlxDependenceAttr* XLXDependence = 
        dyn_cast<XlxDependenceAttr>( Attr );

    // Skip non loop hint attributes
    if (!LH && !OpenCLHint && !XlxUnrollHint && !XCLPipeline && !XlxPipeline && 
        !XCLFlatten && !XlxFlatten && !XCLTripCount && 
        !XlxTripCount && !XCLDataflow && !XCLLatency && !XLXDependence) {
      continue;
    }

    LoopHintAttr::OptionType Option = LoopHintAttr::Unroll;
    LoopHintAttr::LoopHintState State = LoopHintAttr::Disable;
    unsigned ValueInt = 1;
    int PipelineInt = 0;
    int PipelineStyleInt = -1;
    bool DisableDFPropagation = false;
    SmallVector<int, 3> TripInt = {0, 0, 0};
    SmallVector<int, 2> MinMaxInt = {0, 0};
    // Translate opencl_unroll_hint attribute argument to
    // equivalent LoopHintAttr enums.
    // OpenCL v2.0 s6.11.5:  
    // 0 - full unroll (no argument).
    // 1 - disable unroll.
    // other positive integer n - unroll by n.
    if (OpenCLHint) {
      ValueInt = CGF->HLSEvaluateICE(OpenCLHint->getUnrollHint(), "factor", "unroll",
                                 /*Default*/ 0);
      bool ExitCheck = OpenCLHint->getSkipExitCheck();
      if (!ExitCheck) {
        if (ValueInt == 0) {
          State = LoopHintAttr::Full;
        } else if (ValueInt != 1) {
          Option = LoopHintAttr::UnrollCount;
          State = LoopHintAttr::Numeric;
        }
      } else {
        if (ValueInt != 1) {
          Option = LoopHintAttr::UnrollWithoutCheck;
          State = LoopHintAttr::Numeric;
        }
      }
      setUnrollDebugLoc(CGF->PragmaSourceLocToDebugLoc(Attr->getLocation()));
      setUnrollPragmaContext(getPragmaContext(Attr)); 
    } 
    else if (XlxUnrollHint) { 
      ValueInt = CGF->HLSEvaluateICE(XlxUnrollHint->getFactor(), "factor", "unroll", 
                                 /*Default*/ 0);
      bool ExitCheck = XlxUnrollHint->getSkipExitCheck();
      if (!ExitCheck) {
        if (ValueInt == 0) {
          State = LoopHintAttr::Full;
        } else if (ValueInt != 1) {
          Option = LoopHintAttr::UnrollCount;
          State = LoopHintAttr::Numeric;
        }
      } else {
        if (ValueInt == 0) {
          State = LoopHintAttr::Full;
        } else if (ValueInt != 1) {
          Option = LoopHintAttr::UnrollWithoutCheck;
          State = LoopHintAttr::Numeric;
        }
      }
      setUnrollDebugLoc(CGF->PragmaSourceLocToDebugLoc(Attr->getLocation()));
      setUnrollPragmaContext(getPragmaContext(Attr)); 
    }
    else if (XlxPipeline) { 
      PipelineInt = CGF->HLSEvaluateICE(XlxPipeline->getII(), "II", "pipeline", /*Default*/ -1);
      PipelineStyleInt = XlxPipeline->getStyle();
      Option = LoopHintAttr::Pipeline;
      State = LoopHintAttr::Numeric;
      setPipelineDebugLoc(CGF->PragmaSourceLocToDebugLoc(Attr->getLocation()));
      setPipelinePragmaContext(getPragmaContext(Attr)); 
    } else if (XCLPipeline) {
      PipelineInt = CGF->HLSEvaluateICE(XCLPipeline->getII(), "II" , "xcl_pipeline", /*Default*/ -1);
      Option = LoopHintAttr::Pipeline;
      State = LoopHintAttr::Numeric;
      setPipelineDebugLoc(CGF->PragmaSourceLocToDebugLoc(Attr->getLocation()));
      setPipelinePragmaContext(getPragmaContext(Attr)); 
    } else if (XlxFlatten) {
      Option = LoopHintAttr::Flatten;
      State = XlxFlatten->getEnable() ? LoopHintAttr::Enable
                                      : LoopHintAttr::Disable;
      setFlattenDebugLoc(CGF->PragmaSourceLocToDebugLoc(Attr->getLocation()));
      setFlattenPragmaContext(getPragmaContext(Attr)); 
    }else if (XCLFlatten) {
      Option = LoopHintAttr::Flatten;
      State = XCLFlatten->getEnable() ? LoopHintAttr::Enable
                                      : LoopHintAttr::Disable;
      setFlattenDebugLoc(CGF->PragmaSourceLocToDebugLoc(Attr->getLocation()));
      setFlattenPragmaContext(getPragmaContext(Attr)); 
    } else if (XCLTripCount) {
      int Min = CGF->HLSEvaluateICE(XCLTripCount->getMin(), "min", "tripcount", /*Default*/ 0);
      int Max = CGF->HLSEvaluateICE(XCLTripCount->getMax(), "max", "tripcount", /*Default*/ -1);
      // FIXME: potential integer overflow.
      int Avg = CGF->HLSEvaluateICE(XCLTripCount->getAvg(), "avg", "tripcount", /*Default*/ (Min + Max)/2);
      TripInt = {Min, Max, Avg};
      Option = LoopHintAttr::TripCount;
      State = LoopHintAttr::Numeric;
      setTripCountDebugLoc(CGF->PragmaSourceLocToDebugLoc(Attr->getLocation()));
      setTripCountPragmaContext(getPragmaContext(Attr)); 
    } else if (XlxTripCount) {
      int Min = CGF->HLSEvaluateICE(XlxTripCount->getMin(), "min", "tripcount", /*Default*/ 0);
      int Max = CGF->HLSEvaluateICE(XlxTripCount->getMax(), "max", "tripcount", /*Default*/ -1);
      // FIXME: potential integer overflow.
      int Avg = CGF->HLSEvaluateICE(XlxTripCount->getAvg(), "avg", "tripcount",
                                /*Default*/ (Min + Max)/2);
      TripInt = {Min, Max, Avg};
      Option = LoopHintAttr::TripCount;
      State = LoopHintAttr::Numeric;
      setTripCountDebugLoc(CGF->PragmaSourceLocToDebugLoc(Attr->getLocation()));
      setTripCountPragmaContext(getPragmaContext(Attr)); 
    } else if (XCLLatency) {
      int Min = CGF->HLSEvaluateICE(XCLLatency->getMin(), "min", "latency", /*Default*/ 0);
      int Max = CGF->HLSEvaluateICE(XCLLatency->getMax(), "max", "latency", /*Default*/ 65535);
      MinMaxInt = {Min, Max};
      Option = LoopHintAttr::Latency;
      State = LoopHintAttr::Numeric;
    } else if (XCLDataflow) {
      DisableDFPropagation = XCLDataflow->getPropagation();
      Option = LoopHintAttr::DataFlow;
      State = LoopHintAttr::Enable;
      setDataflowDebugLoc(CGF->PragmaSourceLocToDebugLoc(Attr->getLocation()));
      setDataflowPragmaContext(getPragmaContext(Attr)); 
    } else if (LH) {
      //for native "loop_hint" attribute 
      //
      if (!LH->getValue())
        ValueInt = 1; 
      else { 
        auto Res = LH->getValue()->isIntegerConstantExpr(Ctx);
        if (!Res) {
          CGF->CGM.getDiags().Report(LH->getValue()->getExprLoc(), clang::diag::warn_xlx_expr_not_ice) << CGF->getLangOpts().CPlusPlus;
          ValueInt = 1;
        }
        else { 
          ValueInt = LH->getValue()->EvaluateKnownConstInt(Ctx).getZExtValue(); 
        }
      }
      Option = LH->getOption();
      State = LH->getState();
    }

    switch (State) {
    case LoopHintAttr::Disable:
      switch (Option) {
      case LoopHintAttr::Vectorize:
        // Disable vectorization by specifying a width of 1.
        setVectorizeWidth(1);
        break;
      case LoopHintAttr::Interleave:
        // Disable interleaving by speciyfing a count of 1.
        setInterleaveCount(1);
        break;
      case LoopHintAttr::Unroll:
        setUnrollState(LoopAttributes::Disable);
        break;
      case LoopHintAttr::Distribute:
        setDistributeState(false);
        break;
      case LoopHintAttr::Flatten:
        setFlattenState(false);
        break;
      case LoopHintAttr::DataFlow:
      case LoopHintAttr::TripCount:
      case LoopHintAttr::Latency:
      case LoopHintAttr::Pipeline:
      case LoopHintAttr::UnrollCount:
      case LoopHintAttr::UnrollWithoutCheck:
      case LoopHintAttr::VectorizeWidth:
      case LoopHintAttr::InterleaveCount:
        llvm_unreachable("Options cannot be disabled.");
        break;
      }
      break;
    case LoopHintAttr::Enable:
      switch (Option) {
      case LoopHintAttr::Vectorize:
      case LoopHintAttr::Interleave:
        setVectorizeEnable(true);
        break;
      case LoopHintAttr::Unroll:
        setUnrollState(LoopAttributes::Enable);
        break;
      case LoopHintAttr::Distribute:
        setDistributeState(true);
        break;
      case LoopHintAttr::Flatten:
        setFlattenState(true);
        break;
      case LoopHintAttr::DataFlow:
        setDataflow(true, DisableDFPropagation);

        break;
      case LoopHintAttr::TripCount:
      case LoopHintAttr::Latency:
      case LoopHintAttr::Pipeline:
      case LoopHintAttr::UnrollCount:
      case LoopHintAttr::UnrollWithoutCheck:
      case LoopHintAttr::VectorizeWidth:
      case LoopHintAttr::InterleaveCount:
        llvm_unreachable("Options cannot enabled.");
        break;
      }
      break;
    case LoopHintAttr::AssumeSafety:
      switch (Option) {
      case LoopHintAttr::Vectorize:
      case LoopHintAttr::Interleave:
        // Apply "llvm.mem.parallel_loop_access" metadata to load/stores.
        setParallel(true);
        setVectorizeEnable(true);
        break;
      case LoopHintAttr::Unroll:
      case LoopHintAttr::UnrollCount:
      case LoopHintAttr::UnrollWithoutCheck:
      case LoopHintAttr::VectorizeWidth:
      case LoopHintAttr::InterleaveCount:
      case LoopHintAttr::Distribute:
      case LoopHintAttr::Flatten:
      case LoopHintAttr::Pipeline:
      case LoopHintAttr::TripCount:
      case LoopHintAttr::DataFlow:
      case LoopHintAttr::Latency:
        llvm_unreachable("Options cannot be used to assume mem safety.");
        break;
      }
      break;
    case LoopHintAttr::Full:
      switch (Option) {
      case LoopHintAttr::Unroll:
        setUnrollState(LoopAttributes::Full);
        break;
      case LoopHintAttr::Vectorize:
      case LoopHintAttr::Interleave:
      case LoopHintAttr::UnrollCount:
      case LoopHintAttr::UnrollWithoutCheck:
      case LoopHintAttr::VectorizeWidth:
      case LoopHintAttr::InterleaveCount:
      case LoopHintAttr::Distribute:
      case LoopHintAttr::Flatten:
      case LoopHintAttr::Pipeline:
      case LoopHintAttr::TripCount:
      case LoopHintAttr::DataFlow:
      case LoopHintAttr::Latency:
        llvm_unreachable("Options cannot be used with 'full' hint.");
        break;
      }
      break;
    case LoopHintAttr::Numeric:
      switch (Option) {
      case LoopHintAttr::VectorizeWidth:
        setVectorizeWidth(ValueInt);
        break;
      case LoopHintAttr::InterleaveCount:
        setInterleaveCount(ValueInt);
        break;
      case LoopHintAttr::UnrollCount:
        setUnrollCount(ValueInt);
        break;
      case LoopHintAttr::UnrollWithoutCheck:
        setUnrollWithoutCheck(ValueInt);
        break;
      case LoopHintAttr::Pipeline:
        setPipelineII(PipelineInt);
        setPipelineStyle(PipelineStyleInt);
        break;
      case LoopHintAttr::TripCount:
        setTripCount(TripInt);
        break;
      case LoopHintAttr::Latency:
        setMinMax(MinMaxInt);
        break;
      case LoopHintAttr::Unroll:
      case LoopHintAttr::Vectorize:
      case LoopHintAttr::Interleave:
      case LoopHintAttr::Distribute:
      case LoopHintAttr::Flatten:
      case LoopHintAttr::DataFlow:
        llvm_unreachable("Options cannot be assigned a value.");
        break;
      }
      break;
    }
  }

  /// Stage the attributes.
  push(Header, StartLoc, EndLoc);
}

void LoopInfoStack::pop() {
  assert(!Active.empty() && "No active loops to pop");
  Active.pop_back();
}

void LoopInfoStack::InsertHelper(Instruction *I) const {
  if (!hasInfo())
    return;

  const LoopInfo &L = getInfo();
  if (!L.getLoopID())
    return;

  if (TerminatorInst *TI = dyn_cast<TerminatorInst>(I)) {
    for (unsigned i = 0, ie = TI->getNumSuccessors(); i < ie; ++i)
      if (TI->getSuccessor(i) == L.getHeader()) {
        TI->setMetadata(llvm::LLVMContext::MD_loop, L.getLoopID());
        break;
      }
    return;
  }

  if (L.getAttributes().IsParallel && I->mayReadOrWriteMemory())
    I->setMetadata("llvm.mem.parallel_loop_access", L.getLoopID());
}
