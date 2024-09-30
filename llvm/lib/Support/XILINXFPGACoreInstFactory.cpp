#include "llvm/Support/XILINXFPGACoreInstFactory.h"
#include <sstream>

namespace platform {
    int coreInstFactoryInit(const std::string &dbPath, 
                        const std::string &libraryName, 
                        const std::string &resInfo) {
    //Step 0: setup TargetPlatform class and init Sqlite
    TargetPlatform *plat = new TargetPlatform("DefaultPlatform");
    SetTargetPlatform(plat);
    bool sqlInitSuccess = Selector::getSelector().init(dbPath);
    assert(sqlInitSuccess);

    //Step 1: setup ChipInfo class 
    ChipInfo* chipInfo = plat->getChipInfo();
    assert(chipInfo);

    ResUsageMap resMap;
    std::istringstream resStream(resInfo);
    std::string resKey;
    std::string resValue;
    while(std::getline(resStream, resKey, '_') && 
        std::getline(resStream, resValue, '_')) {
        resMap.emplace(resKey, std::stod(resValue));
    }

    chipInfo->setResourceBudget(resMap);
    std::string speedName("medium");
    if(libraryName.find("slow") != std::string::npos) {
        speedName = "slow";
    } else if (libraryName.find("medium") != std::string::npos) {
        speedName = "medium";
    } else if(libraryName.find("fast") != std::string::npos) {
        speedName = "fast";
    }
    chipInfo->setSpeedGrade(speedName);
    // dspStyle is only used in CoreGen, no need in FE
    // chipInfo->setDspStyle(dspStyle);

    //Step 2: setup CoreInstFactory class
    CoreInstFactory* fac = plat->getCoreInstFactory();
    assert(fac);
    fac->setName(libraryName);

    int result = fac->createCores();
    
    //Step 3: setup QuerierFactory
    QuerierFactory::getInstance().init();

    return 0;
}

}  // end of platform