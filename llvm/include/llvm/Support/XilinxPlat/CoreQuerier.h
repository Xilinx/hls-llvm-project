
#ifndef _PLATFORM_CoreQuerier_H
#define _PLATFORM_CoreQuerier_H

#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XILINXFPGAPlatformBasic.h"
#include "llvm/Support/XilinxPlat/SqliteSelector.h"
#include "llvm/Support/XilinxPlat/PortAndExprTreeNode.h"
#else
#include "SqliteSelector.h"
#include "XILINXFPGAPlatformBasic.h"
#include "PortAndExprTreeNode.h"
#endif

namespace platform
{
class CoreInst;
class StorageInst;
class ChannelInst;
class AdapterInst;

class QuerierFactory;
class CoreQuerier;
class ArithmeticQuerier;
class OneKeySparseMuxQuerier;
class FIFOQuerier;
class DoubleKeyFIFOQuerier;
class MemoryQuerier;
class DoubleKeyMemoryQuerier;
class NPMemoryQuerier;
class ChannelQuerier;

// special ones
class DotProductQuerier;
class DivnsQuerier;
class RegisterQuerier;
class DSPUsageQuerier;
class AdapterQuerier;


struct ResourceData {
    int Lut;
    int Ff;
    int Dsp;
    int Bram;
    int Uram;
    // int CarryChain;

    ResourceData(): Lut(0),
                    Ff(0),
                    Dsp(0),
                    Bram(0),
                    Uram(0) {}

    ResourceData& operator+=(const ResourceData& res) {
        Lut += res.Lut;
        Ff += res.Ff;
        Dsp += res.Dsp;
        Bram += res.Bram;
        Uram += res.Uram;
        return *this;
    }
};

struct CacheKey {
    std::string CoreNameInDb;
    int QueryKey0;
    int QueryKey1;
    int UserLatency;
    int DelayBudget;      // int(raw delayBudget * 10000)

    CacheKey(const std::string& nameInDb, int key0, int key1, int userLatency, double delayBudget):
                CoreNameInDb(nameInDb), QueryKey0(key0), QueryKey1(key1), UserLatency(userLatency) {
        DelayBudget = static_cast<int>(delayBudget * 10000);
    }

    bool operator<(const CacheKey& other) const {
        if (CoreNameInDb < other.CoreNameInDb) {
            return true;
        } else if (CoreNameInDb > other.CoreNameInDb){
            return false;
        }

        if (QueryKey0 < other.QueryKey0) {
            return true;
        } else if (QueryKey0 > other.QueryKey0){
            return false;
        }

        if (QueryKey1 < other.QueryKey1) {
            return true;
        } else if (QueryKey1 > other.QueryKey1){
            return false;
        }

        if (UserLatency < other.UserLatency) {
            return true;
        } else if (UserLatency > other.UserLatency){
            return false;
        }
        
        if (DelayBudget < other.DelayBudget) {
            return true;
        } else if (DelayBudget > other.DelayBudget){
            return false;
        }

        return false;
    }
};

struct QueryData {
    int Latency;
    std::vector<double> DelayList;
    ResourceData ResData;
    QueryData() = default;
    QueryData(int latency, std::vector<double> delayList, ResourceData resData):
        Latency(latency), DelayList(delayList), ResData(resData) {}
};

class QuerierCache {
public:
    // content to be cached
    QuerierCache(bool latency, bool delay, bool resource);
    bool hasCached(const CacheKey& k) const;
    QueryData get(const CacheKey& k) const;
    void insert(const CacheKey& k, const QueryData& v);
    bool isFullCache() const;
    bool isCacheLatency() const;
    bool isCacheDelay() const;
    bool isCacheResource() const;

private:
    bool mLatency;          // Whether to cache 'latency' information
    bool mDelay;            // Whether to cache 'Delay' information
    bool mResource;         // Whether to cache 'Resource' information
    std::map<CacheKey, QueryData> mMap;
};

//  factory to create Core Queriers
class QuerierFactory
{
public:
    enum CORE_TYPE
    {
        ARITHMETIC = 0,     // for arithmetic
        ONE_KEY_SPARSEMUX,  // for SparseMux on non-versal
        FIFO,               // for FIFO
        MEMORY,             // for normal memory
        DOUBLE_KEY_FIFO,    // for vu9p(2D) FIFO
        DOUBLE_KEY_MEMORY,  // for versal(2D) memory
        NP_MEMORY,          // for np_memory
        CHANNEL,            // for channel
        DOT_PRODUCT,        // for DSP58_DotProduct
        DIVNS,              // for DivnS, DivnS_SEQ
        REGISTER,           // for register
        DSP_USAGE,          // for Mul, MulnS, Multiplier, Mul_DSP
        DOUBLE_KEY_ARITHMETIC, // for verlsal add/sub, mul
        DOUBLE_KEY_DIVNS,      // for verlsal div
        ADAPTER,            // for adapter
        
        VIVADO_IP,          // VIVADO IP use tcl flow
        DSP48,              // for DSP48 
        DSP58,              // for DSP58 
        DSP_DOUBLE_PUMP,    // for DSP_Double_Pump_Mac16, DSP_Double_Pump_Mac8
        DSP_QADD_SUB,       // for QAddSub_DSP
        BLACK_BOX,          // for BlackBox
        SPARSE_MUX,         // for SparseMux
        QUADRUPLE_KEY_AXI,  // for maxi 
        REG_SLICE,          // for regslice
        AXIS,               // for AXIS
        DSP58BUILTIN,         // for DSP58Builtin
        DSP48BUILTIN,         // for DSP48Builtin
        QUADKEY_APFLOAT,    // for ApFloat Type Conversion
        BIN_SPARSE_MUX      // for Binary SparseMux
    }; 
private:
    QuerierFactory() = default;
    ~QuerierFactory();

public:
    static QuerierFactory& getInstance() { static QuerierFactory qf; return qf; }
    void init();
    void registerCorequerier(CORE_TYPE type, CoreQuerier* coreQuerier);
    CoreQuerier* getCoreQuerier(CoreInst* core) const;

    static int queryLatency(CoreInst* core, OperType oper);
    static std::vector<double> queryDelayList(CoreInst* core, OperType oper);
    static ResourceData queryResource(CoreInst* core);
    static bool getLegality(CoreInst* core, OperType oper);
    // convert core_name in .lib to core_name in Database
    std::string getNameInDB(CoreInst* core) const;

private:
    std::string selectCoreType(const std::string& name) const;
    void initCoreTypeMap();
    bool hasCoreData(const std::string& name) const { return !selectCoreType(name).empty(); }

    const QuerierCache* updateCache(CoreInst* core, CoreQuerier* querier, QueryData& data);

private:
    std::map<CORE_TYPE, CoreQuerier*> mQueriers;
    std::map<CoreQuerier*, QuerierCache*> mCacheMap; 
    std::set<CoreQuerier*> mSingleKeyQuerier;
    std::set<CoreQuerier*> mDoubleKeyQuerier;
    std::map<std::string, std::string> mCoreName2TypeMap;
    QuerierCache* mFullCache;
    QuerierCache* mNoResCache;
};

// base class of Core Querier
class CoreQuerier
{
    friend class QuerierFactory;
protected:
    CoreQuerier() = default; 
    virtual ~CoreQuerier() = default;

    static std::vector<int> getFuncUnitOperands(CoreInst* core);

public:
    // lookup functions
    virtual int queryLatency(CoreInst* core) = 0;
    virtual std::vector<double> queryDelayList(CoreInst* core) = 0;
    virtual ResourceData queryResource(CoreInst* core) = 0;
    virtual bool getLegality(CoreInst* core) { return true; }
protected:
    // linear interpolation
    double interpolate(int index, int leftIndex, int rightIndex, 
                             double leftValue, double rightValue);
    // linear interpolatin for delay list
    std::vector<double> interpolate(int index, int leftIndex, int rightIndex,
                                        const std::vector<double>& leftValue,
                                        const std::vector<double>& rightValue);
    // linear interpolation for delay map
    DelayMap interpolate(int index, int leftIndex, int rightIndex,
                                const DelayMap& leftValue, const DelayMap& rightValue);
    /*
     @brief binary search
     @param vec a vector of data, should be increasing sorted
     @param value the value to be searched
     @param left if return -1, the max index which less than value
     @param right if return -1, the min index which greater than value
     @return index if found, else -1 
    */
    template<class T>
    int binarySearch(const std::vector<T>& vec, T value, int& left, int& right)
    {
        assert(!vec.empty());
        int index = -1;
        if(value <= vec[0])
        {
            left = 0;
            right = 0;
            // FIXME
            //right = (vec.size() > 1) ? 1 : 0;
        }
        else if(value >= vec.back())
        {
            left = vec.size() - 1;
            right = vec.size() - 1;
            // FIXME
            // right = (vec.size() > 1) ? (vec.size() - 2) : left;
        }
        else
        {
            left = 0, right = vec.size() - 1;
            int mid = (left + right) / 2;
            while (left < mid && mid < right)
            {
                if(value > vec[mid])
                {
                    left = mid;
                }
                else if(value < vec[mid])
                {
                    right = mid;    
                }
                else
                {
                    index = mid;
                    break;       
                }
                mid = (left + right) / 2;
            }
        }
        return index;
    }

    // timing driven: find the minimum latency whose delay match budget
    int getLatency(int user_lat, unsigned max_lat, double delay_budget, const DelayMap &lat_delay_map);

    // return the value if key in this map, else do interpolation
    template <class T>
    T getValue(const std::map<int, T> &tmap, int key)
    {
        T value;
        auto itlow = tmap.lower_bound(key);
        if(itlow == tmap.end())
        {
            // bigger than last
            value = tmap.rbegin()->second;
        }
        else if(itlow == tmap.begin())
        {
            // smaller than first
            value = tmap.begin()->second;
        }
        else if(itlow->first == key)
        {
            // found
            value = itlow->second;
        }
        else if(itlow->first > key)
        {
            // interpolation
            auto right = itlow;
            --itlow;
            value = interpolate(key, itlow->first, right->first, itlow->second, right->second);
        }

        return value;
    }
    virtual bool isMultOperQuerier() { return false; }
    virtual std::string getNameInDB(CoreInst* core);
};

class MultOperQuerier : public CoreQuerier {
protected:
    MultOperQuerier() : mCurOper(-1) {} 
    ~MultOperQuerier() = default;
public:
    // lookup functions
    virtual int queryLatency(CoreInst* core) = 0;
    virtual std::vector<double> queryDelayList(CoreInst* core) = 0;
    virtual ResourceData queryResource(CoreInst* core) = 0;
    
    void setCurOper(OperType oper) { mCurOper = oper; }

protected:
    virtual bool isMultOperQuerier() { return true; }
    OperType mCurOper;
};


class RegisterQuerier : public CoreQuerier
{
    friend class QuerierFactory;
    RegisterQuerier() = default;
    ~RegisterQuerier() = default;
public:
    virtual int queryLatency(CoreInst* core) { return 0; }
    virtual std::vector<double> queryDelayList(CoreInst* core) { return {1.0, 1.0, 1.0}; }
    virtual ResourceData queryResource(CoreInst* core); 
};

class DotProductQuerier : public MultOperQuerier 
{
    friend class QuerierFactory;
protected:
    DotProductQuerier() = default;
    ~DotProductQuerier() = default;
public:
    virtual int queryLatency(CoreInst* core);
    virtual std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);
    virtual bool getLegality(CoreInst* core);
    virtual const char* getTableName() { return "DotProduct"; } 
};

// base class for those have one key
class SingleKeyQuerier: public CoreQuerier
{
    friend class QuerierFactory;
protected:
    SingleKeyQuerier() = default;
    ~SingleKeyQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core);
    virtual std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

private:
    virtual int getKey(CoreInst* core) = 0;
    virtual const char* getKeyType() = 0;
    virtual const char* getTableName() = 0;
    virtual const char* getDelayColumn() = 0;

    std::vector<int> selectKeyList(const char* name_db);
    DelayMap selectLatencyDelayMap(const char* name_db, PlatformBasic::IMPL_TYPE impl, int key);
    DelayMap selectKeyDelayMap(const char* name_db, int latency);
    std::vector<std::vector<int>> selectKeyResource2dList(const char *name_db, int latency);
};

class ArithmeticQuerier: public SingleKeyQuerier 
{
    friend class QuerierFactory;
protected:
    ArithmeticQuerier() = default;
    ~ArithmeticQuerier() = default;

    // operands
    virtual int getKey(CoreInst* core);
private:
    virtual const char* getKeyType() { return "OPERANDS"; }
    virtual const char* getTableName() { return "Arithmetic"; } 
    virtual const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }
};

class DSPUsageQuerier : public ArithmeticQuerier
{
    friend class QuerierFactory;
protected:
    DSPUsageQuerier() = default;
    ~DSPUsageQuerier() = default;

public:
    ResourceData queryResource(CoreInst* core);
    unsigned queryDSP(CoreInst* core);
};

class DivnsQuerier : public ArithmeticQuerier 
{
    friend class QuerierFactory;
protected:
    DivnsQuerier() = default;
    ~DivnsQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core); 
};

class OneKeySparseMuxQuerier: public SingleKeyQuerier 
{
    friend class QuerierFactory;
    OneKeySparseMuxQuerier() = default;
    ~OneKeySparseMuxQuerier() = default;

private:
    virtual int getKey(CoreInst* core);
    virtual const char* getKeyType() { return "INPUT_NUMBER"; }
    virtual const char* getTableName() { return "SparseMux"; } 
    virtual const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }
};

class MemoryQuerier: public SingleKeyQuerier 
{
    friend class QuerierFactory;
protected:
    MemoryQuerier() = default;
    ~MemoryQuerier() = default;
public:
    enum PORT_TYPE
    {
        WRITE_ONLY = 0,
        READ_ONLY = 1,
        READ_WRITE = 2
    };
protected:
    virtual ResourceData queryResource(CoreInst* core);
    virtual int queryLatency(CoreInst* core);
private:
    unsigned queryFF(StorageInst* storage);
    // for LUT
    unsigned muxLutNum(unsigned depth);
    unsigned queryLUT(StorageInst* storage);
    //
    unsigned queryURAM(StorageInst* storage);
    // for BRAM 
    // is SDP mode or not
    bool isSDPMode(StorageInst* storage);
    unsigned newBramImplNum(StorageInst* storage, bool isSDP);
    // proc bram_num_cal_old
    unsigned oldBramImplNum(StorageInst* storage, bool isSDP);
    unsigned queryBRAM(StorageInst* storage);

private:
    virtual int getKey(CoreInst* core);
    virtual const char* getKeyType() { return "BITWIDTH"; }
    virtual const char* getTableName() { return "Memory"; } 
    virtual const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }
};

class RegSliceQuerier: public SingleKeyQuerier 
{
    friend class QuerierFactory;
protected:
    RegSliceQuerier() = default;
    ~RegSliceQuerier() = default;

private:
    virtual int getKey(CoreInst* core);
    virtual const char* getKeyType() { return "BITWIDTH"; }
    virtual const char* getTableName() { return "RegSlice"; } 
    virtual const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }

private:
    virtual int queryLatency(CoreInst* core) override;
};

class FIFOQuerier: public MemoryQuerier 
{
    friend class QuerierFactory;
    FIFOQuerier() = default;
    ~FIFOQuerier() = default;

public:
    virtual ResourceData queryResource(CoreInst* core);

private:
    virtual int getKey(CoreInst* core);
    virtual const char* getKeyType() { return "DEPTH"; }
    virtual const char* getTableName() { return "FIFO"; } 
    virtual const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }
};

class DoubleKeyQuerier : public CoreQuerier
{
    friend class QuerierFactory;
protected:
    DoubleKeyQuerier() = default;
    ~DoubleKeyQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core);
    std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

private: 
    virtual const char* getTableName() = 0;
    virtual const char* getDelayColumn() = 0;
    virtual int getKey0(CoreInst* core) = 0;
    virtual int getKey1(CoreInst* core) = 0;
    virtual const char* getKey0Type() = 0;
    virtual const char* getKey1Type() = 0;
    
protected: 
    int selectMinGEValue(const char* core_name, const char* column, int value, bool& overLimit);
    // the minimum(column0 * column1) where column0 >= value0 and column1 >= value1
    std::pair<int, int> selectMinGEValue(const char *core_name, const char* column0, int value0,
                         const char *column1, int value1, std::pair<bool, bool>& overLimit);
    DelayMap selectLatencyDelayMap(const char* name_db, PlatformBasic::IMPL_TYPE impl, int key0, int key1);
    std::vector<double> selectDelayList(const char* core_name, int key0, int key1, int latency);
    ResourceData selectResource(const char* core_name, int key0, int key1, int latency);
};

class DoubleKeyFIFOQuerier : public DoubleKeyQuerier
{
    friend class QuerierFactory;
    DoubleKeyFIFOQuerier() = default;
    ~DoubleKeyFIFOQuerier() = default;
public:
    virtual int queryLatency(CoreInst* core) override;  
    virtual std::vector<double> queryDelayList(CoreInst* core) override;
    virtual ResourceData queryResource(CoreInst* core) override;

private:
    virtual int getKey0(CoreInst* core) override;
    virtual int getKey1(CoreInst* core) override;
    virtual const char* getKey0Type() override { return "BITWIDTH"; };
    virtual const char* getKey1Type() override { return "DEPTH"; }

    const char* getTableName() override { return "2D_FIFO"; }
    const char* getDelayColumn() override { return "DELAY0, DELAY1, DELAY2"; };

    int getRawLatency(CoreInst* core);
};

class DoubleKeyMemoryQuerier : public DoubleKeyQuerier
{
    friend class QuerierFactory;
    DoubleKeyMemoryQuerier() = default; 
    ~DoubleKeyMemoryQuerier() = default;

public:
    virtual ResourceData queryResource(CoreInst* core);
    virtual int queryLatency(CoreInst* core);

private: 
    virtual int getKey0(CoreInst* core);
    virtual int getKey1(CoreInst* core);
    virtual const char* getKey0Type() { return "BITWIDTH"; };
    virtual const char* getKey1Type() { return "DEPTH"; }

    const char* getTableName() { return "2D_Memory"; }
    const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; };
};

class DoubleKeyArithmeticQuerier : public DoubleKeyQuerier
{
    friend class QuerierFactory;
protected:
    DoubleKeyArithmeticQuerier() = default;
    ~DoubleKeyArithmeticQuerier() = default;
private:
    virtual const char* getTableName() { return "2D_Arithmetic"; }
    virtual const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }
    virtual int getKey0(CoreInst* core);
    virtual int getKey1(CoreInst* core);
    virtual const char*  getKey0Type() { return "OPERANDS0"; }
    virtual const char*  getKey1Type() { return "OPERANDS1"; }
};

class DoubleKeyDivnsQuerier : public DoubleKeyArithmeticQuerier
{
    friend class QuerierFactory;
protected:
    DoubleKeyDivnsQuerier() = default;
    ~DoubleKeyDivnsQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core);
private:
    virtual int getKey0(CoreInst* core);
    virtual int getKey1(CoreInst* core);
};

class TripleKeyQuerier : public CoreQuerier
{
    friend class QuerierFactory;
protected:
    TripleKeyQuerier() = default;
    ~TripleKeyQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core);
    std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

private: 
    virtual const char* getTableName() = 0;
    virtual const char* getDelayColumn() = 0;
    virtual int getKey0(CoreInst* core) = 0;
    virtual int getKey1(CoreInst* core) = 0;
    virtual int getKey2(CoreInst* core) = 0;
    virtual const char* getKey0Type() = 0;
    virtual const char* getKey1Type() = 0;
    virtual const char* getKey2Type() = 0;
 
    DelayMap selectLatencyDelayMap(const char* name_db, int key0, int key1, int key2);
    std::vector<double> selectDelayList(const char* core_name, int key0, int key1, int key2, int latency);
    std::vector<int> selectKeyList(const char* name_db, const char* keyType);
    ResourceData selectResource(const char* core_name, int key0, int key1, int key2, int latency);

protected:
    void getKeyPairs(const char* core_name,
                    int key0, int key1, int key2,
                    std::vector<std::tuple<int, int, int>> & pairs, 
                    std::vector<std::tuple<int, int, int>> & products);
    template <class T>
    T interpolateFor3D(std::vector<std::tuple<int,int,int>> & keys,
                                std::vector<T> & values);
};

class NPMemoryQuerier : public TripleKeyQuerier 
{
    friend class QuerierFactory;
protected:
    NPMemoryQuerier() = default;
    ~NPMemoryQuerier() = default;

public:
    virtual ResourceData queryResource(CoreInst* core);
private:
    virtual int getKey0(CoreInst* core);
    virtual int getKey1(CoreInst* core);
    virtual int getKey2(CoreInst* core);
    virtual const char* getKey0Type() { return "BITWIDTH"; }
    virtual const char* getKey1Type() { return "INPUT_NUMBER"; }
    virtual const char* getKey2Type() { return "OUTPUT_NUMBER"; }

    const char* getTableName() { return "NPMEMORY"; }
    const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }
    void configInnerMemories(StorageInst* storageInst, int latency);
};

class ChannelQuerier : public CoreQuerier
{
    friend class QuerierFactory;
    ChannelQuerier() = default;
    ~ChannelQuerier() = default;
private:
    virtual int queryLatency(CoreInst* core);
    virtual std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

    virtual void configInnerMemories(ChannelInst* channInst);
}; 

class AdapterQuerier : public MultOperQuerier 
{
    friend class QuerierFactory;
protected:
    AdapterQuerier() = default;
    ~AdapterQuerier() = default;
public:
    enum MAXI_TYPE {
        SingleChannel = 0,
        MultiChannel,
        ReadOnlyCache,
        WriteOnlyCache,
        ReadWriteCache
    };
    enum IO_TYPE {
        Native_IO = 0,
        Regslice_IO
    };
    enum OP_TYPE {
        READ = 0,
        WRITE,
        READREQ,
        WRITEREQ,
        WRITERESP
    };
    virtual int queryLatency(CoreInst* core);
    virtual std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);
    ResourceData queryAXILiteResource(CoreInst* core);
    ResourceData queryMAXIResource(CoreInst* core);
    // for m_axi resource querier.
    virtual void configInnerMemories(AdapterInst* adapterInst);
};

class DSP48DataQuerier : public MultOperQuerier {

protected:
    DSP48DataQuerier();
    ~ DSP48DataQuerier() = default;

    enum DSP48Type {
        AMA = 0,
        MAC,
        AM,
        MUL
    };
    enum DelayDatakey {
        A_AREG = 0, 
        A_MREG, 
        A_PREG, 
        A_P, 
        AREG_MREG, 
        AREG_PREG, 
        AREG_P, 
        MREG_PREG, 
        MREG_P, 
        PREG_P, 
        C_CREG, 
        C_PREG, 
        C_P,
        MARGIN
    };
    enum DSPPortName {
        A_PORT = 0,
        B_PORT,
        C_PORT,
        D_PORT,
        O_PORT,
        DEC
    };

    std::vector<double> mDelayData; 
    std::vector<int> mPortList;

    std::map<std::string, std::string> metatataToMap(const std::string& str);
    int can_be_signed(int x);
    int can_be_unsigned(int x);
    bool isIncludeAllKeys(const std::map<std::string, std::string>& param, const std::vector<std::string>& keys);
};

class DSP48Querier : public DSP48DataQuerier
{

public:
    DSP48Querier() = default;
    ~ DSP48Querier() = default;

    virtual int queryLatency(CoreInst* core);
    virtual std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);
    int queryMaxLatency(const CoreInst* core);

private:
    DSP48Type getDSP48Type(const std::map<std::string, std::string>& opt);
    bool isPostAdder(const CoreInst* core, const std::string& opcode);
    bool isPreAdder(const CoreInst* core, const std::string& opcode);

    // latency 
    int MAC1DSPnS_latency_lookup(const CoreInst* core, int userLat, const std::string& opcode);
    int AMA1DSPnS_latency_lookup(const CoreInst* core, int userLat, const std::string& opcode);
    int AMnS_latency_lookup(const CoreInst* core, int userLat, const std::string& opcode);
    int MUL1DSPnS_latency_lookup(const CoreInst* core, int userLat, const std::string& opcode);

    // delay 
    double MAC1DSP_delay_lookup(const CoreInst* core, const std::string& opcode);
    double MAC1DSP2S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double MAC1DSP3S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double MAC1DSP4S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double MAC1DSP5S_delay_lookup(const CoreInst* core, const std::string& opcode);

    double AMA1DSP_delay_lookup(const CoreInst* core, const std::string& opcode);
    double AMA1DSP2S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double AMA1DSP3S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double AMA1DSP4S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double AMA1DSP5S_delay_lookup(const CoreInst* core, const std::string& opcode);

    double AM_delay_lookup(const CoreInst* core, const std::string& opcode);
    double AM2S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double AM3S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double AM4S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double AM5S_delay_lookup(const CoreInst* core, const std::string& opcode);

    double MUL1DSP_delay_lookup(const CoreInst* core, const std::string& opcode);
    double MUL1DSP2S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double MUL1DSP3S_delay_lookup(const CoreInst* core, const std::string& opcode);
    double MUL1DSP4S_delay_lookup(const CoreInst* core, const std::string& opcode);

    double queryDelay(CoreInst* core);

    // legality 
    int mac_chk (int a_port, int b_port, int c_port, int o_port, int a_eff,
            int a_sign, int b_eff, int b_sign, int ab_bw, int ab_sign, int c_eff, int o_eff);
    int xil_gen_mac_chk (int a_port, int b_port, int c_port, int o_port, const std::map<std::string, std::string>& param);
    int xil_gen_ama_chk(int a_port, int b_port, int c_port, int d_port, int o_port, const std::map<std::string, std::string>& param);
    int xil_gen_am_chk (int a_port, int b_port, int d_port, int o_port, const std::map<std::string, std::string>& param); 
    int xil_gen_mul_chk (int a_port, int b_port, int o_port, const std::map<std::string, std::string>& param);

    virtual bool getLegality(CoreInst* core);
};

/**
 * DSP_Double_Pump_Mac16
 * DSP_Double_Pump_Mac8	
 * 
 */
class DSPDoublePumpQuerier : public DSP48DataQuerier {

public:
    DSPDoublePumpQuerier() = default;
    ~ DSPDoublePumpQuerier() = default;

    virtual int queryLatency(CoreInst* core);
    virtual std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);
    
};

/**
 * QAddSub_DSP	
 * 
 */
class DSPQAddSubQuerier : public DSP48DataQuerier {

public:
    DSPQAddSubQuerier() = default;
    ~ DSPQAddSubQuerier() = default;

    virtual int queryLatency(CoreInst* core);
    virtual std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

    std::vector<unsigned> queryLatencyList(CoreInst* core);
    std::vector<unsigned> queryLatencyList(CoreInst* core, const std::string& opcode);

    virtual bool getLegality(CoreInst* core);

private:
    std::vector<double> QAddSub_get_delay_by_latency(int latency, const std::string& opcode);
    
};

class TripleMGEKeyQuerier : public CoreQuerier
{
    friend class QuerierFactory;
protected:
    TripleMGEKeyQuerier() = default;
    ~TripleMGEKeyQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core);
    std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

private: 
    virtual const char* getTableName() = 0;
    virtual const char* getDelayColumn() = 0;
    virtual int getKey0(CoreInst* core) = 0;
    virtual int getKey1(CoreInst* core) = 0;
    virtual int getKey2(CoreInst* core) = 0;
    virtual const char* getKey0Type() = 0;
    virtual const char* getKey1Type() = 0;
    virtual const char* getKey2Type() = 0;
    
    int selectMinGEValue(const char* core_name, const char* column, int value, bool& overLimit);
    DelayMap selectLatencyDelayMap(const char* name_db, PlatformBasic::IMPL_TYPE impl, int key0, int key1, int key2);
    std::vector<double> selectDelayList(const char* core_name, int key0, int key1, int key2, int latency);

    void getQueryMinGEKeys(CoreInst *core, int& key0, int& key1, int& key2);
protected: 
    ResourceData selectResource(const char* core_name, int key0, int key1, int key2, int latency);
};

class SparseMuxQuerier : public TripleMGEKeyQuerier 
{
    friend class QuerierFactory;
protected:
    SparseMuxQuerier() = default;
    ~SparseMuxQuerier() = default;

private:
    virtual int getKey0(CoreInst* core);
    virtual int getKey1(CoreInst* core);
    virtual int getKey2(CoreInst* core);
    virtual const char* getKey0Type() { return "DATAWIDTH"; }
    virtual const char* getKey1Type() { return "ADDRWIDTH"; }
    virtual const char* getKey2Type() { return "INPUT_NUMBER"; }

    const char* getTableName() { return "SparseMux"; }
    const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }
};

class BinarySparseMuxQuerier : public CoreQuerier 
{
    friend class QuerierFactory;
protected:
    class SimpleLabelMuxQuerier : public SparseMuxQuerier {
        virtual std::string getNameInDB(CoreInst* core);
    };
    class VarLabelMuxQuerier : public SparseMuxQuerier {
        virtual std::string getNameInDB(CoreInst* core);
    };
    class FullcaseMuxQuerier : public SparseMuxQuerier {
        virtual std::string getNameInDB(CoreInst* core);
        virtual int getKey2(CoreInst* core);
    };

    BinarySparseMuxQuerier();
    ~BinarySparseMuxQuerier();

public:
    virtual int queryLatency(CoreInst* core);
    virtual std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

private:
    SimpleLabelMuxQuerier* mSimpleLabelMux;
    VarLabelMuxQuerier* mVarLabelMux;
    FullcaseMuxQuerier* mFullcaseMux;
};

class MultipleMGEKeyQuerier : public CoreQuerier
{
    friend class QuerierFactory;
protected:
    MultipleMGEKeyQuerier() = default;
    ~MultipleMGEKeyQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core);
    std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

protected:
    int selectMinGEValue(const char* core_name, const char* column, int value, bool& overLimit);

private: 
    virtual const char* getTableName() = 0;
    virtual const char* getDelayColumn() = 0;
    virtual std::vector<int> getKeys(CoreInst* core) = 0;
    virtual std::vector<std::string> getKeyTypes() = 0;
        
    virtual std::vector<int> getMinGEKeys(CoreInst *core);

    DelayMap selectLatencyDelayMap(CoreInst *core);
    std::vector<double> selectDelayList(CoreInst *core, int latency);
    ResourceData selectResource(CoreInst *core, int latency);

};

class QuadrupleAxiQuerier : public MultipleMGEKeyQuerier {
    friend class QuerierFactory;
protected:
    QuadrupleAxiQuerier() = default;
    ~QuadrupleAxiQuerier() = default;

private: 
    virtual const char* getTableName() { return "AXI";}
    virtual const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }
    virtual std::vector<int> getKeys(CoreInst* core);
    virtual std::vector<std::string> getKeyTypes() { return {"BUSWIDTH", "BURSTLENGTH", "OUTSTANDING", "USERLATENCY"};};
};

class DSPBuiltinQuerier : public CoreQuerier {
    friend class QuerierFactory;

protected:
    DSPBuiltinQuerier();
    ~DSPBuiltinQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

protected: 
    void decidePreAdder(CoreInst* core);
    void decideRegAlloca(CoreInst* core);


    std::map<std::string, double> mDelayData;
    bool useAreg;
    bool useBreg;
    bool useCreg;
    bool useDreg;
    bool useADreg;
    bool useMreg;
    bool usePreg;
    bool usePreAdder;
};

class DSP58BuiltinQuerier : public DSPBuiltinQuerier {
    friend class QuerierFactory;

protected:
    DSP58BuiltinQuerier();
    ~DSP58BuiltinQuerier() = default;

public:
    std::vector<double> queryDelayList(CoreInst* core) override;
protected: 
    double decideAPortDelay(double margin);
    double decideBPortDelay(double margin);
    double decideCPortDelay(double margin);
    double decideDPortDelay(double margin);
    double decidePPortDelay(double margin);
    double decideMregDelay(double margin);
    double decidePregDelay(double margin);
};

class DSP48BuiltinQuerier : public DSPBuiltinQuerier {
    friend class QuerierFactory;

protected:
    DSP48BuiltinQuerier();
    ~DSP48BuiltinQuerier() = default;

public:
    std::vector<double> queryDelayList(CoreInst* core) override;
protected: 
    double decideAPortDelay(double margin);
    double decideCPortDelay(double margin);
    double decideDPortDelay(double margin);
    double decidePPortDelay(double margin);
    double decideMregDelay(double margin);
    double decidePregDelay(double margin);
};

class QuadrupleKeyApFloatConversionQuerier : public MultipleMGEKeyQuerier {
    friend class QuerierFactory;

protected:
    QuadrupleKeyApFloatConversionQuerier() = default;
    ~QuadrupleKeyApFloatConversionQuerier() = default;

private:
    virtual const char* getTableName() { return "4D_Arithmetic"; }
    virtual const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }
    virtual std::vector<int> getKeys(CoreInst* core);
    virtual std::vector<std::string> getKeyTypes() { return {"OPERANDS0", "OPERANDS1", "OPERANDS2", "OPERANDS3"}; }

    virtual std::vector<int> getMinGEKeys(CoreInst *core);

    int selectMinGEValueByFixOneKey(const char *core_name,
                        const char *column,
                        int value, 
                        int fixKey,
                        unsigned fixColumn,
                        bool& overLimit);

    int selectMinGEValueByFixDoubleKeys(const char *core_name,
                        const char *column,
                        int value, 
                        int fixKey0,
                        unsigned fixColumn0,
                        int fixKey1,
                        unsigned fixColumn1,
                        bool& overLimit);
    
    int selectMinGEValueByFixTripleKeys(const char *core_name,
                        const char *column,
                        int value, 
                        int fixKey0,
                        unsigned fixColumn0,
                        int fixKey1,
                        unsigned fixColumn1,
                        int fixKey2,
                        unsigned fixColumn2,
                        bool& overLimit);
};

class DSP58Querier : public MultOperQuerier {
    friend class QuerierFactory;
protected:
    DSP58Querier();
    ~ DSP58Querier() = default;

public:
    enum DSPType {
        AMA = 0,
        MA,
        MAC,
        AM
    };
    enum DSPOpcode {
        UNSUPPORTED = -1,
        ALL         = 0,
        PRE_ADD     = 1,
        MUL         = 2,
        POST_ADD    = 3,
        SELECT      = 4
    };

    enum DSPPortName {
        A_PORT = 0,
        B_PORT,
        C_PORT,
        D_PORT,
        P_PORT,            
    };

    virtual int queryLatency(CoreInst* core);
    virtual std::vector<double> queryDelayList(CoreInst* core);
    virtual ResourceData queryResource(CoreInst* core);

    int queryMaxLatency(CoreInst* core);

    virtual bool getLegality(CoreInst* core);

private:
    int queryAmaLatency(DSPOpcode dspOp, int userLat, double clkBudget);
    int queryAmLatency(DSPOpcode dspOp, int userLat, double clkBudget);
    int queryMaLatency(DSPOpcode dspOp, int userLat, double clkBudget);

    std::vector<double> queryAmaDelayList(int latency, DSPOpcode dspOp);
    std::vector<double> queryAmDelayList(int latency, DSPOpcode dspOp);
    std::vector<double> queryMaDelayList(int latency, DSPOpcode dspOp);


    std::map<std::string, std::string> metaDataToMap(const std::string& str);
    DSPType getDSPType(const std::map<std::string, std::string>& opt);

    int can_be_signed(int x);
    int can_be_unsigned(int x);
    bool isIncludeAllKeys(const std::map<std::string, std::string>& param, const std::vector<std::string>& keys);
    bool isPostAdder(int outputBW);

    // legality 
    int mac_chk (int a_port, int b_port, int c_port, int o_port, int a_eff,
            int a_sign, int b_eff, int b_sign, int ab_bw, int ab_sign, int c_eff, int o_eff);
    int xil_gen_ma_chk (int a_port, int b_port, int c_port, int o_port, const std::map<std::string, std::string>& param);
    int xil_gen_ama_chk(int a_port, int b_port, int c_port, int d_port, int o_port, const std::map<std::string, std::string>& param);
    int xil_gen_am_chk (int a_port, int b_port, int d_port, int o_port, const std::map<std::string, std::string>& param); 

    std::vector<double> to3Delay(const std::vector<double>& delay);
    DSPOpcode getDspOpcode(const CoreInst* dsp);

    std::map<std::string, double> mDelayData; 
    std::vector<int> mPortList;
    const int MaxLatency;
};

} //< namespace

#endif
