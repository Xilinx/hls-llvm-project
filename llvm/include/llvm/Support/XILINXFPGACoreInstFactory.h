// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
#ifndef LLVM_SUPPORT_XILINXFPGACOREINSTFACTORY_H
#define LLVM_SUPPORT_XILINXFPGACOREINSTFACTORY_H

#define XILINX_HLS_FE_STANDALONE 1
#include <string> 
#include "llvm/Support/XilinxPlat/CoreQuerier.h"
#include "llvm/Support/XilinxPlat/ChipInfo.h"
#include "llvm/Support/XilinxPlat/CoreInst.h"
#include "llvm/Support/XilinxPlat/CoreRanker.h"
#include "llvm/Support/XilinxPlat/TargetPlatform.h"



namespace platform {




int coreInstFactoryInit(const std::string &dbPath, 
                        const std::string &libraryName, 
                        const std::string &resInfo, 
                        const std::string &deviceName="");





}   // namespace platform
#endif //LLVM_SUPPORT_XILINXFPGACOREINSTFACTORY_H 