// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
#include "llvm/Support/XILINXFPGACoreInstFactory.h"
#include "llvm/Support/XILINXFPGAPlatformBasic.h"
#include <sqlite3.h>
#include <sstream>

namespace platform {
    int coreInstFactoryInit(const std::string &dbPath, 
                        const std::string &libraryName, 
                        const std::string &resInfo,
                        const std::string &deviceName) {
    //Step 0: setup TargetPlatform class and init Sqlite
    TargetPlatform *plat = new TargetPlatform("DefaultPlatform");
    SetTargetPlatform(plat);
    int rc = Selector::getSelector().init(dbPath);
    assert(rc == SQLITE_OK);

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

    //Step 2: setup PlatformBasic
    SetPlatformDeviceResourceInfo(resInfo);
    SetPlatformDeviceNameInfo(deviceName); 
    auto pb = PlatformBasic::getInstance();
    bool success = false;
    if (libraryName.find("versal_") != std::string::npos) {
        success = pb->load("versal_medium");
    } else {
        success = pb->load(libraryName);
    }
    assert(success);


    //Step 3: setup CoreInstFactory class
    CoreInstFactory* fac = plat->getCoreInstFactory();
    assert(fac);
    if (libraryName.find("versal_slow") != std::string::npos) {
        fac->setName("versal_medium");
        fac->setDelayFactor(1.4);
    } else if (libraryName.find("versal_medium") != std::string::npos) {
        fac->setName("versal_medium");
        fac->setDelayFactor(1.0);
    } else if (libraryName.find("versal_fast") != std::string::npos) { 
        fac->setName("versal_medium");
        fac->setDelayFactor(0.9);
    } else {
        fac->setName(libraryName);
    }

    int result = fac->createCores();
    
    //Step 4: setup QuerierFactory
    QuerierFactory::getInstance().init();

    return 0;
}

}  // end of platform