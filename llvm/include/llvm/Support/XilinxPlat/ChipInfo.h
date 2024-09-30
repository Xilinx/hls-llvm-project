
//-*-c++-*-
// ChipInfo.h -- Chip information.
/*
 * Author: Zhiru Zhang
 * Copyright (C) 2009 AutoESL Design Technologies, Inc.
 * All rights reserved.
 */
// $Id: ChipInfo.h,v 1.13 2009/04/14 20:07:44 zhiruz Exp $

#ifndef _PLATFORM_ChipInfo_H
#define _PLATFORM_ChipInfo_H

#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XilinxPlat/CoreInst.h"
#else
#include "CoreInst.h"
#endif

namespace platform 
{
/**
 * Chip information 
 */
class ChipInfo
{
  public:
    ChipInfo();
    virtual ~ChipInfo();
    
  public:  
    /// Set the heterogeneous resource budget map.
    void setResourceBudget(const ResUsageMap& res_budget) {
        mResBudget = res_budget;
    }

    /// Get the resource budget for a specific type.
    double getResourceBudgetByName(std::string name) const;
    /// Get the heterogeneous resource budget map.
    ResUsageMap& getResourceBudget() { return mResBudget; }
    const ResUsageMap& getResourceBudget() const { return mResBudget; }

    /// Get the speed grade.
    std::string getSpeedGrade() const { return mSpeedGrade; }
    /// Set the speed grade.
    void setSpeedGrade(std::string grade) { mSpeedGrade = grade; }

    void setDspStyle(const std::string& dspStyle) { mDspStyle = dspStyle; }
    std::string getDspStyle() { return mDspStyle; }
    
    /// Get the maximum achievable operating frequency.
    // float getMaxOperatingFreq() const;

  private:
    /// Speed grade
    std::string mSpeedGrade;
    /// Resource distribution
    ResUsageMap mResBudget;
    // dsp Style
    std::string mDspStyle;
};

}

#endif


