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

#include"clang/Basic/HLSDirective.h"
#include"llvm/Support/JSON.h"
std::error_code ParseDirectiveList(StringRef DirectiveBuff, std::vector<DirectiveDesc> &DirectiveList) {
  llvm::Expected<llvm::json::Value> parseRet = llvm::json::parse(DirectiveBuff); 
  if (!parseRet) { 

    return std::error_code( 1, std::generic_category()); 
  }

  llvm::json::Array directiveArray = *(parseRet->getAsArray()); 

  for( int i = 0; i < directiveArray.size(); i++) { 
    llvm::json::Object &obj = *directiveArray[i].getAsObject(); 

    llvm::json::Array *optionArray = obj.getObject("pragma")->getArray("option"); 
    std::vector<struct OptionDesc> options;
    if (optionArray) { 
      for ( int j = 0 ; j < (*optionArray).size(); j++ ) { 
        StringRef name = (*optionArray)[j].getAsObject()->getString("name").getValue(); 
        StringRef value = (*optionArray)[j].getAsObject()->getString("value").getValue(); 
        OptionDesc option ; 
        option.Name = name; 
        option.Value = value; 
        options.push_back(option); 
      }
    }

    StringRef name = obj.getObject("pragma")->getString("name").getValue();; 
    PragmaDesc pragma ; 
    pragma.Name = name; 
    pragma.OptionList = std::move(options); 

    DirectiveDesc directive ; 

    directive.PragmaItem = pragma; 
    directive.InsertPosition = obj.getString("insert_position").getValue().str(); 
    directive.FunctionName = obj.getString("functionName").getValue().str(); 
    directive.Label = obj.getString("label").getValue().str(); 
    directive.FunctionLabel = obj.getString("functionLabel").getValue().str(); 
    directive.Id  = obj.getInteger("id").getValue(); 
    directive.SourceFile = obj.getString("sourceFile").getValue().str(); 
    directive.SourceLine = obj.getInteger("sourceLine").getValue(); 
    directive.FromSLX = obj.getBoolean("slx").getValue(); 
    directive.IfCond = obj.getString("ifcond").getValue().str(); 
    directive.success = obj.getBoolean("success").getValue(); 

    DirectiveList.push_back(directive); 
  }
  return std::error_code(); 
}

std::error_code DumpDirectiveList(raw_ostream &os, const std::vector<DirectiveDesc> &DirectiveList) 
{
  std::vector<llvm::json::Value> objs; 
  for( int i = 0; i < DirectiveList.size(); i++) { 
    llvm::json::Object obj; 
    const DirectiveDesc &directive = DirectiveList[i]; 

    llvm::json::Array optionsArray; 
    for ( int j = 0 ; j < directive.PragmaItem.OptionList.size(); j++ ) { 
      const OptionDesc &option = directive.PragmaItem.OptionList[j];
      llvm::json::Object optionObj; 
      optionObj.insert( {"name", option.Name}); 
      optionObj.insert( {"value", option.Value}); 
      optionsArray.push_back( llvm::json::Value(std::move(optionObj))); 
    }

    llvm::json::Object pragmaObj; 
    pragmaObj.insert({"name", directive.PragmaItem.Name});
    if (optionsArray.size() ) { 
      pragmaObj.insert({"option", std::move(optionsArray)}); 
    }

    obj.insert({"insert_position", directive.InsertPosition}); 
    obj.insert({"functionName", directive.FunctionName});
    obj.insert({"label", directive.Label});
    obj.insert({"functionLabel", directive.FunctionLabel});
    obj.insert({ "pragma", llvm::json::Value(std::move( pragmaObj)) } );
    obj.insert({"id", (int64_t)directive.Id});
    obj.insert({"sourceFile", directive.SourceFile});
    obj.insert({"sourceLine", (int64_t)directive.SourceLine});
    obj.insert({"slx", directive.FromSLX});
    obj.insert({"ifcond", directive.IfCond}); 
    obj.insert({"success", directive.success}); 

    objs.push_back(llvm::json::Value(std::move(obj))); 
  }

  llvm::json::Array all_directives( std::move(objs) ); 

  os << llvm::formatv("{0:2}", llvm::json::Value(std::move(all_directives))); 
  return std::error_code(); 
}
