// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
#include "clang/AST/Stmt.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/AttrKinds.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"
#include <iostream>
using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

using namespace llvm;

// Set up the command line options
static cl::OptionCategory PerformancePragmaDetectorCategory(
    "xilinx-performance-pragma-detector options");

/// declare the performance pragma match callback to collect all matched
/// performance pragma
class PerformancePragmaMatchCB : public MatchFinder::MatchCallback {
public:
  std::vector<BoundNodes> &Matches;
  PerformancePragmaMatchCB(std::vector<BoundNodes> &Matches)
      : Matches(Matches) {}

  virtual void run(const MatchFinder::MatchResult &Result) override {
    Matches.push_back(Result.Nodes);
  }
};

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  CommonOptionsParser OptionsParser(argc, argv,
                                    PerformancePragmaDetectorCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  // build the AST for the source code, return -1 if build AST failed
  std::vector<std::unique_ptr<ASTUnit>> ASTs;
  if (Tool.buildASTs(ASTs) != 0)
    return -1;

  auto PerformancePragmaOnLoop =
      attributedStmt(hasAttachedAttr(attr::XlxPerformance)).bind("performance");

  auto PerformancePragmaOnFunction =
      decl(hasAttr(attr::XlxPerformance)).bind("performance");

  std::vector<BoundNodes> Matches;
  PerformancePragmaMatchCB CB(Matches);
  MatchFinder Finder;
  Finder.addMatcher(PerformancePragmaOnLoop, &CB);
  Finder.addMatcher(PerformancePragmaOnFunction, &CB);

  for (auto &AST : ASTs) {
    Finder.matchAST(AST->getASTContext());
  }

  std::cout << "the number of performance pragma: " << Matches.size() << '\n';
  return 0;
}
