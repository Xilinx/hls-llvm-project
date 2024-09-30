// (C) Copyright 2016-2022 Xilinx, Inc.
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc.
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
// Xilinx's platform information. Please do not modify the content of this file.
//
//===----------------------------------------------------------------------===//

#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XILINXFPGAPlatformBasic.h"
#else
#include "XILINXFPGAPlatformBasic.h"
#endif

#include <iostream>
#include <algorithm>
#include <map>
#include <limits>
#include <cassert>

#include <sqlite3.h>


// WARNING: GRPSIZE must > IMPL_MAX
#define GRPSIZE 1000 

#define STR_ALL     "all"
#define STR_AUTO    "auto"
#define STR_FABRIC  "fabric"
#define STR_DSP     "dsp"
#define STR_FIFO    "fifo"
//#define STR_AUTOSRL "autosrl"
#define STR_MEMORY  "memory"

namespace 
{
std::string getLowerString(std::string str)
{
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
  return str;
}
bool iequals(const std::string& a, const std::string& b)
{
    unsigned int sz = a.size();
    if (b.size() != sz)
      return false;
    for (unsigned int i = 0; i < sz; ++i)
      if (tolower(a[i]) != tolower(b[i]))
        return false;
    return true;
}

std::vector<std::string> split(const std::string& str, const std::string& sep)
{
    std::vector<std::string> words;
    std::string::size_type start = 0, pos = 0;
    while ((pos = str.find_first_of(sep, start)) != std::string::npos) {
        if (pos != start) {
        words.push_back(str.substr(start, pos - start));
        }
        start = pos + 1;
    }
    if (start < str.size()) {
    words.push_back(str.substr(start));
    }
    return words;
}

class PFUserControl 
{
public:
  PFUserControl() { init(); }

  const std::vector<std::string> & getAllNames(bool isStorage) const
  {
    return isStorage ? m_storage.getKeys() : m_ops.getKeys();
  }
  const std::vector<std::string> & getAllImpls(bool isStorage) const
  {
    return isStorage ? m_storageimpls.getKeys() : m_opimpls.getKeys();
  }

  std::string getGroup(std::string name) const
  {
    return m_opgroups.getVal(name, false/*defaultIsKey*/);
  }
  std::string getDesc(std::string name, bool isStorage) const
  {
    return isStorage ? m_storage.getVal(name) : m_ops.getVal(name);
  }
  std::string getImplDesc(std::string name, bool isStorage) const
  {
    return isStorage ? m_storageimpls.getVal(name) : m_opimpls.getVal(name);
  }

  std::string getCoreImpl(std::string op, std::string impl, bool isStorage) const
  {
    if (isStorage)
    {
      return impl;
    }
    op = getLowerString(op);
    auto itor = m_fpdsp.find(op);
    if (itor != m_fpdsp.end())
    { 
      if (iequals(impl, STR_DSP))
      {
        const int val = itor->second;
        if (val & MAXDSP)
          return toString(MAXDSP);
        if (val & FULLDSP)
          return toString(FULLDSP);
        if (val & MEDDSP)
          return toString(MEDDSP);
      }
    }
    return impl;
  }

  bool isFPOp(std::string op) const
  {
    return (m_fpdsp.find(getLowerString(op)) != m_fpdsp.end());
  }

private:
  struct DataPairs 
  {
    const std::vector<std::string> & getKeys() const { return m_vals; }
    std::string getVal(std::string key, bool defaultIsKey = true) const 
    {
      auto itor = m_desc.find(getLowerString(key));
      if (itor != m_desc.end())
        return itor->second;
      return defaultIsKey ? key : "";
    }
    void addpair(const std::string & key, const std::string & val)
    {
      m_vals.push_back(key);
      if (val.size())
      {
        m_desc[key] = val;
      }
    }
  private:
    std::vector<std::string> m_vals;
    std::map<std::string,std::string> m_desc;
  };
  void addop(const std::string & value, const std::string & group, const std::string & desc)
  {
    m_ops.addpair(value, desc);
    m_opgroups.addpair(value, group);
  }
  void addopimpl(const std::string & value, const std::string & desc)
  {
    m_opimpls.addpair(value, desc);
  }
  void addstorage(const std::string & value, const std::string & desc)
  {
    m_storage.addpair(value, desc);
  }
  void addstorageimpl(const std::string & value, const std::string & desc)
  {
    m_storageimpls.addpair(value, desc);
  }

  // Enum used to encode
  enum FPDSP
  {
    FPDSP_UNKNOWN = 0,
    NODSP    = (1<<1),
    FULLDSP  = (1<<2),
    MEDDSP   = (1<<3),
    MAXDSP   = (1<<4),
    PRIMDSP  = (1<<5)
  };

  static std::string toString(FPDSP val)
  {
    switch(val)
    {
      case NODSP:  return STR_FABRIC;
      case FULLDSP: return "fulldsp";
      case MEDDSP:  return "meddsp";
      case MAXDSP:  return "maxdsp";
      case PRIMDSP: return "primitivedsp";
      default: return "";
    }
  }

  void fpdsp(const std::string & op, bool full, bool med, bool max, bool prim, bool nodsp)
  {
    unsigned val = 0;
    if (full)
      val |= FULLDSP;
    if (med)
      val |= MEDDSP;
    if (max)
      val |= MAXDSP;
    if (prim)
      val |= PRIMDSP;
    if (nodsp)
      val |= NODSP;
    m_fpdsp[op] = val;
  }

  void init();

private:
  std::map<std::string, int> m_fpdsp;
  DataPairs m_ops;
  DataPairs m_opgroups;
  DataPairs m_opimpls;
  DataPairs m_storage;
  DataPairs m_storageimpls;
};

void PFUserControl::init()
{
  addstorage(STR_FIFO,      "FIFO");
  addstorage("ram_1p",      "Single-port RAM");
  addstorage("ram_1wnr",    "RAM with 1 write port and n read ports, using n banks internally");
  addstorage("ram_2p",      "Dual-port RAM that allows read operations on one port and both read and write operations on the other port.");
  addstorage("ram_s2p",     "Dual-port RAM that allows read operations on one port and write operations on the other port.");
  addstorage("ram_t2p",     "True dual-port RAM with support for both read and write on both ports with read-first mode");
  addstorage("rom_1p",      "Single-port ROM");
  addstorage("rom_2p",      "Dual-port ROM");
  addstorage("rom_np",      "Multi-port ROM");
  //addstorage("pipo",        "Ping-Pong memory (merged/unmerged is selected by the scheduler automatically)");
  //addstorage("pipo_1bank",  "Merged Ping-Pong memory using 1 bank (producer and consumer limited to 1 port each)");
  //addstorage("pipo_nbank",  "Unmerged Ping-Pong memory using n separate memory banks for depth=n (each process gets as many ports as the memory offers)");
  //addstorage("smem_sync",   "Synchronized shared memory");
  //addstorage("smem_unsync", "Unsynchronized shared memory");

  //addstorageimpl(STR_ALL,  "All implementations");
  addstorageimpl(STR_AUTO,    "Automatic selection");
  //addstorageimpl(STR_AUTOSRL, "Automatic SRL");
  addstorageimpl("bram",      "Block RAM");
  addstorageimpl("bram_ecc",  "Block RAM with ECC mode");
  addstorageimpl("lutram",    "Distributed RAM");
  addstorageimpl("uram",      "Ultra RAM");
  addstorageimpl("uram_ecc",  "Ultra RAM with ECC mode");
  addstorageimpl(STR_MEMORY,  "Generic memory, vivado will choose implementation"); // VIVADO-DETERMINE
  addstorageimpl("srl",       "Shift Register Logic");

  addop("mul",    "integer", "integer multiplication operation");
  addop("add",    "integer", "integer add operation");
  addop("sub",    "integer", "integer subtraction operation");
  //addop("udiv",   "integer", "integer unsigned divide operation");
  //addop("sdiv",   "integer", "integer signed divide operation");
  //addop("srem",   "integer", "integer signed module operation");
  //addop("urem",   "integer", "integer unsigned module operation");
  addop("fadd",   "single", "single precision floating-point add operation");
  addop("fsub",   "single", "single precision floating-point subtraction operation");
  addop("fdiv",   "single", "single precision floating-point divide operation");
  addop("fexp",   "single", "single precision floating-point exponential operation");
  addop("flog",   "single", "single precision floating-point logarithmic operation");
  addop("fmul",   "single", "single precision floating-point multiplication operation");
  addop("frsqrt", "single", "single precision floating-point reciprocal square root operation");
  addop("frecip", "single", "single precision floating-point reciprocal operation");
  addop("fsqrt",  "single", "single precision floating-point square root operation");
  addop("dadd",   "double", "double precision floating-point add operation");
  addop("dsub",   "double", "double precision floating-point subtraction operation");
  addop("ddiv",   "double", "double precision floating-point divide operation");
  addop("dexp",   "double", "double precision floating-point exponential operation");
  addop("dlog",   "double", "double precision floating-point logarithmic operation");
  addop("dmul",   "double", "double precision floating-point multiplication operation");
  addop("drsqrt", "double", "double precision floating-point reciprocal square root operation");
  addop("drecip", "double", "double precision floating-point reciprocal operation");
  addop("dsqrt",  "double", "double precision floating-point square root operation");
  addop("hadd",   "half", "half precision floating-point add operation");
  addop("hsub",   "half", "half precision floating-point subtraction operation");
  addop("hdiv",   "half", "half precision floating-point divide operation");
  addop("hmul",   "half", "half precision floating-point multiplication operation");
  addop("hsqrt",  "half", "half precision floating-point square root operation");

  //addopimpl(STR_ALL,           "All implementations");
  //addopimpl(STR_AUTO,          "Plain RTL implementation");
  addopimpl(STR_DSP,           "Use DSP resources");
  addopimpl(STR_FABRIC,        "Use non-DSP resources");
  addopimpl(toString(MEDDSP),  "Floating Point IP Medium Usage of DSP resources");
  addopimpl(toString(FULLDSP), "Floating Point IP Full Usage of DSP resources");
  addopimpl(toString(MAXDSP),  "Floating Point IP Max Usage of DSP resources");
  addopimpl(toString(PRIMDSP), "Floating Point IP Primitive Usage of DSP resources");

  //       op       full    med    max   prim   nodsp
  fpdsp( "dmul",    true,  true,  true, false,  true );
  fpdsp( "fmul",    true,  true,  true,  true,  true );
  fpdsp( "hmul",    true, false,  true, false,  true );

  fpdsp( "dadd",    true, false, false, false,  true );
  fpdsp( "fadd",    true,  true, false,  true,  true );
  fpdsp( "hadd",    true,  true, false, false,  true );

  fpdsp( "dsub",    true, false, false, false,  true );
  fpdsp( "fsub",    true,  true, false,  true,  true );
  fpdsp( "hsub",    true,  true, false, false,  true );
  
  fpdsp( "ddiv",   false, false, false, false,  true );
  fpdsp( "fdiv",   false, false, false, false,  true );
  fpdsp( "hdiv",   false, false, false, false,  true );

  fpdsp( "dexp",   false,  true, false, false,  true );
  fpdsp( "fexp",   false,  true, false, false,  true );
  fpdsp( "hexp",    true, false, false, false, false );

  fpdsp( "dlog",   false,  true, false, false,  true );
  fpdsp( "flog",   false,  true, false, false,  true );
  fpdsp( "hlog",   false,  true, false, false,  true );

  fpdsp( "dsqrt",  false, false, false, false,  true );
  fpdsp( "fsqrt",  false, false, false, false,  true );
  fpdsp( "hsqrt",  false, false, false, false,  true );

  fpdsp( "drsqrt",  true, false, false, false,  true );
  fpdsp( "frsqrt",  true, false, false, false,  true );
  fpdsp( "hrsqrt", false, false, false, false,  true );

  fpdsp( "drecip",  true, false, false, false,  true );
  fpdsp( "frecip",  true, false, false, false,  true );
  fpdsp( "hrecip", false, false, false, false,  true );
}

const PFUserControl & getUserControlData()
{
  static PFUserControl mydata;
  return mydata;
}

} // end namespace

namespace platform
{

//std::string PlatformBasic::getAutoSrlStr() { return STR_AUTOSRL; }
std::string PlatformBasic::getAutoStr() { return STR_AUTO; }
std::string PlatformBasic::getAllStr() { return STR_ALL; }
std::string PlatformBasic::getFifoStr() { return STR_FIFO; }

void PlatformBasic::getAllConfigOpNames(std::vector<std::string> & names)
{
  auto allnames = getUserControlData().getAllNames(false/*isStorage*/);
  for (auto val : allnames) names.push_back(val);
}
void PlatformBasic::getAllBindOpNames(std::vector<std::string> & names)
{
  auto allnames = getUserControlData().getAllNames(false/*isStorage*/);
  for (auto val : allnames) names.push_back(val);
}

void PlatformBasic::getAllConfigStorageTypes(std::vector<std::string> & names)
{
  names.push_back(PlatformBasic::getFifoStr());
}
void PlatformBasic::getAllBindStorageTypes(std::vector<std::string> & names)
{
  auto allnames = getUserControlData().getAllNames(true/*isStorage*/);
  for (auto val : allnames) names.push_back(val);
}
void PlatformBasic::getAllInterfaceStorageTypes(std::vector<std::string> & names)
{
  auto allnames = getUserControlData().getAllNames(true/*isStorage*/);
  for (auto val : allnames) if (val != STR_FIFO) names.push_back(val);
}
void PlatformBasic::getAllBindOpImpls(std::vector<std::string> & impls)
{
  auto allimpls = getUserControlData().getAllImpls(false/*isStorage*/);
  for (auto val : allimpls) impls.push_back(val);
}
void PlatformBasic::getAllConfigOpImpls(std::vector<std::string> & impls)
{
  auto allimpls = getUserControlData().getAllImpls(false/*isStorage*/);
  // NOTE: config_op support "all" impl. However bind_op doesn't support. 
  impls.push_back(STR_ALL);
  for (auto val : allimpls) impls.push_back(val);
}
void PlatformBasic::getAllBindStorageImpls(std::vector<std::string> & impls)
{
  auto allimpls = getUserControlData().getAllImpls(true/*isStorage*/);
  for (auto val : allimpls) 
  {
  // NOTE: config_storage support "autosrl" impl. However, bind_storage doesn't support. 
  //  if (val != STR_AUTOSRL)
  //    impls.push_back(val);
    impls.push_back(val);
  }
}
void PlatformBasic::getAllConfigStorageImpls(std::vector<std::string> & impls)
{
  auto allimpls = getUserControlData().getAllImpls(true/*isStorage*/);
  impls.push_back(STR_ALL);
  for (auto val : allimpls) 
  {
  // NOTE: bind_storage support "auto" impl. However, config_storage doesn't support. in 2021.1
  // NOTE: 'config_storage' should also support "auto" impl in 2021.2
    if (val != "bram_ecc" && val != "uram_ecc")
      impls.push_back(val);
  }
}

std::string PlatformBasic::getOpNameDescription(std::string name)
{
  return getUserControlData().getDesc(name, false/*isStorage*/);
}
std::string PlatformBasic::getOpImplDescription(std::string impl)
{
  return getUserControlData().getImplDesc(impl, false/*isStorage*/);
}
std::string PlatformBasic::getStorageTypeDescription(std::string name)
{
  return getUserControlData().getDesc(name, true/*isStorage*/);
}
std::string PlatformBasic::getStorageImplDescription(std::string impl)
{
  return getUserControlData().getImplDesc(impl, true/*isStorage*/);
}
std::string PlatformBasic::getOpNameGroup(std::string name)
{
  return getUserControlData().getGroup(name);
}

// NOTE: This function is GUI API.
void PlatformBasic::getOpNameImpls(std::string opName, std::vector<std::string> & impls, bool forBind)
{
  std::vector<std::string> allimpls;
  if (forBind)
  {
    getAllBindOpImpls(allimpls);
  }
  else
  {
    getAllConfigOpImpls(allimpls);
  }
  for (auto impl : allimpls)
  {
    if (impl == STR_ALL)
    {
        impls.push_back(STR_ALL);
    }
    else if (PlatformBasic::getPublicCore(opName, impl, false/*isStorage*/))
    {
        impls.push_back(impl);
    }
  }
}

// NOTE: This function is GUI API.
void PlatformBasic::getStorageTypeImpls(std::string storageType, std::vector<std::string> & impls, bool forBind)
{
  std::vector<std::string> allimpls;
  if (forBind)
    getAllBindStorageImpls(allimpls);
  else
    getAllConfigStorageImpls(allimpls);
  for (auto impl : allimpls)
  {
    //if (impl == STR_AUTOSRL)
    //{
    //    impls.push_back(STR_AUTOSRL);
    //} 
    //else if (PlatformBasic::getPublicCore(storageType, impl, true/*isStorage*/))
    if (PlatformBasic::getPublicCore(storageType, impl, true/*isStorage*/))
    {
        impls.push_back(impl);
    }

  }
}

PlatformBasic::CoreBasic* PlatformBasic::getMemoryFromOpImpl(const std::string& type, const std::string& impl) const
{
    auto implCode = getMemoryImpl(type, impl);
    return getCoreFromOpImpl(OP_MEMORY, implCode);
}

PlatformBasic::IMPL_TYPE PlatformBasic::getMemoryImpl(const std::string& type, const std::string& implName) const
{
    IMPL_TYPE impl = UNSUPPORTED;
    std::string name = getLowerString(type);
    if (implName.size())
    {
        name += "_" + getLowerString(implName);
    }
    auto it = mImplStr2Enum.find(name);
    if(it != mImplStr2Enum.end())
  {
      impl = it->second;
  }
  return impl;
}

PlatformBasic::CoreBasic* PlatformBasic::getPublicCore(std::string name, std::string impl, bool isStorage)
{
  PlatformBasic::CoreBasic * coreBasic = nullptr;
  std::string coreimpl = getUserControlData().getCoreImpl(name, impl, isStorage);
  if (isStorage)
  {
    coreBasic = getInstance()->getMemoryFromOpImpl(name, coreimpl);
  }
  else 
  {
    coreBasic = getInstance()->getCoreFromOpImpl(name, coreimpl);
  }

  if (coreBasic && !coreBasic->isPublic())
  {
    coreBasic = nullptr;
  }
  return coreBasic;
}

PlatformBasic::CoreBasic::CoreBasic(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl, int maxLat, int minLat, 
                     std::string name, bool isPublic):
        mIsPublic(isPublic), mOp(op), mImpl(impl), mMaxLatency(maxLat), mMinLatency(minLat), mName(name)
{}

std::pair<PlatformBasic::MEMORY_TYPE, PlatformBasic::MEMORY_IMPL> PlatformBasic::CoreBasic::getMemoryTypeImpl() const
{
  return getMemoryTypeImpl(mImpl);
}

std::pair<PlatformBasic::MEMORY_TYPE, PlatformBasic::MEMORY_IMPL> PlatformBasic::CoreBasic::getMemoryTypeImpl(PlatformBasic::IMPL_TYPE impl)
{
    std::pair<PlatformBasic::MEMORY_TYPE, PlatformBasic::MEMORY_IMPL> memoryPair(MEMORY_UNSUPPORTED, MEMORY_IMPL_UNSUPPORTED);
    switch(impl)
    {
        case PlatformBasic::SHIFTREG_AUTO:
            memoryPair = std::make_pair(MEMORY_SHIFTREG, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::REGISTER_AUTO:
            memoryPair = std::make_pair(MEMORY_REGISTER, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::XPM_MEMORY_AUTO:
            memoryPair = std::make_pair(MEMORY_XPM_MEMORY, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::XPM_MEMORY_DISTRIBUTE:
            memoryPair = std::make_pair(MEMORY_XPM_MEMORY, MEMORY_IMPL_DISTRIBUTE);
            break;
        case PlatformBasic::XPM_MEMORY_BLOCK:
            memoryPair = std::make_pair(MEMORY_XPM_MEMORY, MEMORY_IMPL_BLOCK);
            break;
        case PlatformBasic::XPM_MEMORY_URAM:
            memoryPair = std::make_pair(MEMORY_XPM_MEMORY, MEMORY_IMPL_URAM);
            break;
        case PlatformBasic::FIFO_MEMORY:
            memoryPair = std::make_pair(MEMORY_FIFO, MEMORY_IMPL_MEMORY);
            break;
        case PlatformBasic::FIFO_SRL:
            memoryPair = std::make_pair(MEMORY_FIFO, MEMORY_IMPL_SRL);
            break;
        case PlatformBasic::FIFO_BRAM:
            memoryPair = std::make_pair(MEMORY_FIFO, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::FIFO_URAM:
            memoryPair = std::make_pair(MEMORY_FIFO, MEMORY_IMPL_URAM);
            break;
        case PlatformBasic::FIFO_LUTRAM:
            memoryPair = std::make_pair(MEMORY_FIFO, MEMORY_IMPL_LUTRAM);
            break;
        case PlatformBasic::RAM_AUTO:
            memoryPair = std::make_pair(MEMORY_RAM, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::RAM_1WNR:
            memoryPair = std::make_pair(MEMORY_RAM_1WNR, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::RAM_1WNR_LUTRAM:
            memoryPair = std::make_pair(MEMORY_RAM_1WNR, MEMORY_IMPL_LUTRAM);
            break;
        case PlatformBasic::RAM_1WNR_BRAM:
            memoryPair = std::make_pair(MEMORY_RAM_1WNR, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::RAM_1WNR_URAM:
            memoryPair = std::make_pair(MEMORY_RAM_1WNR, MEMORY_IMPL_URAM);
            break;
        case PlatformBasic::NP_MEMORY:
            memoryPair = std::make_pair(MEMORY_NP_MEMORY, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::NP_MEMORY_LUTRAM:
            memoryPair = std::make_pair(MEMORY_NP_MEMORY, MEMORY_IMPL_LUTRAM);
            break;
        case PlatformBasic::NP_MEMORY_BRAM:
            memoryPair = std::make_pair(MEMORY_NP_MEMORY, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::NP_MEMORY_URAM:
            memoryPair = std::make_pair(MEMORY_NP_MEMORY, MEMORY_IMPL_URAM);
            break;
        case PlatformBasic::RAM_1P_AUTO:
            memoryPair = std::make_pair(MEMORY_RAM_1P, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::RAM_1P_LUTRAM:
            memoryPair = std::make_pair(MEMORY_RAM_1P, MEMORY_IMPL_LUTRAM);
            break;
        case PlatformBasic::RAM_1P_BRAM:
            memoryPair = std::make_pair(MEMORY_RAM_1P, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::RAM_1P_URAM:
            memoryPair = std::make_pair(MEMORY_RAM_1P, MEMORY_IMPL_URAM);
            break;
        case PlatformBasic::RAM_2P_AUTO:
            memoryPair = std::make_pair(MEMORY_RAM_2P, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::RAM_2P_LUTRAM:
            memoryPair = std::make_pair(MEMORY_RAM_2P, MEMORY_IMPL_LUTRAM);
            break;
        case PlatformBasic::RAM_2P_BRAM:
            memoryPair = std::make_pair(MEMORY_RAM_2P, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::RAM_2P_URAM:
            memoryPair = std::make_pair(MEMORY_RAM_2P, MEMORY_IMPL_URAM);
            break;
        case PlatformBasic::RAM_T2P_BRAM:
            memoryPair = std::make_pair(MEMORY_RAM_T2P, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::RAM_T2P_URAM:
            memoryPair = std::make_pair(MEMORY_RAM_T2P, MEMORY_IMPL_URAM);
            break;
        case PlatformBasic::RAM_S2P_LUTRAM:
            memoryPair = std::make_pair(MEMORY_RAM_S2P, MEMORY_IMPL_LUTRAM);
            break;
        case PlatformBasic::RAM_S2P_BRAM:
            memoryPair = std::make_pair(MEMORY_RAM_S2P, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::RAM_S2P_URAM:
            memoryPair = std::make_pair(MEMORY_RAM_S2P, MEMORY_IMPL_URAM);
            break;
        case PlatformBasic::RAM_S2P_BRAM_ECC:
            memoryPair = std::make_pair(MEMORY_RAM_S2P, MEMORY_IMPL_BRAM_ECC);
            break;
        case PlatformBasic::RAM_S2P_URAM_ECC:
            memoryPair = std::make_pair(MEMORY_RAM_S2P, MEMORY_IMPL_URAM_ECC);
            break;
        case PlatformBasic::ROM_AUTO:
            memoryPair = std::make_pair(MEMORY_ROM, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::ROM_1P_AUTO:
            memoryPair = std::make_pair(MEMORY_ROM_1P, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::ROM_1P_LUTRAM:
            memoryPair = std::make_pair(MEMORY_ROM_1P, MEMORY_IMPL_LUTRAM);
            break;
        case PlatformBasic::ROM_1P_BRAM:
            memoryPair = std::make_pair(MEMORY_ROM_1P, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::ROM_1P_1S_AUTO:
            memoryPair = std::make_pair(MEMORY_ROM_1P_1S, MEMORY_IMPL_AUTO);
            break;

        case PlatformBasic::ROM_NP_LUTRAM:
            memoryPair = std::make_pair(MEMORY_ROM_NP, MEMORY_IMPL_LUTRAM);
            break;
        case PlatformBasic::ROM_NP_BRAM:
            memoryPair = std::make_pair(MEMORY_ROM_NP, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::ROM_2P_AUTO:
            memoryPair = std::make_pair(MEMORY_ROM_2P, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::ROM_2P_LUTRAM:
            memoryPair = std::make_pair(MEMORY_ROM_2P, MEMORY_IMPL_LUTRAM);
            break;
        case PlatformBasic::ROM_2P_BRAM:
            memoryPair = std::make_pair(MEMORY_ROM_2P, MEMORY_IMPL_BRAM);
            break;
        case PlatformBasic::RAM3S_AUTO:
            memoryPair = std::make_pair(MEMORY_RAM3S, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::RAM4S_AUTO:
            memoryPair = std::make_pair(MEMORY_RAM4S, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::RAM5S_AUTO:
            memoryPair = std::make_pair(MEMORY_RAM5S, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::RAM_COREGEN_AUTO:
            memoryPair = std::make_pair(MEMORY_RAM_COREGEN, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::SPRAM_COREGEN_AUTO:
            memoryPair = std::make_pair(MEMORY_SPRAM_COREGEN, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::ROM3S_AUTO:
            memoryPair = std::make_pair(MEMORY_ROM3S, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::ROM4S_AUTO:
            memoryPair = std::make_pair(MEMORY_ROM4S, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::ROM5S_AUTO:
            memoryPair = std::make_pair(MEMORY_ROM5S, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::ROM_COREGEN_AUTO:
            memoryPair = std::make_pair(MEMORY_ROM_COREGEN, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::SPROM_COREGEN_AUTO:
            memoryPair = std::make_pair(MEMORY_SPROM_COREGEN, MEMORY_IMPL_AUTO);
            break;
        case PlatformBasic::SPWOM_AUTO:
            memoryPair = std::make_pair(MEMORY_SPWOM, MEMORY_IMPL_AUTO);
            break;
		// for NOIMPL
		case PlatformBasic::NOIMPL_SHIFTREG:
			memoryPair = std::make_pair(MEMORY_SHIFTREG, MEMORY_NOIMPL);
			break;
        case PlatformBasic::NOIMPL_XPM_MEMORY:
            memoryPair = std::make_pair(MEMORY_XPM_MEMORY, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_FIFO:
            memoryPair = std::make_pair(MEMORY_FIFO, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM_1WNR:
            memoryPair = std::make_pair(MEMORY_RAM_1WNR, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM_1P:
            memoryPair = std::make_pair(MEMORY_RAM_1P, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM_2P:
            memoryPair = std::make_pair(MEMORY_RAM_2P, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM_2P_1S:
            memoryPair = std::make_pair(MEMORY_RAM_2P_1S, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM_T2P:
            memoryPair = std::make_pair(MEMORY_RAM_T2P, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM_S2P:
            memoryPair = std::make_pair(MEMORY_RAM_S2P, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_ROM:
            memoryPair = std::make_pair(MEMORY_ROM, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_ROM_1P:
            memoryPair = std::make_pair(MEMORY_ROM_1P, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_ROM_1P_1S:
            memoryPair = std::make_pair(MEMORY_ROM_1P_1S, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_ROM_NP:
            memoryPair = std::make_pair(MEMORY_ROM_NP, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_ROM_2P:
            memoryPair = std::make_pair(MEMORY_ROM_2P, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM3S:
            memoryPair = std::make_pair(MEMORY_RAM3S, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM4S:
            memoryPair = std::make_pair(MEMORY_RAM4S, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM5S:
            memoryPair = std::make_pair(MEMORY_RAM5S, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_RAM_COREGEN:
            memoryPair = std::make_pair(MEMORY_RAM_COREGEN, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_SPRAM_COREGEN:
            memoryPair = std::make_pair(MEMORY_SPRAM_COREGEN, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_ROM3S:
            memoryPair = std::make_pair(MEMORY_ROM3S, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_ROM4S:
            memoryPair = std::make_pair(MEMORY_ROM4S, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_ROM5S:
            memoryPair = std::make_pair(MEMORY_ROM5S, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_ROM_COREGEN:
            memoryPair = std::make_pair(MEMORY_ROM_COREGEN, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_SPROM_COREGEN:
            memoryPair = std::make_pair(MEMORY_SPROM_COREGEN, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_SPROMD_ASYNC:
            memoryPair = std::make_pair(MEMORY_SPROMD_ASYNC, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_DPWOM:
            memoryPair = std::make_pair(MEMORY_DPWOM, MEMORY_NOIMPL);
            break;
        case PlatformBasic::NOIMPL_SPWOM:
            memoryPair = std::make_pair(MEMORY_SPWOM, MEMORY_NOIMPL);
            break;
        default: break;
    };

    return memoryPair;
}

bool PlatformBasic::CoreBasic::isRAM() const
{
    return  getMemoryType() == PlatformBasic::MEMORY_RAM || 
            getMemoryType() == PlatformBasic::MEMORY_RAM_1WNR || 
            getMemoryType() == PlatformBasic::MEMORY_RAM_1P ||
            getMemoryType() == PlatformBasic::MEMORY_RAM_2P ||
            getMemoryType() == PlatformBasic::MEMORY_RAM_T2P ||
            getMemoryType() == PlatformBasic::MEMORY_RAM_S2P ||
            getMemoryType() == PlatformBasic::MEMORY_XPM_MEMORY;
}

bool PlatformBasic::CoreBasic::isROM() const
{
    return  getMemoryType() == PlatformBasic::MEMORY_ROM || 
            getMemoryType() == PlatformBasic::MEMORY_ROM_1P ||
            getMemoryType() == PlatformBasic::MEMORY_ROM_NP ||
            getMemoryType() == PlatformBasic::MEMORY_ROM_2P;
}

bool PlatformBasic::CoreBasic::isNPROM() const
{
    return  getMemoryType() == PlatformBasic::MEMORY_ROM || 
            getMemoryType() == PlatformBasic::MEMORY_ROM_NP;
}


bool PlatformBasic::CoreBasic::isEccRAM() const
{
    return (getImpl() == PlatformBasic::RAM_S2P_BRAM_ECC ||
            getImpl() == PlatformBasic::RAM_S2P_URAM_ECC);
}

bool PlatformBasic::CoreBasic::supportByteEnable() const
{   
    // support ByteWriteEnable : NON-ECC BRAM, NON-ECC URAM, LUTRAM, AUTORAM.
    // Note : 1) byte write enable is not supported with ECC
    //        2) byte write enable is not supported in xpm memory currently.
    return  (isRAM() && getMemoryType() != PlatformBasic::MEMORY_XPM_MEMORY ) && (   
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_AUTO ||
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BRAM || 
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_LUTRAM ||
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_URAM);
}

bool PlatformBasic::CoreBasic::isInitializable() const
{
    // support initialization : all RAMs/ROMs except URAM/ECC URAM
    return  isROM() || (  isRAM() && (   
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_AUTO || 
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BRAM || 
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BRAM_ECC ||
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_LUTRAM ||
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BLOCK ||
                getMemoryImpl() == PlatformBasic::MEMORY_IMPL_DISTRIBUTE || 
                (getMemoryImpl() == PlatformBasic::MEMORY_IMPL_URAM && PlatformBasic::getInstance()->isVersal()) || 
                (getMemoryImpl() == PlatformBasic::MEMORY_IMPL_URAM_ECC && PlatformBasic::getInstance()->isVersal()) ));
}

// class PlatformBasic
PlatformBasic::PlatformBasic()
{
}

PlatformBasic::~PlatformBasic()
{
    for(auto it = mCoreBasicMap.begin(); it != mCoreBasicMap.end(); ++it)
    {
        delete it->second;
    }
    mCoreBasicMap.clear();
    for (auto it = mCoreNameMapInCompleteRepository.begin(); it != mCoreNameMapInCompleteRepository.end(); ++it) {
        auto coreBasics = it->second;
        for (auto *cb: coreBasics) {
            delete cb;
        }
    }
    mCoreNameMapInCompleteRepository.clear();
}

static std::string default_db_file  = "";
void SetPlatformDbFile( std::string db_file) 
{
    default_db_file = db_file;
}
static std::string g_device_resource_info  = "";
void SetPlatformDeviceResourceInfo( std::string resource_info) 
{
    g_device_resource_info = resource_info;
}
static std::string g_device_name_info  = "";
void SetPlatformDeviceNameInfo( std::string deviceName) 
{
    g_device_name_info = deviceName;
}
std::string GetPlatformDeviceNameInfo ()
{
    return g_device_name_info;
}

PlatformBasic* PlatformBasic::getInstance()
{
   static PlatformBasic *pf = NULL;
    if( pf ) { 
      return pf;
    }
    else { 
        pf = new PlatformBasic();
        return pf;
    }
}

std::vector<PlatformBasic::CoreBasic*> PlatformBasic::getCoreFromName(const std::string& coreName) const
{
    std::vector<PlatformBasic::CoreBasic*> cores;
    auto it = mCoreNameMap.find(getLowerString(coreName));
    if(it != mCoreNameMap.end())
    {
        cores = it->second;
    }
    return cores;
}

PlatformBasic::CoreBasic* PlatformBasic::getCoreFromOpImpl(const std::string& op, const std::string& impl) const
{
    auto opCode = getOpFromName(op);
    auto implCode = getImplFromName(impl);

    return getCoreFromOpImpl(opCode, implCode);
}

PlatformBasic::CoreBasic* PlatformBasic::getCoreFromOpImpl(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) const
{
    if(op != OP_UNSUPPORTED && impl != UNSUPPORTED)
    {
        auto it = mCoreBasicMap.find(op * GRPSIZE + impl);
        if(it != mCoreBasicMap.end())
        {
            return it->second;
        }
    }
    return NULL;
}

PlatformBasic::CoreBasic* PlatformBasic::getMemoryFromTypeImpl(PlatformBasic::MEMORY_TYPE type, PlatformBasic::MEMORY_IMPL impl) const
{
    for(auto it = mCoreBasicMap.begin(); it != mCoreBasicMap.end(); ++it)
    {
        auto coreBasic = it->second;
        if(coreBasic->getMemoryTypeImpl() == std::make_pair(type, impl))
        {
            return coreBasic;
        }
    }
    return NULL;
}

std::vector<PlatformBasic::CoreBasic*> PlatformBasic::getPragmaCoreBasics(OP_TYPE op, IMPL_TYPE impl, int latency) const
{
    std::vector<PlatformBasic::CoreBasic*> cores;
    if(isNOIMPL(impl))
    {
        if(op == OP_MEMORY)
        {
            auto memType = static_cast<MEMORY_TYPE>(impl);
            cores = getCoresFromMemoryType(getMemTypeName(memType));
        }
        else
        {
            cores = getCoresFromOp(getOpName(op));
        }
    }
    else
    {
        cores.push_back(getCoreFromOpImpl(op, impl));
    }
    auto notMatchLatency = [latency](CoreBasic* core)
    {
      if (!core) return true;
      return !core->isValidLatency(latency);
    };
    cores.erase(std::remove_if(cores.begin(), cores.end(), notMatchLatency), cores.end());

    return cores;
}



std::vector<PlatformBasic::CoreBasic*> PlatformBasic::getCoresFromOp(const std::string& op) const
{
    std::vector<PlatformBasic::CoreBasic*> cores;
    auto opCode = getOpFromName(op);
    if(opCode != OP_UNSUPPORTED)
    {
        const int lbound = opCode * GRPSIZE;
        for(auto it = mCoreBasicMap.lower_bound(lbound), ub = mCoreBasicMap.upper_bound(lbound + GRPSIZE -1); it != ub; ++it)
        {
            if(it->second->isPublic())
            {
                cores.push_back(it->second);
            }
        }
    }
    return cores;
}

std::vector<PlatformBasic::CoreBasic*> PlatformBasic::getCoresFromMemoryType(const std::string& type) const
{
    std::vector<PlatformBasic::CoreBasic*> cores;
    auto memType = getMemTypeFromName(type);
    for(auto it = mCoreBasicMap.begin(); it != mCoreBasicMap.end(); ++it)
    {
        auto coreBasic = it->second;
        if(coreBasic->isPublic() && coreBasic->getMemoryType() == memType)
        {
            cores.push_back(coreBasic);
        }
    }
    return cores;
}

std::vector<PlatformBasic::CoreBasic*> PlatformBasic::getAllCores() const
{
    std::vector<PlatformBasic::CoreBasic*> cores;
    for(auto it = mCoreBasicMap.begin(); it != mCoreBasicMap.end(); ++it)
    {
        cores.push_back(it->second);
    }
    return cores;
}

std::vector<std::pair<PlatformBasic::OP_TYPE, PlatformBasic::IMPL_TYPE>> PlatformBasic::getOpImplFromCoreName(const std::string& coreName) const
{
    std::vector<std::pair<OP_TYPE, IMPL_TYPE>> opImpls;
    // special for DSP48
    //if(iequals(coreName, "dsp48"))
    //{
    //    opImpls.push_back(std::make_pair(OP_MUL, DSP));
    //    return opImpls;
    //}
    std::vector<PlatformBasic::CoreBasic*> cores;
    auto it = mCoreNameMap.find(getLowerString(coreName));
    if(it != mCoreNameMap.end())
    {
        cores = it->second;
    }
    for(auto& core : cores)
    {
        opImpls.push_back(core->getOpAndImpl());
    }
    return opImpls;
}

std::vector<std::pair<PlatformBasic::OP_TYPE, PlatformBasic::IMPL_TYPE>> PlatformBasic::getOpImplFromCoreNameInCompleteRepository(const std::string& coreName) const
{
    std::vector<std::pair<OP_TYPE, IMPL_TYPE>> opImpls;

    std::vector<PlatformBasic::CoreBasic*> coreBasics;
    auto it = mCoreNameMapInCompleteRepository.find(getLowerString(coreName));
    if(it != mCoreNameMapInCompleteRepository.end())
    {
        coreBasics = it->second;
    }
    for(auto& coreBasic : coreBasics)
    {
        opImpls.push_back(coreBasic->getOpAndImpl());
    }
    return opImpls;
}

bool PlatformBasic::isPublicOp(OP_TYPE op) const
{
    bool isPublic = false;
    for(auto it = mCoreBasicMap.begin(); it != mCoreBasicMap.end(); ++it)
    {
        auto coreBasic = it->second;
        if(coreBasic->getOp() == op && coreBasic->isPublic())
        {
            isPublic = true;
            break;
        }
    }
    return isPublic;
}

bool PlatformBasic::isPublicType(MEMORY_TYPE type) const
{
    bool isPublic = false;
    if(type == MEMORY_UNSUPPORTED) return isPublic;
    for(auto it = mCoreBasicMap.begin(); it != mCoreBasicMap.end(); ++it)
    {
        auto coreBasic = it->second;
        if(coreBasic->getMemoryType() == type && coreBasic->isPublic())
        {
            isPublic = true;
            break;
        }
    }
    return isPublic;
}

std::pair<PlatformBasic::OP_TYPE, PlatformBasic::IMPL_TYPE> PlatformBasic::getOpImplFromStr(const std::string& opStr, const std::string& implStr) const
{
    OP_TYPE op = getOpFromName(opStr);
    IMPL_TYPE impl = UNSUPPORTED;
    if(op != OP_UNSUPPORTED)
    {
        // to support fpdsp, and impl=all
        auto coreImpl = getUserControlData().getCoreImpl(opStr, implStr, false);
        if(coreImpl.empty())
        {
            impl = NOIMPL; 
        }
        else
        {
            impl = getImplFromName(coreImpl);
        }
    }
    return std::make_pair(op, impl);
}

std::pair<PlatformBasic::OP_TYPE, PlatformBasic::IMPL_TYPE> PlatformBasic::verifyBindOp(const std::string& opStr, const std::string& implStr) const
{
    auto Res = getOpImplFromStr(opStr, implStr);
    if (!isNOIMPL(Res.second)) {
        auto coreBasic = getCoreFromOpImpl(Res.first, Res.second);
        if(coreBasic == NULL || !coreBasic->isPublic())
        {
            Res.first = OP_UNSUPPORTED;
            Res.second = UNSUPPORTED;
        }
    }

    return Res;
}

std::pair<PlatformBasic::OP_TYPE, PlatformBasic::IMPL_TYPE> PlatformBasic::getStorageTypeImplFromStr(const std::string& type, const std::string& implName) const
{
    OP_TYPE op = OP_MEMORY;
    IMPL_TYPE impl = UNSUPPORTED;
    if(implName.empty() || getMemImplFromName(getLowerString(implName)) == MEMORY_IMPL_AUTO)
    {
        // Use NOIMPL 
        auto memType = getMemTypeFromName(type);
        if(isPublicType(memType))
        {
            impl = static_cast<IMPL_TYPE>(memType);
        }
    }
    else
    {
        auto it = mImplStr2Enum.find(getLowerString(type + "_" + implName));
        if(it != mImplStr2Enum.end())
        {
            impl = it->second;
        }
    }

    if(impl == UNSUPPORTED)
    {
        op = OP_UNSUPPORTED;
    }

    return std::make_pair(op, impl);
}

std::pair<PlatformBasic::OP_TYPE, PlatformBasic::IMPL_TYPE> PlatformBasic::verifyBindStorage(const std::string& type, const std::string& implName) const
{
    auto Res = getStorageTypeImplFromStr(type, implName);
    
    if (!isNOIMPL(Res.second)) {
        auto coreBasic = getCoreFromOpImpl(Res.first, Res.second);
        if(coreBasic == NULL || !coreBasic->isPublic()) {
            Res.first = OP_UNSUPPORTED;
            Res.second = UNSUPPORTED;
        }
    }
    return Res;
}

bool PlatformBasic::verifyInterfaceStorage(std::string storageType, std::pair<OP_TYPE, IMPL_TYPE> * coreEnums) const
{
  storageType = getLowerString(storageType);

  std::vector<std::string> alltypes;
  getAllInterfaceStorageTypes(alltypes);
  if (std::find(alltypes.begin(), alltypes.end(), storageType) == alltypes.end())
  {
    return false;
  }

  auto memType = getMemTypeFromName(storageType);
  if(!isPublicType(memType))
  {
    return false;
  }

  OP_TYPE op = OP_MEMORY;
  IMPL_TYPE impl = static_cast<IMPL_TYPE>(memType);
  if (coreEnums)
  {
    *coreEnums = std::make_pair(op, impl);
  }
  return true;
}


bool PlatformBasic::isValidLatency(OP_TYPE op, IMPL_TYPE impl, int latency) const
{
  std::pair<int, int> implRange;
  return isValidLatency(op, impl, latency, implRange);
}

bool PlatformBasic::isValidLatency(OP_TYPE op, IMPL_TYPE impl, int latency, std::pair<int, int> & implRange) const
{
    bool isValid = false;
    implRange.first = -1;
    implRange.second = -1;
    if(isNOIMPL(impl))
    {
        std::vector<CoreBasic*> cores;
        if(op == OP_MEMORY)
        {
            cores = getCoresFromMemoryType(getMemTypeName(getMemTypeEnum(impl)));
        }
        else
        {
            cores = getCoresFromOp(getOpName(op));
        }

        bool allRangesEqual = true;
        std::pair<int, int> prevRange = implRange;
        for(auto core : cores)
        {
            if (core && core->isValidLatency(latency))
            {
              isValid = true;
              // Return first range that allows latency
              implRange = core->getLatencyRange();
              break;
            }

            if (prevRange == implRange)
            {
              // Store prevRange if set to -1,-1
              prevRange = core->getLatencyRange();
            }
            else if (allRangesEqual && prevRange != core->getLatencyRange())
            {
              // Check if prevRange same as current core range
              allRangesEqual = false;
            }
        }
        if (!isValid && allRangesEqual)
        {
          // If did not find valid range but all ranges are the same return the range
          implRange = prevRange;
        }
    }
    else
    {
        auto it = mCoreBasicMap.find(op * GRPSIZE + impl);
        if(it != mCoreBasicMap.end())
        {
          isValid =it->second->isValidLatency(latency);
          implRange = it->second->getLatencyRange();
        }
    }
    return isValid;
}

std::pair<int, int> PlatformBasic::verifyLatency(OP_TYPE op, IMPL_TYPE impl) const
{
    int minLat = -1;
    int maxLat = -1;
    if(isNOIMPL(impl))
    {
        std::vector<CoreBasic*> cores;
        if(op == OP_MEMORY)
        {
            cores = getCoresFromMemoryType(getMemTypeName(getMemTypeEnum(impl)));
        }
        else
        {
            cores = getCoresFromOp(getOpName(op));
        }
        for(auto core : cores)
        {
            if(minLat == -1 || minLat > core->getMinLatency())
            {
                minLat = core->getMinLatency();
            }
            if(maxLat < core->getMaxLatency())
            {
                maxLat = core->getMaxLatency();
            }
        }
    }
    else
    {
        auto it = mCoreBasicMap.find(op * GRPSIZE + impl);
        if(it != mCoreBasicMap.end())
        {
            return it->second->getLatencyRange();
        }
    }
    return std::make_pair(minLat, maxLat);
}

bool PlatformBasic::isValidAXILiteMemType(const std::string& AXILiteMemType) const
{ 
    return getMemTypeFromName(AXILiteMemType) == PlatformBasic::MEMORY_RAM_1P;
}

bool PlatformBasic::isValidAXILiteMemImpl(const std::string& AXILiteMemImpl) const
{
    return  getMemImplFromName(AXILiteMemImpl) == PlatformBasic::MEMORY_IMPL_AUTO ||
            getMemImplFromName(AXILiteMemImpl) == PlatformBasic::MEMORY_IMPL_LUTRAM ||
            getMemImplFromName(AXILiteMemImpl) == PlatformBasic::MEMORY_IMPL_BRAM ||
            (getMemImplFromName(AXILiteMemImpl) == PlatformBasic::MEMORY_IMPL_URAM && supportUram());

}

bool PlatformBasic::isValidAXILiteMemLatency(unsigned latency) const 
{
    return latency == 1;

}

std::string PlatformBasic::getDefaultAXILiteMemType() const
{
    return getMemTypeName(PlatformBasic::MEMORY_RAM_1P);
}

std::string PlatformBasic::getDefaultAXILiteMemImpl() const
{
    return getMemImplName(PlatformBasic::MEMORY_IMPL_AUTO);
}

unsigned PlatformBasic::getDefaultAXILiteMemLatency() const
{
    return 1;
}

std::string PlatformBasic::getOpName(OP_TYPE op) const
{
    std::string opStr;
    auto it = mOpEnum2Str.find(op);
    if(it != mOpEnum2Str.end())
    {
        opStr = it->second;
    }
    return opStr;
}

std::string PlatformBasic::getImplName(IMPL_TYPE impl) const
{
    std::string implStr("UNSUPPORTED");
    auto it = mImplEnum2Str.find(impl);
    if(it != mImplEnum2Str.end())
    {
        implStr = it->second;
    }
    if(isNOIMPL(impl))
    {
        implStr = "";
    }
    return implStr;
}

PlatformBasic::OP_TYPE PlatformBasic::getOpFromName(const std::string& opName) const
{
    OP_TYPE op = OP_UNSUPPORTED;
    auto it = mOpStr2Enum.find(getLowerString(opName));
    if(it != mOpStr2Enum.end())
    {
        op = it->second;
    }
    return op;
}

PlatformBasic::IMPL_TYPE PlatformBasic::getImplFromName(const std::string& implName) const
{
    IMPL_TYPE impl = UNSUPPORTED;
    auto it = mImplStr2Enum.find(getLowerString(implName));
    if(it != mImplStr2Enum.end())
    {
        impl = it->second;
    }
    // may be memory without impl
    if(impl == UNSUPPORTED)
    {
        MEMORY_TYPE memType = getMemTypeFromName(implName);
        if(memType != MEMORY_UNSUPPORTED)
        {
            impl = static_cast<IMPL_TYPE>(memType);
        }
    }
    return impl;
}

std::string PlatformBasic::getMemTypeName(MEMORY_TYPE type) const
{
    std::string typeStr;
    auto it = mMemTypeEnum2Str.find(type);
    if(it != mMemTypeEnum2Str.end())
    {
        typeStr = it->second;
    }
    return typeStr;
}

PlatformBasic::MEMORY_TYPE PlatformBasic::getMemTypeEnum(IMPL_TYPE impl)
{
  return CoreBasic::getMemoryTypeImpl(impl).first;
}

PlatformBasic::MEMORY_IMPL PlatformBasic::getMemImplEnum(IMPL_TYPE impl)
{
  return CoreBasic::getMemoryTypeImpl(impl).second;
}


std::string PlatformBasic::getMemImplName(MEMORY_IMPL impl) const
{
    std::string implStr("UNSUPPORTED");
    auto it = mMemImplEnum2Str.find(impl);
    if(it != mMemImplEnum2Str.end())
    {
        implStr = it->second;
    }
    return implStr;
}

PlatformBasic::MEMORY_TYPE PlatformBasic::getMemTypeFromName(const std::string& memTypeName) const
{
    MEMORY_TYPE memType = MEMORY_UNSUPPORTED;
    auto it = mMemTypeStr2Enum.find(getLowerString(memTypeName));
    if(it != mMemTypeStr2Enum.end())
    {
        memType = it->second;
    }
    return memType;
}

PlatformBasic::MEMORY_IMPL PlatformBasic::getMemImplFromName(const std::string& memImplName) const
{
    MEMORY_IMPL memImpl = MEMORY_IMPL_UNSUPPORTED;
    auto it = mMemImplStr2Enum.find(getLowerString(memImplName));
    if( it != mMemImplStr2Enum.end())
    {
        memImpl = it->second;
    }
    return memImpl;
}

bool PlatformBasic::isMemoryOp(OP_TYPE op)
{
    return op == OP_MEMORY || 
           op == OP_LOAD || 
           op == OP_STORE ||
           op == OP_READ || 
           op == OP_WRITE || 
           op == OP_MEMSHIFTREAD ||
           op == OP_NBREAD || 
           op == OP_NBWRITE;
}
namespace // for sqlite reading
{
// load platform from database
int loadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave){
  int rc;                   /* Function return code */
  sqlite3 *pFile;           /* Database connection opened on zFilename */
  sqlite3_backup *pBackup;  /* Backup object used to copy data */
  sqlite3 *pTo;             /* Database to copy to (pFile or pInMemory) */
  sqlite3 *pFrom;           /* Database to copy from (pFile or pInMemory) */

  /* Open the database file identified by zFilename. Exit early if this fails
  ** for any reason. */
  rc = sqlite3_open(zFilename, &pFile);
  if( rc==SQLITE_OK ){

    /* If this is a 'load' operation (isSave==0), then data is copied
    ** from the database file just opened to database pInMemory. 
    ** Otherwise, if this is a 'save' operation (isSave==1), then data
    ** is copied from pInMemory to pFile.  Set the variables pFrom and
    ** pTo accordingly. */
    pFrom = (isSave ? pInMemory : pFile);
    pTo   = (isSave ? pFile     : pInMemory);

    /* Set up the backup procedure to copy from the "main" database of 
    ** connection pFile to the main database of connection pInMemory.
    ** If something goes wrong, pBackup will be set to NULL and an error
    ** code and message left in connection pTo.
    **
    ** If the backup object is successfully created, call backup_step()
    ** to copy data from pFile to pInMemory. Then call backup_finish()
    ** to release resources associated with the pBackup object.  If an
    ** error occurred, then an error code and message will be left in
    ** connection pTo. If no error occurred, then the error code belonging
    ** to pTo is set to SQLITE_OK.
    */
    pBackup = sqlite3_backup_init(pTo, "main", pFrom, "main");
    if( pBackup ){
      (void)sqlite3_backup_step(pBackup, -1);
      (void)sqlite3_backup_finish(pBackup);
    }
    rc = sqlite3_errcode(pTo);
  }

  /* Close the database connection opened on database file zFilename
  ** and return the result of this function. */
  (void)sqlite3_close(pFile);
  return rc;
}

struct CoreBasicDef
{
    std::string type;
    std::string name;
    std::string op;
    std::string impl;
    int maxLat;
    int minLat;
    bool isPublic;
};


class Selector
{
public:
    static Selector& getInstance() { static Selector s; return s; }
private:
    sqlite3* mDb;

// Selector
Selector()
{
    //std::string db_file = HPAParamMgr::getParamMgr()->getValueIfExistAsString("hls.external_core_database", default_db_file);
    //if(db_file.empty())
    //{
    //    db_file = default_db_file;
    //}
    // TODO
    std::string db_file(default_db_file);
    int rc = sqlite3_open(":memory:", &mDb);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    rc = loadOrSaveDb(mDb, db_file.c_str(), 0);
    assert(rc == SQLITE_OK);
    if(rc != SQLITE_OK)
    {
        // TODO
        //ComMsgMgr::SendMsg(ComMsgMgr::MSGTYPE_ERROR, "@200-1608@");
        //throw xpcl::MessageReporter::Exception("");
    }
}

~Selector() { sqlite3_close(mDb); }
    
std::string safe_get_string (sqlite3_stmt* ppStmt, int col)
{
    const unsigned char* result = sqlite3_column_text(ppStmt, col);
    std::string value;
    if(result)
    {   
        value = reinterpret_cast<const char*>(result);
    }
    return value;
};

public:
std::vector<CoreBasicDef*> selectCoreBasics(std::string cmd)
{
    std::vector<CoreBasicDef*> coreBasicDefs;
    sqlite3_stmt* ppStmt;
    const char* pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd.c_str(), -1, &ppStmt, &pzTail);
    if(rc != SQLITE_OK) return coreBasicDefs;

    while(sqlite3_step(ppStmt) == SQLITE_ROW)
    {
        auto def = new CoreBasicDef();
        def->name = safe_get_string(ppStmt, 0);
        def->type = safe_get_string(ppStmt, 1);
        def->op = safe_get_string(ppStmt, 2);
        def->impl = safe_get_string(ppStmt, 3);
        int maxLatency = sqlite3_column_int(ppStmt, 4);
        // -2 means max value of int in this column
        def->maxLat = maxLatency == -2 ? std::numeric_limits<int>::max() : maxLatency;
        def->minLat = sqlite3_column_int(ppStmt, 5);
        def->isPublic = sqlite3_column_int(ppStmt, 6);

        coreBasicDefs.push_back(def);
    }
    sqlite3_finalize(ppStmt);
    return coreBasicDefs;
}

std::map<int, std::string> selectEncode(std::string cmd)
{
    std::map<int, std::string> encodeMap;
    sqlite3_stmt* ppStmt;
    const char* pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd.c_str(), -1, &ppStmt, &pzTail);
    assert(rc == SQLITE_OK);

    while(sqlite3_step(ppStmt) == SQLITE_ROW)
    {
       encodeMap[sqlite3_column_int(ppStmt, 0)] = safe_get_string(ppStmt, 1);
    };
    sqlite3_finalize(ppStmt);
    return encodeMap;
}

std::map<std::string, std::string> selectAliasCores(std::string cmd)
{
    std::map<std::string, std::string> aliasMap;
    sqlite3_stmt* ppStmt;
    const char* pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd.c_str(), -1, &ppStmt, &pzTail);
    assert(rc == SQLITE_OK);
    while(sqlite3_step(ppStmt) == SQLITE_ROW)
    {
       aliasMap[safe_get_string(ppStmt, 0)] = safe_get_string(ppStmt, 1);
    };
    sqlite3_finalize(ppStmt);
    return aliasMap;
}
};//< class Selector

} //< namespace
void PlatformBasic::checkEnumEncode()
{
    assert(OP_ADAPTER == getOpFromName("adapter"));
    assert(OP_ADD == getOpFromName("add"));
    assert(OP_ADDMUL == getOpFromName("addmul"));
    assert(OP_ADDMUL_SUB == getOpFromName("addmul_sub"));
    assert(OP_ADDMULADD == getOpFromName("addmuladd"));
    assert(OP_ADDMULADDACC == getOpFromName("addmuladdacc"));
    assert(OP_ADDMULADDSEL == getOpFromName("addmuladdsel"));
    assert(OP_ADDMULSUB == getOpFromName("addmulsub"));
    assert(OP_ADDMULSUBACC == getOpFromName("addmulsubacc"));
    assert(OP_ALL == getOpFromName("all"));
    assert(OP_ALLOCA == getOpFromName("alloca"));
    assert(OP_AND == getOpFromName("and"));
    assert(OP_ANDREDUCE == getOpFromName("andreduce"));
    assert(OP_ASHR == getOpFromName("ashr"));
    assert(OP_ATOMICCMPXCHG == getOpFromName("atomiccmpxchg"));
    assert(OP_ATOMICRMW == getOpFromName("atomicrmw"));
    assert(OP_BITCAST == getOpFromName("bitcast"));
    assert(OP_BITCONCATENATE == getOpFromName("bitconcatenate"));
    assert(OP_BITSELECT == getOpFromName("bitselect"));
    assert(OP_BITSET == getOpFromName("bitset"));
    assert(OP_BLACKBOX == getOpFromName("blackbox"));
    assert(OP_BR == getOpFromName("br"));
    assert(OP_CALL == getOpFromName("call"));
    assert(OP_CMUL == getOpFromName("cmul"));
    assert(OP_CTLZ == getOpFromName("ctlz"));
    assert(OP_CTTZ == getOpFromName("cttz"));
    assert(OP_CTPOP == getOpFromName("ctpop"));
    assert(OP_DACC == getOpFromName("dacc"));
    assert(OP_DADD == getOpFromName("dadd"));
    assert(OP_DCMP == getOpFromName("dcmp"));
    assert(OP_DDIV == getOpFromName("ddiv"));
    assert(OP_DEXP == getOpFromName("dexp"));
    assert(OP_DLOG == getOpFromName("dlog"));
    assert(OP_DMUL == getOpFromName("dmul"));
    assert(OP_DOTPRA3 == getOpFromName("dotpra3"));
    assert(OP_DOTPRA3ADD == getOpFromName("dotpra3add"));
    assert(OP_DPTOHP == getOpFromName("dptohp"));
    assert(OP_DPTOSI == getOpFromName("dptosi"));
    assert(OP_DPTOUI == getOpFromName("dptoui"));
    assert(OP_DRECIP == getOpFromName("drecip"));
    assert(OP_DREM == getOpFromName("drem"));
    assert(OP_DRSQRT == getOpFromName("drsqrt"));
    assert(OP_DSP == getOpFromName("dsp"));
    assert(OP_DSP_AB == getOpFromName("dsp_ab"));
    assert(OP_DSQRT == getOpFromName("dsqrt"));
    assert(OP_DSUB == getOpFromName("dsub"));
    assert(OP_EXTRACTELEMENT == getOpFromName("extractelement"));
    assert(OP_EXTRACTVALUE == getOpFromName("extractvalue"));
    assert(OP_FACC == getOpFromName("facc"));
    assert(OP_FADD == getOpFromName("fadd"));
    assert(OP_FCMP == getOpFromName("fcmp"));
    assert(OP_FDIV == getOpFromName("fdiv"));
    assert(OP_FENCE == getOpFromName("fence"));
    assert(OP_FEXP == getOpFromName("fexp"));
    assert(OP_FLOG == getOpFromName("flog"));
    assert(OP_FMAC == getOpFromName("fmacc"));
    assert(OP_FMUL == getOpFromName("fmul"));
    assert(OP_FPEXT == getOpFromName("fpext"));
    assert(OP_FPTOSI == getOpFromName("fptosi"));
    assert(OP_FPTOUI == getOpFromName("fptoui"));
    assert(OP_FPTRUNC == getOpFromName("fptrunc"));
    assert(OP_FRECIP == getOpFromName("frecip"));
    assert(OP_FREM == getOpFromName("frem"));
    assert(OP_FRSQRT == getOpFromName("frsqrt"));
    assert(OP_FSQRT == getOpFromName("fsqrt"));
    assert(OP_FSUB == getOpFromName("fsub"));
    assert(OP_GETELEMENTPTR == getOpFromName("getelementptr"));
    assert(OP_HADD == getOpFromName("hadd"));
    assert(OP_HCMP == getOpFromName("hcmp"));
    assert(OP_HDIV == getOpFromName("hdiv"));
    assert(OP_HMUL == getOpFromName("hmul"));
    assert(OP_HPTODP == getOpFromName("hptodp"));
    assert(OP_HPTOSP == getOpFromName("hptosp"));
    assert(OP_HSQRT == getOpFromName("hsqrt"));
    assert(OP_HSUB == getOpFromName("hsub"));
    assert(OP_ICMP == getOpFromName("icmp"));
    assert(OP_IFCANREAD == getOpFromName("ifcanread"));
    assert(OP_IFCANWRITE == getOpFromName("ifcanwrite"));
    assert(OP_IFNBREAD == getOpFromName("ifnbread"));
    assert(OP_IFNBWRITE == getOpFromName("ifnbwrite"));
    assert(OP_IFREAD == getOpFromName("ifread"));
    assert(OP_IFWRITE == getOpFromName("ifwrite"));
    assert(OP_INDIRECTBR == getOpFromName("indirectbr"));
    assert(OP_INSERTELEMENT == getOpFromName("insertelement"));
    assert(OP_INSERTVALUE == getOpFromName("insertvalue"));
    assert(OP_INTTOPTR == getOpFromName("inttoptr"));
    assert(OP_INVOKE == getOpFromName("invoke"));
    assert(OP_LANDINGPAD == getOpFromName("landingpad"));
    assert(OP_LOAD == getOpFromName("load"));
    assert(OP_LSHR == getOpFromName("lshr"));
    assert(OP_MAC16_CLOCKX2 == getOpFromName("mac16_clockx2"));
    assert(OP_MAC6_MAC8_CLOCKX2 == getOpFromName("mac6_mac8_clockx2"));
    assert(OP_MEMORY == getOpFromName("memory"));
    assert(OP_MEMSHIFTREAD == getOpFromName("memshiftread"));
    assert(OP_MUL == getOpFromName("mul"));
    assert(OP_MUL_SELECT == getOpFromName("mul_select"));
    assert(OP_MUL_SELECTIVT == getOpFromName("mul_selectivt"));
    assert(OP_MUL_SUB == getOpFromName("mul_sub"));
    assert(OP_MULADD == getOpFromName("muladd"));
    assert(OP_MULADDACC == getOpFromName("muladdacc"));
    assert(OP_MULSELECT == getOpFromName("mulselect"));
    assert(OP_MULSELECTIVT == getOpFromName("mulselectivt"));
    assert(OP_MULSUB == getOpFromName("mulsub"));
    assert(OP_MULSUBACC == getOpFromName("mulsubacc"));
    assert(OP_MUX == getOpFromName("mux"));
    assert(OP_NANDREDUCE == getOpFromName("nandreduce"));
    assert(OP_NBPEEK == getOpFromName("nbpeek"));
    assert(OP_NBREAD == getOpFromName("nbread"));
    assert(OP_NBREADREQ == getOpFromName("nbreadreq"));
    assert(OP_NBWRITE == getOpFromName("nbwrite"));
    assert(OP_NBWRITEREQ == getOpFromName("nbwritereq"));
    assert(OP_NORREDUCE == getOpFromName("norreduce"));
    assert(OP_OR == getOpFromName("or"));
    assert(OP_ORREDUCE == getOpFromName("orreduce"));
    assert(OP_PARTSELECT == getOpFromName("partselect"));
    assert(OP_PARTSET == getOpFromName("partset"));
    assert(OP_PEEK == getOpFromName("peek"));
    assert(OP_PHI == getOpFromName("phi"));
    assert(OP_POLL == getOpFromName("poll"));
    assert(OP_PTRTOINT == getOpFromName("ptrtoint"));
    assert(OP_QADDSUB == getOpFromName("qaddsub"));
    assert(OP_READ == getOpFromName("read"));
    assert(OP_READREQ == getOpFromName("readreq"));
    assert(OP_RESUME == getOpFromName("resume"));
    assert(OP_RET == getOpFromName("ret"));
    assert(OP_RETURN == getOpFromName("return"));
    assert(OP_SDIV == getOpFromName("sdiv"));
    assert(OP_SELECT == getOpFromName("select"));
    assert(OP_SETEQ == getOpFromName("seteq"));
    assert(OP_SETGE == getOpFromName("setge"));
    assert(OP_SETGT == getOpFromName("setgt"));
    assert(OP_SETLE == getOpFromName("setle"));
    assert(OP_SETLT == getOpFromName("setlt"));
    assert(OP_SETNE == getOpFromName("setne"));
    assert(OP_SEXT == getOpFromName("sext"));
    assert(OP_SHL == getOpFromName("shl"));
    assert(OP_SHUFFLEVECTOR == getOpFromName("shufflevector"));
    assert(OP_SITODP == getOpFromName("sitodp"));
    assert(OP_SITOFP == getOpFromName("sitofp"));
    assert(OP_SPECBITSMAP == getOpFromName("specbitsmap"));
    assert(OP_SPECBRAMWITHBYTEENABLE == getOpFromName("specbramwithbyteenable"));
    assert(OP_SPECBURST == getOpFromName("specburst"));
    assert(OP_SPECCHANNEL == getOpFromName("specchannel"));
    assert(OP_SPECCHCORE == getOpFromName("specchcore"));
    assert(OP_SPECCLOCKDOMAIN == getOpFromName("specclockdomain"));
    assert(OP_SPECDATAFLOWPIPELINE == getOpFromName("specdataflowpipeline"));
    assert(OP_SPECDT == getOpFromName("specdt"));
    assert(OP_SPECEXT == getOpFromName("specext"));
    assert(OP_SPECFUCORE == getOpFromName("specfucore"));
    assert(OP_SPECIFCORE == getOpFromName("specifcore"));
    assert(OP_SPECINTERFACE == getOpFromName("specinterface"));
    assert(OP_SPECIPCORE == getOpFromName("specipcore"));
    assert(OP_SPECLATENCY == getOpFromName("speclatency"));
    assert(OP_SPECLOOPBEGIN == getOpFromName("specloopbegin"));
    assert(OP_SPECLOOPEND == getOpFromName("specloopend"));
    assert(OP_SPECLOOPFLATTEN == getOpFromName("specloopflatten"));
    assert(OP_SPECLOOPNAME == getOpFromName("specloopname"));
    assert(OP_SPECLOOPTRIPCOUNT == getOpFromName("speclooptripcount"));
    assert(OP_SPECMEMCORE == getOpFromName("specmemcore"));
    assert(OP_SPECMODULE == getOpFromName("specmodule"));
    assert(OP_SPECMODULEINST == getOpFromName("specmoduleinst"));
    assert(OP_SPECOCCURRENCE == getOpFromName("specoccurrence"));
    assert(OP_SPECPARALLEL == getOpFromName("specparallel"));
    assert(OP_SPECPARALLELLOOP == getOpFromName("specparallelloop"));
    assert(OP_SPECPIPELINE == getOpFromName("specpipeline"));
    assert(OP_SPECPIPODEPTH == getOpFromName("specpipodepth"));
    assert(OP_SPECPLATFORM == getOpFromName("specplatform"));
    assert(OP_SPECPORT == getOpFromName("specport"));
    assert(OP_SPECPORTMAP == getOpFromName("specportmap"));
    assert(OP_SPECPOWERDOMAIN == getOpFromName("specpowerdomain"));
    assert(OP_SPECPROCESSDECL == getOpFromName("specprocessdecl"));
    assert(OP_SPECPROCESSDEF == getOpFromName("specprocessdef"));
    assert(OP_SPECPROTOCOL == getOpFromName("specprotocol"));
    assert(OP_SPECREGIONBEGIN == getOpFromName("specregionbegin"));
    assert(OP_SPECREGIONEND == getOpFromName("specregionend"));
    assert(OP_SPECRESET == getOpFromName("specreset"));
    assert(OP_SPECRESOURCE == getOpFromName("specresource"));
    assert(OP_SPECRESOURCELIMIT == getOpFromName("specresourcelimit"));
    assert(OP_SPECSENSITIVE == getOpFromName("specsensitive"));
    assert(OP_SPECSTABLE == getOpFromName("specstable"));
    assert(OP_SPECSTABLECONTENT == getOpFromName("specstablecontent"));
    assert(OP_SPECSTATEBEGIN == getOpFromName("specstatebegin"));
    assert(OP_SPECSTATEEND == getOpFromName("specstateend"));
    assert(OP_SPECSYNMODULE == getOpFromName("specsynmodule"));
    assert(OP_SPECTOPMODULE == getOpFromName("spectopmodule"));
    assert(OP_SPTOHP == getOpFromName("sptohp"));
    assert(OP_SREM == getOpFromName("srem"));
    assert(OP_STORE == getOpFromName("store"));
    assert(OP_SUB == getOpFromName("sub"));
    assert(OP_SUBMUL == getOpFromName("submul"));
    assert(OP_SUBMUL_SUB == getOpFromName("submul_sub"));
    assert(OP_SUBMULADD == getOpFromName("submuladd"));
    assert(OP_SUBMULADDACC == getOpFromName("submuladdacc"));
    assert(OP_SUBMULSUB == getOpFromName("submulsub"));
    assert(OP_SUBMULSUBACC == getOpFromName("submulsubacc"));
    assert(OP_SWITCH == getOpFromName("switch"));
    assert(OP_TRUNC == getOpFromName("trunc"));
    assert(OP_UDIV == getOpFromName("udiv"));
    assert(OP_UITODP == getOpFromName("uitodp"));
    assert(OP_UITOFP == getOpFromName("uitofp"));
    assert(OP_UNREACHABLE == getOpFromName("unreachable"));
    assert(OP_UREM == getOpFromName("urem"));
    assert(OP_USEROP1 == getOpFromName("userop1"));
    assert(OP_USEROP2 == getOpFromName("userop2"));
    assert(OP_VAARG == getOpFromName("vaarg"));
    assert(OP_VIVADO_IP == getOpFromName("vivado_ip"));
    assert(OP_WAIT == getOpFromName("wait"));
    assert(OP_WAITEVENT == getOpFromName("waitevent"));
    assert(OP_WRITE == getOpFromName("write"));
    assert(OP_WRITEREQ == getOpFromName("writereq"));
    assert(OP_WRITERESP == getOpFromName("writeresp"));
    assert(OP_XNORREDUCE == getOpFromName("xnorreduce"));
    assert(OP_XOR == getOpFromName("xor"));
    assert(OP_XORREDUCE == getOpFromName("xorreduce"));
    assert(OP_ZEXT == getOpFromName("zext"));
    //noimpl
    assert(NOIMPL == getImplFromName(""));
    assert(AUTO == getImplFromName("auto"));
    assert(AUTO_2STAGE == getImplFromName("auto_2stage"));
    assert(AUTO_3STAGE == getImplFromName("auto_3stage"));
    assert(AUTO_4STAGE == getImplFromName("auto_4stage"));
    assert(AUTO_5STAGE == getImplFromName("auto_5stage"));
    assert(AUTO_6STAGE == getImplFromName("auto_6stage"));
    assert(AUTO_COMB == getImplFromName("auto_comb"));
    assert(AUTO_MUX == getImplFromName("auto_mux"));
    assert(AUTO_PIPE == getImplFromName("auto_pipe"));
    assert(AUTO_SEL == getImplFromName("auto_sel"));
    assert(AUTO_SEQ == getImplFromName("auto_seq"));
    assert(AXI4LITES == getImplFromName("axi4lites"));
    assert(AXI4M == getImplFromName("axi4m"));
    assert(AXI4STREAM == getImplFromName("axi4stream"));
    assert(DPWOM_AUTO == getImplFromName("dpwom_auto"));
    assert(DSP == getImplFromName("dsp"));
    assert(DSP58_DP == getImplFromName("dsp58_dp"));
    assert(DSP_AM == getImplFromName("dsp_am"));
    assert(DSP_AM_2STAGE == getImplFromName("dsp_am_2stage"));
    assert(DSP_AM_3STAGE == getImplFromName("dsp_am_3stage"));
    assert(DSP_AM_4STAGE == getImplFromName("dsp_am_4stage"));
    assert(DSP_AMA == getImplFromName("dsp_ama"));
    assert(DSP_AMA_2STAGE == getImplFromName("dsp_ama_2stage"));
    assert(DSP_AMA_3STAGE == getImplFromName("dsp_ama_3stage"));
    assert(DSP_AMA_4STAGE == getImplFromName("dsp_ama_4stage"));
    assert(DSP_MAC == getImplFromName("dsp_mac"));
    assert(DSP_MAC_2STAGE == getImplFromName("dsp_mac_2stage"));
    assert(DSP_MAC_3STAGE == getImplFromName("dsp_mac_3stage"));
    assert(DSP_MAC_4STAGE == getImplFromName("dsp_mac_4stage"));
    assert(FABRIC == getImplFromName("fabric"));
    assert(FABRIC_COMB == getImplFromName("fabric_comb"));
    assert(FABRIC_SEQ == getImplFromName("fabric_seq"));
    assert(FIFO_BRAM == getImplFromName("fifo_bram"));
    assert(FIFO_LUTRAM == getImplFromName("fifo_lutram"));
    assert(FIFO_MEMORY == getImplFromName("fifo_memory"));
    assert(FIFO_SRL == getImplFromName("fifo_srl"));
    assert(FIFO_URAM == getImplFromName("fifo_uram"));
    assert(FSL == getImplFromName("fsl"));
    assert(FULLDSP == getImplFromName("fulldsp"));
    assert(M_AXI == getImplFromName("m_axi"));
    assert(MAXDSP == getImplFromName("maxdsp"));
    assert(MEDDSP == getImplFromName("meddsp"));
    assert(NODSP == getImplFromName("nodsp"));
    assert(NPI64M == getImplFromName("npi64m"));
    assert(PLB46M == getImplFromName("plb46m"));
    assert(PLB46S == getImplFromName("plb46s"));
    assert(PRIMITIVEDSP == getImplFromName("primitivedsp"));
    assert(QADDER == getImplFromName("qadder"));
    assert(RAM3S_AUTO == getImplFromName("ram3s_auto"));
    assert(RAM4S_AUTO == getImplFromName("ram4s_auto"));
    assert(RAM5S_AUTO == getImplFromName("ram5s_auto"));
    assert(RAM_1P_AUTO == getImplFromName("ram_1p_auto"));
    assert(RAM_1P_BRAM == getImplFromName("ram_1p_bram"));
    assert(RAM_1P_LUTRAM == getImplFromName("ram_1p_lutram"));
    assert(RAM_1P_URAM == getImplFromName("ram_1p_uram"));
    assert(RAM_1WNR == getImplFromName("ram_1wnr_auto"));
    assert(RAM_1WNR_BRAM == getImplFromName("ram_1wnr_bram"));
    assert(RAM_1WNR_LUTRAM == getImplFromName("ram_1wnr_lutram"));
    assert(RAM_1WNR_URAM == getImplFromName("ram_1wnr_uram"));
    assert(RAM_2P_AUTO == getImplFromName("ram_2p_auto"));
    assert(RAM_2P_BRAM == getImplFromName("ram_2p_bram"));
    assert(RAM_2P_LUTRAM == getImplFromName("ram_2p_lutram"));
    assert(RAM_2P_URAM == getImplFromName("ram_2p_uram"));
    assert(RAM_AUTO == getImplFromName("ram_auto"));
    assert(RAM_COREGEN_AUTO == getImplFromName("ram_coregen_auto"));
    assert(RAM_S2P_BRAM == getImplFromName("ram_s2p_bram"));
    assert(RAM_S2P_BRAM_ECC == getImplFromName("ram_s2p_bram_ecc"));
    assert(RAM_S2P_LUTRAM == getImplFromName("ram_s2p_lutram"));
    assert(RAM_S2P_URAM == getImplFromName("ram_s2p_uram"));
    assert(RAM_S2P_URAM_ECC == getImplFromName("ram_s2p_uram_ecc"));
    assert(RAM_T2P_BRAM == getImplFromName("ram_t2p_bram"));
    assert(RAM_T2P_URAM == getImplFromName("ram_t2p_uram"));
    assert(REGISTER_AUTO == getImplFromName("register_auto"));
    assert(ROM3S_AUTO == getImplFromName("rom3s_auto"));
    assert(ROM4S_AUTO == getImplFromName("rom4s_auto"));
    assert(ROM5S_AUTO == getImplFromName("rom5s_auto"));
    assert(ROM_1P_1S_AUTO == getImplFromName("rom_1p_1s_auto"));
    assert(ROM_1P_AUTO == getImplFromName("rom_1p_auto"));
    assert(ROM_1P_BRAM == getImplFromName("rom_1p_bram"));
    assert(ROM_1P_LUTRAM == getImplFromName("rom_1p_lutram"));
    assert(ROM_2P_AUTO == getImplFromName("rom_2p_auto"));
    assert(ROM_2P_BRAM == getImplFromName("rom_2p_bram"));
    assert(ROM_2P_LUTRAM == getImplFromName("rom_2p_lutram"));
    assert(ROM_AUTO == getImplFromName("rom_auto"));
    assert(ROM_COREGEN_AUTO == getImplFromName("rom_coregen_auto"));
    assert(ROM_NP_BRAM == getImplFromName("rom_np_bram"));
    assert(ROM_NP_LUTRAM == getImplFromName("rom_np_lutram"));
    assert(S_AXILITE == getImplFromName("s_axilite"));
    assert(SHIFTREG_AUTO == getImplFromName("shiftreg_auto"));
    assert(SPRAM_COREGEN_AUTO == getImplFromName("spram_coregen_auto"));
    assert(SPROM_COREGEN_AUTO == getImplFromName("sprom_coregen_auto"));
    assert(SPWOM_AUTO == getImplFromName("spwom_auto"));
    assert(STREAM_BUNDLE == getImplFromName("stream_bundle"));
    assert(TADDER == getImplFromName("tadder"));
    assert(VIVADO_DDS == getImplFromName("vivado_dds"));
    assert(VIVADO_DIVIDER == getImplFromName("vivado_divider"));
    assert(VIVADO_FFT == getImplFromName("vivado_fft"));
    assert(VIVADO_FIR == getImplFromName("vivado_fir"));
    assert(XPM_MEMORY_AUTO == getImplFromName("xpm_memory_auto"));
    assert(XPM_MEMORY_BLOCK == getImplFromName("xpm_memory_block"));
    assert(XPM_MEMORY_DISTRIBUTE == getImplFromName("xpm_memory_distribute"));
    assert(XPM_MEMORY_URAM == getImplFromName("xpm_memory_uram"));
    assert(DSP_SLICE == getImplFromName("dsp_slice"));
    assert(COMPACTENCODING_DONTCARE == getImplFromName("compactencoding_dontcare"));
    assert(COMPACTENCODING_REALDEF == getImplFromName("compactencoding_realdef"));
    assert(ONEHOTENCODING_DONTCARE == getImplFromName("onehotencoding_dontcare"));
    assert(ONEHOTENCODING_REALDEF == getImplFromName("onehotencoding_realdef"));
    assert(MEMORY_DPWOM == getMemTypeFromName("dpwom"));
    assert(MEMORY_FIFO == getMemTypeFromName("fifo"));
    assert(MEMORY_RAM == getMemTypeFromName("ram"));
    assert(MEMORY_RAM3S == getMemTypeFromName("ram3s"));
    assert(MEMORY_RAM4S == getMemTypeFromName("ram4s"));
    assert(MEMORY_RAM5S == getMemTypeFromName("ram5s"));
    assert(MEMORY_RAM_1P == getMemTypeFromName("ram_1p"));
    assert(MEMORY_RAM_1WNR == getMemTypeFromName("ram_1wnr"));
    assert(MEMORY_RAM_2P == getMemTypeFromName("ram_2p"));
    assert(MEMORY_RAM_COREGEN == getMemTypeFromName("ram_coregen"));
    assert(MEMORY_RAM_S2P == getMemTypeFromName("ram_s2p"));
    assert(MEMORY_RAM_T2P == getMemTypeFromName("ram_t2p"));
    assert(MEMORY_REGISTER == getMemTypeFromName("register"));
    assert(MEMORY_ROM == getMemTypeFromName("rom"));
    assert(MEMORY_ROM3S == getMemTypeFromName("rom3s"));
    assert(MEMORY_ROM4S == getMemTypeFromName("rom4s"));
    assert(MEMORY_ROM5S == getMemTypeFromName("rom5s"));
    assert(MEMORY_ROM_1P == getMemTypeFromName("rom_1p"));
    assert(MEMORY_ROM_1P_1S == getMemTypeFromName("rom_1p_1s"));
    assert(MEMORY_ROM_2P == getMemTypeFromName("rom_2p"));
    assert(MEMORY_ROM_COREGEN == getMemTypeFromName("rom_coregen"));
    assert(MEMORY_ROM_NP == getMemTypeFromName("rom_np"));
    assert(MEMORY_SHIFTREG == getMemTypeFromName("shiftreg"));
    assert(MEMORY_SPRAM_COREGEN == getMemTypeFromName("spram_coregen"));
    assert(MEMORY_SPROM_COREGEN == getMemTypeFromName("sprom_coregen"));
    assert(MEMORY_SPWOM == getMemTypeFromName("spwom"));
    assert(MEMORY_XPM_MEMORY == getMemTypeFromName("xpm_memory"));
    //noimpl
    assert(MEMORY_NOIMPL == getMemImplFromName(""));
    assert(MEMORY_IMPL_AUTO == getMemImplFromName("auto"));
    assert(MEMORY_IMPL_BLOCK == getMemImplFromName("block"));
    assert(MEMORY_IMPL_BRAM == getMemImplFromName("bram"));
    assert(MEMORY_IMPL_BRAM_ECC == getMemImplFromName("bram_ecc"));
    assert(MEMORY_IMPL_DISTRIBUTE == getMemImplFromName("distribute"));
    assert(MEMORY_IMPL_LUTRAM == getMemImplFromName("lutram"));
    assert(MEMORY_IMPL_MEMORY == getMemImplFromName("memory"));
    assert(MEMORY_IMPL_SRL == getMemImplFromName("srl"));
    assert(MEMORY_IMPL_URAM == getMemImplFromName("uram"));
    assert(MEMORY_IMPL_URAM_ECC == getMemImplFromName("uram_ecc"));
}

bool PlatformBasic::load(const std::string& libraryName)
{
    bool result = loadStrEnumConverter() && loadCoreBasic(libraryName) && loadAlias() && loadCoreBasicInCompleteRepository();
    mLibraryName = libraryName;
    checkEnumEncode();
    return result;
}

template<class T>
bool createStrEnumConverter(std::string type, 
                            std::map<T, std::string>& enum2strMap,
                            std::map<std::string, T>& str2enumMap
                            )
{
    enum2strMap.clear();
    str2enumMap.clear();

    std::string cmd = "select CODE,STRING from platform_basic_assit where TYPE = '"
                      + type +
                      "' ";
    auto& selector = Selector::getInstance();
    std::map<int, std::string> codeMap = selector.selectEncode(cmd);
    for(auto pair : codeMap)
    {
        T enumValue = static_cast<T>(pair.first);
        enum2strMap[enumValue] = pair.second;
        str2enumMap[pair.second] = enumValue;
    }
    return !codeMap.empty();
}

bool PlatformBasic::loadStrEnumConverter()
{
    return createStrEnumConverter("op", mOpEnum2Str, mOpStr2Enum) &&
           createStrEnumConverter("impl", mImplEnum2Str, mImplStr2Enum) &&
           createStrEnumConverter("mem type", mMemTypeEnum2Str, mMemTypeStr2Enum) &&
           createStrEnumConverter("mem impl", mMemImplEnum2Str, mMemImplStr2Enum);
}

void PlatformBasic::createCoreBasics(std::string coreName, std::string op, std::string impl, int maxLat, int minLat, bool isPublic)
{
    std::vector<std::string> ops = split(op, ",");
    std::vector<CoreBasic*> coreBasics;
    for(const auto& opStr : ops)
    {
        OP_TYPE opCode = getOpFromName(opStr);
        
        IMPL_TYPE implCode = getImplFromName(impl);
        
        auto key = opCode * GRPSIZE + implCode;
        CoreBasic *coreBasic = nullptr;
        if (mCoreBasicMap.count(key) <= 0) {
            coreBasic = new CoreBasic(opCode, implCode, maxLat, minLat, coreName, isPublic);
            mCoreBasicMap[key] = coreBasic;
        } else {
            coreBasic = mCoreBasicMap[key];
        }
        coreBasics.push_back(coreBasic);
    } 
    mCoreNameMap[getLowerString(coreName)] = coreBasics;
}

void PlatformBasic::createMemoryCoreBasic(std::string coreName, std::string op, std::string impl, int maxLat, int minLat, bool isPublic)
{
    //MEMORY_TYPE typeCode = getMemTypeFromName(op);
    //assert(typeCode != MEMORY_UNSUPPORTED);
    //MEMORY_IMPL memImplCode = getMemImplFromName(impl);
    //assert(memImplCode != MEMORY_IMPL_UNSUPPORTED);
    OP_TYPE opCode = OP_MEMORY;
    IMPL_TYPE implCode = getImplFromName(op + "_" + impl);
    assert(implCode != UNSUPPORTED);

    auto key = opCode * GRPSIZE + implCode;
    CoreBasic *coreBasic = nullptr;
    if (mCoreBasicMap.count(key) <= 0) {
        coreBasic = new CoreBasic(opCode, implCode, maxLat, minLat, coreName, isPublic);
        mCoreBasicMap[key] = coreBasic;
    } else {
        coreBasic = mCoreBasicMap[key];
    }

    mCoreNameMap[getLowerString(coreName)] = std::vector<CoreBasic*>(1, coreBasic);
}

bool PlatformBasic::loadCoreBasicInCompleteRepository()
{
    mCoreNameMapInCompleteRepository.clear();

    auto& selector = Selector::getInstance();
    std::string cmd = "select CORE_NAME,TYPE,OP,IMPL,MAX_LATENCY,MIN_LATENCY,IS_PUBLIC from COMPLETE_CoreDef ";
    std::vector<CoreBasicDef*> defs = selector.selectCoreBasics(cmd);

    // for alias core
    cmd = "select CORE_NAME,TYPE,OP,IMPL,MAX_LATENCY,MIN_LATENCY,IS_PUBLIC from\
    platform_basic_deprecated where TYPE == 'deprecated' ";
    std::vector<CoreBasicDef*> deprecatedDefs = selector.selectCoreBasics(cmd);
    defs.insert(defs.end(), deprecatedDefs.begin(), deprecatedDefs.end());

    for (auto def : defs) 
    {
        auto defName = getLowerString(def->name);
        if (mCoreNameMapInCompleteRepository.count(defName) <= 0) {
            if (def->type == "storage")
            {
                OP_TYPE opCode = OP_MEMORY;
                IMPL_TYPE implCode = getImplFromName(def->op + "_" + def->impl);

                auto coreBasic = new CoreBasic(opCode, implCode, def->maxLat, def->minLat, def->name, def->isPublic);
                mCoreNameMapInCompleteRepository[defName] = std::vector<CoreBasic*>(1, coreBasic);
            }
            else 
            {
                std::vector<std::string> ops = split(def->op, ",");
                std::vector<CoreBasic*> coreBasics;
                for(const auto& opStr : ops)
                {
                    OP_TYPE opCode = getOpFromName(opStr);
                    IMPL_TYPE implCode = getImplFromName(def->impl);
            
                    auto coreBasic = new CoreBasic(opCode, implCode, def->maxLat, def->minLat, def->name, def->isPublic);
                    coreBasics.push_back(coreBasic);
                } 
                mCoreNameMapInCompleteRepository[defName] = coreBasics;
            }
        }
    }

    cmd = "select CORE_NAME,BASE from platform_basic_deprecated where TYPE == 'alias' ";
    std::map<std::string, std::string> aliasMap = selector.selectAliasCores(cmd);
    for(auto& pair : aliasMap)
    {
        if(mCoreNameMapInCompleteRepository.find(pair.second) != mCoreNameMapInCompleteRepository.end())
        {
            mCoreNameMapInCompleteRepository[pair.first] = mCoreNameMapInCompleteRepository[pair.second];
        }
    }
    for (auto def : defs)
      delete def;

    return !defs.empty();
}

bool PlatformBasic::loadCoreBasic(const std::string& libraryName)
{
    std::string deviceResourceInfo = g_device_resource_info;
    if (deviceResourceInfo.empty()) {
        deviceResourceInfo = "SLICE_112480.0000_LUT_899840.0000_FF_1799680.0000_DSP_1968.0000_BRAM_1934.0000_URAM_463.0000";
        std::cerr << "WARNING: using default platform device information: " << deviceResourceInfo << std::endl;
    }
    mCoreBasicMap.clear();
    mCoreNameMap.clear();

    auto& selector = Selector::getInstance();
    std::string cmd = "select CORE_NAME,TYPE,OP,IMPL,MAX_LATENCY,MIN_LATENCY,IS_PUBLIC from " +
                      libraryName + "_CoreDef ";
    std::vector<CoreBasicDef*> defs = selector.selectCoreBasics(cmd);
    
    // for alias core
    cmd = "select CORE_NAME,TYPE,OP,IMPL,MAX_LATENCY,MIN_LATENCY,IS_PUBLIC from\
    platform_basic_deprecated where TYPE == 'deprecated' ";
    std::vector<CoreBasicDef*> deprecatedDefs = selector.selectCoreBasics(cmd);
    defs.insert(defs.end(), deprecatedDefs.begin(), deprecatedDefs.end());

    std::vector<std::string> seglist;
    std::string delimiter = "_";
    size_t pos = 0;
    std::string token;
    while ((pos = deviceResourceInfo.find(delimiter)) != std::string::npos) {
        token = deviceResourceInfo.substr(0, pos);
        seglist.push_back(token);
        deviceResourceInfo.erase(0, pos + delimiter.length());
    }
    seglist.push_back(deviceResourceInfo);

    std::map<std::string,double> resValue;
    for (int i = 0; i < seglist.size(); i += 2 )
    {
        std::string key = getLowerString(seglist[i]);
        double value = std::stod(seglist[i+1]);
        resValue[key] = value;
    }

    for(auto def : defs)
    {
        if(def->type == "storage")
        {
            if (resValue.find("uram")->second == 0 && def->impl.find("uram") != std::string::npos) {
                continue;
            }
            if (resValue.find("bram")->second == 0 && def->impl.find("bram") != std::string::npos) {
                continue;
            }
            createMemoryCoreBasic(def->name, def->op, def->impl, 
                                  def->maxLat, def->minLat, def->isPublic);
        }
        else
        {
            if (resValue.find("dsp")->second == 0) {
                if (def->impl.find("dsp") != std::string::npos || def->impl.find("vivado_dds") != std::string::npos || def->impl.find("vivado_fir") != std::string::npos || def->impl.find("vivado_fft") != std::string::npos || def->impl.find("qadder") != std::string::npos)
                {
                    continue;
                }
            }
            createCoreBasics(def->name, def->op, def->impl,
                             def->maxLat, def->minLat, def->isPublic);
        }
    }
    for (auto def : defs)
      delete def;
    return !defs.empty();
}

bool PlatformBasic::loadAlias()
{
    auto& selector = Selector::getInstance();
    std::string cmd = "select CORE_NAME,BASE from platform_basic_deprecated where TYPE == 'alias' ";
    std::map<std::string, std::string> aliasMap = selector.selectAliasCores(cmd);
    for(auto& pair : aliasMap)
    {
        if(mCoreNameMap.find(pair.second) != mCoreNameMap.end())
        {
            mCoreNameMap[pair.first] = mCoreNameMap[pair.second];
        }
    }
    return true;
}

bool PlatformBasic::supportFMA() const
{
    return verifyOpIsSupported(OP_FMA);
}

bool PlatformBasic::supportFMAc() const
{
    return verifyOpIsSupported(OP_FMAC);
}

bool PlatformBasic::supportFAcc() const
{
    return verifyOpIsSupported(OP_FACC) && verifyOpIsSupported(OP_DACC);
}

bool PlatformBasic::supportCMul() const
{
    return verifyOpIsSupported(OP_CMUL);
}

bool PlatformBasic::verifyOpIsSupported (OP_TYPE op) const
{
    bool opSupport = false;
    for (auto it = mCoreBasicMap.begin(); it != mCoreBasicMap.end(); ++it)
    {
        auto coreBasic = it->second;
        if(coreBasic->getOp() == op)
        {
            opSupport = true;
            break;
        }
    }
    return opSupport;
}

bool PlatformBasic::supportUram () const
{
    return verifyMemImplIsSupported(MEMORY_IMPL_URAM);
}

bool PlatformBasic::verifyMemImplIsSupported (MEMORY_IMPL memImpl) const
{
    bool memImplSupport = false;
    for (auto it = mCoreBasicMap.begin(); it != mCoreBasicMap.end(); ++it)
    {
        auto coreBasic = it->second;
        if(coreBasic->getOp() == OP_MEMORY)
        {
            IMPL_TYPE typeImpl = coreBasic->getImpl();
            MEMORY_IMPL impl = getMemImplEnum(typeImpl);
            if (memImpl == impl)
            {
                memImplSupport = true;
                break;
            }
            
        }
    }
    return memImplSupport;
}

bool PlatformBasic::hasSuccLoad() const
{
    return !mCoreBasicMap.empty();
}

bool PlatformBasic::isVersal()
{
    return mLibraryName.find("versal") != std::string::npos;
}

} //< namespace platform
