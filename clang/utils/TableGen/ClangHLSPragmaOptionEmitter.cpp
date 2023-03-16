//===- ClangHLSPragmaOptionEmitter.cpp - Generate Xilinx HLS Pragma Option handling =-*- C++ -*--=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// And has the following additional copyright:
//
// (C) Copyright 2016-2022 Xilinx, Inc.
// All Rights Reserved.
//
//===----------------------------------------------------------------------===//
//
// These tablegen backends emit HLS pragma processing code
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/JSON.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/StringMatcher.h"
#include "llvm/TableGen/TableGenBackend.h"

using namespace llvm;

namespace clang {

// Generate HLS Pragma Option Parser Code
static void WritePragmaOption(const Record* Option, raw_ostream &OS) {
  const Record *Format = Option->getValueAsDef("Format");
  bool Optional = Option->getValueAsBit("Optional");
  std::string Name = Option->getValueAsString("Name");
  
  // Generate opt/req prefix, Present no such prefix
  if(Format->getName() != "PresentFormat" && Format->getName() != "EnumPresentFormat") {
    if(Optional) {
      OS << "opt";
    } else {
      OS << "req";
    }
  }

  // Generate static constuctor name for different Format type
  OS << StringSwitch<std::string>(Format->getName())
            .Case("PresentFormat", "presentId")
            .Case("EnumPresentFormat", "presentId")
            .Case("EnumExprFormat", "Enum")
            .Case("IntConstExprFormat", "ICEExpr")
            .Case("VariableExprFormat", "VarRefExpr")
            .Case("IDExprFormat", "Id")
            .Case("DoubleConstExprFormat", "DCEExpr") << "(\"" << Name << "\"";

  StringRef defaultVal = Option->isSubClassOf("DefaultString") ? Option->getValueAsString("Default") : "";
  if(Format->getName() == "PresentFormat" || Format->getName() == "EnumPresentFormat") {
    OS << ", " << Option->getValueAsInt("GroupID");   
  } else if(Option->isSubClassOf("OptIntDefaultConstExprOption")) {
    OS << ", " << Option->getValueAsInt("Default");
  } else if(Option->isSubClassOf("OptIDExprOption")) {
    if (!defaultVal.equals("")) { 
      OS << ", \"" << defaultVal<< "\""; 
    }
  } else if (Option->isSubClassOf("OptDoubleConstExprOption")) {
    if (!defaultVal.empty()) { 
      OS << ", " << defaultVal; 
    }
  } 

  // Generate enum value list
  if(Format->getName() == "EnumExprFormat") {
    std::vector<StringRef> Enums = Option->getValueAsListOfStrings("Enums");
    OS << ", {" << formatv("\"{0:$[\", \"]}\"", make_range(Enums.begin(), Enums.end())) << "}";
  }
    
  OS << ")";   
}
void LinkPragmaMode( const Record* Pragma, std::vector<Record*> &OptRecords) 
{ 
  //Linkd Modes Record
  if (Pragma->getValue("Modes")) { 
    DagInit *Modes = Pragma->getValueAsDag("Modes");
    DefInit * selector = dyn_cast<DefInit>(Modes->getOperator());
    Record *def = selector->getDef();
    if (def->getName().equals("modeSelect")) { 
      for ( int i = 0; i < Modes->getNumArgs(); i++) { 
        if (!isa<DefInit>(Modes->getArg(i))) { 
          PrintFatalError( Pragma->getLoc(), "unexpected, DAG arg , expect def Record");
        }
        Record *mode = cast<DefInit>(Modes->getArg(i))->getDef();
        std::vector<Record*> linkedOpts = mode->getValueAsListOfDefs("Options");
        for ( int i = 0; i < linkedOpts.size(); i++) { 
          //TODO, handle option conflict amonge different Modes
          OptRecords.push_back(linkedOpts[i]);
        }
      }
      std::vector<Record*> tmps; 

      for ( int i = 0; i < OptRecords.size(); i++ ) { 
        int j = 0; 
        for ( j = 0; j < tmps.size(); j++ ) { 
          StringRef optName = tmps[j]->getValueAsString("Name");
          StringRef optFormat = tmps[j]->getValueAsDef("Format")->getName();
          if ( optName == OptRecords[i]->getValueAsString("Name") &&
              optFormat == OptRecords[i]->getValueAsDef("Format")->getName()) { 
            break; 
          }
        }
        if ( j == tmps.size()) { 
          tmps.push_back( OptRecords[i]);
        }
      }
      for( int i = 0; i< tmps.size(); i++ ) { 
        OptRecords[i] = tmps[i];
      }
      OptRecords.resize( tmps.size());
    }
    else { 
      //TODO, support new DAG operator 
      PrintFatalError( Pragma->getLoc(), "unexpected, DAG operator , expect selectMode operator");
    }
  }
}

void SolveConflict( std::vector<Record*> & opts, std::vector<std::vector<Record*>> & conflicts) 
{
  SmallSet<Record*, 4> conflictSet;
  StringMap<std::vector<Record*>> conflictMap;  
  for ( int i = 0; i < opts.size(); i++ ) { 
    Record * opt = opts[i];
    StringRef Name = opt->getValueAsString("Name");
    conflictMap[Name].push_back(opt);
  }
  for ( auto &kv: conflictMap) { 
    if (kv.second.size() > 1) { 
      conflicts.emplace_back(kv.second);
      for( int i = 0; i < kv.second.size(); i++) { 
        conflictSet.insert(kv.second[i]);
      }
    }
  }
  std::vector<Record*> resolvedOpts;
  for ( int i = 0; i < opts.size(); i++ ) { 
    if (!conflictSet.count(opts[i])) { 
      resolvedOpts.push_back( opts[i] );
    }
  }
  for ( int i = 0; i < resolvedOpts.size(); i++ ) { 
    opts[i] = resolvedOpts[i];
  }
  opts.resize( resolvedOpts.size()); 
}


void EmitHLSPragmaParser(RecordKeeper &Records, raw_ostream &OS) {
  emitSourceFileHeader("Xilinx HLS pragma option parser", OS);

  std::vector<Record *> Pragmas = Records.getAllDerivedDefinitions("HLSPragma");

  for( auto *Pragma : Pragmas) {
    std::string PragmaName = Pragma->getValueAsString("Name");

    std::vector<Record*> OptRecords = Pragma->getValueAsListOfDefs("Options");
    LinkPragmaMode(Pragma, OptRecords);
    
    std::vector<std::vector<Record*>> conflicts;
    SolveConflict(OptRecords, conflicts);


    
    OS << "#define PRAGMA_HLS_" << StringRef(PragmaName).upper() << " \\\n";
    OS << "                 { \\\n";
    for(int i = 0; i < OptRecords.size(); i++ ) { 
      const Record* Option = OptRecords[i]; 
      OS << "                     ";
      WritePragmaOption(Option, OS);
      OS << ",";
      OS << " \\\n";   
    }
    for( int i = 0; i < conflicts.size(); i++ ) { 
      StringRef conflictName = conflicts[i][0]->getValueAsString("Name");
      OS << "                     ";
      OS << "optCallBackParser(\"" << conflictName <<"\", "; 
      OS << "Parse" << conflictName <<"OptionFor" << PragmaName << "),"; 
      OS << " \\\n";   
    }

    OS << "                 }\n\n";    
  }  
    
}

void EmitHLSPragmaJson(RecordKeeper &Records, raw_ostream &OS) {
  StringMap<StringRef> child2Parent; 
  std::vector<Record *> Pragmas = Records.getAllDerivedDefinitions("HLSPragma");
  json::Object RootObj;  
  for(const auto *Pragma : Pragmas) {
    StringRef PragmaName = Pragma->getValueAsString("Name");

    json::Object PragmaObj ; 

    StringRef Visibility = Pragma->getValueAsDef("Visibility")->getName(); 
    std::vector<json::Value>  Objects ; 

    for ( Init *init : *(Pragma->getValueAsListInit("Objects"))) { 
      assert( isa<DefInit>(init) && "unexpected" ); 
      Objects.push_back(json::Value(cast<DefInit>(init)->getDef()->getName()));
    };
    StringRef Desc = Pragma->getValueAsString("Desc");

    PragmaObj.insert( { "Visibility", Visibility} ); 
    PragmaObj.insert( { "Objects", json::Array(Objects)} );
    PragmaObj.insert( { "Desc" , Desc} ); 


    std::vector<Record*> OptRecords = Pragma->getValueAsListOfDefs("Options");
    LinkPragmaMode(Pragma, OptRecords);

    json::Object allOptions;

    if (Pragma->getValue("Modes")) { 
      PragmaObj.insert( { "ChildSelectOptionName", Pragma->getValueAsString("ModeOptionName") } ); 
      DagInit *Modes = Pragma->getValueAsDag("Modes");
      //for json dump, we only dump parent 
      DefInit * selector = dyn_cast<DefInit>(Modes->getOperator());
      if (selector->getDef()->getName().equals("modeSelect")) { 
        std::vector<json::Value> subPragmas; 
        for ( int i = 0; i < Modes->getNumArgs(); i++) { 
          if (!isa<DefInit>(Modes->getArg(i))) { 
            PrintFatalError( Pragma->getLoc(), "unexpected, DAG arg , expect def Record");
          }
          Record *mode = cast<DefInit>(Modes->getArg(i))->getDef();
          StringRef subPragmaName = mode->getValueAsString("Name");
          subPragmas.emplace_back(subPragmaName); 
          child2Parent[subPragmaName] = PragmaName; 
        }
        PragmaObj.insert( { "ChildList", json::Array(subPragmas) } );
        // TODO: write out ChildSelectMap of option-value -> child-name
      }
      else { 
        PrintFatalError( Pragma->getLoc(), "unexpected, DAG operator , expect selectMode operator");
      }
    }

    for(const auto *Option : OptRecords) {
      json::Object OptionObj;

      StringRef Name = Option->getValueAsString("Name"); 
      StringRef Format = Option->getValueAsDef("Format")->getName(); 
      bool Optional = Option->getValueAsBit("Optional"); 
      StringRef Desc = Option->getValueAsString("Desc"); 

      OptionObj.insert({"Format", Format}); 
      OptionObj.insert({"Desc", Desc});
      if (!Optional) {
        OptionObj.insert({"Optional", Optional});
      }

      StringRef Visibility = Option->getValueAsDef("Visibility")->getName();
      if (Visibility != "Public") {
        OptionObj.insert({"Visibility", Visibility});
      }
      StringRef UsageOfDefault = Option->getValueAsDef("UsageOfDefault")->getName();
      if (UsageOfDefault != "Explicit") {
        OptionObj.insert({"UsageOfDefault", UsageOfDefault});
      }

      if (Format.equals( "EnumPresentFormat")) { 
        OptionObj.insert( {"ParentName", Option->getValueAsString("ParentName")});
      }
      else if (Format.equals("EnumExprFormat") ) { 
        std::vector<StringRef> Enums = Option->getValueAsListOfStrings("Enums");
        std::vector<json::Value> enum_values;
        for (auto str: Enums) { 
          enum_values.emplace_back(str);
        }
        OptionObj.insert({"EnumValueList", json::Array(enum_values)}); 
      }

      //if (Option->isSubClassOf("PresentOption")) { 
      //  OptionObj.insert( {"PresentGroupID", Option->getValueAsInt("GroupID")});
      //}
      
      if (Option->isSubClassOf("DefaultString")) { 
        if (!Option->isSubClassOf("OptIDExprOption"))
          OptionObj.insert( {"Default", Option->getValueAsString("Default")});
      }
      else if (Option->isSubClassOf("DefaultInt")) {
        if (Option->isSubClassOf("OptUserVisibleIntDefaultConstExprOption")) { 
          OptionObj.insert( { "Default", Option->getValueAsInt("Default")} ); 
        }
      }
      allOptions.insert( {Name, std::move(OptionObj)} ); 
    }
    PragmaObj.insert( { "Options" , std::move(allOptions)} ); 

    RootObj.insert({PragmaName, std::move(PragmaObj) } ) ; 
  }

  for( auto  &kv: child2Parent) { 
    StringRef  subPragmaName= kv.getKey(); 
    StringRef  parentPragmaName = kv.getValue(); 

    json::Object *subPragmaJSON = nullptr; 
    if (json::Value *v = RootObj.get(subPragmaName)) { 
      subPragmaJSON = v->getAsObject(); 
    }

    if (!subPragmaJSON)  { 
      OS << formatv("{0:2}", json::Value(std::move(RootObj)));
      PrintFatalError( "unexpected, can not find subPragma" + subPragmaName );
    }
    subPragmaJSON->insert( { "ParentPragma", parentPragmaName}); 
  }


  OS << formatv("{0:2}", json::Value(std::move(RootObj)));
}

void EmitHLSPragmaPreprocessor(RecordKeeper &Records, raw_ostream &OS) {
  emitSourceFileHeader("Xilinx HLS pragma option preprocessor keyword filter", OS);

  OS << "#include \"llvm/ADT/StringRef.h\"\n";
  OS << "#include <map>\n#include <set>\n";
  OS << "namespace hls {\n";
  OS << "namespace pragma {\n";
  OS << "static std::map<std::string, std::set<std::string>> HLSPragmaKWFilter = {\n";
  std::vector<Record *> Pragmas = Records.getAllDerivedDefinitions("HLSPragma");

  for (auto *Pragma : Pragmas) {
    std::string PragmaName = Pragma->getValueAsString("Name").lower();
    OS << "\t{\"" << PragmaName << "\", {\"" << PragmaName << "\"";
    std::vector<Record*> OptRecords = Pragma->getValueAsListOfDefs("Options");
    LinkPragmaMode(Pragma, OptRecords);
    for (int i = 0; i < OptRecords.size(); i++) {
      const Record *Option = OptRecords[i];
      OS << ", \"" << Option->getValueAsString("Name").lower() << "\"";
 
      const Record *Format = Option->getValueAsDef("Format");
      if (Format->getName() == "EnumExprFormat") {
        std::vector<StringRef> Enums = Option->getValueAsListOfStrings("Enums");
        for (StringRef Name : Enums) {
          OS << ", \"" << Name.lower() << "\"";
        }
      }
    }
    OS << "}}, \n";
  }
  OS << "};\n";

  OS << "\nstatic std::string CurPragma = \"\";\n\n";
  OS << "static inline void setCurPragma(llvm::StringRef Name) {\n";
  OS << "\tCurPragma = Name.lower();\n";
  OS << "}\n\n";

  OS << "static inline bool shouldExpandMacro(llvm::StringRef Name) {\n";
  OS << "\tif (HLSPragmaKWFilter.count(CurPragma) == 0) return true;\n";
  OS << "\tstd::set<std::string> Filters = HLSPragmaKWFilter[CurPragma];\n";
  OS << "\treturn Filters.count(Name.lower()) == 0;\n";
  OS << "}\n\n";

  OS << "static inline void resetCurPragma() {\n";
  OS << "\tCurPragma = \"\";\n";
  OS << "}\n";

  OS << "} // end namespace pragma\n";
  OS << "} // end namespace hls\n";
}

} // end of anonymous namespace
