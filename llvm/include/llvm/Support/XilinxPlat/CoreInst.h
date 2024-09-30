

#ifndef _PLATFORM_CoreInst_H
#define _PLATFORM_CoreInst_H

#include <map>
#include <vector>
#include <string>
#include <tuple>
#include <set>
#include <algorithm>
#include <assert.h> 
#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XILINXFPGAPlatformBasic.h"
#include "llvm/Support/XilinxPlat/PortAndExprTreeNode.h"
#else
#include "XILINXFPGAPlatformBasic.h"
#include "PortAndExprTreeNode.h"
#endif

class ConfigOpHelper;
class CreatePlatformCmd;
class QueryCoreCmd;

namespace platform_agent {
class CoreAgent;
}

namespace pf_internal 
{
class Core;
class Storage;
class FuncUnit;
class Channel;
class IPBlock;
class Adapter;
//class ResLibHelper;
}

namespace platform 
{
class CoreInstFactory;
class ConfigedLib;
class CoreQuerier;
class TargetPlatform;
struct CoreDef;

/// Core list.
typedef std::vector<pf_internal::Core*> CoreList;
/// Function unit list.
typedef std::vector<pf_internal::FuncUnit*> FuncUnitList;
/// Storage list.
typedef std::vector<pf_internal::Storage*> StorageList;
/// IP block list.
typedef std::vector<pf_internal::IPBlock*> IPBlockList;
/// Adapter list.
typedef std::vector<pf_internal::Adapter*> AdapterList;

/// Core list iterator.
typedef CoreList::const_iterator CoreIter;
/// Function unit list iterator.
typedef FuncUnitList::iterator FuncUnitIter;
/// Storage list iterator.
typedef StorageList::iterator StorageIter;
/// IPBlock list iterator.
typedef IPBlockList::iterator IPBlockIter;
/// Adapter list iterator.
typedef AdapterList::iterator AdapterIter;

/// Map: Corename ---> Resource usage
typedef std::map<std::string, double> ResUsageMap;
/// Resource usage iterator.
typedef ResUsageMap::iterator ResUsageIter;

/// Operation name
typedef int OperType;
const int AnyLatency = -1;
const PlatformBasic::IMPL_TYPE AnyImpl 
        = static_cast<PlatformBasic::IMPL_TYPE>(-1);
/// Operation set.
typedef std::set<OperType> OperSet;
// Operation set iterator.
typedef OperSet::iterator OperIter;

typedef unsigned InterfaceMode;
/// Interface set.
typedef std::set<InterfaceMode> InterfaceModes;
// Interface set iterator.
typedef InterfaceModes::iterator InterfaceIter;
// Auto interface mode.
InterfaceMode const AutoInterfaceMode = 0;

class CoreInst;
class FuncUnitInst;
class StorageInst;
class ChannelInst;
class IPBlockInst;
class AdapterInst;

typedef std::vector<std::shared_ptr<CoreInst>> CoreInstList;
typedef std::vector<std::shared_ptr<FuncUnitInst>> FuncUnitInstList;
typedef std::vector<std::shared_ptr<StorageInst>> StorageInstList;
/// ChannelInst List
typedef std::vector<std::shared_ptr<ChannelInst>> ChannelInstList;
/// IPBlockInst list.
typedef std::vector<std::shared_ptr<IPBlockInst>> IPBlockInstList;
/// AdapterInst list.
typedef std::vector<std::shared_ptr<AdapterInst>> AdapterInstList;

/// CoreInst list iterator.
typedef CoreInstList::iterator CoreInstIter;
/// Function unit list iterator.
typedef FuncUnitInstList::iterator FuncUnitInstIter;
/// StorageInst list iterator.
typedef StorageInstList::iterator StorageInstIter;
/// ChannelInst list iterator.
typedef ChannelInstList::iterator ChannelInstIter;
/// IPBlockInst list iterator.
typedef IPBlockInstList::iterator IPBlockInstIter;
/// AdapterInst list iterator.
typedef AdapterInstList::iterator AdapterInstIter;

// Wild card for matching operation names
//OperType const AnyOperation = -1;

class CoreCost {
friend class CoreInstFactory;
public:
    virtual double getDelay(OperType oper=AnyOperation);
    virtual double getFirstStageDelay(OperType oper=AnyOperation);
    virtual double getLastStageDelay(OperType oper=AnyOperation);
    virtual unsigned getPipeLatency(OperType oper=AnyOperation); 
    virtual const ResUsageMap& getResourceUsage();
    virtual double getResourceUsageByName(std::string name);

protected:
    void setResUsage(const ResUsageMap& resUsage); 
    void setDelayList(const std::vector<double>& delayList); 
    void setPipeLatency(const int latency); 

    ResUsageMap mCoreCostResUsage;
    std::vector<double> mCoreCostDelayList;
    int mCoreCostPipeLatency;

};

class QueryCoreCost {
public:
    static std::vector<double> getCoreCostDelayList(CoreInst *core, OperType oper=AnyOperation);
    static unsigned getCoreCostPipeLatency(CoreInst *core, OperType oper=AnyOperation);
    static ResUsageMap getCoreCostResourceUsage(CoreInst *core);
};


/**
 * CoreInst is the superclass of the building blocks (e.g. functional
 * units, storage elements, etc.) of a datapath.
 * It captures the key structure/timing/area attributes of a
 * (configurable) hardware core
 **/
class CoreInst: public CoreCost
{
public:
    /// CoreInst type.
    enum Type
    {
        Any = -1, /// Any type of core.
        Generic = 1 << 0, /// Generic core.
        FunctionUnit = 1 << 1, /// Functional unit.
        Storage = 1 << 2, /// Memory/storage.
        IPBlock = 1 << 3, /// Blackbox IP.
        Adapter = 1 << 4, /// Adapter.
        Channel = 1 << 5, /// Channel.
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
    friend class QueryCoreCost;
protected:
    /// Constructor.
    CoreInst(pf_internal::Core* core, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~CoreInst();

public:
    /// Get the core ID.
    /// for CoreRanker
    unsigned getId() const;
    /// Get the core name, only for report.
    std::string getName() const;

    /// check if 2 coreInst share the same mCore.
    bool isEquivalent(std::shared_ptr<CoreInst> other) const;

    /// Get the core type.
    CoreInst::Type getType() const; 
    /// Get the core type.
    std::string getTypeStr() const;

    /// Check if this core is generic (abstract).
    bool isGeneric() const; 

    /// get CoreInst's max latency
    virtual unsigned getMaxLatency(OperType opcode);
    /// get CoreInst's min latency
    unsigned getMinLatency(OperType opcode) const;

    /// Get the operation set.
    const OperSet& getOpers() const;
    /// Set the operation set.
    /// Add an operation to the operation set.
    /// Match an operation.
    /// @param op Operation name.
    /// @return Returns true if the core is capable of this operation.
    bool matchOper(OperType op) const;

    // get OP+Impl of this inst
    PlatformBasic::OP_TYPE getOp() const { return mOp; }
    PlatformBasic::IMPL_TYPE getImpl() const { return mImpl; }
    bool isValidLatency(int latency) const;

    /// Get interface modes.
    const InterfaceModes& getInterfaceModes() const; 
    /// Match an interface mode.
    bool matchInterfaceMode(InterfaceMode Mode) const;
    std::string getResUsageFunction() const;
    std::string getLatencyFunction() const;

    bool getLegality();


    /// CoreInst generator.
    std::string getGenerator() const;
    bool hasGenerator() const;

    /// Get the port name map.
    const std::map<std::string, std::string>& getPortNameMap() const;

    /// Set clock under which BE desire this CoreInst to run, and converse.
    void configDelayBudget(double delayBudget ) { mClock = delayBudget; }
    double getDelayBudget() const { return mClock; }
    
    /// Set preferred latency on this CoreInst, and converse. 
    void configLatencyBudget(int preferredLatency ) { mPreferredLatency = preferredLatency; }
    int getLatencyBudget() const { return mPreferredLatency; }
    
    /// Get/set the implementation style
    //@NOTE: implStyle is an attribute for core, that is, always configured to Core object
    void configImplStyle(std::string implStyle);
    std::string getImplStyle() const;

    /// Get description.
    std::string getDescription() const;
    /// Get target component.
    std::string getTargetComponent() const; 

    // Metrics
    float getPreference(OperType PTy);

    virtual unsigned getPipeInterval(OperType PTy=AnyOperation); 
    /// Access the input port stages, for dsp_builtin
    std::vector<unsigned> getPortStages(OperType PTy); 

    /// Access the power.
    // double getDynamicPower(); 
    double getDynamicPower(OperType PTy); 
    // double getLeakagePower(); 
    double getLeakagePower(OperType PTy); 

    unsigned string2ResourceTp(std::string res_name) const;

    // BlackBox core flag
    bool isBlackBoxIPCore() const;

    // get user latency, priority: fixed latency > bind_op > config_core > config_op
    virtual int getConfigedLatency() const;

public:
    // void print(std::ostream& out);
    std::string print(int user_lat = -1);

protected:
    // configCoreWithCoreInstInfo
    // virtual void configAttributes();

    // get RTL template 
    std::string selectRTLTemplate(std::string op, int signed0, int signed1, int stageNum) const;
protected:
    /// core template
    pf_internal::Core* mCore;

    PlatformBasic::OP_TYPE mOp;
    PlatformBasic::IMPL_TYPE mImpl;

    /// Clock under which this CoreInst run 
    double mClock;

    /// FE/BE Preferred latency of this CoreInst
    int mPreferredLatency;
};

/**
 * StorageInst is a subclass of the CoreInst class. It abstracts
 * all types of storage elements (e.g., registers, memories, etc.)
 **/
class StorageInst: public CoreInst
{
    friend class CoreInstFactory;
    friend class NPMemoryQuerier;
    friend class ChannelQuerier;
    friend class AdapterQuerier;
protected:
    /// Constructor.
    StorageInst(pf_internal::Storage* storage, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~StorageInst();

public:
    const CPortList& getMemPorts() const;
    /// Get the memory used port map
    const std::vector<unsigned>& getMemUsedPorts() const;
    /// Set the memory used port map.
    void setMemUsedPorts(const std::vector<unsigned>& ports);
    /// Get the number of ports.
    unsigned getNumPorts() const;
    /// Get the port by index.
    MemPort* getPortByIndex(unsigned index) const;

    int getBitWidth() const { return mBitWidth; }

    ///@WARNING special for core Register, which has a fiexd dpeth = 1
    int getDepth() const;
    // TODO:Not used.
    // void configBanksNum(int num) { mBanksNum = num; }
    int getBanksNum() const { return mBanksNum; }

    // for BRAM ecc mode, has init or not leads to different resource usage
    
    bool hasInit() const { return mHasInit; }
    /// Get the total capacity of the memory (cell#).
    unsigned getNumCells() const { return getBitWidth() * getDepth(); }
    bool getReset() const { return mReset; }

    // Only for 1WNR, we define required number of read ports
    unsigned get1WNRRequiredNumPorts() { 
        return (is1wnrRAM() ? m1WNRRequiredNumPorts : 0); }
    void set1WNRRequiredNumPorts(unsigned requiredNumPorts) { 
        m1WNRRequiredNumPorts = requiredNumPorts; }
    void clear1WNRRequiredNumPorts() { m1WNRRequiredNumPorts = 0; }

public:
    // Is storage readonly?
    bool isReadOnly() const;
    // Is it a ROM?
    bool isROM() const;
    // Is it a NPROM?
    bool isNPROM() const;
    // Is it a RAM?
    bool isRAM() const;
    // Is it a NPRAM?
    bool isNPRAM() const;
    // Is it a register?
    bool isRegister() const;
    // Is it a FIFO?
    bool isFifo() const;
    // Is it a shift memory?
    bool isShiftMem() const;
    // Is it a 1WnR memory?
    bool is1wnrRAM() const;
    // Is it a ECC RAM?
    bool isEccRAM() const;
    // Does storage support Byte Write Enable (For FE)
    bool supportByteEnable() const;
    // Is storage initializable (For FE)
    bool isInitializable() const;

    PlatformBasic::MEMORY_TYPE getMemoryType() const;
    PlatformBasic::MEMORY_IMPL getMemoryImpl() const;

    // following 4 functions are for RAM/ROM, not including FIFO, ShiftReg, register
    bool isURAM() const;
    bool isBRAM() const;
    bool isDRAM() const;
    bool isAuto() const;

    // get user latency, priority: pragma bind_storage > config_core > config_storage
    virtual int getConfigedLatency() const;

    int getFIFORawLatency(bool showMsg=false) const;

    // for memories bundled to Adapter
    StorageInstList getSupportedInnerMemInstList() const;
    void pushBackInnerMemInst(std::shared_ptr<StorageInst> inst);

// for CoreQuerier to get Adapter Inner Storage.
    const StorageInstList& getInnerMemInstList() const;

public:
    static const Type ClassType = Storage;
   
private:
    // virtual void configAttributes();
    void configBitWidth(int bw) { mBitWidth = bw; }
    void configDepth(int depth) { mDepth = depth; }
    void configInit(bool hasInit) { mHasInit = hasInit; }
    // set reset mode
    void configReset(bool reset) { mReset = reset; }
private:
    StorageInstList mBundledMemories;
    
protected:
    /// Bitwidth
    int mBitWidth;
    /// Depth
    int mDepth;
    /// BanksNum
    //TODO: Not used now. 
    int mBanksNum;
    //  Used Port
    std::vector<unsigned> mMemUsedPorts;

    bool mHasInit;
    /// need reset?
    bool mReset;

    // For 1WNR, we need required number of ports
    unsigned m1WNRRequiredNumPorts;
};

/**
 * FuncUnitInst is a subclass of the CoreInst class. It abstracts
 * various types of functional units (e.g., adders, mults, gates, etc.)
 */
class FuncUnitInst: public CoreInst
{
    friend class CoreInstFactory;

protected:
    /// Constructor.
    FuncUnitInst(pf_internal::FuncUnit* fu, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~FuncUnitInst();

public:
    int getOutputBW() const { return mOutput; }

    std::vector<unsigned> getInputBWList() const { return mInputList; }

    /// Get the input ports.
    CPortList& getInPorts();
    const CPortList& getInPorts() const;
    /// Get the output ports.
    CPortList& getOutPorts();
    const CPortList& getOutPorts() const;

    /// Get the number of input ports.
    unsigned getNumInPorts()  const;
    /// Get the number of output ports.
    unsigned getNumOutPorts() const;

    /// Get the input port by index.
    FuPort* getInPortByIndex(unsigned index);
    /// Get the output port by index.
    FuPort* getOutPortByIndex(unsigned index);
    /// For SparseMux 
    bool getSparseMuxIncrEncoding() const { return mSparseMuxIncrEncoding; }

public:
    static const Type ClassType = FunctionUnit;

protected:
    void configInputBWList(const std::vector<unsigned>& inputlist) { mInputList = inputlist; }
    void configOutputBW(int bw) { mOutput = bw; }

    /// For SparseMux 
    void configSparseMuxIncrEncoding(bool label) { mSparseMuxIncrEncoding = label; }
protected:
    /// output
    int mOutput;
    /// Mapped operands' bitwidth list
    std::vector<unsigned> mInputList;

    /// Input ports
    platform::CPortList mInPorts;
    /// Output ports
    platform::CPortList mOutPorts;

    /// For SparseMux 
    bool mSparseMuxIncrEncoding;
};

/**
 * @brief ChannelInst is a subclass of the CoreInst class. It abstracts
 * Channel based IP, including FIFOs(TODO), MISO, SIMO, etc. 
 */
class ChannelInst: public CoreInst
{
public:
    enum CHANNEL_ALG_TYPE
    {
        LOAD_BALANCE = 0,
        ROUND_ROBIN = 1,
        TAG_SELECT = 2
    };

    friend class CoreInstFactory;
protected:
    /// Constructor.
    ChannelInst(pf_internal::Channel* chann, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~ChannelInst() { }

public:
    int getDepthIn() const { return mDepthIn; }
    int getDepthOut() const { return mDepthOut; }

    PlatformBasic::MEMORY_IMPL getChannelMemoryImplBySize(int depth); // accoridng to size(bitwidth * depth)
    
    StorageInstList getSupportedInnerMemInstList() const;
    void ConfigInnerMemInst(std::shared_ptr<StorageInst> inst) { mInnerMemories.push_back(inst); }
    // for CoreQuerier to get IP Inner Storage.
    const StorageInstList& getInnerMemInstList() const { return mInnerMemories; }

    /// Get/set the bitwidth.
    int getBitWidth() const { return mBitWidth; }
    
    int getNumInputs() const { return mNumInputs; }
    /// Get/set the number of outputs.
    int getNumOutputs() const { return mNumOutputs; }

public:
    static const Type ClassType = Channel;

private:
    // virtual void configAttributes();
    void configDepthIn(int depth) { mDepthIn = depth; }
    void configDepthOut(int depth) { mDepthOut = depth; }
    // virtual void configAttributes();
    void configBitWidth(int bw) { mBitWidth = bw; }
    void configNumInputs(int num) { mNumInputs = num; }
    void configNumOutputs(int num) { mNumOutputs = num; }

private:
    StorageInstList mInnerMemories;

protected:
    int mDepthIn;
    int mDepthOut;
    /// Bitwidth
    int mBitWidth;
    /// Number of inputs
    int mNumInputs;
    /// Number of outputs
    int mNumOutputs;
};

/**
 * IPBlockInst is a subclass of the CoreInst class. It abstracts
 * user-specified IP blocks (combinational, asynchronous, pipelined.)
 */
class IPBlockInst : public FuncUnitInst
{
public:
    enum IPBType
    {
        IPB_Default = 0,// default value for all IPBlockInst IP core

        // DSP normal start
        DSP_Mul,//{mul a b}
        DSP_AM,//{mul {add d a} b}, {mul {sub d a} b}, 
        DSP_MAC,//{sub {mul a b} c}, {sub c {mul a b}}, {add c {mul a b}},  
        DSP_AMA,//{add c {mul {add d a} b}}, {sub c {mul {add d a} b}}, {sub {mul {add d a} b} c}, {add c {mul {sub d a} b}}, {sub {mul {sub d a} b} c}, {sub c {mul {sub d a} b}}
        DSP_Sel_MAC,//{select {sub {mul a b} c} {add {mul a b} c}}, {select {add {mul a b} c} {sub {mul a b} c}}, {select {sub c {mul a b}} {add c {mul a b}}}, {select {add c {mul a b}} {sub c {mul a b}}}
        DSP_Square_AM,
        DSP_Square_AMA,
        DSP_Acc_MAC,
        DSP_Acc_AMA,
        // DSP tenary start
        DSP_Tenary = 21,
        // DSP QAddSub start
        DSP_QAddSub = 31,
        // DSP58 dot product start
        DSP_Dot = 41,
        DSP_DotAdd,
        //DSP_DotAcc,
        // DSP58 complex multipiler start
        //DSP_Complex = 51,
    };
    enum LogicType
    {
        LogicUndef, /// Undefined logic
        LogicAsync, /// Asynchronous sequential logic
        LogicSync /// Synchronous sequental logic
    };

    enum DSPBuiltinOpcode {
        DSP_mul_add         = 0,
        DSP_mul_sub         = 1,
        DSP_mul_rev_sub     = 2,
        DSP_add_mul_add     = 3,
        DSP_add_mul_sub     = 4,
        DSP_sub_mul_add     = 5,
        DSP_sub_mul_sub     = 6,
        DSP_mul_acc         = 7,
        DSP_add_mul_acc     = 8,
        DSP_sub_mul_rev_sub = 9,
        DSP_full_mul_add    = 10,
        DSP_full_add_mul_add = 11
    };

    friend class CoreInstFactory;
    friend class platform_agent::CoreAgent;
protected:
    /// Constructor.
    IPBlockInst(pf_internal::IPBlock* ip, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~IPBlockInst();

public:

    /// Get IP logic type.
    LogicType getLogicType() const;

    /// Is it an asynchronous logic?
    bool isAsyncLogic() const { return (getLogicType() == LogicAsync); }
    /// Is it a synchronous logic?
    bool isSyncLogic() const { return !isAsyncLogic(); }

    /// Check if the delay of this core is unknown.
    bool hasUnknownDelay();
    /// Check if the latency of this core is unknown.
    bool hasUnknownLatency();

    /// Get the ith supported expression.
    ExprTreeNode_shared_ptr getExpression(unsigned i = 0);
    /// Get the vector of expressions.
    const std::vector<ExprTreeNode_shared_ptr>& getExpressions() const;

    /// Set/Check if it's a Vivado IP
    void setVivadoIP(bool flag) { mIsVivadoIP = flag; }
    bool isVivadoIP() const { return mIsVivadoIP; }

    /// distinguish our own functional IP with VivadoIP
    bool isFuncIP() const;

    /// distinguish our own DSP IP core, QAddSub_DSP
    bool isDSPIP() const;

    /// Get c reg. Note: only for core DSP48
    bool getCReg() const { return mCReg; }

    /// Get acc mode. Note: only for core DSP48
    bool getAcc() const { return mAcc; }

    unsigned getIPBType() const { return mIPBType; }
    std::string getIPBTypeName() const;
    
    unsigned getNodeRef() const { return mNodeRef; }
    
    std::string getMetadata() const { return mMetadata; }

    /// Access the pipeline latency list, for QAddSub_DSP
    std::vector<unsigned> getPipeLatencyList(OperType PTy); 
    
    std::string getBlackBoxName() const { return mBlackBoxName; }

    virtual double getDelay(OperType oper=AnyOperation) override;
    virtual double getFirstStageDelay(OperType oper=AnyOperation) override;
    virtual double getLastStageDelay(OperType oper=AnyOperation) override;
    virtual unsigned getPipeLatency(OperType oper=AnyOperation) override;
    virtual const ResUsageMap& getResourceUsage() override;
    virtual double getResourceUsageByName(std::string name) override;
    double getInputPortDelay(unsigned portId);
    double getOutputPortDelay(unsigned portId);
    unsigned getInputPortStage(unsigned portId);
    unsigned getOutputPortStage(unsigned portId);

    virtual unsigned getPipeInterval(OperType PTy=AnyOperation) override; 
private: 
    // set c reg. Note: only for core DSP48
    void configCReg(bool creg) { mCReg = creg; }
    /// set acc mode. Note: only for core DSP48
    void configAcc(bool acc) { mAcc = acc; }
    void setIPBType(unsigned type) { mIPBType = type; }
    void setNodeRef(unsigned ref) { mNodeRef = ref; }
    void configMetadata(std::string data) { mMetadata = data; }
    void setBlackBoxName(std::string name) { mBlackBoxName = name; }


    // TODO: 
    // void configOutputBW(int bw) { mOutput = bw; }

    // /// config/get operand's input list.
    // void configInputBWList(const std::vector<unsigned>& inputlist) { mInputList = inputlist; }
public:
    static const Type ClassType = IPBlock;

private:
    /// Is a Vivado IP
    bool mIsVivadoIP;

//protected:
//    virtual void configAttributes();

protected:
    unsigned mIPBType;
    // 0 for normal DP node; 1 for DP ref node; 2 for DSP root node.
    // when DSP_Dot, root node is DP ref node, this value is also 1.
    unsigned mNodeRef;

    /// C Reg, only for core DSP48
    bool mCReg;

    /// Acc mode, only for core DSP48
    bool mAcc;

    /// Additional metadata.
    std::string mMetadata;

    /// black box performance related functions and variables
    std::string mBlackBoxName;
};

/*
 * AdapterInst is a subclass of the CoreInst class. It abstracts
 * user-specified adapters (FIFO, shared memory, bus, etc.)
 */
class AdapterInst: public CoreInst
{
    friend class CoreInstFactory;
protected:
    /// Constructors.
    AdapterInst(pf_internal::Adapter* adapter, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl);
public:
    /// Destructor.
    virtual ~AdapterInst(); 

public:
    enum MAXIParaKey
    {
        // common parameters
        NumReadOutstanding = 0,
        NumWriteOutstanding,
        MaxReadBurstLen,
        MaxWriteBurstLen,
        LsuFifoImpl,
        UserLatency,
        BusWidth,
        MaxReadBuffSize,
        MaxWriteBuffSize,
        // channel parameters
        ChanIOType = 100,
        ChanPortWidth,
        // cache parameters
        CacheType = 200,
        CacheImpl,
        CacheLineNum,
        CacheLineDepth,
        CacheLineWays,
        CacheLineWidth
    };
    enum CacheType
    {
        None = 0,
        ReadOnlyCache,
        WriteOnlyCache,
        ReadWriteCache
    };
    enum ChanIOType
    {
        ReadWrite = 0,
        ReadOnly,
        WriteOnly
    };

    /// Get/set the bitwidth.
    void configBitWidth(int bw) { mBitWidth = bw; }
    int getBitWidth() const { return mBitWidth; }
    /// set and get AXILite ports info
    const std::vector<std::vector<unsigned> >& getAXILitePortsVec() const { return mAXILitePortsVec; }
    void setAXILitePortsVec( std::vector<std::vector<unsigned> >& portsVec) { mAXILitePortsVec = portsVec; } 

    /// set and get Adapter ports info
    const CPortList& getAdapterPorts() const { return mAdapterPorts; }
    void setAdapterPorts( CPortList& ports) { mAdapterPorts = ports; }

    /// Get maxi parameters list.
    const std::map<unsigned, unsigned>& getMAXIParaMap() const { return mMAXIParaMap; }
    void setMAXIParaMap(std::map<unsigned, unsigned>& pm) { mMAXIParaMap = pm; }

    /// Get multi-channel maxi parameters list.
    const std::map<unsigned, std::map<unsigned, unsigned> >& getMAXIChanParaMap() const { return mMAXIChanParaMap; }
    void setMAXIChanParaMap(std::map<unsigned, std::map<unsigned, unsigned> >& pm) { mMAXIChanParaMap = pm; }

    /// enable Adapter IO regslice, currently only avaible for m_axi
    void configIORegslice(bool config)  { mIORegslice = config; }
    bool enableIORegslice() const { return mIORegslice; }

    /// for MultChannel MAXI adapter 
    bool isMultiChannelMAXI() const;

    /// for cache type check
    bool enableReadOnlyCache() const;

    /// for memories bundled to Adapter
    StorageInstList getSupportedInnerMemInstList() const;
    void pushBackInnerMemInst(std::shared_ptr<StorageInst> inst);

    /// for CoreQuerier to get Adapter Inner Storage.
    const StorageInstList& getInnerMemInstList() const;
    
    virtual double getDelay(OperType oper=AnyOperation) override;
    virtual double getFirstStageDelay(OperType oper=AnyOperation) override;
    virtual double getLastStageDelay(OperType oper=AnyOperation) override;
    virtual unsigned getPipeLatency(OperType oper=AnyOperation) override;
    virtual const ResUsageMap& getResourceUsage() override;
    virtual double getResourceUsageByName(std::string name) override;

private:
    StorageInstList mBundledMemories;

//protected:
//    virtual void configAttributes();

public:
    static const Type ClassType = Adapter;

protected: 
    /// Bitwidth
    int mBitWidth;
    /// AXILite Ports info
    std::vector<std::vector<unsigned>> mAXILitePortsVec;
    /// Adapter ports info (currently for m_axi only)
    CPortList mAdapterPorts;
    /// MAXI para map
    std::map<unsigned, unsigned> mMAXIParaMap;
    /// MAXI channel para map
    std::map<unsigned, std::map<unsigned, unsigned> > mMAXIChanParaMap;
    /// IO Regslice Enable
    bool mIORegslice;
};

// information and interfaces for commands config_op/config_storage
class ConfigedLib
{
    friend class CoreInstFactory;

public:
    // singleton
    static ConfigedLib& getInstance() { static ConfigedLib cl; return cl; }
    static ConfigedLib* getPointer() { return &(getInstance()); }

private:
    ConfigedLib() {}
    ~ConfigedLib() {}

    friend class pf_internal::Core;
    friend class pf_internal::Storage;
    friend class ::ConfigOpHelper;
private:
    // config_op
    void setConfigOp(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl, int latency = -1 );
    void resetConfigOp(PlatformBasic::OP_TYPE op);
    // config_storage
    void setConfigStorage(PlatformBasic::MEMORY_TYPE type, PlatformBasic::MEMORY_IMPL impl, int latency = -1);
    void resetConfigStorage(PlatformBasic::MEMORY_TYPE type);

public: // Following getters made public in order for DSPMapping to retrieve Mul_DSP latency and apply onto DSP48
    PlatformBasic::IMPL_TYPE getConfigedImpl(PlatformBasic::OP_TYPE op) const;
    PlatformBasic::IMPL_TYPE getConfigedImpl(PlatformBasic::MEMORY_TYPE type) const;
    int getConfigedLatency(PlatformBasic::OP_TYPE op) const;
    int getConfigedLatency(PlatformBasic::MEMORY_TYPE type) const;

    // Latency set by TCL config_core command
    void setConfigCore(const std::string& coreName, int config_lat); 
    int getConfigedLatency(const std::string& coreName) const;
    bool checkOpSupport(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl, int latency = -1 );
    bool checkStorageSupport(PlatformBasic::MEMORY_TYPE type, PlatformBasic::MEMORY_IMPL impl, int latency = -1);

private:
    // filter cores that match op/type but not match impl
    void filterCoreInstList(CoreInstList& coreList, PlatformBasic::OP_TYPE op) const;
    void filterCoreInstList(CoreInstList& coreList) const;
    void filterMemoryInstList(CoreInstList& coreList) const;

private:
    // reslib to check if configured ops/storages are supported 
    CoreInstFactory* mFac;
    // mark config_op
    std::map<PlatformBasic::OP_TYPE, std::pair<PlatformBasic::IMPL_TYPE, int>> mConfigedOpMap;
    // mark config_storage
    std::map<PlatformBasic::MEMORY_TYPE, std::pair<PlatformBasic::MEMORY_IMPL, int>> mConfigedStorageMap;
    // mark conifg_core 
    std::map<std::string, int> mConfigedCoreMap;

}; //< class ConfigedLib

class CoreInstFactory
{
friend class ::CreatePlatformCmd;
friend class ::QueryCoreCmd;
friend class ConfigedLib;
friend class platform::CoreInst;

public:
    CoreInstFactory();
    ~CoreInstFactory();

    /// do not allow copy
    CoreInstFactory(CoreInstFactory const&) = delete; 
    void operator=(CoreInstFactory const&)  = delete;

    /// create CoreInst for BE
    static std::shared_ptr<CoreInst> getGenericCoreInst();
    /// get CoreInst by op+impl instead of core name
    std::shared_ptr<CoreInst> getCoreInst(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) const;
    
    std::string getImplString(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) const;
    std::string getOpString(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) const;
    // special for new resource pragma
    CoreInstList getPragmaCoreInsts(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl, int latency) const;

    CoreInstList getCoreInstsByOper(
        OperType opcode,
        int type_mask = CoreInst::Any) const;

    void getCoreInstsByInterfaceMode(
        CoreInstList& insts,
        InterfaceMode mode,
        int type_mask = CoreInst::Any) const;

    void getROMInsts(CoreInstList& list) const;
    void getRAMInsts(CoreInstList& list) const;
    void getNPRAMInsts(CoreInstList& list) const;
    void getRegisterInsts(CoreInstList& list) const;
    void getShiftMemInsts(CoreInstList& list) const;
    void getIPBlockInsts(IPBlockInstList& list) const;

    ///
    /// requestFuncUnitInstList:
    ///     Get implementable FuncUnitInsts
    /// Argument introduction:
    /// list                : results. If input arguments are invalid or no FuncUnitInsts meets requirement of input arguments, list is empty and return value is false.
    /// op                  : opcode. Must specify effective value
    /// delayBudget         : delay budget. Must specify effective value.
    /// inputBWList         : input ports bit width list. Must specify effective value.
    /// outputBW            : output port bit width. Must specify effective value.
    /// preferredLatency    : preferred latency budget. If BE doesn't have preferred latency, using the default value platform::AnyLatency
    /// preferredImpl       : preferred implementation type, If BE doesn't have preferred impl, using the default value platform::AnyImpl
    /// return              : flag. If input arguments are invalid or no FuncUnitInsts meets requirement of input arguments, return false. Otherwise, return true.
    ///                 
    bool requestFuncUnitInstList(
                        FuncUnitInstList& list,
                        PlatformBasic::OP_TYPE op,
                        double delayBudget,
                        const std::vector<unsigned>& inputBWList,
                        unsigned outputBW,

                        int preferredLatency = AnyLatency,
                        PlatformBasic::IMPL_TYPE preferredImpl = AnyImpl,
                        bool sparseMuxIncrEncoding = false
                    );
    
    ///
    /// requestStorageInstList:
    ///     Get implementable StorageInsts
    /// Argument introduction:
    /// list                : results. If input arguments are invalid or no StorageInsts meets requirement of input arguments, list is empty and return value is false.
    /// memType             : The type of storage required. e.g. MEMORY_RAM, MEMORY_RAM_1P, MEMORY_ROM, MEMORY_FIFO, etc. Must specify effective value.
    /// delayBudget         : delay budget. Must specify effective value.
    /// bitWidth            : memory bit width. Must specify effective value.
    /// depth               : memory depth. Must specify effective value.
    /// hasInst             : Has it been initialized? 
    /// needReset           : Does it need to be reset? 
    /// portTypeVec         : port type list. 0-w, 1-r, 2-wr, 3-NA.
    /// preferredLatency    : preferred latency budget. If doesn't have preferred latency, using the default value platform::AnyLatency
    /// preferredImpl       : preferred implementation type, If doesn't have preferred impl, using the default value MEMORY_IMPL_AUTO
    /// return              : flag. If input arguments are invalid or no StorageInsts meets requirement of input arguments, return false. Otherwise, return true.
    ///     
    bool requestStorageInstList(
                        StorageInstList& list,
                        PlatformBasic::MEMORY_TYPE memType,        
                        double delayBudget,
                        int bitWidth,
                        int depth,

                        bool hasInit = false,
                        bool needReset = false,
                        const std::vector<unsigned> portTypeVec = {},
                        int preferredLatency = AnyLatency,
                        unsigned required1WNRNumPorts = 0,
                        PlatformBasic::MEMORY_IMPL preferredImpl = PlatformBasic::MEMORY_IMPL_AUTO   
                    );
    
    ///
    /// requestChannelInstList:
    ///     Get implementable ChannelInsts
    /// Argument introduction:
    /// list                : results. If input arguments are invalid or no ChannelInsts meets requirement of input arguments, list is empty and return value is false.
    /// op                  : opcode. Must specify effective value
    /// delayBudget         : delay budget. Must specify effective value.
    /// bitWidth            : channel bit width. Must specify effective value.
    /// numInputs           : input port number
    /// numOutputs          : output port number 
    /// preferredLatency    : preferred latency budget. If doesn't have preferred latency, using the default value platform::AnyLatency
    /// preferredImpl       : preferred implementation type, If doesn't have preferred impl, using the default value platform::AnyImpl
    /// return              : flag. If input arguments are invalid or no ChannelInsts meets requirement of input arguments, return false. Otherwise, return true.
    ///     
    bool requestChannelInstList(
                    ChannelInstList& list,
                    PlatformBasic::OP_TYPE op,
                    double delayBudget,
                    int bitWidth,
                    int numInputs,
                    int numOutputs,
                    int depthIn,
                    int depthOut,
                    int preferredLatency = AnyLatency,
                    PlatformBasic::IMPL_TYPE preferredImpl = AnyImpl
                );
     


    /// Get IPBlockInst about VivadoIp (Vivado_DDS, Vivado_FFT, Vivado_FIR)
    /// Argument introduction:
    /// ip                  : result.
    /// delayBudget         : delay budget.
    /// metadata            : String to record metadata
    /// impl                : which VivadoIP. [VIVADO_DDS, VIVADO_FFT, VIVADO_FIR]
    /// return              : flag. If input arguments are invalid, return false. Otherwise, return true.
    ///     
    bool requestVivadoIpInst (
        std::shared_ptr<IPBlockInst> &ip,
        double delayBudget,
        const std::string& metadata,
        PlatformBasic::IMPL_TYPE impl         //  VIVADO_DDS, VIVADO_FFT, VIVADO_FIR
    );

    /// Get IPBlockInst about DSP instance (DSP48, DSP58_DP, QADDER)
    /// Argument introduction:
    /// dsp                 : result.
    /// delayBudget         : delay budget. Must specify effective value.
    /// inputBWList         : The bitwidths of input ports
    /// outputBW            : The bitwidth of out port
    /// metadata            : String to record metadata
    /// impl                : which DSP instance? [DSP48, DSP58_DP, QADDER]
    /// creg                : using C reg? 
    /// acc                 : using acc reg?
    /// ipType              : operator type
    /// nodeRef             : The number of the associated node  
    /// preferredLatency    : preferred latency budget. If doesn't have preferred latency, using the default value platform::AnyLatency
    /// return              : flag. If input arguments are invalid, return false. Otherwise, return true.
    ///      
    bool requestDspInst (
        std::shared_ptr<IPBlockInst> &dsp,
        double delayBudget,
        const std::string& metadata,
        const std::vector<unsigned>& inputBWList,
        unsigned outputBW,

        PlatformBasic::IMPL_TYPE impl,                   //  DSP48, DSP58_DP, QADDER
        bool creg = false,
        bool acc = false,
        unsigned ipType = IPBlockInst::IPBType::IPB_Default,
        unsigned nodeRef = 0,
        int preferredLatency = AnyLatency
    );

    /// Get IPBlockInst about BlackBox 
    /// Argument introduction:
    /// blackBox            : result.
    /// op                  : operation type. [OP_CALL, OP_BLACKBOX]
    /// delayBudget         : delay budget.
    /// bbName              : BlackBox name 
    /// return              : flag. If input arguments are invalid, return false. Otherwise, return true.
    ///     
    bool requestBlackBoxInst(
        std::shared_ptr<IPBlockInst> &blackBox,
        PlatformBasic::OP_TYPE op,
        double delayBudget,
        const std::string& bbName
    );

    /// Get TAddSub 
    /// Argument introduction:
    /// tAddSub             : result.
    /// op                  : operation type. [OP_ALL, OP_ADD, OP_SUB]
    /// delayBudget         : delay budget.
    /// inputBWList         : The bitwidths of input ports
    /// outputBW            : The bitwidth of out port
    /// return              : flag. If input arguments are invalid, return false. Otherwise, return true.
    ///     
    bool requestTAddSubInst(
        std::shared_ptr<IPBlockInst> &tAddSub,
        PlatformBasic::OP_TYPE op,
        double delayBudget,
        const std::vector<unsigned>& inputBWList,
        unsigned outputBW
    );
    
    /// Get implementable AdapterInst
    /// Argument introduction:
    /// axi                 : results.
    /// op                  : opcode. Must specify effective value
    /// impl                : AXI Interface Types. [AXI4STREAM, S_AXILITE, M_AXI]
    /// delayBudget         : delay budget. Must specify effective value.
    /// bitWidth            : interface bit width. Must specify effective value.
    /// paraList            : Interface configuration parameters
    /// axiLitePortsVec     : Port information when impl is S_AXILITE.
    /// ports               : Port information.
    /// bundledMemories     : The memory required for caching data
    /// IORegslice          : Enable IO Regslice
    /// return              : flag. If input arguments are invalid return false. Otherwise, return true.
    /// 
    bool requestAdapterInst(
        std::shared_ptr<AdapterInst> &axi,
        PlatformBasic::OP_TYPE op,
        PlatformBasic::IMPL_TYPE impl,
        double delayBudget,
        int bitWidth,
        
        std::map<unsigned, unsigned> maxiParaMap = {},
        std::vector<std::vector<unsigned>> axiLitePortsVec = {},
        CPortList ports = {},
        StorageInstList bundledMemories = {},
        bool enableIORegslice = false,
        std::map<unsigned, std::map<unsigned, unsigned> > maxiChanParaMap = {}
    );

    void setCoreCost(std::shared_ptr<CoreInst> core);
    // get targetPlatform for CoreRanker
    TargetPlatform* getTargetPlatform() const { return mTargetPlatform; };
    void setTargetPlatform(TargetPlatform* pf) { mTargetPlatform = pf; };
    
    // get library name
    std::string getLibraryName() const;
    bool isVersal() const;
    bool isUltrascale() const;
    bool isVirtex4() const;
    bool isVirtex5() const;
    bool isSpartan() const;
    bool is7Series() const;
    bool is8Series() const;
    bool is9Series() const;

    // set library name, familyName_speed, e.g. versal_fast, virtex_slow  
    void setName(const std::string& libName);
    std::string getName() const { return mName; }

    // family name
    std::string getFamilyName() const { return mFamilyName; }

    // set part name
    void setPartName(std::string partName) { mPartName = partName; }
    std::string getPartName() const { return mPartName; }

    /// Set the resource library helper.
    ///void setHelper(pf_internal::ResLibHelper* helper) { mHelper = helper; }
    /// Get the resource library helper.
    ///pf_internal::ResLibHelper* getHelper() { return mHelper; }

    double getCore2CoreDelay(const std::string& coreA, const std::string& coreB, int bit);

    int createCores();
private:

    pf_internal::Core* getCore(std::string name) const;
 
    // Find the cores by the operation.
    CoreList getCoresByOper(OperType opcode,
                            int type_mask = -1) const;

    // Special Handling on Int Mul
    void makeVivadoAutoMultiplierAsDefault(CoreInstList& coreList) const;

    // Find the cores by a given interface mode.
    void getCoresByInterfaceMode(CoreList& cores,
                                 InterfaceMode mode,
                                 int type_mask = -1) const;
    // Get special cores 
    void getIPBlocks(IPBlockList& ip_list) const;
    // Get ROMs.
    void getROMs(CoreList& rom_list) const;
    // Get RAMs.
    void getRAMs(CoreList& ram_list) const;
    // Get NPRAMs.
    void getNPRAMs(CoreList& ram_list) const;
    // Get registers.
    void getRegisters(CoreList& reg_list) const;
    // Get shift mems.
    void getShiftMems(CoreList& mem_list) const;
    // Convert string to core type.
    static unsigned string2CoreType(std::string ty_str);
    // Convert core type to string.
    // static std::string coreType2String(unsigned type);

    
   
    
private:
    void requestCorebyOpAndImpl(
                            CoreInstList& list, 
                            PlatformBasic::OP_TYPE op,
                            PlatformBasic::IMPL_TYPE impl,
                            int typeMask);
    static std::shared_ptr<CoreInst> createCoreInst(pf_internal::Core* core, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl);
    static CoreInstList createCoreInst(pf_internal::Core* core, OperType opcode = AnyOperation);

    pf_internal::Core* createCore(const CoreDef& def, unsigned id);
    std::vector<CoreDef*> selectCoreDefs() const;
    void addCore(pf_internal::Core* core);
    void clear();
    static pf_internal::Core* getGenericCore();
    void addGenericCore();
    pf_internal::Core* getCore(unsigned id) const;
    void removeCore(pf_internal::Core* core);
    CoreIter locateCore(pf_internal::Core* core);
private:
    TargetPlatform* mTargetPlatform;

    ConfigedLib* mCfgLib;

    // library name 
    std::string mName;
    // family name for tool
    std::string mFamilyName;
    // part name for tool
    std::string mPartName;
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

}; //< class CoreInstFactory

class IOModeType {
public:
    enum IOType {
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
    struct IOModeDescType {
        IOType modeId;
        const char* modeName;
    };

public:
    static unsigned string2Mode(std::string mode_str) {
        static const IOModeDescType IOModeDesc[] = {
            { IOType::ModeNone, "ap_none" },
            { IOType::ModeStable, "ap_stable" },
            { IOType::ModeVld, "ap_vld" },
            { IOType::ModeOVld, "ap_ovld" },
            { IOType::ModeAck, "ap_ack" },
            { IOType::ModeHS, "ap_hs" },
            { IOType::ModeFifo, "ap_fifo" },
            { IOType::ModeMemory, "ap_memory" },
            { IOType::ModeBram, "bram" },
            { IOType::ModeBus, "ap_bus" },
            { IOType::ModeAuto, "ap_auto" },
            { IOType::ModeAxis, "axis"},
            { IOType::ModeMAXI, "m_axi"},
            { IOType::ModeSAXILite, "s_axilite"},
            { IOType::ModeMemFifo, "mem_fifo"},
        };
        std::transform(mode_str.begin(), mode_str.end(),
                    mode_str.begin(), (int(*)(int))tolower);

        unsigned mode = 0;

        unsigned num_modes = sizeof(IOModeDesc) / sizeof(*IOModeDesc);
        assert(num_modes > 0);
        for (unsigned i = 0; i < num_modes; ++i)
            if (mode_str.find(IOModeDesc[i].modeName) != std::string::npos)
                mode |= IOModeDesc[i].modeId;

        return mode;
    }
};

template <class X>
inline bool inst_isa(std::shared_ptr<const CoreInst> inst)
{
    if(inst != 0 && inst->getType() == X::ClassType)
        return true;
    else
        return false;
}

template <class X>
inline bool inst_isa(const CoreInst* inst)
{
    if(inst != 0 && inst->getType() == X::ClassType)
        return true;
    else
        return false;
}

template <class X>
inline X* dyn_cast(CoreInst* inst)
{
    if(inst_isa<X>(inst))
        return static_cast<X*>(inst);
    else
        return nullptr;
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

} //< namespace platform

namespace pf_internal 
{
/**
 * Core is the superclass of the building blocks (e.g. functional
 * units, storage elements, etc.) of a datapath.
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
        IPBlock = 1 << 3, /// Blackbox IP.
        Adapter = 1 << 4, /// Adapter.
        Channel = 1 << 5, /// Channel.
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

public:
    /// Constructor.
    Core();
    /// Copy constructor.
    Core(const Core& one_core) = delete;
    ///TODO: Destructor.
    // virtual ~Core();

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
        case Core::IPBlock:      return "IPBlock";
        case Core::Adapter:      return "Adapter";
        case Core::Channel:      return "Channel";
        default:                 return "ERROR";
        }
    }
    /// Set the core type.
    void setType(Core::Type type) { mType = type; }

    /// Check if this core is generic (abstract).
    bool isGeneric() const { return (mType == Generic); }

    /// Get the heterogeneous resource usage for a specific resource.
    double getInitResourceUsageByName(std::string name) const;
    /// Get the heterogeneous resource usage map.
    const platform::ResUsageMap& getInitResourceUsage() const { return mInitResUsage; }

    /// Get the operation set.
    const platform::OperSet& getOpers() const { return mOperSet; }
    /// Match an operation.
    /// @param op Operation name.
    /// @return Returns true if the core is capable of this operation.
    virtual bool matchOper(platform::OperType op) const;

    /// Check if it is a valid operation.
    static bool isValidOper(platform::OperType op);
 
    /// get impl of this core
    platform::PlatformBasic::IMPL_TYPE getImpl() const;

    /// Get interface modes.
    const platform::InterfaceModes& getInterfaceModes() const { return mIfModes; }
    /// Match an interface mode.
    virtual bool matchInterfaceMode(platform::InterfaceMode Mode) const;

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

    float getFixedPreference(platform::OperType opcode) {
        return mPreference;
    }

    /// Access the delay List.
    std::vector<double> getFixedDelayList(platform::OperType opcode) {
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

    // BlackBox core flag
    bool isBlackBoxIPCore() const;
    unsigned string2ResourceTp(std::string res_name);

protected:
    // return str.split(sep)
    std::vector<std::string> getTclList(const std::string& str, const std::string& sep=" ");
    // 
    bool isNumber(const std::string& s);
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
    platform::OperSet mOperSet;
    /// Interface modes
    platform::InterfaceModes mIfModes;
    
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
    platform::ResUsageMap mInitResUsage;

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

};

/**
 * Storage is a subclass of the Core class. It abstracts
 * all types of storage elements (e.g., registers, memories, etc.)
 **/
class Storage: public Core
{
public:
    /// Constructor.
    Storage() = delete;
    /// Copy constructor.
    Storage(const Storage& other) = delete;
    /// TODO: Destructor.
    // virtual ~Storage();

protected:

    friend class platform::CoreInstFactory;
    Storage(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // storage
         int depth, const std::string& interfaces, const std::string& ports);

public:
    const platform::CPortList& getMemPorts() const { return mMemPorts; }
    /// Get the number of ports.
    unsigned getNumPorts() const { return mMemPorts.size(); }
    /// Get the port by index.
    platform::MemPort* getPortByIndex(unsigned index)
    {
        assert(index <= mMemPorts.size());
        return static_cast<platform::MemPort*>(&mMemPorts[index]);
    }

public:
    // Is storage readonly?
    bool isReadOnly() const;
    // Is it a ROM?
    bool isROM() const;
    // Is it a NPROM?
    bool isNPROM() const;
    // Is it a RAM?
    bool isRAM() const;
    // Is it a NPRAM?
    bool isNPRAM() const;
    // Is it a register?
    bool isRegister() const;
    // Is it a FIFO?
    bool isFifo() const;
    // Is it a shift memory?
    bool isShiftMem() const;
    // Is it a 1WnR memory?
    bool is1wnrRAM() const;
    // Is it a ECC RAM?
    bool isEccRAM() const;

    // get user latency, priority: bind_storage > config_core > config_storage, for query_core -config_latency
    platform::PlatformBasic::MEMORY_TYPE getMemoryType() const;
    platform::PlatformBasic::MEMORY_IMPL getMemoryImpl() const;

private:
    /// Clear the internal STL containers.
    // void clear();

protected:
    /// Memory ports
    platform::CPortList mMemPorts;
};

/**
 * FuncUnit is a subclass of the Core class. It abstracts
 * various types of functional units (e.g., adders, mults, gates, etc.)
 */
class FuncUnit: public Core
{
public:
    /// Constructor.
    FuncUnit() = delete;
    /// Copy constructor.
    FuncUnit(const FuncUnit& other) = delete;
    ///TODO: Destructor.
    // virtual ~FuncUnit();

protected:

    friend class platform::CoreInstFactory;
    FuncUnit(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList,
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // func unit
         int inputs, int outputs, const std::string& portStageFunction):
         Core(id, name, description, opers, style, delay, delayFunction, isDelayList,
         latency, LatencyFunction, resource, resUsageFunction,
         generator, interval, intervalFunction, isHidden, legalityFunction,
         preference)
        {
            mType = Core::FunctionUnit;
            // create inports, outports
            for(int i = 0; i < inputs; ++i)
            {
                mInPorts.push_back(platform::FuPort("pi" + std::to_string(i), platform::CPort::CPortIn, 1));     
            }
            for(int i = 0; i < outputs; ++i)
            {
                mOutPorts.push_back(platform::FuPort("po" + std::to_string(i), platform::CPort::CPortOut, 1));
            }
        }
public:
    // TODO: the ref return should be const.
    /// Get the input ports.
    platform::CPortList& getInPorts() { return mInPorts; }
    /// Get the output ports.
    platform::CPortList& getOutPorts() { return mOutPorts; }

    /// Get the number of input ports.
    unsigned getNumInPorts()  const { return mInPorts.size(); }
    /// Get the number of output ports.
    unsigned getNumOutPorts() const { return mOutPorts.size(); }

    /// Get the port by index.
    platform::FuPort* getPortByIndex(platform::CPortList& port_list, unsigned index);
    /// Get the input port by index.
    platform::FuPort* getInPortByIndex(unsigned index);
    /// Get the output port by index.
    platform::FuPort* getOutPortByIndex(unsigned index);

private:
    /// Clear the internal STL containers.
    void clear();

protected:
    // Note that inPorts and outPorts may not be disjoint
    // due the possible inout ports
    /// Input ports
    platform::CPortList mInPorts;
    /// Output ports
    platform::CPortList mOutPorts;
};

/**
 * @brief Channel is a subclass of the Core class. It abstracts
 * Channel based IP, including FIFOs(TODO), MISO, SIMO, etc. 
 */
class Channel: public Core
{
public:
    /// Constructor.
    Channel() = delete;
    /// Copy constructor.
    Channel(const Channel& other) = delete;
    ///TODO: Destructor.
    // virtual ~Channel() { }

protected:
    friend class platform::CoreInstFactory;
    Channel(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // Channel
         int inputs, int outputs, unsigned depth);

};

/**
 * IPBlock is a subclass of the Core class. It abstracts
 * user-specified IP blocks (combinational, asynchronous, pipelined.)
 */
class IPBlock : public FuncUnit
{
public:

    enum LogicType
    {
        LogicUndef, /// Undefined logic
        LogicAsync, /// Asynchronous sequential logic
        LogicSync /// Synchronous sequental logic
    };
    IPBlock() = delete;
    /// Copy constructor.
    IPBlock(const IPBlock& other) = delete;
    ///TODO: Destructor.
    // virtual ~IPBlock();

protected:

    friend class platform::CoreInstFactory;
    IPBlock(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // func unit
         int inputs, int outputs,
         // IPBlock
         const std::string& expression,
         const std::string& logic,
         const std::string& portStageFunction,
         const std::string& targetComponent
         );

public:
    /// Get IP logic type.
    LogicType getLogicType() const { return mLogicType; }

    /// Convert string to logic type.
    static LogicType string2LogicType(std::string);

    /// Match the operation.
    virtual bool matchOper(platform::OperType op) const;

    /// Get the ith supported expression.
    platform::ExprTreeNode_shared_ptr getExpression(unsigned i = 0) const;
    /// Get the vector of expressions.
    const std::vector<platform::ExprTreeNode_shared_ptr>& getExpressions() const;

private:
    typedef std::vector<std::string>::iterator InputIt;
    // tree is a node, or a list of nodes in {}
    // for example: add or { mul a0 b0 }
    std::vector<std::vector<std::string>> getTrees(InputIt first, InputIt second);
    // @NOTE: leaf nodes with same name should be unique
    platform::ExprTreeNode_shared_ptr buildTree(InputIt first, InputIt second, std::map<std::string, platform::ExprTreeNode_shared_ptr>& leafMap);

private:
    /// Clear the internal containers.
    // void clear() { mExprTrees.clear(); }

protected:
    /// IP logic type
    LogicType mLogicType;
    /// Expression trees
    std::vector<platform::ExprTreeNode_shared_ptr> mExprTrees;
};

/*
 * Adapter is a subclass of the Core class. It abstracts
 * user-specified adapters (FIFO, shared memory, bus, etc.)
 */
class Adapter: public Core
{
public:
    /// Constructors.
    Adapter() = delete;
    Adapter(const Adapter& other) = delete;
    /// TODO: Destructor.
    // virtual ~Adapter();

protected:

    friend class platform::CoreInstFactory;
    Adapter(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // adapter
         const std::string& interfaces);

};

} // end of namespace pf_internal

#endif
