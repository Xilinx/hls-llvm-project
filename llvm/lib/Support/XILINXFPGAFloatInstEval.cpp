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

#include "llvm/Config/config.h"

#if HLS_FPGA_FLOAT_CONSTEVAL

#include "llvm/Support/XILINXFPGAFloatInstEval.h"

#include "llvm/ADT/APInt.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <floating_point_v7_1_bitacc_cmodel.h>

using namespace llvm;

namespace {
// Handle lifetime of mpz_t
class MPZ {
    mpz_t res;
    unsigned width; // mpz_t don't store the width

public:
    MPZ(unsigned width) : width(width) {
        mpz_init2(res, width);
    }

    ~MPZ() {
        mpz_clear(res);
    }

    operator mpz_t &() { return res; }
    operator const mpz_t &() const { return res; }

    void set(const APInt &val) {
        assert(val.getBitWidth() == width && "Invalid bitwidth");

        // Get the number of words and the pointer to the raw data
        unsigned num_words = val.getNumWords();
        const APInt::WordType *data = val.getRawData();

        // Import the data from APInt to mpz_t
        mpz_import(res, num_words,
                   -1/*less significant word first*/,
                   sizeof(APInt::WordType),
                   0/*native endianness*/,
                   0/*no unused bits*/,
                   data);
    }

    APInt get() const {
        // Get the number of words and the pointer to the raw data
        unsigned num_words = (width - 1) / APInt::APINT_BITS_PER_WORD + 1;
        std::vector<APInt::WordType> data(num_words);

        // Export the data from mpz_t to APInt
        mpz_export(data.data(), 0/*no unused bits*/, -1/*less significant word first*/,
                   sizeof(APInt::WordType), 0/*native endianness*/,
                   0/*no unused bits*/, res);

        // Initialize the APInt
        return APInt(width, num_words, data.data());
    }
};

// Handle lifetime of mpfr_t
class MPFR {
    mpfr_t res;
    unsigned width; // mpfr_t don't store the width
    unsigned exp_width; // mpfr_t don't store the exponent width

public:
    MPFR(unsigned width, unsigned exp_width) : width(width), exp_width(exp_width) {
        unsigned mant_width = width - exp_width - 1;
        // IEEE bounds for the exponent
        long exp_min = APInt::getSignedMinValue(exp_width).getSExtValue() + 2;
        long exp_max = APInt::getSignedMaxValue(exp_width).getSExtValue();
        // mpfr exponent is +1 compared to IEEE
        mpfr_set_emin(exp_min + 1 - mant_width); // extra range for subnormals
        mpfr_set_emax(exp_max + 1);
        mpfr_init2(res, mant_width + 1); // must include implicit bit
    }

    ~MPFR() {
        mpfr_clear(res);
    }

    operator mpfr_t &() { return res; }
    operator const mpfr_t &() const { return res; }

#ifndef NDEBUG
    void dump() const {
        mpfr_out_str(stderr, 10, 0, res, MPFR_RNDN);
        dbgs() << "(w: " << width << ", e: " << exp_width << ")\n";
    }
#endif

    // Set the value of the MPFR from APInt containing the IEEE representation
    void set(const APInt &val) {
        assert(val.getBitWidth() == width && "Invalid bitwidth");

        // Bitwidth of the significand (mantissa)
        unsigned mant_width = width - exp_width - 1;

        // Extract sign, exponent and mantissa
        bool sign_bit = val.isNegative();
        APInt exp_bits = val.lshr(mant_width).trunc(exp_width);
        APInt mant_bits = val.trunc(mant_width);

        // Handle edge cases
        if (exp_bits.isNullValue()) {
            if (mant_bits.isNullValue()) {
                // Zero
                mpfr_set_zero(res, sign_bit ? -1 : 1);
                return;
            }
        } else if (exp_bits.isAllOnesValue()) {
            // Infinity or NaN
            if (mant_bits.isNullValue()) {
                // Infinity
                mpfr_set_inf(res, sign_bit ? -1 : 1);
                return;
            } else {
                // NaN
                mpfr_set_nan(res);
                return;
            }
        }

        // Calculate unbiased exponent
        APInt exp = exp_bits - APInt::getSignedMaxValue(exp_width);
        int64_t exp_val = exp.getSExtValue();
        if (exp_bits.isNullValue()) // Subnormal
            exp_val += 1;

        // Add the implicit bit
        mant_bits = mant_bits.zext(mant_width + 1);
        if (!exp_bits.isNullValue()) // Normal
            mant_bits.setBit(mant_width);

        // Convert mantissa to MPZ
        MPZ mant_val(mant_width + 1);
        mant_val.set(mant_bits);

        // Initialize the mpfr_t
        mpfr_set_z_2exp(res, mant_val, exp_val - mant_width, MPFR_RNDN);

        // Set the sign
        mpfr_setsign(res, res, sign_bit, MPFR_RNDN);
    }

    APInt get() const {
        // Bitwidth of the significand (mantissa)
        unsigned mant_width = width - exp_width - 1;

        // Handle edge cases
        if (mpfr_zero_p(res)) { // Zero
            APInt ret(width, 0);
            if (mpfr_signbit(res))
                ret.setSignBit();
            return ret;
        } else if (mpfr_inf_p(res)) { // Infinity
            APInt ret(width, 0);
            ret.insertBits(APInt(exp_width, -1, true), width - exp_width - 1);
            if (mpfr_signbit(res))
                ret.setSignBit();
            return ret;
        } else if (mpfr_nan_p(res)) { // NaN
            return APInt(width, -1, true);
        }

        // Extract sign, mantissa and exponent
        bool sign = mpfr_signbit(res);
        MPZ mant_val(width - exp_width);
        long exp_val = mpfr_get_z_2exp(mant_val, res) + mant_width;

        // TODO: Handle subnormals

        // Calculate biased exponent
        APInt exp_bits = APInt::getSignedMaxValue(exp_width) + exp_val;

        // Extract mantissa
        APInt mant_bits = mant_val.get();
        mant_bits = mant_bits.trunc(width - exp_width - 1);

        // Initialize the APInt
        APInt ret(width, 0);
        ret.insertBits(mant_bits, 0);
        ret.insertBits(exp_bits, width - exp_width - 1);
        if (sign)
            ret.setSignBit();

        return ret;
    }
};

// Handle lifetime of xip_fpo_t
class XIP_FPO {
    xip_fpo_t res;
    unsigned width;
    unsigned exp_width;
    
public:
    XIP_FPO(int width, int exp_width) : width(width), exp_width(exp_width) {
        xip_fpo_init2(res, exp_width, width - exp_width);
    }

    ~XIP_FPO() {
        xip_fpo_clear(res);
    }

    operator xip_fpo_t &() { return res; }
    operator const xip_fpo_t &() const { return res; }

    void set(const APInt &val) {
        assert(val.getBitWidth() == width && "Invalid bitwidth");
        MPFR mpfr(width, exp_width);
        mpfr.set(val);
        xip_fpo_set_fr(res, mpfr);
    }

    APInt get() const {
        MPFR mpfr(width, exp_width);
        xip_fpo_get_fr(mpfr, res);
        return mpfr.get();
    }
};

// Handle lifetime of xip_fpo_fix_t
class XIP_FPO_FIX {
    xip_fpo_fix_t res;
    unsigned width;
    int exp;

public:
    XIP_FPO_FIX(unsigned width, int exp) : width(width), exp(exp) {
        xip_fpo_fix_init2(res, exp, (int)width - exp);
    }

    ~XIP_FPO_FIX() {
        xip_fpo_fix_clear(res);
    }

    operator xip_fpo_fix_t &() { return res; }
    operator const xip_fpo_fix_t &() const { return res; }

    void set(const APInt &val) {
        assert(val.getBitWidth() == width && "Invalid bitwidth");
        MPZ mpz(width);
        mpz.set(val);
        MPFR mpfr(width+32, 32); // xip_fpo_fix is limited to 32-bit exponent
        mpfr_set_z(mpfr, mpz, MPFR_RNDN);
        mpfr_mul_2si(mpfr, mpfr, exp - (int)width, MPFR_RNDN);
        xip_fpo_fix_set_fr(res, mpfr);
    }

    APInt get() const {
        MPFR mpfr(width+32, 32); // xip_fpo_fix is limited to 32-bit exponent
        xip_fpo_fix_get_fr(mpfr, res);
        mpfr_div_2si(mpfr, mpfr, exp - (int)width, MPFR_RNDN);
        MPZ mpz(width);
        mpfr_get_z(mpz, mpfr, MPFR_RNDN);
        return mpz.get();
    }
};
} // namespace

namespace fpga {
APInt EvalFloatAdd(const APInt &Lhs, const APInt &Rhs, int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    XIP_FPO ret(Width, ExpWidth);
    lhs.set(Lhs);
    rhs.set(Rhs);
    xip_fpo_add(ret, lhs, rhs);
    return ret.get();
}

APInt EvalFloatSub(const APInt &Lhs, const APInt &Rhs, int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    XIP_FPO ret(Width, ExpWidth);
    lhs.set(Lhs);
    rhs.set(Rhs);
    xip_fpo_sub(ret, lhs, rhs);
    return ret.get();
}

APInt EvalFloatMul(const APInt &Lhs, const APInt &Rhs, int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    XIP_FPO ret(Width, ExpWidth);
    lhs.set(Lhs);
    rhs.set(Rhs);
    xip_fpo_mul(ret, lhs, rhs);
    return ret.get();
}

APInt EvalFloatDiv(const APInt &Lhs, const APInt &Rhs, int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    XIP_FPO ret(Width, ExpWidth);
    lhs.set(Lhs);
    rhs.set(Rhs);
    xip_fpo_div(ret, lhs, rhs);
    return ret.get();
}

APInt EvalFloatFMA(const APInt &Lhs, const APInt &Rhs, const APInt &Add,
                   int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    assert(Lhs.getBitWidth() == Add.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    XIP_FPO add(Width, ExpWidth);
    XIP_FPO ret(Width, ExpWidth);
    lhs.set(Lhs);
    rhs.set(Rhs);
    add.set(Add);
    xip_fpo_fma(ret, lhs, rhs, add);
    return ret.get();
}

APInt EvalFloatSqrt(const APInt &Val, int ExpWidth) {
    int Width = Val.getBitWidth();
    XIP_FPO val(Width, ExpWidth);
    XIP_FPO ret(Width, ExpWidth);
    val.set(Val);
    xip_fpo_sqrt(ret, val);
    return ret.get();
}

APInt EvalFloatFromFixed(const APInt &Val, int FixedExp, int ExpWidth, int DestWidth) {
    XIP_FPO_FIX val(Val.getBitWidth(), FixedExp);
    val.set(Val);
    XIP_FPO ret(DestWidth, ExpWidth);
    xip_fpo_fixtoflt(ret, val);
    return ret.get();
}

APInt EvalFloatToFixed(const APInt &Val, int ExpWidth, int FixedExp, int DestWidth) {
    XIP_FPO val(Val.getBitWidth(), ExpWidth);
    val.set(Val);
    XIP_FPO_FIX ret(DestWidth, FixedExp);
    xip_fpo_flttofix(ret, val);
    return ret.get();
}

APInt EvalFloatToFloat(const APInt &Val, int SrcExpWidth, int DestExpWidth, int DestWidth) {
    int SrcWidth = Val.getBitWidth();
    XIP_FPO val(SrcWidth, SrcExpWidth);
    val.set(Val);
    XIP_FPO ret(DestWidth, DestExpWidth);
    xip_fpo_flttoflt(ret, val);
    return ret.get();
}

bool EvalFloatCompareEQ(const APInt &Lhs, const APInt &Rhs, int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    int ret;
    lhs.set(Lhs);
    rhs.set(Rhs);
    xip_fpo_equal(&ret, lhs, rhs);
    return ret;
}

bool EvalFloatCompareLT(const APInt &Lhs, const APInt &Rhs, int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    int ret;
    lhs.set(Lhs);
    rhs.set(Rhs);
    xip_fpo_less(&ret, lhs, rhs);
    return ret;
}

bool EvalFloatCompareLE(const APInt &Lhs, const APInt &Rhs, int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    int ret;
    lhs.set(Lhs);
    rhs.set(Rhs);
    xip_fpo_lessequal(&ret, lhs, rhs);
    return ret;
}

bool EvalFloatCompareNE(const APInt &Lhs, const APInt &Rhs, int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    int ret;
    lhs.set(Lhs);
    rhs.set(Rhs);
    xip_fpo_notequal(&ret, lhs, rhs);
    return ret;
}

bool EvalFloatCompareUO(const APInt &Lhs, const APInt &Rhs, int ExpWidth) {
    assert(Lhs.getBitWidth() == Rhs.getBitWidth() && "Invalid bitwidth");
    int Width = Lhs.getBitWidth();
    XIP_FPO lhs(Width, ExpWidth);
    XIP_FPO rhs(Width, ExpWidth);
    int ret;
    lhs.set(Lhs);
    rhs.set(Rhs);
    xip_fpo_unordered(&ret, lhs, rhs);
    return ret;
}
} // namespace fpga

#endif // HLS_FPGA_FLOAT_CONSTEVAL
