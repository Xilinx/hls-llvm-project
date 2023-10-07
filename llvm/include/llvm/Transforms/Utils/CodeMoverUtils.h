/*
Copyright (C) 2023, Advanced Micro Devices, Inc.
SPDX-License-Identifier: X11

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of Advanced Micro Devices
shall not be used in advertising or otherwise to promote the sale,
use or other dealings in this Software without prior written authorization
from Advanced Micro Devices, Inc.
*/
//===----------------------------------------------------------------------===//
//
// This family of functions determine movements are safe on basic blocks, and
// instructions contained within a function.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_UTILS_CODEMOVERUTILS_H
#define LLVM_TRANSFORMS_UTILS_CODEMOVERUTILS_H

namespace llvm {

class BasicBlock;
class DependenceInfo;
class DominatorTree;
class Instruction;
class PostDominatorTree;

/// Return true if \p I0 and \p I1 are control flow equivalent.
/// Two instructions are control flow equivalent if their basic blocks are
/// control flow equivalent.
bool isControlFlowEquivalent(const Instruction &I0, const Instruction &I1,
                             const DominatorTree &DT,
                             const PostDominatorTree &PDT);

/// Return true if \p BB0 and \p BB1 are control flow equivalent.
/// Two basic blocks are control flow equivalent if when one executes, the other
/// is guaranteed to execute.
bool isControlFlowEquivalent(const BasicBlock &BB0, const BasicBlock &BB1,
                             const DominatorTree &DT,
                             const PostDominatorTree &PDT);

/// Return true if \p I can be safely moved before \p InsertPoint.
bool isSafeToMoveBefore(Instruction &I, Instruction &InsertPoint,
                        DominatorTree &DT, const PostDominatorTree &PDT,
                        DependenceInfo &DI,
                        bool CheckForEntireBlock = false);

/// Return true if all instructions (except the terminator) in \p BB can be
/// safely moved before \p InsertPoint.
bool isSafeToMoveBefore(BasicBlock &BB, Instruction &InsertPoint,
                        DominatorTree &DT, const PostDominatorTree &PDT,
                        DependenceInfo &DI);

/// Move instructions, in an order-preserving manner, from \p FromBB to the
/// beginning of \p ToBB when proven safe.
void moveInstructionsToTheBeginning(BasicBlock &FromBB, BasicBlock &ToBB,
                                    DominatorTree &DT,
                                    const PostDominatorTree &PDT,
                                    DependenceInfo &DI);

/// Move instructions, in an order-preserving manner, from \p FromBB to the end
/// of \p ToBB when proven safe.
void moveInstructionsToTheEnd(BasicBlock &FromBB, BasicBlock &ToBB,
                              DominatorTree &DT, const PostDominatorTree &PDT,
                              DependenceInfo &DI);

/// In case that two BBs \p ThisBlock and \p OtherBlock are control flow
/// equivalent but they do not strictly dominate and post-dominate each
/// other, we determine if \p ThisBlock is reached after \p OtherBlock
/// in the control flow.
bool nonStrictlyPostDominate(const BasicBlock *ThisBlock,
                             const BasicBlock *OtherBlock,
                             const DominatorTree *DT,
                             const PostDominatorTree *PDT);

// Check if I0 is reached before I1 in the control flow.
bool isReachedBefore(const Instruction *I0, const Instruction *I1,
                     const DominatorTree *DT, const PostDominatorTree *PDT);

} // end namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_CODEMOVERUTILS_H
