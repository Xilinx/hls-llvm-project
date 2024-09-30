// (c) Copyright 2016-2022 Xilinx, Inc.
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
// This contains code to emit Xilinx specific attributes
//
//===----------------------------------------------------------------------===//

#include "CGRecordLayout.h"
#include "CodeGenFunction.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "clang/Basic/HLSDiagnostic.h"
#include "clang/Basic/TargetBuiltins.h"

#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/Support/XILINXFPGAPlatformBasic.h"

using namespace clang;
using namespace sema;
using namespace CodeGen;

StringRef getPragmaContext(const Attr* A) 
{
  if (!A->getPragmaContext() ) { 
    //for "top attribute , some opencl attribute , there is no pragma annotation, so return "user" directly 
    return "user" ; 
  }
    if(A->getPragmaContext()->getName().equals_lower("SLXDIRECTIVE")) { 
      return "user";
    }
    else if (A->getPragmaContext()->getName().equals_lower("HLSDIRECTIVE")) { 
      return "user" ;
    }
    else if (
        A->getPragmaContext()->getName().equals_lower("HLS") || 
        A->getPragmaContext()->getName().equals_lower("AP") || 
        A->getPragmaContext()->getName().equals_lower("AUTOPILOT")) { 
      return "user"; 
    }
    
#if 0
  switch (A->getKind()) { 
    case attr::XlxPerformance:
    case attr::XCLMaxWorkGroupSize : 
    case attr::XCLZeroGlobalWorkOffset: 
    case attr::XCLSingleWorkitem:
    case attr::XCLPipelineLoop: 
    case attr::XlxRewinding: 
    case attr::XCLPipelineWorkitems: 
    case attr::XlxPipeline: 
    case attr::XCLUnrollWorkitems: 
    case attr::XlxUnrollRegionHint: 
    case attr::XCLFlattenLoop: 
    case attr::XlxMergeLoop: 
    case attr::XCLLoopTripCount: 
    case attr::XlxLoopTripCount: 
    case attr::XlxStable: 
    case attr::XlxBindStorage:
    case attr::XlxStableContent:
    case attr::XlxShared:
    case attr::XlxDisaggr:
    case attr::XlxDataPack:
    case attr::XlxBindOp:
    case attr::XlxBindOpExpr:
    case attr::XlxDependence:
    case attr::XlxCrossDependence:
    case attr::XlxArrayStencil:
    case attr::XCLReqdPipeDepth: 
    case attr::XlxReqdPipeDepth: 
    case attr::XCLVisibility: 
    case attr::XCLDataFlow: 
    case attr::XCLOutline: 
    case attr::XCLInline: 
    case attr::XCLRegionName: 
    case attr::FPGAResourceHint: 
    case attr::XlxFunctionAllocation: 
    case attr::FPGAResourceLimitHint: 
    case attr::XlxArrayGeometry: 
    case attr::XCLArrayGeometry:
    case attr::XlxArrayPartitionXForm:
    case attr::XlxArrayReshapeXForm:
    case attr::XCLArrayXForm:
    case attr::XCLArrayView:
    case attr::XlxArrayView:
    case attr::SDxKernel:
    case attr::XlxExprBalance:
    case attr::XlxFuncInstantiate:
    case attr::XlxOccurrence:
    case attr::XlxProtocol:
    case attr::XlxVarReset:
    case attr::XlxResetIntrinsic:
    case attr::XCLLatency:
    case attr::MAXIAdaptor:
    case attr::SAXIAdaptor:
    case attr::AXISAdaptor:
    case attr::BRAMAdaptor:
    case attr::FPGAScalarInterface:
    case attr::FPGAScalarInterfaceWrapper:
    case attr::FPGAAddressInterface:
    case attr::SAXILITEOffsetInterface:
    case attr::MAXIInterface:
    case attr::AXIStreamInterface:
    case attr::MemoryInterface:
    case attr::APFifoInterface:
    case attr::APScalarInterface:
    case attr::APScalarInterruptInterface:
    case attr::FPGAFunctionCtrlInterface:
    case attr::XlxCache:
    case attr::FPGARegister:
    case attr::FPGADataFootPrintHint:
    case attr::FPGASignalName:
    case attr::FPGAMaxiMaxWidenBitwidth:
    case attr::FPGAMaxiLatency:
    case attr::FPGAMaxiNumRdOutstand:
    case attr::FPGAMaxiNumWtOutstand:
    case attr::FPGAMaxiRdBurstLen:
    case attr::FPGAMaxiWtBurstLen:
    case attr::CodeGenType:
    case attr::Unpacked:
    case attr::HLSPreserve:
    default:
      return "user"; 
#endif 
} 

/// ExtractAttrInteger - Extract the integer value from the attribute argument.
/// \param Attr, The attribute we are trying to extract the integer value.
/// \param E, The expression from the attribute argument.
/// \param LB, The lower bound for the integer value which is included.
/// \param UB, the upper bound for the integer value which is included.
/// returns the extracted integer or \param Default value.
static int ExtractAttrInteger(const Attr *Attr, Expr *E,
                              DiagnosticsEngine &Diag, const ASTContext &Ctx,
                              int LB, int UB, int Default = -1) {
  if (!E)
    return Default;

  llvm::APSInt ValueAPS = E->EvaluateKnownConstInt(Ctx);
  int64_t Ret = ValueAPS.getSExtValue();

  if (LB <= UB && (Ret < LB || Ret > UB)) {
    Diag.Report(Attr->getLocation(), diag::err_attribute_argument_outof_range)
        << Attr->getSpelling() << LB << UB << E->getSourceRange();
    return Default;
  }

  if (LB == 1 && Ret < 1) {
    Diag.Report(Attr->getLocation(),
                diag::err_attribute_requires_positive_integer)
        << Attr->getSpelling() << E->getSourceRange();
    return Default;
  }

  if (LB == 0 && Ret < 0) {
    Diag.Report(Attr->getLocation(), diag::err_attribute_argument_out_of_bounds)
        << Attr->getSpelling() << 0 << E->getSourceRange();
    return Default;
  }

  return Ret;
}

static int EvaluateInteger(Expr *E, const ASTContext &Ctx, int Default = -1) {
  if (!E)
    return Default;
  
  llvm::APSInt Value = E->EvaluateKnownConstInt(Ctx);
  return Value.getSExtValue();
}


static int64_t HLSEvaluateClockCycle(Expr *E, bool isSec, double clockPeriod, const char * option_name, CodeGenModule& CGM,  const ASTContext &Ctx) {

  if (!E)
    return 0;

  Expr::EvalResult EvalResult;
  bool Result = E->EvaluateAsRValue(EvalResult, Ctx);

  if (Result && !EvalResult.HasSideEffects) {
    if (EvalResult.Val.isInt()) {
      llvm::APSInt IntValue = EvalResult.Val.getInt();
      if (isSec){ 
        llvm::APFloat clockPeriod_Float(clockPeriod);
        SmallString<32> clockPeriod_Str;
        clockPeriod_Float.toString(clockPeriod_Str);

        int64_t ret =  IntValue.getSExtValue() * 1000000000 / clockPeriod; 
        if (ret == 0 && !IntValue.isNullValue()){ 
          CGM.getDiags().Report(E->getLocStart(), diag::warn_xlx_performance_option_is_near_to_zero)
          << option_name << clockPeriod_Str; 
          return -1; 
        }
      }
      else { 
        return IntValue.getSExtValue();
      }
    } else if (EvalResult.Val.isFloat()) {
      llvm::APFloat FloatVal = EvalResult.Val.getFloat();
      if (isSec){ 
        FloatVal = (FloatVal * llvm::APFloat((double)(1000000000))) / llvm::APFloat((double)clockPeriod); 
      }

      llvm::APSInt IntValue(64, false);
      bool isExact = false;
      llvm::APFloat::opStatus status = FloatVal.convertToInteger(
          IntValue, llvm::APFloat::rmNearestTiesToAway, &isExact);

      if (!isSec && status != llvm::APFloat::opOK && status != llvm::APFloat::opInexact) {
        /// error out this convert float to integer
        CGM.getDiags().Report(E->getExprLoc(), diag::err_xlx_float2int_failed);
        return 0;
      }

      if (!isSec && status == llvm::APFloat::opInexact) {
        /// warning floating fraction truncate
        CGM.getDiags().Report(E->getExprLoc(),
                              diag::warn_xlx_float2int_inexact);
      }

      int64_t ret = IntValue.getSExtValue();
      if (isSec && ret == 0 && !EvalResult.Val.getFloat().isZero()){ 
        llvm::APFloat clockPeriod_Float(clockPeriod);
        SmallString<32> clockPeriod_Str;
        clockPeriod_Float.toString(clockPeriod_Str);

        CGM.getDiags().Report(E->getLocStart(), diag::warn_xlx_performance_option_is_near_to_zero)
        << option_name << clockPeriod_Str.c_str();
        return -1;
      }
      return ret; 
    }
  }

  CGM.getDiags().Report(E->getExprLoc(), diag::err_xlx_expr_not_dce);
  return 0;
}

//evaluate the 'if (cond)' option in HLS pragma , default value is true
bool CodeGenFunction::EvaluateHLSIFCond(const Expr* ifCond) { 
  if (!ifCond)
    return true; 

  if (isa<CallExpr>(ifCond) && cast<CallExpr>(ifCond)->getBuiltinCallee()) 
  {
    const CallExpr *call = cast<CallExpr>(ifCond); 
    if (Builtin::BI__hls_function_name_match == cast<CallExpr>(ifCond)->getBuiltinCallee() ) { 
      const Expr* E = call->getArg(0); 
      if (isa<CastExpr>(E)){ 
        E = cast<CastExpr>(E)->getSubExpr();
      }
      const StringLiteral *nameLiteral = cast<StringLiteral>(E);  
      if (nameLiteral->getBytes().equals(CurFn->getName())) { 
        return true; 
      }
      else { 
        return false; 
      }
    }
    else { 
      CGM.getDiags().Report(ifCond->getExprLoc(), diag::err_xlx_expr_not_ice);
      return true; 
    }
  }

  if (!ifCond->isEvaluatable(getContext())) {
    CGM.getDiags().Report(ifCond->getExprLoc(), diag::err_xlx_expr_not_ice);
  }
  else { 
    llvm::APSInt Value = ifCond->EvaluateKnownConstInt(getContext());
    if (Value.getZExtValue() == 0){ 
      return false; 
    }
  }
  return true; 
}

int CodeGenFunction::HLSEvaluateICE(Expr *E, StringRef optionName, StringRef pragmaName, int Default ) {
  if (!E)
    return Default;

  if (!E->isEvaluatable(getContext())) {
    CGM.getDiags().Report(E->getExprLoc(), diag::err_xlx_option_not_ice)
      << optionName << pragmaName;
    return Default;
  }
  else { 
    llvm::APSInt Value = E->EvaluateKnownConstInt(getContext());
    return (int)Value.getSExtValue();
  }
  return Default; 
}

Optional<int> CodeGenFunction::HLSEvaluateICEResult(Expr *E)  
{
  if (!E)
    return None;

  if (!E->isEvaluatable(getContext())) {
    return None;
  }
  else { 
    llvm::APSInt Value = E->EvaluateKnownConstInt(getContext());
    return (int)Value.getSExtValue();
  }
}


template <unsigned N>
static llvm::ConstantAsMetadata *CreateIntMeatadata(llvm::LLVMContext &Ctx,
                                                    uint64_t V) {
  return llvm::ConstantAsMetadata::get(
      llvm::ConstantInt::get(llvm::IntegerType::get(Ctx, N), V));
}

static void EmitXCLArrayXFormAttr(XCLArrayXFormAttr *A, llvm::LLVMContext &Ctx,
                                  SmallVectorImpl<llvm::Metadata *> &MDs,
                                  const ASTContext &ASTCtx) {
  auto Dim = EvaluateInteger(A->getDim(), ASTCtx, /*Default*/ 1);
  auto Factor = EvaluateInteger(A->getFactor(), ASTCtx, /*Default*/ 0);
  MDs.push_back(llvm::MDNode::get(
      Ctx, {
               llvm::MDString::get(Ctx, A->getSpelling()),
               llvm::MDString::get(
                   Ctx, XCLArrayXFormAttr::ConvertXCLArrayXFormTypeToStr(
                            A->getType())),
               CreateIntMeatadata<32>(Ctx, Dim),
               CreateIntMeatadata<32>(Ctx, Factor),
           }));
}

static void EmitXCLVisibilityAttr(XCLVisibilityAttr *A, llvm::LLVMContext &Ctx,
                                  SmallVectorImpl<llvm::Metadata *> &MDs) {
  MDs.push_back(llvm::MDNode::get(
      Ctx, {llvm::MDString::get(Ctx, A->getSpelling()),
            llvm::MDString::get(
                Ctx, XCLVisibilityAttr::ConvertXCLVisibilityTypeToStr(
                         A->getVisibility()))}));
}

static void EmitXCLReqdPipeDepthAttr(XCLReqdPipeDepthAttr *A,
                                     llvm::LLVMContext &Ctx,
                                     SmallVectorImpl<llvm::Metadata *> &MDs,
                                     const ASTContext &ASTCtx) {
  if (A->getType() > 0) {
    auto Depth = EvaluateInteger(A->getDepth(), ASTCtx, /*Default*/ 1);
    MDs.push_back(
        llvm::MDNode::get(Ctx, {llvm::MDString::get(Ctx, "xlx_fpga_pipo_depth"),
                                CreateIntMeatadata<32>(Ctx, Depth)}));
  } else {
    auto Depth = EvaluateInteger(A->getDepth(), ASTCtx, /*Default*/ 1);
    MDs.push_back(
        llvm::MDNode::get(Ctx, {llvm::MDString::get(Ctx, A->getSpelling()),
                                CreateIntMeatadata<32>(Ctx, Depth)}));
  }
}

static void EmitXlxReqdPipeDepthAttr(XlxReqdPipeDepthAttr *A,
                                     llvm::LLVMContext &Ctx,
                                     SmallVectorImpl<llvm::Metadata *> &MDs,
                                     const ASTContext &ASTCtx) {
  if (A->getType() > 0) {
    auto Depth = EvaluateInteger(A->getDepth(), ASTCtx, /*Default*/ 1);
    MDs.push_back(
        llvm::MDNode::get(Ctx, {llvm::MDString::get(Ctx, "xlx_fpga_pipo_depth"),
                                CreateIntMeatadata<32>(Ctx, Depth)}));
  } else {
    auto Depth = EvaluateInteger(A->getDepth(), ASTCtx,  /*Default*/ 1);
    // to be compatible with Reflow's handler, using "xcl_reqd_pipe_depth"
    // instead of "xlx_read_pipe_depth"
    MDs.push_back(
        llvm::MDNode::get(Ctx, {llvm::MDString::get(Ctx, "xcl_reqd_pipe_depth"),
                                CreateIntMeatadata<32>(Ctx, Depth)}));
  }
}

template <typename AttrType>
static void EmitVarEnableAttr(AttrType *A, llvm::LLVMContext &Ctx,
                              SmallVectorImpl<llvm::Metadata *> &MDs) {
  MDs.push_back(
      llvm::MDNode::get(Ctx, {llvm::MDString::get(Ctx, A->getSpelling()),
                              CreateIntMeatadata<32>(Ctx, A->getEnabled())}));
}

static bool IsPhysicalInterfaceType(const RecordDecl *D) {
  auto *NS = dyn_cast_or_null<NamespaceDecl>(D->getDeclContext());
  if (!NS) {
    return llvm::StringSwitch<bool>(D->getName())
        .Case("ap_shift_reg", true)
        .Default(false);
  }

  if (!NS->getName().equals("hls"))
    return false;
  auto *ParentCtx = NS->getDeclContext();
  if (!ParentCtx || !isa<TranslationUnitDecl>(ParentCtx))
    return false;

  return llvm::StringSwitch<bool>(D->getName())
      .Case("stream", true)
      .Default(false);
}

static void CollectBundles(const RecordType *RT, CodeGenTypes &CGT,
                           llvm::Constant *Base,
                           SmallVectorImpl<llvm::Value *> &Indicies,
                           SmallVectorImpl<llvm::Metadata *> &GEPs) {
  auto *D = RT->getDecl();
  if (IsPhysicalInterfaceType(D)) {
    auto *ET = Base->getType()->getPointerElementType();
    auto *P = llvm::ConstantExpr::getInBoundsGetElementPtr(ET, Base, Indicies);
    auto &Ctx = P->getContext();
    GEPs.push_back(
        llvm::MDTuple::get(Ctx, {llvm::MDString::get(Ctx, D->getName()),
                                 llvm::ConstantAsMetadata::get(P)}));
    return;
  }

  auto *I32T = Indicies.front()->getType();
  Indicies.push_back(Indicies.front());
  for (auto *F : D->fields()) {
    if (F->isBitField())
      continue;

    unsigned int Idx = CGT.getCGRecordLayout(D).getLLVMFieldNo(F);
    Indicies.back() = llvm::ConstantInt::get(I32T, Idx);
    if (auto *T = dyn_cast<RecordType>(F->getType().getCanonicalType()))
      CollectBundles(T, CGT, Base, Indicies, GEPs);
  }
  Indicies.pop_back();
}

// emit attribute for variable decl
static void EmitXlxAttributesImpl(const VarDecl *D, CodeGenTypes &CGT,
                                  llvm::Value *V, llvm::LLVMContext &Ctx,
                                  const ASTContext &ASTCtx,
                                  DiagnosticsEngine &Diag) {
  SmallVector<llvm::Metadata *, 8> MDs;

  for (auto *A : D->specific_attrs<XCLArrayXFormAttr>())
    EmitXCLArrayXFormAttr(A, Ctx, MDs, ASTCtx);

  for (auto *A : D->specific_attrs<XCLVisibilityAttr>())
    EmitXCLVisibilityAttr(A, Ctx, MDs);

  for (auto *A : D->specific_attrs<XlxVarResetAttr>())
    EmitVarEnableAttr<XlxVarResetAttr>(A, Ctx, MDs);

  for (auto *A : D->specific_attrs<XCLReqdPipeDepthAttr>()) {
    if (!D->getType()->isPipeType()) {
      for (auto *A : D->specific_attrs<XCLReqdPipeDepthAttr>())
        EmitXCLReqdPipeDepthAttr(A, Ctx, MDs, ASTCtx);
    }
  }

  for (auto *A : D->specific_attrs<XlxReqdPipeDepthAttr>()) {
    if (!D->getType()->isPipeType()) {
      for (auto *A : D->specific_attrs<XlxReqdPipeDepthAttr>())
        EmitXlxReqdPipeDepthAttr(A, Ctx, MDs, ASTCtx);
    }
  }

  if (MDs.empty())
    return;

  auto *N = llvm::MDNode::get(Ctx, MDs);
  if (auto *AI = dyn_cast<llvm::AllocaInst>(V)) {
    AI->setMetadata("xilinx.attributes", N);
    return;
  }

  if (auto *GV = dyn_cast<llvm::GlobalVariable>(V)) {
    GV->setMetadata("xilinx.attributes", N);
    return;
  }
}

void CodeGenFunction::EmitStopPoint(SourceLocation Loc) {
  if (CGDebugInfo *DI = getDebugInfo()) {
    DI->EmitLocation(Builder, Loc);
    LastStopPoint = Loc;
  }
}

void CodeGenFunction::EmitXlxAttributes(const VarDecl *D, llvm::Value *V) {
  if (getLangOpts().HLSExt) {
    EmitXlxAttributesImpl(D, CGM.getTypes(), V, getLLVMContext(), getContext(),
                          CGM.getDiags());
    // if the VarDecl is static local const
  }
}

void CodeGenModule::SetXlxAttributes(const VarDecl *D,
                                     llvm::GlobalVariable *GV) {
  if (getLangOpts().HLSExt)
    EmitXlxAttributesImpl(D, getTypes(), GV, getLLVMContext(), getContext(),
                          getDiags());
}

static std::string SynthesisAttr(FPGAFunctionCtrlInterfaceAttr *A) {
  std::string S = A->getMode().str() + "." + std::to_string(0);
  auto Adaptor = A->getName();
  if (!Adaptor.empty()) {
    S += ".";
    S += Adaptor.str();
  }

  return S;
}

static std::string SynthesisAttr(FPGAScalarInterfaceAttr *A) {
  SmallString<64> S = A->getMode();
  auto Adaptor = A->getAdaptor();
  if (!Adaptor.empty()) {
    S += ".";
    S += Adaptor;
  }

  return S.str();
}

static std::string SynthesisAttr(FPGAScalarInterfaceWrapperAttr *A,
                                 ASTContext &Ctx) {
  SmallString<64> S = A->getMode();
  auto Adaptor = A->getAdaptor();
  auto *OffsetExpr = A->getOffset();
  if (!Adaptor.empty()) {
    S += ".";
    S += Adaptor;
    // Offset should be available only if the Adaptor is specified.
    if (OffsetExpr) {
      auto OffsetInt = EvaluateInteger(OffsetExpr, Ctx);
      if (OffsetInt != -1) {
        const auto &Offset = llvm::utostr(OffsetInt);
        S += ".";
        S += Offset;
      }
    }
  }

  return S.str();
}

static std::string SynthesisAttr(FPGAAddressInterfaceAttr *A,
                                 bool hasInterfaceWrapper = false) {
  SmallString<64> S = A->getMode();

  auto Adaptor = A->getAdaptor();
  if (!Adaptor.empty()) {
    S += ".";
    S += Adaptor;
  }

  auto CurrentMode = A->getMode();
  auto OffsetMode = A->getOffsetMode();
  if (OffsetMode != FPGAAddressInterfaceAttr::Default &&
      OffsetMode != FPGAAddressInterfaceAttr::Off) {
    // Not only maxi mode
    S += ".";
    S += FPGAAddressInterfaceAttr::ConvertOffsetModeTypeToStr(OffsetMode);
  } else if (CurrentMode == "m_axi" &&
             OffsetMode != FPGAAddressInterfaceAttr::Default) {
    // Only maxi mode, if user sets offset value, no matter s_axilite or not,
    // dump safely to LLVM IR
    S += ".";
    S += FPGAAddressInterfaceAttr::ConvertOffsetModeTypeToStr(OffsetMode);
  } else {
    // If no offset (default), do nothing on LLVM IR
  }

  return S.str();
}

static bool IsHLSStreamOfBlocksType(QualType Ty) { 
  if (isa<DecayedType>(Ty)) { 
    Ty = cast<DecayedType>(Ty)->getOriginalType();
  }
  Ty = Ty.getCanonicalType();

  auto *BTy = Ty->isReferenceType() ? Ty->getPointeeType().getTypePtr()
                                    : Ty->getPointeeOrArrayElementType();

  if (BTy->isClassType() && !BTy->getAsCXXRecordDecl()
                                 ->getCanonicalDecl()
                                 ->getQualifiedNameAsString()
                                 .compare("hls::stream_of_blocks"))
    return true;

  // hls::stream<type> with template in type
  if (dyn_cast<TemplateSpecializationType>(BTy)) {
    TemplateName name =
        dyn_cast<TemplateSpecializationType>(BTy)->getTemplateName();
    if (TemplateDecl *decl = name.getAsTemplateDecl()) {
      std::string base_name = decl->getQualifiedNameAsString();
      if (base_name == "hls::stream_of_blocks")
        return true;
    }
  }
  return false;

}

static bool IsHLSBurstMaxiType(QualType ty) {
  auto Ty = ty.getCanonicalType();

  if (Ty->isClassType() && !Ty->getAsCXXRecordDecl()
                                 ->getCanonicalDecl()
                                 ->getQualifiedNameAsString()
                                 .compare("hls::burst_maxi"))
    return true;

  // hls::stream<type> with template in type
  if (dyn_cast<TemplateSpecializationType>(Ty)) {
    TemplateName name =
        dyn_cast<TemplateSpecializationType>(Ty)->getTemplateName();
    if (TemplateDecl *decl = name.getAsTemplateDecl()) {
      std::string base_name = decl->getQualifiedNameAsString();
      if (base_name == "hls::burst_maxi")
        return true;
    }
  }
  return false;
}

static bool IsHlsDirectIOType(QualType ty) {
  auto Ty = ty.getCanonicalType();

  if (Ty->isClassType() && !Ty->getAsCXXRecordDecl()
                                 ->getCanonicalDecl()
                                 ->getQualifiedNameAsString()
                                 .compare("hls::directio"))
    return true;

  // hls::directio<type> with template in type
  if (dyn_cast<TemplateSpecializationType>(Ty)) {
    TemplateName name =
        dyn_cast<TemplateSpecializationType>(Ty)->getTemplateName();
    if (TemplateDecl *decl = name.getAsTemplateDecl()) {
      std::string base_name = decl->getQualifiedNameAsString();
      if (base_name == "hls::directio")
        return true;
    }
  }
  return false;
}

static bool IsHLSStreamType(QualType ty) {
  auto Ty = ty.getCanonicalType();

  if (Ty->isClassType() && !Ty->getAsCXXRecordDecl()
                                 ->getCanonicalDecl()
                                 ->getQualifiedNameAsString()
                                 .compare("hls::stream"))
    return true;

  // hls::stream<type> with template in type
  if (dyn_cast<TemplateSpecializationType>(Ty)) {
    TemplateName name =
        dyn_cast<TemplateSpecializationType>(Ty)->getTemplateName();
    if (TemplateDecl *decl = name.getAsTemplateDecl()) {
      std::string base_name = decl->getQualifiedNameAsString();
      if (base_name == "hls::stream")
        return true;
    }
  }
  return false;
}

template <typename BuilderType>
static void CreateCallSideEffect(CodeGenModule &CGM, BuilderType &Builder, StringRef name, llvm::ArrayRef<llvm::Value*> args, int64_t port_width = -1, StringRef pragmaContext = "user") {
  SmallVector<llvm::OperandBundleDef, 1> bundleDefs;
  bundleDefs.emplace_back(name, args);

  auto *decl = llvm::Intrinsic::getDeclaration(
      &(CGM.getModule()), llvm::Intrinsic::sideeffect);

  auto *CI = Builder.CreateCall(decl, None, bundleDefs);

  llvm::AttrBuilder builder;
  if (port_width >= 0) {
    builder.addAttribute("xlx.port.bitwidth", std::to_string(port_width));
  }
  builder.addAttribute("xlx.source", pragmaContext);
  llvm::AttributeList attr_list = llvm::AttributeList::get(CGM.getLLVMContext(), llvm::AttributeList::FunctionIndex, builder);
  CI->setAttributes(attr_list);

  CI->setOnlyAccessesInaccessibleMemory();
  CI->setDoesNotThrow();
}

void CodeGenModule::GenerateDirectIOAnnotationIntrinsic(llvm::IRBuilder<> &Builder, llvm::Value* parm, const ParmVarDecl *parm_decl, QualType type, GEPFields  fields, bool parm_is_pointer)
{
  type = type.getCanonicalType();
  bool cur_is_pointer = false;
  clang::RecordDecl* recordDecl = nullptr;
  if (type->isPointerType() || type->isReferenceType()){ 
    type  = type->getPointeeType();
    cur_is_pointer = true;
  }
  if (IsHlsDirectIOType(type)){ 
    if (!cur_is_pointer & !parm_is_pointer) {
      getDiags().Report(parm_decl->getLocation(),
         diag::warn_invalid_variable_expr);
      return ;
    }
    

    if ( parm_is_pointer) { 
      llvm::SmallVector<llvm::Value*, 4> idxs;
      if (fields.size() <= 1 ) { 
        llvm::Value *args[] = { parm };
        CreateCallSideEffect(*this, Builder, "directio_interface", args);
      }
      else { 
        for( int i = 0; i < fields.size(); i++ ) { 
          idxs.push_back( Builder.getInt32(fields[i]));
        }
        auto lv = Builder.CreateInBoundsGEP(parm, idxs);
    
        llvm::Value *args[] = { lv };
        CreateCallSideEffect(*this, Builder, "directio_interface", args);
      }
      return ;
    }
    else { 
      auto lv = Builder.CreateExtractValue(parm, makeArrayRef(fields));

      llvm::Value *args[] = { lv };
      CreateCallSideEffect(*this, Builder, "directio_interface", args);
      return ;
    }
  }

  if (cur_is_pointer) { 
    // current field is pointer type , we can not suppot it , if use specific following pragma 
    // #pragma HLS interface m_axi port =  arg.pointer_field
    //
    return ;
  }

  if (auto* typedefType = llvm::dyn_cast<clang::TypedefType>(type)) {
      GenerateDirectIOAnnotationIntrinsic(Builder, parm, parm_decl, typedefType->getDecl()->getUnderlyingType(), fields, parm_is_pointer);
      return;
  } else if (auto* elaboratedType = llvm::dyn_cast<clang::ElaboratedType>(type)) {
      GenerateDirectIOAnnotationIntrinsic(Builder, parm, parm_decl, elaboratedType->getNamedType(), fields, parm_is_pointer);
      return;
  } else if (auto* recordType = llvm::dyn_cast<clang::RecordType>(type)) {
      recordDecl = recordType->getDecl()->getDefinition();
  } else if (type->isStructureType()) {
      recordDecl = type->getAsStructureType()->getDecl();
  } else if (type->isArrayType()) {
      fields.push_back(0);
      GenerateDirectIOAnnotationIntrinsic(Builder, parm, parm_decl, getContext().getAsArrayType(type)->getElementType(), fields, parm_is_pointer);
      return;
  }

  if (recordDecl) { 
    for (auto it = recordDecl->field_begin(); it != recordDecl->field_end(); ++it) {
      fields.push_back(it->getFieldIndex());
      GenerateDirectIOAnnotationIntrinsic(Builder, parm, parm_decl, it->getType(), fields, parm_is_pointer);
      fields.pop_back();
    }
  }
}

void CodeGenModule::GenerateStreamAnnotationIntrinsic(llvm::IRBuilder<> &Builder, llvm::Value* parm, const ParmVarDecl *parm_decl, QualType type, GEPFields  fields, bool parm_is_pointer)
{
  type = type.getCanonicalType();
  bool cur_is_pointer = false;
  clang::RecordDecl* recordDecl = nullptr;
  if (type->isPointerType() || type->isReferenceType()){ 
    type  = type->getPointeeType();
    cur_is_pointer = true;
  }
  if (IsHLSStreamType(type)){ 
    if (!cur_is_pointer & !parm_is_pointer) {
      getDiags().Report(parm_decl->getLocation(),
         diag::warn_invalid_variable_expr);
      return ;
    }
    

    if ( parm_is_pointer) { 
      llvm::SmallVector<llvm::Value*, 4> idxs;
      if (fields.size() <= 1 ) { 
        llvm::Value *args[] = { parm };
        CreateCallSideEffect(*this, Builder, "stream_interface", args);
      }
      else { 
        for( int i = 0; i < fields.size(); i++ ) { 
          idxs.push_back( Builder.getInt32(fields[i]));
        }
        auto lv = Builder.CreateInBoundsGEP(parm, idxs);
    
        llvm::Value *args[] = { lv };
        CreateCallSideEffect(*this, Builder, "stream_interface", args);
      }
      return ;
    }
    else { 
      auto lv = Builder.CreateExtractValue(parm, makeArrayRef(fields));

      llvm::Value *args[] = { lv };
      CreateCallSideEffect(*this, Builder, "stream_interface", args);
      return ;
    }
  }

  if (cur_is_pointer) { 
    // current field is pointer type , we can not suppot it , if use specific following pragma 
    // #pragma HLS interface m_axi port =  arg.pointer_field
    //
    return ;
  }

  if (auto* typedefType = llvm::dyn_cast<clang::TypedefType>(type)) {
      GenerateStreamAnnotationIntrinsic(Builder, parm, parm_decl, typedefType->getDecl()->getUnderlyingType(), fields, parm_is_pointer);
      return;
  } else if (auto* elaboratedType = llvm::dyn_cast<clang::ElaboratedType>(type)) {
      GenerateStreamAnnotationIntrinsic(Builder, parm, parm_decl, elaboratedType->getNamedType(), fields, parm_is_pointer);
      return;
  } else if (auto* recordType = llvm::dyn_cast<clang::RecordType>(type)) {
      recordDecl = recordType->getDecl()->getDefinition();
  } else if (type->isStructureType()) {
      recordDecl = type->getAsStructureType()->getDecl();
  } else if (type->isArrayType()) {
      fields.push_back(0);
      GenerateStreamAnnotationIntrinsic(Builder, parm, parm_decl, getContext().getAsArrayType(type)->getElementType(), fields, parm_is_pointer);
      return;
  }

  if (recordDecl) { 
    for (auto it = recordDecl->field_begin(); it != recordDecl->field_end(); ++it) {
      fields.push_back(it->getFieldIndex());
      GenerateStreamAnnotationIntrinsic(Builder, parm, parm_decl, it->getType(), fields, parm_is_pointer);
      fields.pop_back();
    }
  }
}

void CodeGenFunction::EmitXlxParamAttributes(const ParmVarDecl *D,
                                             llvm::Value *V) {
  auto *Parm = dyn_cast<llvm::Argument>(V);
  if (!Parm) {
    assert(false &&
           "seriouse bug, some parameter can not emit llvm argument correctly");
    return;
  }
  auto &Ctx = getLLVMContext();
  SmallVector<llvm::OperandBundleDef, 4> ScopeAttrs;

  // strip decayed and typedef type
  QualType DeclType = D->getOriginalType().getCanonicalType();

  if (auto *A = D->getAttr<XCLArrayGeometryAttr>()) {
    SmallVector<llvm::Value *, 4> Dims;
    for (auto Dim : A->dims()) {
      ApplyDebugLocation DL(*this, Dim);
      Dims.push_back(EmitScalarExpr(Dim));
    }

    ScopeAttrs.emplace_back(A->getSpelling(), Dims);
  }

  if (auto *AT = dyn_cast<ConstantArrayType>(DeclType)) {
    // Annotate the dim from the original type
    const auto &Depth = llvm::utostr(AT->getSize().getZExtValue());
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.decayed.dim.hint", Depth));
  }

  if (auto *A = D->getAttr<XCLReqdPipeDepthAttr>()) {
    if (A->getType() > 0) {
      auto DepthInt =
          EvaluateInteger(A->getDepth(), getContext(), /*Default*/ 1);
      const auto &Depth = llvm::utostr(DepthInt);
      Parm->addAttr(llvm::Attribute::get(Ctx, "xlx.fpga.pipo.depth", Depth));
    } else {
      auto DepthInt =
          EvaluateInteger(A->getDepth(), getContext(), /*Default*/ 1);
      const auto &Depth = llvm::utostr(DepthInt);
      Parm->addAttr(llvm::Attribute::get(Ctx, "xcl.reqd.pipe.depth", Depth));
    }
  }

  if (auto *A = D->getAttr<XlxReqdPipeDepthAttr>()) {
    if (A->getType() > 0) {
      auto DepthInt =
          EvaluateInteger(A->getDepth(), getContext(), /*Default*/ 1);
      const auto &Depth = llvm::utostr(DepthInt);
      Parm->addAttr(llvm::Attribute::get(Ctx, "xlx.fpga.pipo.depth", Depth));
    } else {
      auto DepthInt =
          EvaluateInteger(A->getDepth(), getContext(), /*Default*/ 1);
      const auto &Depth = llvm::utostr(DepthInt);
      Parm->addAttr(llvm::Attribute::get(Ctx, "xcl.reqd.pipe.depth", Depth));
    }
  }

  if (D->hasAttr<FPGARegisterAttr>())
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.register"));

  if (auto *A = D->getAttr<FPGADataFootPrintHintAttr>()) {
    auto DepthInt = EvaluateInteger(A->getDepth(), getContext(), /*Default*/ 0);
    const auto &Depth = llvm::utostr(DepthInt);
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.data.footprint.hint", Depth));
  }

  if (auto *A = D->getAttr<FPGASignalNameAttr>()) {
    auto Name = A->getName();
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.signal.name", Name));
  }

  if (auto *A = D->getAttr<FPGAMaxiMaxWidenBitwidthAttr>()) {
    auto MaxWidenBitwidthInt =
        EvaluateInteger(A->getMaxWidenBitwidth(), getContext(), /*Default*/ 0);
    const auto &MaxWidenBitwidth = llvm::utostr(MaxWidenBitwidthInt);
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.maxi.max_widen_bitwidth",
                                       MaxWidenBitwidth));
  }

  if (auto *A = D->getAttr<FPGAMaxiLatencyAttr>()) {
    auto LatencyInt =
        EvaluateInteger(A->getLatency(), getContext(), /*Default*/ 0);
    const auto &Latency = llvm::utostr(LatencyInt);
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.maxi.latency", Latency));
  }

  if (auto *A = D->getAttr<FPGAMaxiNumRdOutstandAttr>()) {
    auto NumRdOutstandInt =
        EvaluateInteger(A->getNumRdOutstand(), getContext(), /*Default*/ 0);
    const auto &NumRdOutstand = llvm::utostr(NumRdOutstandInt);
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.maxi.num_read_outstanding",
                                       NumRdOutstand));
  }

  if (auto *A = D->getAttr<FPGAMaxiNumWtOutstandAttr>()) {
    auto NumWtOutstandInt =
        EvaluateInteger(A->getNumWtOutstand(), getContext(), /*Default*/ 0);
    const auto &NumWtOutstand = llvm::utostr(NumWtOutstandInt);
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.maxi.num_write_outstanding",
                                       NumWtOutstand));
  }

  if (auto *A = D->getAttr<FPGAMaxiRdBurstLenAttr>()) {
    auto RdBurstLenInt =
        EvaluateInteger(A->getRdBurstLen(), getContext(), /*Default*/ 0);
    const auto &RdBurstLen = llvm::utostr(RdBurstLenInt);
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.maxi.max_read_burst_length",
                                       RdBurstLen));
  }

  if (auto *A = D->getAttr<FPGAMaxiWtBurstLenAttr>()) {
    auto WtBurstLenInt =
        EvaluateInteger(A->getWtBurstLen(), getContext(), /*Default*/ 0);
    const auto &WtBurstLen = llvm::utostr(WtBurstLenInt);
    Parm->addAttr(llvm::Attribute::get(Ctx, "fpga.maxi.max_write_burst_length",
                                       WtBurstLen));
  }

  if (auto *A = D->getAttr<FPGAScalarInterfaceAttr>()) {
    const std::string &Name = "fpga.scalar.interface";
    const std::string &Attr = SynthesisAttr(A);
    Parm->addAttr(llvm::Attribute::get(Ctx, Name, Attr));
  }

  if (auto *A = D->getAttr<FPGAScalarInterfaceWrapperAttr>()) {
    const std::string &Name = "fpga.interface.wrapper";
    const std::string &Attr = SynthesisAttr(A, getContext());
    Parm->addAttr(llvm::Attribute::get(Ctx, Name, Attr));
  }

  if (auto *A = D->getAttr<FPGAAddressInterfaceAttr>()) {
    bool hasInterfaceWrapper = D->hasAttr<FPGAScalarInterfaceWrapperAttr>();

    const std::string &Name = "fpga.address.interface";
    const std::string &Attr = SynthesisAttr(A, hasInterfaceWrapper);
    Parm->addAttr(llvm::Attribute::get(Ctx, Name, Attr));

    // check whether set s_axilite interface for maxi slave mode
    // add default s_axilite for maxi slave mode
    StringRef OffsetMode = FPGAAddressInterfaceAttr::ConvertOffsetModeTypeToStr(
        A->getOffsetMode());
    if (OffsetMode.equals_lower("slave") && !hasInterfaceWrapper) {
      // add "fpga.interface.wrapper"="s_axilite.0" for parmdecl
      Parm->addAttr(
          llvm::Attribute::get(Ctx, "fpga.interface.wrapper", "s_axilite.0"));
      // add !fpga.adaptor.saxi.control for function
      llvm::MDBuilder MDB(Ctx);
      auto *Fn = Parm->getParent();
      const StringRef SAXILite = "fpga.adaptor.saxi.0";
      if (Fn->getMetadata(SAXILite) == nullptr) {
        auto *Node = llvm::MDNode::get(Ctx, {MDB.createString("")});
        Parm->getParent()->addMetadata(SAXILite, *Node);
      }
    }
  }

    /*
    DeclRefExpr* parm = DeclRefExpr::Create(
                              getContext(), 
                              NestedNameSpecificerLoc(), 
                              QualifierLoc(), 
                              D, 
                              false, 
                              D->getLocation(),  
                              D->getType(),
                              VK_LValue);
    Create(const ASTContext &Context, NestedNameSpecifierLoc, QualifierLoc,
         SourceLocation TemplateKWLoc, ValueDecl *D,
         bool RefersToEnclosingVariableOrCapture, SourceLocation NameLoc,
         QualType T, ExprValueKind VK, NamedDecl *FoundD = nullptr,
    */

  // Return nullptr if we didn't annotated anything
  if (ScopeAttrs.empty())
    return;

  auto lv = MakeAddrLValue(GetAddrOfLocalVar(D), D->getType());

  auto *SSACopy = llvm::Intrinsic::getDeclaration(
      CurFn->getParent(), llvm::Intrinsic::ssa_copy, Parm->getType());

  auto *Copy = Builder.CreateCall(SSACopy, Parm, ScopeAttrs, Parm->getName());
  EmitStoreOfScalar(Copy, lv);
}


void CodeGenFunction::EmitXlxFunctionAttributes(const FunctionDecl *FD,
                                                llvm::Function *Fn) {
  if (getLangOpts().CPlusPlus)
    if (auto *NameII = FD->getIdentifier())
      Fn->addFnAttr("fpga.demangled.name", NameII->getName());
  
  if (!FD->hasAttrs())
    return ; 
  auto &Ctx = getLLVMContext(); 
  llvm::MDBuilder MDB(Ctx);

  SmallVector<Attr *, 4> resultAttrs; 
  //check resultAttrs after 'HLSIFcond evaluation' , 
  //if there is conflict and redundant pragma, report warning message , and ignore one 
  bool find_dataflow = false;
  bool find_pipeline = false;
  bool find_inline = false;
  bool find_performance = false;
  for ( Attr* A: FD->getAttrs()) { 
    bool HLSIfCondRet = EvaluateHLSIFCond(A->getHLSIfCond()); 
    if (HLSIfCondRet) { 
      if (isa<AlwaysInlineAttr>(A) || isa<NoInlineAttr>(A)) { 
        if (find_inline) { 
          CGM.getDiags().Report(A->getLocation(), diag::warn_xlx_ignore_duplicate_pragma)
            << "inline" << "function" ; 
          continue; 
        }
        find_inline = true; 
      }
      else if(isa<XCLDataFlowAttr>(A)) { 
        if (find_dataflow) { 
          CGM.getDiags().Report(A->getLocation(), diag::warn_xlx_ignore_duplicate_pragma)
            << "dataflow" << "function" ; 
          continue; 
        }
        find_dataflow = true; 
      }
      else if (isa<XlxPipelineAttr>(A)) { 
        if (find_pipeline) { 
          CGM.getDiags().Report(A->getLocation(), diag::warn_xlx_ignore_duplicate_pragma)
            << "pipeline" << "function" ; 
          continue; 
        }
        find_pipeline = true; 

      }
      else if (isa<XlxPerformanceAttr>(A)) { 
        if (find_performance) { 
          CGM.getDiags().Report(A->getLocation(), diag::warn_xlx_ignore_duplicate_pragma)
            << "performance" << "function" ; 
          continue; 
        }
        find_performance = true; 
      }
      resultAttrs.push_back(A); 
    }
  }

  AttrVec &attrs = const_cast<AttrVec &>(FD->getAttrs()); 
  attrs.clear();
  attrs.append(resultAttrs.begin(), resultAttrs.end()); 


  SmallVector<llvm::Metadata *, 4> Args;
  if (auto DownwardInline = FD->getAttr<XCLInlineAttr>()) {
    auto mode = DownwardInline->getRecursive();
    if (mode == 0)
      Fn->addFnAttr("fpga.region.inline");
    else if (mode == 1)
      Fn->addFnAttr("fpga.recursive.inline");
    else if (mode == 2)
      Fn->addFnAttr("fpga.region.inline.off");
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(DownwardInline->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.inline"), MDB.createString(getPragmaContext(DownwardInline)), Loc.getAsMDNode()}));
  }

  if (auto inlineAttr =  FD->getAttr<AlwaysInlineAttr>()) { 
    //becuase , 'alwaysInline' is clang native  attribute, so clang will insert 'AlwaysAttr' in to llvm ir automatically 
    //Fn->addFnAttr(llvm::Attribute::AlwaysInline);
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(inlineAttr->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.inline"), MDB.createString(getPragmaContext(inlineAttr)), Loc.getAsMDNode()}));
  }

  if (auto NoInline = FD->getAttr<NoInlineAttr>()) { 
    //becuase , 'noInline' is clang native  attribute, so clang will insert 'noinline' in to llvm ir automatically 
    //Fn->addFnAttr(llvm::Attribute::NoInlineAttr); 
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(NoInline->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.inline"), MDB.createString(getPragmaContext(NoInline)), Loc.getAsMDNode()}));
  }



  if (auto *A = FD->getAttr<XCLDataFlowAttr>()) {
    Fn->addFnAttr("fpga.dataflow.func", std::to_string(A->getPropagation()));
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(A->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.dataflow.func"), MDB.createString(getPragmaContext(A)), Loc.getAsMDNode()}));
  }

  // according Xlnx's document, OpenCL 's xcl_pipeline_workitems can not apply
  // on function, but , there are some XCL case using  XCLPipelineWorkItems on
  // function , so handle it here,  TODO,  clean it in future;
  if (auto *A = FD->getAttr<XCLPipelineWorkitemsAttr>()) {
    int II = ExtractAttrInteger(A, A->getII(), CGM.getDiags(), getContext(),
                                /*LB*/ -1, /*UB*/ INT32_MAX, /*Default*/ -1);

    std::string args = std::to_string(II) + "." + std::to_string(0);

    Fn->addFnAttr("fpga.static.pipeline", args);
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(A->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.static.pipeline"), MDB.createString(getPragmaContext(A)), Loc.getAsMDNode()}));
  }

  if (auto *A = FD->getAttr<XlxPipelineAttr>()) {
    int II = ExtractAttrInteger(A, A->getII(), CGM.getDiags(), getContext(),
                                /*LB*/ -1, /*UB*/ INT32_MAX, /*Default*/ -1);
    int Style = A->getStyle();

    std::string args = std::to_string(II) + "." + std::to_string(Style);

    Fn->addFnAttr("fpga.static.pipeline", args);
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(A->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.static.pipeline"), MDB.createString(getPragmaContext(A)), Loc.getAsMDNode()}));
  }

  if (auto *A = FD->getAttr<XlxExprBalanceAttr>()) {
    Fn->addFnAttr("fpga.exprbalance.func", std::to_string(A->getEnabled()));
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(A->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.exprbalance.func"), MDB.createString(getPragmaContext(A)), Loc.getAsMDNode()}));
  }

  if (auto *A = FD->getAttr<XlxMergeLoopAttr>()) {
    Fn->addFnAttr("fpga.mergeloop", std::to_string(A->getForce()));
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(A->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.mergeloop"), MDB.createString(getPragmaContext(A)), Loc.getAsMDNode()}));
  }

  if (auto *A = FD->getAttr<SDxKernelAttr>()) {
    Fn->addFnAttr("fpga.top.func", A->getRTLName());
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(A->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.top"), MDB.createString(getPragmaContext(A)), Loc.getAsMDNode()}));
  }


  for (auto *A : FD->specific_attrs<XCLLatencyAttr>()) {
    int Min = EvaluateInteger(A->getMin(), getContext(), /*Default*/ 0);
    int Max = EvaluateInteger(A->getMax(), getContext(), /*Default*/ 65535);
    Fn->addFnAttr("fpga.latency",
                  std::to_string(Min) + "." + std::to_string(Max));

    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(A->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("fpga.latency"), MDB.createString(getPragmaContext(A)), Loc.getAsMDNode()}));
  }

  for (auto *A : FD->specific_attrs<HLSPreserveAttr>()) {
    Fn->addFnAttr("hls_preserve");
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(A->getLocation());
    Args.push_back(llvm::MDNode::get(Ctx, {MDB.createString("hls_preserve"), MDB.createString(getPragmaContext(A)), Loc.getAsMDNode()}));
  }

   
  if(!Args.empty()) {
    Fn->addMetadata("fpga.function.pragma", *llvm::MDNode::get(Ctx, Args));
  }
  if (auto *A = FD->getAttr<SDxKernelAttr>()) {
    for( auto *param : FD->parameters()) { 
      QualType param_type = param->getType(); 
      if (IsHLSStreamOfBlocksType(param_type)) { 
        CGM.getDiags().Report(param->getLocation(), diag::err_xlx_not_supported_by_scout_HLS) 
        << "the argument of hls::stream_of_blocks type in 'top' function "; 
      }
    }
  }
}

static Expr *StripImplicitCast(Expr *FE) {
  if (auto *Cast = dyn_cast<ImplicitCastExpr>(FE))
    return Cast->getSubExpr();

  return FE;
}

static DeclRefExpr *extractDeclRef(Expr *FE) {
  FE = StripImplicitCast(FE); 

  if (auto *array = dyn_cast<ArraySubscriptExpr>(FE)) { 
    Expr * base = array->getBase(); 
    base = StripImplicitCast(base); 
    return extractDeclRef(base);
  }
  if (isa<DeclRefExpr>(FE)){ 
    return cast<DeclRefExpr>(FE); 
  }
  return nullptr; 
}


void CodeGenFunction::LowerBindOpScope(
    Stmt *&stmt, std::map<const XlxBindOpAttr *, bool> &BindOpAttrs) {

  switch (stmt->getStmtClass()) {
  case Stmt::NullStmtClass: {
    break;
  }
#define STMT(Type, Base)
#define ABSTRACT_STMT(Op)
#define EXPR(Type, Base) case Stmt::Type##Class:
#include "clang/AST/StmtNodes.inc"
    {

      //this is redundant sanity check, because above macro expand can ensure 
      //only hanlde 'stmt' that is 'expr' 
      if (!isa<Expr>(stmt)){ 
        break; 
      }
      Expr *expr = cast<Expr>(stmt); 
      if (isa<ExprWithCleanups>(expr)) { 
        expr = cast<ExprWithCleanups>(expr)->getSubExpr(); 
      }

      //check LHS of 'assignment expr' 
      DeclRefExpr *assignmentDeclRef = nullptr; 
      if (isa<BinaryOperator>(expr) && dyn_cast<BinaryOperator>(expr)->isAssignmentOp()){
        assignmentDeclRef = extractDeclRef(cast<BinaryOperator>(expr)->getLHS()); 
      }
      else if(isa<CXXOperatorCallExpr>(expr) && cast<CXXOperatorCallExpr>(expr)->isAssignmentOp()){ 
        assignmentDeclRef = extractDeclRef(cast<CXXOperatorCallExpr>(expr)->getArg(0)); 
      }

      SmallVector<const Attr *, 4> attrs;
      for (auto &kv : BindOpAttrs) {
        const XlxBindOpAttr* attr = kv.first; 
        auto var_expr = attr->getVariable();
        auto var_ref = dyn_cast<DeclRefExpr>(var_expr);

        if (assignmentDeclRef && assignmentDeclRef->getDecl() == var_ref->getDecl()) {
          auto new_attr = XlxBindOpExprAttr::CreateImplicit(
              getContext(), attr->getVariable(), attr->getOp(), attr->getImpl(),
              attr->getLatency(), attr->getRange());
          attrs.push_back(new_attr);
          kv.second = true; 
        }
      }
      if (attrs.size()) {

        stmt = AttributedStmt::Create(getContext(), stmt->getLocStart(), attrs,
                                      stmt);
      }
      break;
    }
  case Stmt::CompoundStmtClass: {
    auto *compound = dyn_cast<CompoundStmt>(stmt);
    for (auto &iter : compound->body()) {
      LowerBindOpScope(iter, BindOpAttrs);
    }
    break;
  }
  case Stmt::DeclStmtClass: {
    auto *declStmt = dyn_cast<DeclStmt>(stmt);
    auto declGroup = declStmt->getDeclGroup();
    for (auto decl : declGroup) { 
      if(!isa<VarDecl>(decl)) { 
        continue;
      }
      auto var_decl = dyn_cast<VarDecl>(decl);
      auto decl_init = var_decl->getInit();
      //skip check, if there is no initialization expr
      if (!decl_init)
        continue;

      if (isTrivialInitializer(decl_init) || isNoCtorCXXConstructExpr(decl_init, *var_decl))
        continue;

      /* for following decl : 
      ap_fixed a = b + c
    will produce following  AST tree, we need scan the "CXXConstructor" 
                     
  | | `-VarDecl 0xaf309a8 <col:5, col:23> col:14 used kk 'ap_fixed' cinit
  | |   `-ExprWithCleanups 0xaf30d78 <col:19, col:23> 'ap_fixed'
  | |     `-CXXConstructExpr 0xaf30d40 <col:19, col:23> 'ap_fixed' 'void (ap_fixed &&) noexcept' elidable
  | |       `-MaterializeTemporaryExpr 0xaf30d28 <col:19, col:23> 'ap_fixed' xvalue
  | |         `-CXXOperatorCallExpr 0xaf30ce0 <col:19, col:23> 'ap_fixed'
  | |           |-ImplicitCastExpr 0xaf30cc8 <col:21> 'ap_fixed (*)(const ap_fixed)' <FunctionToPointerDecay>
  | |           | `-DeclRefExpr 0xaf30c40 <col:21> 'ap_fixed (const ap_fixed)' lvalue CXXMethod 0xaf14700 'operator+' 'ap_fixed (const ap_fixed)'
  | |           |-DeclRefExpr 0xaf30a08 <col:19> 'ap_fixed' lvalue Var 0xaf30728 'm' 'ap_fixed'
  | |           `-CXXConstructExpr 0xaf30c08 <col:23> 'ap_fixed' 'void (const ap_fixed &) noexcept'
  | |             `-ImplicitCastExpr 0xaf30a58 <col:23> 'const ap_fixed' lvalue <NoOp>
  | |               `-DeclRefExpr 0xaf30a30 <col:23> 'ap_fixed' lvalue Var 0xaf30868 'n' 'ap_fixed'
  | |-NullStmt 0xaf30e50 <line:17:5>

      if (const ExprWithCleanups *Cleanup = dyn_cast<ExprWithCleanups>(decl_init))
        if(isa<CXXConstructExpr>(Cleanup->getSubExpr())) continue;
      */

      for (auto &kv: BindOpAttrs) { 
        auto bind_op = kv.first; 
        auto var_expr = bind_op->getVariable();
        if (!isa<DeclRefExpr>(var_expr)) { 
          //the variable for bind_op is possible  MemberExpr, skip check it
          continue;
        }
        auto var_ref = cast<DeclRefExpr>(var_expr);

        if (decl == var_ref->getDecl() && 
            !var_decl->isDirectInit()) {
          auto new_attr = XlxBindOpExprAttr::CreateImplicit(
              getContext(), bind_op->getVariable(), bind_op->getOp(), bind_op->getImpl(),
              bind_op->getLatency(), bind_op->getRange());
          decl->addAttr(new_attr);
          kv.second = true; 
        }
      }
    }
    break;
  }
  case Stmt::LabelStmtClass: {
    LabelStmt *labelStmt = dyn_cast<LabelStmt>(stmt);
    Stmt *subStmt = labelStmt->getSubStmt();
    LowerBindOpScope(subStmt, BindOpAttrs);
    labelStmt->setSubStmt(subStmt);
    break;
  }
  case Stmt::AttributedStmtClass: {
    AttributedStmt *attributedStmt = dyn_cast<AttributedStmt>(stmt);
    Stmt *subStmt = attributedStmt->getSubStmt();
    auto attrs = attributedStmt->getAttrs();
    for (auto attr : attrs) {
      if (isa<XlxBindOpAttr>(attr)) {
        auto bindOp = dyn_cast<XlxBindOpAttr>(attr);
        BindOpAttrs[bindOp] = false; 
      }
    }
    LowerBindOpScope(subStmt, BindOpAttrs);
    for(auto kv: BindOpAttrs){ 
      if (!kv.second){ 
        for( auto attr: attrs){ 
          if (isa<XlxBindOpAttr>(attr) && cast<XlxBindOpAttr>(attr) == kv.first){ 
            //failed locate the assign expression for the lower bind_op, report warning message 
            Expr *var_ref = kv.first->getVariable(); 
            std::string var_name ; 
            llvm::raw_string_ostream os(var_name); 
            var_ref->printPretty(os, nullptr, PrintingPolicy(getContext().getLangOpts()) ); 
            CGM.getDiags().Report(kv.first->getLocation(), 
              diag::warn_xlx_bind_op_failed)  << os.str();
            break;
          }
        }
      }
    }
    for(auto attr: attrs){ 
      if (isa<XlxBindOpAttr>(attr)){ 
        BindOpAttrs.erase(cast<XlxBindOpAttr>(attr)); 
      }
    }
    attributedStmt->setSubStmt(subStmt);
    break;
  }
  case Stmt::DefaultStmtClass: {
    auto caseStmt = dyn_cast<DefaultStmt>(stmt);
    auto subStmt = caseStmt->getSubStmt();
    LowerBindOpScope(subStmt, BindOpAttrs);
    caseStmt->setSubStmt(subStmt);
    break;
  }
  case Stmt::CaseStmtClass: {
    auto caseStmt = dyn_cast<CaseStmt>(stmt);
    auto subStmt = caseStmt->getSubStmt();
    LowerBindOpScope(subStmt, BindOpAttrs);
    caseStmt->setSubStmt(subStmt);
    break;
  }
  case Stmt::SwitchStmtClass: { 
    auto switchStmt = dyn_cast<SwitchStmt>(stmt);
    auto body = switchStmt->getBody();
    LowerBindOpScope(body, BindOpAttrs);
    switchStmt->setBody(body);
    break;
  }
  case Stmt::IfStmtClass: {
    auto ifStmt = dyn_cast<IfStmt>(stmt);
    auto thenStmt = ifStmt->getThen();
    auto elseStmt = ifStmt->getElse();
    if (thenStmt) {
      LowerBindOpScope(thenStmt, BindOpAttrs);
      ifStmt->setThen(thenStmt);
    }

    if (elseStmt) {
      LowerBindOpScope(elseStmt, BindOpAttrs);
      ifStmt->setElse(elseStmt);
    }
    break;
  }
  case Stmt::WhileStmtClass: {
    auto whileStmt = dyn_cast<WhileStmt>(stmt);
    auto body = whileStmt->getBody();
    LowerBindOpScope(body, BindOpAttrs);
    whileStmt->setBody(body);
    break;
  }
  case Stmt::DoStmtClass: {
    auto doStmt = dyn_cast<DoStmt>(stmt);
    auto body = doStmt->getBody();
    LowerBindOpScope(body, BindOpAttrs);
    break;
  }
  case Stmt::ForStmtClass: {
    auto forStmt = dyn_cast<ForStmt>(stmt);
    Stmt *body = forStmt->getBody();
    LowerBindOpScope(body, BindOpAttrs);
    forStmt->setBody(body);
    break;
  }
  case Stmt::CXXForRangeStmtClass: { 
    auto forStmt = dyn_cast<CXXForRangeStmt>(stmt);
    Stmt *body = forStmt->getBody();
    LowerBindOpScope(body, BindOpAttrs);
    forStmt->setBody(body);
    break;
  }
  default:
    break;
  }
}

static Expr * GetBaseExpr( Expr* E) 
{
  Expr *base = E;
  while (auto *Cast = dyn_cast<ImplicitCastExpr>(base)){
    base = Cast->getSubExpr();
  }
  //for expr, var.filed1.field2[idx].field3
  while (1) { 
    if (isa<MemberExpr>(base)) { 
      base = cast<MemberExpr>(base)->getBase();
    }
    else if (isa<ArraySubscriptExpr>(base)) { 
      base = cast<ArraySubscriptExpr>(base)->getBase();
    }
    else if (isa<ImplicitCastExpr>(base)) { 
      base = cast<ImplicitCastExpr>(base)->getSubExpr();
    }
    else if (isa<DeclRefExpr>(base)) { 
      return base;
    }
    else if (isa<CXXThisExpr>(base)){
      return base;
    }
    else { 
      return nullptr;
    }
  }
  return nullptr;
}


void CodeGenFunction::EmitBundleForScope(
    const Stmt *SubStmt, ArrayRef<const Attr *> Attrs,
    SmallVectorImpl<llvm::OperandBundleDef> &DirectiveBundleList,
    SmallVectorImpl<llvm::MDNode*> &DirectivePragmaLocs,
    SmallVectorImpl<llvm::OperandBundleDef> &HintBundleList,
    SmallVectorImpl<llvm::MDNode*> &HintPragmaLocs) {
  // auto &Ctx = Builder.getContext();
  SmallVector<llvm::Value*, 4> ComputeRegionPtrs;
  SourceLocation ComputeRegionLoc; 
  if (isa<NullStmt>(SubStmt)) { 
    return ;
  }

  llvm::MDBuilder MDB(getLLVMContext());

  StringRef computeRegionPragmaContext ; 

  for (auto *A : Attrs) {
    if (!EvaluateHLSIFCond(A->getHLSIfCond())) { 
      continue; 
    }

    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(A->getLocation());
    llvm::MDNode* MDN =
        llvm::MDNode::get(getLLVMContext(),
            {MDB.createString(A->getSpelling()),
             MDB.createString(getPragmaContext(A)), Loc.get()});
    switch (A->getKind()) {
    default:
      break;
    case attr::XCLSingleWorkitem:{
      DirectiveBundleList.emplace_back(A->getSpelling(), None);
      DirectivePragmaLocs.emplace_back(MDN);
    }
      break;
    case attr::XCLPipelineWorkitems: {
      // xilinx "xcl_pipeline_workitems" support workitems
      int II = ExtractAttrInteger(A, cast<XCLPipelineWorkitemsAttr>(A)->getII(),
                                  CGM.getDiags(), getContext(), /*LB*/ -1,
                                  /*UB*/ INT32_MAX, /*Default*/ -1);

      DirectiveBundleList.emplace_back(A->getSpelling(), Builder.getInt32(II));
      DirectivePragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XCLUnrollWorkitems:{
      DirectiveBundleList.emplace_back(
          A->getSpelling(),
          Builder.getInt32(cast<XCLUnrollWorkitemsAttr>(A)->getUnrollHint()));
      DirectivePragmaLocs.emplace_back(MDN);
    }
      break;
    case attr::XCLArrayView: {
      auto *View = cast<XCLArrayViewAttr>(A);
      auto LV = EmitLValue(View->getArray());
      SmallVector<llvm::Value *, 4> Shape;
      for (auto *E : View->shape())
        Shape.push_back(EmitScalarExpr(E));

      auto ArrayPtr = EmitLoadOfLValue(LV, SourceLocation());
      SmallVector<llvm::OperandBundleDef, 4> ScopeAttrs;
      ScopeAttrs.emplace_back(View->getSpelling(), Shape);
      switch (View->getAccessMode()) {
      case XCLArrayViewAttr::Readonly:
        ScopeAttrs.emplace_back("xcl_read_only", None);
        break;
      case XCLArrayViewAttr::Writeonly:
        ScopeAttrs.emplace_back("xcl_write_only", None);
        break;
      case XCLArrayViewAttr::ReadWrite:
        break;
      }

      auto *V = ArrayPtr.getScalarVal();
      auto *SSACopy = llvm::Intrinsic::getDeclaration(
          CurFn->getParent(), llvm::Intrinsic::ssa_copy, V->getType());

      auto *Copy = Builder.CreateCall(SSACopy, V, ScopeAttrs, V->getName());
      ComputeRegionPtrs.push_back(Copy);
      ComputeRegionLoc = A->getLocation();
      computeRegionPragmaContext = getPragmaContext(A); 
      break;
    }
    case attr::XCLOutline: {
      // auto Name = cast<XCLOutlineAttr>(A)->getName();
      // auto *Str = llvm::MDString::get(Ctx, Name);
      DirectiveBundleList.emplace_back(A->getSpelling(), None);
      DirectivePragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XCLInline: {
      auto Recursive = cast<XCLInlineAttr>(A)->getRecursive();
      DirectiveBundleList.emplace_back(A->getSpelling(), Builder.getInt32(Recursive));
      DirectivePragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XlxExprBalance: {
      auto Enabled = cast<XlxExprBalanceAttr>(A)->getEnabled();
      HintBundleList.emplace_back(A->getSpelling(), Builder.getInt32(Enabled));
      HintPragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XlxMergeLoop: {
      auto Force = cast<XlxMergeLoopAttr>(A)->getForce();
      HintBundleList.emplace_back(A->getSpelling(), Builder.getInt32(Force));
      HintPragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::FPGAResourceLimitHint: {
      auto resourceLimit = dyn_cast<FPGAResourceLimitHintAttr>(A);
      auto Limit = resourceLimit->getLimit();
      auto LimitInt = EvaluateInteger(Limit, getContext(), /*Default*/ 0);
      auto InstanceType = resourceLimit->getInstanceType()->getName();
      auto Name = resourceLimit->getInstanceName()->getName();
      llvm::Value *Args[] = {llvm::ConstantDataArray::getString(getLLVMContext(),Name),
                             llvm::ConstantDataArray::getString(getLLVMContext(), InstanceType),
                             Builder.getInt32(LimitInt)};
      SmallVector<llvm::OperandBundleDef, 1> bundleDefs;
      HintBundleList.emplace_back(A->getSpelling(), Args);
      HintPragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XlxOccurrence: {
      auto Cycle = cast<XlxOccurrenceAttr>(A)->getCycle();
      auto CycleInt = EvaluateInteger(Cycle, getContext(), /*Default*/ 1);
      HintBundleList.emplace_back(A->getSpelling(), Builder.getInt32(CycleInt));
      HintPragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XlxProtocol: {
      DirectiveBundleList.emplace_back(
          A->getSpelling(),
          Builder.getInt32(cast<XlxProtocolAttr>(A)->getProtocolMode()));
      DirectivePragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XCLLatency: {
      int Min = EvaluateInteger(cast<XCLLatencyAttr>(A)->getMin(), getContext(),
                                /*Default*/ 0);
      int Max = EvaluateInteger(cast<XCLLatencyAttr>(A)->getMax(), getContext(),
                                /*Default*/ 65535);

      llvm::Value *LatencyArray[] = {Builder.getInt32(Min),
                                     Builder.getInt32(Max)
      };

      HintBundleList.emplace_back(A->getSpelling(), LatencyArray);
      HintPragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XlxPerformance: {

      const XlxPerformanceAttr *PerformanceAttr = cast<XlxPerformanceAttr>(A);
      bool isLoop =
          PerformanceAttr->getPerformanceScope() == XlxPerformanceAttr::Loop;

      bool isSec = PerformanceAttr->getUnit() == XlxPerformanceAttr::Seconds; 
      if (getLangOpts().HLSClockPeriod == 0 && isSec){ 
        CGM.getDiags().Report(PerformanceAttr->getLocation(),
          diag::err_xlx_clock_period_is_invalid);
      }
      
      int64_t TargetTI = HLSEvaluateClockCycle(PerformanceAttr->getTargetTI(), isSec, getLangOpts().HLSClockPeriod, "target_ti", CGM, getContext());
      if (TargetTI < 0)
        break;

      int64_t TargetTL = HLSEvaluateClockCycle(
          PerformanceAttr->getTargetTL(), isSec, getLangOpts().HLSClockPeriod, "target_tl", CGM, getContext());
      if (TargetTL < 0)
        break;
      int64_t AssumeTI = HLSEvaluateClockCycle(
          PerformanceAttr->getAssumeTI(), isSec, getLangOpts().HLSClockPeriod, "assume_ti", CGM, getContext());
      if (AssumeTI < 0)
        break;
      int64_t AssumeTL = HLSEvaluateClockCycle(
          PerformanceAttr->getAssumeTL(), isSec, getLangOpts().HLSClockPeriod, "assume_tl", CGM, getContext());
      if (AssumeTL < 0)
        break;

      llvm::Value *Args[] = {Builder.getInt32(isLoop),
                             Builder.getInt64(TargetTI),
                             Builder.getInt64(TargetTL),
                             Builder.getInt64(AssumeTI),
                             Builder.getInt64(AssumeTL),
                             Builder.getInt32(0) /// 0 means for cycle
                            };

      HintBundleList.emplace_back(A->getSpelling(), Args);
      HintPragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XlxBindOpExpr: {
      auto *bindOp = dyn_cast<XlxBindOpExprAttr>(A);

      auto var_expr = bindOp->getVariable();
      auto op_enum = (platform::PlatformBasic::OP_TYPE)EvaluateInteger(
                                      bindOp->getOp(), getContext(), -1);
      auto impl_enum = (platform::PlatformBasic::IMPL_TYPE)EvaluateInteger(
                                      bindOp->getImpl(), getContext(), -1);

      // strip null terminator
      auto latency =
            EvaluateInteger(bindOp->getLatency(), getContext(), /*Default*/ -1);

      // TODO, use  enum encoding insteading of "string"

      llvm::Value *args[] = {Builder.getInt32(op_enum),
                         Builder.getInt32(impl_enum),
                         Builder.getInt32(latency),
                       };

      HintBundleList.emplace_back("fpga_resource_hint", args);
      HintPragmaLocs.emplace_back(
          llvm::MDNode::get(getLLVMContext(),
                            {MDB.createString("fpga_resource_hint"),
                             MDB.createString(getPragmaContext(A)),
                             Loc.get()}));
      break;
    }
    case attr::XlxTask: {
      auto task = dyn_cast<XlxTaskAttr>(A);
      auto task_id = EmitScalarExpr(task->getTaskID());

      llvm::Value *args[] = {task_id};
      HintBundleList.emplace_back("xlx_task_def", args);
      HintPragmaLocs.emplace_back(MDN);
      break;
    }
    case attr::XlxInfiniteTask: { 
      auto task = dyn_cast<XlxInfiniteTaskAttr>(A);
      auto task_id = EmitScalarExpr(task->getTaskID());

      llvm::Value *args[] = {task_id};
      HintBundleList.emplace_back("xlx_infinite_task_def", args);
      HintPragmaLocs.emplace_back(MDN);
      break;
    }
    }
  }

  if (!ComputeRegionPtrs.empty()){ 
    DirectiveBundleList.emplace_back("fpga_compute_region", ComputeRegionPtrs);
    llvm::DebugLoc Loc  = PragmaSourceLocToDebugLoc(ComputeRegionLoc);
    DirectivePragmaLocs.push_back(llvm::MDNode::get(getLLVMContext(), {MDB.createString("fpga_compute_region"), 
          MDB.createString(computeRegionPragmaContext), Loc.get()}));
  }
}

Expr* EmitHLSVariableRecur( Expr *E, SmallVectorImpl<unsigned int> &idxs, std::string &name , CodeGenModule &CGM, ASTContext &context) 
{
  Expr *base = nullptr;
  if (isa<MemberExpr>(E)) { 
    base = EmitHLSVariableRecur(cast<MemberExpr>(E)->getBase(), idxs,  name, CGM, context);
    auto member = cast<MemberExpr>(E);
    ValueDecl * sub = member->getMemberDecl();
    if (cast<MemberExpr>(E)->getBase()->getType()->isPointerType()){ 
      idxs.push_back( 0 );
    }
    if (isa<FieldDecl>(sub)) { 
      int idx = cast<FieldDecl>(sub)->getFieldIndex();
      idxs.push_back(idx);
      name.append( "." ).append(sub->getName().str());
    } else if (isa<VarDecl>(sub)) {
      // field is static
      return E;
    }
    else{ 
      CGM.getDiags().Report(E->getExprLoc(),
        diag::warn_invalid_variable_expr);
      return E;
    }
  }
  else if (isa<ArraySubscriptExpr>(E)) { 
    base = EmitHLSVariableRecur(cast<ArraySubscriptExpr>(E)->getBase(), idxs,  name, CGM, context);
    Expr * idxSub = cast<ArraySubscriptExpr>(E)->getIdx();
    //TODO, we need evaluate and check const int after Sematic Action
    if (!idxSub->isEvaluatable(context)) {
      CGM.getDiags().Report(idxSub->getExprLoc(), diag::err_xlx_expr_not_ice); 
      return E; 
    }
    else { 
      llvm::APSInt Value = idxSub->EvaluateKnownConstInt(context);
      int idx = (int)Value.getSExtValue();
      idxs.push_back(idx); 
      name.append( "_" ).append( std::to_string(idx));
    }
  }
  else if (isa<ImplicitCastExpr>(E)) { 
    base = EmitHLSVariableRecur(cast<ImplicitCastExpr>(E)->getSubExpr(), idxs, name, CGM, context);
  }
  else if (isa<DeclRefExpr>(E)) {
    name.append(cast<DeclRefExpr>(E)->getDecl()->getName().str());
    return E; 
  }
  else if (isa<CXXThisExpr>(E)) { 
    name.append("this");
    return E;
  }
  else { 
    //TODO, should we error out for no-LValue expression ? 
    //=================================
    //int a[ 10];
    //int (*px)[10];
    //*px = &a;
    //#pragma HLS array_partition variable = *px
    //===================================
    //I thought above code is silly, should not be supported ,  but precommit contain such code 
#if 0
    CGM.getDiags().Report(E->getExprLoc(),
      diag::warn_invalid_variable_expr);
    return nullptr;
#endif 
    return E;
  }
  return base;
}


//there is following  special process for TopArgument: 
//1. HLS Calliing conversion define:  the top argument of struct type is passed by value
//2. array undecay for top argument  cause the outtest dimension size is missing , to let reflow pragma get happy , 
//   we need generate additional GEP for  global/local array 
//3. hls::burst_maxi is struct, and is passed by value , we need  populate the HLS variable to the inner field of maxi_burst
//4. some eror check : 
//  1.  top argument is about struct pointer , while the struct type contains field of pointer type , if curren tHLS 
//      pragma appplying on the point field as following show : 
//      struct SS {
//        int * x;
//        ....
//      }
//      top( SS * input) { 
//        #HLS pragma xxxx variable = input->x 
//      } 
//      we need error out it 
//       
//  2. HLS interface  pragma apply on top argument of struct type , error out TODO in semantic check phase 
//  3. HLS  interface pragma apply on no top-argument , error out( Semantic check phase has handled this?)
std::pair<llvm::Value*, int64_t>  CodeGenFunction::EmitHLSVariableExpr( Expr *expr ) 
{

  /*
   * for following  pragma 
   *
   * #pragma HLS stream variable = &strm depth= ...
   *
   * above code will generate CXXOperatorCallExpr for "&" operator 
   * CXXOperatorCallExpr 0x809f6c8 '<dependent type>'
   * |-UnresolvedLookupExpr 
   * `-DeclRefExpr 0x809f3c8 'hls::stream<typename StreamType<WORDWIDTH_SRC>::name>':'stream<typename StreamType<WORDWIDTH_SRC>::name>' lvalue Var 0x809f298 'strm' 
   *
   * after Template Instantiation, the "CXXOperatorCallExpr" will be rewrited and generate following 
   * UnaryOperator &
   *  DeclRefExpr  strm
   *
   *  so , here we need  to extract  subExpr from "UnaryOperator &" 
   */

  /// error out for VLA for now, CodeGen for VLA will get continued
  /// FIXME: This will crash in partiotion/ reshape for inserting the additional gep
  /// and will be fixed in 22.2 to cleanup the insertion of gep operation
  if (expr->getType()->isVariablyModifiedType()) {
    CGM.getDiags().Report(expr->getExprLoc(), diag::err_xlx_variable_length_array_type_unsupport);
    return std::make_pair(nullptr, -1);
  }
  
  if (isa<UnaryOperator>(expr) &&
      dyn_cast<UnaryOperator>(expr)->getOpcode() == UO_AddrOf) {
    expr = cast<UnaryOperator>(expr)->getSubExpr();
  }

  SmallVector<unsigned int, 4> idxs;
  std::string name("");
  Expr *base = EmitHLSVariableRecur(expr, idxs, name, CGM, getContext()) ;

  if (isa<DeclRefExpr>(base) && isa<ParmVarDecl>(cast<DeclRefExpr>(base)->getDecl())) { 
    ParmVarDecl *Parm = cast<ParmVarDecl>(cast<DeclRefExpr>(base)->getDecl());
    int idx = Parm->getFunctionScopeIndex();
    llvm::Argument *Args = CurFn->arg_begin();
    llvm::Value* V = nullptr;
    for (llvm::Argument &arg: CurFn->args()) { 
      if ( 0 == arg.getName().compare(Parm->getName())) { 
        V = &arg;
      }
    }
    assert( V && "can not find the corresponding llvm::Function::Argument ");
    

    if (idxs.size() == 0) {
      //it is some declref expr
      QualType originalType = Parm->getType();
      if (isa<DecayedType>(originalType)) { 
        originalType = cast<DecayedType>(originalType)->getOriginalType();
      }
      llvm::Type *llvm_type = V->getType();
      int64_t port_width = 0;
      if(originalType->isReferenceType()) { 
        originalType = cast<ReferenceType>(originalType)->getPointeeType();
      }
      //if the variable is function::Argument, and is pointer type, port_width is 0
      if (!originalType->isPointerType() ) { 
        llvm::Type *lv_type = ConvertType(originalType);
 
        // because we have changed the ConvertType()'s behaviour for array of incomplete struct type, so we must check for this
        while(lv_type->isArrayTy()) {
            lv_type = lv_type->getArrayElementType();
        }

        if ( isa<llvm::StructType>(lv_type) && cast<llvm::StructType>(lv_type)->isOpaque()) { 
          //TODO,  codegen  will delay the concrete type generate for the tempalte specilaization class
          //if current ParmVarDecl is about pointer type to template sepcialization 
          //the result of "ConvertType(roginalType)" is opaque llvm::StructType  , 
          //untile the field/method of specialization class if called , the opaque type is translated to 
          //concret type ,   
          //there are following  two solutions: 
          //1. create concret type for specialization class which is pointtee
          //2. create  array_size attribute that means how many objects are pointed by current pointer 
          //  eg:   call llvm.siedeeffect #3 [ xlx_pragma_intrinsic] ( [4 x i32] * p , ..... ) ; 
          //        attribute  #3  { ...., "array_size" = 16 } 
          //   means: 
          //        ponter "p" is pointing to "[16 x [ 4 x i32]]"
          //
          port_width = 0;
        }
        else { 
          port_width = CGM.getDataLayout().getTypeAllocSizeInBits(ConvertType(originalType));
        }
      }
      return std::make_pair(V, port_width);
    }
    else { 
      if (V->getType()->isPointerTy()) { 
        //TODO, check  pointer to pointer 
        llvm::Type * llvm_type = ConvertType(expr->getType());
        int64_t port_width = CGM.getDataLayout().getTypeAllocSizeInBits(llvm_type);
        SmallVector<llvm::Value*, 4> idx_values;

        if (!Parm->getType()->isPointerType()) { 
          idx_values.push_back( Builder.getInt32(0));
        }

        for(int i : idxs) { 
          idx_values.push_back(Builder.getInt32(i));
        }
        llvm::Value *port_V = Builder.CreateInBoundsGEP(V, makeArrayRef(idx_values));
#if 0
  llvm::dbgs() << " variable expr dump : \n" ;
  expr->dump();
  llvm::dbgs() << "base type: \n";
  Parm->getType()->getPoitnerElementType()->dump();
  llvm::dbgs() << "================> \n";
  port_V->dump();
  llvm::dbgs()<< "\n";
#endif 

        return std::make_pair(
            port_V, port_width);
      }
      else { 
        int64_t port_width = 0;
        //original type is not pointer type, so it can be pointer 
        if (expr->getType()->isPointerType()) { 
          llvm::Type *llvm_type = ConvertType(cast<PointerType>(expr->getType())->getPointeeType());
          port_width = CGM.getDataLayout().getTypeAllocSizeInBits(llvm_type);
        }
        else { 
          llvm::Type *llvm_type = ConvertType(expr->getType());
          port_width = CGM.getDataLayout().getTypeAllocSizeInBits(llvm_type);
        }
        return std::make_pair( Builder.CreateExtractValue(V, makeArrayRef(idxs), name), port_width );
      }
    }
  }
  else if (isa<CXXThisExpr>(base) && idxs.size() == 0) { 
    int64_t port_width = CGM.getDataLayout().getTypeAllocSizeInBits(CXXThisValue->getType()->getPointerElementType());
    return std::make_pair(CXXThisValue, port_width);
  }
  else {
    llvm::Value *V = nullptr;
    if (expr->getType()->isPointerType()){ 
      V = EmitScalarExpr(expr);
    }
    else { 
      LValue lv = EmitLValue(expr);
      V = lv.getPointer();
    }
    llvm::Type *llvm_type = V->getType();
    assert(isa<llvm::PointerType>(llvm_type) && "unexpected");
    llvm_type = llvm_type->getPointerElementType();
    int64_t port_width =
        llvm_type->isSized()
            ? CGM.getDataLayout().getTypeAllocSizeInBits(llvm_type)
            : 0;
    return std::make_pair(V, port_width);
  }
}

void CodeGenFunction::EmitStableIntrinsic(const XlxStableAttr *A) {
  llvm::Value *V = nullptr;
  Expr *expr = A->getVariable();
  std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;

  V = port_info.first;
  int64_t port_width = port_info.second;

  llvm::Value *args[] = {
    V
  };

  CreateCallSideEffect(CGM, Builder, "stable", args, port_width, getPragmaContext(A));
}

void CodeGenFunction::EmitStableContentIntrinsic(
    const XlxStableContentAttr *A) {
  llvm::Value *addr = nullptr;
  Expr *expr = A->getVariable();

  std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;


  int64_t port_width = port_info.second;
  addr = port_info.first;
  llvm::Value *args[] = {
      addr,
  };

  CreateCallSideEffect(CGM, Builder, "stable_content", args, port_width, getPragmaContext(A));
}

void CodeGenFunction::EmitSharedIntrinsic(const XlxSharedAttr *A) {
  llvm::Value *V = nullptr;
  Expr *expr = A->getVariable();

  std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;

  int64_t port_width = port_info.second;
  V = port_info.first;
  llvm::Value *Args[] = {V};

  CreateCallSideEffect(CGM, Builder, "shared", Args, port_width, getPragmaContext(A));
}

void CodeGenFunction::EmitBindStorageIntrinsic( const XlxBindStorageAttr *bindStorage) {

  Expr *expr = bindStorage->getVariable();
  llvm::Value* pointer = nullptr;
  std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;

  int64_t port_width = port_info.second;
  pointer = port_info.first;

  auto latency =
      EvaluateInteger(bindStorage->getLatency(), getContext(), /*Default*/ -1);
  auto impl = (platform::PlatformBasic::IMPL_TYPE)EvaluateInteger(
      bindStorage->getImpl(), getContext(), -1);

  llvm::Value *Args[] = {pointer,
                         Builder.getInt32(platform::PlatformBasic::OP_MEMORY),
                         Builder.getInt32(impl), Builder.getInt32(latency)};

  CreateCallSideEffect(CGM, Builder, "xlx_bind_storage", Args, port_width);
}

// TODO, need good name, applyReqdPipeDepthAttr ?
void CodeGenFunction::EmitReqdPipeDepthIntrinsic(
    const XlxReqdPipeDepthAttr *stream) {

  // it is about memory attribute, s
  // TODO, error out , if variable value  is not varRefExpr
  llvm::Value *V = nullptr;
  Expr *E = stream->getVariable();

  std::pair<llvm::Value*, uint64_t> port_info = EmitHLSVariableExpr(E);
  if (nullptr == port_info.first) return;

  V = port_info.first;
  int64_t port_width = port_info.second;

  auto Depth = EvaluateInteger(stream->getDepth(), getContext(), /*Default*/ 1);
  llvm::Value *Args1[] = {V, Builder.getInt32(Depth)};
  llvm::Value *Args2[] = {V, Builder.getInt32(Depth), Builder.getInt32(stream->getType())};

  if (stream->getType() > 0) {
    CreateCallSideEffect(CGM, Builder, "xcl_fpga_pipo_depth", Args2, port_width, getPragmaContext(stream));
  } else {
    CreateCallSideEffect(CGM, Builder, stream->getSpelling(), Args1, port_width, getPragmaContext(stream));
  }
}

#if 0
// xlx_array_partition, xlx_array_reshape
void CodeGenFunction::EmitArrayXFormIntrinsic(const XlxArrayXFormAttr *A) {

  Expr *expr = A->getVariable();
  std::pair<llvm::Value*, uint64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;
  
  llvm::Value *V = port_info.first;
  int64_t port_width = port_info.second;

  if (!isa<llvm::Argument>(V) && V->getType()->isPointerTy()) { 
    //TODO, we need delete this ugly code 
    llvm::Value *const0 = Builder.getInt32(0);
    llvm::Value *idxs[] = {const0, const0};
    V = Builder.CreateInBoundsGEP(V, idxs);
  }

  Expr *DimE = A->getDim();
  Expr *FactorE = A->getFactor();

  /*
    enum XlxArrayXFromType {
      Cyclic,
      Block,
      Complete
    };
  */
  XlxArrayXFormAttr::XlxArrayXFormType xtype = A->getType();

  llvm::Value *Args[] = {V, Builder.getInt32(xtype), EmitScalarExpr(FactorE),
                         EmitScalarExpr(DimE)};

  CreateCallSideEffect(CGM, Builder, A->getSpelling(), Args, port_width);
}
#endif

// xlx_array_partition
void CodeGenFunction::EmitArrayPartitionXFormIntrinsic(const XlxArrayPartitionXFormAttr *A) {

  Expr *expr = A->getVariable();
  std::pair<llvm::Value*, uint64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;
  
  llvm::Value *V = port_info.first;
  int64_t port_width = port_info.second;

  Expr *DimE = A->getDim();
  Expr *FactorE = A->getFactor();
  int factor = HLSEvaluateICE(FactorE, "factor" , "array_partition",  -1); 
  if (factor == -1 ) 
    return ; 

  int dim = HLSEvaluateICE(DimE, "dim", "array_partition", -1); 
  if (dim == -1)
    return; 
  /*
    enum XlxArrayXFromType {
      Cyclic,
      Block,
      Complete
    };
  */
  XlxArrayPartitionXFormAttr::XlxArrayPartitionXFormType xtype = A->getType();
  bool Dynamic = A->getDynamic();
  if (A->getOff()) {
    xtype = (XlxArrayPartitionXFormAttr::XlxArrayPartitionXFormType)999;
  }

  llvm::Value *Args[] = {V, Builder.getInt32(xtype), EmitScalarExpr(FactorE),
                         EmitScalarExpr(DimE), Builder.getInt1(Dynamic)};

  CreateCallSideEffect(CGM, Builder, A->getSpelling(), Args, port_width, getPragmaContext(A));
}

// xlx_array_reshape
void CodeGenFunction::EmitArrayReshapeXFormIntrinsic(const XlxArrayReshapeXFormAttr *A) {

  Expr *expr = A->getVariable();
  std::pair<llvm::Value*, uint64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;
  
  llvm::Value *V = port_info.first;
  int64_t port_width = port_info.second;

  Expr *DimE = A->getDim();
  Expr *FactorE = A->getFactor();

  int factor = HLSEvaluateICE(FactorE, "factor", "array_reshape",  -1); 
  if (factor == -1 ) 
    return ; 

  int dim = HLSEvaluateICE(DimE, "dim", "array_reshape", -1); 
  if (dim == -1)
    return; 
  /*
    enum XlxArrayXFromType {
      Cyclic,
      Block,
      Complete
    };
  */
  XlxArrayReshapeXFormAttr::XlxArrayReshapeXFormType xtype = A->getType();
  if (A->getOff()) {
    xtype = (XlxArrayReshapeXFormAttr::XlxArrayReshapeXFormType)999;
  }

  llvm::Value *Args[] = {V, Builder.getInt32(xtype), EmitScalarExpr(FactorE),
                         EmitScalarExpr(DimE)};

  CreateCallSideEffect(CGM, Builder, A->getSpelling(), Args, port_width, getPragmaContext(A));
}

//for TopFunction use CC_FPGAAccel ccalling conversion , this need 
//the 
void CodeGenFunction::EmitDisaggrIntrinsic(const XlxDisaggrAttr *A) {
  Expr *expr = A->getVariable();
  std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;
  
  llvm::Value *V = port_info.first;
  int64_t port_width = port_info.second;

  llvm::Value *Args[] = {V};
  CreateCallSideEffect(CGM, Builder, "disaggr", Args, port_width, getPragmaContext(A));
}

void CodeGenFunction::EmitAggregateIntrinsic(const XlxAggregateAttr *A) {
  Expr *expr = A->getVariable();
  std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;
  
  llvm::Value *V = port_info.first;
  int64_t port_width = port_info.second;

  auto compact = A->getCompact();

  llvm::Value * compact_v = Builder.getInt64(compact);
  llvm::Value *Args[] = {V, compact_v };
  CreateCallSideEffect(CGM, Builder, "aggregate", Args, port_width, getPragmaContext(A));
}
void CodeGenFunction::EmitDataPackIntrinsic(const XlxDataPackAttr* A) 
{
  Expr *expr = A->getVariable();
  std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(expr);
  if (nullptr == port_info.first) return;
  
  llvm::Value *V = port_info.first;
  int64_t port_width = port_info.second;

  int aggregate_compact = (int)XlxAggregateAttr::bit;
  auto level = A->getBytePadLevel();
  if (level == XlxDataPackAttr::field_level) { 
    //translate to "XlxAggregateAttr::byte"
    aggregate_compact = (int)XlxAggregateAttr::byte;
  }
  llvm::Value * level_v = Builder.getInt64(aggregate_compact);
  llvm::Value *Args[] = {V, level_v };

  CreateCallSideEffect(CGM, Builder, "aggregate", Args, port_width, getPragmaContext(A));
}

void CodeGenFunction::EmitHLSConstIntrinsic(llvm::Value *var) {
  llvm::Value *Args[] = {var};

  CreateCallSideEffect(CGM, Builder, "const", Args);
}

void CodeGenFunction::EmitXlxCrossDependenceIntrinsic(const XlxCrossDependenceAttr *attr) { 
  Expr * var0 = attr->getCrossVar0();
  Expr * var1 = attr->getCrossVar1();
  llvm::Value* addr0 = nullptr;
  llvm::Value* addr1 = nullptr;
  if( var0->getType()->isPointerType()){
    //pointer variable
    addr0 = EmitScalarExpr(var0);
  }
  else {
    //array variable, or struct with only one array field 
    LValue lvalue = EmitLValue(var0);
    addr0 = lvalue.getPointer();
  }

  if( var1->getType()->isPointerType()){
    //pointer variable
    addr1 = EmitScalarExpr(var1);
  }
  else {
    //array variable, or struct with only one array field 
    LValue lvalue = EmitLValue(var1);
    addr1 = lvalue.getPointer();
  }
  
  
  auto dep_class = attr->getXClass();
  auto dep_compel = attr->getCompel();
  auto dep_type = attr->getType();
  auto dep_direction = attr->getDirection();
  auto dep_distance = HLSEvaluateICE(attr->getDistance(), "distance", "dependence with cross_variables");

  llvm::Value* args[] = {
       addr0,
       addr1,
       Builder.getInt32(dep_class),
       Builder.getInt32(dep_compel), 
       Builder.getInt32(dep_direction -1 ), 
       Builder.getInt32(dep_distance),
       Builder.getInt32(dep_type)
  };

  assert(&(CGM.getModule())==CurFn->getParent() && "unexpected");
  CreateCallSideEffect(CGM, Builder, "fpga.cross.dependence", args, -1, getPragmaContext(attr));
}

// this is step 2 , will be commit after step1 that is  clang parser part
void CodeGenFunction::EmitResourceLimitIntrinsic(const FPGAResourceLimitHintAttr* resourceLimit) 
{
  auto Limit = resourceLimit->getLimit();
  auto LimitInt = HLSEvaluateICE(Limit, "limit", "allocation",  /*Default*/ 0);
  auto InstanceType = resourceLimit->getInstanceType()->getName();
  auto Name = resourceLimit->getInstanceName()->getName();
  llvm::Value *Args[] = {llvm::ConstantDataArray::getString(getLLVMContext(), Name),
                         llvm::ConstantDataArray::getString(getLLVMContext(), InstanceType),
                         Builder.getInt32(LimitInt)

  };
  CreateCallSideEffect(CGM, Builder, resourceLimit->getSpelling(), Args, -1, getPragmaContext(resourceLimit));
}

void CodeGenFunction::EmitFunctionAllocationIntrinsic(const XlxFunctionAllocationAttr* functionAlloc) 
{
  auto func_pointer = functionAlloc->getFunction();
  if (isa<UnaryOperator>(func_pointer) &&
        dyn_cast<UnaryOperator>(func_pointer)->getOpcode() == UO_AddrOf) {
      UnaryOperator *up = dyn_cast<UnaryOperator>(func_pointer);
      func_pointer = up->getSubExpr();
  }

  FunctionDecl * FD = nullptr;
  if (isa<DeclRefExpr>(func_pointer)) {
    auto value_decl = dyn_cast<DeclRefExpr>(func_pointer)->getDecl();
    assert(isa<FunctionDecl>(value_decl) && "unexpected,  Semantic check should have checked the Decl type");
    FD = dyn_cast<FunctionDecl>(value_decl);
  }
  else if(isa<MemberExpr>(func_pointer)){
    auto member_func = dyn_cast<MemberExpr>(func_pointer);
    NamedDecl *ND = member_func->getMemberDecl();
    assert(isa<FunctionDecl>(ND) && "unexpected, no member function ");
    FD = dyn_cast<FunctionDecl>(ND);
  }
  else { 
    CGM.getDiags().Report(func_pointer->getLocStart(),
           diag::warn_xlx_attribute_ignore_because_invalid_option)
        << "ALLOCATION"
        << "Instances value is not valid function pointer expression";
    return ;
  }
  assert( FD && "unexpected, function allocation 's instances option is not an function pointer");
  if (FD->getAttr<AlwaysInlineAttr>()) { 
    CGM.getDiags().Report(functionAlloc->getLocation(),
                          diag::warn_allocation_conflict)
        << "inline" ;
    return;
  }

  auto Limit = functionAlloc->getLimit();
  auto LimitInt = HLSEvaluateICE(Limit, "limit", "allocation", /*Default*/ 0);

  assert(FD && "unexpected, instances options is not function");

  llvm::Value* llvm_func = CGM.GetAddrOfGlobal(FD);


  llvm::Value *Args[] = { llvm_func,
                         llvm::ConstantDataArray::getString(getLLVMContext(), "function"),
                         Builder.getInt32(LimitInt) 
  };
  CreateCallSideEffect(CGM, Builder, functionAlloc->getSpelling(), Args, -1, getPragmaContext(functionAlloc));
}

static bool IsFunctionPointer(Expr* port ) 
{
  FunctionDecl * FD = nullptr;
  if (isa<DeclRefExpr>(port)) {
    auto value_decl = dyn_cast<DeclRefExpr>(port)->getDecl();
    FD = dyn_cast<FunctionDecl>(value_decl);
  }
  else if(isa<MemberExpr>(port)){
    auto member_func = dyn_cast<MemberExpr>(port);
    NamedDecl *ND = member_func->getMemberDecl();
    FD = dyn_cast<FunctionDecl>(ND);
  }

  if (FD) { 
    return true;
  }
  else {
    return false;
  }
}

void CodeGenFunction::EmitSAXILITEOffsetIntrinsic( const SAXILITEOffsetInterfaceAttr *interface) 
{ 
  if (!CurFuncDecl->hasAttr<SDxKernelAttr>()) { 
    return ;
  }
  Expr *port = interface->getPort();
  llvm::Value* port_var = nullptr;

  int64_t port_width = 0;

  if (port->getType()->isPointerType() || port->getType()->isReferenceType()) { 
    if (IsHLSBurstMaxiType(port->getType()->getPointeeType())) { 
      CGM.getDiags().Report(interface->getLocation(), diag::err_xlx_invalid_port_expr)
          << "m_axi interface for 'burst_maxi' doesn't support pointer or reference  ";
      return ;
    }
  }

  if (IsFunctionPointer(port)) { 
    port_var = llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo());
    port_width = 0;
  }
  else if (IsHLSBurstMaxiType(port->getType())) { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    llvm::Value *V = port_info.first;
    if (V->getType()->isPointerTy()) { 
      CGM.getDiags().Report(interface->getLocation(), diag::err_xlx_invalid_port_expr)
          << "m_axi interface for 'burst_maxi' doesn't support pointer or reference  ";
      return ;
    }
    else { 
      port_var = Builder.CreateExtractValue(V, {0});
      port_width = CGM.getDataLayout().getTypeAllocSizeInBits(port_var->getType()->getPointerElementType());
    }
  }
  else { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    port_var = port_info.first;
    port_width = port_info.second;
  }

  auto bundName = llvm::ConstantDataArray::getString(getLLVMContext(), interface->getBundleName(), false);
  auto offset = Builder.getInt64(HLSEvaluateICE(interface->getOffset(), "offset", "s_axilite", -1));
  auto isRegister = Builder.getInt32(interface->getIsRegister());
  auto signalName = llvm::ConstantDataArray::getString(getLLVMContext(), interface->getSignalName(), false);
  auto clockName = llvm::ConstantDataArray::getString(getLLVMContext(), interface->getClockName(), false);
  auto implName = llvm::ConstantDataArray::getString(getLLVMContext(), interface->getImplName(), false);

  llvm::Value *Args[] = { port_var, bundName, offset, isRegister, signalName, clockName, implName };
  CreateCallSideEffect(CGM, Builder, "xlx_s_axilite", Args, port_width, getPragmaContext(interface));
}

void CodeGenFunction::EmitMAXIInterfaceIntrinsic( const MAXIInterfaceAttr *interface) 
{
  if (!CurFuncDecl->hasAttr<SDxKernelAttr>()) { 
    return ;
  }
  Expr *port = interface->getPort();
  int64_t port_width = 0;

  if (port->getType()->isPointerType() || port->getType()->isReferenceType()) { 
    if (IsHLSBurstMaxiType(port->getType()->getPointeeType())) { 
      CGM.getDiags().Report(interface->getLocation(), diag::err_xlx_invalid_port_expr)
          << "m_axi interface for 'burst_maxi' doesn't support pointer or reference  ";
      return ;
    }
  }

  llvm::Value* port_var = nullptr;
  if (IsFunctionPointer(port)) { 
    port_var = llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo());
    port_width = 0;
  }
  else if (IsHLSBurstMaxiType(port->getType())) { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    llvm::Value *V = port_info.first;
    if (V->getType()->isPointerTy()) { 
      CGM.getDiags().Report(interface->getLocation(), diag::err_xlx_invalid_port_expr)
          << "m_axi interface for 'burst_maxi' doesn't support pointer or reference  ";
      return ;
    }
    else { 
      port_var = Builder.CreateExtractValue(V, {0});
      port_width = CGM.getDataLayout().getTypeAllocSizeInBits(port_var->getType()->getPointerElementType());
    }
  }
  else { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    port_var = port_info.first;
    port_width = port_info.second;
  }

  StringRef bundleName = interface->getBundleName();
  std::string channelID = 
      cast<StringLiteral>(interface->getChannel())->getString();
  
  int depth = HLSEvaluateICE(interface->getDepth(), "depth", "m_axi", -1);
  StringRef offsetType = MAXIInterfaceAttr::ConvertOffsetModeTypeToStr(interface->getOffsetMode());
  StringRef signalName = interface->getSignalName();
  int num_read_outstanding = HLSEvaluateICE(interface->getNumReadOutstanding(), "read_outstanding", "m_axi");
  int num_write_outstanding = HLSEvaluateICE(interface->getNumWriteOutstanding(), "write_outstanding", "m_axi");
  int max_read_burst_length = HLSEvaluateICE(interface->getMaxReadBurstLength(), "max_read_burst_length", "m_axi");
  int max_write_burst_length = HLSEvaluateICE(interface->getMaxWriteBurstLength(), "max_write_burst_length", "m_axi");
  int latency = HLSEvaluateICE(interface->getLatency(), "latency", "m_axi");
  int max_widen_bitwidth = HLSEvaluateICE(interface->getMaxWidenBitWidth(), "bitwidth" , "m_axi");
  llvm::Value *Args[] = {
    port_var, 
    llvm::ConstantDataArray::getString(getLLVMContext(), bundleName, false), 
    Builder.getInt64(depth), 
    llvm::ConstantDataArray::getString(getLLVMContext(), offsetType, false), 
    llvm::ConstantDataArray::getString(getLLVMContext(), signalName, false),
    Builder.getInt64(num_read_outstanding),
    Builder.getInt64(num_write_outstanding),
    Builder.getInt64(max_read_burst_length),
    Builder.getInt64(max_write_burst_length),
    Builder.getInt64(latency),
    Builder.getInt64(max_widen_bitwidth),
    llvm::ConstantDataArray::getString(getLLVMContext(), channelID, false),
  };
                    
  CreateCallSideEffect(CGM, Builder, "xlx_m_axi", Args, port_width, getPragmaContext(interface));
}

void CodeGenFunction::EmitAXIStreamInterfaceIntrinsic( const AXIStreamInterfaceAttr* interface)
{
  if (!CurFuncDecl->hasAttr<SDxKernelAttr>()) { 
    return ;
  }
  int64_t port_width = 0;
  Expr *port = interface->getPort();
  llvm::Value* port_var = nullptr;
  if (IsFunctionPointer(port)) { 
    port_var = llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo());
    port_width = 0;
  }
  else { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    port_var = port_info.first;
    port_width = port_info.second;
  }

  int isRegister = interface->getIsRegister();
  int registerMode = (int)interface->getRegisterMode();
  int depth = HLSEvaluateICE(interface->getDepth(), "depth","axis",  0);
  StringRef signalName = interface->getSignalName();
  StringRef bundleName = interface->getBundleName();

  llvm::Value* Args[] = {
    port_var,
    Builder.getInt32(isRegister),
    Builder.getInt64(registerMode), 
    Builder.getInt64(depth), 
    llvm::ConstantDataArray::getString(getLLVMContext(), signalName, false),
    llvm::ConstantDataArray::getString(getLLVMContext(), bundleName, false)
  };

  CreateCallSideEffect(CGM, Builder, "xlx_axis", Args, port_width, getPragmaContext(interface));
}

void CodeGenFunction::EmitMemoryInterfaceIntrinsic( const MemoryInterfaceAttr *interface) 
{
  if (!CurFuncDecl->hasAttr<SDxKernelAttr>()) { 
    return ;
  }
  Expr *port = interface->getPort();
  llvm::Value* port_var = nullptr;
  int64_t port_width = 0;
  if (IsFunctionPointer(port)) { 
    port_var = llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo());
    port_width = 0;
  }
  else {
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    port_var = port_info.first;
    port_width = port_info.second;
  }

  StringRef storageTypeStr = interface->getStorageType();
  int storage_type_op = platform::PlatformBasic::OP_UNSUPPORTED;
  int storage_type_impl = platform::PlatformBasic::UNSUPPORTED;
  if (storageTypeStr != "") { 
    const platform::PlatformBasic *xilinxPlatform =
        platform::PlatformBasic::getInstance();
    std::pair<platform::PlatformBasic::OP_TYPE,
              platform::PlatformBasic::IMPL_TYPE>
        mem_impl_type;
    if (!xilinxPlatform->verifyInterfaceStorage(
            storageTypeStr.str(), &mem_impl_type)) {
      CGM.getDiags().Report(interface->getLocation(), diag::err_xlx_attribute_invalid_option)
          << storageTypeStr
          << "interface " +interface->getMode().str() + "'s option 'storage_type'";
      return;
    }
    storage_type_impl = (int)mem_impl_type.second;
    storage_type_op = (int)mem_impl_type.first;
  }

  int latency = HLSEvaluateICE(interface->getLatency(), "latency", "interface " + interface->getMode().str());
  StringRef signalName = interface->getSignalName();
  int depth = HLSEvaluateICE(interface->getDepth(), "depth", "interface " + interface->getMode().str());
  int addressMode = interface->getAddressMode();
  bool directIO = interface->getDirectIO();

  llvm::Value* Args[] = {
    port_var,
    Builder.getInt64(storage_type_op),
    Builder.getInt64(storage_type_impl), 
    Builder.getInt64(latency), 
    llvm::ConstantDataArray::getString(getLLVMContext(), signalName, false),
    Builder.getInt64(depth),
    llvm::ConstantDataArray::getString(getLLVMContext(), storageTypeStr, false), /* reuse storageTypeStr for recoding */
    Builder.getInt64(addressMode),
    Builder.getInt32(directIO)
  };

  if (interface->getMode() != "ap_memory" &&
      interface->getMode() != "bram") {
    assert(false && "unexpected");
  }
  CreateCallSideEffect(CGM, Builder, "xlx_" + interface->getMode().str(), Args, port_width, getPragmaContext(interface));
}


void CodeGenFunction::EmitAPFifoInterfaceIntrinsic( const APFifoInterfaceAttr *interface) 
{
  if (!CurFuncDecl->hasAttr<SDxKernelAttr>()) { 
    return ;
  }
  int64_t port_width = 0;
  Expr *port = interface->getPort();
  llvm::Value* port_var = nullptr;
  if (IsFunctionPointer(port)) { 
    port_var = llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo());
    port_width = 0;
  }
  else { 
    auto port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    port_var = port_info.first;
    port_width = port_info.second;
  }

  int isRegister = interface->getIsRegister();
  StringRef signalName = interface->getSignalName();
  int depth = HLSEvaluateICE(interface->getDepth(), "depth", "interface ap_fifo" );
  llvm::Value *Args [] = { 
    port_var,
    Builder.getInt32(isRegister), 
    llvm::ConstantDataArray::getString(getLLVMContext(), signalName, false), 
    Builder.getInt64(depth)
  };

  CreateCallSideEffect(CGM, Builder, "xlx_ap_fifo", Args, port_width, getPragmaContext(interface));
}

void CodeGenFunction::EmitAPScalarInterfaceIntrinsic(const APScalarInterfaceAttr *interface) 
{
  if (!CurFuncDecl->hasAttr<SDxKernelAttr>()) { 
    return ;
  }
  Expr *port = interface->getPort();
  llvm::Value* port_var = nullptr;
  int64_t port_width = 0;
  if (IsFunctionPointer(port)) { 
    port_var = llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo());
  }
  else { 
    auto port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    port_var = port_info.first;
    port_width = port_info.second;
  }

  int isRegister = interface->getIsRegister();
  StringRef signalName = interface->getSignalName();
  bool directIO = interface->getDirectIO();

  llvm::Value* Args[] = { 
    port_var,
    Builder.getInt32(isRegister), 
    llvm::ConstantDataArray::getString(getLLVMContext(), signalName, false),
    Builder.getInt32(directIO)
  };

  if (interface->getMode() != "ap_none" &&
      interface->getMode() != "ap_ack" &&
      //interface->getMode() != "ap_vld" &&
      interface->getMode() != "ap_ovld" &&
      //interface->getMode() != "ap_hs" &&
      interface->getMode() != "ap_stable") {
    assert(false && "unexpected");
  }

  CreateCallSideEffect(CGM, Builder, "xlx_" + interface->getMode().str(), Args, port_width, getPragmaContext(interface));
}

void CodeGenFunction::EmitAPScalarInterruptInterfaceIntrinsic(const APScalarInterruptInterfaceAttr *interface) 
{
  if (!CurFuncDecl->hasAttr<SDxKernelAttr>()) { 
    return ;
  }
  Expr *port = interface->getPort();
  llvm::Value* port_var = nullptr;
  int64_t port_width = 0;
  if (IsFunctionPointer(port)) { 
    port_var = llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo());
  }
  else { 
    auto port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    port_var = port_info.first;
    port_width = port_info.second;
  }

  int isRegister = interface->getIsRegister();
  StringRef signalName = interface->getSignalName();
  int interruptValue = HLSEvaluateICE(interface->getInterrupt(), "interrupt",  "interrupt" );
  bool directIO = interface->getDirectIO();

  llvm::Value* Args[] = { 
    port_var,
    Builder.getInt32(isRegister), 
    llvm::ConstantDataArray::getString(getLLVMContext(), signalName, false), 
    Builder.getInt64(interruptValue),
    Builder.getInt32(directIO)
  };

  if (interface->getMode() != "ap_vld" &&
      interface->getMode() != "ap_hs") {
    assert(false && "unexpected");
  }
  CreateCallSideEffect(CGM, Builder, "xlx_" + interface->getMode().str(), Args, port_width, getPragmaContext(interface));
}

void  CodeGenFunction::EmitFPGAFunctionCtrlInterfaceIntrinsic( const FPGAFunctionCtrlInterfaceAttr *interface)
{
  // TODO:
  // Here also generate ap_ctrl for sub function.
  // In future, we could remove such hack
  //if (!CurFuncDecl->hasAttr<SDxKernelAttr>()) { 
  //  return ;
  //}

  StringRef signalName = interface->getName();

  llvm::Value* Args[] = { 
    llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo()),
    llvm::ConstantDataArray::getString(getLLVMContext(), signalName, false), 
  };

  if (interface->getMode() != "ap_ctrl_hs" &&
      interface->getMode() != "ap_ctrl_none" &&
      interface->getMode() != "ap_ctrl_chain") {
    assert(false && "unexpected");
  }

  CreateCallSideEffect(CGM, Builder, "xlx_" + interface->getMode().str(), Args, 1, getPragmaContext(interface));
}

void CodeGenFunction::EmitResetIntrinsic( const XlxResetIntrinsicAttr* reset)
{
  int64_t port_width = 0;
  Expr *port = reset->getVariable();

  Expr::EvalResult Eval;
  if (!port->EvaluateAsLValue(Eval, getContext()) || !Eval.isGlobalLValue()) {
    CGM.getDiags().Report(port->getExprLoc(), diag::warn_invalid_pragma_variable)
      << "reset"
      << "static or global";
    return;
  }

  llvm::Value* port_var = nullptr;
  if (IsFunctionPointer(port)) { 
    port_var = llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo());
    port_width = 0;
  }
  else { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    port_var = port_info.first;
    port_width = port_info.second;
  }

  if (isa<llvm::BitCastOperator>(port_var)) { 
    port_var = cast<llvm::BitCastOperator>(port_var)->getOperand(0);
  }

  bool isEnable = reset->getEnabled();
  llvm::Value* Args[] = {
    port_var,
    Builder.getInt1(isEnable)
  };
  CreateCallSideEffect(CGM, Builder, "xlx_reset", Args, port_width, getPragmaContext(reset));
}

void CodeGenFunction::EmitMAXIAliasIntrinsic( const XlxMAXIAliasAttr* maxi_alias) { 
 
  llvm::SmallVector<llvm::Value *, 8> args;
  for(auto i = maxi_alias->ports_begin(), e = maxi_alias->ports_end(); i != e; ++i) {
    auto port = *i;
    auto port_info = EmitHLSVariableExpr(*i);
    if (nullptr == port_info.first) return;
    llvm::Value *port_var = port_info.first;

    if (IsHLSBurstMaxiType(port->getType())) {
      if (port_var->getType()->isPointerTy()) {
        CGM.getDiags().Report(maxi_alias->getLocation(), diag::err_xlx_invalid_port_expr)
            << "'burst_maxi' cannot be pointer or reference  ";
        return;
      }
      port_var = Builder.CreateExtractValue(port_var, {0});
    }

    args.push_back(port_var);   
  }

  for(auto i = maxi_alias->offsets_begin(), e = maxi_alias->offsets_end(); i != e; ++i) {
    auto offset = HLSEvaluateICE(*i, "offset", "alias");
    if(offset < 0) {
      CGM.getDiags().Report((*i)->getExprLoc(), diag::err_invalid_offset_for_alias); 
      return;
    }
    args.push_back(Builder.getInt32(offset));
  }

  assert(&(CGM.getModule())==CurFn->getParent() && "unexpected");
  CreateCallSideEffect(CGM, Builder, "fpga.maxi.alias", args, -1, getPragmaContext(maxi_alias));
}

void CodeGenFunction::EmitFuncInstantiateIntrinsic( const XlxFuncInstantiateAttr* func_inst) { 

  Expr *E = func_inst->getVariable(); 
  auto info = EmitHLSVariableExpr(E);  
  if (nullptr == info.first) return;

  llvm::Value *args[] = {info.first};

  assert(&(CGM.getModule())==CurFn->getParent() && "unexpected");
  CreateCallSideEffect(CGM, Builder, "fpga.func.instantiate", args, -1, getPragmaContext(func_inst));
}

void CodeGenFunction::EmitXlxArrayStencilIntrinsic( const XlxArrayStencilAttr* stencil) { 
  int64_t port_width = 0;


  Expr *port = stencil->getVariable();
  llvm::Value* port_var = nullptr;
  if (IsFunctionPointer(port)) { 
    port_var = llvm::ConstantPointerNull::get(Builder.getInt8Ty()->getPointerTo());
    port_width = 0;
  }
  else { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;
    port_var = port_info.first;
    port_width = port_info.second;
  }
 
  llvm::Value* args[] = {
    port_var,
    Builder.getInt1(stencil->getOff())
  };

  assert(&(CGM.getModule())==CurFn->getParent() && "unexpected");
  CreateCallSideEffect(CGM, Builder, "fpga_array_stencil", args, port_width, getPragmaContext(stencil));
}

void  CodeGenFunction::EmitXlxDependenceIntrinsic( const XlxDependenceAttr*  XLXDependence) {
  llvm::Value* addr = nullptr;
  int port_width = 0;
  Expr *expr = XLXDependence->getVariable();
  // if "variable=..." option is missing, Parser will generate "IntegerLietral(1)" 
  // to set Dependence::variable option, we can not set "dependence::variable" option 
  // as value "Null" , becaues  AST dump/printer will crash for "Null" expr
  // if "variable" is integerLiteral, just skip it 
  
  if (expr && !isa<IntegerLiteral>(expr)) { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(expr);
    if (nullptr == port_info.first) return;

    addr = port_info.first;
    port_width = port_info.second;
    if (IsHLSBurstMaxiType(expr->getType())) { 
      if(addr->getType()->isPointerTy()){
        auto baseType = dyn_cast<llvm::PointerType>(addr->getType())->getElementType();
        int GEPIdx = 0;
        SmallVector<llvm::Value*, 4> idxs;
        idxs.push_back( Builder.getInt32(0));
        addr = Builder.CreateGEP(addr, idxs);
        addr = Builder.CreateLoad(Address(addr,  CharUnits::fromQuantity(8)));
      }
      else { 
        SmallVector<unsigned int, 4> idxs;
        idxs.push_back( 0 );
        addr = Builder.CreateExtractValue(addr, makeArrayRef(idxs));
      }
    }
  }
  else {
    addr = Builder.getInt32(0);
    port_width = 0;
  }
  
  auto dep_class = XLXDependence->getXClass();
  auto dep_compel = XLXDependence->getCompel();
  auto dep_type = XLXDependence->getType();
  auto dep_direction = XLXDependence->getDirection();
  auto dep_distance = HLSEvaluateICE(XLXDependence->getDistance(), "distance" , "dependence");

  if (dep_distance > 0 && dep_type ==  XlxDependenceAttr::intra ){
    dep_distance = 0;
  }

  if (dep_distance <= 0 && dep_type ==  XlxDependenceAttr::inter ){
    dep_distance = 0;
  }




  llvm::Value* args[] = {
       addr,
       Builder.getInt32(dep_class),
       Builder.getInt32(dep_compel), 
       Builder.getInt32(dep_direction -1 ), 
       Builder.getInt32(dep_distance),
       Builder.getInt32(dep_type),
       Builder.getInt1(true)
  };

  assert(&(CGM.getModule())==CurFn->getParent() && "unexpected");
  CreateCallSideEffect(CGM, Builder, "fpga.dependence", args, port_width, getPragmaContext(XLXDependence));
}

void  CodeGenFunction::EmitXCLDependenceIntrinsic( const XCLDependenceAttr*  XLXDependence) {
  llvm::Value* addr = nullptr;
  int port_width = 0;
  Expr *expr = XLXDependence->getVariable();
  // if "variable=..." option is missing, Parser will generate "IntegerLietral(1)" 
  // to set Dependence::variable option, we can not set "dependence::variable" option 
  // as value "Null" , becaues  AST dump/printer will crash for "Null" expr
  // if "variable" is integerLiteral, just skip it 
  
  if (expr && !isa<IntegerLiteral>(expr)) { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(expr);
    if (nullptr == port_info.first) return;

    addr = port_info.first;
    port_width = port_info.second;
  }
  else {
    addr = Builder.getInt32(0);
    port_width = 0;
  }
  
  auto dep_class = XLXDependence->getXClass();
  auto dep_compel = XLXDependence->getCompel();
  auto dep_type = XLXDependence->getType();
  auto dep_direction = XLXDependence->getDirection();
  auto dep_distance = HLSEvaluateICE( XLXDependence->getDistance(), "distance", "dependence");

  llvm::Value* args[] = {
       addr,
       Builder.getInt32(dep_class),
       Builder.getInt32(dep_compel), 
       Builder.getInt32(dep_direction -1 ), 
       Builder.getInt32(dep_distance),
       Builder.getInt32(dep_type),
       Builder.getInt1(true)
  };

  assert(&(CGM.getModule())==CurFn->getParent() && "unexpected");
  CreateCallSideEffect(CGM, Builder, "fpga.dependence", args, port_width, getPragmaContext(XLXDependence));
}


static bool must_be_power_of_2(int val, const char *option, SourceLocation Loc, DiagnosticsEngine &diag)
{

  if (val == 0 || (val & ~(val-1)) != val) {
    diag.Report(Loc, diag::err_xlx_attribute_invalid_option_and_because)
        << StringRef(llvm::formatv("'{0}'", option))
        << StringRef(llvm::formatv("Valid value for the '{0}' option must be power of 2", option));
    return false; 
  }
  return true; 
}


void CodeGenFunction::EmitXlxCacheIntrinsic(const XlxCacheAttr *cache)
{
  if (!CurFuncDecl->hasAttr<SDxKernelAttr>()) { 
    return;
  }
  Expr *port = cache->getPort();
  int64_t port_width = 0;

  if (port->getType()->isPointerType() || port->getType()->isReferenceType()) { 
    if (IsHLSBurstMaxiType(port->getType()->getPointeeType())) { 
      CGM.getDiags().Report(cache->getLocation(), diag::err_xlx_invalid_port_expr)
          << "cache for 'burst_maxi' doesn't support pointer or reference  ";
      return;
    }
  }

  llvm::Value* port_var = nullptr;
  if (IsFunctionPointer(port)) { 
    CGM.getDiags().Report(cache->getLocation(), diag::err_xlx_invalid_port_expr)
        << "cache pragma option 'port' doesn't support function pointer";
    return;
  }
  else if (IsHLSBurstMaxiType(port->getType())) { 
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    llvm::Value *V = port_info.first;
    if (V->getType()->isPointerTy()) { 
      CGM.getDiags().Report(cache->getLocation(), diag::err_xlx_invalid_port_expr)
          << "cache for 'burst_maxi' doesn't support pointer or reference  ";
      return;
    }
    else {
      port_var = Builder.CreateExtractValue(V, {0});
      port_width = CGM.getDataLayout().getTypeAllocSizeInBits(port_var->getType()->getPointerElementType());
    }
  }
  else {
    std::pair<llvm::Value*, int64_t> port_info = EmitHLSVariableExpr(port);
    if (nullptr == port_info.first) return;

    port_var = port_info.first;
    port_width = port_info.second;
  }

  size_t lines = 1;
  size_t depth = 1; 
  size_t ways = 1; 
  size_t users = 1; 
  bool invalidExpr = false; 
  if (Optional<int> result = HLSEvaluateICEResult(cache->getLines()) ) { 
    lines = result.getValue(); 
  }
  else { 
    CGM.getDiags().Report(cache->getLines()->getExprLoc(), diag::err_xlx_option_not_ice)
      << "lines" << "cache";
    invalidExpr = true; 
  }

  if (Optional<int> result =  HLSEvaluateICEResult(cache->getDepth())){ 
    depth = result.getValue(); 
  }
  else { 
    CGM.getDiags().Report(cache->getDepth()->getExprLoc(), diag::err_xlx_option_not_ice)
      << "depth" << "cache";
    invalidExpr = true; 
  }
  if (Optional<int> result = HLSEvaluateICEResult(cache->getWays())) { 
    ways = result.getValue(); 
  }
  else { 
    CGM.getDiags().Report(cache->getWays()->getExprLoc(), diag::err_xlx_option_not_ice)
      << "ways" << "cache";
    invalidExpr = true; 
  }

  if (Optional<int> result =  HLSEvaluateICEResult(cache->getUsers())) { 
    users = result.getValue(); 
  }
  else { 
    CGM.getDiags().Report(cache->getUsers()->getExprLoc(), diag::err_xlx_option_not_ice)
      << "users" << "cache";
    invalidExpr = true; 
  }

  if (invalidExpr) 
    return ; 

  size_t burst = (size_t)cache->getBurst();
  size_t write = (size_t)cache->getWrite();

  bool validLines = must_be_power_of_2(lines, "Lines", cache->getLocation(), CGM.getDiags()); 

  bool validDepth = true; 
  if (!cache->getIsDefaultDepth()){ 
    validDepth = must_be_power_of_2(depth, "Depth", cache->getLocation(), CGM.getDiags());
  }

  if (!validDepth || !validLines)
    return ; 

  llvm::Value *Args[] = {
    port_var,
    Builder.getInt64(lines),
    Builder.getInt64(depth),
    Builder.getInt64(ways),
    Builder.getInt64(users),
    Builder.getInt64(burst), 
    Builder.getInt64(write), 
  };

  CreateCallSideEffect(CGM, Builder, "xlx_cache", Args, port_width, getPragmaContext(cache));
}
