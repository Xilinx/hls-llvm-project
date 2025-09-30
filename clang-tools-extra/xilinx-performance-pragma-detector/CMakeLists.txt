# (C) Copyright 2016-2022 Xilinx, Inc.
# (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
set(LLVM_LINK_COMPONENTS 
  support
  )

add_clang_tool(xilinx-performance-pragma-detector
  XilinxPerformancePragmaDetector.cpp
  )

target_link_libraries(xilinx-performance-pragma-detector
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
  )
