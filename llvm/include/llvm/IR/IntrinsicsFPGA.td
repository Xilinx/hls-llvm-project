// (C) Copyright 2016-2022 Xilinx, Inc.
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
// This file defines all of the FPGA-specific intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "fpga" in {
//===-------------------- Bit Manipulation Intrinsics ---------------------===//
//

// None of these intrinsics accesses memory at all.
let IntrProperties = [IntrNoMem] in {
// iN fpga.smod(iN %lhs, iN %rhs)
//
// Computes the mathematical modulo:
//   smod iN %lhs, %rhs                 when %lhs is non-negative
//   add iN (smod iN %lhs, %rhs), %rhs  when %lhs is negative
//
// This corresponds to the usual modulo:
// - the result is always non-negative
// - the result is always contained between 0 (incl.) and %rhs (excl.)
// - %lhs - fpga.smod(%lhs, %rhs) is divisible by %rhs
def int_fpga_smod : Intrinsic<[llvm_anyint_ty], [LLVMMatchType<0>, LLVMMatchType<0>]>;

// iP fpga.part.select(iN %src, iK %off) - Select src[off+P-1:off]
def int_fpga_part_select : Intrinsic<[llvm_anyint_ty],
                                     [llvm_anyint_ty, llvm_anyint_ty]>;

// iN fpga.part.set(iN %src, iP part, iK %off) - Inject part to src[off+P-1:off]
def int_fpga_part_set : Intrinsic<[llvm_anyint_ty],
                                  [LLVMMatchType<0>, llvm_anyint_ty,
                                   llvm_anyint_ty]>;


// fpga.legacy.part.select(V, Lo, Hi) - Select V[Hi:Lo]
// NOTE: reverse mode is supported.
def int_fpga_legacy_part_select : Intrinsic<[llvm_anyint_ty],
                                            [LLVMMatchType<0>, llvm_i32_ty,
                                             llvm_i32_ty]>;

// fpga.legacy.part.set(V, R, Lo, Hi) - Replace V[Hi:Lo] to R
// NOTE: reverse mode is supported.
def int_fpga_legacy_part_set : Intrinsic<[llvm_anyint_ty],
                                         [LLVMMatchType<0>, llvm_anyint_ty,
                                          llvm_i32_ty, llvm_i32_ty]>;

// fpga.bit.concat(V0, V1, V2, ...) - { V0, V1, V2 } in verilog
def int_fpga_bit_concat : Intrinsic<[llvm_anyint_ty], [llvm_vararg_ty]>;

// Unpack bytes to struct/array, based on 'aggregate -compact byte' definition
def int_fpga_unpack_bytes : Intrinsic<[llvm_any_ty], [llvm_anyint_ty]>;

// Pack struct/array to bytes, based on 'aggregate -compact byte' definition
def int_fpga_pack_bytes : Intrinsic<[llvm_anyint_ty], [llvm_any_ty]>;

// Unpack bits to struct/array, based on 'aggregate -compact bit' definition
def int_fpga_unpack_bits : Intrinsic<[llvm_any_ty], [llvm_anyint_ty]>;

// Pack struct/array to bits, based on 'aggregate -compact bit' definition
def int_fpga_pack_bits : Intrinsic<[llvm_anyint_ty], [llvm_any_ty]>;

// Unpack bytes to struct/array, based on the standard data layout
def int_fpga_unpack_none: Intrinsic<[llvm_any_ty], [llvm_anyint_ty]>;

// Pack struct/array to bytes, based on the standard data layout
def int_fpga_pack_none: Intrinsic<[llvm_anyint_ty], [llvm_any_ty]>;

// Multiplexer (generalize the select)
// a_k = mux(k, a_0, a_1, a_2, ..., a_N)
def int_fpga_mux : Intrinsic<[llvm_any_ty], [llvm_anyint_ty, llvm_vararg_ty]>;
}

// fpga.gmem.load(Ptr, volatile)
def int_fpga_gmem_load : Intrinsic<[llvm_any_ty],
                                   [LLVMAnyPointerType<LLVMMatchType<0>>,
                                    llvm_i1_ty],
                                   [IntrReadMem, IntrArgMemOnly]>;
def int_fpga_maxi_load : Intrinsic<[llvm_any_ty],
                                   [LLVMAnyPointerType<LLVMMatchType<0>>,
                                    llvm_i1_ty],
                                   [IntrReadMem, IntrArgMemOnly]>;
def int_fpga_bram_load : Intrinsic<[llvm_any_ty],
                                   [LLVMAnyPointerType<LLVMMatchType<0>>,
                                    llvm_i1_ty],
                                   [IntrReadMem, IntrArgMemOnly]>;

// fpga.gmem.store(Data, Ptr, byteenable, volatile)
def int_fpga_gmem_store : Intrinsic<[], [llvm_any_ty,
                                         LLVMAnyPointerType<LLVMMatchType<0>>,
                                         llvm_anyint_ty, llvm_i1_ty],
                                    [IntrWriteMem, IntrArgMemOnly]>;
def int_fpga_maxi_store : Intrinsic<[], [llvm_any_ty,
                                         LLVMAnyPointerType<LLVMMatchType<0>>,
                                         llvm_anyint_ty, llvm_i1_ty],
                                    [IntrWriteMem, IntrArgMemOnly]>;
def int_fpga_bram_store : Intrinsic<[], [llvm_any_ty,
                                         LLVMAnyPointerType<LLVMMatchType<0>>,
                                         llvm_anyint_ty, llvm_i1_ty],
                                    [IntrWriteMem, IntrArgMemOnly]>;

// preserve-properties-per-object (prevent passes from changing the number of
// related operations or changing their order of execution relatively on the
// pointer itself
// fpga.pppo.load(Ptr)
def int_fpga_pppo_load : Intrinsic<[llvm_any_ty],
                                   [LLVMAnyPointerType<LLVMMatchType<0>>],
                                   [IntrArgMemOnly]>;
// fpga.pppo.store(Data, Ptr)
def int_fpga_pppo_store : Intrinsic<[], [llvm_any_ty,
                                         LLVMAnyPointerType<LLVMMatchType<0>>],
                                    [IntrArgMemOnly]>;

//===---
//
//  FIFO Intrinsics
//
//  ...
//
//===---

// i1 fpga.fifo.not.empty(%type *%fifo)
def int_fpga_fifo_not_empty : Intrinsic<[llvm_i1_ty], [LLVMAnyPointerType<llvm_any_ty>],
                                        [IntrArgMemOnly, NoCapture<0>]>;

// i1 fpga.fifo.not.full(%type *%fifo)
def int_fpga_fifo_not_full : Intrinsic<[llvm_i1_ty], [LLVMAnyPointerType<llvm_any_ty>],
                                       [IntrArgMemOnly, NoCapture<0>]>;

// %type fpga.fifo.pop(%type *%fifo)
def int_fpga_fifo_pop : Intrinsic<[llvm_any_ty], [LLVMAnyPointerType<LLVMMatchType<0>>],
                                  [IntrArgMemOnly, NoCapture<0>]>;

// void fpga.fifo.push(%type %val, %type *%fifo)
def int_fpga_fifo_push : Intrinsic<[], [llvm_any_ty, LLVMAnyPointerType<LLVMMatchType<0>>],
                                   [IntrArgMemOnly, NoCapture<1>]>;

// {i1, %type} fpga.fifo.nb_pop(%type *%fifo)
def int_fpga_fifo_nb_pop : Intrinsic<[llvm_i1_ty, llvm_any_ty], [LLVMAnyPointerType<LLVMMatchType<0>>],
                                     [IntrArgMemOnly, NoCapture<0>]>;

// i1 fpga.fifo.nb_push(%type %val, %type *%fifo)
def int_fpga_fifo_nb_push : Intrinsic<[llvm_i1_ty], [llvm_any_ty, LLVMAnyPointerType<LLVMMatchType<0>>],
                                      [IntrArgMemOnly, NoCapture<1>]>;

// i32 fpga.fifo.size(%type *%fifo)
def int_fpga_fifo_size: Intrinsic<[llvm_i32_ty], [LLVMAnyPointerType<llvm_any_ty>],
                                  [IntrArgMemOnly, NoCapture<0>]>;

// i32 fpga.fifo.capacity(%type *%fifo)
def int_fpga_fifo_capacity: Intrinsic<[llvm_i32_ty], [LLVMAnyPointerType<llvm_any_ty>],
                                      [IntrArgMemOnly, NoCapture<0>]>;

//===---
//
// Shift register builtins
//
// Note that the underlying object is expected to be an array.
// The size of that array represents the depth of the shift register.
//
// void fpga.shift.register.shift(%data_in, %shift_reg, %pred)
//
// If %pred is true, shifts by one data the shift register pointed to by
// %shift_reg then writes %data_in into the new empty slot, at index 0.
// Shifting moves the data from index i to index i+1 and discards the last.
//
// If %pred is false, does nothing.
//
def int_fpga_shift_register_shift :
    Intrinsic<[], [llvm_any_ty, LLVMAnyPointerType<LLVMMatchType<0>>, llvm_i1_ty],
              [IntrArgMemOnly, NoCapture<1>]>,
    GCCBuiltin<"__fpga_shift_register_shift">;
//
// %data = fpga.shift.register.peek(%shift_reg, %index)
//
// Returns the data at index %index of the shift register pointed to by
// %shift_reg.
//
// note: this does not shift nor modify the shift register.
//
def int_fpga_shift_register_peek :
    Intrinsic<[llvm_any_ty], [LLVMAnyPointerType<LLVMMatchType<0>>, llvm_anyint_ty],
              [IntrReadMem, IntrArgMemOnly, NoCapture<0>]>,
    GCCBuiltin<"__fpga_shift_register_peek">;


//===---
//
//  Stream-of-Blocks Intrinsics
//
//  Note that the underlying object is expected to be an array.
//  The depth of the stream-of-block isn't related to any array dimension.
//  The array passed as argument serves as both: the handle that represents the
//  stream-of-block, and the buffer in which normal load/store can be performed.
//
//  `acquire` take the ownership of the block, which allows to read and write into the buffer.
//  `release` relinquish the ownership of the block, which forbids further access on it until
//  a subsequent acquire is performed.
//
//  `pop` is intended for the consumer side while `push` is intended for the producer side.
//  Semantically, the blocks `pop.acquired` contains the data from blocks `push.relased`.
//  Only one block can be held at a time (as only one buffer is available).
//
// `empty` and `full` are intentionally not marked as read-only. Even though they semantically
// are read-only, we would need them to be considered as atomic because dataflow context are
// a form of multi-process environment in which channels can lead to data-race if their atomic
// behavior isn't accounted for.
//
//===---

// i1 fpga.pipo.not.empty(%type *%pipo)
def int_fpga_pipo_not_empty : Intrinsic<[llvm_i1_ty], [LLVMAnyPointerType<llvm_any_ty>],
                                        [IntrArgMemOnly, NoCapture<0>]>;

// i1 fpga.pipo.not.full(%type *%pipo)
def int_fpga_pipo_not_full : Intrinsic<[llvm_i1_ty], [LLVMAnyPointerType<llvm_any_ty>],
                                       [IntrArgMemOnly, NoCapture<0>]>;

// void fpga.pipo.pop.acquire(%type *%pipo)
def int_fpga_pipo_pop_acquire : Intrinsic<[], [LLVMAnyPointerType<LLVMMatchType<0>>],
                                          [IntrArgMemOnly, NoCapture<0>]>;

// void fpga.pipo.pop.release(%type *%pipo)
def int_fpga_pipo_pop_release : Intrinsic<[], [LLVMAnyPointerType<LLVMMatchType<0>>],
                                          [IntrArgMemOnly, NoCapture<0>]>;

// void fpga.pipo.push.acquire(%type *%pipo)
def int_fpga_pipo_push_acquire : Intrinsic<[], [LLVMAnyPointerType<LLVMMatchType<0>>],
                                           [IntrArgMemOnly, NoCapture<0>]>;

// void fpga.pipo.push.release(%type *%pipo)
def int_fpga_pipo_push_release : Intrinsic<[], [LLVMAnyPointerType<LLVMMatchType<0>>],
                                           [IntrArgMemOnly, NoCapture<0>]>;


//===---
//
// axis side channels related builtins
//
// %b = fpga.axis.ready(%V.data, %V.keep, %V.strb, %V.user, %V.last, %V.id, %V.dest)
// Check data in axis is ready or not.
//
def int_fpga_axis_ready: Intrinsic<[llvm_i1_ty],
    [LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>],
    [IntrArgMemOnly, NoCapture<0>, NoCapture<1>, NoCapture<2>, NoCapture<3>,
     NoCapture<4>, NoCapture<5>, NoCapture<6>]>,
    GCCBuiltin<"__fpga_axis_ready">;

// %b = fpga.axis.valid(%V.data, %V.keep, %V.strb, %V.user, %V.last, %V.id, %V.dest)
// Check data in axis is valid or not.
//
def int_fpga_axis_valid: Intrinsic<[llvm_i1_ty],
    [LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>],
    [IntrArgMemOnly, NoCapture<0>, NoCapture<1>, NoCapture<2>, NoCapture<3>,
     NoCapture<4>, NoCapture<5>, NoCapture<6>]>,
    GCCBuiltin<"__fpga_axis_valid">;

// void fpga.axis.pop(%V.data, %V.keep, %V.strb, %V.user, %V.last, %V.id, %V.dest,
//                    %T.data, %T.keep, %T.strb, %T.user, %T.last, %T.id, %T.dest)
// Read the data in the AXIS side channel 'V', into temporary 'T'.
//
def int_fpga_axis_pop: Intrinsic<[],
    [LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>,
     LLVMMatchType<0>, LLVMMatchType<1>, LLVMMatchType<2>, LLVMMatchType<3>, 
     LLVMMatchType<4>, LLVMMatchType<5>, LLVMMatchType<6>],
    [IntrArgMemOnly, NoCapture<0>, NoCapture<1>, NoCapture<2>, NoCapture<3>,
     NoCapture<4>, NoCapture<5>, NoCapture<6>, NoCapture<7>, WriteOnly<7>,
     NoCapture<8>, WriteOnly<8>, NoCapture<9>, WriteOnly<9>,
     NoCapture<10>, WriteOnly<10>, NoCapture<11>, WriteOnly<11>,
     NoCapture<12>, WriteOnly<12>, NoCapture<13>, WriteOnly<13>]>,
    GCCBuiltin<"__fpga_axis_pop">;

//===---
//
// void fpga.axis.push(%V.data, %V.keep, %V.strb, %V.user, %V.last, %V.id, %V.dest,
//                     %T.data, %T.keep, %T.strb, %T.user, %T.last, %T.id, %T.dest)
// Write the temporary data 'V' to the AXIS side channel 'T'.
//
def int_fpga_axis_push: Intrinsic<[],
    [LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>,
     LLVMMatchType<0>, LLVMMatchType<1>, LLVMMatchType<2>, LLVMMatchType<3>, 
     LLVMMatchType<4>, LLVMMatchType<5>, LLVMMatchType<6>],
    [IntrArgMemOnly, NoCapture<0>, NoCapture<1>, NoCapture<2>, NoCapture<3>,
     NoCapture<4>, NoCapture<5>, NoCapture<6>, NoCapture<7>, ReadOnly<7>,
     NoCapture<8>, ReadOnly<8>, NoCapture<9>, ReadOnly<9>,
     NoCapture<10>, ReadOnly<10>, NoCapture<11>, ReadOnly<11>,
     NoCapture<12>, ReadOnly<12>, NoCapture<13>, ReadOnly<13>]>,
    GCCBuiltin<"__fpga_axis_push">;

// void fpga.axis.nb.pop(%V.data, %V.keep, %V.strb, %V.user, %V.last, %V.id, %V.dest,
//                       %T.data, %T.keep, %T.strb, %T.user, %T.last, %T.id, %T.dest)
// NonBlocking Read the data in the AXIS side channel 'V', into temporary 'T'.
//
def int_fpga_axis_nb_pop: Intrinsic<[llvm_i1_ty],
    [LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>,
     LLVMMatchType<0>, LLVMMatchType<1>, LLVMMatchType<2>, LLVMMatchType<3>, 
     LLVMMatchType<4>, LLVMMatchType<5>, LLVMMatchType<6>],
    [IntrArgMemOnly, NoCapture<0>, NoCapture<1>, NoCapture<2>, NoCapture<3>,
     NoCapture<4>, NoCapture<5>, NoCapture<6>, NoCapture<7>, WriteOnly<7>,
     NoCapture<8>, WriteOnly<8>, NoCapture<9>, WriteOnly<9>,
     NoCapture<10>, WriteOnly<10>, NoCapture<11>, WriteOnly<11>,
     NoCapture<12>, WriteOnly<12>, NoCapture<13>, WriteOnly<13>]>,
    GCCBuiltin<"__fpga_axis_nb_pop">;

//===---
//
// void fpga.axis.nb.push(%V.data, %V.keep, %V.strb, %V.user, %V.last, %V.id, %V.dest,
//                        %T.data, %T.keep, %T.strb, %T.user, %T.last, %T.id, %T.dest)
// Write the temporary data 'V' to the AXIS side channel 'T'.
//
def int_fpga_axis_nb_push: Intrinsic<[llvm_i1_ty],
    [LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>, LLVMAnyPointerType<llvm_any_ty>,        
     LLVMAnyPointerType<llvm_any_ty>,
     LLVMMatchType<0>, LLVMMatchType<1>, LLVMMatchType<2>, LLVMMatchType<3>, 
     LLVMMatchType<4>, LLVMMatchType<5>, LLVMMatchType<6>],
    [IntrArgMemOnly, NoCapture<0>, NoCapture<1>, NoCapture<2>, NoCapture<3>,
     NoCapture<4>, NoCapture<5>, NoCapture<6>, NoCapture<7>, ReadOnly<7>,
     NoCapture<8>, ReadOnly<8>, NoCapture<9>, ReadOnly<9>,
     NoCapture<10>, ReadOnly<10>, NoCapture<11>, ReadOnly<11>,
     NoCapture<12>, ReadOnly<12>, NoCapture<13>, ReadOnly<13>]>,
    GCCBuiltin<"__fpga_axis_nb_push">;
//
//===---

// FIXME:
def int_fpga_type_annotate: Intrinsic<[], [llvm_vararg_ty], [IntrInaccessibleMemOnly]>;

//===---
//
//  Sequential Load / Store
//
//  A sequence of load must look like:
//
//      %tok = fpga.seq.load.begin(%addr, %n)
//      for (%i = 0; %i < %n; ++%i)
//        ... = fpga.seq.load(%tok, %i)
//      fpga.seq.load.end(%tok, %n)
//
//  The loop structure can be more complex, but:
//    - load.begin and load.end must have a matching number (here %n)
//    - all %n loads *must happen* exactly once at run-time
//    - all %n load must happen in the *increasing order*
//    - all %n loads must happen after the begin and before the end
//
//  Same idea for store.
//
//  Even though loads only read and stores only write, they are marked
//  as read/write to guarantee they are not reordered by any LLVM passes.
//
//  For example, we don't want a load to move inside a seq.load:
//
//      %tok = fpga.seq.load.begin(%addr, %n)
//      ...
//      fpga.seq.load.end(%tok, %n)
//      %z = load %addr  ; This one should not go inside the region above
//
//===---

// Ptr = fpga.seq.load.begin(Ptr, Len)
def int_fpga_seq_load_begin : Intrinsic<[LLVMAnyPointerType<llvm_any_ty>],
                                        [LLVMMatchType<0>, llvm_anyint_ty],
                                        [IntrArgMemOnly]>;

// Data = fpga.seq.load(Ptr, Offset)
def int_fpga_seq_load : Intrinsic<[llvm_any_ty],
                                  [LLVMAnyPointerType<LLVMMatchType<0>>,
                                   llvm_anyint_ty],
                                  [IntrArgMemOnly]>;

// fpga.seq.load.end(Ptr, Len)
def int_fpga_seq_load_end : Intrinsic<[], [LLVMAnyPointerType<llvm_any_ty>,
                                           llvm_anyint_ty],
                                      [IntrArgMemOnly]>;

// Ptr = fpga.seq.store.begin(Ptr, Len)
def int_fpga_seq_store_begin : Intrinsic<[LLVMAnyPointerType<llvm_any_ty>],
                                         [LLVMMatchType<0>, llvm_anyint_ty],
                                         [IntrArgMemOnly]>;

// fpga.seq.store(Data, Ptr, Offset)
def int_fpga_seq_store : Intrinsic<[], [llvm_any_ty,
                                        LLVMAnyPointerType<LLVMMatchType<0>>,
                                        llvm_anyint_ty, llvm_anyint_ty],
                                   [IntrArgMemOnly]>;

// fpga.seq.store.end(Ptr, Len)
def int_fpga_seq_store_end : Intrinsic<[], [LLVMAnyPointerType<llvm_any_ty>,
                                            llvm_anyint_ty],
                                       [IntrArgMemOnly]>;

// Abstracted function to write to printf buffer
def int_fpga_get_printf_buffer : Intrinsic<[LLVMAnyPointerType<llvm_anyint_ty>], [],
                                    [IntrNoMem]>;

// Abstracted function to write to printf buffer
def int_fpga_set_stream_depth : Intrinsic<[], [llvm_vararg_ty], []>;

// llvm.recip
def int_fpga_recip : Intrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>],
                               [IntrNoMem, IntrSpeculatable]>;

// llvm.rsqrt
def int_fpga_rsqrt : Intrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>],
                               [IntrNoMem, IntrSpeculatable]>;

// void llvm.fpga.maxi.read.req(%type *%addr, i32 len)
def int_fpga_maxi_read_req : Intrinsic<[], [LLVMAnyPointerType<llvm_any_ty>, 
                                            llvm_i32_ty],
                                       [IntrArgMemOnly, NoCapture<0>]>;

// %type llvm.fpga.maxi.read(%type *%port)
def int_fpga_maxi_read : Intrinsic<[llvm_any_ty], 
                                   [LLVMAnyPointerType<LLVMMatchType<0>>], 
                                   [IntrArgMemOnly, NoCapture<0>]>;

// void llvm.fpga.maxi.write.req(%type *%addr, i32 len)
def int_fpga_maxi_write_req : Intrinsic<[], [LLVMAnyPointerType<llvm_any_ty>, 
                                             llvm_i32_ty],
                                        [IntrArgMemOnly, NoCapture<0>]>;

// void llvm.fpga.maxi.write(%type val, %type *%port, ix %byteenable)
def int_fpga_maxi_write : Intrinsic<[], 
                                    [llvm_any_ty, 
                                     LLVMAnyPointerType<LLVMMatchType<0>>,
                                     llvm_anyint_ty], 
                                    [IntrArgMemOnly, NoCapture<1>]>;

// void llvm.fpga.maxi.write.resp(%type *%port)
def int_fpga_maxi_write_resp : Intrinsic<[], [LLVMAnyPointerType<llvm_any_ty>],
                                         [IntrArgMemOnly, NoCapture<0>]>;

// void llvm.fgpa.add.task(%taskregion_id, %task_id)
def int_fpga_add_task : Intrinsic<[], 
                                  [LLVMAnyPointerType<llvm_i32_ty>, LLVMAnyPointerType<llvm_i32_ty>],
                                  [IntrArgMemOnly, NoCapture<0>]>;

// void llvm.fgpa.add.infinite.task(%taskregion_id, %task_id)
def int_fpga_add_infinite_task : Intrinsic<[],
                                           [LLVMAnyPointerType<llvm_i32_ty>, LLVMAnyPointerType<llvm_i32_ty>],
                                           [IntrArgMemOnly, NoCapture<0>]>;


// i64* llvm.fpga.any()
def int_fpga_any : Intrinsic<[llvm_i64_ty], [], [IntrNoMem]>;

// void llvm.fpga.fence(...)
def int_fpga_fence : Intrinsic<[], 
                               [llvm_vararg_ty],
                               [IntrArgMemOnly]>;
}
