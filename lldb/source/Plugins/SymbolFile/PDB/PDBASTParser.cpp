//===-- PDBASTParser.cpp ----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PDBASTParser.h"

#include "clang/AST/CharUnits.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"

#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Symbol/ClangUtil.h"
#include "lldb/Symbol/Declaration.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Symbol/TypeSystem.h"

#include "llvm/DebugInfo/PDB/PDBSymbol.h"
#include "llvm/DebugInfo/PDB/PDBSymbolData.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeArray.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeBuiltin.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeEnum.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeFunctionArg.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeFunctionSig.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypePointer.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeTypedef.h"
#include "llvm/DebugInfo/PDB/PDBSymbolTypeUDT.h"

using namespace lldb;
using namespace lldb_private;
using namespace llvm::pdb;

namespace {
int TranslateUdtKind(PDB_UdtType pdb_kind) {
  switch (pdb_kind) {
  case PDB_UdtType::Class:
    return clang::TTK_Class;
  case PDB_UdtType::Struct:
    return clang::TTK_Struct;
  case PDB_UdtType::Union:
    return clang::TTK_Union;
  case PDB_UdtType::Interface:
    return clang::TTK_Interface;
  }
  return -1;
}

lldb::Encoding TranslateBuiltinEncoding(PDB_BuiltinType type) {
  switch (type) {
  case PDB_BuiltinType::Float:
    return lldb::eEncodingIEEE754;
  case PDB_BuiltinType::Int:
  case PDB_BuiltinType::Long:
  case PDB_BuiltinType::Char:
    return lldb::eEncodingSint;
  case PDB_BuiltinType::Bool:
  case PDB_BuiltinType::UInt:
  case PDB_BuiltinType::ULong:
  case PDB_BuiltinType::HResult:
    return lldb::eEncodingUint;
  default:
    return lldb::eEncodingInvalid;
  }
}

lldb::Encoding TranslateEnumEncoding(PDB_VariantType type) {
  switch (type) {
  case PDB_VariantType::Int8:
  case PDB_VariantType::Int16:
  case PDB_VariantType::Int32:
  case PDB_VariantType::Int64:
    return lldb::eEncodingSint;

  case PDB_VariantType::UInt8:
  case PDB_VariantType::UInt16:
  case PDB_VariantType::UInt32:
  case PDB_VariantType::UInt64:
    return lldb::eEncodingUint;

  default:
    break;
  }

  return lldb::eEncodingSint;
}

CompilerType GetBuiltinTypeForPDBEncodingAndBitSize(
    ClangASTContext *clang_ast, const PDBSymbolTypeBuiltin *pdb_type,
    Encoding encoding, uint32_t width) {
  if (!pdb_type)
    return CompilerType();
  if (!clang_ast)
    return CompilerType();
  auto *ast = clang_ast->getASTContext();
  if (!ast)
    return CompilerType();

  switch (pdb_type->getBuiltinType()) {
  default: break;
  case PDB_BuiltinType::None:
    return CompilerType();
  case PDB_BuiltinType::Void:
    // FIXME: where is non-zero size of `void` from?
    if (width == 0)
      return clang_ast->GetBasicType(eBasicTypeVoid);
  case PDB_BuiltinType::Bool:
    return clang_ast->GetBasicType(eBasicTypeBool);
  case PDB_BuiltinType::Long:
    if (width == ast->getTypeSize(ast->LongTy))
      return CompilerType(ast, ast->LongTy);
    if (width == ast->getTypeSize(ast->LongLongTy))
      return CompilerType(ast, ast->LongLongTy);
    break;
  case PDB_BuiltinType::ULong:
    if (width == ast->getTypeSize(ast->UnsignedLongTy))
      return CompilerType(ast, ast->UnsignedLongTy);
    if (width == ast->getTypeSize(ast->UnsignedLongLongTy))
      return CompilerType(ast, ast->UnsignedLongLongTy);
    break;
  case PDB_BuiltinType::WCharT:
    if (width == ast->getTypeSize(ast->WCharTy))
      return CompilerType(ast, ast->WCharTy);
    break;
  case PDB_BuiltinType::Float:
    // Note: types `long double` and `double` have same bit size in MSVC and there
    // is no information in the PDB to distinguish them. So when falling back
    // to default search, the compiler type of `long double` will be represented by
    // the one generated for `double`.
    break;
  }
  // If there is no match on PDB_BuiltinType, fall back to default search
  // by encoding and width only
  return clang_ast->GetBuiltinTypeForEncodingAndBitSize(encoding, width);
}

ConstString GetPDBBuiltinTypeName(const PDBSymbolTypeBuiltin *pdb_type,
                                  CompilerType &compiler_type) {
  if (!pdb_type)
    return compiler_type.GetTypeName();

  PDB_BuiltinType kind = pdb_type->getBuiltinType();
  switch (kind) {
  default: break;
  case PDB_BuiltinType::Currency:
    return ConstString("CURRENCY");
  case PDB_BuiltinType::Date:
    return ConstString("DATE");
  case PDB_BuiltinType::Variant:
    return ConstString("VARIANT");
  case PDB_BuiltinType::Complex:
    return ConstString("complex");
  case PDB_BuiltinType::Bitfield:
    return ConstString("bitfield");
  case PDB_BuiltinType::BSTR:
    return ConstString("BSTR");
  case PDB_BuiltinType::HResult:
    return ConstString("HRESULT");
  case PDB_BuiltinType::BCD:
    return ConstString("BCD");
  case PDB_BuiltinType::None:
    return ConstString("...");
  }
  return compiler_type.GetTypeName();
}
}

PDBASTParser::PDBASTParser(lldb_private::ClangASTContext &ast) : m_ast(ast) {}

PDBASTParser::~PDBASTParser() {}

// DebugInfoASTParser interface

lldb::TypeSP PDBASTParser::CreateLLDBTypeFromPDBType(const PDBSymbol &type) {
  // PDB doesn't maintain enough information to robustly rebuild the entire
  // tree, and this is most problematic when it comes to figure out the
  // right DeclContext to put a type in.  So for now, everything goes in
  // the translation unit decl as a fully qualified type.
  clang::DeclContext *tu_decl_ctx = m_ast.GetTranslationUnitDecl();
  Declaration decl;

  if (auto udt = llvm::dyn_cast<PDBSymbolTypeUDT>(&type)) {
    AccessType access = lldb::eAccessPublic;
    PDB_UdtType udt_kind = udt->getUdtKind();
    auto tag_type_kind = TranslateUdtKind(udt_kind);
    if (tag_type_kind == -1)
      return nullptr;

    if (udt_kind == PDB_UdtType::Class)
      access = lldb::eAccessPrivate;

    CompilerType clang_type = m_ast.CreateRecordType(
        tu_decl_ctx, access, udt->getName().c_str(), tag_type_kind,
        lldb::eLanguageTypeC_plus_plus, nullptr);

    m_ast.SetHasExternalStorage(clang_type.GetOpaqueQualType(), true);

    return std::make_shared<lldb_private::Type>(
        type.getSymIndexId(), m_ast.GetSymbolFile(),
        ConstString(udt->getName()), udt->getLength(), nullptr,
        LLDB_INVALID_UID, lldb_private::Type::eEncodingIsUID, decl, clang_type,
        lldb_private::Type::eResolveStateForward);
  } else if (auto enum_type = llvm::dyn_cast<PDBSymbolTypeEnum>(&type)) {
    auto underlying_type_up = enum_type->getUnderlyingType();
    if (!underlying_type_up)
      return nullptr;
    lldb::Encoding encoding =
        TranslateBuiltinEncoding(underlying_type_up->getBuiltinType());
    // FIXME: Type of underlying builtin is always `Int`. We correct it with
    // the very first enumerator's encoding if any.
    auto first_child = enum_type->findOneChild<PDBSymbolData>();
    if (first_child) {
      encoding = TranslateEnumEncoding(first_child->getValue().Type);
    }
    std::string name = enum_type->getName();
    uint64_t bytes = enum_type->getLength();
    CompilerType builtin_type;
    if (bytes > 0)
      builtin_type = GetBuiltinTypeForPDBEncodingAndBitSize(
           &m_ast, underlying_type_up.get(), encoding, bytes * 8);
    else
      builtin_type = m_ast.GetBasicType(eBasicTypeInt);
    // FIXME: PDB does not have information about scoped enumeration (Enum Class).
    // Set it false for now.
    bool isScoped = false;

    CompilerType ast_enum = m_ast.CreateEnumerationType(
        name.c_str(), tu_decl_ctx, decl, builtin_type, isScoped);
    auto enum_values = enum_type->findAllChildren<PDBSymbolData>();
    if (enum_values) {
      while (auto enum_value = enum_values->getNext()) {
        if (enum_value->getDataKind() != PDB_DataKind::Constant)
          continue;
        AddEnumValue(ast_enum, *enum_value);
      }
    }
    if (ClangASTContext::StartTagDeclarationDefinition(ast_enum))
      ClangASTContext::CompleteTagDeclarationDefinition(ast_enum);

    return std::make_shared<lldb_private::Type>(
        type.getSymIndexId(), m_ast.GetSymbolFile(), ConstString(name), bytes,
        nullptr, LLDB_INVALID_UID, lldb_private::Type::eEncodingIsUID, decl,
        ast_enum, lldb_private::Type::eResolveStateFull);
  } else if (auto type_def = llvm::dyn_cast<PDBSymbolTypeTypedef>(&type)) {
    lldb_private::Type *target_type =
        m_ast.GetSymbolFile()->ResolveTypeUID(type_def->getTypeId());
    if (!target_type)
      return nullptr;
    std::string name = type_def->getName();
    uint64_t bytes = type_def->getLength();
    CompilerType target_ast_type = target_type->GetFullCompilerType();
    CompilerDeclContext target_decl_ctx =
        m_ast.GetSymbolFile()->GetDeclContextForUID(target_type->GetID());
    CompilerType ast_typedef =
        m_ast.CreateTypedefType(target_ast_type, name.c_str(), target_decl_ctx);
    return std::make_shared<lldb_private::Type>(
        type_def->getSymIndexId(), m_ast.GetSymbolFile(), ConstString(name),
        bytes, nullptr, target_type->GetID(),
        lldb_private::Type::eEncodingIsTypedefUID, decl, ast_typedef,
        lldb_private::Type::eResolveStateFull);
  } else if (auto func_sig = llvm::dyn_cast<PDBSymbolTypeFunctionSig>(&type)) {
    auto arg_enum = func_sig->getArguments();
    uint32_t num_args = arg_enum->getChildCount();
    std::vector<CompilerType> arg_list;

    bool is_variadic = func_sig->isCVarArgs();
    // Drop last variadic argument.
    if (is_variadic)
      --num_args;
    for (int arg_idx = 0; arg_idx < num_args; arg_idx++) {
      auto arg = arg_enum->getChildAtIndex(arg_idx);
      if (!arg)
        break;
      lldb_private::Type *arg_type =
          m_ast.GetSymbolFile()->ResolveTypeUID(arg->getSymIndexId());
      // If there's some error looking up one of the dependent types of this
      // function signature, bail.
      if (!arg_type)
        return nullptr;
      CompilerType arg_ast_type = arg_type->GetFullCompilerType();
      arg_list.push_back(arg_ast_type);
    }
    lldbassert(arg_list.size() <= num_args);

    auto pdb_return_type = func_sig->getReturnType();
    lldb_private::Type *return_type =
        m_ast.GetSymbolFile()->ResolveTypeUID(pdb_return_type->getSymIndexId());
    // If there's some error looking up one of the dependent types of this
    // function signature, bail.
    if (!return_type)
      return nullptr;
    CompilerType return_ast_type = return_type->GetFullCompilerType();
    uint32_t type_quals = 0;
    if (func_sig->isConstType())
      type_quals |= clang::Qualifiers::Const;
    if (func_sig->isVolatileType())
      type_quals |= clang::Qualifiers::Volatile;
    CompilerType func_sig_ast_type = m_ast.CreateFunctionType(
        return_ast_type, arg_list.data(), arg_list.size(), is_variadic,
        type_quals);

    return std::make_shared<lldb_private::Type>(
        func_sig->getSymIndexId(), m_ast.GetSymbolFile(), ConstString(), 0,
        nullptr, LLDB_INVALID_UID, lldb_private::Type::eEncodingIsUID, decl,
        func_sig_ast_type, lldb_private::Type::eResolveStateFull);
  } else if (auto array_type = llvm::dyn_cast<PDBSymbolTypeArray>(&type)) {
    uint32_t num_elements = array_type->getCount();
    uint32_t element_uid = array_type->getElementType()->getSymIndexId();
    uint32_t bytes = array_type->getLength();

    lldb_private::Type *element_type =
        m_ast.GetSymbolFile()->ResolveTypeUID(element_uid);
    if (!element_type)
      return nullptr;
    CompilerType element_ast_type = element_type->GetFullCompilerType();
    CompilerType array_ast_type =
        m_ast.CreateArrayType(element_ast_type, num_elements, false);
    return std::make_shared<lldb_private::Type>(
        array_type->getSymIndexId(), m_ast.GetSymbolFile(), ConstString(),
        bytes, nullptr, LLDB_INVALID_UID, lldb_private::Type::eEncodingIsUID,
        decl, array_ast_type, lldb_private::Type::eResolveStateFull);
  } else if (auto *builtin_type = llvm::dyn_cast<PDBSymbolTypeBuiltin>(&type)) {
    PDB_BuiltinType builtin_kind = builtin_type->getBuiltinType();
    if (builtin_kind == PDB_BuiltinType::None)
      return nullptr;

    uint64_t bytes = builtin_type->getLength();
    Encoding encoding = TranslateBuiltinEncoding(builtin_kind);
    CompilerType builtin_ast_type = GetBuiltinTypeForPDBEncodingAndBitSize(
        &m_ast, builtin_type, encoding, bytes * 8);

    Type::EncodingDataType encoding_data_type = Type::eEncodingIsUID;
    if (builtin_type->isConstType()) {
      encoding_data_type = Type::eEncodingIsConstUID;
      builtin_ast_type = builtin_ast_type.AddConstModifier();
    }
    if (builtin_type->isVolatileType()) {
      encoding_data_type = Type::eEncodingIsVolatileUID;
      builtin_ast_type = builtin_ast_type.AddVolatileModifier();
    }
    auto type_name = GetPDBBuiltinTypeName(builtin_type, builtin_ast_type);

    return std::make_shared<lldb_private::Type>(
        builtin_type->getSymIndexId(), m_ast.GetSymbolFile(), type_name,
        bytes, nullptr, LLDB_INVALID_UID, encoding_data_type,
        decl, builtin_ast_type, lldb_private::Type::eResolveStateFull);
  } else if (auto *pointer_type = llvm::dyn_cast<PDBSymbolTypePointer>(&type)) {
    Type *pointee_type = m_ast.GetSymbolFile()->ResolveTypeUID(
        pointer_type->getPointeeType()->getSymIndexId());
    if (!pointee_type)
      return nullptr;

    CompilerType pointer_ast_type;
    Type::EncodingDataType encoding_data_type = Type::eEncodingIsPointerUID;
    if (pointer_type->isReference()) {
      encoding_data_type = Type::eEncodingIsLValueReferenceUID;
      pointer_ast_type =
          pointee_type->GetFullCompilerType().GetLValueReferenceType();
    } else
      pointer_ast_type = pointee_type->GetFullCompilerType().GetPointerType();

    return std::make_shared<lldb_private::Type>(
        pointer_type->getSymIndexId(), m_ast.GetSymbolFile(), ConstString(),
        pointer_type->getLength(), nullptr, LLDB_INVALID_UID,
        encoding_data_type, decl, pointer_ast_type,
        lldb_private::Type::eResolveStateFull);
  }
  return nullptr;
}

bool PDBASTParser::AddEnumValue(CompilerType enum_type,
                                const PDBSymbolData &enum_value) const {
  Declaration decl;
  Variant v = enum_value.getValue();
  std::string name = enum_value.getName();
  int64_t raw_value;
  switch (v.Type) {
  case PDB_VariantType::Int8:
    raw_value = v.Value.Int8;
    break;
  case PDB_VariantType::Int16:
    raw_value = v.Value.Int16;
    break;
  case PDB_VariantType::Int32:
    raw_value = v.Value.Int32;
    break;
  case PDB_VariantType::Int64:
    raw_value = v.Value.Int64;
    break;
  case PDB_VariantType::UInt8:
    raw_value = v.Value.UInt8;
    break;
  case PDB_VariantType::UInt16:
    raw_value = v.Value.UInt16;
    break;
  case PDB_VariantType::UInt32:
    raw_value = v.Value.UInt32;
    break;
  case PDB_VariantType::UInt64:
    raw_value = v.Value.UInt64;
    break;
  default:
    return false;
  }
  CompilerType underlying_type =
      m_ast.GetEnumerationIntegerType(enum_type.GetOpaqueQualType());
  uint32_t byte_size = m_ast.getASTContext()->getTypeSize(
      ClangUtil::GetQualType(underlying_type));
  return m_ast.AddEnumerationValueToEnumerationType(
      enum_type.GetOpaqueQualType(), underlying_type, decl, name.c_str(),
      raw_value, byte_size * 8);
}
