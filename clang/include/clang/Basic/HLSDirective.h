
/// \brief structure for directive option list. These options are read from
/// directive file.
#include<vector>
#include<string>
#include"llvm/ADT/StringRef.h"
#include"llvm/Support/raw_ostream.h"

using llvm::StringRef; 
using llvm::raw_ostream; 

struct OptionDesc {
  std::string Name;
  std::string Value;

};
struct PragmaDesc {
  std::string Name;
  std::vector<struct OptionDesc> OptionList; // optional
};

struct DirectiveDesc {
  std::string FunctionName;
  std::string Label;
  std::string FunctionLabel;
  std::string SourceFile;
  uint64_t SourceLine;
  uint64_t Id;
  struct PragmaDesc PragmaItem; //  1 on 1 map
  bool  FromSLX; 
  std::string InsertPosition;
  std::string IfCond; 
  bool success; 
  DirectiveDesc() :  SourceLine(0), Id(0), FromSLX(false),  success(false) { }
};

std::error_code ParseDirectiveList(StringRef buff, std::vector<DirectiveDesc> &directiveList); 
std::error_code DumpDirectiveList(raw_ostream &os, const std::vector<DirectiveDesc> &directiveList); 

