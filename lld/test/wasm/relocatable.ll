; RUN: llc -filetype=obj -mtriple=wasm32-unknown-unknown-wasm %p/Inputs/hello.ll -o %t.hello.o
; RUN: llc -filetype=obj -mtriple=wasm32-unknown-unknown-wasm %s -o %t.o
; RUN: lld -flavor wasm -r -o %t.wasm %t.hello.o %t.o
; RUN: obj2yaml %t.wasm | FileCheck %s

; Function Attrs: nounwind
define hidden i32 @my_func() local_unnamed_addr {
entry:
  %call = tail call i32 @foo_import()
  %call2 = tail call i32 @bar_import()
  ret i32 1
}

declare i32 @foo_import() local_unnamed_addr
declare extern_weak i32 @bar_import() local_unnamed_addr
@data_import = external global i64

@func_addr1 = hidden global i32()* @my_func, align 4
@func_addr2 = hidden global i32()* @foo_import, align 4
@func_addr3 = hidden global i32()* @bar_import, align 4
@data_addr1 = hidden global i64* @data_import, align 8

$func_comdat = comdat any
@data_comdat = weak_odr constant [3 x i8] c"abc", comdat($func_comdat)
define linkonce_odr i32 @func_comdat() comdat {
entry:
  ret i32 ptrtoint ([3 x i8]* @data_comdat to i32)
}

; CHECK:      --- !WASM
; CHECK-NEXT: FileHeader:
; CHECK-NEXT:   Version:         0x00000001
; CHECK-NEXT: Sections:
; CHECK-NEXT:   - Type:            TYPE
; CHECK-NEXT:     Signatures:
; CHECK-NEXT:       - Index:           0
; CHECK-NEXT:         ReturnType:      NORESULT
; CHECK-NEXT:         ParamTypes:
; CHECK-NEXT:       - Index:           1
; CHECK-NEXT:         ReturnType:      NORESULT
; CHECK-NEXT:         ParamTypes:
; CHECK-NEXT:           - I32
; CHECK-NEXT:       - Index:           2
; CHECK-NEXT:         ReturnType:      I32
; CHECK-NEXT:         ParamTypes:
; CHECK-NEXT:   - Type:            IMPORT
; CHECK-NEXT:     Imports:
; CHECK-NEXT:       - Module:          env
; CHECK-NEXT:         Field:           puts
; CHECK-NEXT:         Kind:            FUNCTION
; CHECK-NEXT:         SigIndex:        1
; CHECK-NEXT:       - Module:          env
; CHECK-NEXT:         Field:           foo_import
; CHECK-NEXT:         Kind:            FUNCTION
; CHECK-NEXT:         SigIndex:        2
; CHECK-NEXT:       - Module:          env
; CHECK-NEXT:         Field:           bar_import
; CHECK-NEXT:         Kind:            FUNCTION
; CHECK-NEXT:         SigIndex:        2
; CHECK-NEXT:       - Module:          env
; CHECK-NEXT:         Field:           data_import
; CHECK-NEXT:         Kind:            GLOBAL
; CHECK-NEXT:         GlobalType:      I32
; CHECK-NEXT:         GlobalMutable:   false
; CHECK-NEXT:   - Type:            FUNCTION
; CHECK-NEXT:     FunctionTypes:   [ 0, 2, 2 ]
; CHECK-NEXT:   - Type:            TABLE
; CHECK-NEXT:     Tables:
; CHECK-NEXT:       - ElemType:        ANYFUNC
; CHECK-NEXT:         Limits:
; CHECK-NEXT:           Flags:           [ HAS_MAX ]
; CHECK-NEXT:           Initial:         0x00000003
; CHECK-NEXT:           Maximum:         0x00000003
; CHECK-NEXT:   - Type:            MEMORY
; CHECK-NEXT:     Memories:
; CHECK-NEXT:       - Initial:         0x00000001
; CHECK-NEXT:   - Type:            GLOBAL
; CHECK-NEXT:     Globals:
; CHECK-NEXT:       - Index:           1
; CHECK-NEXT:         Type:            I32
; CHECK-NEXT:         Mutable:         false
; CHECK-NEXT:         InitExpr:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           0
; CHECK-NEXT:       - Index:           2
; CHECK-NEXT:         Type:            I32
; CHECK-NEXT:         Mutable:         false
; CHECK-NEXT:         InitExpr:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           28
; CHECK-NEXT:       - Index:           3
; CHECK-NEXT:         Type:            I32
; CHECK-NEXT:         Mutable:         false
; CHECK-NEXT:         InitExpr:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           8
; CHECK-NEXT:       - Index:           4
; CHECK-NEXT:         Type:            I32
; CHECK-NEXT:         Mutable:         false
; CHECK-NEXT:         InitExpr:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           12
; CHECK-NEXT:       - Index:           5
; CHECK-NEXT:         Type:            I32
; CHECK-NEXT:         Mutable:         false
; CHECK-NEXT:         InitExpr:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           16
; CHECK-NEXT:       - Index:           6
; CHECK-NEXT:         Type:            I32
; CHECK-NEXT:         Mutable:         false
; CHECK-NEXT:         InitExpr:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           24
; CHECK-NEXT:   - Type:            EXPORT
; CHECK-NEXT:     Exports:
; CHECK-NEXT:       - Name:            hello
; CHECK-NEXT:         Kind:            FUNCTION
; CHECK-NEXT:         Index:           3
; CHECK-NEXT:       - Name:            my_func
; CHECK-NEXT:         Kind:            FUNCTION
; CHECK-NEXT:         Index:           4
; CHECK-NEXT:       - Name:            func_comdat
; CHECK-NEXT:         Kind:            FUNCTION
; CHECK-NEXT:         Index:           5
; CHECK-NEXT:       - Name:            hello_str
; CHECK-NEXT:         Kind:            GLOBAL
; CHECK-NEXT:         Index:           1
; CHECK-NEXT:       - Name:            data_comdat
; CHECK-NEXT:         Kind:            GLOBAL
; CHECK-NEXT:         Index:           2
; CHECK-NEXT:       - Name:            func_addr1
; CHECK-NEXT:         Kind:            GLOBAL
; CHECK-NEXT:         Index:           3
; CHECK-NEXT:       - Name:            func_addr2
; CHECK-NEXT:         Kind:            GLOBAL
; CHECK-NEXT:         Index:           4
; CHECK-NEXT:       - Name:            func_addr3
; CHECK-NEXT:         Kind:            GLOBAL
; CHECK-NEXT:         Index:           5
; CHECK-NEXT:       - Name:            data_addr1
; CHECK-NEXT:         Kind:            GLOBAL
; CHECK-NEXT:         Index:           6
; CHECK-NEXT:   - Type:            ELEM
; CHECK-NEXT:     Segments:
; CHECK-NEXT:       - Offset:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           0
; CHECK-NEXT:         Functions:       [ 4, 1, 2 ]
; CHECK-NEXT:   - Type:            CODE
; CHECK-NEXT:     Relocations:
; CHECK-NEXT:       - Type:            R_WEBASSEMBLY_MEMORY_ADDR_SLEB
; CHECK-NEXT:         Index:           1
; CHECK-NEXT:         Offset:          0x00000004
; CHECK-NEXT:       - Type:            R_WEBASSEMBLY_FUNCTION_INDEX_LEB
; CHECK-NEXT:         Index:           0
; CHECK-NEXT:         Offset:          0x0000000A
; CHECK-NEXT:       - Type:            R_WEBASSEMBLY_FUNCTION_INDEX_LEB
; CHECK-NEXT:         Index:           1
; CHECK-NEXT:         Offset:          0x00000013
; CHECK-NEXT:       - Type:            R_WEBASSEMBLY_FUNCTION_INDEX_LEB
; CHECK-NEXT:         Index:           2
; CHECK-NEXT:         Offset:          0x0000001A
; CHECK-NEXT:       - Type:            R_WEBASSEMBLY_MEMORY_ADDR_SLEB
; CHECK-NEXT:         Index:           2
; CHECK-NEXT:         Offset:          0x00000026
; CHECK-NEXT:     Functions:
; CHECK-NEXT:       - Index:         3
; CHECK-NEXT:         Locals:
; CHECK-NEXT:         Body:          4180808080001080808080000B
; CHECK-NEXT:       - Index:         4
; CHECK-NEXT:         Locals:
; CHECK-NEXT:         Body:          1081808080001A1082808080001A41010B
; CHECK-NEXT:       - Index:         5
; CHECK-NEXT:         Locals:
; CHECK-NEXT:         Body:          419C808080000B
; CHECK-NEXT:   - Type:            DATA
; CHECK-NEXT:     Relocations:
; CHECK-NEXT:       - Type:            R_WEBASSEMBLY_TABLE_INDEX_I32
; CHECK-NEXT:         Index:           4
; CHECK-NEXT:         Offset:          0x00000012
; CHECK-NEXT:       - Type:            R_WEBASSEMBLY_TABLE_INDEX_I32
; CHECK-NEXT:         Index:           1
; CHECK-NEXT:         Offset:          0x0000001B
; CHECK-NEXT:       - Type:            R_WEBASSEMBLY_TABLE_INDEX_I32
; CHECK-NEXT:         Index:           2
; CHECK-NEXT:         Offset:          0x00000024
; CHECK-NEXT:       - Type:            R_WEBASSEMBLY_MEMORY_ADDR_I32
; CHECK-NEXT:         Index:           0
; CHECK-NEXT:         Offset:          0x0000002D
; CHECK-NEXT:     Segments:
; CHECK-NEXT:       - SectionOffset:   6
; CHECK-NEXT:         MemoryIndex:     0
; CHECK-NEXT:         Offset:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           0
; CHECK-NEXT:         Content:         68656C6C6F0A00
; CHECK-NEXT:       - SectionOffset:   18
; CHECK-NEXT:         MemoryIndex:     0
; CHECK-NEXT:         Offset:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           8
; CHECK-NEXT:         Content:         '00000000'
; CHECK-NEXT:       - SectionOffset:   27
; CHECK-NEXT:         MemoryIndex:     0
; CHECK-NEXT:         Offset:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           12
; CHECK-NEXT:         Content:         '01000000'
; CHECK-NEXT:       - SectionOffset:   36
; CHECK-NEXT:         MemoryIndex:     0
; CHECK-NEXT:         Offset:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           16
; CHECK-NEXT:         Content:         '02000000'
; CHECK-NEXT:       - SectionOffset:   45
; CHECK-NEXT:         MemoryIndex:     0
; CHECK-NEXT:         Offset:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           24
; CHECK-NEXT:         Content:         '00000000'
; CHECK-NEXT:       - SectionOffset:   54
; CHECK-NEXT:         MemoryIndex:     0
; CHECK-NEXT:         Offset:
; CHECK-NEXT:           Opcode:          I32_CONST
; CHECK-NEXT:           Value:           28
; CHECK-NEXT:         Content:         '616263'
; CHECK-NEXT:   - Type:            CUSTOM
; CHECK-NEXT:     Name:            linking
; CHECK-NEXT:     DataSize:        31
; CHECK-NEXT:     SymbolInfo:
; CHECK-NEXT:       - Name:            bar_import
; CHECK-NEXT:         Flags:           [ BINDING_WEAK ]
; CHECK-NEXT:       - Name:            hello
; CHECK-NEXT:         Flags:           [ VISIBILITY_HIDDEN ]
; CHECK-NEXT:       - Name:            my_func
; CHECK-NEXT:         Flags:           [ VISIBILITY_HIDDEN ]
; CHECK-NEXT:       - Name:            func_comdat
; CHECK-NEXT:         Flags:           [ BINDING_WEAK ]
; CHECK-NEXT:       - Name:            data_comdat
; CHECK-NEXT:         Flags:           [ BINDING_WEAK ]
; CHECK-NEXT:       - Name:            func_addr1
; CHECK-NEXT:         Flags:           [ VISIBILITY_HIDDEN ]
; CHECK-NEXT:       - Name:            func_addr2
; CHECK-NEXT:         Flags:           [ VISIBILITY_HIDDEN ]
; CHECK-NEXT:       - Name:            func_addr3
; CHECK-NEXT:         Flags:           [ VISIBILITY_HIDDEN ]
; CHECK-NEXT:       - Name:            data_addr1
; CHECK-NEXT:         Flags:           [ VISIBILITY_HIDDEN ]
; CHECK-NEXT:     SegmentInfo:
; CHECK-NEXT:       - Index:           0
; CHECK-NEXT:         Name:            .rodata.hello_str
; CHECK-NEXT:         Alignment:       1
; CHECK-NEXT:         Flags:           [  ]
; CHECK-NEXT:       - Index:           1
; CHECK-NEXT:         Name:            .data.func_addr1
; CHECK-NEXT:         Alignment:       4
; CHECK-NEXT:         Flags:           [  ]
; CHECK-NEXT:       - Index:           2
; CHECK-NEXT:         Name:            .data.func_addr2
; CHECK-NEXT:         Alignment:       4
; CHECK-NEXT:         Flags:           [  ]
; CHECK-NEXT:       - Index:           3
; CHECK-NEXT:         Name:            .data.func_addr3
; CHECK-NEXT:         Alignment:       4
; CHECK-NEXT:         Flags:           [  ]
; CHECK-NEXT:       - Index:           4
; CHECK-NEXT:         Name:            .data.data_addr1
; CHECK-NEXT:         Alignment:       8
; CHECK-NEXT:         Flags:           [  ]
; CHECK-NEXT:       - Index:           5
; CHECK-NEXT:         Name:            .rodata.data_comdat
; CHECK-NEXT:         Alignment:       1
; CHECK-NEXT:         Flags:           [  ]
; CHECK-NEXT:     Comdats:
; CHECK-NEXT:       - Name:            func_comdat
; CHECK-NEXT:         Entries:
; CHECK-NEXT:           - Kind:            FUNCTION
; CHECK-NEXT:             Index:           5
; CHECK-NEXT:           - Kind:            DATA
; CHECK-NEXT:             Index:           5
; CHECK-NEXT:   - Type:            CUSTOM
; CHECK-NEXT:     Name:            name
; CHECK-NEXT:     FunctionNames:
; CHECK-NEXT:       - Index:           0
; CHECK-NEXT:         Name:            puts
; CHECK-NEXT:       - Index:           1
; CHECK-NEXT:         Name:            foo_import
; CHECK-NEXT:       - Index:           2
; CHECK-NEXT:         Name:            bar_import
; CHECK-NEXT:       - Index:           3
; CHECK-NEXT:         Name:            hello
; CHECK-NEXT:       - Index:           4
; CHECK-NEXT:         Name:            my_func
; CHECK-NEXT:       - Index:           5
; CHECK-NEXT:         Name:            func_comdat
; CHECK-NEXT: ...
