// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.

#include "clang/Frontend/MultiplexConsumer.h"
#include "clang/AST/ASTMutationListener.h"
#include "clang/AST/DeclGroup.h"
#include "clang/Sema/Sema.h"
#include "clang/Serialization/ASTDeserializationListener.h"

using namespace clang;

namespace clang {

/*
 * some clarify for HoistXlxScope:
 *
 * 1. ASConsumer:
 *  HandleTopLevelDecl is called after parser generate one Decl
 *  take notation that Class/Function instantiate  in
 * SemaTemplateInstantiate.cpp file  will generate new TopLevelDecl , and call
 * ASTConsumer::HandleTopLevelDecl
 *
 * 2. can we use a standalone FrontendAction to handle HoistXlxScope ?
 *    No,  ParseAST is drived by FrontendAction, CodeGenAction is subclass of
 *    FrontendAction, we can not add a standalone Action which call Parse source
 * code and generate AST in memory, and feed the memory AST to CodgenAction,
 *    Clang 's action mechanism doesn't support it
 *
 * 3. When HoistXlxScope is called ?
 * ParseAST call HandleTopLevelDecl for Sema's ASTConsumer , then
 * ASTConsumer::HandleTopLevelDecl is called, and do Xlx Scope Hoist
 *
 */

class XlxAttrHoistConsumer : public SemaConsumer {
  Sema *sema_ptr;

public:
  virtual bool HandleTopLevelDecl(DeclGroupRef D);
  virtual void InitializeSema(Sema &sema);
};

void XlxAttrHoistConsumer::InitializeSema(Sema &sema) { sema_ptr = &sema; }

bool XlxAttrHoistConsumer::HandleTopLevelDecl(DeclGroupRef D) {
  for (DeclGroupRef::iterator I = D.begin(), E = D.end(); I != E; I++) {
    sema_ptr->HoistXlxScope(*I);
  }
  return true;
}
} //end namespace  clang 



