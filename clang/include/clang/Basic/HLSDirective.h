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

