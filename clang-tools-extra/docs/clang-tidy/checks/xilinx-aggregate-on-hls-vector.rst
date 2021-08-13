.. title:: clang-tidy - xilinx-aggregate-on-hls-vector

xilinx-aggregate-on-hls-vector
==============================

This check automatically adds the `aggregate` pragma and align assumption to top
function `hls::vector` parameters.

`alignof` attribute is added for getting the alignmnet assumption specified by
the `hls::vector` library with `alignas` attribute. Then, apply the  alignmnet
assumption to the top function `hls::vector` parameters with `align_value`
attribute.
