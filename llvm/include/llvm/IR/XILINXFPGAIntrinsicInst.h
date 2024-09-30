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
// This file defines classes that make it really easy to deal with intrinsic
// functions in FPGA with the isa/dyncast family of functions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_XILINXFPGAINTRINSICINST_H
#define LLVM_IR_XILINXFPGAINTRINSICINST_H

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Attributes.h"
#include "llvm/Support/XILINXFPGAPlatformBasic.h"

namespace llvm {

StringRef getPragmaSourceFromMDNode(MDNode *MD);

class MallocInst : public CallInst {
public:
  MallocInst() = delete;
  MallocInst(const MallocInst &) = delete;
  MallocInst &operator=(const MallocInst &) = delete;

  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static bool classof(const CallInst *I) {
    auto *F = I->getCalledFunction();
    if (!F)
      return false;
    return (F->getName() == "malloc");
  }

  static bool classof(const Value *V) {
    return isa<CallInst>(V) && classof(cast<CallInst>(V));
  }

  Value *getSize() const {
    return getArgOperand(0);
  }

  bool hasConstantSize() const {
    return isa<ConstantInt>(getSize());
  }

  uint64_t getConstantSize() const {
    if (!hasConstantSize())
      return 0;
    auto Size = cast<ConstantInt>(getSize());
    return Size->getZExtValue();
  }
};

/// This represents the fpga_smod
class SModInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_smod;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
};

class FloatInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return is_one_value_of(I->getIntrinsicID(),
                           Intrinsic::fpga_float_add,
                           Intrinsic::fpga_float_sub,
                           Intrinsic::fpga_float_mul,
                           Intrinsic::fpga_float_div,
                           Intrinsic::fpga_float_fma,
                           Intrinsic::fpga_float_sqrt,
                           Intrinsic::fpga_float_from_fixed,
                           Intrinsic::fpga_float_to_fixed,
                           Intrinsic::fpga_float_to_float,
                           Intrinsic::fpga_float_compare_eq,
                           Intrinsic::fpga_float_compare_le,
                           Intrinsic::fpga_float_compare_lt,
                           Intrinsic::fpga_float_compare_ne,
                           Intrinsic::fpga_float_compare_uo);
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// isCommutative - Return true if the first two arguments can be exchanged
  // FIXME Unfortunately, IntrinsincInst::isCommutative isn't virtual...
  bool isCommutative() const {
    return is_one_value_of(getIntrinsicID(),
                           Intrinsic::fpga_float_add,
                           Intrinsic::fpga_float_mul,
                           Intrinsic::fpga_float_fma,
                           Intrinsic::fpga_float_compare_eq,
                           Intrinsic::fpga_float_compare_ne,
                           Intrinsic::fpga_float_compare_uo);
  }

  /// isHomogeneous - Return true if all the arguments have the same layout
  // If true, then there is one extra argument (constant) to record
  // the common exponent bitwidth;
  // If false, then there is as many extra argument (constants) as
  // floating point values (one per input and one for the output) to record
  // each exponent bitwidth.
  bool isHomogeneous() const {
    return !is_one_value_of(getIntrinsicID(),
                            Intrinsic::fpga_float_from_fixed,
                            Intrinsic::fpga_float_to_fixed,
                            Intrinsic::fpga_float_to_float);
  }
};

class FloatComputeInst : public FloatInst {
public:
  static inline bool classof(const FloatInst *I) {
    return is_one_value_of(I->getIntrinsicID(),
                           Intrinsic::fpga_float_add,
                           Intrinsic::fpga_float_sub,
                           Intrinsic::fpga_float_mul,
                           Intrinsic::fpga_float_div,
                           Intrinsic::fpga_float_fma,
                           Intrinsic::fpga_float_sqrt);
  }

  static inline bool classof(const Value *V) {
    return isa<FloatInst>(V) && classof(cast<FloatInst>(V));
  }

  /// getType - Overload to return type as IntegerType
  IntegerType *getType() const {
    return cast<IntegerType>(IntrinsicInst::getType());
  }

  /// getExpArgIndex - Return the ArgOperand index
  unsigned getExpArgIndex() const { return getNumArgOperands() - 1; }

  /// getExp - Return the Exponent argument as ConstantInt
  ConstantInt *getExp() const {
    return cast<ConstantInt>(getArgOperand(getExpArgIndex()));
  }

  /// getBitwidth - Return the total bitwidth
  unsigned getBitwidth() const { return getType()->getBitWidth(); }

  /// getExpBitwidth - Return the exponent bitwidth
  unsigned getExpBitwidth() const {
    return (unsigned) (getExp()->getZExtValue());
  }

  /// getMantBitwidth - Return the mantissa bitwidth (includes hidden bit)
  unsigned getMantBitwidth() const {
    return getBitwidth() - getExpBitwidth();
  }
};

class FloatUnaryInst : public FloatComputeInst {
public:
  static inline bool classof(const FloatComputeInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_float_sqrt;
  }

  static inline bool classof(const Value *V) {
    return isa<FloatComputeInst>(V) && classof(cast<FloatComputeInst>(V));
  }
};

#define FUInst(Name, name)                                                     \
  class Float##Name##Inst : public FloatUnaryInst {                            \
  public:                                                                      \
    static inline bool classof(const FloatUnaryInst *I) {                      \
      return I->getIntrinsicID() == Intrinsic::fpga_float_##name;              \
    }                                                                          \
                                                                               \
    static inline bool classof(const Value *V) {                               \
      return isa<FloatUnaryInst>(V) && classof(cast<FloatUnaryInst>(V));       \
    }                                                                          \
  };
FUInst(Sqrt, sqrt)
#undef FUInst

class FloatBinaryInst : public FloatComputeInst {
public:
  static inline bool classof(const FloatComputeInst *I) {
    return is_one_value_of(I->getIntrinsicID(),
                           Intrinsic::fpga_float_add,
                           Intrinsic::fpga_float_sub,
                           Intrinsic::fpga_float_mul,
                           Intrinsic::fpga_float_div);
  }

  static inline bool classof(const Value *V) {
    return isa<FloatComputeInst>(V) && classof(cast<FloatComputeInst>(V));
  }
};

#define FBInst(Name, name)                                                     \
  class Float##Name##Inst : public FloatBinaryInst {                           \
  public:                                                                      \
    static inline bool classof(const FloatBinaryInst *I) {                     \
      return I->getIntrinsicID() == Intrinsic::fpga_float_##name;              \
    }                                                                          \
                                                                               \
    static inline bool classof(const Value *V) {                               \
      return isa<FloatBinaryInst>(V) && classof(cast<FloatBinaryInst>(V));     \
    }                                                                          \
  };
FBInst(Add, add)
FBInst(Sub, sub)
FBInst(Mul, mul)
FBInst(Div, div)
#undef FBInst

class FloatTernaryInst : public FloatComputeInst {
public:
  static inline bool classof(const FloatComputeInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_float_fma;
  }

  static inline bool classof(const Value *V) {
    return isa<FloatComputeInst>(V) && classof(cast<FloatComputeInst>(V));
  }
};

#define FTInst(Name, name)                                                     \
  class Float##Name##Inst : public FloatTernaryInst {                          \
  public:                                                                      \
    static inline bool classof(const FloatTernaryInst *I) {                    \
      return I->getIntrinsicID() == Intrinsic::fpga_float_##name;              \
    }                                                                          \
                                                                               \
    static inline bool classof(const Value *V) {                               \
      return isa<FloatTernaryInst>(V) && classof(cast<FloatTernaryInst>(V));   \
    }                                                                          \
  };
FTInst(FMA, fma)
#undef FTInst

class FloatCastInst : public FloatInst {
public:
  static inline bool classof(const FloatInst *I) {
    return is_one_value_of(I->getIntrinsicID(),
                           Intrinsic::fpga_float_from_fixed,
                           Intrinsic::fpga_float_to_fixed,
                           Intrinsic::fpga_float_to_float);
  }

  static inline bool classof(const Value *V) {
    return isa<FloatInst>(V) && classof(cast<FloatInst>(V));
  }

  /// getSrcType - Overload to input type as IntegerType
  IntegerType *getSrcType() const {
    return cast<IntegerType>(getArgOperand(0)->getType());
  }

  /// getDestType - Overload to return type as IntegerType
  IntegerType *getDestType() const {
    return cast<IntegerType>(IntrinsicInst::getType());
  }

  /// getSrcExpArgIndex - Return the SrcArgOperand index
  unsigned getSrcExpArgIndex() const { return getNumArgOperands() - 2; }

  /// getDestExpArgIndex - Return the DestArgOperand index
  unsigned getDestExpArgIndex() const { return getNumArgOperands() - 1; }

  /// getSrcExponent - Return the input Exponent argument as ConstantInt
  ConstantInt *getSrcExp() const {
    return cast<ConstantInt>(getArgOperand(getSrcExpArgIndex()));
  }

  /// getDestExponent - Return the return Exponent argument as ConstantInt
  ConstantInt *getDestExp() const {
    return cast<ConstantInt>(getArgOperand(getDestExpArgIndex()));
  }

  /// getSrcBitwidth - Return the input total bitwidth
  unsigned getSrcBitwidth() const { return getSrcType()->getBitWidth(); }

  /// getDestBitwidth - Return the return total bitwidth
  unsigned getDestBitwidth() const { return getDestType()->getBitWidth(); }

  /// isSrcFloat - Return true if the input is floating point
  bool isSrcFloat() const {
    return getIntrinsicID() == Intrinsic::fpga_float_to_fixed ||
           getIntrinsicID() == Intrinsic::fpga_float_to_float;
  }

  /// isSrcFixed - Return true if the input is fixed point
  bool isSrcFixed() const {
    return getIntrinsicID() == Intrinsic::fpga_float_from_fixed;
  }

  /// isDestFloat - Return true if the output is floating point
  bool isDestFloat() const {
    return getIntrinsicID() == Intrinsic::fpga_float_from_fixed ||
           getIntrinsicID() == Intrinsic::fpga_float_to_float;
  }

  /// isDestFixed - Return true if the output is fixed point
  bool isDestFixed() const {
    return getIntrinsicID() == Intrinsic::fpga_float_to_fixed;
  }

  /// getSrcExpBitwidth - Return the input exponent bitwidth
  // (only meaningful if isSrcFloat() is true)
  unsigned getSrcExpBitwidth() const {
    return (unsigned) (getSrcExp()->getZExtValue());
  }

  /// getDestExpBitwidth - Return the output exponent bitwidth
  // (only meaningful if isDestFloat() is true)
  unsigned getDestExpBitwidth() const {
    return (unsigned) (getDestExp()->getZExtValue());
  }

  /// getSrcMantBitwidth - Return the input mantissa bitwidth (includes hidden bit)
  // (only meaningful if isSrcFloat() is true)
  unsigned getSrcMantBitwidth() const {
    return getSrcBitwidth() - getSrcExpBitwidth();
  }

  /// getDestMantBitwidth - Return the output mantissa bitwidth (includes hidden bit)
  // (only meaningful if isDestFloat() is true)
  unsigned getDestMantBitwidth() const {
    return getDestBitwidth() - getDestExpBitwidth();
  }

  /// getSrcIntegerBitwidth - Return the input integer bitwidth
  // (only meaningful if isSrcFixed() is true)
  unsigned getSrcIntBitwidth() const {
    return (unsigned) (getSrcExp()->getZExtValue());
  }

  /// getDestIntegerBitwidth - Return the output integer bitwidth
  // (only meaningful if isDestFixed() is true)
  unsigned getDestIntBitwidth() const {
    return (unsigned) (getDestExp()->getZExtValue());
  }
};

#define FCInst(Name, name)                                                     \
  class Float##Name##Inst : public FloatCastInst {                             \
  public:                                                                      \
    static inline bool classof(const FloatCastInst *I) {                       \
      return I->getIntrinsicID() == Intrinsic::fpga_float_##name;              \
    }                                                                          \
                                                                               \
    static inline bool classof(const Value *V) {                               \
      return isa<FloatCastInst>(V) && classof(cast<FloatCastInst>(V));         \
    }                                                                          \
  };
FCInst(FromFixed, from_fixed)
FCInst(ToFixed, to_fixed)
FCInst(ToFloat, to_float)
#undef FCInst

class FloatCmpInst : public FloatInst {
public:
  static inline bool classof(const FloatInst *I) {
    return is_one_value_of(I->getIntrinsicID(),
                           Intrinsic::fpga_float_compare_eq,
                           Intrinsic::fpga_float_compare_le,
                           Intrinsic::fpga_float_compare_lt,
                           Intrinsic::fpga_float_compare_ne,
                           Intrinsic::fpga_float_compare_uo);
  }

  static inline bool classof(const Value *V) {
    return isa<FloatInst>(V) && classof(cast<FloatInst>(V));
  }

  /// getPredicate - Return the FCmpInst predicate for this intrinsic
  CmpInst::Predicate getPredicate() const {
    switch (getIntrinsicID()) {
    case Intrinsic::fpga_float_compare_eq: return FCmpInst::FCMP_OEQ;
    case Intrinsic::fpga_float_compare_le: return FCmpInst::FCMP_OLE;
    case Intrinsic::fpga_float_compare_lt: return FCmpInst::FCMP_OLT;
    case Intrinsic::fpga_float_compare_ne: return FCmpInst::FCMP_ONE;
    case Intrinsic::fpga_float_compare_uo: return FCmpInst::FCMP_UNO;
    default: return FCmpInst::BAD_FCMP_PREDICATE;
    }
  }

  /// getIntrinsicIDForPred - Return the intrinsic for a given FCmpInst predicate
  static Intrinsic::ID getIntrIDForPred(CmpInst::Predicate Pred) {
    switch (Pred) {
    case FCmpInst::FCMP_OEQ: return Intrinsic::fpga_float_compare_eq;
    case FCmpInst::FCMP_OLE: return Intrinsic::fpga_float_compare_le;
    case FCmpInst::FCMP_OLT: return Intrinsic::fpga_float_compare_lt;
    case FCmpInst::FCMP_ONE: return Intrinsic::fpga_float_compare_ne;
    case FCmpInst::FCMP_UNO: return Intrinsic::fpga_float_compare_uo;
    default: return Intrinsic::not_intrinsic;
    }
  }

  /// isCommutative - Return true if %lhs and %rhs can be exchanged
  bool isCommutative() const {
    return is_one_value_of(getIntrinsicID(),
                           Intrinsic::fpga_float_compare_eq,
                           Intrinsic::fpga_float_compare_ne,
                           Intrinsic::fpga_float_compare_uo);
  }

  /// getInputType - Return input type as IntegerType
  IntegerType *getInputType() const {
    return cast<IntegerType>(getArgOperand(0)->getType());
  }

  /// getExpArgIndex - Return the exp's ArgOperand index
  unsigned getExpArgIndex() const { return getNumArgOperands() - 1; }

  /// getExp - Return the Exponent argument as ConstantInt
  ConstantInt *getExp() const {
    return cast<ConstantInt>(getArgOperand(getExpArgIndex()));
  }

  /// getBitwidth - Return the input total bitwidth
  unsigned getBitwidth() const { return getInputType()->getBitWidth(); }

  /// getExpBitwidth - Return the input exponent bitwidth
  unsigned getExpBitwidth() const {
    return (unsigned) (getExp()->getZExtValue());
  }

  /// getMantBitwidth - Return the input mantissa bitwidth (includes hidden bit)
  unsigned getMantBitwidth() const {
    return getBitwidth() - getExpBitwidth();
  }
};

#define FCInst(Name, name)                                                     \
  class Float##Name##Inst : public FloatCmpInst {                              \
  public:                                                                      \
    static inline bool classof(const FloatCmpInst *I) {                        \
      return I->getIntrinsicID() == Intrinsic::fpga_float_##name;              \
    }                                                                          \
                                                                               \
    static inline bool classof(const Value *V) {                               \
      return isa<FloatCmpInst>(V) && classof(cast<FloatCmpInst>(V));           \
    }                                                                          \
  };
FCInst(CmpEQ, compare_eq)
FCInst(CmpLE, compare_le)
FCInst(CmpLT, compare_lt)
FCInst(CmpNE, compare_ne)
FCInst(CmpUO, compare_uo)
#undef FCInst

/// This represents the fpga_bit_concat
class BitConcatInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_bit_concat;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// getType - Overload to return most specific vector type.
  ///
  IntegerType *getType() const {
    return cast<IntegerType>(IntrinsicInst::getType());
  }

  /// \brief Return the element at [Hi, Lo]
  Value *getBits(unsigned Hi, unsigned Lo) const;

  /// \brief Return the element at [Hi, Lo], without the bitcast
  Value *getElement(unsigned Hi, unsigned Lo) const;
};

/// This represent the fpga_part_select
class PartSelectInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_part_select;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// \brief Return the return type
  IntegerType *getRetTy() const;

  /// \brief Return the source from which we select part
  Value *getSrc() const;

  /// \brief Return the type of the source
  IntegerType *getSrcTy() const;

  /// \brief Return the offset starting at which we select part
  Value *getOffset() const;

  /// \brief Return the type of the offset
  IntegerType *getOffsetTy() const;
};

/// This represent the fpga_legacy_part_select
class LegacyPartSelectInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_legacy_part_select;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// \brief Return the return type
  IntegerType *getRetTy() const;

  /// \brief Return the source from which we select part
  Value *getSrc() const;

  /// \brief Return the type of source from which we select part
  IntegerType *getSrcTy() const;

  /// \brief Return the Lo from which we select part
  Value *getLo() const;

  /// \brief Return the Hi from which we select part
  Value *getHi() const;
};

/// This represents the fpga_unpack_none
class UnpackNoneInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_unpack_none;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  IntegerType *getSrcType() const {
    return cast<IntegerType>(getOperand()->getType());
  }

  Value *getOperand() const;
};

/// This represents the fpga_pack_none
class PackNoneInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pack_none;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// getType - Overload to return most specific vector type.
  ///
  IntegerType *getType() const {
    return cast<IntegerType>(IntrinsicInst::getType());
  }

  Type *getSrcType() const { return getOperand()->getType(); }

  Value *getOperand() const;
};

/// This represents the fpga_unpack_bits
class UnpackBitsInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_unpack_bits;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  IntegerType *getSrcType() const {
    return cast<IntegerType>(getOperand()->getType());
  }

  Value *getOperand() const;
};

/// This represents the fpga_pack_bits
class PackBitsInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pack_bits;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// getType - Overload to return most specific vector type.
  ///
  IntegerType *getType() const {
    return cast<IntegerType>(IntrinsicInst::getType());
  }

  Type *getSrcType() const { return getOperand()->getType(); }

  Value *getOperand() const;
};

/// This represents the fpga_unpack_bytes
class UnpackBytesInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_unpack_bytes;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  IntegerType *getSrcType() const {
    return cast<IntegerType>(getOperand()->getType());
  }

  Value *getOperand() const;
};

/// This represents the fpga_pack_bytes
class PackBytesInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pack_bytes;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// getType - Overload to return most specific vector type.
  ///
  IntegerType *getType() const {
    return cast<IntegerType>(IntrinsicInst::getType());
  }

  Type *getSrcType() const { return getOperand()->getType(); }

  Value *getOperand() const;
};

/// This represents the fpga_mux
class FPGAMuxInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_mux;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// \brief Return the Condition argument
  Value *getCondition() const {
    return getArgOperand(0);
  }

  /// \brief Return the Condition type
  IntegerType *getConditionType() const {
    return cast<IntegerType>(getArgOperand(0)->getType());
  }

  /// \brief Return the Number of input value
  unsigned getNumMuxValues() const {
    return getNumArgOperands() - 1;
  }

  /// \brief Return the Value for when Condition is equal to Idx
  Value *getMuxValue(unsigned Idx) const {
    return getArgOperand(Idx + 1);
  }

  /// \brief Set the Value for when Condition is equal to Idx
  void setMuxValue(unsigned Idx, Value *Val) {
    return setArgOperand(Idx + 1, Val);
  }

  /// \brief Return the Use for when Condition is equal to Idx
  const Use &getMuxUse(unsigned Idx) const {
    return getArgOperandUse(Idx + 1);
  }

  /// \brief Return the Use for when Condition is equal to Idx
  Use &getMuxUse(unsigned Idx) {
    return getArgOperandUse(Idx + 1);
  }

  /// \brief Return Condition value that correspond to the Use
  unsigned getMuxUseIdx(const Use *U) const {
    return U - arg_begin() - 1;
  }
};

/// This represents the fpga_sparse_mux
// Arguments:
//  0: Condition
//  1: Default case's value (undef if unused)
//  2i+2: i-th case's label (always constant)
//  2i+3: i-th case's value
class FPGASparseMuxInst : public IntrinsicInst {
public:
  enum CORE_FEATURES {
    ONE_HOT_ENCODING = 0,
    HAS_DEFAULT_INPUT = 1
  };
  
  bool isOneHotEncoding() const;
  bool isOneHotWithZeroEncoding() const;

  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_sparse_mux;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// \brief Return the Condition argument
  Value *getCondition() const {
    return getArgOperand(0);
  }

  /// \brief Return the Condition type
  IntegerType *getConditionType() const {
    return cast<IntegerType>(getArgOperand(0)->getType());
  }

  /// \brief Return the Default value
  Value *getDefaultValue() const {
    return getArgOperand(1);
  }

  /// \brief Has a Default value
  bool hasDefaultValue() const {
    return !isa<UndefValue>(getDefaultValue());
  }

  /// \brief Return the Number of input value (excluding default value)
  unsigned getNumMuxValues() const {
    unsigned NumArgs = getNumArgOperands();
    assert(NumArgs >= 2);
    assert(NumArgs % 2 == 0);
    return (NumArgs - 2)/2;
  }

  /// \brief Return the Constant that the Condition must take in order for
  ///        the mux to return the Idx-th mux Value
  ConstantInt *getMuxCase(unsigned Idx) const {
    Value *Arg = getArgOperand(2*Idx + 2);
    assert(Arg->getType() == getConditionType());
    return cast<ConstantInt>(Arg);
  }

  /// \brief Return the Idx-th mux Value that will be returned when Condition
  ///        is selected by the Idx-th case
  Value *getMuxValue(unsigned Idx) const {
    Value *Arg = getArgOperand(2*Idx + 3);
    assert(Arg->getType() == this->getType());
    return Arg;
  }

  /// \brief Set the Idx-th case's condition
  void setMuxCase(unsigned Idx, ConstantInt *Cst) {
    return setArgOperand(2*Idx + 2, Cst);
  }

  /// \brief Set the Idx-th case's Value
  void setMuxValue(unsigned Idx, Value *Val) {
    return setArgOperand(2*Idx + 3, Val);
  }

  /// \brief Return the Use for when Condition is equal to Idx
  const Use &getMuxUse(unsigned Idx) const {
    return getArgOperandUse(2*Idx + 3);
  }

  /// \brief Return the Use for when Condition is equal to Idx
  Use &getMuxUse(unsigned Idx) {
    return getArgOperandUse(2*Idx + 3);
  }

  /// \brief Return Condition value that correspond to the Use
  unsigned getMuxUseIdx(const Use *U) const {
    return (U - arg_begin() - 3)/2;
  }
};

/// This represent the fpga_seq_[load|store]_[begin|end]
class SeqBeginInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return is_one_value_of(I->getIntrinsicID(), Intrinsic::fpga_seq_load_begin,
                           Intrinsic::fpga_seq_store_begin);
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  bool isLoad() const {
    return getIntrinsicID() == Intrinsic::fpga_seq_load_begin;
  }

  bool isStore() const {
    return getIntrinsicID() == Intrinsic::fpga_seq_store_begin;
  }

  unsigned getPointerAddressSpace() const {
    return getPointerOperand()->getType()->getPointerAddressSpace();
  }

  Value *getPointerOperand() const;
  Value *getSize() const { return getArgOperand(1); }
  uint64_t getSmallConstantSize() const;
  uint64_t getSmallConstantSizeInBytes(const DataLayout &DL) const;

  PointerType *getPointerType() const;
  Type *getDataType() const { return getPointerType()->getElementType(); }
  Type *getSizeType() const { return getSize()->getType(); }

  void updatePointer(Value *V);
  void updateSize(Value *V);
};

class SeqEndInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return is_one_value_of(I->getIntrinsicID(), Intrinsic::fpga_seq_load_end,
                           Intrinsic::fpga_seq_store_end);
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  bool isLoad() const {
    return getIntrinsicID() == Intrinsic::fpga_seq_load_end;
  }

  bool isStore() const {
    return getIntrinsicID() == Intrinsic::fpga_seq_store_end;
  }

  SeqBeginInst *getPointerOperand() const { return getBegin(); }
  SeqBeginInst *getBegin() const;
  Value *getSize() const { return getArgOperand(1); }
  Type *getSizeType() const { return getSize()->getType(); }
  void updateSize(Value *V);
};

class SeqAccessInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return is_one_value_of(I->getIntrinsicID(), Intrinsic::fpga_seq_load,
                           Intrinsic::fpga_seq_store);
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  bool isLoad() const { return getIntrinsicID() == Intrinsic::fpga_seq_load; }

  bool isStore() const { return getIntrinsicID() == Intrinsic::fpga_seq_store; }

  Type *getDataType() const;
  SeqBeginInst *getPointerOperand() const;
  Value *getIndex() const;
  unsigned getPointerAddressSpace() const {
    return getPointerOperand()->getType()->getPointerAddressSpace();
  }

  void updateIndex(Value *V);
};

class ShiftRegInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return is_one_value_of(I->getIntrinsicID(),
                           Intrinsic::fpga_shift_register_peek,
                           Intrinsic::fpga_shift_register_shift);
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  bool isShift() const {
    return getIntrinsicID() == Intrinsic::fpga_shift_register_shift;
  }

  bool isPeek() const {
    return getIntrinsicID() == Intrinsic::fpga_shift_register_peek;
  }

  Value *getPointerOperand() const;
  unsigned getPointerAddressSpace() const {
    return getPointerOperand()->getType()->getPointerAddressSpace();
  }

  Type *getDataType() const;
};

class ShiftRegPeekInst : public ShiftRegInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_shift_register_peek;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  Value *getPointerOperand() const;
  Value *getIndex() const;
};

class ShiftRegShiftInst : public ShiftRegInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_shift_register_shift;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  Value *getValueOperand() const;
  Value *getPointerOperand() const;
  Value *getPredicate() const;
  Type *getDataType() const { return getValueOperand()->getType(); }
};

class SeqLoadInst : public SeqAccessInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_seq_load;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  Type *getDataType() const { return getType(); }
  SeqBeginInst *getPointerOperand() const;
  Value *getIndex() const { return getArgOperand(1); }
  void updateIndex(Value *V);
};

class SeqStoreInst : public SeqAccessInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_seq_store;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  Value *getValueOperand() const;
  Type *getDataType() const { return getValueOperand()->getType(); }
  Value *getByteEnable() const;
  IntegerType *getByteEnableType() const {
    return cast<IntegerType>(getByteEnable()->getType());
  }
  bool isMasked() const;
  SeqBeginInst *getPointerOperand() const;
  Value *getIndex() const { return getArgOperand(2); }
  void updateIndex(Value *V);
};

/// This represent the assume
class AssumeInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::assume;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
};

/// This represent the fpga_part_select
class PartSetInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_part_set;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// \brief Return the return type
  IntegerType *getRetTy() const;

  /// \brief Return the source into which we set part
  Value *getSrc() const;

  /// \brief Return the type of the source into which we set part
  IntegerType *getSrcTy() const;

  /// \brief Return the replacement from which we get part
  Value *getRep() const;

  /// \brief Return the type of the replacement from which we get part
  IntegerType *getRepTy() const;

  /// \brief Return the offset at which we start the replacement
  Value *getOffset() const;

  /// \brief Return the type of the offset
  IntegerType *getOffsetTy() const;
};

/// This represent the fpga_legacy_part_set
class LegacyPartSetInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_legacy_part_set;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// \brief Return the return type
  IntegerType *getRetTy() const;

  /// \brief Return the source into which we set part
  Value *getSrc() const;

  /// \brief Return the type of source into which we set part
  IntegerType *getSrcTy() const;

  /// \brief Return the replacement from which we get part
  Value *getRep() const;

  /// \brief Return the type of the replacement from which we get part
  IntegerType *getRepTy() const;

  /// \brief Return the Lo from which we set part
  Value *getLo() const;

  /// \brief Return the Hi from which we set part
  Value *getHi() const;
};

class FPGALoadStoreInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_load ||
           I->getIntrinsicID() == Intrinsic::fpga_maxi_store ||
           I->getIntrinsicID() == Intrinsic::fpga_bram_load ||
           I->getIntrinsicID() == Intrinsic::fpga_bram_store ||
           I->getIntrinsicID() == Intrinsic::fpga_pppo_load ||
           I->getIntrinsicID() == Intrinsic::fpga_pppo_store;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// \brief Returns the pointer operand.
  Value *getPointerOperand();
  const Value *getPointerOperand() const;

  /// \brief Returns the address space of the pointer operand.
  unsigned getPointerAddressSpace() const;

  /// \brief Returns the pointer operand type.
  PointerType *getPointerType() const;

  /// \brief Returns the alignment of the pointer operand
  unsigned getAlignment() const;

  /// \brief Returns the access data type.
  Type *getDataType() const;

  /// \brief Returns true if the FPGALoadStoreInst is volatile access.
  /// Deprecated.
  bool isVolatile() const;
};

class FPGAPPPOLoadStoreInst : public FPGALoadStoreInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pppo_load ||
           I->getIntrinsicID() == Intrinsic::fpga_pppo_store;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
};

class FPGAPPPOLoadInst : public FPGAPPPOLoadStoreInst {
public:
  static inline bool classof(const FPGAPPPOLoadStoreInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pppo_load;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPPPOLoadStoreInst>(V) &&
           classof(cast<FPGAPPPOLoadStoreInst>(V));
  }

  Value *getPointerOperand() { return getArgOperand(0); }
  const Value *getPointerOperand() const { return getArgOperand(0); }

  unsigned getAlignment() const { return getParamAlignment(0); }
};

class FPGAPPPOStoreInst : public FPGAPPPOLoadStoreInst {
public:
  static inline bool classof(const FPGAPPPOLoadStoreInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pppo_store;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPPPOLoadStoreInst>(V) &&
           classof(cast<FPGAPPPOLoadStoreInst>(V));
  }

  Value *getValueOperand() { return getArgOperand(0); }
  const Value *getValueOperand() const { return getArgOperand(0); }

  Value *getPointerOperand() { return getArgOperand(1); }
  const Value *getPointerOperand() const {return getArgOperand(1); }

  unsigned getAlignment() const { return getParamAlignment(1); }
};

class FPGADirectIOInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_direct_valid ||
           I->getIntrinsicID() == Intrinsic::fpga_direct_ready ||
           I->getIntrinsicID() == Intrinsic::fpga_direct_load ||
           I->getIntrinsicID() == Intrinsic::fpga_direct_store;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  // DirectIO operand is always the last one
  Value *getDirectIOOperand() { return getArgOperand(getNumArgOperands() - 1); }
  const Value *getDirectIOOperand() const {
    return getArgOperand(getNumArgOperands() - 1);
  }

  PointerType *getDirectIOType() const {
    return cast<PointerType>(getDirectIOOperand()->getType());
  }
  Type *getDataType() const { return getDirectIOType()->getElementType(); }

  std::string getHandShake() const {
    auto HandShake =
        getAttributes().getAttribute(
            llvm::AttributeList::FunctionIndex, "xlx.handshake");
    return HandShake.getValueAsString();
  }

  unsigned getAlignment() const {
    return getParamAlignment(getNumArgOperands() - 1);
  }
};

class FPGADirectIOStatusInst : public FPGADirectIOInst {
public:
  static inline bool classof(const FPGADirectIOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_direct_valid ||
           I->getIntrinsicID() == Intrinsic::fpga_direct_ready;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGADirectIOInst>(V) && classof(cast<FPGADirectIOInst>(V));
  }
};

class FPGADirectIOValidInst : public FPGADirectIOStatusInst {
public:
  static inline bool classof(const FPGADirectIOStatusInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_direct_valid;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGADirectIOStatusInst>(V) &&
           classof(cast<FPGADirectIOStatusInst>(V));
  }
};

class FPGADirectIOReadyInst : public FPGADirectIOStatusInst {
public:
  static inline bool classof(const FPGADirectIOStatusInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_direct_ready;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGADirectIOStatusInst>(V) &&
           classof(cast<FPGADirectIOStatusInst>(V));
  }
};

class FPGADirectLoadStoreInst : public FPGADirectIOInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_direct_load ||
           I->getIntrinsicID() == Intrinsic::fpga_direct_store;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
};

class FPGADirectLoadInst : public FPGADirectLoadStoreInst {
public:
  static inline bool classof(const FPGADirectLoadStoreInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_direct_load;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGADirectLoadStoreInst>(V) &&
           classof(cast<FPGADirectLoadStoreInst>(V));
  }
};

class FPGADirectStoreInst : public FPGADirectLoadStoreInst {
public:
  static inline bool classof(const FPGADirectLoadStoreInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_direct_store;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGADirectLoadStoreInst>(V) &&
           classof(cast<FPGADirectLoadStoreInst>(V));
  }

  Value *getDataOperand() { return getArgOperand(0); }
  const Value *getDataOperand() const { return getArgOperand(0); }
};

class FPGALoadInst : public FPGALoadStoreInst {
public:
  static inline bool classof(const FPGALoadStoreInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_load ||
           I->getIntrinsicID() == Intrinsic::fpga_bram_load;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGALoadStoreInst>(V) && classof(cast<FPGALoadStoreInst>(V));
  }

  Value *getPointerOperand() { return getArgOperand(0); }
  const Value *getPointerOperand() const { return getArgOperand(0); }

  unsigned getAlignment() const { return getParamAlignment(0); }
};

class FPGAStoreInst : public FPGALoadStoreInst {
public:
  static inline bool classof(const FPGALoadStoreInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_store ||
           I->getIntrinsicID() == Intrinsic::fpga_bram_store;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGALoadStoreInst>(V) && classof(cast<FPGALoadStoreInst>(V));
  }

  Value *getValueOperand() { return getArgOperand(0); }
  const Value *getValueOperand() const { return getArgOperand(0); }

  Value *getPointerOperand() { return getArgOperand(1); }
  const Value *getPointerOperand() const { return getArgOperand(1); }

  unsigned getAlignment() const { return getParamAlignment(1); }

  Value *getByteEnable() { return getArgOperand(2); }
  const Value *getByteEnable() const { return getArgOperand(2); }
};

struct MAXILoadInst : public FPGALoadInst {
  static inline bool classof(const FPGALoadInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_load;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGALoadInst>(V) && classof(cast<FPGALoadInst>(V));
  }
};

struct BRAMLoadInst : public FPGALoadInst {
  static inline bool classof(const FPGALoadInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_bram_load;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGALoadInst>(V) && classof(cast<FPGALoadInst>(V));
  }
};

struct MAXIStoreInst : public FPGAStoreInst {
  static inline bool classof(const FPGAStoreInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_store;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAStoreInst>(V) && classof(cast<FPGAStoreInst>(V));
  }
};

struct BRAMStoreInst : public FPGAStoreInst {
  static inline bool classof(const FPGAStoreInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_bram_store;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAStoreInst>(V) && classof(cast<FPGAStoreInst>(V));
  }
};

//===---
//
//  FIFO Intrinsics
//
//  Inheritance diagram (only leaves are actual instrinsics):
//
//  FPGAFIFOInst
//  |- FPGAFIFOStatusInst
//  |  |- FPGAFIFONotEmptyInst
//  |  `- FPGAFIFONotFullInst
//  |- FPGAFIFOLengthInst
//  |  |- FPGAFIFOSizeInst
//  |  `- FPGAFIFOCapacityInst
//  |- FPGAFIFOBlockingInst
//  |  |- FPGAFIFOPopInst
//  |  `- FPGAFIFOPushInst
//  `- FPGAFIFONonBlockingInst
//     |- FPGAFIFONbPopInst
//     `- FPGAFIFONbPushInst
//
//===---

class FPGAFIFOInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_not_empty ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_not_full ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_size ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_capacity ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_pop ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_push ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_nb_pop ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_nb_push;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  // FIFO operand is always the last one
  Value *getFIFOOperand() { return getArgOperand(getNumArgOperands() - 1); }
  const Value *getFIFOOperand() const {
    return getArgOperand(getNumArgOperands() - 1);
  }

  PointerType *getFIFOType() const {
    return cast<PointerType>(getFIFOOperand()->getType());
  }
  Type *getDataType() const { return getFIFOType()->getElementType(); }

  bool isConsumerSide() const {
    switch (getIntrinsicID()) {
    case Intrinsic::fpga_fifo_not_empty:
    case Intrinsic::fpga_fifo_pop:
    case Intrinsic::fpga_fifo_nb_pop:
      return true;
    case Intrinsic::fpga_fifo_not_full:
    case Intrinsic::fpga_fifo_push:
    case Intrinsic::fpga_fifo_nb_push:
      return false;
    default:
      llvm_unreachable("Forgot to handle a FIFO intrinsic?");
    }
  }
  bool isProducerSide() const { return !isConsumerSide(); }
};

class FPGAFIFOStatusInst : public FPGAFIFOInst {
public:
  static inline bool classof(const FPGAFIFOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_not_empty ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_not_full;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOInst>(V) && classof(cast<FPGAFIFOInst>(V));
  }
};

class FPGAFIFONotEmptyInst : public FPGAFIFOStatusInst {
public:
  static inline bool classof(const FPGAFIFOStatusInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_not_empty;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOStatusInst>(V) && classof(cast<FPGAFIFOStatusInst>(V));
  }
};

class FPGAFIFONotFullInst : public FPGAFIFOStatusInst {
public:
  static inline bool classof(const FPGAFIFOStatusInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_not_full;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOStatusInst>(V) && classof(cast<FPGAFIFOStatusInst>(V));
  }
};

class FPGAFIFOLengthInst : public FPGAFIFOInst {
public:
  static inline bool classof(const FPGAFIFOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_size ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_capacity;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOInst>(V) && classof(cast<FPGAFIFOInst>(V));
  }
};

class FPGAFIFOSizeInst : public FPGAFIFOLengthInst {
public:
  static inline bool classof(const FPGAFIFOLengthInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_size;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOLengthInst>(V) && classof(cast<FPGAFIFOLengthInst>(V));
  }
};

class FPGAFIFOCapacityInst : public FPGAFIFOLengthInst {
public:
  static inline bool classof(const FPGAFIFOLengthInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_capacity;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOLengthInst>(V) && classof(cast<FPGAFIFOLengthInst>(V));
  }
};

class FPGAFIFOBlockingInst : public FPGAFIFOInst {
public:
  static inline bool classof(const FPGAFIFOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_pop ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_push;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOInst>(V) && classof(cast<FPGAFIFOInst>(V));
  }
};

class FPGAFIFOPopInst : public FPGAFIFOBlockingInst {
public:
  static inline bool classof(const FPGAFIFOBlockingInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_pop;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOBlockingInst>(V) &&
           classof(cast<FPGAFIFOBlockingInst>(V));
  }
};

class FPGAFIFOPushInst : public FPGAFIFOBlockingInst {
public:
  static inline bool classof(const FPGAFIFOBlockingInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_push;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOBlockingInst>(V) &&
           classof(cast<FPGAFIFOBlockingInst>(V));
  }

  // Push take the value as first argument, lets provide a helper
  Value *getValueOperand() { return getArgOperand(0); }
  const Value *getValueOperand() const { return getArgOperand(0); }
};

class FPGAFIFONonBlockingInst : public FPGAFIFOInst {
public:
  static inline bool classof(const FPGAFIFOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_nb_pop ||
           I->getIntrinsicID() == Intrinsic::fpga_fifo_nb_push;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFOInst>(V) && classof(cast<FPGAFIFOInst>(V));
  }
};

class FPGAFIFONbPopInst : public FPGAFIFONonBlockingInst {
public:
  static inline bool classof(const FPGAFIFONonBlockingInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_nb_pop;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFONonBlockingInst>(V) &&
           classof(cast<FPGAFIFONonBlockingInst>(V));
  }

  // Non-blocking pop returns a struct { bool success; type_t value; }
  static unsigned getReturnedBoolIdx() { return 0; }
  static unsigned getReturnedValueIdx() { return 1; }
};

class FPGAFIFONbPushInst : public FPGAFIFONonBlockingInst {
public:
  static inline bool classof(const FPGAFIFONonBlockingInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fifo_nb_push;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAFIFONonBlockingInst>(V) &&
           classof(cast<FPGAFIFONonBlockingInst>(V));
  }

  // Non-blocking push simply returns a bool (no need for helper)

  // Push take the value as first argument, lets provide a helper
  Value *getValueOperand() { return getArgOperand(0); }
  const Value *getValueOperand() const { return getArgOperand(0); }
};

//===---
//
//  AXIS related Intrinsics
//
//  Inheritance diagram (only leaves are actual instrinsics):
//
//  AXISIntrinsicInst
//  |- AXISStatusIntrinsic
//  |  |- AXISReadyIntrinsic
//  |  `- AXISValidIntrinsic
//  |- AXISOpIntrinsicInst
//     |- AXISReadIntrinsic
//     |  |- AXISBlockingReadInst
//     |  `- AXISNonBlockingReadInst
//     `- AXISWriteIntrinsic
//        |- AXISBlockingWriteInst
//        `- AXISNonBlockingWriteInst
//
//===---

class AXISIntrinsicInst : public IntrinsicInst {
public:
  static const unsigned NumChannels = 7;

  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_pop ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_nb_pop ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_push ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_nb_push ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_valid ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_ready;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
  // check if the Value (under the Use) is from an AXIS port or not
  static bool isFromAXISWithSideChannelPort(Use &U) {
    if (!isa<AXISIntrinsicInst>(U.getUser()))
      return false;
    return U.getOperandNo() < AXISIntrinsicInst::NumChannels;
  }

  bool isConsumerSide() const {
    switch (getIntrinsicID()) {
    case Intrinsic::fpga_axis_valid:
    case Intrinsic::fpga_axis_pop:
    case Intrinsic::fpga_axis_nb_pop:
      return true;
    case Intrinsic::fpga_axis_ready:
    case Intrinsic::fpga_axis_push:
    case Intrinsic::fpga_axis_nb_push:
      return false;
    default:
      llvm_unreachable("Forgot to handle a FIFO intrinsic?");
    }
  }
  bool isProducerSide() const { return !isConsumerSide(); }
};

class AXISStatusIntrinsic : public AXISIntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_ready ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_valid;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISIntrinsicInst>(V) && classof(cast<AXISIntrinsicInst>(V));
  }
};

class AXISReadyIntrinsic : public AXISStatusIntrinsic {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_ready;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISStatusIntrinsic>(V) && classof(cast<AXISStatusIntrinsic>(V));
  }
};

class AXISValidIntrinsic : public AXISStatusIntrinsic {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_valid;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISStatusIntrinsic>(V) && classof(cast<AXISStatusIntrinsic>(V));
  }
};

/// This represent the AXIS operation related intrinsics,
/// like Read/Write/NbRead/NbWrite.
class AXISOpIntrinsicInst : public AXISIntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_pop ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_nb_pop ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_push ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_nb_push;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISIntrinsicInst>(V) && classof(cast<AXISIntrinsicInst>(V));
  }
};

class AXISReadIntrinsic : public AXISOpIntrinsicInst {
public:
  static inline bool classof(const AXISOpIntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_pop ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_nb_pop;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISOpIntrinsicInst>(V) && classof(cast<AXISOpIntrinsicInst>(V));
  }
};

class AXISBlockingReadInst : public AXISReadIntrinsic {
public:
  static inline bool classof(const AXISReadIntrinsic *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_pop;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISReadIntrinsic>(V) && classof(cast<AXISReadIntrinsic>(V));
  }
};

class AXISNonBlockingReadInst : public AXISReadIntrinsic {
public:
  static inline bool classof(const AXISReadIntrinsic *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_nb_pop;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISReadIntrinsic>(V) && classof(cast<AXISReadIntrinsic>(V));
  }
};

class AXISWriteIntrinsic : public AXISOpIntrinsicInst {
public:
  static inline bool classof(const AXISOpIntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_push ||
           I->getIntrinsicID() == Intrinsic::fpga_axis_nb_push;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISOpIntrinsicInst>(V) && classof(cast<AXISOpIntrinsicInst>(V));
  }
};

class AXISBlockingWriteInst : public AXISWriteIntrinsic {
public:
  static inline bool classof(const AXISWriteIntrinsic *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_push;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISWriteIntrinsic>(V) && classof(cast<AXISWriteIntrinsic>(V));
  }
};

class AXISNonBlockingWriteInst : public AXISWriteIntrinsic {
public:
  static inline bool classof(const AXISWriteIntrinsic *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_axis_nb_push;
  }

  static inline bool classof(const Value *V) {
    return isa<AXISWriteIntrinsic>(V) && classof(cast<AXISWriteIntrinsic>(V));
  }
};

//===---
//
//  PIPO Intrinsics
//
//  Inheritance diagram (only leaves are actual instrinsics):
//
//  FPGAPIPOInst
//  |- FPGAPIPOStatusInst
//  |  |- FPGAPIPONotEmptyInst
//  |  `- FPGAPIPONotFullInst
//  |- FPGAPIPOAcquireInst
//  |  |- FPGAPIPOPopAcquireInst
//  |  |- FPGAPIPOPushAcquireInst
//  `- FPGAPIPOReleaseInst
//     |- FPGAPIPOPopReleaseInst
//     `- FPGAPIPOPushReleaseInst
//
//===---

class FPGAPIPOInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_not_empty ||
           I->getIntrinsicID() == Intrinsic::fpga_pipo_not_full ||
           I->getIntrinsicID() == Intrinsic::fpga_pipo_pop_acquire ||
           I->getIntrinsicID() == Intrinsic::fpga_pipo_pop_release ||
           I->getIntrinsicID() == Intrinsic::fpga_pipo_push_acquire ||
           I->getIntrinsicID() == Intrinsic::fpga_pipo_push_release;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  Value *getPIPOOperand() { return getArgOperand(0); }
  const Value *getPIPOOperand() const { return getArgOperand(0); }

  PointerType *getPIPOType() const {
    return cast<PointerType>(getPIPOOperand()->getType());
  }

  bool isConsumerSide() const {
    switch (getIntrinsicID()) {
    case Intrinsic::fpga_pipo_not_empty:
    case Intrinsic::fpga_pipo_pop_acquire:
    case Intrinsic::fpga_pipo_pop_release:
      return true;
    case Intrinsic::fpga_pipo_not_full:
    case Intrinsic::fpga_pipo_push_acquire:
    case Intrinsic::fpga_pipo_push_release:
      return false;
    default:
      llvm_unreachable("Forgot to handle a PIPO intrinsic?");
    }
  }
  bool isProducerSide() const { return !isConsumerSide(); }
};

class FPGAPIPOStatusInst : public FPGAPIPOInst {
public:
  static inline bool classof(const FPGAPIPOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_not_empty ||
           I->getIntrinsicID() == Intrinsic::fpga_pipo_not_full;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPIPOInst>(V) && classof(cast<FPGAPIPOInst>(V));
  }
};

class FPGAPIPONotEmptyInst : public FPGAPIPOStatusInst {
public:
  static inline bool classof(const FPGAPIPOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_not_empty;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPIPOInst>(V) && classof(cast<FPGAPIPOInst>(V));
  }
};

class FPGAPIPONotFullInst : public FPGAPIPOStatusInst {
public:
  static inline bool classof(const FPGAPIPOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_not_full;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPIPOInst>(V) && classof(cast<FPGAPIPOInst>(V));
  }
};

class FPGAPIPOAcquireInst : public FPGAPIPOInst {
public:
  static inline bool classof(const FPGAPIPOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_pop_acquire ||
           I->getIntrinsicID() == Intrinsic::fpga_pipo_push_acquire;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPIPOInst>(V) && classof(cast<FPGAPIPOInst>(V));
  }
};

class FPGAPIPOPopAcquireInst : public FPGAPIPOAcquireInst {
public:
  static inline bool classof(const FPGAPIPOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_pop_acquire;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPIPOInst>(V) && classof(cast<FPGAPIPOInst>(V));
  }
};

class FPGAPIPOPushAcquireInst : public FPGAPIPOAcquireInst {
public:
  static inline bool classof(const FPGAPIPOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_push_acquire;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPIPOInst>(V) && classof(cast<FPGAPIPOInst>(V));
  }
};

class FPGAPIPOReleaseInst : public FPGAPIPOInst {
public:
  static inline bool classof(const FPGAPIPOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_pop_release ||
           I->getIntrinsicID() == Intrinsic::fpga_pipo_push_release;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPIPOInst>(V) && classof(cast<FPGAPIPOInst>(V));
  }
};

class FPGAPIPOPopReleaseInst : public FPGAPIPOReleaseInst {
public:
  static inline bool classof(const FPGAPIPOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_pop_release;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPIPOInst>(V) && classof(cast<FPGAPIPOInst>(V));
  }
};

class FPGAPIPOPushReleaseInst : public FPGAPIPOReleaseInst {
public:
  static inline bool classof(const FPGAPIPOInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_pipo_push_release;
  }

  static inline bool classof(const Value *V) {
    return isa<FPGAPIPOInst>(V) && classof(cast<FPGAPIPOInst>(V));
  }
};

//===---
//
// M-AXI Intrinsics
//
//===---

class MAXIIOInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_read_req ||
           I->getIntrinsicID() == Intrinsic::fpga_maxi_read ||
           I->getIntrinsicID() == Intrinsic::fpga_maxi_write_req ||
           I->getIntrinsicID() == Intrinsic::fpga_maxi_write ||
           I->getIntrinsicID() == Intrinsic::fpga_maxi_write_resp;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  bool isReadIO() const {
    return getIntrinsicID() == Intrinsic::fpga_maxi_read_req ||
           getIntrinsicID() == Intrinsic::fpga_maxi_read;
  }

  Value *getPointerOperand() {
    return getIntrinsicID() == Intrinsic::fpga_maxi_write ? getArgOperand(1)
                                                          : getArgOperand(0);
  }

  const Value *getPointerOperand() const {
    return getIntrinsicID() == Intrinsic::fpga_maxi_write ? getArgOperand(1)
                                                          : getArgOperand(0);
  }

  PointerType *getPointerType() const {
    return cast<PointerType>(getPointerOperand()->getType());
  }

  Type *getDataType() const { return getPointerType()->getElementType(); }
};

class MAXIReqInst : public MAXIIOInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_read_req || 
           I->getIntrinsicID() == Intrinsic::fpga_maxi_write_req;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  Value *getLength() { return getArgOperand(1); }
  const Value *getLength() const { return getArgOperand(1); }
};


class MAXIReadReqInst : public MAXIReqInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_read_req;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
};

class MAXIReadInst : public MAXIIOInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_read;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
};

class MAXIWriteReqInst : public MAXIReqInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_write_req;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
};

class MAXIWriteInst : public MAXIIOInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_write;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  Value *getValueOperand() { return getArgOperand(0); }
  const Value *getValueOperand() const { return getArgOperand(0); }

  Value *getByteEnable() { return getArgOperand(2); }
  const Value *getByteEnable() const { return getArgOperand(2); }
};

class MAXIWriteRespInst : public MAXIIOInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_maxi_write_resp;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
};

//===---
//
//  Scope
//
//===---

struct ScopeEntry : public IntrinsicInst {
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::directive_scope_entry ||
           I->getIntrinsicID() == Intrinsic::hint_scope_entry;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  template <typename T>
  T *getScopeAttrs(ArrayRef<Value *> Attr, unsigned i) const {
    if (Attr.size() <= i)
      return nullptr;
    return dyn_cast<T>(Attr[i]);
  }

  StringRef getPragmaSource(StringRef pragmaName) const {
    MDNode *md = getMetadata("pragma.location");
    if (!md)
      return StringRef();

    auto Name = dyn_cast<MDString>(md->getOperand(0));
    assert(Name && Name->getString() == pragmaName && "unexpected");
    return getPragmaSourceFromMDNode(md);
  }

  DILocation *getPragmaLoc(StringRef pragmaName) const {
    MDNode *md = getMetadata("pragma.location");
    if (!md)
      return nullptr;

    auto Name = dyn_cast<MDString>(md->getOperand(0));
    assert(Name && Name->getString() == pragmaName && "unexpected");
    return dyn_cast_or_null<DILocation>(
        md->getOperand(md->getNumOperands() - 1));
  }
};

struct ScopeExit : public IntrinsicInst {
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::directive_scope_exit ||
           I->getIntrinsicID() == Intrinsic::hint_scope_exit;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  ScopeEntry *getEntry() const {
    return dyn_cast_or_null<ScopeEntry>(getArgOperand(0));
  }
};

//===---
//
//  Directive Scope
//
//===---

struct DirectiveScopeExit;
struct DirectiveScopeEntry : public ScopeEntry {
  static inline bool classof(const ScopeEntry *I) {
    return I->getIntrinsicID() == Intrinsic::directive_scope_entry;
  }

  static inline bool classof(const Value *V) {
    return isa<ScopeEntry>(V) && classof(cast<ScopeEntry>(V));
  }

  static DirectiveScopeExit *
  BuildDirectiveScope(ArrayRef<OperandBundleDef> ScopeAttrs, Instruction &Entry,
                      Instruction &Exit);

  static DirectiveScopeExit *BuildDirectiveScope(StringRef Tag,
                                                 ArrayRef<Value *> Operands,
                                                 Instruction &Entry,
                                                 Instruction &Exit);
};

struct DirectiveScopeExit : public ScopeExit {
  static inline bool classof(const ScopeExit *I) {
    return I->getIntrinsicID() == Intrinsic::directive_scope_exit;
  }

  static inline bool classof(const Value *V) {
    return isa<ScopeExit>(V) && classof(cast<ScopeExit>(V));
  }

  DirectiveScopeEntry *getEntry() const {
    return dyn_cast_or_null<DirectiveScopeEntry>(getArgOperand(0));
  }
};

//===---
//
//  Hint Scope
//
//===---

struct HintScopeExit;
struct HintScopeEntry : public ScopeEntry {
  static inline bool classof(const ScopeEntry *I) {
    return I->getIntrinsicID() == Intrinsic::hint_scope_entry;
  }

  static inline bool classof(const Value *V) {
    return isa<ScopeEntry>(V) && classof(cast<ScopeEntry>(V));
  }

  static HintScopeExit *
  BuildHintScope(ArrayRef<OperandBundleDef> ScopeAttrs, Instruction &Entry,
                 Instruction &Exit);

  static HintScopeExit *BuildHintScope(StringRef Tag,
                                       ArrayRef<Value *> Operands,
                                       Instruction &Entry, Instruction &Exit);
};

struct HintScopeExit : public ScopeExit {
  static inline bool classof(const ScopeExit *I) {
    return I->getIntrinsicID() == Intrinsic::hint_scope_exit;
  }

  static inline bool classof(const Value *V) {
    return isa<ScopeExit>(V) && classof(cast<ScopeExit>(V));
  }

  HintScopeEntry *getEntry() const {
    return dyn_cast_or_null<HintScopeEntry>(getArgOperand(0));
  }
};

#define DEFINE_SCOPE(Kind, Name, Tag)                                          \
  struct Name##Exit;                                                           \
  struct Name##Entry : public Kind##ScopeEntry {                               \
    static inline bool classof(const Kind##ScopeEntry *I) {                    \
      return I->getOperandBundle(#Tag) != None;                                \
    }                                                                          \
                                                                               \
    static inline bool classof(const Value *V) {                               \
      return isa<Kind##ScopeEntry>(V) &&                                       \
             classof(cast<Kind##ScopeEntry>(V));                               \
    }                                                                          \
                                                                               \
    ArrayRef<Use> getScopeAttrs() const {                                      \
      return getOperandBundle(#Tag).getValue().Inputs;                         \
    }                                                                          \
                                                                               \
    template <typename T> T *getScopeAttrs(unsigned i) const {                 \
      auto Attr = getScopeAttrs();                                             \
      if (Attr.size() <= i)                                                    \
        return nullptr;                                                        \
      return dyn_cast<T>(Attr[i]);                                             \
    }                                                                          \
                                                                               \
    StringRef getPragmaSrc() const {                                           \
      return getPragmaSource(#Tag);                                            \
    }                                                                          \
                                                                               \
    bool isUserPragma() const  {                                               \
      return "user" == getPragmaSrc();                                         \
    }                                                                          \
                                                                               \
    DebugLoc getPragmaDebugLoc( ) const {                                      \
      return getPragmaLoc( #Tag );                                             \
    }                                                                          \
                                                                               \
    static Name##Exit *Build##Name(Instruction &Entry, Instruction &Exit,      \
                                   ArrayRef<Value *> Operands = None) {        \
      return cast<Name##Exit>(Kind##ScopeEntry::Build##Kind##Scope(            \
          #Tag, Operands, Entry, Exit));                                       \
    }                                                                          \
                                                                               \
    static bool compatible(const OperandBundleDef &D) {                        \
      return D.getTag() == #Tag;                                               \
    }                                                                          \
    static const char *tag() { return #Tag; }                                  \
                                                                               \
  };                                                                           \
                                                                               \
  struct Name##Exit : public Kind##ScopeExit {                                 \
    static inline bool classof(const Kind##ScopeExit *I) {                     \
      return isa<Name##Entry>(I->getEntry());                                  \
    }                                                                          \
                                                                               \
    static inline bool classof(const Value *V) {                               \
      return isa<Kind##ScopeExit>(V) &&                                        \
             classof(cast<Kind##ScopeExit>(V));                                \
    }                                                                          \
                                                                               \
    Name##Entry *getEntry() const {                                            \
      return cast<Name##Entry>(Kind##ScopeExit::getEntry());                   \
    }                                                                          \
  };

DEFINE_SCOPE(Directive, SingleWorkItem, xcl_single_workitem)
DEFINE_SCOPE(Directive, UnrollWorkItem, xcl_unroll_workitems)
DEFINE_SCOPE(Directive, PipelineWorkItem, xcl_pipeline_workitems)
DEFINE_SCOPE(Directive, ImplicitBarrier, implicit_barrier)
DEFINE_SCOPE(Directive, SPMDRegion, fpga_spmd_region)
DEFINE_SCOPE(Directive, GlobalIdSLTRegion, global_id_slt)
DEFINE_SCOPE(Directive, GlobalIdULTRegion, global_id_ult)

DEFINE_SCOPE(Directive, PipelineStage, pipeline_stage)
DEFINE_SCOPE(Directive, OutlineRegion, xcl_outline)
DEFINE_SCOPE(Directive, LatencyRegion, xcl_latency)
DEFINE_SCOPE(Directive, PerformanceRegion, xlx_performance)
DEFINE_SCOPE(Directive, ExprBalanceRegion, xlx_expr_balance)
DEFINE_SCOPE(Directive, InlineRegion, xcl_inline)
DEFINE_SCOPE(Directive, OccurrenceRegion, xlx_occurrence)
DEFINE_SCOPE(Directive, ProtocolRegion, xlx_protocol)
DEFINE_SCOPE(Directive, LoopMergeRegion, xlx_merge_loop)
DEFINE_SCOPE(Directive, FunctionAllocationRegion, xlx_function_allocation)
DEFINE_SCOPE(Directive, ResourceRegion, fpga_resource_hint)
DEFINE_SCOPE(Directive, ResourceLimitRegion, fpga_resource_limit_hint)
DEFINE_SCOPE(Directive, ComputeRegion, fpga_compute_region)
DEFINE_SCOPE(Directive, InfiniteTask, xlx_infinite_task_def)
DEFINE_SCOPE(Directive, Task, xlx_task_def)

DEFINE_SCOPE(Hint, LatencyHintRegion, xcl_latency)
DEFINE_SCOPE(Hint, PerformanceHintRegion, xlx_performance)
DEFINE_SCOPE(Hint, ExprBalanceHintRegion, xlx_expr_balance)
DEFINE_SCOPE(Hint, OccurrenceHintRegion, xlx_occurrence)
DEFINE_SCOPE(Hint, LoopMergeHintRegion, xlx_merge_loop)
DEFINE_SCOPE(Hint, FunctionAllocationHintRegion, xlx_function_allocation)
DEFINE_SCOPE(Hint, ResourceHintRegion, fpga_resource_hint)
DEFINE_SCOPE(Hint, ResourceLimitHintRegion, fpga_resource_limit_hint)
DEFINE_SCOPE(Hint, InfiniteTaskHintRegion, xlx_infinite_task_def)
DEFINE_SCOPE(Hint, TaskHintRegion, xlx_task_def)

/// This represent the fpga_ssa_keep
class SSAKeepInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_ssa_keep;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  /// \brief Return the input
  Value *getValue() const { return getArgOperand(0); }
};

class SSACopyInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::ssa_copy;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  Value *getValue() const { return getArgOperand(0); }
  void fold() { replaceAllUsesWith(getValue()); }
};

class FPGAAnyInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_any;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  static CallInst* Create(Instruction *InsertBefore = nullptr, Module *M = nullptr) {
    Function *FpgaAnyFunc = Intrinsic::getDeclaration(
        M ? M : InsertBefore->getParent()->getParent()->getParent(),
        Intrinsic::fpga_any);

    CallInst *Call = CallInst::Create(
        FpgaAnyFunc, None,
        "", InsertBefore);
    return Call;
  }
};

#define DEFINE_SSA_ATTRIBUTE(Name, Tag)                                        \
  class Name##Attr : public SSACopyInst {                                      \
  public:                                                                      \
    static inline bool classof(const SSACopyInst *I) {                         \
      return I->getOperandBundle(#Tag) != None;                                \
    }                                                                          \
                                                                               \
    static inline bool classof(const Value *V) {                               \
      return isa<SSACopyInst>(V) && classof(cast<SSACopyInst>(V));             \
    }                                                                          \
    ArrayRef<Use> getAttrs() const {                                           \
      return getOperandBundle(#Tag).getValue().Inputs;                         \
    }                                                                          \
    template <typename T> T *getAttrs(unsigned i) const {                      \
      auto Attr = getAttrs();                                                  \
      if (Attr.size() <= i)                                                    \
        return nullptr;                                                        \
      return dyn_cast<T>(Attr[i]);                                             \
    }                                                                          \
  };

DEFINE_SSA_ATTRIBUTE(ArrayGeometry, xcl_array_geometry)
DEFINE_SSA_ATTRIBUTE(ArrayView, xcl_array_view)
DEFINE_SSA_ATTRIBUTE(ReadOnly, xcl_read_only)
DEFINE_SSA_ATTRIBUTE(WriteOnly, xcl_write_only)

// Intrinsics for pragmas
class PragmaInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::sideeffect &&
           I->hasOperandBundles();
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  static void setAttributesForCall(LLVMContext &Ctx, CallInst *Call,
                                   int64_t TypeSize = -1,
                                   StringRef Source = "infer-from-design") {
    AttrBuilder builder;
    if (TypeSize >= 0) {
      builder.addAttribute("xlx.port.bitwidth", std::to_string(TypeSize));
    }
    if (Source != "") {
      builder.addAttribute("xlx.source", Source);
    }
    if (builder.hasAttributes()) {
      AttributeList attr_list = AttributeList::get(Ctx, AttributeList::FunctionIndex, builder);
      Call->setAttributes(attr_list);
    }

    Call->setOnlyAccessesInaccessibleMemory();
    Call->setDoesNotThrow();
    Call->setWillReturn();
  }

  void setAttributesForPragma(int64_t TypeSize = -1,
                              StringRef Source = "infer-from-design") {
    setAttributesForCall(getContext(), this, TypeSize, Source);
  }

  // Create a PragmaInst and insert it before InsertBefore.
  // If InsertBefore is null, create PragmaInst but don't insert it. In this
  // case, Module M is necessary because we need it to get sideeffect
  // intrinsic function.
  template <typename PragmaInstType>
  static PragmaInstType *Create(ArrayRef<Value *> Options,
                                Instruction *InsertBefore = nullptr,
                                Module *M = nullptr,
                                int64_t TypeSize = -1,
                                StringRef Source = "infer-from-design") {
    return Create<PragmaInstType>(OperandBundleDef(PragmaInstType::BundleTagName, Options),
                                  InsertBefore, M, TypeSize, Source);
  }

  template <typename PragmaInstType>
  static PragmaInstType *Create(ArrayRef<OperandBundleDef> OpBundles,
                                Instruction *InsertBefore = nullptr,
                                Module *M = nullptr, int64_t TypeSize = -1,
                                StringRef Source = "infer-from-design") {
    Function *SideEffectF = Intrinsic::getDeclaration(
        M ? M : InsertBefore->getParent()->getParent()->getParent(),
        Intrinsic::sideeffect);
    assert(SideEffectF && "can't find llvm::Intrinsic::sideeffect");

    CallInst *Call =
        CallInst::Create(SideEffectF, None, OpBundles, "", InsertBefore);

    setAttributesForCall(SideEffectF->getContext(), Call, TypeSize, Source);

    // metadata
    if (InsertBefore)
      Call->setDebugLoc(InsertBefore->getDebugLoc());
    return cast<PragmaInstType>(Call);
  }

  template <typename PragmaInstType>
  static PragmaInstType *Create(ArrayRef<Value *> Options,
                                BasicBlock *InsertAtEnd,
                                int64_t TypeSize = -1,
                                StringRef Source = "infer-from-design") {
    Function *SideEffectF = Intrinsic::getDeclaration(
        InsertAtEnd->getParent()->getParent(), Intrinsic::sideeffect);
    assert(SideEffectF && "can't find llvm::Intrinsic::sideeffect");

    CallInst *Call = CallInst::Create(
        SideEffectF, None,
        OperandBundleDef(PragmaInstType::BundleTagName, Options), "",
        InsertAtEnd);

    setAttributesForCall(SideEffectF->getContext(), Call, TypeSize, Source);

    return cast<PragmaInstType>(Call);
  }

  // Check every bundle operands to see if this Pragma is for
  // some specified Value.
  bool isForSpecifiedValue() {
    for (unsigned i = 0, e = getNumOperandBundles(); i != e; ++i) {
      OperandBundleUse B = getOperandBundleAt(i);
      for (const Value *V : B.Inputs) {
        if (!isa<Constant>(V) || V->getType()->isPointerTy())
          return true;
      }
    }
    return false;
  }

  Value *getVariable() const;

  static void getAllPragmas(Value *V, SetVector<PragmaInst *> &PSet) {
    return get(V, PSet, true);
  }

  static void getAllPragmas(const Value *V,
                            SetVector<const PragmaInst *> &PSet) {
    return get(V, PSet, true);
  }

  static void getDirectPragmas(Value *V, SetVector<PragmaInst *> &PSet) {
    return get(V, PSet, false);
  }

  static void getDirectPragmas(const Value *V,
                            SetVector<const PragmaInst *> &PSet) {
    return get(V, PSet, false);
  }

  static PragmaInst *getAnyPragma(Value *V) { return get<PragmaInst>(V, true); }
  static PragmaInst *getAnyPragmaOnDeclaration(Value *V) { 
    auto PI = getAnyPragma(V);
    if (PI && PI->ShouldBeOnDeclaration())
      return PI;
    else
      return nullptr;
  }

  static const PragmaInst *getAnyPragma(const Value *V) {
    return get<PragmaInst>(V, true);
  }

  static const PragmaInst *getAnyPragmaOnDeclaration(const Value *V) { 
    auto PI = getAnyPragma(V);
    if (PI && PI->ShouldBeOnDeclaration())
      return PI;
    else
      return nullptr;
  }

  // Return true if this pragma should be applied on variable declaration site.
  bool ShouldBeOnDeclaration() const;

  // get port size for target variable
  // -1: means no such attribute
  // 0:  means don't know exact size
  // So both -1 and 0 are invalid value
  int64_t getPragmaVarAllocaSizeInBits() const {
    auto result = this->getAttributes().getAttribute(llvm::AttributeList::FunctionIndex, "xlx.port.bitwidth");
    StringRef Str = result.getValueAsString();
    if (Str.empty())
      return -1;
    return std::stoll(Str);
  }

  // set port size for target variable
  // -1: means no such attribute
  // 0:  means don't know exact size
  // So both -1 and 0 are invalid value
  // However, here allow to set 0, but not allow to set -1
  bool setPragmaVarAllocaSizeInBits(int64_t NewTypeSize) {
    if (NewTypeSize < 0)
      return false;
    setAttributesForPragma(NewTypeSize);
    return true;
  }

  // true: valid bit size
  // -1: means no such attribute
  // 0:  means don't know exact size
  // So both -1 and 0 are invalid value
  bool hasValidPragmaVarAllocaSizeInBits() const {
    return this->getPragmaVarAllocaSizeInBits() > 0;
  }

  uint64_t guessPragmaVarAllocaSizeInBits(const DataLayout &DL) const;

  StringRef getPragmaSource() const {
    auto result = getAttributes().getAttribute(llvm::AttributeList::FunctionIndex, "xlx.source");
    return result.getValueAsString();
  }

  bool isUserPragma() const;

protected:
  template <typename ValueT, typename PragmaInstType>
  static void get(ValueT *V, SetVector<PragmaInstType *> &PSet,
                  bool PopulateGEP) {
    for (auto *U : V->users()) {
      if (auto *BC = dyn_cast<BitCastOperator>(U)) {
        get(BC, PSet, PopulateGEP);
      } else if (auto *GEP = dyn_cast<GEPOperator>(U)) {
        if (PopulateGEP)
          get(GEP, PSet, PopulateGEP);
      } else if (auto * Extract = dyn_cast<ExtractValueInst>(U)) { 
        if (PopulateGEP)
          get(Extract, PSet, PopulateGEP);
      } else if (auto *PI = dyn_cast<PragmaInstType>(U)) {
        PSet.insert(PI);
      }
    }
  }

  template <typename PragmaInstType>
  static const PragmaInstType *get(const Value *V, bool PopulateGEP) {
    SetVector<const PragmaInstType *> PSet;
    get(V, PSet, PopulateGEP);
    return PSet.empty() ? nullptr : *PSet.begin();
  }

  template <typename PragmaInstType>
  static PragmaInstType *get(Value *V, bool PopulateGEP) {
    SetVector<PragmaInstType *> PSet;
    get(V, PSet, PopulateGEP);
    return PSet.empty() ? nullptr : *PSet.begin();
  }
};

class AXISChannelInst: public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  void getChannels(SmallVectorImpl<Value *> &ChannelVals) const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    ChannelVals.append(Bundle.getValue().Inputs.begin(),
                       Bundle.getValue().Inputs.end());
  }
};

class DependenceInst : public PragmaInst {
public:
  enum class Direction { NODIR = -1, RAW = 0, WAR = 1, WAW = 2 };
  enum class DepType { INTRA, INTER };

  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  void getOptions(SmallVectorImpl<Value *> &Options) {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    for (auto &U : Bundle.getValue().Inputs) {
      Options.push_back(U);
    }
  }

  Value *getVariable() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    Value *V = Bundle.getValue().Inputs[0];
    // A valid variable in dependence pragma must have pointer type
    return V->getType()->isPointerTy() ? V : nullptr;
  }

  int32_t getClass() const {
    // class option
    // 0: No class set; 1: Array; 2: Pointer
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    return cast<ConstantInt>(Bundle.getValue().Inputs[1])->getSExtValue();
  }

  bool isEnforced() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    auto *isEnforced = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return isEnforced->getZExtValue();
  }

  Direction getDirection() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    auto *Dir = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    int64_t DirCode = Dir->getSExtValue();
    assert((DirCode >= -1) && (DirCode <= 2) &&
            "unexpected dependence pragma direction!");
    return static_cast<Direction>(DirCode);
  }

  int32_t getDistance() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    return cast<ConstantInt>(Bundle.getValue().Inputs[4])->getSExtValue();
  }

  DepType getType() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    auto *Ty = cast<ConstantInt>(Bundle.getValue().Inputs[5]);
    uint64_t DepTypeCode = Ty->getSExtValue();
    assert((DepTypeCode <= 1) &&
            "unexpected dependence pragma type!");
    return static_cast<DepType>(DepTypeCode);
  }

  bool isUserPragma() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    return !cast<ConstantInt>(Bundle.getValue().Inputs[6])->isZero();
  }

  static DependenceInst *get(Value *V) {
    return PragmaInst::get<DependenceInst>(V, true);
  }

  static const DependenceInst *get(const Value *V) {
    return PragmaInst::get<DependenceInst>(V, true);
  }

  static void get(Value *V, SetVector<DependenceInst *> &PSet) {
    return PragmaInst::get(V, PSet, true);
  }

  static void get(const Value *V,
                  SetVector<const DependenceInst *> &PSet) {
    return PragmaInst::get(V, PSet, true);
  }

};

class FuncInstantiateInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  static FuncInstantiateInst *get(Value *V) {
    return PragmaInst::get<FuncInstantiateInst>(V, true);
  }

  static const FuncInstantiateInst *get(const Value *V) {
    return PragmaInst::get<FuncInstantiateInst>(V, true);
  }
};

class ArrayStencilInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  static ArrayStencilInst *get(Value *V) {
    return PragmaInst::get<ArrayStencilInst>(V, true);
  }

  static const  ArrayStencilInst *get(const Value *V) {
    return PragmaInst::get<ArrayStencilInst>(V, true);
  }

  bool isOff() const {
    if (!isValidInst())
      assert(0 && "Illegal array_stencil intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *OffVal = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return OffVal->isOne();
  }

private:
  unsigned getNumArgs() const {
    return 2;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};

class MAXIAliasInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  static MAXIAliasInst *get(Value *V) {
    return PragmaInst::get<MAXIAliasInst>(V, true);
  }

  static const MAXIAliasInst *get(const Value *V) {
    return PragmaInst::get<MAXIAliasInst>(V, true);
  }

  static std::pair<const MAXIAliasInst *, int64_t> getWithOffset(const Value *V) {
    for (auto *U : V->users()) {
      if (auto *BC = dyn_cast<BitCastOperator>(U)) {
        auto P = getWithOffset(BC);
        if (P.first)
          return P;
      } else if (auto *GEP = dyn_cast<GEPOperator>(U)) {
        auto P = getWithOffset(GEP);
        if (P.first)
          return P;
      } else if (auto * Extract = dyn_cast<ExtractValueInst>(U)) {
        auto P = getWithOffset(Extract);
        if (P.first)
          return P;
      } else if (auto *PI = dyn_cast<MAXIAliasInst>(U)) {
        return std::make_pair(PI, PI->getOffset(V));
      }
    }

    return {nullptr, 0};
  }

  void getOptions(SmallVectorImpl<Value *> &Options) {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal alias intrinsic");
    for (auto &U : Bundle.getValue().Inputs) {
      Options.push_back(U);
    }
  }

  void getVariables(SmallVectorImpl<Value *> &Vars) const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal alias intrinsic");
    for (Value *V : Bundle.getValue().Inputs) {
      if (V->getType()->isPointerTy())
        Vars.push_back(V);
    }
  }

  void getOffsets(SmallVectorImpl<int64_t> &Offsets) const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal alias intrinsic");
    for (Value *V : Bundle.getValue().Inputs) {
      if (V->getType()->isPointerTy() == false)
        Offsets.push_back(cast<ConstantInt>(V)->getSExtValue());
    }
  }

private:

  int64_t getOffset(const Value *V) const {

    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal maxi alias intrinsic");
    size_t index = 0;
    for (auto &U : Bundle.getValue().Inputs) {
      if(U == V) break;
      ++index;
    }

    assert(index != Bundle.getValue().Inputs.size() && "Illegal value for maxi alias intrinsic");
    auto offsetIndex = index + Bundle.getValue().Inputs.size() / 2;
    assert(offsetIndex < Bundle.getValue().Inputs.size() && "Illegal offset index for maxi alias intrinsic");

    return cast<ConstantInt>(Bundle.getValue().Inputs[offsetIndex])->getSExtValue();
  }

};

class CrossDependenceInst : public PragmaInst {
public:
  enum class Direction { NODIR = -1, RAW = 0, WAR = 1, WAW = 2 };
  enum class DepType { INTRA, INTER };

  static const std::string BundleTagName;
  static const unsigned VarNum = 2;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  void getOptions(SmallVectorImpl<Value *> &Options) {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    for (auto &U : Bundle.getValue().Inputs) {
      Options.push_back(U);
    }
  }

  void getVariables(SmallVectorImpl<Value *> &Vars) const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    for (Value *V : Bundle.getValue().Inputs) {
      if (V->getType()->isPointerTy())
        Vars.push_back(V);
    }
  }

  int64_t getClass() const {
    // class option
    // 0: No class set; 1: Array; 2: Pointer
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal cross_dependence intrinsic");
    return cast<ConstantInt>(Bundle.getValue().Inputs[(VarNum-1)+1])->getSExtValue();
  }

  bool isEnforced() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal cross_dependence intrinsic");
    auto *isEnforced = cast<ConstantInt>(Bundle.getValue().Inputs[(VarNum-1)+2]);
    return isEnforced->getZExtValue();
  }

  Direction getDirection() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal cross_dependence intrinsic");
    auto *Dir = cast<ConstantInt>(Bundle.getValue().Inputs[(VarNum-1)+3]);
    int64_t DirCode = Dir->getSExtValue();
    assert((DirCode >= -1) && (DirCode <= 2) &&
            "unexpected cross_dependence pragma direction!");
    return static_cast<Direction>(DirCode);
  }

  int64_t getDistance() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal cross_dependence intrinsic");
    return cast<ConstantInt>(Bundle.getValue().Inputs[(VarNum-1)+4])->getSExtValue();
  }

  DepType getType() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal cross_dependence intrinsic");
    auto *Ty = cast<ConstantInt>(Bundle.getValue().Inputs[(VarNum-1)+5]);
    uint64_t DepTypeCode = Ty->getSExtValue();
    assert((DepTypeCode <= 1) &&
            "unexpected cross_dependence pragma type!");
    return static_cast<DepType>(DepTypeCode);
  }

};

class StableInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  static inline unsigned getConstValueNum() { return 0; }

  void getStables(SmallVectorImpl<Value *> &Stables) {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal stable intrinsic");
    for (auto &U : Bundle.getValue().Inputs) {
      Stables.push_back(U);
    }
  }
};

class StableContentInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  static inline unsigned getConstValueNum() { return 0; }

  void getStableContents(SmallVectorImpl<Value *> &StableContents) {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal stable content intrinsic");
    for (auto &U : Bundle.getValue().Inputs) {
      StableContents.push_back(U);
    }
  }
};

class SharedInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  void getSharedVals(SmallVectorImpl<Value *> &SharedVals) {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal shared intrinsic");
    for (auto &U : Bundle.getValue().Inputs) {
      SharedVals.push_back(U);
    }
  }

  static SharedInst *get(Value *V) {
    return PragmaInst::get<SharedInst>(V, true);
  }

  static const SharedInst *get(const Value *V) {
    return PragmaInst::get<SharedInst>(V, true);
  }
};

class DisaggrInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  void getDisaggrVals(SmallVectorImpl<Value *> &DisaggrVals) {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal disaggregate intrinsic");
    for (auto &U : Bundle.getValue().Inputs) {
      DisaggrVals.push_back(U);
    }
  }

  static DisaggrInst *get(Value *V) {
    return PragmaInst::get<DisaggrInst>(V, true);
  }

  static const DisaggrInst *get(const Value *V) {
    return PragmaInst::get<DisaggrInst>(V, true);
  }
};

class AggregateInst : public PragmaInst {
public:
  static const std::string BundleTagName;

public:
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  Value* getVariable() {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    return Bundle.getValue().Inputs[0];
  }
  // 0: none
  // 1: bit
  // 2: byte
  // 3: default
  int64_t getCompact() { 
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    if (Bundle.getValue().Inputs.size() <= 1)
      return 3; // default mode
    assert(isa<ConstantInt>(Bundle.getValue().Inputs[1]));
    return cast<ConstantInt>(Bundle.getValue().Inputs[1])->getSExtValue();
  }

  static AggregateInst *get(Value *V) {
    return PragmaInst::get<AggregateInst>(V, true);
  }

  static const AggregateInst *get(const Value *V) {
    return PragmaInst::get<AggregateInst>(V, true);
  }
};

// CRTP
// TODO: allow more than one variable in the same operand bundle
template <class SpecificXFromInst> class ArrayXFormInst : public PragmaInst {
public:
  enum XFormMode { AlreadyTouched = 998, Off = 999,
                   Cyclic = 0, Block = 1, Complete = 2 };

  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::sideeffect &&
           I->getOperandBundle(SpecificXFromInst::BundleTagName);
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  StringRef getMode() const {
    Optional<OperandBundleUse> Bundle =
        getOperandBundle(SpecificXFromInst::BundleTagName);
    assert(Bundle && "Illegal array transform intrinsic");
    ConstantInt *Type = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    switch (Type->getZExtValue()) {
    case AlreadyTouched:
      return "already_touched";
    case Off:
      return "off";
    case Cyclic:
      return "cyclic";
    case Block:
      return "block";
    case Complete:
      return "complete";
    }
    llvm_unreachable("unexpected array transfrom type!");
    return "";
  }

  int getFactor() const {
    Optional<OperandBundleUse> Bundle =
        getOperandBundle(SpecificXFromInst::BundleTagName);
    assert(Bundle && "Illegal array transform intrinsic");
    ConstantInt *Factor = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return Factor->getZExtValue();
  }

  int getDim() const {
    Optional<OperandBundleUse> Bundle =
        getOperandBundle(SpecificXFromInst::BundleTagName);
    assert(Bundle && "Illegal array transform intrinsic");
    ConstantInt *dim = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return dim->getZExtValue();
  }
};

class ArrayPartitionInst : public ArrayXFormInst<ArrayPartitionInst> {
public:
  static const std::string BundleTagName;
  static inline unsigned getConstValueNum() { return 4; }

  bool isDynamic() const {
    Optional<OperandBundleUse> Bundle =
        getOperandBundle(ArrayPartitionInst::BundleTagName);
    assert(Bundle && "Illegal array partition intrinsic");
    Value *V = nullptr;
    V = Bundle.getValue().Inputs[4];
    if (!V || isa<ConstantAggregateZero>(V))
      return false;
    else
      return cast<ConstantInt>(V)->isOne();
  }

  static void get(Value *V, SetVector<ArrayPartitionInst *> &PSet) {
    return PragmaInst::get(V, PSet, true);
  }

  static void get(const Value *V,
                  SetVector<const ArrayPartitionInst *> &PSet) {
    return PragmaInst::get(V, PSet, true);
  }

  static ArrayPartitionInst *get(Value *V) {
    return PragmaInst::get<ArrayPartitionInst>(V, true);
  }

  static const ArrayPartitionInst *get(const Value *V) {
    return PragmaInst::get<ArrayPartitionInst>(V, true);
  }
};

class ArrayReshapeInst : public ArrayXFormInst<ArrayReshapeInst> {
public:
  static const std::string BundleTagName;
  static inline unsigned getConstValueNum() { return 3; }

  static void get(Value *V, SetVector<ArrayReshapeInst *> &PSet) {
    return PragmaInst::get(V, PSet, true);
  }

  static void get(const Value *V,
                  SetVector<const ArrayReshapeInst *> &PSet) {
    return PragmaInst::get(V, PSet, true);
  }

  static ArrayReshapeInst *get(Value *V) {
    return PragmaInst::get<ArrayReshapeInst>(V, true);
  }

  static const ArrayReshapeInst *get(const Value *V) {
    return PragmaInst::get<ArrayReshapeInst>(V, true);
  }
};

class StreamPragmaInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  static inline unsigned getConstValueNum() { return 1; }

  int32_t getDepth() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    ConstantInt *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return (int32_t)Depth->getSExtValue();
  }

  Value *getStream() const { return getVariable(); }

  static void get(Value *V, SetVector<StreamPragmaInst *> &PSet,
                  bool PopulateGEP = false) {
    return PragmaInst::get(V, PSet, PopulateGEP);
  }

  static void get(const Value *V,
                  SetVector<const StreamPragmaInst *> &PSet,
                  bool PopulateGEP = false) {
    return PragmaInst::get(V, PSet, PopulateGEP);
  }

  static StreamPragmaInst *get(Value *V, bool PopulateGEP = false) {
    return PragmaInst::get<StreamPragmaInst>(V, PopulateGEP);
  }

  static const StreamPragmaInst *get(const Value *V, bool PopulateGEP = false) {
    return PragmaInst::get<StreamPragmaInst>(V, PopulateGEP);
  }
};

class ScalarStreamInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  static inline unsigned getConstValueNum() { return 2; }

  int32_t getDepth() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    ConstantInt *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return (int32_t)Depth->getSExtValue();
  }

  bool isOff() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    ConstantInt *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return Depth->getSExtValue() > 0;
  }

  Value *getStream() const { return getVariable(); }
#endif

  static void get(Value *V, SetVector<ScalarStreamInst *> &PSet,
                  bool PopulateGEP = false) {
    return PragmaInst::get(V, PSet, PopulateGEP);
  }

  static void get(const Value *V,
                  SetVector<const ScalarStreamInst *> &PSet,
                  bool PopulateGEP = false) {
    return PragmaInst::get(V, PSet, PopulateGEP);
  }

  static ScalarStreamInst *get(Value *V, bool PopulateGEP = false) {
    return PragmaInst::get<ScalarStreamInst>(V, PopulateGEP);
  }

  static const ScalarStreamInst *get(const Value *V, bool PopulateGEP = false) {
    return PragmaInst::get<ScalarStreamInst>(V, PopulateGEP);
  }
};

class StreamOfBlocksPragmaInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  int32_t getDepth() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Operand bundle not found");
    ConstantInt *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return (int32_t)Depth->getSExtValue();
  }

  Value *getStream() const { return getVariable(); }
  static StreamOfBlocksPragmaInst *get(Value *V, bool PopulateGEP = false) {
    return PragmaInst::get<StreamOfBlocksPragmaInst>(V, PopulateGEP);
  }

  static const StreamOfBlocksPragmaInst *get(
                   const Value *V, bool PopulateGEP = false) {
    return PragmaInst::get<StreamOfBlocksPragmaInst>(V, PopulateGEP);
  }
};

class PipoPragmaInst : public PragmaInst {
public:
  enum PipoType {
    PIPO = 1,
    SHARED = 2,
    UNSYNC = 3
  };

  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  static inline unsigned getConstValueNum() { return 2; }

  Value *getPipo() const { return getVariable(); }
  int getDepth() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal stream pipo intrinsic");
    ConstantInt *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return (int)Depth->getSExtValue();
  }

  int getType() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal stream pipo intrinsic");
    ConstantInt *Type = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return (int)Type->getSExtValue();
  }

  static void get(Value *V, SetVector<PipoPragmaInst *> &PSet,
                  bool PopulateGEP = false) {
    return PragmaInst::get(V, PSet, PopulateGEP);
  }

  static void get(const Value *V,
                  SetVector<const PipoPragmaInst *> &PSet,
                  bool PopulateGEP = false) {
    return PragmaInst::get(V, PSet, PopulateGEP);
  }

  static PipoPragmaInst *get(Value *V, bool PopulateGEP = false) {
    return PragmaInst::get<PipoPragmaInst>(V, PopulateGEP);
  }

  static const PipoPragmaInst *get(const Value *V, bool PopulateGEP = false) {
    return PragmaInst::get<PipoPragmaInst>(V, PopulateGEP);
  }
};

class BindOpPragmaInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }
  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }
  static inline const BindOpPragmaInst *get(Value *V) {
    return PragmaInst::get<BindOpPragmaInst>(V, false);
  }

  static inline const BindOpPragmaInst *get(const Value *V) {
    return PragmaInst::get<BindOpPragmaInst>(V, false);
  }
  static void get(Value *V, SetVector<BindOpPragmaInst *> &PSet) {
    return PragmaInst::get(V, PSet, false);
  }

  static void get(const Value *V,
                  SetVector<const BindOpPragmaInst *> &PSet) {
    return PragmaInst::get(V, PSet, false);
  }

  static inline unsigned getConstValueNum() { return 3; }

  platform::PlatformBasic::OP_TYPE getOp() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    ConstantInt *Op = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return (platform::PlatformBasic::OP_TYPE)Op->getSExtValue();
  }
  platform::PlatformBasic::IMPL_TYPE getImpl() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    ConstantInt *Impl = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return (platform::PlatformBasic::IMPL_TYPE)Impl->getSExtValue();
  }
  int getLatency() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    ConstantInt *Latency = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return Latency->getSExtValue();
  }
};

class BindStoragePragmaInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }
  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }
  static inline const BindStoragePragmaInst *get(Value *V, 
                                                 bool PopulateGEP = false) {
    return PragmaInst::get<BindStoragePragmaInst>(V, PopulateGEP);
  }

  static inline const BindStoragePragmaInst *get(const Value *V,
                                                 bool PopulateGEP = false) {
    return PragmaInst::get<BindStoragePragmaInst>(V, PopulateGEP);
  }
  static void get(Value *V, SetVector<BindStoragePragmaInst *> &PSet,
                  bool PopulateGEP = false) {
    return PragmaInst::get(V, PSet, PopulateGEP);
  }

  static void get(const Value *V,
                  SetVector<const BindStoragePragmaInst *> &PSet,
                  bool PopulateGEP = false) {
    return PragmaInst::get(V, PSet, PopulateGEP);
  }

  static inline unsigned getConstValueNum() { return 3; }

  platform::PlatformBasic::OP_TYPE getOp() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    ConstantInt *Op = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return (platform::PlatformBasic::OP_TYPE)Op->getSExtValue();
  }
  platform::PlatformBasic::IMPL_TYPE getImpl() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    ConstantInt *Impl = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return (platform::PlatformBasic::IMPL_TYPE)Impl->getSExtValue();
  }
  int getLatency() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal aggregate intrinsic");
    ConstantInt *Latency = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return Latency->getSExtValue();
  }

  bool supportByteEnable() const {
    auto *XilinxPlatform = platform::PlatformBasic::getInstance();
    platform::PlatformBasic::CoreBasic *Core = 
        XilinxPlatform->getCoreFromOpImpl(getOp(), getImpl());
    return !Core || Core->supportByteEnable();
  }

  bool isInitializable() const {
    auto *XilinxPlatform = platform::PlatformBasic::getInstance();
    platform::PlatformBasic::CoreBasic *Core = 
        XilinxPlatform->getCoreFromOpImpl(getOp(), getImpl());
    return !Core || Core->isInitializable();
  }

  bool isInitializableByAllZeros() const {
    auto *XilinxPlatform = platform::PlatformBasic::getInstance();
    platform::PlatformBasic::CoreBasic *Core = 
        XilinxPlatform->getCoreFromOpImpl(getOp(), getImpl());
    return !Core || Core->isInitializableByAllZeros();
  }
};

class ConstSpecInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  static ConstSpecInst *get(Value *V) {
    return PragmaInst::get<ConstSpecInst>(V, false);
  }

  static const ConstSpecInst *get(const Value *V) {
    return PragmaInst::get<ConstSpecInst>(V, false);
  }
};
class FPGAResourceLimitInst: public PragmaInst{ 
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }
  StringRef getInstanceName() const{ 
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert( Bundle && "Illegal ConstSpec intrinsic" );
    StringRef instanceName = dyn_cast<ConstantDataArray>(Bundle.getValue().Inputs[0])->getAsString();
    return instanceName;
  }
  int getInstanceType() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert( Bundle && "Illegal ConstSpec intrinsic" );
    StringRef InstanceType = dyn_cast<ConstantDataArray>(Bundle.getValue().Inputs[1])->getAsString();
    if (InstanceType.contains_lower("operation")) { 
      return 0;
    }
    else if (InstanceType.contains_lower("core")) { 
      return 1;
    }
    else { 
      llvm_unreachable("unexpected Allocation type" );
    }
  }
  int getLimit() const { 
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert( Bundle && "Illegal ConstSpec intrinsic" );
    ConstantInt* limit = dyn_cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return limit->getSExtValue();
  }
};

class XlxFunctionAllocationInst: public PragmaInst { 
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }
  // return the function in tag 'xlx_function_allocation'
  Function *getFunction() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal ConstSpec intrinsic");
    Function *func = dyn_cast<Function>(Bundle.getValue().Inputs[0]);
    return func;
  }

  // collect all functions in this intrinsic
  void getFunctions(SetVector<Function *> &Set) const {
    SmallVector<OperandBundleDef, 2> Bundles;
    this->getOperandBundlesAsDefs(Bundles);
    assert(Bundles.size() >= 1 && "No operand bundle?!");
    Set.insert(this->getFunction());
    if (Bundles.size() == 1)
      return;
    for (auto *V : Bundles[1].inputs())
      Set.insert(cast<Function>(V));
  }

  StringRef getFunctionString() const{
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert( Bundle && "Illegal ConstSpec intrinsic" );
    Value *V = Bundle.getValue().Inputs[1];
    if (auto *GV = dyn_cast<GlobalVariable>(V))
      V = GV->getInitializer();
    if (auto *CDS = dyn_cast<ConstantDataSequential>(V))
      return CDS->getRawDataValues();
    return StringRef();
  }

  int32_t getLimit() const { 
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert( Bundle && "Illegal ConstSpec intrinsic" );
    ConstantInt* limit = dyn_cast<ConstantInt>( Bundle.getValue().Inputs[2]);
    return limit->getSExtValue();
  }
  // create allocation intrinsic based on \p OldAllocationIntr
  static XlxFunctionAllocationInst *
  Create(ArrayRef<Function *> Fns,
         XlxFunctionAllocationInst *OldAllocationIntr) {
    auto Source = OldAllocationIntr->getPragmaSource();

    SmallVector<OperandBundleDef, 2> Bundles;
    OldAllocationIntr->getOperandBundlesAsDefs(Bundles);
    SmallVector<Value *, 3> NewBundleVals(Bundles[0].input_begin() + 1,
                                          Bundles[0].input_end());
    NewBundleVals.insert(NewBundleVals.begin(), Fns[0]);
    SmallVector<OperandBundleDef, 2> NewBundles;
    NewBundles.emplace_back(Bundles[0].getTag(), NewBundleVals);

    if (1 == Fns.size())
      return PragmaInst::Create<XlxFunctionAllocationInst>(NewBundles,
                                                           OldAllocationIntr,
                                                           nullptr, -1, Source);
    // 2 operand bundles
    NewBundleVals.clear();
    for (unsigned i = 1; i < Fns.size(); i++)
      NewBundleVals.push_back(Fns[i]);
    NewBundles.emplace_back("allocation_dup_list", NewBundleVals);
    return PragmaInst::Create<XlxFunctionAllocationInst>(NewBundles,
                                                         OldAllocationIntr,
                                                         nullptr, -1, Source);
  }

  static XlxFunctionAllocationInst *get(Value *V) {
    return PragmaInst::get<XlxFunctionAllocationInst>(V, false);
  }

  static const XlxFunctionAllocationInst *get(const Value *V) {
    return PragmaInst::get<XlxFunctionAllocationInst>(V, false);
  }

  // collect allocation pragmas (in corresponding region if it's specified)
  static void getAll(Function *F,
                     SmallVectorImpl<XlxFunctionAllocationInst *> &Allocations,
                     Function *RegionF = nullptr);
};

#define DEFINE_LABEL(Name)                                                     \
  class Name##LabelInst : public PragmaInst {                                  \
   public:                                                                     \
    static const std::string BundleTagName;                                    \
    static bool classof(const PragmaInst *I) {                                 \
      return I->getOperandBundle(BundleTagName).hasValue();                    \
    }                                                                          \
                                                                               \
    static inline bool classof(const Value *V) {                               \
      return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));               \
    }                                                                          \
                                                                               \
    static Name##LabelInst *get(Value *V, bool Indirect = true) {              \
      return PragmaInst::get<Name##LabelInst>(V, Indirect);                    \
    }                                                                          \
                                                                               \
    static const Name##LabelInst *get(const Value *V, bool Indirect = true) {  \
      return PragmaInst::get<Name##LabelInst>(V, Indirect);                    \
    }                                                                          \
                                                                               \
    int getDim() const {                                                       \
      Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);     \
      assert(Bundle && "Illegal label intrinsic");                             \
      if (Bundle.getValue().Inputs.size() <= 1)                                \
        return 0;                                                              \
      ConstantInt *dim = cast<ConstantInt>(Bundle.getValue().Inputs[1]);       \
      return dim->getZExtValue();                                              \
    }                                                                          \
  };

DEFINE_LABEL(Stream)
DEFINE_LABEL(StreamOfBlocks)
DEFINE_LABEL(ShiftReg)
DEFINE_LABEL(DirectIO)

// pre-declare
class SAXIInst;
class MaxiInst;
class AxiSInst;
class ApFifoInst;
class ApMemoryInst;
class BRAMInst;
class ApStableInst;
class ApNoneInst;
class ApAckInst;
class ApVldInst;
class ApOvldInst;
class ApHsInst;
class ApAutoInst;
class ApCtrlNoneInst;
class ApCtrlChainInst;
class ApCtrlHsInst;

////////////////////////////////////////
// Interface Intrinsic
////////////////////////////////////////
class InterfaceInst : public PragmaInst {
public:
  static bool classof(const PragmaInst *I);
  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }
  // Since all interface related sideeffect all have signal name option
  StringRef getSignalName() const {

    SmallVector<OperandBundleDef, 1> BundleDefs;
    this->getOperandBundlesAsDefs(BundleDefs);
    assert(BundleDefs.size() == 1 &&
           "More than one bundle in the same intrinsic?");
    auto BundleDef = BundleDefs[0];

    Value *V = nullptr;
    // op 4
    if (isa<SAXIInst>(this) || isa<MaxiInst>(this) || isa<AxiSInst>(this) ||
        isa<ApMemoryInst>(this) || isa<BRAMInst>(this))
      V = BundleDef.inputs()[4];
    // op 2
    else if (isa<ApFifoInst>(this) || isa<ApStableInst>(this) ||
             isa<ApNoneInst>(this) || isa<ApAckInst>(this) ||
             isa<ApVldInst>(this) || isa<ApOvldInst>(this) ||
             isa<ApHsInst>(this) || isa<ApAutoInst>(this))
      V = BundleDef.inputs()[2];
    // op 1
    else if (isa<ApCtrlNoneInst>(this) || isa<ApCtrlChainInst>(this) ||
             isa<ApCtrlHsInst>(this))
      V = BundleDef.inputs()[1];
    else
      llvm_unreachable("Other interface intrinsic?!");

    if (isa<GlobalVariable>(V))
      V = cast<GlobalVariable>(V)->getInitializer();
    if (isa<ConstantAggregateZero>(V))
      return "";
    else
      return cast<ConstantDataSequential>(V)->getRawDataValues();
  }

  void setSignalName(StringRef newName) {
    auto newV =
        ConstantDataArray::getString(this->getContext(), newName, false);
    if (isa<SAXIInst>(this) || isa<MaxiInst>(this) || isa<AxiSInst>(this) ||
        isa<ApMemoryInst>(this) || isa<BRAMInst>(this))
      this->setOperand(4, newV);
    // op 2
    else if (isa<ApFifoInst>(this) || isa<ApStableInst>(this) ||
             isa<ApNoneInst>(this) || isa<ApAckInst>(this) ||
             isa<ApVldInst>(this) || isa<ApOvldInst>(this) ||
             isa<ApHsInst>(this) || isa<ApAutoInst>(this))
      this->setOperand(2, newV);
    // op 1
    else if (isa<ApCtrlNoneInst>(this) || isa<ApCtrlChainInst>(this) ||
             isa<ApCtrlHsInst>(this))
      this->setOperand(1, newV);
    else
      llvm_unreachable("Other interface intrinsic?!");
  }

  int getRegister() const {

    SmallVector<OperandBundleDef, 1> BundleDefs;
    this->getOperandBundlesAsDefs(BundleDefs);
    assert(BundleDefs.size() == 1 &&
           "More than one bundle in the same intrinsic?");
    auto BundleDef = BundleDefs[0];

    Value *V = nullptr;
    // op 3
    if (isa<SAXIInst>(this))
      V = BundleDef.inputs()[3];
    // op 1
    else if (isa<AxiSInst>(this) || isa<ApFifoInst>(this) ||
             isa<ApNoneInst>(this) || isa<ApAckInst>(this) ||
             isa<ApVldInst>(this) || isa<ApOvldInst>(this) ||
             isa<ApHsInst>(this) || isa<ApStableInst>(this) || isa<ApAutoInst>(this))
      V = BundleDef.inputs()[1];
    else
      llvm_unreachable("Other unsupport interface for register!");

    if (!V || isa<ConstantAggregateZero>(V))
      return 0;
    else
      return cast<ConstantInt>(V)->getSExtValue();
  }

  bool hasRegister() const {

    SmallVector<OperandBundleDef, 1> BundleDefs;
    this->getOperandBundlesAsDefs(BundleDefs);
    assert(BundleDefs.size() == 1 &&
           "More than one bundle in the same intrinsic?");
    auto BundleDef = BundleDefs[0];

    Value *V = nullptr;
    // op 3
    if (isa<SAXIInst>(this))
      V = BundleDef.inputs()[3];
    // op 1
    else if (isa<AxiSInst>(this) || isa<ApFifoInst>(this) ||
             isa<ApNoneInst>(this) || isa<ApAckInst>(this) ||
             isa<ApVldInst>(this) || isa<ApOvldInst>(this) ||
             isa<ApHsInst>(this) || isa<ApStableInst>(this) || isa<ApAutoInst>(this))
      V = BundleDef.inputs()[1];
    else
      llvm_unreachable("Other unsupport interface for register!");

    if (!V || isa<ConstantAggregateZero>(V))
      return false;
    else
      return cast<ConstantInt>(V)->isOne();
  }

};

class SAXIInst : public InterfaceInst {
public:
  static const std::string BundleTagName;
  static bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  StringRef getBundleName() const {
    if (!isValidInst())
      assert(0 && "Illegal s_axilite intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    Value *V = Bundle.getValue().Inputs[1];
    if (auto *GV = dyn_cast<GlobalVariable>(V))
      V = GV->getInitializer();
    if (auto *CDS = dyn_cast<ConstantDataSequential>(V))
      return CDS->getRawDataValues();
    return StringRef();
  }

  void setBundle(StringRef Bundle) {
    auto newV = ConstantDataArray::getString(
                       this->getContext(), 
                       Bundle, 
                       false);
    this->setOperand(1, newV);
  }

  int64_t getOffset() const {
    if (!isValidInst())
      assert(0 && "Illegal s_axilite intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Offset = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return Offset->getSExtValue();
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal s_axilite intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return Reg->isOne();
  }
#endif

  StringRef getClockName() const {
    if (!isValidInst())
      assert(0 && "Illegal s_axilite intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    Value *V = Bundle.getValue().Inputs[5];
    if (isa<GlobalVariable>(V))
      V = cast<GlobalVariable>(V)->getInitializer();
    if (isa<ConstantAggregateZero>(V))
      return "";
    else
      return cast<ConstantDataSequential>(V)->getRawDataValues();
  }

  void setClockName(StringRef Clock) {
    auto newV = ConstantDataArray::getString(
                       this->getContext(), 
                       Clock, 
                       false);
    this->setOperand(5, newV);
  }

  StringRef getImplName() const {
    if (!isValidInst())
      assert(0 && "Illegal s_axilite intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    Value *V = Bundle.getValue().Inputs[6];
    if (isa<GlobalVariable>(V))
      V = cast<GlobalVariable>(V)->getInitializer();
    if (isa<ConstantAggregateZero>(V))
      return "";
    else
      return cast<ConstantDataSequential>(V)->getRawDataValues();
  }

  void setImplName(StringRef Impl) {
    auto newV = ConstantDataArray::getString(
                       this->getContext(), 
                       Impl, 
                       false);
    this->setOperand(6, newV);
  }

  // get call intrinsic from root value
  static void get(Value *V, SetVector<SAXIInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const SAXIInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static SAXIInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<SAXIInst>(V, Indirect);
  }

  static const SAXIInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<SAXIInst>(V, Indirect);
  }



private:
  unsigned getNumArgs() const {
    return 7;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};

class MaxiInst : public InterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  StringRef getBundleName() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    Value *V = Bundle.getValue().Inputs[1];
    if (isa<GlobalVariable>(V))
      V = cast<GlobalVariable>(V)->getInitializer();
    if (isa<ConstantAggregateZero>(V))
      return "";
    else
      return cast<ConstantDataSequential>(V)->getRawDataValues();
  }

  void setBundle(StringRef Bundle) {
    auto newV = ConstantDataArray::getString(
                       this->getContext(), 
                       Bundle, 
                       false);
    this->setOperand(1, newV);
  }

  int64_t getDepth() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return Depth->getSExtValue();
  }

  StringRef getOffset() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    Value *V = Bundle.getValue().Inputs[3];
    if (isa<GlobalVariable>(V))
      V = cast<GlobalVariable>(V)->getInitializer();
    if (isa<ConstantAggregateZero>(V))
      return "";
    else
      return cast<ConstantDataSequential>(V)->getRawDataValues();
  }

  void setOffset(StringRef Offset) {
    auto newV = ConstantDataArray::getString(
                       this->getContext(), 
                       Offset, 
                       false);
    this->setOperand(3, newV);
  }

  int64_t getNumReadOutstanding() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *V = cast<ConstantInt>(Bundle.getValue().Inputs[5]);
    return V->getSExtValue();
  }

  void setNumReadOutstanding(int64_t NumReadOutstanding) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, NumReadOutstanding);
    this->setOperand(5, newV);
  }

  int64_t getNumWriteOutstanding() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *V = cast<ConstantInt>(Bundle.getValue().Inputs[6]);
    return V->getSExtValue();
  }

  void setNumWriteOutstanding(int64_t NumWriteOutstanding) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, NumWriteOutstanding);
    this->setOperand(6, newV);
  }

  int64_t getMaxReadBurstLen() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *V = cast<ConstantInt>(Bundle.getValue().Inputs[7]);
    return V->getSExtValue();
  }

  void setMaxReadBurstLen(int64_t MaxReadBurstLen) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, MaxReadBurstLen);
    this->setOperand(7, newV);
  }

  int64_t getMaxWriteBurstLen() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *V = cast<ConstantInt>(Bundle.getValue().Inputs[8]);
    return V->getSExtValue();
  }

  void setMaxWriteBurstLen(int64_t MaxWriteBurstLen) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, MaxWriteBurstLen);
    this->setOperand(8, newV);
  }

  int64_t getLatency() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *V = cast<ConstantInt>(Bundle.getValue().Inputs[9]);
    return V->getSExtValue();
  }

  void setLatency(int64_t Latency) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, Latency);
    this->setOperand(9, newV);
  }

  int64_t getMaxWidenBitwidth() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *V = cast<ConstantInt>(Bundle.getValue().Inputs[10]);
    return V->getSExtValue();
  }

  void setMaxWidenBitwidth(int64_t Bitwidth) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, Bitwidth);
    this->setOperand(10, newV);
  }

  StringRef getChannelID() const {
    if (!isValidInst())
      assert(0 && "Illegal m_axi intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    Value *V = Bundle.getValue().Inputs[11];
    if (isa<GlobalVariable>(V))
      V = cast<GlobalVariable>(V)->getInitializer();
    if (isa<ConstantAggregateZero>(V))
      return "";
    else
      return cast<ConstantDataSequential>(V)->getRawDataValues();
  }

  void setChannelID(StringRef ID) {
    auto newV = ConstantDataArray::getString(getContext(), ID, false);
    setOperand(11, newV);
  }

  // get call intrinsic from root value
  static void get(Value *V, SetVector<MaxiInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const MaxiInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static MaxiInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<MaxiInst>(V, Indirect);
  }

  static const MaxiInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<MaxiInst>(V, Indirect);
  }


private:
  unsigned getNumArgs() const {
    return 12;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};


class AxiSInst : public InterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal axis intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Reg->isOne();
  }
#endif

  int64_t getRegisterMode() const {
    if (!isValidInst())
      assert(0 && "Illegal axis intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return Depth->getSExtValue();
  }

  int64_t getDepth() const {
    if (!isValidInst())
      assert(0 && "Illegal axis intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return Depth->getSExtValue();
  }

  StringRef getBundleName() const {
    if (!isValidInst())
      assert(0 && "Illegal axis intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    Value *V = Bundle.getValue().Inputs[5];
    if (isa<GlobalVariable>(V))
      V = cast<GlobalVariable>(V)->getInitializer();
    if (isa<ConstantAggregateZero>(V))
      return "";
    else
      return cast<ConstantDataSequential>(V)->getRawDataValues();
  }

  void setDepth(int64_t Depth) {
    auto I64T = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(I64T, Depth);
    this->setOperand(3, newV);
  }

  void setBundle(StringRef Bundle) {
    auto newV = ConstantDataArray::getString(
                       this->getContext(), 
                       Bundle, 
                       false);
    this->setOperand(5, newV);
  }

  // get call intrinsic from root value
  static void get(Value *V, SetVector<AxiSInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const AxiSInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static AxiSInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<AxiSInst>(V, Indirect);
  }

  static const AxiSInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<AxiSInst>(V, Indirect);
  }


private:
  unsigned getNumArgs() const {
    return 6;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};

class ApFifoInst : public InterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_fifo intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Reg->isOne();
  }
#endif

  int64_t getDepth() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_fifo intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return Depth->getSExtValue();
  }

  // get call intrinsic from root value
  static void get(Value *V, SetVector<ApFifoInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const ApFifoInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static ApFifoInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ApFifoInst>(V, Indirect);
  }

  static const ApFifoInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ApFifoInst>(V, Indirect);
  }


private:
  unsigned getNumArgs() const {
    return 4;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};

class ApMemoryInst : public InterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  int64_t getStorageType() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_memory intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Depth->getSExtValue();
  }

  void setStorageType(int64_t newType) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, newType);
    this->setOperand(1, newV);
  }

  int64_t getImplType() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_memory intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return Depth->getSExtValue();
  }

  void setImplType(int64_t newType) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, newType);
    this->setOperand(2, newV);
  }

  int64_t getLatency() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_memory intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return Depth->getSExtValue();
  }

  void setLatency(int64_t newLatency) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, newLatency);
    this->setOperand(3, newV);
  }

  int64_t getDepth() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_memory intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[5]);
    return Depth->getSExtValue();
  }

  StringRef getSAxiliteRamTypeName() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_memory intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    Value *V = Bundle.getValue().Inputs[6];
    if (isa<GlobalVariable>(V))
      V = cast<GlobalVariable>(V)->getInitializer();
    if (isa<ConstantAggregateZero>(V))
      return "";
    else
      return cast<ConstantDataSequential>(V)->getRawDataValues();
  }

  void setSAxiliteRamTypeName(StringRef SAxiliteRamType) {
    auto newV = ConstantDataArray::getString(
                       this->getContext(), 
                       SAxiliteRamType, 
                       false);
    this->setOperand(6, newV);
  }

  int64_t getAddressMode() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_memory intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[7]);
    return Depth->getSExtValue();
  }

  void setAddressMode(int64_t newAddressMode) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, newAddressMode);
    this->setOperand(7, newV);
  }

  bool isDirectIO() const {
    // Direct IO control info is encoded as the last operand in scalar
    // interface operand.
    auto CS = CallSite(const_cast<ApMemoryInst *>(this));
    auto DirectIO = dyn_cast<ConstantInt>(*(CS.data_operands_end()-1));
    return !DirectIO->isZero();
  }

  int32_t getDirectIO() const {
    auto CS = CallSite(const_cast<ApMemoryInst *>(this));
    auto DirectIO = cast<ConstantInt>(*(CS.data_operands_begin()+8));
    return DirectIO->getSExtValue();
  }

  // get call intrinsic from root value
  static void get(Value *V, SetVector<ApMemoryInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const ApMemoryInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static ApMemoryInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ApMemoryInst>(V, Indirect);
  }

  static const ApMemoryInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ApMemoryInst>(V, Indirect);
  }

  void setDirectIO() {
    Type *Int32Ty = Type::getInt32Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int32Ty, 1);
    this->setOperand(8, newV);
  }


private:
  unsigned getNumArgs() const {
    return 9;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};

class BRAMInst : public InterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  int64_t getStorageType() const {
    if (!isValidInst())
      assert(0 && "Illegal bram intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Depth->getSExtValue();
  }

  int64_t getImplType() const {
    if (!isValidInst())
      assert(0 && "Illegal bram intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[2]);
    return Depth->getSExtValue();
  }

  int64_t getLatency() const {
    if (!isValidInst())
      assert(0 && "Illegal bram intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return Depth->getSExtValue();
  }

  int64_t getDepth() const {
    if (!isValidInst())
      assert(0 && "Illegal bram intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[5]);
    return Depth->getSExtValue();
  }

  StringRef getSAxiliteRamTypeName() const {
    if (!isValidInst())
      assert(0 && "Illegal bram intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    Value *V = Bundle.getValue().Inputs[6];
    if (isa<GlobalVariable>(V))
      V = cast<GlobalVariable>(V)->getInitializer();
    if (isa<ConstantAggregateZero>(V))
      return "";
    else
      return cast<ConstantDataSequential>(V)->getRawDataValues();
  }

  int64_t getAddressMode() const {
    if (!isValidInst())
      assert(0 && "Illegal bram intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Depth = cast<ConstantInt>(Bundle.getValue().Inputs[7]);
    return Depth->getSExtValue();
  }

  void setAddressMode(int64_t newAddressMode) {
    Type *Int64Ty = Type::getInt64Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int64Ty, newAddressMode);
    this->setOperand(7, newV);
  }

  bool isDirectIO() const {
    // Direct IO control info is encoded as the last operand in scalar
    // interface operand.
    auto CS = CallSite(const_cast<BRAMInst *>(this));
    auto DirectIO = dyn_cast<ConstantInt>(*(CS.data_operands_end()-1));
    return !DirectIO->isZero();
  }

  int32_t getDirectIO() const {
    auto CS = CallSite(const_cast<BRAMInst *>(this));
    auto DirectIO = cast<ConstantInt>(*(CS.data_operands_begin()+8));
    return DirectIO->getSExtValue();
  }

  // get call intrinsic from root value
  static void get(Value *V, SetVector<BRAMInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const BRAMInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static BRAMInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<BRAMInst>(V, Indirect);
  }

  static const BRAMInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<BRAMInst>(V, Indirect);
  }

  void setDirectIO() {
    Type *Int32Ty = Type::getInt32Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int32Ty, 1);
    this->setOperand(8, newV);
  }


private:
  unsigned getNumArgs() const {
    return 9;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};

class ScalarInterfaceInst : public InterfaceInst {
public:
  static inline bool classof(const PragmaInst *I) {
    return isa<ApStableInst>(I) || isa<ApNoneInst>(I) || isa<ApAckInst>(I) ||
           isa<ApVldInst>(I) || isa<ApOvldInst>(I) || isa<ApHsInst>(I) ||
           isa<ApAutoInst>(I);
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  static ScalarInterfaceInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ScalarInterfaceInst>(V, Indirect);
  }

  static const ScalarInterfaceInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ScalarInterfaceInst>(V, Indirect);
  }

  bool isDirectIO() const {
    // Direct IO control info is encoded as the last operand in scalar
    // interface operand.
    auto CS = CallSite(const_cast<ScalarInterfaceInst *>(this));
    auto DirectIO = dyn_cast<ConstantInt>(*(CS.data_operands_end()-1));
    return !DirectIO->isZero();
  }

  int32_t getDirectIO() const {
    auto CS = CallSite(const_cast<ScalarInterfaceInst *>(this));
    if (isa<ApStableInst>(this)) {
      auto DirectIO = cast<ConstantInt>(*(CS.data_operands_begin()+3));
      return DirectIO->getSExtValue();
    }
    else if (isa<ApNoneInst>(this)) {
      auto DirectIO = cast<ConstantInt>(*(CS.data_operands_begin()+3));
      return DirectIO->getSExtValue();
    }
    else if (isa<ApAckInst>(this)) {
      auto DirectIO = cast<ConstantInt>(*(CS.data_operands_begin()+3));
      return DirectIO->getSExtValue();
    }
    else if (isa<ApVldInst>(this)) {
      auto DirectIO = cast<ConstantInt>(*(CS.data_operands_begin()+4));
      return DirectIO->getSExtValue();
    }
    else if (isa<ApOvldInst>(this)) {
      auto DirectIO = cast<ConstantInt>(*(CS.data_operands_begin()+3));
      return DirectIO->getSExtValue();
    }
    else if (isa<ApHsInst>(this)) {
      auto DirectIO = cast<ConstantInt>(*(CS.data_operands_begin()+4));
      return DirectIO->getSExtValue();
    }
    else if (isa<ApAutoInst>(this)) {
      auto DirectIO = cast<ConstantInt>(*(CS.data_operands_begin()+3));
      return DirectIO->getSExtValue();
    }
    else {
      llvm_unreachable("Other interface intrinsic?!");
    }
  }

  void setPragmaDirectIO() {
    Type *Int32Ty = Type::getInt32Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int32Ty, 1);
    if (isa<ApStableInst>(this)) {
      this->setOperand(3, newV);
    }
    else if (isa<ApNoneInst>(this)) {
      this->setOperand(3, newV);
    }
    else if (isa<ApAckInst>(this)) {
      this->setOperand(3, newV);
    }
    else if (isa<ApVldInst>(this)) {
      this->setOperand(4, newV);
    }
    else if (isa<ApOvldInst>(this)) {
      this->setOperand(3, newV);
    }
    else if (isa<ApHsInst>(this)) {
      this->setOperand(4, newV);
    }
    else if (isa<ApAutoInst>(this)) {
      this->setOperand(3, newV);
    }
    else {
      llvm_unreachable("Other interface intrinsic?!");
    }
  }

  void setClassDirectIO() {
    Type *Int32Ty = Type::getInt32Ty(this->getContext());
    auto newV = ConstantInt::getSigned(Int32Ty, 2);
    if (isa<ApStableInst>(this)) {
      this->setOperand(3, newV);
    }
    else if (isa<ApNoneInst>(this)) {
      this->setOperand(3, newV);
    }
    else if (isa<ApAckInst>(this)) {
      this->setOperand(3, newV);
    }
    else if (isa<ApVldInst>(this)) {
      this->setOperand(4, newV);
    }
    else if (isa<ApOvldInst>(this)) {
      this->setOperand(3, newV);
    }
    else if (isa<ApHsInst>(this)) {
      this->setOperand(4, newV);
    }
    else if (isa<ApAutoInst>(this)) {
      this->setOperand(3, newV);
    }
    else {
      llvm_unreachable("Other interface intrinsic?!");
    }
  }
};

class ApStableInst : public ScalarInterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_stable intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Reg->isOne();
  }
#endif

  // get call intrinsic from root value
  static void get(Value *V, SetVector<ApStableInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const ApStableInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static ApStableInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ApStableInst>(V, Indirect);
  }

  static const ApStableInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ApStableInst>(V, Indirect);
  }


private:
  unsigned getNumArgs() const {
    return 4;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    return Bundle && Bundle.getValue().Inputs.size() == getNumArgs();
  }
};

class ApNoneInst : public ScalarInterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_none intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Reg->isOne();
  }
#endif

  // get call intrinsic from root value
  static void get(Value *V, SetVector<ApNoneInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const ApNoneInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static ApNoneInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ApNoneInst>(V, Indirect);
  }

  static const ApNoneInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ApNoneInst>(V, Indirect);
  }


private:
  unsigned getNumArgs() const {
    return 4;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    return Bundle && Bundle.getValue().Inputs.size() == getNumArgs();
  }
};

class ApAckInst : public ScalarInterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_ack intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Reg->isOne();
  }
#endif

  // get call intrinsic from root value
  static void get(Value *V, SetVector<ApAckInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const ApAckInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static ApAckInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ApAckInst>(V, Indirect);
  }

  static const ApAckInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ApAckInst>(V, Indirect);
  }


private:
  unsigned getNumArgs() const {
    return 4;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    return Bundle && Bundle.getValue().Inputs.size() == getNumArgs();
  }
};

class ApVldInst : public ScalarInterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_vld intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Reg->isOne();
  }
#endif

  int64_t getInterrupt() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_vld intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *V = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return V->getSExtValue();
  }

  // get call intrinsic from root value
  static void get(Value *V, SetVector<ApVldInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const ApVldInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static ApVldInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ApVldInst>(V, Indirect);
  }

  static const ApVldInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ApVldInst>(V, Indirect);
  }


private:
  unsigned getNumArgs() const {
    return 5;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    return Bundle && Bundle.getValue().Inputs.size() == getNumArgs();
  }
};

class ApOvldInst : public ScalarInterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_ovld intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Reg->isOne();
  }
#endif

  // get call intrinsic from root value
  static void get(Value *V, SetVector<ApOvldInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const ApOvldInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static ApOvldInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ApOvldInst>(V, Indirect);
  }

  static const ApOvldInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ApOvldInst>(V, Indirect);
  }

private:
  unsigned getNumArgs() const {
    return 4;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    return Bundle && Bundle.getValue().Inputs.size() == getNumArgs();
  }
};

class ApHsInst : public ScalarInterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_hs intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Reg->isOne();
  }
#endif

  int64_t getInterrupt() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_hs intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *V = cast<ConstantInt>(Bundle.getValue().Inputs[3]);
    return V->getSExtValue();
  }

  // get call intrinsic from root value
  static void get(Value *V, SetVector<ApHsInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const ApHsInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static ApHsInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ApHsInst>(V, Indirect);
  }

  static const ApHsInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ApHsInst>(V, Indirect);
  }

private:
  unsigned getNumArgs() const {
    return 5;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    return Bundle && Bundle.getValue().Inputs.size() == getNumArgs();
  }
};

class ApAutoInst : public ScalarInterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

#if 0
  bool hasRegister() const {
    if (!isValidInst())
      assert(0 && "Illegal ap_auto intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Reg = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Reg->isOne();
  }
#endif

  // get call intrinsic from root value
  static void get(Value *V, SetVector<ApAutoInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const ApAutoInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static ApAutoInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<ApAutoInst>(V, Indirect);
  }

  static const ApAutoInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<ApAutoInst>(V, Indirect);
  }


private:
  unsigned getNumArgs() const {
    return 4;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    return Bundle && Bundle.getValue().Inputs.size() == getNumArgs();
  }
};

class ApCtrlNoneInst : public InterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

private:
  unsigned getNumArgs() const {
    return 2;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};

class ApCtrlChainInst : public InterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

private:
  unsigned getNumArgs() const {
    return 2;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};

class ApCtrlHsInst : public InterfaceInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

private:
  unsigned getNumArgs() const {
    return 2;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};


/// Reset Pragma Intrinsic
class ResetPragmaInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  bool isEnabled() const {
    if (!isValidInst())
      assert(0 && "Illegal reset intrinsic");
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    auto *Enabled = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return Enabled->isOne();
  }

private:
  unsigned getNumArgs() const {
    return 2;
  }

  bool isValidInst() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    if (Bundle && Bundle.getValue().Inputs.size() == getNumArgs())
      return true;
    else
      return false;
  }
};

class NPortChannelInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  void getOptions(SmallVectorImpl<Value *> &Opts) const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal dependence intrinsic");
    for (Value *V : Bundle.getValue().Inputs) 
      Opts.push_back(V);
  }
};

class XlxIPInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  void getOptions(SmallVectorImpl<Value *> &Opts) const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal xlx_ip intrinsic");
    for (Value *V : Bundle.getValue().Inputs) 
      Opts.push_back(V);
  }

  platform::PlatformBasic::OP_TYPE getOp() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal xlx_ip intrinsic");
    ConstantInt *Op = cast<ConstantInt>(Bundle.getValue().Inputs[0]);
    return (platform::PlatformBasic::OP_TYPE)Op->getSExtValue();
  }

  platform::PlatformBasic::IMPL_TYPE getImpl() const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal xlx_ip intrinsic");
    ConstantInt *Impl = cast<ConstantInt>(Bundle.getValue().Inputs[1]);
    return (platform::PlatformBasic::IMPL_TYPE)Impl->getSExtValue();
  }
};

class MaxiCacheInst : public PragmaInst {
public:
  static const std::string BundleTagName;
  static inline bool classof(const PragmaInst *I) {
    return I->getOperandBundle(BundleTagName).hasValue();
  }

  static inline bool classof(const Value *V) {
    return isa<PragmaInst>(V) && classof(cast<PragmaInst>(V));
  }

  void getOptions(SmallVectorImpl<Value *> &Opts) const {
    Optional<OperandBundleUse> Bundle = getOperandBundle(BundleTagName);
    assert(Bundle && "Illegal xlx_maxi_cache intrinsic");
    auto It = Bundle.getValue().Inputs.begin();
    ++It;
    for (auto Ie = Bundle.getValue().Inputs.end(); It != Ie; ++It) {
      Opts.push_back(*It);
    }
  }

  Value *getPort() const { return getVariable(); }

  uint64_t getLines() const {
    return cast<ConstantInt>(getOperand(1))->getZExtValue();
  }

  uint64_t getDepth() const {
    return cast<ConstantInt>(getOperand(2))->getZExtValue();
  }

  static void get(Value *V, SetVector<MaxiCacheInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static void get(const Value *V,
                  SetVector<const MaxiCacheInst *> &PSet, bool Indirect = true) {
    return PragmaInst::get(V, PSet, Indirect);
  }

  static MaxiCacheInst *get(Value *V, bool Indirect = true) {
    return PragmaInst::get<MaxiCacheInst>(V, Indirect);
  }

  static const MaxiCacheInst *get(const Value *V, bool Indirect = true) {
    return PragmaInst::get<MaxiCacheInst>(V, Indirect);
  }
};

/// This represents the fpga_fence
class FPGAFenceInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fence;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  void getBeforeObjects(SmallVectorImpl<Value *> &BeforeObjs) {
    SmallVector<Value *, 4> AfterObjs;
    getBeforeAfterObjects(BeforeObjs, AfterObjs);
  }

  void getAfterObjects(SmallVectorImpl<Value *> &AfterObjs) {
    SmallVector<Value *, 4> BeforeObjs;
    getBeforeAfterObjects(BeforeObjs, AfterObjs);
  }

  unsigned getNumBeforeObjects() {
    SmallVector<Value *, 4> BeforeObjs;
    getBeforeObjects(BeforeObjs);
    return BeforeObjs.size();
  }

  unsigned getNumAfterObjects() {
    SmallVector<Value *, 4> AfterObjs;
    getAfterObjects(AfterObjs);
    return AfterObjs.size();
  }

  void getBeforeAfterObjects(SmallVectorImpl<Value *> &BeforeObjs,
                             SmallVectorImpl<Value *> &AfterObjs) {
    bool IsBefore = true;
    for (Value *Arg : arg_operands()) {
      if (Arg->getType()->isPointerTy()) {
        if (IsBefore)
          BeforeObjs.push_back(Arg);
        else
          AfterObjs.push_back(Arg);
      } else if (ConstantInt *C = dyn_cast<ConstantInt>(Arg)) {
        if (-1 == (int)C->getSExtValue())
          IsBefore = false;     
      }
    }
  }
};

class FPGAFenceGroupInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fence_group;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }

  unsigned getNumObjects() const {
    return getNumArgOperands();
  }

  iterator_range<op_iterator> objects() {
    return arg_operands();
  }
 
  iterator_range<const_op_iterator> objects() const {
    return arg_operands();
  }
};

class FPGAFenceWithGroupInst : public IntrinsicInst {
public:
  static inline bool classof(const IntrinsicInst *I) {
    return I->getIntrinsicID() == Intrinsic::fpga_fence_with_group;
  }

  static inline bool classof(const Value *V) {
    return isa<IntrinsicInst>(V) && classof(cast<IntrinsicInst>(V));
  }
};

} // namespace llvm

#endif // REFLOW_SPIR_INTRINSICINST_H
