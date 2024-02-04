//==--- XlxPragmaOption.td - Xilinx HLS pragma option definitions -----------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// And has the following additional copyright:
//
// (C) Copyright 2016-2022 Xilinx, Inc.
// Copyright (C) 2023, Advanced Micro Devices, Inc.
// All Rights Reserved.
//
//===----------------------------------------------------------------------===//

class VisibilityClass; 

def Public: VisibilityClass; 
def Deprecated: VisibilityClass; 
def Unsupported: VisibilityClass; 
def Hidden: VisibilityClass; 

class ExplicitOrImplicit; 
def Implic: ExplicitOrImplicit; 
def Explicit: ExplicitOrImplicit; 


// Option format used to mark the format the Pragma Option used
class OptionFormat<string format> {
    string Format = format;
}

// Default values
class DefaultInt<int default> {
    int Default = default;
}
class DefaultString<string default> {
    string Default = default;
}

// def option format
def PresentFormat : OptionFormat<"PresentFormat">;
def EnumPresentFormat : OptionFormat<"EnumPresentFormat">;
def EnumExprFormat : OptionFormat<"EnumExprFormat">;
def IntConstExprFormat : OptionFormat<"IntConstExprFormat">;
def VariableExprFormat : OptionFormat<"VariableExprFormat">;
def IDExprFormat : OptionFormat<"IDExprFormat">;
def DoubleConstExprFormat : OptionFormat<"DoubleConstExprFormat">;


// define Option class for HLS pragma option
class Option<string name, string description, OptionFormat format, bit optional, VisibilityClass visibility ,  ExplicitOrImplicit explicit_or_implicit = Explicit > {
    string Name = name; 
    OptionFormat Format = format; 
    bit Optional = optional;
    string Desc = description;
    VisibilityClass Visibility  = visibility; 
    ExplicitOrImplicit UsageOfDefault = explicit_or_implicit;
    
}

// derive the concrete option layout
class PresentOption<string name, int groupID, string description, VisibilityClass visibility = Public, OptionFormat format = PresentFormat> : Option<name, description, format, 1, visibility> {
    int GroupID = groupID;
}
class EnumPresentOption<string parentName, string name, int groupID, string description, VisibilityClass visibility = Public> : PresentOption<name, groupID, description, visibility, EnumPresentFormat> {
  string ParentName = parentName;
}
 
// Enum Format
class EnumExprOption<string name, list<string> enums, bit optional, string description, VisibilityClass visibility, string default = "" > : Option<name, description, EnumExprFormat, optional, visibility>, DefaultString<default> {
    list<string> Enums = enums;
}
class OptEnumExprOption<string name, list<string> enums, string description, VisibilityClass visibility = Public, string default = ""> : EnumExprOption<name, enums, 1, description, visibility, default>;
class ReqEnumExprOption<string name, list<string> enums, string description, VisibilityClass visibility = Public, string default = ""> : EnumExprOption<name, enums, 0, description, visibility, default>;

// integer constant format
class IntConstExprOption<string name, bit optional, string description, VisibilityClass visibility = Public> : Option<name, description, IntConstExprFormat, optional, visibility>;
class ReqIntConstExprOption<string name, string description, VisibilityClass visibility = Public> : IntConstExprOption<name, 0, description, visibility>;
class OptIntConstExprOption<string name, string description, VisibilityClass visibility = Public> : IntConstExprOption<name, 1, description, visibility>;
class OptIntDefaultConstExprOption<string name, int default, string description, VisibilityClass visibility = Public> : OptIntConstExprOption<name, description, visibility>, DefaultInt<default>;
class OptUserVisibleIntDefaultConstExprOption<string name, int default, string userVisibleDefault,  string description, VisibilityClass visibility = Public> : OptIntDefaultConstExprOption<name, default, description, visibility>; 
// constant with unit format
class DoubleConstExprOption<string name, bit optional, string description, VisibilityClass visibility = Public> : Option<name, description, DoubleConstExprFormat, optional, visibility>;
class ReqDoubleConstExprOption<string name, string description, VisibilityClass visibility = Public > : DoubleConstExprOption<name, 0, description, visibility>;
class OptDoubleConstExprOption<string name, string default, string description, VisibilityClass visibility = Public > : DoubleConstExprOption<name, 1, description, visibility>, DefaultString<default>;

// variable reference format
class VariableExprOption<string name, bit optional, string description, VisibilityClass visibility = Public> : Option<name, description, VariableExprFormat, optional, visibility>;
class OptVariableExprOption<string name, string description, VisibilityClass visibility = Public> : VariableExprOption<name, 1, description>;
class ReqVariableExprOption<string name, string description, VisibilityClass visibility = Public> : VariableExprOption<name, 0, description>;

// ID format
class IDExprOption<string name, bit optional, string description, VisibilityClass visibility = Public> : Option<name, description, IDExprFormat, optional, visibility>;
class OptIDExprOption<string name, string default, string description, VisibilityClass visibility = Public> : IDExprOption<name, 1, description, visibility>, DefaultString<default>;
class ReqIDExprOption<string name, string description, VisibilityClass visibility = Public> : IDExprOption<name, 0, description, visibility>;

class ObjectClass; 

def Variable:   ObjectClass;
def Parameter:  ObjectClass;
def Struct:     ObjectClass;
def Array:    ObjectClass;
def Loop:     ObjectClass; 
def Region:   ObjectClass; 
def Function: ObjectClass; 
def Top:      ObjectClass; 
def Static:      ObjectClass; 



def modeSelect;

class ModesFormat{ 
  dag Modes;
  string ModeOptionName; 
}


// define HLS pragma layout
class HLSPragma {
    string Name;
    string Desc;
    VisibilityClass Visibility = Public;
    
    list<ObjectClass> Objects; 
    list<Option> Options;
}   

// # pragma HLS alias ports=<variable> [offset=<variable>] [distance=<variable>]
def XlxAliasPragma : HLSPragma {
    let Name = "alias";
    let Desc = "Specify that two or more M_AXI pointer arguments all point into the same underlying buffer in DRAM (DDR or HBM) and indicate any aliasing between the pointers by setting the distance between them"; 
    let Objects = [Parameter, Function];

    let Options = [ReqVariableExprOption<"ports",  "Specifies the list of ports that point into the same MAXI-mapped array">,
                   OptVariableExprOption<"offset", "Specifies the offset of the pointer passed to each port in the -ports list with respect to the origin of the array">,
                   OptVariableExprOption<"distance",  "Specifies the difference between the pointer values passed to each pair of successive ports in the list" >];
}

// #pragma HLS aggregate variable=<variable> [none | bit | byte | auto] [compact = {none | bit | byte | auto}]
def XlxAggregatePragma : HLSPragma {
    let Name = "aggregate";
    let Desc = "Specify that a struct or class variable should be aggregated."; 
    let Objects = [Struct, Array];
    let Options = [ReqVariableExprOption<"variable" , "" >,
                   EnumPresentOption<"compact", "bit", 1, "">,
                   EnumPresentOption<"compact", "byte", 1, "">,
                   EnumPresentOption<"compact", "none", 1, "">,
                   EnumPresentOption<"compact", "auto", 1, "">,
                   OptEnumExprOption<"compact", ["bit", "byte", "none", "auto"], "Specify the padding alignment", Public, "auto">
                   ];
}


// #pragma HLS allocation [operation | function] instances=<list> limit=<int>
def XlxFunctionAllocationPragma : HLSPragma {
    let Name = "function_allocation";
    let Desc = "function allocation" ; 
    let Objects = [Region];

    let Options = [
        ReqVariableExprOption<"instances", "A function or operator. The function can be any function in the original C code which has not been inlined. Valid operators are: add sub mul icmp sdiv udiv srem urem lshr ashr shl fadd fsub fmul fdiv dadd dsub dmul ddiv fcmp frem fsqrt frsqrt frecip dsqrt drecip dcmp drsqrt flog dlog fexp dexp"> 
    ];

}

def XlxOperationAllocationPragma : HLSPragma {
    let Name = "operation_allocation";
    let Desc = "operation allocation"; 
    let Objects = [Region]; 

    let Options = [
      ReqIDExprOption<"instances", "A function or operator. The function can be any function in the original C code which has not been inlined. Valid operators are: add sub mul icmp sdiv udiv srem urem lshr ashr shl fadd fsub fmul fdiv dadd dsub dmul ddiv fcmp frem fsqrt frsqrt frecip dsqrt drecip dcmp drsqrt flog dlog fexp dexp"> 
    ];
}

def XlxAllocationPragma: HLSPragma, ModesFormat { 
  let Name = "allocation";
  let Desc = "Specify a limit for the number of operations, implementations, or functions used. This can force the sharing or hardware resources and may increase latency." ; 
  let Objects = [Region];

  let Options = [ 
      EnumPresentOption<"type", "function", 1, "">, 
      EnumPresentOption<"type", "operation", 1, "">,
      OptEnumExprOption<"type", ["operation", "function"], "", Public, "function">, // ETP: New
      ReqIntConstExprOption<"limit", "A maximum limit on the number of instances (of the type defined by the -type option) to be used in the RTL design.">
  ];
  let ModeOptionName = "type";
  let Modes = (modeSelect XlxFunctionAllocationPragma: $function, XlxOperationAllocationPragma : $operation);
}


// #pragma HLS array_partition variable=<name> [cyclic | block | complete] [factor=<int>] [dim=<int>] [type] = {cyclic | block | complete} [dynamic] [off[=<false|true|0|1>]]
def XlxArrayPartitionPragma : HLSPragma {
    let Name = "array_partition";
    let Desc = "Partitions large arrays into multiple smaller arrays or into individual registers, to improve access to data and remove block RAM bottlenecks."; 
    let Objects = [Array, Function];

    let Options = [ReqVariableExprOption<"variable",  "">,
                   OptIntConstExprOption<"factor", "Integer number to specify the number of smaller arrays which are to be created. This option is only relevant for type 'block' or 'cyclic' partitioning.">,
                   OptUserVisibleIntDefaultConstExprOption<"dim", 1, "1", "This is only relevant for multi-dimensional arrays and specifies which dimension of the array is to be partitioned. If a value of 0 is used, all dimensions will be partitioned with the specified options. Any other value will partition only the specified dimension.">,
                   EnumPresentOption<"type", "block", 1, "">,
                   EnumPresentOption<"type", "cyclic", 1, "">,
                   EnumPresentOption<"type", "complete", 1, "">,
                   OptEnumExprOption<"type", ["block", "cyclic", "complete"], "Block partitioning creates smaller arrays from consecutive blocks of the original array. Cyclic partitioning creates smaller arrays by interleaving elements from the original array. Complete partitioning decomposes the array into individual elements. ", Public, "complete">,
                   PresentOption<"dynamic", 0, "", Hidden>,
                   PresentOption<"off", 2, "Turn off auto array partition">
                   ];
}

// #pragma HLS array_reshape variable=<name> [cyclic | block | complete] [factor=<int>] [dim=<int>] [type] = {cyclic | block | complete} [object] [off[=<false|true|0|1>]]
def XlxArrayReshapePragma : HLSPragma {
    let Name = "array_reshape";
    let Desc = "Reshape an array from one with many elements to one with greater word-width. Useful for improving block RAM accesses without using more block RAM." ; 
    let Objects  = [Array, Function]; 

    let Options = [ReqVariableExprOption<"variable", "">,
                   OptIntConstExprOption<"factor", "Integer number to specify the number of temporary smaller arrays which are to be created; Only relevant for type 'block' or 'cyclic' reshaping.">,
                   OptUserVisibleIntDefaultConstExprOption<"dim", 1, "1", "This is only relevant for multi-dimensional arrays and specifies which dimension of the array is to be reshaped. If a value of 0 is used, all dimensions will be reshaped with the specified options. Any other value will reshape only the specified dimension.">,
                   EnumPresentOption<"type", "block", 1, "">,
                   EnumPresentOption<"type", "cyclic", 1, "">,
                   EnumPresentOption<"type", "complete", 1, "">,
                   PresentOption<"object", 2, "This is only relevant for container arrays and applies reshape on the objects within the container. If the option is specified, all dimensions of the objects will be reshaped, but all dimensions of container will be kept.">,
                   OptEnumExprOption<"type", ["block", "cyclic", "complete"], "Block reshaping creates smaller arrays from consecutive blocks of the original array. Cyclic reshaping creates smaller arrays by interleaving elements from the original array. Complete reshaping decomposes the array into temporary individual elements and then recombines them into an array with a wider word.", Public, "complete">,
                   PresentOption<"off", 3, "Turn off auto array reshape">
         ];
}

// #pragma HLS array_stencil variable=<variable> [off]
def XlxArrayStencilPragma : HLSPragma {
    let Visibility = Hidden;
    let Name = "array_stencil";
    let Desc = "Generates line buffer and sliding window for 1D/2D maxi interface variable with stencil access pattern and enables burst." ;
    let Objects = [Array, Parameter] ; 
    let Options = [
        ReqVariableExprOption<"variable", "Specify the specific variable to consider for the array_stencil directive.">,
        PresentOption<"off", 1, "If present, auto array_stencil will not take place on the variable in the loop where the pragma is placed.">
        ];
}

// #pragma HLS bind_op variable=<variable> op=<type> [impl=<value>] [latency=<int>]
def XlxBindOpPragma : HLSPragma {
    let Name = "bind_op";
    let Desc = "Define a specific implementation and latency for an operation in the RTL"; 
    let Objects = [Array, Parameter, Function];

    let Options = [ReqVariableExprOption<"variable", "">,
                   ReqIDExprOption<"op", "Specify the operator type">,
                   OptIDExprOption<"impl", "", "Specify the implementation style for the operator">,
                   OptIntDefaultConstExprOption<"latency", -1, "Specify the latency the operator">];
}

// #pragma HLS bind_storage variable=<variable> type=<type> [impl=<value>] [latency=<int>]
def XlxBindStoragePragma : HLSPragma {
    let Name = "bind_storage";
    let Desc = "Define a specific implementation and latency for a storage element, or memory, in the RTL"; 
    let Objects = [Array, Parameter, Function]; 

    let Options = [ReqVariableExprOption<"variable", "">,
                   ReqIDExprOption<"type", "Specify the storage type">,
                   OptIDExprOption<"impl", "", "Specify the implementation style for the storage">,
                   OptIntDefaultConstExprOption<"latency", -1, "Specify the latency for the storage">];
}

// #pragma HLS dataflow [disable_start_propagation]
def XlxDataflowPragma : HLSPragma {
    let Name = "dataflow";
    let Desc = "Enables task level pipelining, allowing functions and loops to execute concurrently. Used to optimize throughput and/or latency."; 
    let Objects = [Function, Loop, Region]; 

    let Options = [OptIntDefaultConstExprOption<"interval", -1, "", Deprecated>,
                   PresentOption<"disable_start_propagation", 1, "Disable the start propagation control layer that is added to control when each process inside a dataflow region can start.">];
}

// #pragma HLS data_pack variable=<variable> [instance=<string>] [struct_level | field_level]
def XlxDataPackPragma : HLSPragma {
    let Name = "data_pack";
    let Visibility = Hidden;
    let Desc = "" ; 
    let Objects = [Struct] ; 
    let Options = [ReqVariableExprOption<"variable", "">,
                   OptIDExprOption<"instance", "", "">,
                   EnumPresentOption<"byte_pad", "struct_level", 1, "">,
                   EnumPresentOption<"byte_pad", "field_level", 1, "">,
                   OptEnumExprOption<"byte_pad", ["struct_level", "field_level"],  ""> // ETP: New
                   ];
}

// #pragma HLS dependence [cross_variables=<variable>] [variable=<variable>] [array | pointer] [intra | inter] [distance=<int>] [raw | war | waw]
//     [false | true] [class = {array | pointer}] [type = {intra | inter}] [dependent = {true | false}] [direction = {raw | war | waw}]
def XlxDependecePragma : HLSPragma {
    let Name = "dependence";
    let Desc = "Used to provide additional information that can overcome loop-carried dependencies and allow loops to be pipelined (or pipelined with lower intervals)" ; 
    let Objects = [Region]; 

    let Options = [OptVariableExprOption<"cross_variables", "", Hidden>,
                   OptVariableExprOption<"variable", "Specify the specific variable to consider for the dependence directive; This is mutually exclusive with the -class option.">,
                   EnumPresentOption<"class", "array", 1, "">,
                   EnumPresentOption<"class", "pointer", 1, "">,
                   EnumPresentOption<"type", "intra", 2, "">,
                   EnumPresentOption<"type", "inter", 2, "">,
                   EnumPresentOption<"direction", "raw", 3, "RAW is Read-After-Write">,
                   EnumPresentOption<"direction", "war", 3, "WAR is Write-After-Read">,
                   EnumPresentOption<"direction", "waw", 3, "WAW is Write-After-Write">,
                   OptIntConstExprOption<"distance", "Specify the inter-iteration distance for array access as a positive integer value. This is only relevant for loop-carry dependencies where the -dependent option is set to true.">,
                   EnumPresentOption<"dependent", "true", 0, "">,
                   EnumPresentOption<"dependent", "false", 0, "">,
                   OptEnumExprOption<"class", ["array", "pointer"], "Specify a class of variables where the dependence need clarification. This is mutually exclusive with the -variable option.">,
                   OptEnumExprOption<"type", ["intra", "inter"], "Specify if the dependence is within the same loop iteration (intra) or between different loop iterations (inter).", Public, "inter">,
                   OptEnumExprOption<"dependent", ["true", "false"], "Specify if a dependence needs to be enforced (true) or removed (false).", Public, "false">,
                   OptEnumExprOption<"direction", ["raw", "war", "waw"], "Specify the direction for a dependence, this is only relevant for loop-carry dependencies. RAW is Read-After-Write, WAR is Write-After-Read, WAW is Write-After-Write">];
}

// #pragma HLS disaggregate variable=<variable>
def XlxDisaggregatePragma : HLSPragma {
    let Name = "disaggregate";
    let Desc = "Break a struct down into its individual elements."; 
    let Objects = [Struct, Array];
    let Options = [ReqVariableExprOption<"variable", "">];
}

// #pragma HLS expression_balance [off[=<false|true|0|1>]]
def XlxExpressionBalancePragma : HLSPragma {
    let Name = "expression_balance";
    let Desc = "Allows automatic expression balancing to be turned off"; 
    let Objects = [Region]; 
    let Options = [PresentOption<"off", 1, "Turn off expression balancing">
                  ];
}

// todo: function_instantiate pragma parser use subjectParam to parser variable option, maybe we need refactor this to integrated into td parser
// #pragma HLS function_instantiate variable=<variable>
def XlxFunctionInstantiatePragma: HLSPragma {
    let Name = "function_instantiate";
    let Desc = "";
    let Objects = [Function]; 
    let Options = [ReqVariableExprOption<"variable", "">];
}

// #pragma HLS inline [self | region | all] [recursive] [off[=<false|true|0|1>]]
def XlxInlinePragma : HLSPragma {
    let Name = "inline";
    let Desc = "Inlines a function, removing function hierarchy at this level."; 
    let Objects = [Region]; 
    let Options = [  EnumPresentOption<"type", "self", 1, "", Hidden>,
                     EnumPresentOption<"type", "all", 1, "", Hidden>,
                     EnumPresentOption<"type", "region", 1, "", Hidden>,
                     OptEnumExprOption<"type", ["self", "all", "region"], "", Hidden>, // ETP: New
                     PresentOption<"recursive", 2, "By default only one level of function inlining is performed. The -recursive option inlines all functions recursively down the hierarchy.">,
                     PresentOption<"off", 3, "This disables function inlining and is used to prevent particular functions from being inlined.">
                  ];
}

// #pragma HLS latency [min=<int>] [max=<int>]
def XlxLatencyPragma : HLSPragma {
    let Name = "latency";
    let Desc = "Allows a minimum and/or maximum latency constraint to be specified"; 
    let Objects = [Region]; 
    let Options = [OptIntDefaultConstExprOption<"min", 0, "">,
                   OptIntDefaultConstExprOption<"max", 65535, "">];
}

// #pragma HLS loop_flatten [off[=<false|true|0|1>]]
def XlxLoopFlattenPragma : HLSPragma {
    let Name = "loop_flatten";
    let Desc = "Allows nested loops to be collapsed into a single loop with improved latency"; 
    let Objects = [Loop]; 
    let Options = [PresentOption<"off", 1, "This option prevents flattening from taking place. This can be used to prevent some loops from being flattened while all others in the specified location are flattened.">
                  ];
}

// #pragma HLS loop_merge [force]
def XlxLoopMergePragma : HLSPragma {
    let Name = "loop_merge";
    let Desc = "Merge consecutive loops to reduce overall latency, increase sharing and improve logic optimization"; 
    let Objects = [Region];
    let Options = [PresentOption<"force", 1, "This option forces loops to be merged even when Vitis HLS issues a warning. In this case the user takes responsibility that the merged loop will function correctly.">];
}

// #pragma HLS loop_tripcount min=<int> max=<int> avg=<int>
def XlxLoopTripCountPragma : HLSPragma {
    let Name = "loop_tripcount";
    let Desc = "Used for loops which have variables bounds to provide an estimate for the loop iteration count. This has no impact on synthesis, only on reporting."; 
    let Objects = [Loop]; 
    let Options = [OptIntDefaultConstExprOption<"min", 0, "Specify the minimum loop tripcount">,
                   ReqIntConstExprOption<"max", "Specify the maximum loop tripcount">,
                   OptIntDefaultConstExprOption<"avg", 0, "Specify the average loop tripcount">];
}

// #pragma HLS occurrence [cycle=<int>] [off[=<false|true|0|1>]]
def XlxOccurrencePragma : HLSPragma {
    let Name = "occurrence";
    let Desc = "Used when pipelining functions or loops, to specify that the code in a location is executed at a lesser rate than the code in the enclosing function or loop"; 
    let Objects = [Region] ; 
    let Options = [OptIntDefaultConstExprOption<"cycle", 1, "Specify the occurrence N/M, where N is the number of times the enclosing function or loop is executed and M is the number of times the conditional region is executed. N must be an integer multiple of M.">,
                   PresentOption<"off", 2, "Turn off auto occurrence inference">
                  ];
}

// #pragma HLS PERFORMANCE target_ti=<value> target_tl=<value> assume_ti=<value> assume_tl=<value> [scope=[loop|region]]
// scope default is loop 
def XlxPerformancePragma : HLSPragma {
    let Name = "performance";
    let Desc = "Allows a single pragma to parallelize a loop nests based on a performance target, and can be accomplished by internally applying pipeline, unroll, and array partition pragmas to the associated loops/arrays."; 
    let Objects = [Loop, Region]; 
    let Options = [
                   OptDoubleConstExprOption<"target_ti", "0.0", "Specify the value of transaction interval that is targeted for the scope.">,
                   OptDoubleConstExprOption<"target_tl", "0.0", "Specify the value of transaction latency that is targeted for the scope.">,
                   OptDoubleConstExprOption<"assume_ti", "0.0", "", Hidden>,
                   OptDoubleConstExprOption<"assume_tl", "0.0", "", Hidden>,
                   OptEnumExprOption<"unit", ["cycle", "sec"], "">, 
                   OptEnumExprOption<"scope", ["loop", "region"], "", Hidden>
                  ];
}

// #pragma HLS pipeline II=<int> [off[=<false|true|0|1>]] [rewind[=<true|false>]] enable_flush
def XlxPipelinePragma : HLSPragma {
    let Name = "pipeline";
    let Desc = "Reduces the initiation interval by allowing overlapped execution of operations within a loop or function"; 
    let Objects = [Function, Loop]; 

    let Options = [OptIntDefaultConstExprOption<"ii", -1, "A positive integer value specifying the desired initiation interval for the pipeline.">,
                   PresentOption<"rewind", 1, "This enables a rewinding architecture for loop pipelines, which has continuous pipelining with no pause between one loop iteration ending and the next one starting.">,
                   OptEnumExprOption<"rewind", ["true", "false"], "">,
                   PresentOption<"enable_flush", 2, "Use style=flp instead", Deprecated>,
                   PresentOption<"off", 3, "Disable loop pipelining for a loop that is automatically pipelined via 'config_compile -pipeline_loops'.">,
                   OptEnumExprOption<"style", ["stp", "flp", "frp"], "Specify the pipeline implementation style">
                  ];
}

// #pragma HLS protocol [floating | fixed] [mode={floating | fixed}]
def XlxProtocolPragma : HLSPragma {
    let Name = "protocol";
    let Desc = "Specify a region of code as a 'protocol' region where no clock operations will be inserted by Vitis HLS unless explicitly specified in the code" ; 
    let Objects = [Region] ; 
    let Options = [EnumPresentOption<"mode", "floating", 1, "">,
                   EnumPresentOption<"mode", "fixed", 1, "">,
                   OptEnumExprOption<"mode", ["fixed", "floating"], "Floating mode allows code from outside the protocol region to overlap with the region execution (other operations may be concurrent to the region).  Fixed mode ensures there is no overlap.", Public, "floating">];
}

// #pragma HLS reset variable=<a> [off[=<false|true|0|1>]]
def XlxResetPragma : HLSPragma {
    let Name = "reset";
    let Desc = "This command can be used to add or remove reset for specific state variables (global or static)"; 
    let Objects = [Static] ; 
    let Options = [ReqVariableExprOption<"variable", "">,
                   PresentOption<"off", 1, "If present, reset will not be generated for the specified variable. On the other hand, without the option, reset will be generated for the specified variable.">
                  ];
}

// #pragma resource variable=<variable> core=<string> [latency=<int>] [ecc_mode={none | encode | decode | both}] 
//               [auto | distribute | block | uram] [metadata=<string>]
def XlxResoucePragma : HLSPragma {
    let Name = "resource";
    let Visibility = Deprecated;
    let Desc = ""; 
    let Objects = [Array, Parameter, Function]; 
    let Options = [ReqVariableExprOption<"variable", "", Deprecated>,
                   ReqIDExprOption<"core", "", Deprecated>,
                   OptEnumExprOption<"ecc_mode", ["none", "encode", "decode", "both"], "", Hidden>,
                   OptIntDefaultConstExprOption<"latency", -1, "", Deprecated>,
                   EnumPresentOption<"memory_style", "auto", 1, "">,
                   EnumPresentOption<"memory_style", "distribute", 1, "">,
                   EnumPresentOption<"memory_style", "block", 1, "">,
                   EnumPresentOption<"memory_style", "uram", 1, "">,
                   OptEnumExprOption<"memory_style", ["auto", "distribute", "block", "uram"], "", Deprecated>, // ETP: New
                   OptIDExprOption<"metadata", "", "", Deprecated>];
}

// #pragma HLS shared variable=<variable>
def XlxSharedPragma : HLSPragma {
    let Visibility = Deprecated;
    let Name = "shared";
    let Desc = "Specifies that a global variable, or function argument array is shared among multiple dataflow processes, without the need for synchronization."; 
    let Objects = [Parameter, Array, Function] ; 
    let Options = [ReqVariableExprOption<"variable", "Replace set_directive_shared with 'set_directive_stream type=shared'", Deprecated>];
}

// #pragma HLS stable variable=<variable>
def XlxStablePragma : HLSPragma {
    let Name = "stable";
    let Desc = "Indicates that a variable input or output of a dataflow region can be ignored when generating the synchronizations at entry and exit of the dataflow region."; 
    let Objects = [Parameter, Array, Function] ; 
    let Options = [ReqVariableExprOption<"variable", "">];
}

// #pragma HLS stable_content variable=<variable>
def XlxStableContentPragma : HLSPragma {
    let Visibility = Hidden;
    let Name = "stable_content";
    let Desc = ""; 
    let Objects = [Parameter, Array, Function] ; 
    let Options = [ReqVariableExprOption<"variable", "">];
}

// #pragma HLS stream variable=<variable> [depth=<int>] [dim=<int>] [off] [type={fifo | pipo | shared | unsync}]
def XlxStreamPragma : HLSPragma {
    let Name = "stream";
    let Desc = "Specifies that a specific array is to be implemented as a FIFO or RAM memory channel during dataflow optimization."; 
    let Objects = [Parameter, Array, Function] ; 
    let Options = [ReqVariableExprOption<"variable", "">,
                   OptIntDefaultConstExprOption<"depth", 0, "This option can be used to specified the RTL FIFO depth of a stream object of array type or hls::stream type.">,
                   OptIntDefaultConstExprOption<"dim", 0, "dimension", Deprecated>,
                   PresentOption<"off", 1, "Replace 'off' with 'type=pipo'", Deprecated>,
                   OptEnumExprOption<"type", ["fifo", "pipo", "shared", "unsync"], "Specify type of fifo or pipo">];
}

// #pragma HLS top name=<string>
def XlxTopPragma : HLSPragma {
    let Name = "top";
    let Desc = "This directive attaches a name to a function, which can then be used with the set_top command."; 
    let Objects = [Function] ; 
    let Options = [OptIDExprOption<"name", "", "Specify the name to be used by the set_top command.">];
}

// #pragma HLS unroll [factor=<int>] [region | partial | complete] [skip_exit_check] [off[=<false|true|0|1>]]
def XlxUnrollPragma : HLSPragma {
    let Name = "unroll";
    let Desc = "Transform loops to create multiple instances of the loop body and its instructions that can then be scheduled independently"; 
    let Objects = [Region] ; 
    let Options = [OptIntConstExprOption<"factor", "Non-zero integer indicating that partial unrolling is requested. The loop body will be repeated this number of times, and the iteration information will be adjusted accordingly.">,
                   EnumPresentOption<"type", "region", 1, "", Hidden>,
                   EnumPresentOption<"type", "complete", 1, "", Hidden>,
                   EnumPresentOption<"type", "partial", 1, "", Hidden>, 
                   OptEnumExprOption<"type", ["partial", "complete", "region"], "", Hidden>, // ETP: New
                   PresentOption<"skip_exit_check", 2, "For partial unrolling, remove exit check logic if possible">,
                   PresentOption<"off", 3, "">
                   ];
}


// #pragma HLS interface [ap_none | ap_fifo | ap_hs | ap_ack | ap_vld | ap_ovld | ap_stable | ap_bus | 
//                        ap_ctrl_none | ap_ctrl_hs | ap_ctrl_chain | m_axi | axis | s_axilite | ap_memory | bram] 
//                        port=<name> bundle=<string> \
// register register_mode=<mode> depth=<int> offset=<string> latency=<value>\
// clock=<string> name=<string> storage_type=<value>\
// num_read_outstanding=<int> num_write_outstanding=<int> \
// max_read_burst_length=<int> max_write_burst_length=<int>

def XlxMAXIInterfacePragma : HLSPragma { 
  let Name = "maxi_interface";
  let Desc = "maxi interface"; 
  let Objects = [Parameter, Function] ; 
  let Options = [
    OptIDExprOption<"bundle", "0", "Specify a name for an AXI interface. Used to group multiple arguments into the same interface bundle.">,
    OptIntDefaultConstExprOption<"depth", 0, "Specifies the maximum number of samples for the test bench to process. This is required for pointer interfaces using ap_fifo mode.">, 
    OptIDExprOption<"offset",  "", "For m_axi and s_axilite, specify how the the address offset will be implemented">,
    OptIntDefaultConstExprOption<"num_read_outstanding", 16, "For m_axi, specify how many read requests can be made to the bus, without a response, before the design stalls"> ,
    OptIntDefaultConstExprOption<"num_write_outstanding", 16, "For m_axi, specify how many write requests can be made to the bus, without a response, before the design stalls">, 
    OptIntDefaultConstExprOption<"max_read_burst_length", 16, "For m_axi, specify the maximum number of data values read during a burst transfer.">, 
    OptIntDefaultConstExprOption<"max_write_burst_length", 16, "For m_axi, specify the maximum number of data values written during a burst transfer.">,
    OptIntDefaultConstExprOption<"latency" , 0, "Specify the latency for ap_memory and m_axi interfaces">, 
    OptIntDefaultConstExprOption<"max_widen_bitwidth", 0, "For m_axi, specify the maximum burst bitwidth that Vitis HLS will use when automatically widening (must be a power-of-two).">,
    OptIntConstExprOption<"channel", "For multi-channel m_axi, specify the channel ID.">
  ];
}

def XlxAXISInterfacePragma: HLSPragma { 
  let Name = "axis_interface"; 
  let Desc = "axis interface"; 
  let Objects = [Parameter, Function] ; 

  let Options = [ 
    PresentOption<"register", 1, "Registers the signal and any relevant protocol signals and instructs the signals to persist until at least the last cycle of the function execution">,
    OptEnumExprOption<"register_mode", ["forward", "reverse", "off", "both"], "For axis, specifies if port registers are placed on the forward path (TDATA and TVALID), the reverse path (TREADY), both forward and reverse, or off (no port registers).">, 
    EnumPresentOption<"register_mode", "forward", 2, "">, 
    EnumPresentOption<"register_mode", "reverse", 2, "">,
    EnumPresentOption<"register_mode", "both", 2, "">,
    EnumPresentOption<"register_mode", "off", 2, "">,
    OptIntConstExprOption<"depth", "Specifies the maximum number of samples for the test bench to process. This is required for pointer interfaces using ap_fifo mode.">
  ];

}

def XlxSAXILiteInterfacePragma: HLSPragma { 
  let Name = "s_axilite"; 
  let Desc = "s_axilite interface pragma"; 
  let Objects = [Parameter, Function] ; 

  let Options = [ 
  OptIDExprOption<"bundle", "0", "Specify a name for an AXI interface. Used to group multiple arguments into the same interface bundle.">, 
  OptIntDefaultConstExprOption<"offset", 0, "For m_axi and s_axilite, specify how the the address offset will be implemented">, 
  OptIDExprOption<"clock", "", "Specify a separate clock for an s_axilite interface">, 
  OptIDExprOption<"storage_impl", "", "For s_axilite, specify the storage implementation to use.">
  ];
}

def XlxScalarInterfacePragma: HLSPragma { 
  let Name = "ap_scalar"; 
  let Desc = "scalar interface pragmas"; 
  let Objects = [Parameter, Function] ; 
  let Options = [ 
    PresentOption<"register", 1, "Registers the signal and any relevant protocol signals and instructs the signals to persist until at least the last cycle of the function execution">, 
    OptIntDefaultConstExprOption<"depth", 0, "Specifies the maximum number of samples for the test bench to process. This is required for pointer interfaces using ap_fifo mode.">, 
    OptIntDefaultConstExprOption<"latency", 0, "Specify the latency for ap_memory and m_axi interfaces">,
    OptEnumExprOption<"direct_io", ["true", "false"], "Specify DirectIO property for scalar access", Hidden>
  ];
}

def XlxInterruptInterfacePragma: HLSPragma { 
  let Name = "ap_stable"; 
  let Desc = "ap_stable interface"; 
  let Objects = [Parameter, Function] ; 
  let Options = [ 
    PresentOption<"register", 1, "Registers the signal and any relevant protocol signals and instructs the signals to persist until at least the last cycle of the function execution">, 
    OptIntDefaultConstExprOption<"depth", 0, "Specifies the maximum number of samples for the test bench to process. This is required for pointer interfaces using ap_fifo mode.">, 
    OptIntDefaultConstExprOption<"latency", 0, "Specify the latency for ap_memory and m_axi interfaces">, 
    OptIntDefaultConstExprOption<"interrupt", 0, "Only used by ap_vld/ap_hs. This option enables the I/O to be managed in interrupt, by creating the corresponding bits in the ISR and IER SAXILite file. Value N=16..31 specifies the bit position in both registers (by default assigned contiguously from 16)">,
    OptEnumExprOption<"direct_io", ["true", "false"], "Specify DirectIO property for scalar access", Hidden>
  ];

}

def XlxBRAMInterfacePragma : HLSPragma { 
  let Name = "memory"; 
  let Desc = "bram/ap_memory interface"; 
  let Objects = [Parameter, Function] ; 

  let Options = [ 
  OptIntDefaultConstExprOption<"depth", 1, "Specifies the maximum number of samples for the test bench to process. This is required for pointer interfaces using ap_fifo mode.">,
  OptIntDefaultConstExprOption<"latency", -1, "Specify the latency for ap_memory and m_axi interfaces">, 
  OptIDExprOption<"storage_type", "default", "For bram and ap_memory, specify the type of storage to use.">
  ];
}

def XlxCallConventionInterfacePragma: HLSPragma { 
  let Name = "ap_stable"; 
  let Desc = "ap_stable for call convertion";
  let Objects = [Parameter, Function] ; 
  let Options = [ ];
}


def XlxInterfacePragma: HLSPragma, ModesFormat { 
  let Name = "interface" ; 
  let Desc = "Specifies how RTL ports are created from the function description."; 
  let Objects = [Parameter, Function] ; 
  let Options = [ 
    ReqVariableExprOption<"port", "">,
    OptIDExprOption<"name", "", "Specify name of interface in RTL">,

    EnumPresentOption<"mode", "m_axi", 6, "">, 
    EnumPresentOption<"mode", "s_axilite", 6, "">, 
    EnumPresentOption<"mode", "axis", 6, "">, 

    EnumPresentOption<"mode", "ap_memory", 6, "">, 
    EnumPresentOption<"mode", "bram", 6, "">, 

    EnumPresentOption<"mode", "ap_none", 6, "">, 
    EnumPresentOption<"mode", "ap_fifo", 6, "">, 
    EnumPresentOption<"mode", "ap_hs", 6, "">, 
    EnumPresentOption<"mode", "ap_ack", 6, "">, 
    EnumPresentOption<"mode", "ap_vld", 6, "">, 
    EnumPresentOption<"mode", "ap_ovld", 6, "">, 
    EnumPresentOption<"mode", "ap_stable", 6, "">, 
    EnumPresentOption<"mode", "ap_bus", 6, "", Deprecated>, 

    EnumPresentOption<"mode", "ap_ctrl_none", 6, "">, 
    EnumPresentOption<"mode", "ap_ctrl_hs", 6, "">, 
    EnumPresentOption<"mode", "ap_ctrl_chain", 6, "">, 
    OptEnumExprOption<"mode", ["ap_none", "ap_vld", "ap_ack", "ap_hs", "ap_ovld", "ap_fifo", "ap_memory", "bram", "axis", "s_axilite", "m_axi", "ap_ctrl_none", "ap_ctrl_hs", "ap_ctrl_chain", "ap_stable"],
      "Specify the interface I/O protocol" >
    ];

    let ModeOptionName = "mode";
    let Modes = (modeSelect 
                XlxMAXIInterfacePragma : $m_axi, 

                XlxAXISInterfacePragma : $axis, 

                XlxSAXILiteInterfacePragma : $s_axilite,

                XlxBRAMInterfacePragma : $bram, 
                XlxBRAMInterfacePragma : $ap_memory, 

                XlxScalarInterfacePragma : $ap_none, 
                XlxScalarInterfacePragma : $ap_fifo, 
                XlxScalarInterfacePragma : $ap_ack, 
                XlxScalarInterfacePragma : $ap_vld, 
                XlxScalarInterfacePragma : $ap_stable, 
                XlxScalarInterfacePragma : $ap_bus, 

                XlxInterruptInterfacePragma: $ap_hs, 
                XlxInterruptInterfacePragma: $ap_ovld,

                XlxCallConventionInterfacePragma : $ap_ctrl_none, 
                XlxCallConventionInterfacePragma : $ap_ctrl_hs, 
                XlxCallConventionInterfacePragma : $ap_ctrl_chain
                );


}

// #pragma HLS cache port=<name>
// lines=<int> size=<int> ways=<int> users=<int> \
// burst=<on|adaptive|off> write_mode=<write_back|write_through>

def XlxCachePragma : HLSPragma { 
  let Name = "cache";
  let Desc = "cache";
  let Objects = [Parameter, Function];
  let Options = [
    ReqVariableExprOption<"port", "Specify the interface for which cache is used.">,
    OptIntDefaultConstExprOption<"lines", 1, "Number of cache lines">,
    OptIntDefaultConstExprOption<"depth", 0, "Line size in words">,
//  OptIntDefaultConstExprOption<"ways", 1, "cache ways", Hidden>,
//  OptIntDefaultConstExprOption<"users", 1, "Number of port users", Hidden>,
//  OptEnumExprOption<"burst", ["off", "on"/*, "adaptive"*/], "HLS burst mode", Hidden>,
    OptEnumExprOption<"write_mode", ["write_back", "write_through"], "Write mode", Hidden>
  ];
}
