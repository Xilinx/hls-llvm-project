
#ifndef LLVM_SUPPORT_XILINX_SYSTEM_INFO_H
#define LLVM_SUPPORT_XILINX_SYSTEM_INFO_H
#include"llvm/Support/Path.h"
#include<string>
#include<set>

namespace XilinxSystemInfo{ 
bool isSystemHLSHeaderFile(const std::string FileName); 
}
#endif 
