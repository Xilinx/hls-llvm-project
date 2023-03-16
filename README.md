# Low Level Virtual Machine (LLVM) for Xilinx Vitis HLS

This directory and its subdirectories contain source code for LLVM,
a toolkit for the construction of highly optimized compilers,
optimizers, and runtime environments.

## How to build hls-llvm-project
- Use a Xilinx compatible linux [Build Machine OS](https://docs.xilinx.com/r/en-US/ug1393-vitis-application-acceleration/Installation)
  - This requirement is due to [ext](ext) library usage
- Clone the HLS repo sources (including hls-llvm-project submodule)
- Install [CMake 3.4.3 or higher](https://cmake.org/download/)
- Install [ninja](https://ninja-build.org/) [optional for faster builds]
- run [build-hls-llvm-project.sh](build-hls-llvm-project.sh) in the cloned directory:
  > ./build-hls-llvm-project.sh
