#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XilinxPlat/ChipInfo.h"
#include "llvm/Support/XilinxPlat/TargetPlatform.h"
#else
#include "ChipInfo.h"
#include "TargetPlatform.h"
#endif 

using namespace platform;
using namespace pf_internal;

namespace platform
{

TargetPlatform::TargetPlatform(std::string name) : mPlatformName(name)
{
    mChipInfo = new ChipInfo; 
    
    mFac = new CoreInstFactory;
    mFac->setTargetPlatform(this);

#ifdef PLATFORM_DEBUG
    SS << "TargetPlatform \"" << mPlatformName << "\" is created";
    DEBUG_LOG;
#endif
}


TargetPlatform::~TargetPlatform()
{
    if (mChipInfo) 
    {
        delete mChipInfo;
        mChipInfo = 0;
    }
    if (mFac) 
    {
        delete mFac;
        mFac = 0;
    }
}


double TargetPlatform::getNormalizedCost(const ResUsageMap& res_usage) const
{
    double cost = 0;
    
    const ChipInfo* chip_info = getChipInfo();
    for (ResUsageMap::const_iterator ri = res_usage.begin();
         ri != res_usage.end(); ++ri)
    {
        std::string name = ri->first;
        double usage = ri->second;
        
        double total = chip_info->getResourceBudgetByName(name);
        // No budget? This cost will dominate.
        if (total <= 0) total = 1.0; 
        
        cost += usage / total;
    }

    return cost;
}

TargetPlatform* gTargetPlat = nullptr;
void SetTargetPlatform(TargetPlatform* targetPlat) {
  gTargetPlat = targetPlat;
}

TargetPlatform* GetTargetPlatform() {
    // assert(gTargetPlat != nullptr);
    return gTargetPlat;
}

} // namespace platform


