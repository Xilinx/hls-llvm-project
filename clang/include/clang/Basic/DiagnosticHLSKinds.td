//==--- DiagnosticHLSKinds.td -----HLS related diagnostics ----------------===//
//
// (C) Copyright 2016-2022 Xilinx, Inc.
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc.
// All Rights Reserved.
//
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//===----------------------------------------------------------------------===//
//
// Xilinx HLS sema and parser related Diagnostic message
//
//===----------------------------------------------------------------------===//

let Component = "HLS" in {
let CategoryName = "HLS Issue" in {


// OpenCL Xilinx extension
def err_xcl_dataflow_on_non_for_loop : Error<
  "Only for-loops and functions support the dataflow">;
def err_xcl_dataflow_on_trivial_loop_body : Error<
  "The for-loop body with pragma/attribute must be enclosed within braces">;
def warn_remove_xcl_dataflow_attr : Warning<
  "Ignore dataflow attribute or pragma">,
  InGroup<IgnoredDataflowAttr>;
def warn_remove_interface_specific_attr : Warning<
  "Ignore interface attribute or pragma which is not used in top function">,
  InGroup<IgnoredInterfaceAttr>;

def warn_invalid_variable_expr: Warning<
  "invalid variable expr ">,
  InGroup<InvalidXlxPragmas>;

def warn_xlx_pragma_ignored : Warning<"unknown HLS pragma ignored">,
   InGroup<InvalidXlxPragmas>;

def dump_xlx_pragma : Warning<"HLS pragma dump %0">,
   InGroup<DumpXlxPragmas>, DefaultIgnore;
   
def warn_unexpected_pragma_parameter : Warning<
  "unexpected pragma parameter '%0'">,
  InGroup<InvalidXlxPragmas>;
def warn_unexpected_token_in_pragma_argument : Warning<
  "unexpected pragma argument '%0', expects %1">,
  InGroup<InvalidXlxPragmas>;
def warn_repeated_pragma_parameter : Warning<
  "pragma parameter '%0' repeated">,
  InGroup<InvalidXlxPragmas>;
def warn_pragma_named_argument_missing : Warning<
  "missing argument for '%0'">,
  InGroup<InvalidXlxPragmas>;

def warn_extra_tokens_before_variable_expression : Warning<
  "extra token before variable expression is ignored">,
  InGroup<ExtraTokens>;

def warn_extra_tokens_before_pragma_subject : Warning<
  "extra token before pragma subject is ignored">,
  InGroup<ExtraTokens>;
def warn_deprecated_pragma_ignored_by_scout: Warning<
  "'%0' is deprecated, and it will be removed in future release">,
  InGroup<InvalidXlxPragmas>;
def warn_deprecated_pragma_ignored_by_scout_skip_strict_mode: Warning<
  "'%0' is deprecated, use '%1' instead">,
  InGroup<IgnoredXlxPragmas>;
def warn_deprecated_pragma_option: Warning<
  "'%0' in '#pragma HLS %1' is deprecated">,
  InGroup<IgnoredXlxPragmas>;
def warn_deprecated_pragma_option_ignored_by_scout: Warning<
  "'%0' in '#pragma HLS %1' is deprecated, use '%2' instead">,
  InGroup<IgnoredXlxPragmas>;
def warn_extra_pragma_hls_token_ignored : Warning<
  "'%0' in '#pragma HLS %1' is ignored">,
  InGroup<ExtraTokens>;
def warn_conflict_pragma_parameter : Warning<
  "pragma parameter '%0' conflicts with '%1'">,
  InGroup<InvalidXlxPragmas>;

def err_conflict_pragma_parameter : Error<
  "pragma parameter '%0' conflicts with '%1'">;

def err_invalid_option_unless : Error<
  "pragma parameter '%0' is not allowed unless %1">;

def err_invalid_option_value: Error<
  "invalid value '%0' for option '%1'">;

def err_cache_on_volatile_port : Error<
  "cache is not allowed on port with 'volatile' modifier">;

def err_performance_at_least_one_parameter_required : Error<
  "performance pragma requires at least one of 'target_ti'|'target_tl' to be specified"
>;

def err_resource_pragma_xpm_memory_option_conflict : Error<
  "auto|distribute|block|uram can only be selected as one choice for XPM_MEMORY in Resource pragma.">;

def warn_conflict_pragma_parameter_and_ignored: Warning<
  "pragma option '%0' is conflicting with '%1', and option '%2' is ignored">,
  InGroup<IgnoredXlxPragmas>;

def warn_at_least_one_parameter_required: Warning<
  "at least one of '%0' and '%1' option is required">,
  InGroup<IgnoredXlxPragmas>;

def error_pipeline_style_unsupport_value: Error<
  "The 'style' option of the 'HLS pipeline' directive or pragma only has 3 values: stp/flp/frp ">;
def error_pipeline_style_conflict: Error<
  "The 'style' option of the 'HLS pipeline' directive or pragma has conflict with %0">;
def warn_pipeline_enable_flush_deprecate: Warning<
  "The 'enable_flush' option of the 'HLS pipeline' directive or pragma is deprecated, use style=flp instead">, 
  InGroup<IgnoredXlxPragmas>;
def error_stream_type_conflict: Error<
  "The 'type' option of the 'HLS stream' directive or pragma cannot be used with %0">;
def warn_stream_off_deprecate: Warning<
  "The 'off' option of the 'HLS stream' directive or pragma is deprecated, use type=pipo instead.">, 
  InGroup<IgnoredXlxPragmas>;

def warn_register_mode_forward_deprecated: Warning<
  "The register mode 'forward' is deprecated, using the register option instead.">, 
  InGroup<IgnoredXlxPragmas>;
def warn_register_mode_reverse_deprecated: Warning<
  "The register mode 'reverse' is deprecated, using the register option instead.">, 
  InGroup<IgnoredXlxPragmas>;

def error_unknown_interface_mode : Error<
  "The 'mode' option of the 'HLS interface' pragma is either missing or set to incorrect position. Pls guarantee to put it following closely INTERFACE.">;
def warn_invalid_interface_port : Warning<
  "Invalid interface port in '#pragma HLS interface': expect function parameter">,
  InGroup<IgnoredXlxPragmas>;
def warn_invalid_funtion_level_interface_port : Warning<
  "Invalid function level interface port in '#pragma HLS interface': expect 'return'">,
  InGroup<IgnoredXlxPragmas>;
def err_xlx_pragma_invalid_unroll_factor : Error<
  "Invalid pragma unroll factor: '%0'">;
def warn_unsupported_interface_port_data_type : Warning<
  "Unsupported interface port data type in '#pragma HLS interface %0'">,
  InGroup<IgnoredXlxPragmas>;
def warn_unsupported_interface_bram_type : Warning<
  "Array of size 1 cannot be specified as %0, '#pragma HLS interface %0' is ignored">,
  InGroup<IgnoredXlxPragmas>;
def warn_invalid_pragma_variable : Warning<
  "Invalid variable in '#pragma HLS %0': expect variable to be %1">,
  InGroup<InvalidXlxPragmas>;
def warn_implicit_hls_stream : Warning<
  "Invalid variable in '#pragma HLS %0': expect variable to have '%1' type">,
  InGroup<InvalidXlxPragmas>;
def warn_obsolete_pragma_hls_token_replaced : Warning<
  "'%0' in '#pragma HLS %1' is replaced by '%2'">,
  InGroup<ObsoleteXlxPragmas>;
def warn_obsolete_pragma_replaced : Warning<
  "'%0' is obsoleted and replaced by '%1'">,
  InGroup<ObsoleteXlxPragmas>;
def err_xlx_pragma_not_in_function_scope : Error<
  "'#pragma HLS' is only allowed in function scope">;
def warn_xlx_pragma_applied_in_wrong_scope : Warning<
  "'#pragma HLS %0' can only be applied inside %select{function|loop|compound}1 body">, InGroup<InvalidXlxPragmas>;
def err_top_pragma_appiled_in_wrong_scope : Error<
  "'#pragma HLS top' cannot be applied in class method">;

def warn_xlx_invalid_resource_option: Warning< 
  "Invalid Directive: for current device, %0 is invalid combination for %1">;
def err_xlx_invalid_resource_option: Error< 
  "Invalid Directive: for current device, %0 is invalid combination for %1">;

def warn_xlx_bind_op_failed: Warning< 
  "Unknown pragma bind_op variable value '%0', please check that the variable exists in the same scope as the pragma.">;

def err_invalid_variable_in_bind_op : Error< 
  "Invalid pragma bind_op variable value '%0', the value must be a variable reference expression.">;

def err_xlx_invalid_offset_value: Error<
  "unexpected interface offset value '%0', expects '[slave, direct, off]'">;

def warn_attributes_are_not_compatible : Warning<
  "%0 and %1 attributes are not compatible">,
  InGroup<IgnoredXlxAttributes>;
def warn_attribute_argument_n_type : Warning<
  "%0 attribute requires parameter %1 to be %select{int or bool|an integer "
  "constant|a string|an identifier}2">,
  InGroup<IgnoredXlxAttributes>;
def warn_attribute_argument_type : Warning<
  "%0 attribute requires %select{int or bool|an integer "
  "constant|a string|an identifier}1">,
  InGroup<IgnoredXlxAttributes>;

def warn_lower_assert_failed : Warning<
  "Usage of user-defined 'assert' %0 can cause side-effects like missed HLS optimizations. Consider replacing this usage with system defined 'assert' or 'hls::assume()' (defined in hls_assume.h) instead">;

//===----------------------------------------------------------------------===//
// Xilinx HLS  Semantic related Diagnostic message
//===----------------------------------------------------------------------===//

// OpenCL Xilinx Extension
def err_opencl_unroll_attr_wrong_target : Error<
  "opencl_unroll_hint attribute is only allowed on loop statements">;
def err_xcl_pipeline_loop_attr_wrong_target : Error<
  "xcl_pipeline_loop attribute is only allowed on loop statements">;
def err_xcl_parallel_loop_attr_wrong_target : Error<
  "xcl_parallel_loop attribute is only allowed on loop statements">;
def err_xcl_array_map_unknown_mode : Error<
  "unknown array mapping mode '%0'">;
def err_xcl_array_unknown_xform_type : Error<
  "unknown array %select{partition|reshape}0 type '%1'">;
def err_xcl_max_wg_size_wrong_number_arguments : Error<
  "'%0' attribute takes exactly 1 or 3 arguments">;
def err_xcl_dataflow_attr_spmd_kernel: Error<
  "The xcl_dataflow attribute requires use of the reqd_work_group_size(1, 1, 1) attribute value "
  "%select{; did you forget to add __attribute__ ((reqd_work_group_size(1, 1, 1)))?|}0">;
def err_xocc_wrong_compiler_version : Error<
  "'%0' is not supported by the current compiler">;
def warn_xocc_wrong_compiler_version : Warning<
  "'%0' is ignored by the current compiler">, InGroup<DeprecatedXlx>;

// HLS Xilinx Extension
def err_builtin_expected_type : Error<
  "builtin argument expected to be of type %0, was of type %1">;

def err_attr_in_wrong_scope : Error<
  "%0 attribute is only allowed on function or compound statements">;
def err_sdxkernel_in_wrong_scope : Error<
  "%0 cannot be used as top function since it's class method">;
def err_attribute_unsupported_for_func_param
    : Error<"%0 attribute is not supported for function parameters">;
def warn_attribute_unsupported_for_func_param
    : Warning<"%0 attribute is not supported for function parameters">;
def err_attribute_unexpected_value
    : Error<"xcl_dependence %0 = %1  is unexpected, only support %3">;


def warn_xlx_expr_not_ice : Warning<
  "expression is not an %select{integer|integral}0 constant expression">, 
  InGroup<IgnoredXlxPragmas>;


def err_xlx_expr_not_ice : Error<
  "The expression must be a constant integer">;
 
 def err_xlx_option_not_ice: Error< 
 "The option '%0' in pragma '%1' must be an integral constant expression">; 

def err_xlx_expr_not_dce : Error<
  "The expression must be a constant floating number">;

def warn_allocation_conflict: Warning<
  "ALLOCATION pragma conflicts with %0, and will be ignored">,
  InGroup<IgnoredXlxPragmas>;

def warn_xlx_loop_tripcount_ignored: Warning<
  "%0, the pragma LOOP_TRIPCOUNT is ignored">, 
  InGroup<IgnoredXlxPragmas>;

def err_xlx_attribute_invalid_option_and_because
    : Error<"Invalid value for option %0 encountered. %1" >;

def err_xlx_attribute_invalid_option
    : Error<"%0 is not valid value for option %1" >;

def warn_xlx_attribute_ignore_option : Warning<
  "%0 attribute will ignore the option %1 because %2">,
  InGroup<IgnoredXlxPragmas>;
def warn_xlx_attribute_ignore_because_invalid_option: Warning<
  "%0 pragma is ignored, because %1 ">, 
  InGroup<InvalidXlxPragmas>;
def err_xlx_attribute_ignore_because_invalid_option: Error<
  "%0 pragma is ignored, because %1 ">;
def warn_xlx_attribute_argument_outof_range: Warning<
  "%0 requires integer const between %1 and %2 inclusive">, 
  InGroup<InvalidXlxPragmas>;

def warn_xlx_dependence_missing_dependent_option: Warning<
  "Assuming false dependence as a \"true/false\" selection was not specified">,
  InGroup<InvalidXlxPragmas>;

def warn_xlx_dependence_inter_require_distance_option: Warning< 
  "Ignoring invalid 'distance' value in 'dependence' pragma , 'inter' option require 'distance' option presented and 'distance' should be more than 0">,
  InGroup<InvalidXlxPragmas>;

def err_xlx_variable_length_array_type_unsupport: Error<
  "Variable length array is not supported">;

// ------- Scout HLS error for pragma options: error out directly
def err_xlx_pragma_not_supported_by_scout_HLS: Error<
   "the pragma is not supported">;
def err_xlx_pragma_option_not_supported_by_HLS: Error<
   "'%0' pragma '%1' option is not supported, please use '%2' instead.">;
def err_xlx_not_implement_yet :Error<
   "'%0' pragma is not supported">;
// ------- Scout HLS error for pragma options: unsupport but only warning out instead for temp
def err_xlx_pragma_not_supported_by_scout_HLS_WarnOut: Warning<
   "the pragma is not supported and will be ignored">,
  InGroup<InvalidXlxPragmas>;
def err_xlx_pragma_option_not_supported_by_HLS_WarnOut: Warning<
   "the '%1' option to '%0' pragma is not supported and will be ignored">,
  InGroup<InvalidXlxPragmas>;
def err_xlx_pragma_scope_not_supported_by_HLS_WarnOut: Warning<
   "the '%0' pragma is only supported for function and loop and will otherwise be ignored">,
  InGroup<InvalidXlxPragmas>;

def err_xlx_not_supported_by_scout_HLS: Error<
  "%0 is not supported">;

def err_top_argument_type_check: Error<
  "unexpected top argument type: %0">;

def err_xlx_invalid_port_expr: Error<
  "the expression for 'port' option is invalid: %0">;

def warn_xlx_invalid_port_expr: Warning<
  "the expression for 'port' option is invalid, %0">, 
  InGroup<AllXlxDiagnostics>;

//===----------------------------------------------------------------------===//
// Xilinx HLS  CodeGen related Diagnostic message
//===----------------------------------------------------------------------===//
 
def err_unsupported_virt_fuc: Warning<
  "virtual function is not supported">, InGroup<InvalidXlxPragmas>;

def err_invalid_offset_for_alias: Error<
    "'offset' for 'alias' pragma must be non-negative">;


def warn_xlx_attr_wrong_stmt_target : Warning<
  "Only %1 support the %0 pragma" >,InGroup<IgnoredXlxAttributes>;
def warn_xcl_loop_attr_wrong_target : Warning<
  "Only for/while loops support the '%0'">,InGroup<IgnoredXlxAttributes>;
def warn_xcl_dataflow_attr_wrong_target : Warning<
  "Only for-loops and functions support the dataflow">,InGroup<IgnoredDataflowAttr>;
def warn_ignore_xcl_dataflow_on_invalid_loop_initial_stmt : Warning<
  "Since the loop counter is not declared in loop header and/or initialized to '0', the compiler may not successfully process the dataflow loop">,
  InGroup<IgnoredDataflowAttr>;
def warn_ignore_xcl_dataflow_on_multiple_loop_induction_variable : Warning<
  "Due to multiple loop counters, the compiler may not successfully process the dataflow loop">,
  InGroup<IgnoredDataflowAttr>;
def warn_ignore_xcl_dataflow_on_invalid_loop_iteration_stmt : Warning<
  "As the loop counter is not increased by 1, the compiler may not successfully process the dataflow loop">,
  InGroup<IgnoredDataflowAttr>;
def warn_ignore_xcl_dataflow_on_invalid_loop_condition_stmt : Warning<
  "Ignore 'dataflow' on loop: Unsupported for-loop condition statement for dataflow">,
  InGroup<IgnoredDataflowAttr>;
def warn_ignore_xcl_dataflow_on_invalid_loop_bound : Warning<
  "As the loop bound is not a constant or function argument, the compiler may not successfully process the dataflow loop">,
  InGroup<IgnoredDataflowAttr>;
def warn_dataflow_static_variable : Warning<
  "Static scalars and arrays declared inside a dataflow region will be treated like local variables">,
  InGroup<IgnoredDataflowAttr>;
def warn_dataflow_non_static_stream : Warning<
  "Streams will behave like static variables inside a dataflow region">,
  InGroup<IgnoredDataflowAttr>;
def warn_ignore_xcl_dataflow_on_invalid_call_return_type : Warning<
  "'dataflow' ignored: Functions with a return value cannot be used inside a dataflow region">,
  InGroup<IgnoredDataflowAttr>;
def warn_ignore_xcl_dataflow_on_invalid_call_argument_type : Warning<
  "Either use an argument of the function or declare the variable inside the dataflow loop body">,
  InGroup<IgnoredDataflowAttr>;
def warn_ignore_xcl_dataflow_on_invalid_statement : Warning<
  "Since the only kind of statements allowed in a canonical dataflow region are variable declarations and function calls, the compiler may not be able to correctly handle the region">,
  InGroup<IgnoredDataflowAttr>;
def warn_statistics_xcl_dataflow_on_invalid_statement : Warning<
  "There are a total of %0 such instances of non-canonical statements in the dataflow region">,
  InGroup<IgnoredDataflowAttr>;

def err_xlx_float2int_failed : Error<
  "Convert floating number into integer failed">;

def warn_xlx_float2int_inexact: Warning<
  "Convert floating number into integer causes fractional truncation">;

def err_xlx_set_directive_fail: Error< 
  "%1" > ; 
def err_xlx_pragma_if_cond_init_var: Error< 
  "the init-statement is not expected in if-cond clause in the pramga"> ; 
def err_xlx_pragma_if_cond_constexpr: Error< 
  "if-cond clause is not 'constexpr' in the pragma "> ; 

def err_xlx_clock_period_is_invalid: Error<
    "unexpected, the clock period is 0, and can not evaluate the number of cycles of performance region" >; 
def warn_xlx_performance_option_is_near_to_zero: Warning<
    "the performance pragma is ignored, because option '%0' in performance pragma is too small, with clock period %1, it is near to zero">; 
def err_hls_function_name_match_arg: Error< 
    "unexpected argument type, the argument type for  __hls_function_name_match function call must be string literal">; 

def warn_xlx_ignore_duplicate_pragma: Warning<
    "there are more than one pragma %0 in the %1 scope, ignore the pragma ">; 

}


}

