// (c) Copyright 2016-2022 Xilinx, Inc.
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
#ifndef LLVM_SUPPORT_XILINXFPGACOREINSTFACTORY_H
#define LLVM_SUPPORT_XILINXFPGACOREINSTFACTORY_H
#include <map>
#include <vector>
#include <string>
#include <set>
#include <tuple>
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/XILINXFPGAPlatformBasic.h"

namespace pf_internal 
{
class Core;
class FuncUnit;
class Storage;
class Adapter;
}

namespace pf_newFE
{
class CoreInstFactory;

/// Core list.
typedef std::vector<pf_internal::Core*> CoreList;
/// Function unit list.
typedef std::vector<pf_internal::FuncUnit*> FuncUnitList;

/// Core list iterator.
typedef CoreList::const_iterator CoreIter;
/// Function unit list iterator.
typedef FuncUnitList::iterator FuncUnitIter;

/// Map: Corename ---> Resource usage
typedef std::map<std::string, double> ResUsageMap;
/// Resource usage iterator.
typedef ResUsageMap::iterator ResUsageIter;

/// Operation name
typedef int OperType;
OperType const AnyOperation = -1;
const platform::PlatformBasic::IMPL_TYPE AnyImpl 
        = static_cast<platform::PlatformBasic::IMPL_TYPE>(-1);
/// Operation set.
typedef llvm::SetVector<OperType> OperSet;
// Operation set iterator.
typedef OperSet::iterator OperIter;

typedef unsigned InterfaceMode;
/// Interface set.
typedef llvm::SetVector<InterfaceMode> InterfaceModes;
// Interface set iterator.
typedef InterfaceModes::iterator InterfaceIter;

// key: latency or bitwidth, value: delay list
typedef std::map<int, std::vector<double>> DelayMap;

class CoreInst;
class FuncUnitInst;

typedef std::vector<std::shared_ptr<CoreInst>> CoreInstList;
typedef std::vector<std::shared_ptr<FuncUnitInst>> FuncUnitInstList;

/// CoreInst list iterator.
typedef CoreInstList::iterator CoreInstIter;
/// Function unit list iterator.
typedef FuncUnitInstList::iterator FuncUnitInstIter;

/**
 * CoreInst is the superclass of the building blocks (e.g. functional
 * units, storage elements, connectors, etc.) of a datapath.
 * It captures the key structure/timing/area attributes of a
 * (configurable) hardware core
 **/
class CoreInst: public std::enable_shared_from_this<CoreInst>
{
public:
    /// CoreInst type.
    enum Type
    {
        Any = -1, /// Any type of core.
        Generic = 1 << 0, /// Generic core.
        FunctionUnit = 1 << 1, /// Functional unit.
        Storage = 1 << 2, /// Memory/storage.
        Connector = 1 << 3, /// Connector.
        IPBlock = 1 << 4, /// Blackbox IP.
        Adapter = 1 << 5, /// Adapter.
        Channel = 1 << 6, /// Channel.
    };
    // Resource type
    enum ResourceTp
    {
        UNKNOWN = -1,
        FF = 1 << 0,
        LUT = 1 << 1,
        BRAM = 1 << 2,
        URAM = 1 << 3,
        DSP = 1 << 4
    };
    friend class CoreInstFactory;
protected:
    /// Constructor.
    CoreInst(pf_internal::Core* core, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~CoreInst();

public:
    /// Set clock under which BE desire this CoreInst to run, and converse.
    void configDelayBudget(double delayBudget ); 
    double getDelayBudget();
    
    /// Get the heterogeneous resource usage map.
    const ResUsageMap& getResourceUsage();

    /// Access the delay.
    std::vector<double> getDelayList();
    
    /// Access the pipeline latency.
    unsigned getPipeLatency();
    
    /// Access the pipeline interval.
    unsigned getPipeInterval(); 

private:
    /// Get the heterogeneous resource usage for a specific resource.
    double getResourceUsageByName(std::string name);

public: 
    std::string getName() const;
    platform::PlatformBasic::OP_TYPE getOp() const;
    platform::PlatformBasic::IMPL_TYPE getImpl() const;
    CoreInst::Type getType() const; 
    float getPreference() const;
    unsigned getId() const;
    unsigned getMaxLatency() const;

protected:
    /// core template
    pf_internal::Core* mCore;
    ResUsageMap mResUsage;

    platform::PlatformBasic::OP_TYPE mOp;
    platform::PlatformBasic::IMPL_TYPE mImpl;

    /// Clock under which this CoreInst run 
    double mClock;
};

/**
 * FuncUnitInst is a subclass of the CoreInst class. It abstracts
 * various types of functional units (e.g., adders, mults, gates, etc.)
**/
class FuncUnitInst: public CoreInst
{
    friend class CoreInstFactory;
protected:
    /// Constructor.
    FuncUnitInst(pf_internal::FuncUnit* fu, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~FuncUnitInst();

public:
    /// config/get operand's output bitwidth.
    void configOutputBW(const int bw);
    int getOutputBW();

    /// config/get operand's input list.
    void configInputBWList(const std::vector<unsigned>& inputlist); 
    std::vector<unsigned> getInputBWList();

public:
    static const Type ClassType = FunctionUnit;

protected:
    /// output
    int mOutput;
    /// Mapped operands' bitwidth list
    std::vector<unsigned> mInputList;
};

/**
 * StorageInst is a subclass of the CoreInst class. It abstracts
 * all types of storage elements (e.g., registers, memories, etc.)
**/
class StorageInst: public CoreInst
{
    friend class CoreInstFactory;
protected:
    /// Constructor.
    StorageInst(pf_internal::Storage* storage, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~StorageInst();

public:
    void setMemUsedPorts(const std::vector<unsigned>& ports);
    const std::vector<unsigned>& getMemUsedPorts() const;

    void configBitWidth(int bw); 
    int getBitWidth() const;

    void configDepth(int depth);
    int getDepth() const;

public: 
    static const Type ClassType = Storage;

public:
    platform::PlatformBasic::MEMORY_IMPL getMemoryImpl() const; 

public: 
    bool isBRAM() const;
    bool isDRAM() const;
    bool isURAM() const;
    bool isVivadoAuto() const;

protected:
    int mBitWidth;
    int mDepth;
    std::vector<unsigned> mMemUsedPorts;
};

class AdapterInst: public CoreInst 
{
    friend class CoreInstFactory;
protected:
    /// Constructor.
    AdapterInst(pf_internal::Adapter* ada, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~AdapterInst();

public:
    unsigned getPipeLatency(OperType PTy); 

public: 
    static const Type ClassType = Adapter;

};

class CoreInstFactory
{
friend class pf_newFE::CoreInst;

public:
    static CoreInstFactory* getInstance();

public:
    CoreInstFactory();

public:
    ~CoreInstFactory();

    /// do not allow copy
    CoreInstFactory(CoreInstFactory const&) = delete; 
    void operator=(CoreInstFactory const&)  = delete;

    CoreInstList getCoreInstsByOper(
        OperType opcode,
        int type_mask = CoreInst::FunctionUnit) const;

    bool requestFuncUnitInstList(
        FuncUnitInstList& funcList, 
        OperType opcode, 
        double clock, 
        std::vector<unsigned>& inputBWList, 
        int outputBW) const;
    
    CoreInstList getMemoryInsts() const;

    CoreInstList getCoreInstsByInterfaceMode(
        InterfaceMode ifmode,
        int type_mask = CoreInst::Any) const;

public:
    int createCores(const std::string& libraryName);

private:
    // Find the cores by the operation.
    CoreList getFuncCores(OperType opcode,
                          int type_mask = -1) const;

    CoreList getMemCores() const;                       

    CoreList getInterfaceCores(InterfaceMode ifmode,
                               int type_mask = -1) const;

    // Convert string to core type.
    static unsigned string2CoreType(std::string ty_str);
    // Convert core type to string.
    // static std::string coreType2String(unsigned type);   
    
private:
    static std::shared_ptr<CoreInst> createCoreInst(pf_internal::Core* core, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl);
    static CoreInstList createCoreInst(pf_internal::Core* core, OperType opcode = AnyOperation);

    void addCore(pf_internal::Core* core);
    void clear();
    void addGenericCore();
    pf_internal::Core* getGenericCore();

private:
    // CoreList 
    std::vector<pf_internal::Core*> mCores;
    /// Core ID counter
    unsigned mCoreId;
    /// Map: ID --> Core*
    std::map<unsigned, pf_internal::Core*> mId2Core;
    /// Map: Name --> ID
    std::map<std::string, unsigned> mName2Id;
    /// Map: ID --> Name(s)
    std::map<unsigned, std::set<std::string> > mId2Names;

private:
    void setName(std::string name);
public:
    std::string getName();

private:
    std::string mName;

}; //< class CoreInstFactory

template <class X>
inline bool inst_isa(std::shared_ptr<const CoreInst> inst)
{
    if(inst != 0 && inst->getType() == X::ClassType)
        return true;
    else
        return false;
}

template <class X>
inline std::shared_ptr<X> dyn_cast(std::shared_ptr<CoreInst> inst)
{
    if(inst_isa<X>(inst))
        return std::static_pointer_cast<X>(inst);
    else
        return 0;
}

template <class X>
inline std::shared_ptr<const X> dyn_cast(std::shared_ptr<const CoreInst> inst)
{
    if(inst_isa<X>(inst))
        return std::static_pointer_cast<X>(inst);
    else
        return 0;
}

template <class X>
inline std::vector<std::shared_ptr<X>> dyn_cast(const CoreInstList& coreInsts)
{
    std::vector<std::shared_ptr<X>> insts;
    for(auto coreInst : coreInsts)
    {
        auto inst = dyn_cast<X>(coreInst);
        if(inst)
        {
            insts.push_back(inst);
        }
    }
    return insts;
} 

// prevent comparison of 2 CoreInst pointers
template<class T, class U>
typename std::enable_if<
std::is_base_of<CoreInst, T>::value && std::is_base_of<CoreInst, U>::value
>::type operator==(const std::shared_ptr<T>&, const std::shared_ptr<U>&) = delete;

template<class T, class U>
typename std::enable_if<
std::is_base_of<CoreInst, T>::value && std::is_base_of<CoreInst, U>::value
>::type operator!=(const std::shared_ptr<T>&, const std::shared_ptr<U>&) = delete;

template<class T, class U>
typename std::enable_if<
std::is_base_of<CoreInst, T>::value && std::is_base_of<CoreInst, U>::value
>::type operator<(const std::shared_ptr<T>&, const std::shared_ptr<U>&) = delete;

void SetPlatformDbFile( std::string path );

struct CoreRanker : public std::binary_function<std::shared_ptr<CoreInst>, std::shared_ptr<CoreInst>, bool>
{
    enum MetricType
    {
        DelayOrder = 0,
        IntervalOrder = 1,
        LatencyOrder = 2,
        PreferenceOrder = 3,
        ThroughputOrder = 4,
        HasMetDelayBudgetOrder = 5
    };

    enum OrderType
    {
        EQ,
        LT,
        GT
    };
    
    std::vector<MetricType> mMetrics;
    OperType mOpcode;
    double mDelayBudget;

    CoreRanker(OperType opcode = AnyOperation, double delayBudget = -1) 
                : mOpcode(opcode), mDelayBudget(delayBudget) { } 
    
    virtual ~CoreRanker() { }

    void setDefaultMetrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::PreferenceOrder); // high priority
        mMetrics.push_back(CoreRanker::IntervalOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // low priority
    }

    void setPipeYes_iiYes_relaxiiYes_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::ThroughputOrder); // high priority
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    void setPipeYes_iiYes_relaxiiNo_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::IntervalOrder); // high priority
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    void setPipeYes_iiNo_relaxiiYes_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::ThroughputOrder); // high priority
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    void setPipeYes_iiNo_relaxiiNo_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::ThroughputOrder); // high priority
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    void setPipeNo_iiNo_relaxiiNo_metrics()
    {
        mMetrics.clear();
        mMetrics.push_back(CoreRanker::HasMetDelayBudgetOrder); // high priority
        mMetrics.push_back(CoreRanker::ThroughputOrder);
        mMetrics.push_back(CoreRanker::LatencyOrder);
        mMetrics.push_back(CoreRanker::DelayOrder); // when II and Latency are tied
    }

    double getMaxDelayFromList(std::shared_ptr<CoreInst> c)
    {
        std::vector<double> delays = c->getDelayList();
        return *std::max_element(delays.begin(),delays.end());
    }

    template <class T>
    OrderType compareInt(T a, T b)
    {
        if (a < b)
            return LT;
        else if (a > b)
            return GT;
        else
            return EQ;
    }

    template <class T>
    OrderType compareFloat(T a, T b)
    {
        if ((a + 1e-10) < b)
            return LT;
        else if (a > (b + 1e-10))
            return GT;
        else
            return EQ;
    }

    // seems no use, delete by changhon
    //static OperType getCoreOper(std::shared_ptr<const CoreInst> core, OperType opcode)
    //{
    //    return opcode;
    //}
    
    OrderType rankByDelay(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        double delay1 = getMaxDelayFromList(c1);
        double delay2 = getMaxDelayFromList(c2); 

        return compareFloat<double>(delay1, delay2);
    }

    OrderType rankByInterval(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        unsigned ii1 = c1->getPipeInterval();
        unsigned ii2 = c2->getPipeInterval();

        // II=0 means the component is not pipelined.
        if (ii1 == 0 && ii2 > 0)
            return GT;
        if (ii2 == 0 && ii1 > 0)
            return LT;

        return compareInt<unsigned>(ii1, ii2);
    }

    OrderType rankByHasMetDelayBudgetOrder(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        unsigned hasMetDelayBudget1 = getMaxDelayFromList(c1) <= mDelayBudget ? 1 : 0;
        unsigned hasMetDelayBudget2 = getMaxDelayFromList(c2) <= mDelayBudget ? 1 : 0;
    
        return compareInt<unsigned>(hasMetDelayBudget1, hasMetDelayBudget2);
    }

    OrderType rankByThroughput(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        double delay1 = getMaxDelayFromList(c1);
        double delay2 = getMaxDelayFromList(c2);
        // not divide by zero
        assert(delay1);
        assert(delay2);

        delay1 = (mDelayBudget > 0 && mDelayBudget > delay1) ? mDelayBudget : delay1;
        delay2 = (mDelayBudget > 0 && mDelayBudget > delay2) ? mDelayBudget : delay2;
        
        unsigned ii1 = c1->getPipeInterval();
        unsigned ii2 = c2->getPipeInterval(); 
        
        // II=0 means the component is not pipelined.
        if (ii1 == 0 && ii2 > 0)
            return LT;
        if (ii2 == 0 && ii1 > 0)
            return GT; 
        if (ii1 == 0 && ii2 == 0)
            return EQ;

        // fix divide by zeros, not functional
        if (delay1 == 0 && delay2 > 0)
            return LT;
        if (delay2 == 0 && delay1 > 0)
            return GT; 
        if (delay1 == 0 && delay2 == 0)
            return EQ;

        double throughput1 = 1/delay1 * 1/(double)ii1;
        double throughput2 = 1/delay2 * 1/(double)ii2;

        return compareFloat<double>(throughput1, throughput2);
    }

    OrderType rankByLatency(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        unsigned lat1 = c1->getPipeLatency();
        unsigned lat2 = c2->getPipeLatency();

        return compareInt<unsigned>(lat1, lat2);
    }
    
    OrderType rankByPreference(std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        float p1 = c1->getPreference();
        float p2 = c2->getPreference();

        return compareFloat<float>(p1, p2);
    }
    
    bool operator() (std::shared_ptr<CoreInst> c1, std::shared_ptr<CoreInst> c2)
    {
        assert(c1 && c2);
        
        for (auto& it : mMetrics)
        {
            // Preference.
            if (it == PreferenceOrder)
            {
                OrderType order = rankByPreference(c1, c2);            
                if (order == GT)
                    return true;
                else if (order == LT)
                    return false;
            }

            // Interval.
            if (it == IntervalOrder)
            {
                OrderType order = rankByInterval(c1, c2);
                if (order == LT)
                    return true;
                else if (order == GT)
                    return false;
            }
            
            // Latency.
            if (it == LatencyOrder)
            {
                OrderType order = rankByLatency(c1, c2);            
                if (order == LT)
                    return true;
                else if (order == GT)
                    return false;
            }

            // Delay.
            if (it == DelayOrder)
            {
                OrderType order = rankByDelay(c1, c2);
                if (order == LT)
                    return true;
                else if (order == GT)
                    return false;
            }

            // Throughput.
            if (it == ThroughputOrder)
            {
                OrderType order = rankByThroughput(c1, c2);
                if (order == GT)
                    return true;
                else if (order == LT)
                    return false;  
            }  

            // HasMetDelayBudget.
            if (it == HasMetDelayBudgetOrder)
            {
                OrderType order = rankByHasMetDelayBudgetOrder(c1, c2);
                if (order == GT)
                        return true;
                else if (order == LT)
                    return false;
            }
        }            

        return (c1->getId() < c2->getId());
    }    
};

} //< namespace pf_newFE

namespace pf_internal 
{


/**
 * Core is the superclass of the building blocks (e.g. functional
 * units, storage elements, connectors, etc.) of a datapath.
 * It captures the key structure/timing/area attributes of a
 * (configurable) hardware core
 **/
class Core
{
public:
    /// Core type.
    enum Type
    {
        Any = -1, /// Any type of core.
        Generic = 1 << 0, /// Generic core.
        FunctionUnit = 1 << 1, /// Functional unit.
        Storage = 1 << 2, /// Memory/storage.
        Connector = 1 << 3, /// Connector.
        IPBlock = 1 << 4, /// Blackbox IP.
        Adapter = 1 << 5, /// Adapter.
        Channel = 1 << 6, /// Channel.
    };
    // Resource type
    enum ResourceTp
    {
        UNKNOWN = -1,
        FF = 1 << 0,
        LUT = 1 << 1,
        BRAM = 1 << 2,
        URAM = 1 << 3,
        DSP = 1 << 4
    };

    enum IOModeType
    {
        ModeAuto = 1 << 0, // Auto mode
        ModeNone = 1 << 1, // None mode
        ModeStable = 1 << 2,  // Stable mode
        ModeVld = 1 << 3, // Valid mode
        ModeOVld = 1 << 4, // Output valid mode
        ModeAck = 1 << 5, // Acknowledge mode
        ModeHS = 1 << 6, // Handshaking mode
        ModeFifo = 1 << 7, // Fifo mode
        ModeMemory = 1 << 8, // Memory mode
        ModeBram = 1 << 9, // bram mode
        ModeBus = 1 << 10, // Bus mode
        ModeAxis = 1 << 11, //Axis mode
        ModeMAXI = 1 << 12, // AXI master mode
        ModeSAXILite = 1 << 13, // AXI lite slave mode
        ModeMemFifo = 1 << 14, //Memory fifo mode
    };
public:
    /// Constructor.
    Core();
    /// Copy constructor.
    Core(const Core& one_core) = delete;
    ///TODO: Destructor.
    virtual ~Core();

protected:
    Core(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference);

public:
    /// Get the core ID.
    unsigned getId() const { return mId; }
    /// Set the core ID.
    void setId(unsigned id) { mId = id; }
    /// Get the core name.
    std::string getName() const { return mName; }

    /// Get the core type.
    Core::Type getType() const { return mType; }
    /// Get the core type.
    std::string getTypeStr() const
    {
        switch (mType)
        {
        case Core::Any:          return "Any";
        case Core::Generic:      return "Generic";
        case Core::FunctionUnit: return "FuncUnit";
        case Core::Storage:      return "Storage";
        case Core::Connector:    return "Connector";
        case Core::IPBlock:      return "IPBlock";
        case Core::Adapter:      return "Adapter";
        case Core::Channel:      return "Channel";
        }
    }
    /// Set the core type.
    void setType(Core::Type type) { mType = type; }

    /// Check if this core is generic (abstract).
    bool isGeneric() const { return (mType == Generic); }

    /// Get the heterogeneous resource usage for a specific resource.
    double getInitResourceUsageByName(std::string name) const;
    /// Get the heterogeneous resource usage map.
    const pf_newFE::ResUsageMap& getInitResourceUsage() const { return mInitResUsage; }

    /// Get the operation set.
    const pf_newFE::OperSet& getOpers() const { return mOperSet; }
    /// Match an operation.
    /// @param op Operation name.
    /// @return Returns true if the core is capable of this operation.
    virtual bool matchOper(pf_newFE::OperType op) const;

    /// Check if it is a valid operation.
    static bool isValidOper(pf_newFE::OperType op);
 
    virtual bool matchInterfaceMode(pf_newFE::InterfaceMode mode) const;

    /// get impl of this core
    platform::PlatformBasic::IMPL_TYPE getImpl() const;

    // std::string getDelayFunction() const {
    //     return mDelayFunction; 
    // }
    bool hasDelayFunction() const {
        return !mDelayFunction.empty();
    }

    std::string getLatencyFunction() const {
        return mLatencyFunction;
    }
    bool hasLatencyFunction() const {
        return !mLatencyFunction.empty();
    }

    // std::string getIntervalFunction() const {
    //     return mIntervalFunction;
    // }
    // bool hasIntervalFunction() const {
    //     return !mIntervalFunction.empty();
    // }

    /// Resource usage lookup function.
    std::string getResUsageFunction() const {
        return mResUsageFunction;
    }
    bool hasResUsageFunction() const {
        return !mResUsageFunction.empty();
    }
 
    /// Legality check function.
    // std::string getLegalityFunction() const {
    //     return mLegalityFunction;
    // }
    // bool hasLegalityFunction() const {
    //     return !mLegalityFunction.empty();
    // }

    /// Preference function.
    // std::string getPreferenceFunction() const {
    //     return mPreferenceFunction;
    // }
    // bool hasPreferenceFunction() const {
    //     return !mPreferenceFunction.empty();
    // }

    /// Core generator.
    std::string getGenerator() const {
        return mGenerator;
    }
    bool hasGenerator() const {
        return !mGenerator.empty();
    }

    /// Get the port name map.
    const std::map<std::string, std::string>& getPortNameMap() const;

    // TODO: 
    void configImplStyle(std::string implStyle) {
        mImplStyle = implStyle;
    }
    std::string getImplStyle() const {
        return mImplStyle;
    }

    /// Get/set description.
    std::string getDescription() const {
        return mDescription;
    }

    std::string getTargetComponent() const {
        return mTargetComponent;
    }

    float getFixedPreference(pf_newFE::OperType opcode) {
        return mPreference;
    }

    /// Access the delay List.
    std::vector<double> getFixedDelayList(pf_newFE::OperType opcode) {
        return mDelayList;
    }

    /// Access the pipeline latency.
    unsigned getFixedPipeLatency() {
        return mPipeLatency;
    }

    /// Access the pipeline interval.
    unsigned getFixedPipeInterval() {
        return mPipeInterval;
    }

    /// Set/Check if the core is hidden for the scheduler.
    bool isHidden() const { return mIsHidden; }

    /// Set/Check if the core has a delay list
    bool isDelayList() const { return mIsDelayList; }

    // BlackBox core flag
    bool isBlackBoxIPCore() const;
    unsigned string2ResourceTp(std::string res_name);
    
    // Latency set by TCL config_core command
    void setConfigCmdLatency(int config_lat) { mConfigCmdLat = config_lat; }
    int getConfigCmdLatency() const { return mConfigCmdLat; }

    // get user latency, priority: bind_op > config_core > config_op, for query_core -config_latency

protected:
    // return str.split(sep)
    std::vector<std::string> getTclList(const std::string& str, const std::string& sep=" ");

    bool isNumber(const std::string& s);

    static unsigned string2Mode(std::string str);
private:
    /// Clear the STL containers.
    // void clear();

protected:
    /// Core ID
    unsigned mId;
    /// Core name
    std::string mName;
    /// Core type
    Type mType;
    /// Operation set
    pf_newFE::OperSet mOperSet;

    pf_newFE::InterfaceModes mIfModes;

    std::string mImplStyle;

    /// Description
    std::string mDescription;

    /// Target component
    std::string mTargetComponent;

    // == Preference metric
    float mPreference;
    // == Timing metrics
    /// Logic delay list
    std::vector<double> mDelayList;
    /// Pipeline latency
    unsigned mPipeLatency;
    /// Pipeline latency list
    std::vector<unsigned> mPipeLatencyList;
    /// Pipeline interval
    unsigned mPipeInterval;
    /// Input port stages
    std::vector<unsigned> mPortStages;
    // == Area metrics
    /// Hetro resource usage map
    pf_newFE::ResUsageMap mInitResUsage;

    /// Delay lookup function
    std::string mDelayFunction;
    /// Latency lookup function
    std::string mLatencyFunction;
    /// Interval lookup function
    std::string mIntervalFunction;
    /// Resource usage lookup function
    std::string mResUsageFunction;
    /// Function for the legality test
    std::string mLegalityFunction;
    /// Function for the preference test
    std::string mPreferenceFunction;
    /// Generation routine.
    std::string mGenerator;
    
    /// Port name mapping
    std::map<std::string, std::string> mPortNameMap;

    ///
    bool mIsHidden;

    ///
    bool mIsDelayList;

    int mConfigCmdLat = -1;

};

/**
 * FuncUnit is a subclass of the Core class. It abstracts
 * various types of functional units (e.g., adders, mults, gates, etc.)
**/
class FuncUnit: public Core
{
    friend class pf_newFE::CoreInstFactory;

public:
    /// Constructor.
    FuncUnit() = delete;
    FuncUnit(const FuncUnit& other) = delete;
    ///Destructor.
    virtual ~FuncUnit();

protected:
    FuncUnit(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList,
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // func unit
         int inputs, int outputs, const std::string& portStageFunction);
};

/**
 * Storage is a subclass of the Core class. It abstracts
 * all types of storage elements (e.g., registers, memories, etc.)
**/
class Storage: public Core 
{
    friend class pf_newFE::CoreInstFactory;

public:
    /// Constructor.
    Storage() = delete;
    Storage(const Storage& other) = delete;
    /// Destructor.
    virtual ~Storage();

protected:
    Storage(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // storage
         int depth, const std::string& interfaces, const std::string& ports);

public:
    platform::PlatformBasic::MEMORY_IMPL getMemoryImpl() const;
};

/*
 * Adapter is a subclass of the Core class. It abstracts
 * user-specified adapters (FIFO, shared memory, bus, etc.)
 */
class Adapter: public Core
{
    friend class pf_newFE::CoreInstFactory;

public:
    /// Constructors.
    Adapter() = delete;
    Adapter(const Adapter& other) = delete;
    /// Destructor.
    virtual ~Adapter();

protected:
    Adapter(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // adapter
         const std::string& interfaces);
    
};

typedef struct {
    Core::IOModeType modeId;
    const char* modeName;
} IOModeDescType;

static const IOModeDescType IOModeDesc[] = {
    { Core::IOModeType::ModeNone, "ap_none" },
    { Core::IOModeType::ModeStable, "ap_stable" },
    { Core::IOModeType::ModeVld, "ap_vld" },
    { Core::IOModeType::ModeOVld, "ap_ovld" },
    { Core::IOModeType::ModeAck, "ap_ack" },
    { Core::IOModeType::ModeHS, "ap_hs" },
    { Core::IOModeType::ModeFifo, "ap_fifo" },
    { Core::IOModeType::ModeMemory, "ap_memory" },
    { Core::IOModeType::ModeBram, "bram" },
    { Core::IOModeType::ModeBus, "ap_bus" },
    { Core::IOModeType::ModeAuto, "ap_auto" },
    { Core::IOModeType::ModeAxis, "axis"},
    { Core::IOModeType::ModeMAXI, "m_axi"},
    { Core::IOModeType::ModeSAXILite, "s_axilite"},
    { Core::IOModeType::ModeMemFifo, "mem_fifo"},
};

} // end of namespace pf_internal
#endif //LLVM_SUPPORT_XILINXFPGACOREINSTFACTORY_H 
