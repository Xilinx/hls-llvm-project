// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
//-*-c++-*-
// ChipInfo.h -- Chip information.
/*
 * Author: Zhiru Zhang
 * Copyright (C) 2009 AutoESL Design Technologies, Inc.
 * All rights reserved.
 */
// $Id: ChipInfo.h,v 1.13 2009/04/14 20:07:44 zhiruz Exp $

#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XilinxPlat/ChipInfo.h"
#else
#include "ChipInfo.h"
#endif

using namespace platform;

namespace platform
{

ChipInfo::ChipInfo() : mSpeedGrade("")
{
    mResBudget.clear();
}


ChipInfo::~ChipInfo()
{
    mResBudget.clear();
}


double ChipInfo::getResourceBudgetByName(std::string res_name) const
{
    if (mResBudget.count(res_name) <= 0)
        return 0;
    
    return mResBudget.find(res_name)->second;
}

} // namespace platform


