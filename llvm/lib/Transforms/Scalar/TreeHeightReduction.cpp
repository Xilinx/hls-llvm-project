//===- TreeHeightReduction.cpp - Minimize the height of an operation tree -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass implements a tree-height-reduction pass.
//
// Tree height reduction is an optimization to increase instruction level
// parallelism by transforming an operation tree like the following.
//
//            Before                                  After
//
//                    _ N1 _                  _________ N1 _________
//                   |      |                |                      |
//                 _ N2 _   L1           ___ N2 ___             ___ N3 ___
//                |      |              |          |           |          |
//              _ N3 _   L2     --->  _ N4 _     _ N5 _      _ N6 _     _ N7 _
//             |      |              |      |   |      |    |      |   |      |
//           _ N4 _   L3             L1     L2  L3     L4   L5     L6  L7     L8
//          |      |
//        _ N5 _   L4
//       |      |
//     _ N6 _   L5
//    |      |
//  _ N7 _   L6
// |      |
// L7     L8
//
//===----------------------------------------------------------------------===//
//
// The algorithm of tree height reduction is based on the paper:
//   Katherine Coons, Warren Hunt, Bertrand A. Maher, Doug Burger,
//   Kathryn S. McKinley.
//   Optimal Huffman Tree-Height Reduction for Instruction-Level Parallelism.
//

#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/TreeHeightReduction.h"
#include <queue>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::thr;
#define DEBUG_TYPE "tree-height-reduction"

// Whether to apply tree height reduction to integer instructions.
static cl::opt<bool> EnableIntTHR(
    "enable-int-thr",
    cl::desc("Enable tree height reduction to integer instructions"),
    cl::init(true));

// Whether to apply tree height reduction to floating-point instructions.
static cl::opt<bool> EnableFpTHR(
    "enable-fp-thr",
    cl::desc("Enable tree height reduction to floating-point instructions"),
    cl::init(false));

// Minimum number of leaves to apply tree height reduction.
// Tree height reduction has effect only if the number of leaves is 4 or more.
static cl::opt<unsigned> MinLeaves(
    "thr-min-leaves",
    cl::desc("Minimum number of leaves to apply tree height reduction "
             "(default = 4)"),
    cl::init(4));

namespace {
class Node {
public:
  explicit Node(Value *V)
      : Inst(nullptr), DefValue(V), Parent(nullptr), Left(nullptr),
        Right(nullptr), Latency(0), TotalCost(0) {
    if (Instruction *I = dyn_cast<Instruction>(V)) {
      Inst = I;
    }
  }

  /// Set the parent node of this node.
  void setParent(Node *P) { Parent = P; }

  /// Set the left node of this node.
  void setLeft(Node *L) { Left = L; }

  /// Set the right node of this node.
  void setRight(Node *R) { Right = R; }

  /// Set the latency of this node.
  void setLatency(int L) { Latency = L; }

  /// Set the total cost of this node.
  void setTotalCost(int C) { TotalCost = C; }

  /// Get the original instruction of this node.
  Instruction *getOrgInst() const {
    assert(Inst && "Inst should not be nullptr.");
    return Inst;
  }

  /// Get the defined value of this node.
  Value *getDefinedValue() const {
    assert(DefValue && "DefValue should not be nullptr");
    return DefValue;
  }

  /// Get the parent node of this node.
  Node *getParent() const { return Parent; }

  /// Get the left node of this node.
  Node *getLeft() const { return Left; }

  /// Get the right node of this node.
  Node *getRight() const { return Right; }

  /// Get the latency of this node.
  int getLatency() const { return Latency; }

  /// Get the total cost of this node.
  int getTotalCost() const { return TotalCost; }

  /// Return true if this node is a branch (including a root).
  bool isBranch() const { return Left != nullptr && Right != nullptr; }

  /// Return true if this node is a pure leaf.
  bool isLeaf() const { return !isBranch(); }

  /// Return true if this node is a root.
  bool isRoot() const { return Parent == nullptr; }

  /// Return true if this node is considered as a leaf node.
  /// Tree height reduction can be applied only to nodes whose operation codes
  /// are same. In addition, IR flags like 'nuw' must be same to preserve them.
  /// So it is necessary to consider a node whose operation code or IR flags
  /// are different from its parent's ones as a leaf node.
  bool isConsideredAsLeaf() const {
    if (isLeaf())
      return true;
    if (isRoot())
      return false;
    if (getOrgInst()->getOpcode() != getParent()->getOrgInst()->getOpcode() ||
        !getOrgInst()->hasSameSubclassOptionalData(getParent()->getOrgInst()))
      return true;
    return false;
  }

  /// Update tree's latency under this node.
  void updateTreeLatency(TargetTransformInfo *TTI);

  /// Update the latency of this node.
  void updateNodeLatency(TargetTransformInfo *TTI);

  /// Update the latency of this node using the left or right node.
  void updateLeftOrRightNodeLatency(TargetTransformInfo *TTI, Node *SubNode);

private:
  /// Original instruction of this node.
  Instruction *Inst;

  /// Defined value of this node.
  /// This may be a constant value.
  Value *DefValue;

  /// Parent node of this node.
  Node *Parent;
  /// Left node and right node of this node.
  Node *Left, *Right;

  /// Instruction latency.
  int Latency;
  /// Total cost of nodes under this node.
  int TotalCost;
};

class TreeHeightReduction {
public:
  enum struct InstTy { INTEGER, FLOATING_POINT };

  TreeHeightReduction(InstTy Ty, TargetTransformInfo *TTI,
                      OptimizationRemarkEmitter *ORE)
      : TargetInstTy(Ty), TTI(TTI), ORE(ORE) {}

  /// Apply tree height reduction to one basic block.
  bool runOnBasicBlock(BasicBlock *BB);

private:
  /// Type of instruction included in the operation tree.
  InstTy TargetInstTy;

  TargetTransformInfo *TTI;
  OptimizationRemarkEmitter *ORE;

  /// Return true if 'I' is a target instruction of tree height reduction.
  bool isTHRTargetInst(Instruction *I) const;

  /// Construct an operation tree from the value 'V'.
  Node *constructTree(Value *V, BasicBlock *BB, bool top = false);

  /// Destruct an operation tree constructed by constructTree.
  void destructTree(Node *N);

  /// Collect original instructions to be erased from the basic block.
  void collectInstsToBeErasedFrom(Node *N, std::vector<Instruction *> &Insts);

  /// Apply tree height reduction to the tree 'N'.
  /// Returned value is a tree to which tree height reduction is applied.
  Node *applyTreeHeightReduction(Node *N, bool isLeft);

  /// Collect leaf nodes and reusable branch nodes under the node 'N'.
  void collectLeavesAndReusableBranches(Node *N, std::vector<Node *> &Leaves,
                                        std::vector<Node *> &ReusableBranches);

  /// Construct an optimized subtree by applying tree height reduction.
  Node *constructOptimizedSubtree(std::vector<Node *> &Leaves,
                                  std::vector<Node *> &ReusableBranches);

  /// Combine two leaf elements, create a branch from them, and put it into
  /// 'Leaves'.
  void combineLeaves(std::vector<Node *> &Leaves, Node *Op1, Node *Op2,
                     std::vector<Node *> &ReusableBranches);

  /// Create IRs for the new tree.
  void createIRs(Node *Root, std::set<Instruction *> &GeneratedInsts);

  /// Create a new instruction for the node 'N' with operands 'Op1' and 'Op2'.
  Value *createInst(IRBuilder<> &Builder, Node *N, Value *Op1, Value *Op2);

  /// Return true if 'I' is a root candidate.
  bool isRootCandidate(Instruction &I) const {
    if (!isTHRTargetInst(&I))
      return false;
    for (unsigned i = 0; i < I.getNumOperands(); ++i)
      if (isBranchCandidate(I.getOperand(i)))
        return true;
    return false;
  }

  /// Return true if 'Op' is a branch candidate.
  bool isBranchCandidate(Value *Op) const {
    assert(Op && "Operand should not be nullptr");
    if (!Op->hasOneUse())
      return false;
    if (Instruction *I = dyn_cast<Instruction>(Op))
      return isTHRTargetInst(I);
    return false;
  }

  void printTree(raw_ostream &OS, Node *N, const int Indent) const {
    if (N->isLeaf())
      return;

    for (int i = 0; i < Indent; ++i)
      OS << " ";
    N->getOrgInst()->dump();

    for (int i = 0; i < Indent + 4; ++i)
      OS << " ";
    OS << "Latency: " << N->getLatency();
    OS << ", TotalCost: " << N->getTotalCost() << "\n";

    printTree(OS, N->getLeft(), Indent + 2);
    printTree(OS, N->getRight(), Indent + 2);
  }

  void printLeaves(raw_ostream &OS, std::vector<Node *> &Leaves,
                   bool isBefore) const {
    if (isBefore)
      OS << "  --- Before ---\n";
    else
      OS << "  --- After ---\n";
    for (auto *Node : Leaves)
      printTree(OS, Node, 2);
  }
};
} // end anonymous namespace

// BFS: Breadth First Search
static std::vector<Node *> getNodesByBFS(Node *N) {
  std::vector<Node *> Nodes = {N};

  for (unsigned i = 0; i < Nodes.size(); ++i) {
    Node *CurNode = Nodes[i];
    if (CurNode->isBranch()) {
      Nodes.push_back(CurNode->getLeft());
      Nodes.push_back(CurNode->getRight());
    }
  }

  return std::move(Nodes);
}

void Node::updateTreeLatency(TargetTransformInfo *TTI) {
  std::vector<Node *> Nodes = getNodesByBFS(this);
  // Update node latency by bottom-up order.
  for (auto Begin = Nodes.rbegin(), End = Nodes.rend(); Begin != End; ++Begin) {
    Node *CurNode = *Begin;
    CurNode->updateNodeLatency(TTI);
  }
}

void Node::updateNodeLatency(TargetTransformInfo *TTI) {
  setLatency(0);
  setTotalCost(0);

  if (isLeaf())
    return;

  // Tree height reduction minimizes the weighted sum of heights.
  // The latency of each instruction is used as the height of each node.
  // This makes an optimal operation tree even if operation types (add, mul,
  // etc.) are mixed in the tree.

  updateLeftOrRightNodeLatency(TTI, getLeft());
  updateLeftOrRightNodeLatency(TTI, getRight());

  Instruction *Inst = getOrgInst();
  int InstLatency =
      TTI->getInstructionCost(Inst, TargetTransformInfo::TCK_Latency);
#if 0
  // To balance the operation tree by sorting nodes by latency even if the
  // latency is unknown, set a valid latency.
  if (!InstLatency.isValid())
    InstLatency = 1;
#endif
  setLatency(getLatency() + InstLatency);
  setTotalCost(getTotalCost() + InstLatency);
}

void Node::updateLeftOrRightNodeLatency(TargetTransformInfo *TTI,
                                        Node *SubNode) {
  assert(SubNode && "Left or right node should not be nullptr.");
  const int SubNodeLatency = SubNode->getLatency();
  if (SubNodeLatency > getLatency())
    setLatency(SubNodeLatency);
  setTotalCost(getTotalCost() + SubNode->getTotalCost());
}

static void eraseOrgInsts(std::vector<Instruction *> &Insts) {
  for (auto *I : Insts)
    I->eraseFromParent();
}

static bool isProfitableToApply(Node *Root) {
  std::vector<Node *> Nodes = getNodesByBFS(Root);
  unsigned NumLeaves = 0;

  for (auto *N : Nodes) {
    if (N->isLeaf())
      ++NumLeaves;
    if (NumLeaves >= MinLeaves)
      return true;
  }

  return false;
}

bool TreeHeightReduction::runOnBasicBlock(BasicBlock *BB) {
  bool Applied = false;
  std::set<Instruction *> GeneratedInsts;

  // Search target instructions in the basic block reversely.
  for (auto Begin = BB->rbegin(); Begin != BB->rend(); ++Begin) {
    Instruction &I = *Begin;
    if (GeneratedInsts.count(&I) == 1)
      continue;
    if (!isRootCandidate(I))
      continue;

    // Construct an original operation tree from the root instruction.
    Node *OrgTree = constructTree(&I, BB, true);

    if (!isProfitableToApply(OrgTree)) {
      destructTree(OrgTree);
      continue;
    }

    std::vector<Instruction *> OrgInsts;
    collectInstsToBeErasedFrom(OrgTree, OrgInsts);

    // Apply tree height reduction to the original tree 'OrgTree'
    // and create a new tree 'ReducedTree'.
    Node *ReducedTree = applyTreeHeightReduction(OrgTree, true);
    assert(ReducedTree->getOrgInst() == OrgTree->getOrgInst() &&
           "OrgInst of ReducedTree and OrgTree should be same.");

    // Create IRs from the tree to which tree height reduction was applied.
    createIRs(ReducedTree, GeneratedInsts);

    // The following optimization message output process must be called
    // before 'eraseOrgInsts' because 'I' is erased there.
    ORE->emit([&]() {
      return OptimizationRemark(DEBUG_TYPE, "TreeHeightReduction", &I)
             << "reduced tree height";
    });

    --Begin;
    eraseOrgInsts(OrgInsts);
    Applied = true;

    // 'OrgTree' and 'ReducedTree' share memory, so it is enough to release
    // memory of 'ReducedTree'.
    destructTree(ReducedTree);
  }

  return Applied;
}

template <typename T> static bool isUsedAtCmpInst(Instruction *I) {
  for (User *U : I->users()) {
    Instruction *UserInst = dyn_cast<Instruction>(U);
    if (UserInst && isa<T>(*UserInst))
      return true;
  }
  return false;
}

// Return true if 'I' is an integer type and is a target instruction of tree
// height reduction.
static bool isIntgerInstTHRTarget(Instruction *I) {
  if (!I->getType()->isIntegerTy())
    return false;
  // 'I' which is used at ICmpInst may be an induction variable.
  if (isUsedAtCmpInst<ICmpInst>(I))
    return false;
  // Add, Mul, And, Or, or Xor
  return I->isCommutative() && I->isAssociative();
}

// Return true if 'I' is a floating-point type and is a target instruction
// of tree height reduction.
static bool isFpInstTHRTarget(Instruction *I) {
  if (!I->getType()->isFloatingPointTy())
    return false;
  // FAdd or FMul with reassoc
  return I->isCommutative() && I->isAssociative();
}

bool TreeHeightReduction::isTHRTargetInst(Instruction *I) const {
  switch (TargetInstTy) {
  case InstTy::INTEGER:
    return isIntgerInstTHRTarget(I);
  case InstTy::FLOATING_POINT:
    return isFpInstTHRTarget(I);
  default:
    return false;
  }
}

Node *TreeHeightReduction::constructTree(Value *V, BasicBlock *BB, bool top) {
  if (!top && !isBranchCandidate(V))
    return new Node(V);

  Instruction *I = dyn_cast<Instruction>(V);
  assert(I && "Instruction should not be nullptr.");
  assert(I->getNumOperands() == 2 && "The number of operands should be 2.");

  if (I->getParent() != BB)
    return new Node(V);

  Node *Parent = new Node(V);

  Value *LeftOp = I->getOperand(0);
  Node *Left = constructTree(LeftOp, BB);
  Parent->setLeft(Left);
  Left->setParent(Parent);

  Value *RightOp = I->getOperand(1);
  Node *Right = constructTree(RightOp, BB);
  Parent->setRight(Right);
  Right->setParent(Parent);

  return Parent;
}

void TreeHeightReduction::destructTree(Node *N) {
  std::vector<Node *> Nodes = getNodesByBFS(N);
  for (auto *CurNode : Nodes)
    delete CurNode;
}

void TreeHeightReduction::collectInstsToBeErasedFrom(
    Node *N, std::vector<Instruction *> &Insts) {
  std::vector<Node *> Nodes = getNodesByBFS(N);
  for (auto *CurNode : Nodes)
    // Instructions belonging to leaf nodes should be saved.
    if (!CurNode->isLeaf())
      Insts.push_back(CurNode->getOrgInst());
}

Node *TreeHeightReduction::applyTreeHeightReduction(Node *N, bool isLeft) {
  // Postorder depth-first search.
  if (!N->isBranch())
    return N;
  applyTreeHeightReduction(N->getLeft(), true);
  applyTreeHeightReduction(N->getRight(), false);

  // Save original parent information.
  Node *Parent = N->getParent();

  std::vector<Node *> Leaves;
  // 'ReusableBranches' holds branch nodes which are reused when updating
  // parent and child node's relationship in constructOptimizedSubtree().
  // By doing so, the amount of memory used can be reduced.
  std::vector<Node *> ReusableBranches;
  collectLeavesAndReusableBranches(N, Leaves, ReusableBranches);

  Node *NewNode = constructOptimizedSubtree(Leaves, ReusableBranches);
  NewNode->setParent(Parent);
  if (Parent) {
    if (isLeft)
      Parent->setLeft(NewNode);
    else
      Parent->setRight(NewNode);
  }
  // Return value has a meaning only if 'Parent' is nullptr because
  // this means 'Node' is a root node.
  return NewNode;
}

void TreeHeightReduction::collectLeavesAndReusableBranches(
    Node *N, std::vector<Node *> &Leaves,
    std::vector<Node *> &ReusableBranches) {
  std::vector<Node *> Worklist = {N};

  // NOTE: Don't use 'getNodesAndLeavesByBFS'! Like that function, the following
  //       processing collects all leaves and all branches by BFS, but it is
  //       different from that function because this is BFS with a condition.
  for (unsigned i = 0; i < Worklist.size(); ++i) {
    Node *CurNode = Worklist[i];
    if (CurNode->isConsideredAsLeaf()) {
      Leaves.push_back(CurNode);
    } else {
      ReusableBranches.push_back(CurNode);
      Worklist.push_back(CurNode->getLeft());
      Worklist.push_back(CurNode->getRight());
    }
  }
}

Node *TreeHeightReduction::constructOptimizedSubtree(
    std::vector<Node *> &Leaves, std::vector<Node *> &ReusableBranches) {
  while (Leaves.size() > 1) {
    std::stable_sort(Leaves.begin(), Leaves.end(), [](Node *LHS, Node *RHS) -> bool {
      if (LHS->getLatency() != RHS->getLatency())
        return LHS->getLatency() < RHS->getLatency();
      if (LHS->getTotalCost() != RHS->getTotalCost())
        return LHS->getTotalCost() < RHS->getTotalCost();
      return false;
    });
    DEBUG(printLeaves(dbgs(), Leaves, true));

    Node *Op1 = Leaves[0], *Op2 = Leaves[1];
    Leaves.erase(Leaves.begin(), Leaves.begin() + 2);
    combineLeaves(Leaves, Op1, Op2, ReusableBranches);

    DEBUG(printLeaves(dbgs(), Leaves, false));
  }

  return Leaves.front();
}

void TreeHeightReduction::combineLeaves(std::vector<Node *> &Leaves, Node *Op1,
                                        Node *Op2,
                                        std::vector<Node *> &ReusableBranches) {
  Node *N = ReusableBranches.back();
  ReusableBranches.pop_back();

  // Update the parent-child relationship.
  N->setLeft(Op1);
  N->setRight(Op2);
  Op1->setParent(N);
  Op2->setParent(N);

  N->updateTreeLatency(TTI);
  Leaves.push_back(N);
}

static std::vector<Node *> getBranches(Node *N) {
  std::vector<Node *> Nodes = getNodesByBFS(N);

  // Exclude leaves.
  for (auto Iter = Nodes.begin(); Iter != Nodes.end(); ++Iter)
    if ((*Iter)->isLeaf()) {
      Nodes.erase(Iter);
      --Iter;
    }

  return std::move(Nodes);
}

static void setLeftAndRightOperand(Value *&LeftOp, Value *&RightOp, Node *N,
                                   std::queue<Value *> &Ops) {
  auto frontPopVal = [](std::queue<Value *> &Ops) -> Value * {
    Value *V = Ops.front();
    Ops.pop();
    return V;
  };

  Node *L = N->getLeft();
  Node *R = N->getRight();
  if (L->isLeaf() && R->isLeaf()) {
    // Both the left child and the right child of 'N' are leaves.
    LeftOp = L->getDefinedValue();
    RightOp = R->getDefinedValue();
  } else {
    assert(!Ops.empty());
    if (L->isLeaf()) {
      // The left child is a leaf and the right child is a branch.
      LeftOp = L->getDefinedValue();
      RightOp = frontPopVal(Ops);
    } else if (R->isLeaf()) {
      // The left child is a branch and the right child is a leaf.
      LeftOp = frontPopVal(Ops);
      RightOp = R->getDefinedValue();
    } else {
      // Both the left child and the right child of 'N' are branches.
      LeftOp = frontPopVal(Ops);
      RightOp = frontPopVal(Ops);
    }
  }
}

void TreeHeightReduction::createIRs(Node *Root,
                                    std::set<Instruction *> &GeneratedInsts) {
  Instruction *RootInst = Root->getOrgInst();
  IRBuilder<> Builder(RootInst);

  std::vector<Node *> Nodes = getBranches(Root);
  std::queue<Value *> Ops;
  while (!Nodes.empty()) {
    Node *N = Nodes.back();

    Value *LeftOp = nullptr, *RightOp = nullptr;
    setLeftAndRightOperand(LeftOp, RightOp, N, Ops);

    Value *NewNodeValue = createInst(Builder, N, LeftOp, RightOp);
    Ops.push(NewNodeValue);
    GeneratedInsts.insert(dyn_cast<Instruction>(NewNodeValue));

    Nodes.pop_back();
  }

  assert(Ops.size() == 1 && "The size of queue should be 1.");
  Value *NewRootValue = Ops.front();
  GeneratedInsts.insert(dyn_cast<Instruction>(NewRootValue));
  RootInst->replaceAllUsesWith(NewRootValue);
}

Value *TreeHeightReduction::createInst(IRBuilder<> &Builder, Node *N,
                                       Value *Op1, Value *Op2) {
  Value *V;
  switch (N->getOrgInst()->getOpcode()) {
  case Instruction::Add:
    V = Builder.CreateAdd(Op1, Op2, Twine("thr.add"));
    break;
  case Instruction::Mul:
    V = Builder.CreateMul(Op1, Op2, Twine("thr.mul"));
    break;
  case Instruction::And:
    V = Builder.CreateAnd(Op1, Op2, Twine("thr.and"));
    break;
  case Instruction::Or:
    V = Builder.CreateOr(Op1, Op2, Twine("thr.or"));
    break;
  case Instruction::Xor:
    V = Builder.CreateXor(Op1, Op2, Twine("thr.xor"));
    break;
  case Instruction::FAdd:
    V = Builder.CreateFAdd(Op1, Op2, Twine("thr.fadd"));
    break;
  case Instruction::FMul:
    V = Builder.CreateFMul(Op1, Op2, Twine("thr.fmul"));
    break;
  default:
    llvm_unreachable("Unexpected operation code");
  }

  // Take over the original instruction IR flags.
  // V may have been folded to Constant if Op1 and Op2 are both Constant.
  if (Instruction *NewInst = dyn_cast<Instruction>(V))
    NewInst->copyIRFlags(N->getDefinedValue());

  return V;
}

bool TreeHeightReductionPass::runImpl(Loop &L,  
                              TargetTransformInfo *TTI) {
 // Tree height reduction is applied only to inner-most loops.
  if (!L.getSubLoops().empty())
    return false;

  OptimizationRemarkEmitter ORE(L.getHeader()->getParent());
  bool Changed = false;
  auto &LoopBlocks = L.getBlocksVector();

  if (EnableIntTHR) {
    auto THR = TreeHeightReduction(TreeHeightReduction::InstTy::INTEGER,
                                   TTI, &ORE);
    for (auto *BB : LoopBlocks)
      Changed |= THR.runOnBasicBlock(BB);
  }

  if (EnableFpTHR) {
    auto THR = TreeHeightReduction(TreeHeightReduction::InstTy::FLOATING_POINT,
                                   TTI, &ORE);
    for (auto *BB : LoopBlocks)
      Changed |= THR.runOnBasicBlock(BB);
  }
  return Changed;
}


PreservedAnalyses TreeHeightReductionPass::run(Loop &L, LoopAnalysisManager &AM,
                                               LoopStandardAnalysisResults &AR,
                                               LPMUpdater &U) {
  if (!runImpl(L, &AR.TTI)) 
    return PreservedAnalyses::all();
  return getLoopPassPreservedAnalyses();
}

struct llvm::thr::LegacyTreeHeightReductionPass : public LoopPass {
  static char ID; // Pass identification, replacement for typeid
  LegacyTreeHeightReductionPass() : LoopPass(ID) {
    initializeLegacyTreeHeightReductionPassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnLoop(Loop *L, LPPassManager &LPM) override {
    return THRPass.runImpl(*L, 
                          &getAnalysis<TargetTransformInfoWrapperPass>().getTTI(
                              *L->getHeader()->getParent()));
  }

  /// This transformation requires natural loop information & requires that
  /// loop preheaders be inserted into the CFG...
  ///
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<TargetTransformInfoWrapperPass>();
  }

  using llvm::Pass::doFinalization;

  bool doFinalization() override {
    return false;
  }

private:
  TreeHeightReductionPass THRPass;
};

char LegacyTreeHeightReductionPass::ID = 0;

INITIALIZE_PASS_BEGIN(LegacyTreeHeightReductionPass, "tree-height-reduction", "Reduce the height of tree", false, false)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(LegacyTreeHeightReductionPass, "tree-height-reduction", "Reduce the height of tree", false, false)

// The public interface to this file...
Pass *llvm::createLegacyTreeHeightReductionPass() {
  return new LegacyTreeHeightReductionPass();
}
