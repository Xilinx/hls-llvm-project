// RUN: xilinx-performance-pragma-detector %s -- | FileCheck %s 
// RUN: xilinx-performance-pragma-detector %s -- -fhls | FileCheck %s --check-prefix=CHECK-HLS 

// CHECK: the number of performance pragma: 0
// CHECK-HLS: the number of performance pragma: 1

void dut(int a[4][4], int b[4][4], int c[4][4]) {
  #pragma HLS PERFORMANCE target_ti = 16
L1:
  for (int i = 0; i < 4; ++i) {
  L2:
    for (int j = 0; j < 4; ++j) {
      c[i][j] = a[i][j] + b[i][j];
    }
  }
}
