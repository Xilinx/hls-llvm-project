
//-*-c++-*-
// TargetPlatform.h -- Target platform modeling for hardware synthesis.
/*
 * Author: Zhiru Zhang
 * Copyright (C) 2006-2009 AutoESL Design Technologies, Inc.
 * All rights reserved.
 */
// $Id: TargetPlatform.h,v 1.13 2006/03/30 20:07:44 zhiruz Exp $ 

#ifndef _PLATFORM_TargetPlatform_H
#define _PLATFORM_TargetPlatform_H

#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XilinxPlat/CoreInst.h"
#else
#include "CoreInst.h"
#endif

namespace platform 
{

class ChipInfo;
/**
 * Target plaform class definition
 */
class TargetPlatform
{
  public:
    /// Constructor.
    TargetPlatform(std::string name = "");
    /// Destructor.
    ~TargetPlatform();

  public:
    /// Get the name of the platform.
    std::string getName() const { return mPlatformName; }

    /// Get the chip info. 
    ChipInfo* getChipInfo() { return mChipInfo; }
    const ChipInfo* getChipInfo() const { return mChipInfo; }

    // get CoreInstFactory 
    CoreInstFactory* getCoreInstFactory() { return mFac; }
    CoreInstFactory& getFactory() { return *mFac; }

    /// Get the normalized area cost.
    double getNormalizedCost(const ResUsageMap& res_usage) const;
        
  protected:
    /// Name of the target platform
    std::string mPlatformName;
    /// Chip info
    ChipInfo* mChipInfo;     
   
    /// CoreInst Factory
    CoreInstFactory* mFac;
};

void SetTargetPlatform(TargetPlatform* targetPlat);
TargetPlatform* GetTargetPlatform();

} // end of namespace platform 
#endif


