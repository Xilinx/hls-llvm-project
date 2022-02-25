// (C) Copyright 2016-2021 Xilinx, Inc.
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
// Xilinx's pragma information, which will be used by both clang and GUI. Please do not modify the content of this file.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_XILINXFPGAHLS_PRAGMA_OPTION_DEFINE_H
#define LLVM_SUPPORT_XILINXFPGAHLS_PRAGMA_OPTION_DEFINE_H


/** the macros definition for pragma options in clang and GUI **/

#ifndef HLS_PRAGMA_DEF
#define HLS_PRAGMA_DEF(name) 
#endif


#define HLS_PRAGMA_Dataflow\
  PresentID(interval)\
  PresentID(disable_start_propagation)

#define HLS_PRAGMA_Pipeline \
    OptIntConstExpr(ii, -1)\
    OptEnumExpr(style, {"stp" COMMA_SPERATOR "flp" COMMA_SPERATOR "frp"}, flp)\
    PresentID(rewind)\
    PresentID(enable_flush) \
    PresentID(off) 

HLS_PRAGMA_DEF(Pipeline)


#define HLS_OPTION_RULE_Pipeline\
   ConflictDecl( enable_flush, style) \
   ConflictDecl( off, style) \
   ConflictDecl( rewind, style) \
   ConflictDecl( rewind, off) \
   ConflictDecl( enable_flush, off) \
   DeprecatedOption(enable_flush) 
  


#define HLS_PRAGMA_Unroll \
    OptIntConstExpr(factor, -1)\
    PresentID(region)\
    PresentID(skip_exit_check)\
    PresentID(complete)\
    PresentID(partial)

HLS_PRAGMA_DEF(Unroll)

#define HLS_OPTION_RULE_Unroll \
    DeprecatedOption(complete)\
    DeprecatedOption(partial)\
    DeprecatedOption(region)


#define HLS_PRAGMA_Loop_Merge\
  PresentID(force)

HLS_PRAGMA_DEF(Loop_Merge)

#define HLS_PRAGMA_Loop_Flatten\
  PresentID(off)

HLS_PRAGMA_DEF(Loop_Flatten)

#define HLS_PRAGMA_Loop_TripCount\
  OptIntConstExpr(min, 0)\
  OptIntConstExpr(max, 0)\
  OptIntConstExpr(avg, 0)

HLS_PRAGMA_DEF(Loop_TripCount)

#define HLS_PRAGMA_Stream\
  ReqVariableExpr(variable)\
  OptIntConstExpr(depth, 1)\
  PresentID(off)
  
HLS_PRAGMA_DEF(Stream)

#define HLS_PRAGMA_Resource\
  ReqVariableExpr(variable)\
  ReqIDExpr(core)\
  OptEnumExpr(acc_mode, {"none" COMMA_SPERATOR "encode" COMMA_SPERATOR "decode" COMMA_SPERATOR "both"}, both)\
  OptIntConstExpr(latency, -1)\
  PresentID(auto)\
  PresentID(distribute)\
  PresentID(block)\
  PresentID(uram)\
  OptIDExpr(metadata, "")

HLS_PRAGMA_DEF(Resource)

#define HLS_PRAGMA_Inline\
  PresentID(off)

HLS_PRAGMA_DEF(Inline)

#define HLS_PRAGMA_Reset\
  ReqVariableExpr(variable)\
  PresentID(off)

HLS_PRAGMA_DEF(Reset)

#define HLS_PRAGMA_Allocation\
  PresentEnum(type, { "function" COMMA_SPERATOR "operation"} )\
  ReqIDExprWithDep(instance, operation)\
  ReqVariableExprWithDep(instance, function)

HLS_PRAGMA_DEF(Allocation)

#define HLS_PRAGMA_Expression_Balance\
  PresentID(off)

HLS_PRAGMA_DEF(Expression_Balance)

#define HLS_PRAGMA_Function_Instantiate\
  ReqVariableExpr(variable)

HLS_PRAGMA_DEF(Function_Instantiate)

#define HLS_PRAGMA_Occurrence\
  PresentID(cycle)

HLS_PRAGMA_DEF(Function_Instantiate)

#define HLS_PRAGMA_Protocal\
  PresentEnum(type, {"floating" COMMA_SPERATOR "fixed"})
 
HLS_PRAGMA_DEF(Protocal)

#define HLS_PRAGMA_Latency\
  ReqIntConstExpr(min)\
  ReqIntConstExpr(max)

HLS_PRAGMA_DEF(Latency)


#define HLS_PRAGMA_Array_Partition\
  ReqVariableExpr(variabel)\
  ReqIntConstExpr(factor)\
  OptIntConstExpr(dim, 1)\
  PresentEnum(type, { "block" COMMA_SPERATOR "cyclic" COMMA_SPERATOR "complete"} )
HLS_PRAGMA_DEF(Array_Partition)

#define HLS_PRAGMA_Array_Reshape\
  ReqVariableExpr(variabel)\
  ReqIntConstExpr(factor)\
  OptIntConstExpr(dim, 1)\
  PresentEnum(type, { "block" COMMA_SPERATOR "cyclic" COMMA_SPERATOR "complete"} )
HLS_PRAGMA_DEF(Array_Reshape)


#define HLS_PRAGMA_Top\
  PresentID(name)
HLS_PRAGMA_DEF(Top)

//TODO, cross_variable and variable  is exclud, and one of them must be showing 
#define HLS_PRAGMA_Dependence\
  OptVariableExpr(cross_variables)\
  OptVariableExpr(variable)\
  PresentEnum(dep_obj, {"array" COMMA_SPERATOR "pointer"})\
  PresentEnum(type, {"intra" COMMA_SPERATOR "inter"})\
  PresentEnum(direction, {"raw" COMMA_SPERATOR "war" COMMA_SPERATOR "waw"} )\
  OptIntConstExpr(distance, -1)\
  PresentEnum(dependent, {"false" COMMA_SPERATOR "true"})

HLS_PRAGMA_DEF(Dependence)

#define HLS_PRAGMA_Stable\
  ReqVariableExpr(variable)
HLS_PRAGMA_DEF(Stable)

#define HLS_PRAGMA_Stable_Content\
  ReqVariableExpr(variable)
HLS_PRAGMA_DEF(Stable_Content)

#define HLS_PRAGMA_Shared\
  ReqVariableExpr(variable)
HLS_PRAGMA_DEF(Shared)

#define HLS_PRAGMA_Disaggregate\
  ReqVariableExpr(variable)
HLS_PRAGMA_DEF(Disaggregate)

#define HLS_PRAGMA_Aggregate\
  ReqVariableExpr(variable)\
  PresentEnum(type, { "bit" COMMA_SPERATOR "byte" COMMA_SPERATOR "none"})
HLS_PRAGMA_DEF(Aggregate)

#define HLS_PRAGMA_Bind_Op\
  ReqVariableExpr(variable)\
  ReqIDExpr(op)\
  OptIDExpr(impl, "")\
  OptIntConstExpr(latency, -1)
HLS_PRAGMA_DEF(Bind_Op)

#define HLS_PRAGMA_Bind_Storage\
  ReqVariableExpr(variable)\
  ReqIDExpr(type)\
  OptIDExpr(impl, "")\
  OptIntConstExpr(latency, -1)
HLS_PRAGMA_DEF(Bind_Storage)



#define HLS_SUB_PRAGMA_MAXI \
  ReqVariableExpr(port)\
  PresentEnum(maxi_mode_name,  { "m_axi" })\
  OptIDExpr( bundle, "0")\
  OptIntConstExpr(depth, 1)\
  OptEnumExprWithDep(offset, {"off" COMMA_SPERATOR "direct" COMMA_SPERATOR "slave"}, off, )\
  OptIDExpr(name, "0")\
  OptIntConstExpr(num_read_outstanding, 0)\
  OptIntConstExpr(num_write_outstanding, 0)\
  OptIntConstExpr(num_read_burst_length, 0)\
  OptIntConstExpr(num_write_burst_length, 0)\
  OptIntConstExpr(latency, -1)\
  OptIntConstExpr(max_widen_bitwidth, 0)

#define HLS_SUB_PRAGMA_SAXILITE\
  ReqVariableExpr(port)\
  PresentEnum(saxilit_mode_name, {"s_axilit"})\
  OptIDExpr(bundle, "0")\
  PresentID(register)\
  OptIntConstExprWithDep(offset, -1, m_axi)\
  OptIDExpr(clock, "")\
  OptIDExpr(name, "0")

#define HLS_SUB_PRAGMA_AXIS\
  ReqVariableExpr(port)\
  PresentEnum(axis_mode_name, {"axis"})\
  PresentID(register)\
  OptEnumExpr(register_mode, { "forward" COMMA_SPERATOR "reverse" COMMA_SPERATOR "both"  COMMA_SPERATOR "off"}, both)\
  PresentEnum(register_mode_present, {"forward" COMMA_SPERATOR "reverse" COMMA_SPERATOR "both" COMMA_SPERATOR "off"})\
  OptIntConstExpr(depth, 1) \
  OptIDExpr(name, "0")
  
#define HLS_SUB_PRAGMA_MEMORY\
  ReqVariableExpr(port)\
  PresentEnum(memory_mode_name, {"ap_memory" COMMA_SPERATOR  "bram" COMMA_SPERATOR "ap_fifo"})\
  OptIntConstExpr(depth, 1)\
  OptIntConstExpr(latency, -1)\
  OptIDExpr(storage_type, default)\
  OptIDExpr(name, "")

#define HLS_SUB_PRAGMA_SCALAR\
  ReqVariableExpr(port)\
  OptIntConstExpr(depth, 1)\
  OptIntConstExpr(latency, -1)\
  PresentID(register)\
  OptIDExpr(name, "")
 
#define HLS_SUB_PRAGMA_AP_NONE\
  ReqVariableExpr(port)

#define HLS_PRAGMA_Interface \
  HLS_SUB_PRAGMA_MAXI\
  HLS_SUB_PRAGMA_SAXILITE\
  HLS_SUB_PRAGMA_AXIS\
                     \
  HLS_SUB_PRAGMA_MEMORY\
                       \
  HLS_SUB_PRAGMA_AP_NONE\
  HLS_SUB_PRAGMA_SCALAR

HLS_PRAGMA_DEF(Interface)




#endif
