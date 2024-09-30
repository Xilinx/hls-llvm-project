
#include <cstring>
#include <iostream>
#include <algorithm>

#include <cstring>
#include <cassert>
#include <cmath>
#include <sstream>
#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XilinxPlat/CoreInst.h"
#include "llvm/Support/XilinxPlat/CoreQuerier.h"
#include "llvm/Support/XilinxPlat/TargetPlatform.h"
#include "llvm/Support/XilinxPlat/ChipInfo.h"
#include "llvm/Support/XilinxPlat/PlatformSettings.h"
#else 
#include "CoreInst.h"
#include "CoreQuerier.h"
#include "TargetPlatform.h"
#include "ChipInfo.h"
#include "PlatformSettings.h"
#endif 

namespace platform
{
class ExtrapolationManager {
public:
    static bool isEnableExtrapolationDelay(const std::string& name) {        
        if (mEnableExtrapolation.count(name)) {
            return std::get<0>(mEnableExtrapolation.at(name));
        }
        return false;
    }
    static bool isEnableExtrapolationLatency(const std::string& name) {
        if (mEnableExtrapolation.count(name)) {
            return std::get<1>(mEnableExtrapolation.at(name));
        }
        return false;
    }
    static bool isEnableExtrapolationResource(const std::string& name) {
        if (mEnableExtrapolation.count(name)) {
            return std::get<2>(mEnableExtrapolation.at(name));
        }
        return false;
    }
private:
    // core name -> (delay, latency, resource) 
    const static std::map<std::string, std::tuple<bool, bool, bool>> mEnableExtrapolation;
};
const std::map<std::string, std::tuple<bool, bool, bool>> ExtrapolationManager::mEnableExtrapolation{
    {"RAMBlock", {false, false, true}},
    {"RAMVivadoDo", {false, false, true}},
    {"RAMDistributed", {false, false, true}},
    {"RAMUltra", {false, false, true}},
    {"URAMECC", {false, false, true}},
    {"BRAMECC", {false, false, true}},
    {"BRAMECC_WInit", {false, false, true}}
};

// max length of SQL command string
const int MAX_CMD_SIZE = 200;

ResourceData operator*(int a, const ResourceData& res) {
    ResourceData value;
    value.Lut = a * res.Lut;
    value.Ff = a * res.Ff;
    value.Dsp = a * res.Dsp;
    value.Bram = a * res.Bram;
    value.Uram = a * res.Uram;

    return value;
}

ResourceData operator*(double a, const ResourceData& res) {
    ResourceData value;
    value.Lut = std::round(a * res.Lut);
    value.Ff = std::round(a * res.Ff);
    value.Dsp = std::round(a * res.Dsp);
    value.Bram = std::round(a * res.Bram);
    value.Uram = std::round(a * res.Uram);

    return value;
}

bool QuerierCache::hasCached(const CacheKey& k) const {
    return mMap.count(k) > 0;
}
QueryData QuerierCache::get(const CacheKey& k) const {
    return mMap.at(k);
}

void QuerierCache::insert(const CacheKey& k, const QueryData& v) {
    // Keep the capacity of Cache less than 100000 (About 6-10 MB)
    if (mMap.size() > 100000) {
        // delete last 100 item
        mMap.erase(std::prev(mMap.end(), 100), mMap.end());
    }
    mMap.emplace(k, v); 
}  

QuerierCache::QuerierCache(bool latency, bool delay, bool resource):
    mLatency(latency), mDelay(delay), mResource(resource) {}

bool QuerierCache::isFullCache() const {
    return mLatency && mDelay && mResource;
}
bool QuerierCache::isCacheLatency() const {
    return mLatency;
}
bool QuerierCache::isCacheDelay() const {
    return mDelay;
}
bool QuerierCache::isCacheResource() const {
    return mResource;
}

void QuerierFactory::init() {
    mQueriers.clear();
    mCacheMap.clear();
    mSingleKeyQuerier.clear();
    mDoubleKeyQuerier.clear();
    mCoreName2TypeMap.clear();

    mQueriers[ARITHMETIC]               = new ArithmeticQuerier();
    mQueriers[ONE_KEY_SPARSEMUX]        = new OneKeySparseMuxQuerier();
    mQueriers[FIFO]                     = new FIFOQuerier();
    mQueriers[MEMORY]                   = new MemoryQuerier();
    mQueriers[DOUBLE_KEY_FIFO]          = new DoubleKeyFIFOQuerier();
    mQueriers[DOUBLE_KEY_MEMORY]        = new DoubleKeyMemoryQuerier();
    mQueriers[NP_MEMORY]                = new NPMemoryQuerier();
    mQueriers[CHANNEL]                  = new ChannelQuerier();
    mQueriers[DOT_PRODUCT]              = new DotProductQuerier();
    mQueriers[DIVNS]                    = new DivnsQuerier();
    mQueriers[REGISTER]                 = new RegisterQuerier();
    mQueriers[DSP_USAGE]                = new DSPUsageQuerier();
    mQueriers[DOUBLE_KEY_ARITHMETIC]    = new DoubleKeyArithmeticQuerier();
    mQueriers[DOUBLE_KEY_DIVNS]         = new DoubleKeyDivnsQuerier();
    mQueriers[ADAPTER]                  = new AdapterQuerier();
    mQueriers[DSP48]                    = new DSP48Querier();
    mQueriers[DSP58]                    = new DSP58Querier();
    mQueriers[DSP_DOUBLE_PUMP]          = new DSPDoublePumpQuerier();
    mQueriers[DSP_QADD_SUB]             = new DSPQAddSubQuerier();
    mQueriers[SPARSE_MUX]               = new SparseMuxQuerier();
    mQueriers[BIN_SPARSE_MUX]           = new BinarySparseMuxQuerier();
    mQueriers[QUADRUPLE_KEY_AXI]        = new QuadrupleAxiQuerier();
    mQueriers[REG_SLICE]                = new RegSliceQuerier();
    mQueriers[DSP58BUILTIN]             = new DSP58BuiltinQuerier();
    mQueriers[DSP48BUILTIN]             = new DSP48BuiltinQuerier();
    mQueriers[QUADKEY_APFLOAT]          = new QuadrupleKeyApFloatConversionQuerier();

    mFullCache = new QuerierCache(true, true, true);
    mCacheMap.emplace(mQueriers[ARITHMETIC], mFullCache);
    mCacheMap.emplace(mQueriers[ONE_KEY_SPARSEMUX], mFullCache);
    mCacheMap.emplace(mQueriers[DOUBLE_KEY_ARITHMETIC], mFullCache);
    mCacheMap.emplace(mQueriers[DOUBLE_KEY_FIFO], mFullCache);
    mCacheMap.emplace(mQueriers[DOUBLE_KEY_DIVNS], mFullCache);

    mNoResCache = new QuerierCache(true, true, false);
    mCacheMap.emplace(mQueriers[FIFO], mNoResCache);
    mCacheMap.emplace(mQueriers[MEMORY], mNoResCache);

    mCacheMap.emplace(mQueriers[DOUBLE_KEY_MEMORY], mNoResCache);

    mSingleKeyQuerier.insert(mQueriers[ARITHMETIC]);
    mSingleKeyQuerier.insert(mQueriers[ONE_KEY_SPARSEMUX]);
    mSingleKeyQuerier.insert(mQueriers[FIFO]);
    mSingleKeyQuerier.insert(mQueriers[MEMORY]);

    mDoubleKeyQuerier.insert(mQueriers[DOUBLE_KEY_ARITHMETIC]);
    mDoubleKeyQuerier.insert(mQueriers[DOUBLE_KEY_FIFO]);
    mDoubleKeyQuerier.insert(mQueriers[DOUBLE_KEY_DIVNS]);
    mDoubleKeyQuerier.insert(mQueriers[DOUBLE_KEY_MEMORY]);

    initCoreTypeMap();
}

QuerierFactory::~QuerierFactory()
{
    for(auto &item : mQueriers)
    {
        delete item.second;
        item.second = nullptr;
    }
    delete mFullCache;
    delete mNoResCache;
}

std::string QuerierFactory::getNameInDB(CoreInst* core) const
{
    std::string core_name = core->getName();
    std::string name_in_db(core_name);
    if(core->getType() == CoreInst::Storage)
    {
        auto storageInst = static_cast<StorageInst*>(core);
        if(core_name == "RAM_S2P_BRAM_ECC")
        {
            if(storageInst->hasInit())
                name_in_db = hasCoreData("BRAMECC_WInit") ? "BRAMECC_WInit" : "BRAM";
            else
                name_in_db = hasCoreData("BRAMECC") ? "BRAMECC" : "BRAM";
        }
        else if(core_name == "RAM_S2P_URAM_ECC")
        {
            name_in_db = hasCoreData("URAMECC") ? "URAMECC" : "URAM";
        }
        // old device has no FIFO_URAM data, use FIFO_BRAM instead
        else if(core_name == "FIFO_URAM")
        {
            name_in_db = hasCoreData("FIFO_URAM") ? "FIFO_URAM" : "FIFO_BRAM";
        }
        else if(storageInst->isNPRAM())
        {
            auto memInsts = storageInst->getInnerMemInstList();
            //assert(!memInsts.empty());
            if(!memInsts.empty())
            {
                auto memType = memInsts.back()->getMemoryType();
                switch(memType)
                {
                    case PlatformBasic::MEMORY_RAM_1P :  name_in_db = "np_memory_1p";  break;
                    case PlatformBasic::MEMORY_RAM_2P :  name_in_db = "np_memory_2p";  break;
                    case PlatformBasic::MEMORY_RAM_S2P : name_in_db = "np_memory_s2p"; break;
                    case PlatformBasic::MEMORY_RAM_T2P : name_in_db = "np_memory_t2p"; break;
                    default : assert(0);
                }
            }
            else
            {
                // default np_memory
                name_in_db = "np_memory_2p";
            }
        }
        else if (storageInst->isBRAM())
        {
            name_in_db = hasCoreData("RAMBlock") ? "RAMBlock" : "BRAM";
        }
        else if (storageInst->isDRAM())
        {
            name_in_db = hasCoreData("RAMDistributed") ? "RAMDistributed" : "DRAM";
        }
        else if(storageInst->isURAM())
        {
            name_in_db = hasCoreData("RAMUltra") ? "RAMUltra" : "BRAM";
        }
        else if(storageInst->isAuto())
        {
            name_in_db = (storageInst->getNumCells() >= 1024) ? "BRAM" : "DRAM";
            if(hasCoreData("RAMVivadoDo"))
            {
                name_in_db = "RAMVivadoDo";
            }
        }
    }
    else if(name_in_db == "AddSub" || name_in_db == "AddSubnS")
    {
        name_in_db = "Adder";
    }
    else if(name_in_db == "Mul" || name_in_db == "MulnS")
    {
        name_in_db = "Multiplier";
    }
    else if ((core->getOp() == PlatformBasic::OP_TYPE::OP_UREM || core->getOp() == PlatformBasic::OP_TYPE::OP_UDIV) && name_in_db == "Divider_IP") 
    {
        name_in_db = "UnsignedDivider_IP";
    }

    return name_in_db;
}

void QuerierFactory::initCoreTypeMap() {
    mCoreName2TypeMap.clear();
    int length = MAX_CMD_SIZE;
    char* cmd = new char[length];
    std::string libName = GetTargetPlatform()->getFactory().getLibraryName();
    int n = snprintf(cmd, 
             length, 
             "select CORE_NAME, CORE_TYPE from %s_CoreType ",
             libName.c_str());
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto name2type = s.selectStr2StrMap(cmd);
    for (auto &item: name2type) {
        std::string key = item.first;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        mCoreName2TypeMap.emplace(key, item.second);
    }
    delete [] cmd;
} 

std::string QuerierFactory::selectCoreType(const std::string& name) const
{
    std::string value;
    std::string k = name;
    std::transform(k.begin(), k.end(), k.begin(), ::tolower);
    if (mCoreName2TypeMap.count(k)) {
        value = mCoreName2TypeMap.at(k);
    }
    return value;
}

void QuerierFactory::registerCorequerier(CORE_TYPE type, CoreQuerier* coreQuerier) {
    mQueriers[type] =  coreQuerier;
}

CoreQuerier* QuerierFactory::getCoreQuerier(CoreInst* core) const 
{
    // select CORE_TYPE from DB to choose CoreQuerier
    CoreQuerier *querier = NULL;
    std::string name_in_db = getNameInDB(core);
    std::string typeStr = selectCoreType(name_in_db.c_str());

    // Following cores can be auto-characterized, therefore assign its querier by its type, refer to CoreType table in SQLite DB.
    if(typeStr == "Arithmetic")
    {
        querier = mQueriers.at(ARITHMETIC);
        // Following special case exist for hard-code alogrithm, rather than pure data driven. 
        if (name_in_db == "Multiplier" || name_in_db == "Mul_DSP" ) 
        {
            querier = mQueriers.at(DSP_USAGE);
        } 
        else if (name_in_db == "Divider" || name_in_db == "DivnS_SEQ")
        {
            querier = mQueriers.at(DIVNS);
        }
    }
    else if(typeStr == "2D_Arithmetic")
    {
        querier = mQueriers.at(DOUBLE_KEY_ARITHMETIC);
        // Following special case exist for hard-code alogrithm, rather than pure data driven. 
        if (name_in_db == "Divider" || name_in_db == "DivnS_SEQ")
        {
            querier = mQueriers.at(DOUBLE_KEY_DIVNS);
        }
    }
    else if(typeStr == "FIFO")
    {
        querier = mQueriers.at(FIFO);   
    }
    else if(typeStr == "Memory")
    {
        querier = mQueriers.at(MEMORY);
    }
    else if(typeStr == "2D_FIFO")
    {
        querier = mQueriers.at(DOUBLE_KEY_FIFO);
    }
    else if(typeStr == "2D_Memory")
    {
        querier = mQueriers.at(DOUBLE_KEY_MEMORY);
    }
    else if(typeStr == "NPMemory")
    {
        querier = mQueriers.at(NP_MEMORY);
    }
    else if(typeStr == "SparseMux") 
    {
        if (GetTargetPlatform()->getFactory().isVersal()) {
            querier = mQueriers.at(SPARSE_MUX);
        } else {
            querier = mQueriers.at(ONE_KEY_SPARSEMUX);
        }
    }
    else if(typeStr == "RegSlice")
    {
        querier = mQueriers.at(REG_SLICE);
    }
    else if (typeStr == "4D_Arithmetic")
    {
        querier = mQueriers.at(QUADKEY_APFLOAT);
    }

    // Following cores are special because they cannot been auto-characterized, assign its querier by its name. 
    if(name_in_db == "DSP58_DotProduct")
    {
        querier = mQueriers.at(DOT_PRODUCT);
    }
    else if(name_in_db == "Register")
    {
        querier = mQueriers.at(REGISTER);
    }
    else if (name_in_db == "s_axilite" || name_in_db == "m_axi")
    {
        querier = mQueriers.at(ADAPTER);
        if (name_in_db == "m_axi") {
            std::string axiTable = GetTargetPlatform()->getFactory().getLibraryName() + "_AXI";
            if (Selector::getSelector().isExistTable(axiTable.c_str())) {
                querier = mQueriers.at(QUADRUPLE_KEY_AXI);
            }
        }
    } 
    else if(name_in_db == "simo" || name_in_db == "miso")
    {
        querier = mQueriers.at(CHANNEL);
    } else if (name_in_db == "Vivado_DDS" || name_in_db == "Vivado_FFT" || name_in_db == "Vivado_FIR") 
    {
        querier = mQueriers.at(VIVADO_IP);
    } 
    else if (name_in_db == "DSP48") 
    {
        std::string dsp58Table = GetTargetPlatform()->getFactory().getLibraryName() + "_DSP58";
        // If the device supports DSP58 we will use the DSP58Querier
        if (Selector::getSelector().isExistTable(dsp58Table.c_str())) {
            querier = mQueriers.at(DSP58);
        } else {
            querier = mQueriers.at(DSP48);
        }
    } 
    else if (name_in_db == "DSPBuiltin")
    {
        if (GetTargetPlatform()->getFactory().isVersal()) {
            querier = mQueriers.at(DSP58BUILTIN);
        } else {
            querier = mQueriers.at(DSP48BUILTIN);
        }
    }
    else if (name_in_db == "DSP_Double_Pump_Mac16" || name_in_db == "DSP_Double_Pump_Mac8") 
    {
        querier = mQueriers.at(DSP_DOUBLE_PUMP);
    } 
    else if (name_in_db == "QAddSub_DSP") 
    {
        querier = mQueriers.at(DSP_QADD_SUB);
    } 
    else if (name_in_db == "BlackBox") 
    {
        querier = mQueriers.at(BLACK_BOX);
    }
    else if (name_in_db == "BinarySparseMux_DontCare" || name_in_db == "BinarySparseMux_HasDef") {
        querier = mQueriers.at(BIN_SPARSE_MUX);
    }
    return querier;
}

int QuerierFactory::queryLatency(CoreInst* core, OperType oper) {
    auto querier = getInstance().getCoreQuerier(core);
    QueryData data;
    auto cache = getInstance().updateCache(core, querier, data);
    if (cache != nullptr && cache->isCacheLatency()) {
        return data.Latency;
    }

    if (querier->isMultOperQuerier()) {
        auto multOpQuerier = static_cast<MultOperQuerier *>(querier);
        multOpQuerier->setCurOper(oper);
    }
    return querier->queryLatency(core);
}
std::vector<double> QuerierFactory::queryDelayList(CoreInst* core, OperType oper) {
    auto querier = getInstance().getCoreQuerier(core);
    QueryData data;
    auto cache = getInstance().updateCache(core, querier, data);
    if (cache != nullptr && cache->isCacheDelay()) {
        return data.DelayList;
    }
    if (querier->isMultOperQuerier()) {
        auto multOpQuerier = static_cast<MultOperQuerier *>(querier);
        multOpQuerier->setCurOper(oper);
    }
    return querier->queryDelayList(core);
}
ResourceData QuerierFactory::queryResource(CoreInst* core) {
    auto querier = getInstance().getCoreQuerier(core);
    QueryData data;
    auto cache = getInstance().updateCache(core, querier, data);
    if (cache != nullptr && cache->isCacheResource()) {
        return data.ResData;
    }
    return querier->queryResource(core);
}

bool QuerierFactory::getLegality(CoreInst* core, OperType oper) {
    auto querier = getInstance().getCoreQuerier(core);
    if (querier->isMultOperQuerier()) {
        auto multOpQuerier = static_cast<MultOperQuerier *>(querier);
        multOpQuerier->setCurOper(oper);
    }
    return querier->getLegality(core);
}

const QuerierCache* QuerierFactory::updateCache(CoreInst* core, CoreQuerier* querier, QueryData& data) {
    if (!mCacheMap.count(querier)) {
        return nullptr;
    }
    auto cache = mCacheMap.at(querier);

    // int id = core->getId();
    auto nameInDb  = getNameInDB(core);
    int userLatency = core->getConfigedLatency();
    double delayBudget = core->getDelayBudget();
    int key0 = 0;
    int key1 = 0;
    if (mSingleKeyQuerier.count(querier)) {
        key0 = static_cast<SingleKeyQuerier*>(querier)->getKey(core);
    } else if (mDoubleKeyQuerier.count(querier)) {
        auto doubleKeyQuerier = static_cast<DoubleKeyQuerier*>(querier);
        key0 = doubleKeyQuerier->getKey0(core);
        key1 = doubleKeyQuerier->getKey1(core);
    }
    
    CacheKey cacheKey(nameInDb, key0, key1, userLatency, delayBudget);
    if (cache->hasCached(cacheKey)) {
        data = cache->get(cacheKey);
    } else {
        int latency = -1;
        std::vector<double> delay;
        ResourceData resource;
        if (cache->isCacheLatency()) {
            latency = querier->queryLatency(core);
        }
        if (cache->isCacheDelay()) {
            delay = querier->queryDelayList(core);
        }
        if (cache->isCacheResource()) {
            resource = querier->queryResource(core);
        }
        QueryData queryData(latency, delay, resource);
        data = queryData;
        cache->insert(cacheKey, queryData);
    }
    return cache;
}

// CoreQuerier

std::vector<int> CoreQuerier::getFuncUnitOperands(CoreInst* core)
{
    std::vector<int> operands;
    auto type = core->getType();
    assert(type == CoreInst::FunctionUnit);
    auto fu = static_cast<FuncUnitInst*>(core);
    auto bwList = fu->getInputBWList();
    for (auto a: bwList) {
        operands.push_back(a);
    }
    return operands; 
}

double CoreQuerier::interpolate(int index, int leftIndex, int rightIndex,
                  double leftValue, double rightValue)
{
    double value = 0.0;
    if(rightIndex == leftIndex)
    {
        assert(leftValue == rightValue);
        value = leftValue;
    }
    else
    {
        value = (rightValue - leftValue) / (rightIndex - leftIndex) * (index - leftIndex) + leftValue;
    }
    return value;
}
std::vector<double> CoreQuerier::interpolate(int index, int leftIndex, int rightIndex,
                               const std::vector<double> &leftValue,
                               const std::vector<double> &rightValue)
{
    std::vector<double> rst;
    for (int i = 0; i < leftValue.size() && i < rightValue.size(); ++i)
    {
        rst.push_back(interpolate(index, leftIndex, rightIndex,
                                  leftValue[i], rightValue[i]));
    }
    return rst;
}


DelayMap CoreQuerier::interpolate(int index, int leftIndex, int rightIndex,
                                const DelayMap& leftValue, const DelayMap& rightValue)
{       
    DelayMap results;
    auto lit = leftValue.begin();
    auto rit = rightValue.begin();
    while(lit != leftValue.end() && rit != rightValue.end())
    {
        // below code is to make left_key and right_key have same latency range
        // the database should have ensured that.
        if (lit->first < rit->first)
        {
            ++lit;
        }
        else if (lit->first > rit->first)
        {
            ++rit;
        }
        else
        {
            // real interpolation
            auto inter_delay = interpolate(index, leftIndex, rightIndex, lit->second, rit->second);
            results.insert(std::make_pair(lit->first, inter_delay));
            ++lit;
            ++rit;
        }
    }
    if (results.empty())
    {
        results = rightValue;
    }
    return results;
}

int CoreQuerier::getLatency(int user_lat, unsigned max_lat, double delay_budget,
                             const DelayMap &lat_delay_map)
{
    assert(!lat_delay_map.empty());

    int latency = 0;
    if (user_lat < 0)
    {
        bool match = false;
        for (const auto &p : lat_delay_map)
        {
            if(p.first > max_lat) continue;

            auto delays = p.second;
            assert(!delays.empty());
            double maxDelay = *std::max_element(delays.begin(), delays.end());
            if (maxDelay < delay_budget)
            {
                latency = p.first;
                match = true;
                break;
            }
        }
        if (!match)
        {
            latency = (max_lat < lat_delay_map.rbegin()->first) ? max_lat : lat_delay_map.rbegin()->first;
        }
    }
    else
    {
        auto it = lat_delay_map.find(user_lat);
        if (it != lat_delay_map.end())
        {
            latency = user_lat;
        }
        else
        {
            latency = (max_lat < lat_delay_map.rbegin()->first) ? max_lat : lat_delay_map.rbegin()->first;
        }
    }
    return latency;
}

std::string CoreQuerier::getNameInDB(CoreInst* core) {
    return QuerierFactory::getInstance().getNameInDB(core);
}

// Register
ResourceData RegisterQuerier::queryResource(CoreInst* core) 
{
    ResourceData resource;
    resource.Ff = static_cast<StorageInst*>(core)->getBitWidth();
    return resource;
}

// DotProductQuerier
int DotProductQuerier::queryLatency(CoreInst* core)
{
    auto ipblock = static_cast<IPBlockInst*>(core);
    std::string funcType = ipblock->getIPBTypeName();

    assert(funcType == "DSP_Dot" || funcType == "DSP_DotAdd");
    double delayBudget = core->getDelayBudget();
    int userLat = core->getConfigedLatency();
    unsigned maxLat = core->getMaxLatency(core->getOp());
    // select
    int length = MAX_CMD_SIZE + funcType.size();
    char* cmd = new char[length];
    int n = snprintf(cmd,
        length,
        "select LATENCY, DELAY0, DELAY1, DELAY2 from %s_%s "
        "where CORE_NAME = '%s' COLLATE NOCASE ",
        GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), funcType.c_str());
    assert(n > 0 && n < length);
    auto& selector = Selector::getSelector();
    DelayMap delayMap = selector.selectDelayMap(cmd);
    int latency = getLatency(userLat, maxLat, delayBudget, delayMap);
    delete [] cmd;
    // special handling
    int nodeRef = ipblock->getNodeRef();
    if(funcType == "DSP_Dot")
    {
        if(mCurOper != PlatformBasic::OP_UNSUPPORTED && nodeRef != 1)
        {
            latency = 0;
        }
    }
    else if(funcType == "DSP_DotAdd")
    {
        if(mCurOper != PlatformBasic::OP_UNSUPPORTED)
        {
            if(nodeRef == 1)
            {
                // 0->0, 1->1, 2->1, 3->1, 4->2
                latency /= 2;
            }
            else if(nodeRef == 2)
            {
                // 0->0, 1->0, 2->1, 3->2, 4->2
                latency = (latency == 1) ? 0 : (latency + 1) / 2;
            }
            else
            {
                latency = 0;
            }
        }
    }
    return latency;
}

std::vector<double> DotProductQuerier::queryDelayList(CoreInst* core)
{
    auto ipblock = static_cast<IPBlockInst*>(core);
    std::string funcType = ipblock->getIPBTypeName();
    if(funcType != "DSP_Dot" && funcType != "DSP_DotAdd")
    {
        return {0.0, 0.0, 0.0};
    }
    //int latency = queryLatency(core);
    int latency = 0;
    {
        int length = MAX_CMD_SIZE + funcType.size();
        char* cmd = new char[length];
        int n = snprintf(cmd,
                 length,
                 "select LATENCY, DELAY0, DELAY1, DELAY2 from %s_%s "
                 "where CORE_NAME = '%s' COLLATE NOCASE",
                 GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), funcType.c_str());
        assert(n > 0 && n < length);
        auto& selector = Selector::getSelector();
        auto delayMap = selector.selectDelayMap(cmd);
        double delayBudget = core->getDelayBudget();
        int userLat = core->getConfigedLatency();
        unsigned maxLat = core->getMaxLatency(core->getOp());
        latency = getLatency(userLat, maxLat, delayBudget, delayMap);
    }

    // select 
    int length = MAX_CMD_SIZE + funcType.size();
    char* cmd = new char[length];
    int n = snprintf(cmd,
        length,
        "select DELAY0, DELAY1, DELAY2 from %s_%s "
        "where CORE_NAME = '%s' COLLATE NOCASE and LATENCY = %d ",
        GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), funcType.c_str(), latency);
    assert(n > 0 && n < length);
    auto& selector = Selector::getSelector();
    auto delays = selector.selectDoubleList(cmd); 
    assert(delays.size() == 3);
    delete [] cmd;
    // special handling
    int nodeRef = ipblock->getNodeRef();
    if(funcType == "DSP_Dot")
    {
        if(nodeRef != 1)
        {
            delays.clear();
            return { 0.0, 0.0, 0.0 } ;
        }
    }
    else if(funcType == "DSP_DotAdd")
    {
        if(nodeRef == 1)
        {
            double effectiveValue = delays.front();
            delays.clear();
            return { effectiveValue, effectiveValue, effectiveValue };
        }
        else if(nodeRef == 2)
        {
            double effectiveValue = delays.back();
            delays.clear();
            return { effectiveValue, effectiveValue, effectiveValue };
        }
        else
        {
            delays.clear();
            return { 0.0, 0.0, 0.0 } ;
        }
    }
    return delays;
}

ResourceData DotProductQuerier::queryResource(CoreInst* core)
{
    ResourceData resource;
    resource.Dsp = 1;
    return resource;
}

bool DotProductQuerier::getLegality(CoreInst* core)
{
    IPBlockInst* ip = static_cast<IPBlockInst*>(core);
    std::string metadata = ip->getMetadata();
    // string 2 map
    // split 2 words first
    std::vector<std::string> words;
    std::string::size_type start = 0, pos = 0;
    while ((pos = metadata.find(' ', start)) != std::string::npos) {
        if (pos != start) {
          words.push_back(metadata.substr(start, pos - start));
        }
        start = pos + 1;
    }
    if (start < metadata.size()) {
       words.push_back(metadata.substr(start));
    }
    assert(words.size() % 2 == 0);
    std::map<std::string, int> dataMap;
    for(int i = 0; i < words.size() - 1; i += 2)
    {
        std::string value = words[i + 1];
        if(!std::all_of(value.begin(), value.end(), ::isdigit))
        {
            assert(false);
            break;
        }
        dataMap[words[i]] = std::stoi(value);
    }
    int maxBit = 0;
    int minBit = std::numeric_limits<int>::max();
    std::vector<std::string> ports = {"a0", "a1", "a2", "b0", "b1", "b2"};
    for(auto& port : ports)
    {
        int sign = dataMap[port + "_sign"];
        int bit = sign == 1 ? dataMap[port] : dataMap[port] + 1;
        if(bit > maxBit) maxBit = bit;
        if(bit < minBit) minBit = bit;
    }
    return (maxBit <= 9) && (minBit <= 8);
}

// SingleKeyQuerier
std::vector<int> SingleKeyQuerier::selectKeyList(const char* name_db)
{
    const char* table_name = getTableName();
    int length = MAX_CMD_SIZE + std::strlen(name_db) + std::strlen(table_name);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select distinct(%s) from %s_%s where CORE_NAME = '%s' COLLATE NOCASE ",
              getKeyType(), GetTargetPlatform()->getFactory().getLibraryName().c_str(), table_name, name_db);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto keys = s.selectIntList(cmd);

    delete[] cmd;
    return keys;
}

DelayMap SingleKeyQuerier::selectLatencyDelayMap(const char *name_db,
                                                 PlatformBasic::IMPL_TYPE impl,
                                                 int key)
{
    const char* table_name = getTableName();
    const char* delay_column = getDelayColumn();
    int length = MAX_CMD_SIZE + std::strlen(table_name) + std::strlen(delay_column) + std::strlen(name_db);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select LATENCY, %s from %s_%s "
             "where %s = %d and CORE_NAME = '%s' COLLATE NOCASE ",
             delay_column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), table_name, getKeyType(), key, name_db);
    assert(n > 0 && n < length);

    if((std::strcmp(name_db, "Adder") == 0 && impl == PlatformBasic::FABRIC_COMB) ||    // AddSub
       (std::strcmp(name_db, "Multiplier") == 0 && impl == PlatformBasic::AUTO_COMB))   // Mul

    {
        strcat(cmd, " and LATENCY = 0");
    }
    else if((std::strcmp(name_db, "Adder") == 0 && impl == PlatformBasic::FABRIC_SEQ) ||  // AddSubnS
            (std::strcmp(name_db, "Multiplier") == 0 && impl == PlatformBasic::AUTO_PIPE)) // MulnS
    {
        strcat(cmd, " and LATENCY > 0");
    }
    auto& s = Selector::getSelector();
    auto values = s.selectDelayMap(cmd);
    delete[] cmd;
    return values;
}

DelayMap SingleKeyQuerier::selectKeyDelayMap(const char *name_db,
                                                         int latency)
{
    const char* table_name = getTableName();
    const char* delay_column = getDelayColumn();
    int length = MAX_CMD_SIZE + std::strlen(table_name) + std::strlen(delay_column) + std::strlen(name_db);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select %s, %s from %s_%s "
             "where LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
             getKeyType(), delay_column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), table_name, latency, name_db);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto values = s.selectDelayMap(cmd);
    delete[] cmd;
    return values;
}

std::vector<std::vector<int>> SingleKeyQuerier::selectKeyResource2dList(const char *name_db,
                                                             int latency)
{
    const char* table_name = getTableName();
    int length = MAX_CMD_SIZE + std::strlen(table_name) + std::strlen(name_db);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select %s, LUT, FF, DSP, BRAM, URAM from %s_%s "
             "where LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
             getKeyType(), GetTargetPlatform()->getFactory().getLibraryName().c_str(), table_name, latency, name_db);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto values = s.selectInt2dList(cmd);
    delete[] cmd;
    return values;
}

int SingleKeyQuerier::queryLatency(CoreInst* core)
{
    int key = getKey(core);
    int user_lat = core->getConfigedLatency();
    unsigned maxLat = core->getMaxLatency(core->getOp());
    double delay_budget = core->getDelayBudget();
    std::string nameDb = getNameInDB(core);
    const char* name_db = nameDb.c_str();

    int latency = 0;
    std::vector<int> keys = selectKeyList(name_db);
    int left = 0, right = 0;
    int index = binarySearch<int>(keys, key, left, right);
    if (index == -1)
    {
        // if the key is not in the database, do interpolation for it first
        int left_key= keys[left];
        int right_key = keys[right];

        auto left_map = selectLatencyDelayMap(name_db, core->getImpl(), left_key);
        auto right_map = selectLatencyDelayMap(name_db, core->getImpl(), right_key);
        DelayMap intersection_map = CoreQuerier::interpolate(key, left_key, right_key, left_map, right_map);

        latency = getLatency(user_lat, maxLat, delay_budget, intersection_map);
    }
    else
    {
        auto lat_delay_map = selectLatencyDelayMap(name_db, core->getImpl(), key);
        latency = getLatency(user_lat, maxLat, delay_budget, lat_delay_map);
    }

    return latency; 
}

std::vector<double> SingleKeyQuerier::queryDelayList(CoreInst* core)
{
    int key = getKey(core);
    int latency = SingleKeyQuerier::queryLatency(core);
    std::string name_db = getNameInDB(core);
    auto map = selectKeyDelayMap(name_db.c_str(), latency);
    auto delayList = getValue<std::vector<double>>(map, key);
    if(core->getName() == "TAddSub")
    {
        for(int i = 0; i < delayList.size(); ++i)
        {
            delayList[i] /= 2;
        }
    }
    return delayList;
}

ResourceData SingleKeyQuerier::queryResource(CoreInst* core)
{
    int key = getKey(core);
    int latency = SingleKeyQuerier::queryLatency(core);
    std::string name_db = getNameInDB(core);
    auto res2dList = selectKeyResource2dList(name_db.c_str(), latency);
    std::map<int, int> lutMap;
    std::map<int, int> ffMap;
    std::map<int, int> dspMap;
    std::map<int, int> bramMap;
    std::map<int, int> uramMap;

    for (const auto& line: res2dList) {
        lutMap[line[0]]     = line[1];      // ff
        ffMap[line[0]]      = line[2];      // lut
        dspMap[line[0]]     = line[3];      // dsp
        bramMap[line[0]]    = line[4];      // bram
        uramMap[line[0]]    = line[5];      // uram
    }
    ResourceData resource;
    resource.Lut = getValue<int>(lutMap, key);
    resource.Ff = getValue<int>(ffMap, key);
    resource.Dsp = getValue<int>(dspMap, key);
    resource.Bram = getValue<int>(bramMap, key);
    resource.Uram = getValue<int>(uramMap, key);

    return resource;
}

// TODO: 
int MemoryQuerier::queryLatency(CoreInst* core) {
    return SingleKeyQuerier::queryLatency(core);
}
ResourceData MemoryQuerier::queryResource(CoreInst* core)
{
    assert(core->getType() == CoreInst::Storage);
    auto storage = static_cast<StorageInst*>(core);
    ResourceData resource;
    resource.Ff = queryFF(storage);
    resource.Bram = queryBRAM(storage);
    resource.Lut = queryLUT(storage);
    resource.Uram = queryURAM(storage);

    return resource;
}

unsigned MemoryQuerier::queryFF(StorageInst* storage)
{
    unsigned bitWidth = storage->getBitWidth();
    unsigned portNum = storage->getMemUsedPorts().size();
    int latency = queryLatency(storage);
    unsigned ramBits = storage->getNumCells();
    bool isDRAM = storage->isDRAM() || (ramBits < 1024 && storage->isAuto()); 
    unsigned pipeFF = 0;
    if(latency > 1)
    { 
        pipeFF = bitWidth * (latency - 1);
    }
    else if(isDRAM)
    {
        pipeFF = bitWidth * portNum;
    }
    bool haveReset = storage->getReset();
    unsigned depth = storage->getDepth();
    unsigned muxFF = haveReset ? depth : 0;

    return pipeFF + muxFF;
}

unsigned MemoryQuerier::muxLutNum(unsigned depth)
{
    // proc Mux_Lut_calculate
    unsigned num = 0;
    if(depth <= 4)
    {
        num = 4;
    } 
    else if(depth < 16)
    {
        num = depth / 4 * 4;
    }
    else if(depth < 64)
    {
        num = (depth / 4) + (depth / 16 * 4);
    }
    else
    {
        num = (depth / 4) + (depth / 16) + (depth / 64 * 4);
    }
    return num;
}

unsigned MemoryQuerier::queryLUT(StorageInst* storage)
{
    unsigned ramBits = storage->getNumCells();
    bool isDRAM = storage->isDRAM() || (ramBits < 1024 && storage->isAuto()); 
    // lut
    unsigned lutNum = isDRAM ? ((ramBits + 63) / 64) : 0;
    // mux 
    bool haveReset = storage->getReset();
    unsigned depth = storage->getDepth();
    unsigned muxNum = haveReset ? (muxLutNum(depth)) : 0;
    return lutNum + muxNum;
}

unsigned MemoryQuerier::queryURAM(StorageInst* storage)
{
    // For V7 URAM estimation
    // Divide the data bus by 72 bits.  Each 72 bits will be a URAM matrix.  
    // For each matrix, divide the depth of the RAM by 4k to get the number of URAM blocks needed.  
    // The total number of URAMs will be the number of matrices multiply number of URAM blocks in each matrix.
    unsigned num = 0;
    bool isURAM = storage->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_URAM;
    if(isURAM)
    {
        unsigned bitWidth = storage->getBitWidth();
        unsigned depth = storage->getDepth();
        num = ((bitWidth + 71) / 72) * ((depth + 4095) / 4096);
    }

    std::vector<unsigned> usedPorts = storage->getMemUsedPorts();
    usedPorts.erase(std::remove_if(usedPorts.begin(), usedPorts.end(), [](unsigned i) { return i > READ_WRITE;}),usedPorts.end());
    unsigned memPortNum = usedPorts.size();

    // post-handling based on memPortNum for 1WnR RAM
    if (memPortNum > 2) 
    {
        // indicate it is 1WnR RAM
        unsigned additionalMem = memPortNum - 2;
        unsigned totalMem = additionalMem + 1;
        num = num * totalMem;
    }
    return num; 
}

bool MemoryQuerier::isSDPMode(StorageInst* storage)
{
    auto& usedPorts = storage->getMemUsedPorts();
    bool isSDP = false;
    if(storage->getName() != "RAM_coregen" && usedPorts.size() > 1)
    {
        // if any_of unused
        for(unsigned port : usedPorts)
        {
            if(port > READ_WRITE)
            {
                isSDP = true;
                break;
            }
        }
        if(usedPorts == std::vector<unsigned>{READ_ONLY, WRITE_ONLY} || 
           usedPorts == std::vector<unsigned>{WRITE_ONLY, READ_ONLY} ||
           usedPorts == std::vector<unsigned>{READ_ONLY, READ_ONLY})
           {
               isSDP = true;
           }
    }
    else if(usedPorts.size() == 1)
    {
        isSDP = true;
    }

    return isSDP;
}

unsigned MemoryQuerier::newBramImplNum(StorageInst* storage, bool isSDP)
{
    // New BRAM implementation scheme for Ultrascale(+) families
    // BRAM_36k has configurable width of 36, 18, 9, 4, 2, 1.  
    // Given the width of the RAM:  Each 36 bits of the data bus will be a BRAM cascade.  
    // If the remaining data bus width is smaller than 36, then pick one of the BRAM widths, that's bigger than the remaining bits.  
    // For example, if there are 20 bits remaining, then pick 36; if there are 15 bits remaining, then pick 18, etc.  
    // For each of the BRAM cascade, since we've decided the width of the BRAM used in the cascade, then we already know the depth of the BRAM block.  
    // For example, for a cascade with 36 width, each block will be 1k deep.  
    // Divide the depth of the RAM by the block's depth to get the number of BRAM blocks in each cascade.  
    // Sum up the number of BRAM blocks in all cascade.
    // HERE We calculate BRAM_18k numbers, so it has configurable width of 18, 9, 4, 2, 1.
    // key: depth, value: bitwidth 
    std::map<unsigned, unsigned> pairs = {{1024, 36},
                                           {1024 * 2, 18},
                                           {1024 * 4, 9},
                                           {1024 * 8, 4},
                                           {1024 * 16, 2},
                                           {1024 * 32, 1}};
    if(isSDP)
    {
        pairs[512] = 72;
    }
    unsigned totalNum = 0;
    unsigned remainWidth = storage->getBitWidth();
    unsigned depth = storage->getDepth();
    // bitwidth decreasing
    for(auto p : pairs)
    {
        unsigned num = remainWidth / p.second;
        remainWidth -= num * p.second;
        totalNum += num * ((depth + p.first - 1) / p.first);
    }
    // Why need to multiply 2? "totalNum" in above code is counted by RAMB36 and HLS use RAMB18 to counter usage. 1 RAMB36 = 2 RAMB18. 
    return totalNum * 2;
}

unsigned MemoryQuerier::oldBramImplNum(StorageInst* storage, bool isSDP)
{
    unsigned num = 0;
    unsigned bitWidth = storage->getBitWidth();
    unsigned depth = storage->getDepth();
    auto& factory = GetTargetPlatform()->getFactory();
    bool isVirtex4 = factory.isVirtex4();
    bool isSpartan = factory.isSpartan();
    bool isVirtex5 = factory.isVirtex5();
    if(depth > 8192)
    {
        // 16k * 1
        num = std::pow(2, std::ceil(std::log2(std::ceil(depth / 1024.0 / 16.0)))) * bitWidth;
    } 
    else if(depth > 4096)
    {
        // 8k * 2
        num = (bitWidth + 1) / 2;
    }
    else if(depth > 2048)
    {
        // 4k * 4
        num = (bitWidth + 3) / 4;
        if(!isVirtex4 && !isSpartan)
        {
            num = std::min(num, 2 * ((bitWidth + 8) / 9));
        }
    }
    else if(depth > 1024)
    {
        // 2k * 9
        num = (bitWidth + 8) / 9;
    }
    else if(depth > 512)
    {
        num = (bitWidth + 17) / 18;
    }
    else
    {
        if(!isVirtex5 && isSDP)
        {
            num = (bitWidth + 35) / 36;
        }
        else
        {
            num = (bitWidth + 17) / 18;
        }
    }
    return num;
}

unsigned MemoryQuerier::queryBRAM(StorageInst* storage)
{
    std::string coreName = storage->getName();
    unsigned ramBits = storage->getNumCells();
    bool isDRAM = storage->isDRAM() || (ramBits < 1024 && storage->isAuto());
    bool isURAM = storage->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_URAM;
    if(isURAM || isDRAM)
    {
        return 0;
    } 
    bool isSDP = isSDPMode(storage);
    // remove unused
    std::vector<unsigned> usedPorts = storage->getMemUsedPorts();
    usedPorts.erase(std::remove_if(usedPorts.begin(), usedPorts.end(), [](unsigned i) { return i > READ_WRITE;}),
                    usedPorts.end());
    unsigned portNum = usedPorts.size();
    // for 1WnR RAM (i.e., port_num > 2), each additional port requires one more ram
    // so effectively each additional ro port needs 2 physical port 
    bool isROM = true;
    for (auto value : usedPorts) 
    {
        if (value != 1)
        {
            isROM = false;
            break;
        }    
    }
    if(portNum > 2 && !isROM)
    {
        isSDP = false;
        portNum = portNum * 2 - 2;
    }
    if(portNum > 2 && isROM)
    {
        isSDP = false;
    }

    unsigned num = oldBramImplNum(storage, isSDP);

    // bram_num_cal
    bool isUltrascale = GetTargetPlatform()->getFactory().isUltrascale();
    if(isUltrascale)
    {
        num = std::min(num, newBramImplNum(storage, isSDP));
    }

    // FIFO port number is 0
    if(portNum > 0)
    {
        num = num * ((portNum + 1) / 2);
    }
    return num;
}

// ArithmeticQuerier
int ArithmeticQuerier::getKey(CoreInst* core)
{
    // this function is translated from QueryCoreCmd::getOperands(),
    // defined in file shared/hls/main/cmds/ResLibCmd.cpp
    std::vector<int> operands;
    auto type = core->getType();
    if(type == CoreInst::FunctionUnit)
    {
        auto fu = static_cast<FuncUnitInst*>(core);
        auto bwList = fu->getInputBWList();

        for(unsigned bitwidth : bwList)
        {
            operands.push_back(static_cast<int>(bitwidth));
        }
    }
    else if(type == CoreInst::IPBlock)
    {
        FuncUnitInst* ipcore = static_cast<FuncUnitInst*>(core);
        auto widthList = ipcore->getInputBWList();
        for(unsigned bitwidth : widthList)
        {
            operands.push_back(static_cast<int>(bitwidth));
        }
    }
    // TODO
    //else
    //{
    //    std::cout << "name: " << core->getName() << " type: " << type << std::endl;
    //    assert(false);
    //}
    
    return operands.empty() ? 0 : (*std::max_element(operands.begin(), operands.end()));
}

int DivnsQuerier::queryLatency(CoreInst* core)
{
    auto operands = CoreQuerier::getFuncUnitOperands(core);
    return operands[0] + 3;
}

// DSPUsageQuerier
ResourceData DSPUsageQuerier::queryResource(CoreInst* core)
{
    ResourceData resource = ArithmeticQuerier::queryResource(core);
    resource.Dsp = queryDSP(core);
    return resource;
}

unsigned DSPUsageQuerier::queryDSP(CoreInst* core)
{
    std::vector<unsigned> operands;
    auto type = core->getType();
    assert(type == CoreInst::FunctionUnit);
    auto fu = static_cast<FuncUnitInst*>(core);
    auto bwList = fu->getInputBWList();
    operands = bwList;
    
    assert(operands.size() >= 2);
    int lookupKey = *std::max_element(operands.begin(), operands.end());
    if(core->getName() == "Mul_DSP" && lookupKey < 11)
    {
        return 1;
    }
    // proc DSP_cal
    int input0 = operands[0];
    int input1 = operands[1];
    if((input0 <= 4 || input1 <= 4) && core->getImpl() != PlatformBasic::DSP)
    {
        return 0;
    }
    // proc y
    int baseUnit = 0;
    int shiftUnit = 0;
    int threshold4Fabric = 0;
    auto& fac = GetTargetPlatform()->getFactory();
    if(fac.is7Series())
    {
        baseUnit = 25;
        shiftUnit = 17; 
        threshold4Fabric = 20;
    }
    else if(fac.is8Series() || fac.is9Series())
    {
        baseUnit = 27;
        shiftUnit = 17;
        threshold4Fabric = 20;
    }
    else if(fac.isVersal())
    {
        baseUnit = 27;
        shiftUnit = 23;
        threshold4Fabric = 26;
    }
    else
    {
        assert(0);
    }
    // proc u
    auto u = [&](int& operand, int& len){
        int x = operand;
        len = 1;
        while (x > baseUnit) {
            x   -= shiftUnit;
            len += 1;
        }
        return x;
    };

    int length0, length1;
    int msb0 = u(input0, length0);
    int msb1 = u(input1, length1);
    // proc v
    std::vector<int> shiftValueList; 
    for(int i = 0; i < length0; ++i)
        for(int j = 0; j < length1; ++j)
            shiftValueList.push_back(i + j);
    // proc z
    std::sort(shiftValueList.begin(), shiftValueList.end());
    int maxValue = shiftValueList.back();
    if(msb0 > shiftUnit && msb1 > shiftUnit && 
      msb0 >= threshold4Fabric && msb1 >= threshold4Fabric)
    {
        shiftValueList.push_back(maxValue + 1);
    }
    if(msb0 < 11 && msb1 < 11)
    {
        shiftValueList.pop_back();
    }
    // proc w
    unsigned dspNum = 0;
    int output = static_cast<FuncUnitInst*>(core)->getOutputBW();
    for(auto i : shiftValueList)
    {
        if(output > shiftUnit * i)
        {
            ++dspNum;
        }
    }
    return dspNum;
}

// FIFOQuerier
ResourceData FIFOQuerier::queryResource(CoreInst* core)
{
    std::string coreName = core->getName();
    ResourceData resource = SingleKeyQuerier::queryResource(core);
    if (coreName == "FIFO" || coreName == "FIFO_BRAM" || coreName == "FIFO_URAM") {
        auto temp = MemoryQuerier::queryResource(core);
        resource.Bram = temp.Bram;
        resource.Uram = temp.Uram;
    }
    return resource;
}

int FIFOQuerier::getKey(CoreInst* core)
{
    auto fifo = static_cast<StorageInst*>(core);
    return fifo->getDepth();
}


// OneKeySparseMuxQuerier
int OneKeySparseMuxQuerier::getKey(CoreInst* core)
{ 
    int inputs = 0; 
    if(auto multiplexer = dyn_cast<FuncUnitInst>(core))
    {
        inputs = multiplexer->getInputBWList().size() - 1;
    }
    else
    {
        assert(false);
    }
    return inputs;
}

int RegSliceQuerier::getKey(CoreInst* core)
{
    return static_cast<AdapterInst*>(core)->getBitWidth();
}

int RegSliceQuerier::queryLatency(CoreInst* core) {
    return 0;
}

// MemoryQuerier
int MemoryQuerier::getKey(CoreInst* core)
{
    return static_cast<StorageInst*>(core)->getBitWidth();
}

int DoubleKeyQuerier::selectMinGEValue(const char *core_name,
                                             const char *column,
                                             int value, 
                                             bool& overLimit)
{
    int length = MAX_CMD_SIZE + std::strlen(core_name) + std::strlen(column);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select min(%s) from %s_%s "
             "where CORE_NAME = '%s' COLLATE NOCASE "
             "and %s >= %d",
             column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), core_name, column, value);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    int result = s.selectInt(cmd);
    // value is greater than max(column), then choose max(column)
    if(result == 0)
    {
        int n = snprintf(cmd,
                 length,
                 "select max(%s) from %s_%s "
                 "where CORE_NAME = '%s' COLLATE NOCASE ",
                 column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), core_name);
        assert(n > 0 && n < length);
        result = s.selectInt(cmd);
        overLimit = true;
    }
    delete[] cmd;
    return result;
}

// DoubleKeyQuerier 
std::pair<int, int> DoubleKeyQuerier::selectMinGEValue(const char *core_name,
                                             const char* column0,
                                             int value0,
                                             const char* column1,
                                             int value1,
                                             std::pair<bool, bool>& overLimit)
{
    value0 = selectMinGEValue(core_name, column0, value0, overLimit.first);
    value1 = selectMinGEValue(core_name, column1, value1, overLimit.second); 
    int length = MAX_CMD_SIZE + std::strlen(core_name) + std::strlen(column0)*3 + std::strlen(column1)*3;
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select %s, %s from %s_%s "
             "where CORE_NAME = '%s' COLLATE NOCASE "
             "and %s >= %d and %s >= %d "
             "ORDER BY %s * %s ASC LIMIT 1",
             column0, column1, GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), core_name, 
             column0, value0, column1, value1, column0, column1);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto result = s.selectIntPair(cmd);
    // value is greater than max, then choose max
    if(result.first == 0)
    {
        int n = snprintf(cmd,
                length,
                "select %s, %s from %s_%s "
                "where CORE_NAME = '%s' COLLATE NOCASE "
                "ORDER BY %s * %s DESC LIMIT 1",
                column0, column1, GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), core_name,
                column0, column1);
        assert(n > 0 && n < length);
        result = s.selectIntPair(cmd);
    }
    delete[] cmd;
    return result;
}

DelayMap DoubleKeyQuerier::selectLatencyDelayMap(const char *name_db,
                                                 PlatformBasic::IMPL_TYPE impl,
                                                                   int key0,
                                                                   int key1)
{
    std::pair<bool, bool> overLimit {false, false};
    auto keyPair = selectMinGEValue(name_db, getKey0Type(), key0, getKey1Type(), key1, overLimit);

    auto queryDelayMap = [&](int queryKey0, int queryKey1) {
        int length = MAX_CMD_SIZE + std::strlen(name_db);
        char* cmd = new char[length];
        int n = snprintf(cmd,
             length,
             "select LATENCY, %s from %s_%s "
             "where "
             "%s = %d and %s = %d and CORE_NAME = '%s' COLLATE NOCASE ",
             getDelayColumn(), GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), 
             getKey0Type(), queryKey0, getKey1Type(), queryKey1, name_db);
        assert(n > 0 && n < length);
        if(std::strcmp(name_db, "Multiplexer") == 0 && impl == PlatformBasic::AUTO_MUX)
        {
            strcat(cmd, " and LATENCY = 0");
        }
        auto& s = Selector::getSelector();
        auto values = s.selectDelayMap(cmd);
        delete[] cmd;
        return values;
    };
    
    auto delayMap = queryDelayMap(keyPair.first, keyPair.second);
    
    return delayMap;
}

std::vector<double> DoubleKeyQuerier::selectDelayList(const char* core_name,
                                  int key0, 
                                  int key1, 
                                  int latency)
{
    
    std::pair<bool, bool> overLimit {false, false};
    auto keyPair = selectMinGEValue(core_name, getKey0Type(), key0, getKey1Type(), key1, overLimit);

    auto queryDelayList = [&](int queryKey0, int queryKey1) {
        int length = MAX_CMD_SIZE + std::strlen(core_name);
        char* cmd = new char[length];
        int n = snprintf(cmd,
                length,
                "select %s from %s_%s "
                "where "
                "%s = %d and %s = %d and LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
                getDelayColumn(), GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), 
                getKey0Type(), queryKey0, getKey1Type(), queryKey1, latency, core_name);
        assert(n > 0 && n < length);
        auto& s = Selector::getSelector();
        auto values = s.selectDoubleList(cmd);
        delete[] cmd;
        return values;
    };
    
    auto delay = queryDelayList(keyPair.first, keyPair.second);
    return delay;
    
}

ResourceData DoubleKeyQuerier::selectResource(const char* core_name,
                                   int key0, 
                                   int key1, 
                                   int latency)
{
    
    std::pair<bool, bool> overLimit {false, false};
    auto keyPair = selectMinGEValue(core_name, getKey0Type(), key0, getKey1Type(), key1, overLimit);

    auto queryResource = [&](int queryKey0, int queryKey1) {
        int length = MAX_CMD_SIZE + std::strlen(core_name);
        char* cmd = new char[length];
        int n = snprintf(cmd,
             length,
             "select LUT, FF, DSP, BRAM, URAM from %s_%s "
             "where "
             "%s = %d and %s = %d and LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
             GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), 
             getKey0Type(), queryKey0, getKey1Type(), queryKey1, latency, core_name);
        assert(n > 0 && n < length);
        auto& s = Selector::getSelector();
        auto value = s.selectInt2dList(cmd)[0];
        delete[] cmd;
        ResourceData resource;
        resource.Lut = value[0];
        resource.Ff = value[1];
        resource.Dsp = value[2];
        resource.Bram = value[3];
        resource.Uram = value[4];
        return resource;
    };
    ResourceData resource = queryResource(keyPair.first, keyPair.second);
    if (ExtrapolationManager::isEnableExtrapolationResource(std::string(core_name))){
        // memory's bitwidth or funcUnit's operands 0 over limit   
        // memory's depth or funcUnit's operands 1 over limit  
        if (overLimit.first || overLimit.second) {
            // over limit, use extrapolation to estimate the data.
            double factor = 1.0 * key0 / keyPair.first * key1 / keyPair.second;
            resource = factor * resource;
        }
    }
    return resource;
}

int DoubleKeyQuerier::queryLatency(CoreInst* core)
{
    int user_lat = core->getConfigedLatency();
    unsigned maxLat = core->getMaxLatency(core->getOp());
    double delay_budget = core->getDelayBudget();
    std::string name_db = getNameInDB(core);
    auto lat_delay_map = selectLatencyDelayMap(name_db.c_str(), core->getImpl(), getKey0(core), getKey1(core));
    int latency = getLatency(user_lat, maxLat, delay_budget, lat_delay_map);
    return latency;
}

std::vector<double> DoubleKeyQuerier::queryDelayList(CoreInst* core)
{
    int latency = DoubleKeyQuerier::queryLatency(core);
    std::string name_db = getNameInDB(core);

    return selectDelayList(name_db.c_str(), getKey0(core), getKey1(core), latency);
}

ResourceData DoubleKeyQuerier::queryResource(CoreInst* core)
{
    int latency = DoubleKeyQuerier::queryLatency(core);
    std::string name_db = getNameInDB(core);
    auto value = selectResource(name_db.c_str(), getKey0(core), getKey1(core), latency);

    return value;
}

int DoubleKeyFIFOQuerier::queryLatency(CoreInst* core) {
    // the "read latency" of FIFO is 0
    return 0;
}

std::vector<double> DoubleKeyFIFOQuerier::queryDelayList(CoreInst* core) {
    int rawLatency = getRawLatency(core);
    std::string name_db = getNameInDB(core);

    return selectDelayList(name_db.c_str(), getKey0(core), getKey1(core), rawLatency);
}
ResourceData DoubleKeyFIFOQuerier::queryResource(CoreInst* core) {
    int rawLatency = getRawLatency(core);
    std::string name_db = getNameInDB(core);
    auto value = selectResource(name_db.c_str(), getKey0(core), getKey1(core), rawLatency);

    return value;
}

int DoubleKeyFIFOQuerier::getRawLatency(CoreInst* core) {
    auto fifo = static_cast<StorageInst*>(core);
    std::string name_db = getNameInDB(core);
    int rawLatency = fifo->getFIFORawLatency();
    std::string cmd = "SELECT DISTINCT LATENCY FROM " + 
                        GetTargetPlatform()->getFactory().getLibraryName() + 
                        "_" + getTableName() + " WHERE core_name=\"" +
                        name_db + "\"";
    auto& s = Selector::getSelector();
    auto lats = s.selectIntList(cmd.c_str());
    // not find the read after write latency in DB table, use the fist value in lats instead.
    if (std::find(lats.begin(), lats.end(), rawLatency) == lats.end()) {
        rawLatency = lats.front();
    }
    return rawLatency;
}

int DoubleKeyFIFOQuerier::getKey0(CoreInst* core) { 
    return static_cast<StorageInst*>(core)->getBitWidth(); 
}
int DoubleKeyFIFOQuerier::getKey1(CoreInst* core) { 
    return static_cast<StorageInst*>(core)->getDepth();
}

int DoubleKeyMemoryQuerier::getKey0(CoreInst* core) { 
    return static_cast<StorageInst*>(core)->getBitWidth(); 
}
int DoubleKeyMemoryQuerier::getKey1(CoreInst* core) { 
    return static_cast<StorageInst*>(core)->getDepth();
}

// TODO: 
int DoubleKeyMemoryQuerier::queryLatency(CoreInst* core) {
    return DoubleKeyQuerier::queryLatency(core);
}
ResourceData DoubleKeyMemoryQuerier::queryResource(CoreInst* core)
{
    int latency = DoubleKeyQuerier::queryLatency(core);
    std::string name_db = getNameInDB(core);
    
    ResourceData value = selectResource(name_db.c_str(), getKey0(core), getKey1(core), latency);
        
    auto storage = static_cast<StorageInst*>(core);
    std::vector<unsigned> usedPorts = storage->getMemUsedPorts();
    usedPorts.erase(std::remove_if(usedPorts.begin(), usedPorts.end(), [](unsigned i) { return i > 2;}),usedPorts.end());
    unsigned memPortNum= usedPorts.size();
    
    // post-handling based on memPortNum for 1WnR RAM
    bool isROM = true;
    for (auto value : usedPorts) 
    {
        if (value != 1)
        {
            isROM = false;
            break;
        }    
    }
    if (memPortNum > 2 && !isROM) 
    {
        // indicate it is 1WnR RAM
        unsigned additionalMem = memPortNum - 2;
        unsigned totalMem = additionalMem + 1;
        value.Bram = value.Bram * totalMem;
        value.Uram = value.Uram * totalMem;
    }
    if (memPortNum > 2 && isROM)
    {
        // indicate it is ROM_nP
        int temp = (memPortNum + 1) / 2;
        value.Bram = value.Bram * temp;
        value.Uram = value.Uram * temp;
        value.Ff = value.Ff * temp;
        value.Lut = value.Lut * temp;
        value.Dsp = value.Dsp * temp;
    }

    return value;
}

int DoubleKeyArithmeticQuerier::getKey0(CoreInst* core)
{
    auto operands = CoreQuerier::getFuncUnitOperands(core);
    return *std::min_element(operands.begin(), operands.end());
}

int DoubleKeyArithmeticQuerier::getKey1(CoreInst* core)
{
    auto operands = CoreQuerier::getFuncUnitOperands(core);
    if (core->getName() == "ApFloatMul") {
        int expo = *std::min_element(operands.begin(), operands.end());
        int total = *std::max_element(operands.begin(), operands.end());
        int mant = total - expo;
        return mant;
    }
    return *std::max_element(operands.begin(), operands.end());
}

int DoubleKeyDivnsQuerier::queryLatency(CoreInst* core)
{
    auto operands = CoreQuerier::getFuncUnitOperands(core);
    return operands[0] + 3;
}

int DoubleKeyDivnsQuerier::getKey0(CoreInst* core)
{
    auto operands = CoreQuerier::getFuncUnitOperands(core);
    // 0: diveidend, 1: divisor, 2: output
    return operands[1];
}

int DoubleKeyDivnsQuerier::getKey1(CoreInst* core)
{
    auto operands = CoreQuerier::getFuncUnitOperands(core);
    // 0: diveidend, 1: divisor, 2: output
    return operands[0];
}

std::vector<int> TripleKeyQuerier::selectKeyList(const char* name_db, const char* keyType)
{
    const char* table_name = getTableName();
    int length = MAX_CMD_SIZE + std::strlen(name_db) + std::strlen(table_name);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select distinct(%s) from %s_%s where CORE_NAME = '%s' COLLATE NOCASE ",
              keyType, GetTargetPlatform()->getFactory().getLibraryName().c_str(), table_name, name_db);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto keys = s.selectIntList(cmd);

    delete[] cmd;
    return keys;
}

DelayMap TripleKeyQuerier::selectLatencyDelayMap(const char *name_db,
                                                            int key0,
                                                            int key1,
                                                            int key2)
{
    int length = MAX_CMD_SIZE + std::strlen(name_db);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select LATENCY, %s from %s_%s "
             "where "
             "%s = %d and %s = %d and %s = %d and CORE_NAME = '%s' COLLATE NOCASE ",
             getDelayColumn(), GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), 
             getKey0Type(), key0, getKey1Type(), key1, getKey2Type(), key2, name_db);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto values = s.selectDelayMap(cmd);
    delete[] cmd;
    return values;
}

std::vector<double> TripleKeyQuerier::selectDelayList(const char* core_name,
                                  int key0, 
                                  int key1,
                                  int key2,
                                  int latency)
{
    int length = MAX_CMD_SIZE + std::strlen(core_name);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select %s from %s_%s "
             "where "
             "%s = %d and %s = %d and %s = %d and LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
             getDelayColumn(), GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), 
             getKey0Type(), key0, getKey1Type(), key1, getKey2Type(), key2, latency, core_name);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto values = s.selectDoubleList(cmd);
    delete[] cmd;
    return values;
}

ResourceData TripleKeyQuerier::selectResource(const char* core_name,
                                   int key0, 
                                   int key1,
                                   int key2,
                                   int latency)
{
    int length = MAX_CMD_SIZE + std::strlen(core_name);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select LUT, FF, DSP, BRAM, URAM from %s_%s "
             "where "
             "%s = %d and %s = %d and %s = %d and LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
             GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), 
             getKey0Type(), key0, getKey1Type(), key1, getKey2Type(), key2, latency, core_name);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto value = s.selectInt2dList(cmd)[0];
    delete[] cmd;
    ResourceData resource;
    resource.Lut = value[0];
    resource.Ff = value[1];
    resource.Dsp = value[2];
    resource.Bram = value[3];
    resource.Uram = value[4];
    return resource;
}

void TripleKeyQuerier::getKeyPairs(const char* core_name,
                            int key0, int key1, int key2,
                            std::vector<std::tuple<int, int, int>> & pairs, 
                            std::vector<std::tuple<int, int, int>> & products)
{
    
    int keys[3];
    keys[0] = key0;
    keys[1] = key1;
    keys[2] = key2;

    std::vector<std::vector<int>> keyLists;
    keyLists.push_back(selectKeyList(core_name, getKey0Type()));
    keyLists.push_back(selectKeyList(core_name, getKey1Type()));
    keyLists.push_back(selectKeyList(core_name, getKey2Type()));

    int indexs[3][3] = {0};
    int i, index, left, right;

    for (i = 0; i < 3; i++)
    {
        left = 0; right = 0;
        index = binarySearch<int>(keyLists[i], keys[i], left, right);
        indexs[i][0] = (index != -1) || (left == right) ? 1 : 2;
        indexs[i][1] = (index != -1) ? keys[i] : keyLists[i][left];
        indexs[i][2] = (index != -1) ? keys[i] : keyLists[i][right];
        if ((left != right) && (index == -1))
            pairs.push_back(std::make_tuple(keys[i], keyLists[i][left], keyLists[i][right]));   
    }

    int j, k;
    for (i = 0; i < indexs[0][0]; i++)
        for (j = 0; j < indexs[1][0]; j++)
            for (k = 0; k < indexs[2][0]; k++)
                products.push_back(std::make_tuple(indexs[0][i+1], indexs[1][j+1], indexs[2][k+1]));
}

template <class T>
T TripleKeyQuerier::interpolateFor3D(std::vector<std::tuple<int,int,int>> & keys,
                                    std::vector<T> & values)
{
    assert(!values.empty());

    T result;
    if (keys.empty())
    {    
        result = values.back();
    }
    else
    {
        int iter = keys.size();
        int size = values.size();
        int i, offset = 0;

        while (iter > 0 )
        {
            for (i = 0; i < size/2; i++)
            {
                result = CoreQuerier::interpolate(std::get<0>(keys[iter-1]), std::get<1>(keys[iter-1]), std::get<2>(keys[iter-1]), values[offset+2*i], values[offset+2*i+1]);
                values.push_back(result);
            }
            offset += size;
            size = size / 2;
            iter--;
        }
    }
    return result;
}

int TripleKeyQuerier::queryLatency(CoreInst* core)
{
    int user_lat = core->getConfigedLatency();
    unsigned maxLat = core->getMaxLatency(core->getOp());
    double delay_budget = core->getDelayBudget();
    std::string name_db = getNameInDB(core);

    std::vector<std::tuple<int, int, int>> pairs, products;
    std::vector<DelayMap> delayMaps;

    getKeyPairs(name_db.c_str(), getKey0(core), getKey1(core), getKey2(core), pairs, products);

    for (auto & product : products)
        delayMaps.push_back(selectLatencyDelayMap(name_db.c_str(), std::get<0>(product), std::get<1>(product), std::get<2>(product)));

    assert(!delayMaps.empty());

    auto lat_delay_map = interpolateFor3D<DelayMap>(pairs, delayMaps);
    int latency = getLatency(user_lat, maxLat, delay_budget, lat_delay_map);
    return latency;
}

std::vector<double> TripleKeyQuerier::queryDelayList(CoreInst* core)
{
    int latency = queryLatency(core);
    std::string name_db = getNameInDB(core);

    std::vector<std::tuple<int, int, int>> pairs, products;
    std::vector<std::vector<double>> delayLists;

    getKeyPairs(name_db.c_str(), getKey0(core), getKey1(core), getKey2(core), pairs, products);

    for (auto & product : products)
        delayLists.push_back(selectDelayList(name_db.c_str(), std::get<0>(product), std::get<1>(product), std::get<2>(product), latency));

    return interpolateFor3D<std::vector<double>>(pairs, delayLists);
}

ResourceData TripleKeyQuerier::queryResource(CoreInst* core)
{
    int latency = queryLatency(core);
    std::string name_db = getNameInDB(core);

    std::vector<std::tuple<int, int, int>> pairs, products;
    std::vector<int> lutList;
    std::vector<int> ffList;
    std::vector<int> dspList;
    std::vector<int> bramList;
    std::vector<int> uramList;

    getKeyPairs(name_db.c_str(), getKey0(core), getKey1(core), getKey2(core), pairs, products);

    for (auto & product : products) {
        auto resTemp = selectResource(name_db.c_str(), std::get<0>(product), std::get<1>(product), std::get<2>(product), latency);
        lutList.push_back(resTemp.Lut);
        ffList.push_back(resTemp.Ff);
        dspList.push_back(resTemp.Dsp);
        bramList.push_back(resTemp.Bram);
        uramList.push_back(resTemp.Uram);
    }
    ResourceData resource;
    resource.Lut = interpolateFor3D<int>(pairs, lutList);
    resource.Ff = interpolateFor3D<int>(pairs, ffList);
    resource.Dsp = interpolateFor3D<int>(pairs, dspList);
    resource.Bram = interpolateFor3D<int>(pairs, bramList);
    resource.Uram = interpolateFor3D<int>(pairs, uramList);
    return resource;
}

int NPMemoryQuerier::getKey0(CoreInst* core) { 
    return static_cast<StorageInst*>(core)->getBitWidth(); 
}

int NPMemoryQuerier::getKey1(CoreInst* core)
{
    int ports = 0; 
    if(auto npmem = dyn_cast<StorageInst>(core))
    {
        ports = npmem->getMemUsedPorts().size();
    }
    else
    {
        assert(false);
    }
    return ports;
}

int NPMemoryQuerier::getKey2(CoreInst* core)
{
    int banks = 0; 
    if(auto npmem = dyn_cast<StorageInst>(core))
    {
        banks = npmem->getBanksNum();
    }
    else
    {
        assert(false);
    }
    return banks;
}

ResourceData NPMemoryQuerier::queryResource(CoreInst* core)
{
    auto usage = TripleKeyQuerier::queryResource(core);
    int latency = queryLatency(core);
    auto storageInst = static_cast<StorageInst*>(core);

    configInnerMemories(storageInst, latency > 4 ? latency - 3 : 1);
    
    auto& innerMem = storageInst->getInnerMemInstList().back();
    
    CoreQuerier* querier = QuerierFactory::getInstance().getCoreQuerier(innerMem.get());
    usage += querier->queryResource(innerMem.get());

    return usage;
}

void NPMemoryQuerier::configInnerMemories(StorageInst* storageInst, int latency)
{
    auto& innerMemories = storageInst->getInnerMemInstList();
    
    if(innerMemories.empty())
    {
        StorageInstList insts = storageInst->getSupportedInnerMemInstList();
        for (auto& inst : insts) 
        {
            if ( inst->getMemoryImpl() == storageInst->getMemoryImpl() && inst->getMemoryType() == PlatformBasic::MEMORY_RAM_2P) 
            {
                storageInst->pushBackInnerMemInst(inst);
                break;
            }
        }
    }

    auto& innerMem = innerMemories.back();
    innerMem->configLatencyBudget(latency);
    innerMem->setMemUsedPorts({0});
    innerMem->configDepth(storageInst->getDepth());
    innerMem->configBitWidth(storageInst->getBitWidth());

}

int ChannelQuerier::queryLatency(CoreInst* core)
{
    return 0;
}

std::vector<double> ChannelQuerier::queryDelayList(CoreInst* core)
{
    auto channInst = static_cast<ChannelInst*>(core); 
    auto& innerMemories = channInst->getInnerMemInstList();
    if (innerMemories.empty()) 
        configInnerMemories(channInst); // config when getInnerMemInstList() is empty.
    
    assert(innerMemories.size() == 2);
    double max = 0.0;
    std::vector<double> delay (3, 0.0);
    for (auto& memInst : innerMemories)
    {
        if (memInst->getDepth() > 0)
        { 
            CoreQuerier* querier = QuerierFactory::getInstance().getCoreQuerier(memInst.get());
            auto delaylist = querier->queryDelayList(memInst.get());
            double max_delay = *std::max_element(delaylist.begin(), delaylist.end());
            if ( max_delay > max)
            {
                delay = delaylist;
                max   = max_delay;
            }
        }
    }
    return delay; 
    
}

ResourceData ChannelQuerier::queryResource(CoreInst* core)
{
    ResourceData usage;
    auto channInst = static_cast<ChannelInst*>(core); 
    auto& innerMemories = channInst->getInnerMemInstList();
    if (innerMemories.empty()) 
        configInnerMemories(channInst); // config when getInnerMemInstList() is empty.
    
    assert(innerMemories.size() == 2);

    std::vector<int> nports;
    nports.push_back(channInst->getNumInputs());
    nports.push_back(channInst->getNumOutputs());

    for (int i = 0; i < 2 ; i++)
    {
        auto& memInst = innerMemories[i]; 
        if (memInst->getDepth() > 0)
        {
            CoreQuerier* querier = QuerierFactory::getInstance().getCoreQuerier(memInst.get());
            usage += nports[i] * querier->queryResource(memInst.get());
        }
    }
    return usage;
}

void ChannelQuerier::configInnerMemories(ChannelInst* channInst)
{
    std::vector<int> depths;
    depths.push_back(channInst->getDepthIn());
    depths.push_back(channInst->getDepthOut());

    for (auto &depth : depths){

        std::shared_ptr<platform::StorageInst> fifoInst = NULL;
        StorageInstList insts = channInst->getSupportedInnerMemInstList();
        for (auto& inst : insts) 
        {
            if ( inst->getMemoryImpl() == channInst->getChannelMemoryImplBySize(depth))
            {
                fifoInst = inst;
                break;
            }
        }
        // fifoInst->configDepth(depth);
        // fifoInst->configBitWidth(channInst->getBitWidth());
        auto& fac = GetTargetPlatform()->getFactory();
        StorageInstList list;
        auto coreBasic = PlatformBasic::getInstance()->getCoreFromOpImpl(fifoInst->getOp(), fifoInst->getImpl()); 
        fac.requestStorageInstList(list, coreBasic->getMemoryType(), -1, channInst->getBitWidth(), 
            depth, false, false, {}, 
            AnyLatency, 0 /*required1WNRNumPorts0*/, coreBasic->getMemoryImpl());
        channInst->ConfigInnerMemInst(list[0]);
    }
}

int AdapterQuerier::queryLatency(CoreInst* core)
{
    if (core->getName() == "s_axilite")
    {
        return 0;
    }
    else if (core->getName() == "m_axi")
    {

        std::map<MAXI_TYPE, std::map<IO_TYPE, std::vector<int>>> mMAXILatMap = {
            // Op sequence : READ, WRITE, READREQ, WRITEREQ, WRITERESP
            {SingleChannel, {   { Native_IO,   {3, 5, 6,  9, 3} }, 
                                { Regslice_IO, {4, 7, 8, 11, 4} } } },
            {MultiChannel,  {   { Native_IO,   {3, 3, 6,  9, 3} },
                                { Regslice_IO, {4, 5, 8, 11, 4} } } },
            {ReadOnlyCache, {   { Native_IO,   {1, 5, 4,  9, 3} }, 
                                { Regslice_IO, {1, 6, 4, 10, 4} } } }
        };

        auto adapInst = static_cast<AdapterInst*>(core);

        MAXI_TYPE MAXIChanType = SingleChannel;
        if (adapInst->enableReadOnlyCache()) MAXIChanType = ReadOnlyCache;
        else if (adapInst->isMultiChannelMAXI()) MAXIChanType = MultiChannel;

        IO_TYPE IOType = Native_IO; 
        if (adapInst->enableIORegslice()) IOType = Regslice_IO;

        std::vector<int> latVals = mMAXILatMap[MAXIChanType][IOType];
        //std::cout << "DBG : query OP " << mCurOper <<  " for " << MAXIChanType << " MAXI , with " << IOType << " Interface." << std::endl;
        //std::cout << "the latency list are : " << latVals[READ] << ", " << latVals[WRITE] << ", " << 
        //            latVals[READREQ] << ", " << latVals[WRITEREQ] << ", " << latVals[WRITERESP] << std::endl;
        switch (mCurOper)
        {
            case PlatformBasic::OP_READ     : return latVals[READ]; 
            case PlatformBasic::OP_WRITE    : return latVals[WRITE];
            case PlatformBasic::OP_READREQ  : return latVals[READREQ];
            case PlatformBasic::OP_WRITEREQ : return latVals[WRITEREQ];
            case PlatformBasic::OP_WRITERESP: return latVals[WRITERESP];
            case PlatformBasic::OP_ADAPTER  : return 0 ;
            default : assert(0);
        }
    }
    return 0;
}

std::vector<double> AdapterQuerier::queryDelayList(CoreInst* core) 
{
    return { 1.0, 1.0, 1.0 };
}

ResourceData AdapterQuerier::queryResource(CoreInst* core)
{
    ResourceData resource;
    if (core->getName() == "s_axilite")
    {
        resource = queryAXILiteResource(core);
    }
    else if (core->getName() == "m_axi")
    {
        resource = queryMAXIResource(core);
    }  
    return resource;
}

ResourceData AdapterQuerier::queryAXILiteResource(CoreInst* core)
{
    assert(core->getName() == "s_axilite");
    ResourceData resource;
    resource.Ff = 30;
    auto adapInst = static_cast<AdapterInst*>(core);
    auto ports = adapInst->getAXILitePortsVec();
    for (auto port : ports)
    resource.Ff = (port[0]) ? (resource.Ff + 80) : (resource.Ff + port[1] + 6) ; 
    resource.Lut = 40;
    // auto adapInst = static_cast<AdapterInst*>(core);
    // auto ports = adapInst->getAXILitePortsVec();
    for (auto port : ports)
            resource.Lut = (port[0]) ? (resource.Lut + 70) : (resource.Lut + port[1] * 2) ; 

    auto adapterInst = static_cast<AdapterInst*>(core); 
    auto& adaptMemories = adapterInst->getInnerMemInstList();
    CoreQuerier *querier = NULL;
    for (auto& mem : adaptMemories) 
    {
        querier = QuerierFactory::getInstance().getCoreQuerier(mem.get());
        resource += querier->queryResource(mem.get());
    }

    return resource;
}

ResourceData AdapterQuerier::queryMAXIResource(CoreInst* core)
{
    assert(core->getName() == "m_axi");
    std::map<std::string, std::vector<int>> mAXIResMap = {
        { "KEY",        {8,   16,  32,  64,  128, 256,  512,  1024} },
        { "FF",         {548, 537, 512, 566, 613, 881,  1415, 2493} },
        { "LUT",        {700, 677, 580, 766, 787, 1052, 1585, 2663} }
    };

    ResourceData resourceUsage;
    auto getResourceUsageByResourceType = [&](const std::string& resourceType) {
        int usage = 0;
        int key = static_cast<AdapterInst*>(core)->getBitWidth();
        std::vector<int> keys = mAXIResMap["KEY"];
        std::vector<int> vals = mAXIResMap[resourceType];
        int left = 0, right = 0;
        int index = binarySearch<int>(keys, key, left, right);
        if (index == -1)
        {
            usage = interpolate(key, keys[left], keys[right], vals[left], vals[right]) ;
        }
        else
        {
            usage = vals[index];
        }
        return usage;
    };
    resourceUsage.Ff = getResourceUsageByResourceType("FF");
    resourceUsage.Lut = getResourceUsageByResourceType("LUT");

    auto adapterInst = static_cast<AdapterInst*>(core); 
    auto& adaptMemories = adapterInst->getInnerMemInstList();
    if (adaptMemories.empty()) 
    {
        // config when getInnerMemInstList() is empty.
        configInnerMemories(adapterInst);
    }

    CoreQuerier* querier = NULL;
    for (auto& mem : adaptMemories) 
    {
        querier = QuerierFactory::getInstance().getCoreQuerier(mem.get());
        resourceUsage += querier->queryResource(mem.get());
    }
    
    return resourceUsage;
}
void AdapterQuerier::configInnerMemories(AdapterInst* adapterInst)
{
    if (adapterInst->getName() == "m_axi")
    {
        // tuple<impl, width, depth>
        std::vector<std::tuple<unsigned, unsigned, unsigned> > fifoCfg, memCfg;

        auto& paraMap      = adapterInst->getMAXIParaMap();
        auto& chanParaMaps = adapterInst->getMAXIChanParaMap();

        auto toMultipleOf8 = [](int value) -> int {
            return ((value + 7) / 8) * 8;
        };
        auto toNextPowerOf2 = [](int value) -> int {
            int power = 1;
            while (power < value) {
                power <<= 1;
            }
            return power;
        };

        // user control on read/write buffer size 
        int maxReadBuffSize  = -1;
        int maxWriteBuffSize = -1;
        if (paraMap.count(AdapterInst::MAXIParaKey::MaxReadBuffSize) > 0) {
            maxReadBuffSize = static_cast<int>(paraMap.at(AdapterInst::MAXIParaKey::MaxReadBuffSize));
        }
        if (paraMap.count(AdapterInst::MAXIParaKey::MaxWriteBuffSize) > 0) {
            maxWriteBuffSize = static_cast<int>(paraMap.at(AdapterInst::MAXIParaKey::MaxWriteBuffSize));
        }

        // user/bus width configuration
        unsigned userWidth = toMultipleOf8(adapterInst->getBitWidth());
        unsigned busWidth  = toNextPowerOf2(userWidth);
        if (paraMap.count(AdapterInst::MAXIParaKey::MaxReadBuffSize) > 0) {
            busWidth = static_cast<unsigned>(paraMap.at(AdapterInst::MAXIParaKey::BusWidth));
        }
        
        if (chanParaMaps.size() < 1) {
            // buff_rdata
            fifoCfg.push_back( std::make_tuple(
                paraMap.at(AdapterInst::MAXIParaKey::LsuFifoImpl), 
                busWidth,
                (maxReadBuffSize < 0) ? paraMap.at(AdapterInst::MAXIParaKey::NumReadOutstanding) * paraMap.at(AdapterInst::MAXIParaKey::MaxReadBurstLen) : maxReadBuffSize ));

            // buff_wdata
            fifoCfg.push_back(std::make_tuple(
                paraMap.at(AdapterInst::MAXIParaKey::LsuFifoImpl), 
                userWidth,
                (maxWriteBuffSize < 0) ? int(paraMap.at(AdapterInst::MAXIParaKey::MaxWriteBurstLen) * busWidth / toNextPowerOf2(userWidth)) : maxWriteBuffSize));
        
        } else {
            for (auto& pm : chanParaMaps) {
                //auto& chId     = pm.first;
                auto&    chParaMap     = pm.second;
                int      cacheType     = chParaMap.at(AdapterInst::MAXIParaKey::CacheType);
                int      chanIOType    = chParaMap.at(AdapterInst::MAXIParaKey::ChanIOType);
                unsigned chanPortWidth = toMultipleOf8(chParaMap.at(AdapterInst::MAXIParaKey::ChanPortWidth));

                assert(cacheType == AdapterInst::CacheType::None || cacheType == AdapterInst::CacheType::ReadOnlyCache);

                // Read Buffer/Cache Configurations
                if (chanIOType == AdapterInst::ChanIOType::ReadWrite || chanIOType == AdapterInst::ChanIOType::ReadOnly) {
                    // read buffer configuration when cacheType is None
                    if (cacheType == AdapterInst::CacheType::None){
                        fifoCfg.push_back( std::make_tuple(
                            chParaMap.at(AdapterInst::MAXIParaKey::LsuFifoImpl), 
                            busWidth,
                            (maxReadBuffSize < 0) ? chParaMap.at(AdapterInst::MAXIParaKey::NumReadOutstanding) * chParaMap.at(AdapterInst::MAXIParaKey::MaxReadBurstLen) : maxReadBuffSize));

                    // read cache configuration when cacheType is ReadOnlyCache
                    } else if (cacheType == AdapterInst::CacheType::ReadOnlyCache) {
                        auto cacheLineWidth = chParaMap.at(AdapterInst::MAXIParaKey::CacheLineWidth);
                        auto cacheLineDepth = chParaMap.at(AdapterInst::MAXIParaKey::CacheLineDepth);
                        auto cacheLineNum   = chParaMap.at(AdapterInst::MAXIParaKey::CacheLineNum);
                        int  memDepth       = int(toNextPowerOf2(cacheLineWidth) * cacheLineDepth / busWidth ) * cacheLineNum;

                        memCfg.push_back( std::make_tuple(
                            chParaMap.at(AdapterInst::MAXIParaKey::CacheImpl), 
                            busWidth, 
                            memDepth));
                    }
                }
                
                // Write Buffer configuraitons.
                if (chanIOType == AdapterInst::ChanIOType::ReadWrite || chanIOType == AdapterInst::ChanIOType::WriteOnly) {
                    fifoCfg.push_back( std::make_tuple(
                        chParaMap.at(AdapterInst::MAXIParaKey::LsuFifoImpl),
                        chanPortWidth,
                        (maxWriteBuffSize < 0) ? 2 * int(paraMap.at(AdapterInst::MAXIParaKey::MaxWriteBurstLen) * busWidth / toNextPowerOf2(chanPortWidth)) : maxWriteBuffSize ));
                }
            }
        }

        //std::cout << "DBG: query resource of fifo/mem inside maxi adapter : " << fifoCfg.size() << " fifo(s), " << memCfg.size() << " memory(s)"<< std::endl;
        for (auto& cfg : fifoCfg) {
            auto impl  = static_cast<PlatformBasic::MEMORY_IMPL>(std::get<0>(cfg));
            auto width = std::get<1>(cfg);
            auto depth = std::get<2>(cfg);
            //std::cout << "DBG : fifoCfg : {impl : " << impl << ", width : " << width << ", depth : " << depth << "}"<< std::endl;
            
            if (depth == 0) continue;

            PlatformBasic::MEMORY_TYPE fifoType = PlatformBasic::MEMORY_FIFO;
            // FIFO doesn't have MEMORY_IMPL_AUTO impl, convert to MEMORY_IMPL_MEMORY instead
            PlatformBasic::MEMORY_IMPL fifoImpl = (impl == PlatformBasic::MEMORY_IMPL_AUTO) ? PlatformBasic::MEMORY_IMPL_MEMORY : impl;
            
            auto& fac = GetTargetPlatform()->getFactory();
            StorageInstList list;
            fac.requestStorageInstList(list, fifoType, -1, width, 
                depth, false, false, {}, 
                AnyLatency, 0 /*required1WNRNumPorts0*/, fifoImpl);
            adapterInst->pushBackInnerMemInst(list[0]);
        }

        for (auto& cfg : memCfg) {
            auto impl  = static_cast<PlatformBasic::MEMORY_IMPL>(std::get<0>(cfg));
            auto width = std::get<1>(cfg);
            auto depth = std::get<2>(cfg);
            //std::cout << "DBG : memCfg : {impl : " << impl << ", width : " << width << ", depth : " << depth << "}"<< std::endl;

            if (depth == 0) continue;

            // PF does not support RAM_S2P_AUTO currenlty, use RAM_2P_AUTO instead to estimate cache memory resource usage.
            PlatformBasic::MEMORY_TYPE memType = (impl == PlatformBasic::MEMORY_IMPL_AUTO) ? PlatformBasic::MEMORY_RAM_2P : PlatformBasic::MEMORY_RAM_S2P;
            PlatformBasic::MEMORY_IMPL memImpl = impl;

            auto& fac = GetTargetPlatform()->getFactory();
            StorageInstList list;
            fac.requestStorageInstList(list, memType, -1, width, 
                depth, false, false, {}, 
                AnyLatency, 0 /*required1WNRNumPorts0*/, memImpl);
            adapterInst->pushBackInnerMemInst(list[0]);
        }
    }
}

//*********************************DSP48*******************************//
DSP48DataQuerier::DSP48DataQuerier() {

    // get DSP delay data
    std::vector<std::string> colNames = {"a_areg", "a_mreg", "a_preg", "a_p", 
                                         "areg_mreg", "areg_preg", "areg_p",  
                                         "mreg_preg", "mreg_p", "preg_p",     
                                         "c_creg", "c_preg", "c_p", "margin"};
    std::string cmd = "select ";
    for (int i=0; i < colNames.size()-1; i ++) {
        cmd.append(colNames[i]);
        cmd.append(",");
    }
    cmd.append(colNames[colNames.size()-1]);
    cmd.append(" from DSP_delays where core_name=");
    std::string familyName = GetTargetPlatform()->getFactory().getFamilyName();
    std::string speed = GetTargetPlatform()->getChipInfo()->getSpeedGrade();
    cmd += "\"" + familyName + "_" + speed + "\"";
    auto& select = Selector::getSelector();
    mDelayData = select.selecDSPDelayList(cmd.c_str());
    assert(!mDelayData.empty());

    // get DSP port data
    colNames = {"a", "b", "c", "d", "o", "dec"};
    cmd = "select ";
    for (int i=0; i < colNames.size()-1; i ++) {
        cmd.append(colNames[i]);
        cmd.append(",");
    }
    cmd.append(colNames[colNames.size()-1]);
    cmd.append(" from DSP_ports where core_name=");
    cmd += "\"" + familyName + "\"";
    mPortList = select.selectDSPPortList(cmd.c_str());
    assert(!mPortList.empty());
}

std::map<std::string, std::string> DSP48DataQuerier::metatataToMap(const std::string& str) {
    std::map<std::string, std::string> res;
    std::vector<std::string> strVer;
    int strLen = str.length();
    int left = 0;
    while (left < strLen) {
        while (str[left] == ' ' || str[left] == '{' || str[left] == '}') {
            left ++;
        }
        int right = left;
        while (right < strLen && str[right] != ' ' 
            && str[right] != '{' && str[right] != '}') {
            right ++;
        }
        strVer.push_back(str.substr(left, right-left));
        left = right;
    }
    int size = strVer.size();
    if (size % 2 > 0) {
        size -= 1;
    }
    for (int i=0; i < size; i += 2) {
        res[strVer[i]] = strVer[i+1];
    }
    return res;
}

//#enum { Invalid = -1, Signed = 1, Unsigned = 2, Both = 3 } mSign;
int DSP48DataQuerier::can_be_signed(int x) {
    if (x == 1 || x == 3) {
        return 1;
    }
    return 0;
}

int DSP48DataQuerier::can_be_unsigned(int x) {
    if (x == 2 || x == 3) {
        return 1;
    }
    return 0;
}

bool DSP48DataQuerier::isIncludeAllKeys(const std::map<std::string, std::string>& param, const std::vector<std::string>& keys) {
    for (const auto& key: keys) {
        if(param.count(key) == 0) {
            return false;
        }
    }
    return true;
}

bool DSP48Querier::isPostAdder(const CoreInst* core, const std::string& opcode) {
    if (opcode == "add" || opcode == "sub") {
        int bw = static_cast<const FuncUnitInst*>(core)->getOutputBW();
        if (bw > 40) {
            if (bw < 48) {
                // TODO: error output
                std::cout << "post-adder bitwidth should be 48!" << std::endl;
            }
            return true;
        }
    }
    return false;
}

bool DSP48Querier::isPreAdder(const CoreInst* core, const std::string& opcode) {
    if (opcode == "add" || opcode == "sub") {
        int bw = static_cast<const FuncUnitInst*>(core)->getOutputBW();
        if (bw < 30) {
            if (bw > 2) {
                std::cout << "bitwidth error in DSP core setting." << std::endl;
            }
            return true;
        }
    }
    return false;
}

// dsp48 type 
//          c_sign      d_sign
// ama      y           y
// mac      y           n 
// am       n           y
// mul      n           n
DSP48Querier::DSP48Type DSP48Querier::getDSP48Type(const std::map<std::string, std::string>& opt) {
    DSP48Type type = MUL;
    if (opt.count("c_sign") > 0 && opt.count("d_sign") > 0) {
        type = AMA;
    } else if (opt.count("c_sign") > 0 && opt.count("d_sign") == 0) {
        type = MAC;
    } else if (opt.count("c_sign") == 0 && opt.count("d_sign") > 0) {
        type = AM;
    } else {
        type = MUL;
    }
    return type;
}

int DSP48Querier::MAC1DSPnS_latency_lookup(const CoreInst* core, int userLat, const std::string& opcode) {
    int latency = 0;
    const IPBlockInst* ipBlock = static_cast<const IPBlockInst*>(core);
    bool c_reg = ipBlock->getCReg();
    bool acc = ipBlock->getAcc();
    // TODO: What should be the default value
    switch (userLat) {
        case 2:
            if (opcode == "*" || opcode == "mul") {
                latency = 1;
            } else {
                latency = 0;
            }
            break;

        case 3:
            if (opcode == "*" || opcode == "mul") {
                latency = 2;
            } else {
                latency = 0;
            }
            break;
        case 4:
            if (opcode == "*") {
                latency = 3;
            } else {
                if (opcode == "mul") {
                    latency = 2;
                } else {
                    latency = 1;
                }
            }
        break;

        case 5:

            if (acc) {
                c_reg = false;
            }

            if (opcode == "*") {
                latency = 4;
            } else if (opcode == "mul") {
                if (c_reg) {
                    latency = 2;
                } else {
                    latency = 3;
                }
            } else if (c_reg) {
                latency = 2;
            } else {
                latency = 1;
            }
        break;

        default:
            latency = 0;
            break;
    }
    return latency;
}

int DSP48Querier::AMA1DSPnS_latency_lookup(const CoreInst* core, int userLat, const std::string& opcode) {
    int latency = 0;
    // TODO: What should be the default value
    const IPBlockInst* ipBlock = static_cast<const IPBlockInst*>(core);
    bool acc = ipBlock->getAcc();
    bool c_reg = ipBlock->getCReg();
    switch (userLat) {
        case 2:
            if (opcode == "*" || opcode == "mul") {
                latency = 1;
            } else {
                latency = 0;
            }
            break;

        case 3:
            if (opcode == "*") {
                latency = 2;
            } else if (acc) {
                if (opcode == "mul") {
                    latency = 1;
                } else if (isPostAdder(core, opcode)) {
                    latency = 1;
                }
            } else {
                if (opcode == "mul") {
                    latency = 2;
                } else {
                    latency = 0;
                }
            } 
            break;
        case 4:
            if (opcode == "*") {
                latency = 3;
            } else {
                if (opcode == "mul") {
                    latency = 2;
                } else if (isPostAdder(core, opcode)) {
                    latency = 1;
                } else {
                    latency = 0;
                }
            }
        break;

        case 5:
            if (acc) {
                c_reg = false;
            }

            if (opcode == "*") {
                latency = 4;
            } else if (opcode == "mul") {
                if (c_reg) {
                    latency = 1;
                } else {
                    latency = 2;
                }
            } else if (isPostAdder(core, opcode)) {
                if (c_reg) {
                    latency = 2;
                } else {
                    latency = 1;
                }
            } else {
                latency = 1;
            }
        break;

        default:
            latency = 0;
            break;
    }
    return latency;
}

int DSP48Querier::AMnS_latency_lookup(const CoreInst* core, int userLat, const std::string& opcode) {
    int latency = 0;
    switch (userLat) {
        case 2:
            if (opcode == "*" || opcode == "mul") {
                latency = 1;
            } else {
                latency = 0;
            }
            break;

        case 3:
            if (opcode == "*" || opcode == "mul") {
                latency = 2;
            } else {
                latency = 0;
            }
            break;
        case 4:
            if (opcode == "*" || opcode == "mul") {
                latency = 3;
            } else {
                latency = 0;
            }
        break;

        case 5:
            if (opcode == "*") {
                latency = 4;
            } else if (opcode == "mul") {
                latency = 3;
            } else {
                latency = 1;
            }
        break;

        default:
            latency = 0;
            break;
    }
    return latency;
}

int DSP48Querier::MUL1DSPnS_latency_lookup(const CoreInst* core, int userLat, const std::string& opcode) {
    int latency = 0;
    switch (userLat) {
        case 2:
            if (opcode == "*" || opcode == "mul") {
                latency = 1;
            }
            break;

        case 3:
            if (opcode == "*" || opcode == "mul") {
                latency = 2;
            }
            break;
        case 4:
            if (opcode == "*" || opcode == "mul") {
                latency = 3;
            }
        break;
        default:
            latency = 0;
            break;
    }

    return latency;
}

// require two functions
// 1. DSP_datasheet_query, takes two strings and return the delay
// strings can be A/B/C/AReg/BReg/CReg/MReg/PReg/P
// 2. DSP_margin, add some value to consider interconnect
double DSP48Querier::MAC1DSP_delay_lookup(const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    if (opcode == "*") {
        delay = mDelayData[A_P] + 2 * mDelayData[MARGIN];
    } else if (opcode == "add" || opcode == "sub") {
        delay = mDelayData[C_P] + 2 * mDelayData[MARGIN];
    } else if (opcode == "mul") {
        double diff = mDelayData[A_P] - mDelayData[C_P];
        delay = std::max(diff, 0.0);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::MAC1DSP2S_delay_lookup(const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    double clk_period = core->getDelayBudget();
    
    if (opcode == "*") {
        delay = std::max({mDelayData[A_PREG], mDelayData[AREG_P], mDelayData[A_MREG],
                mDelayData[MREG_P], mDelayData[C_P] + mDelayData[MARGIN]}) + mDelayData[MARGIN];
    } else if (opcode == "add" || opcode == "sub") {
        // # FIXME: we should really distinguish among the data sources for the Z
        // # Mux. Now we assume external, it is pessimistic, but we often want a
        // # CReg and PReg anyway.
        delay = mDelayData[C_P] + 2 * mDelayData[MARGIN];
        delay = std::min(clk_period, delay);
    } else if (opcode == "mul") {
        double diff = mDelayData[AREG_P] - mDelayData[C_P] - mDelayData[MARGIN];
        delay = std::max(diff, mDelayData[A_MREG] + mDelayData[MARGIN]);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::MAC1DSP3S_delay_lookup(const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    double clk_period = core->getDelayBudget();
    
    if (clk_period < 0.1) {
        // #error "clock period not set correctly"
        clk_period = 10.0;
    }

    if (opcode == "*") {
        // # We don't want the delay to be $CP + 2 * $margin again...
        double t1 = std::max({mDelayData[A_AREG], mDelayData[MREG_P], mDelayData[C_P]}) + mDelayData[MARGIN];
        delay = std::max(t1, mDelayData[AREG_PREG]);
    } else if (opcode == "add" || opcode == "sub") {
        // # FIXME: we should really distinguish among the data sources for the Z
        // # Mux. Now we assume external, it is pessimistic, but we often want a
        // # CReg and PReg anyway.
        delay = mDelayData[C_P] + 2 * mDelayData[MARGIN];
        delay = std::min(clk_period, delay);
    } else if (opcode == "mul") {
        delay = std::min(mDelayData[A_AREG] + mDelayData[MARGIN], clk_period);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::MAC1DSP4S_delay_lookup(const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    double clk_period = core->getDelayBudget();
    if (clk_period < 0.1) {
        clk_period = 8.0;
    } 
    if (opcode == "*") {
        delay = std::max({mDelayData[A_AREG], mDelayData[AREG_MREG], mDelayData[MREG_PREG], mDelayData[PREG_P]});
    } else if (opcode == "add" || opcode == "sub") {
        delay = mDelayData[C_PREG];
    } else if (opcode == "mul") {
        delay = std::min(clk_period, mDelayData[A_AREG] + mDelayData[MARGIN]);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::MAC1DSP5S_delay_lookup(const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    double clk_period = core->getDelayBudget();
    const IPBlockInst* ipBlock = static_cast<const IPBlockInst*>(core);
    bool c_reg = ipBlock->getCReg();
    if (clk_period < 0.1) {
        clk_period = 8.0;
    }
    if (opcode == "*") {
        delay = std::max({mDelayData[A_AREG], mDelayData[AREG_MREG], mDelayData[MREG_PREG], mDelayData[PREG_P]}) - 0.1;
    } else if (opcode == "add" || opcode == "sub") {
        if (c_reg) {
            delay = mDelayData[C_PREG] - mDelayData[C_CREG];
        } else {
            delay = mDelayData[C_PREG];
        }
    } else if (opcode == "mul") {
        delay = std::min(clk_period, mDelayData[A_AREG] + mDelayData[MARGIN]);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::AMA1DSP_delay_lookup(const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    // # combinational AMA
    if (opcode == "*") {
        delay = mDelayData[A_P] + 2 * mDelayData[MARGIN];
    } else if (isPostAdder(core, opcode)) {
        delay = mDelayData[C_P] + 2 * mDelayData[MARGIN];
    } else if (opcode == "mul") {
        double diff = mDelayData[A_P] - mDelayData[C_P];
        delay = std::max(diff, 0.0);
    } else if (isPreAdder(core, opcode)) {
        delay = 0; //# delay already included in mul...
    }
    return delay;
}

double DSP48Querier::AMA1DSP2S_delay_lookup (const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    double clk_period = core->getDelayBudget();
    // # I assume we use MReg here? 
    if (opcode == "*") {
        delay = std::max({mDelayData[A_MREG], mDelayData[MREG_P], mDelayData[A_PREG], mDelayData[C_P] + mDelayData[MARGIN]}) + mDelayData[MARGIN];
    } else if (isPostAdder(core, opcode)) {
        delay = std::min(clk_period, mDelayData[C_P] + 2.0 * mDelayData[MARGIN]);
    } else if (opcode == "mul") {
        double muldelay = mDelayData[A_MREG] + mDelayData[MARGIN];
        delay = std::min(muldelay, clk_period);
    } else if (isPreAdder(core, opcode)) {
        delay = 0;
    }
    return delay;
}

double DSP48Querier::AMA1DSP3S_delay_lookup (const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    double clk_period = core->getDelayBudget();
    // # I assume we use ADReg and MReg here
    if (clk_period < 0.1) {
        // #error "clock period not set correctly"
        clk_period = 5.0; 
    }

    double muldelay = std::min(mDelayData[A_AREG] + mDelayData[MARGIN], clk_period);
    if (opcode == "*") {
        double t1 = std::max({mDelayData[A_AREG], mDelayData[MREG_P], mDelayData[C_P]}) + mDelayData[MARGIN];
        delay = std::max(t1, mDelayData[AREG_MREG]);
    } else if (isPostAdder(core, opcode)) {
        delay = mDelayData[C_P] + 2 * mDelayData[MARGIN];
        delay = std::min(clk_period, delay);
    } else if (opcode == "mul") {
        // # only consider B input
        delay = muldelay;
    } else if (isPreAdder(core, opcode)) {
        double upper = clk_period - muldelay;
        delay = std::min(upper, mDelayData[A_AREG] + 2 * mDelayData[MARGIN]);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::AMA1DSP4S_delay_lookup(const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    // # I assume we use A/BReg and MReg here
    double clk_period = core->getDelayBudget();
    if (clk_period < 0.1) {
        // #puts "clock period specification error"
        // #error "clock period not set correctly"
        clk_period = 8.0; 
    }

    double muldelay = std::min(mDelayData[A_AREG] + mDelayData[MARGIN], clk_period);
    if (opcode == "*") {
        // # Please note, we do not use margin here.
        delay = std::max({mDelayData[A_AREG], mDelayData[AREG_MREG], mDelayData[MREG_PREG], mDelayData[PREG_P]});
    } else if (isPostAdder(core, opcode)) {
        // #return 1000000;        #old method
        // # if CReg, cregpreg=c_preg-c_creg
        // # if no CReg, c_preg
        delay = mDelayData[C_PREG];
    } else if (opcode == "mul") {
        // # only consider B input
        delay = muldelay;
    } else if (isPreAdder(core, opcode)) {
        double upper = clk_period - muldelay;
        delay = std::min(upper, mDelayData[A_AREG] + 2 * mDelayData[MARGIN]);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::AMA1DSP5S_delay_lookup (const CoreInst* core, const std::string& opcode)  {
    double delay = 0.0;
    const IPBlockInst* ipBlock = static_cast<const IPBlockInst*>(core);
    bool c_reg = ipBlock->getCReg();

    double clk_period = core->getDelayBudget();
    if (clk_period < 0.1) {
        // #puts "clock period specification error"
        // #error "clock period not set correctly"
        clk_period = 8.0; 
    }
    double muldelay = std::min({mDelayData[A_AREG] + mDelayData[MARGIN], clk_period});
    if (opcode == "*") {
        // # Please note, we do not use margin here.
        // # minus 0.1 to differ with 4S
        delay = std::max({mDelayData[A_AREG], mDelayData[AREG_MREG], mDelayData[MREG_PREG], mDelayData[PREG_P]}) - 0.1;
    } else if (isPostAdder(core, opcode)) {
        // # if CReg, cregpreg=c_preg-c_creg
        // # if no CReg, c_preg
        if (c_reg) {
            delay = mDelayData[C_PREG] - mDelayData[C_CREG];
        } else {
            delay = mDelayData[C_PREG];
        }
    } else if (opcode == "mul") {
        // # only consider B input
        delay = muldelay;
    } else if (isPreAdder(core, opcode)) {
        double upper = clk_period - muldelay;
        delay = std::min(upper, mDelayData[A_AREG] + 2 * mDelayData[MARGIN]);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::AM_delay_lookup (const CoreInst* core, const std::string& opcode) {
    // # combinational
    if (opcode == "*" || opcode == "mul") {
        return  mDelayData[A_P] + 2 * mDelayData[MARGIN];
    }
    return 0;
}

double DSP48Querier::AM2S_delay_lookup (const CoreInst* core, const std::string& opcode) {
    if (opcode == "*" || opcode == "mul") {
        return std::max({mDelayData[A_MREG], mDelayData[MREG_P], mDelayData[A_PREG]}) + 1.5 * mDelayData[MARGIN];
    }
    return 0;
}

double DSP48Querier::AM3S_delay_lookup (const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    double clk_period = core->getDelayBudget();
    if (clk_period < 0.1) {
        // #error "clock period not set correctly"
        clk_period = 5.0; 
    }
    double muldelay = mDelayData[MARGIN] + std::max(mDelayData[A_AREG], mDelayData[PREG_P]);
    if (opcode == "*") {
        delay = mDelayData[AREG_PREG];
    } else if (opcode == "mul") {
        delay = std::min(clk_period, muldelay);
    } else if (isPreAdder(core, opcode)) {
        double upper = clk_period - muldelay;
        delay = std::min(upper, mDelayData[A_AREG] + 3 * mDelayData[MARGIN]);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::AM4S_delay_lookup (const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    double clk_period = core->getDelayBudget();
    if (clk_period < 0.1) {
        // #puts "clock period specification error"
        // #error "clock period not set correctly"
        clk_period = 8.0; 
    }

    double muldelay = std::min(mDelayData[MARGIN] + std::max(mDelayData[A_AREG], mDelayData[PREG_P]), clk_period);
    if (opcode == "*") {
        // # Please note, we do not use margin here.
        delay = std::max({mDelayData[A_AREG], mDelayData[AREG_MREG], mDelayData[MREG_PREG], mDelayData[PREG_P]});
    } else if (opcode == "mul") {
        delay = muldelay;
    } else if (isPreAdder(core, opcode)) {
        double upper = clk_period - muldelay;
        delay = std::min(upper, mDelayData[A_AREG] + 3 * mDelayData[MARGIN]);
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::AM5S_delay_lookup (const CoreInst* core, const std::string& opcode) {
    double delay = 0.0;
    double clk_period = core->getDelayBudget();
    if (clk_period < 0.1) {
        // #puts "clock period specification error"
        // #error "clock period not set correctly"
        clk_period = 8.0; 
    }
    double muldelay = std::min({mDelayData[MARGIN] + std::max(mDelayData[A_AREG], mDelayData[PREG_P]), clk_period});

    if (opcode == "*") {
        // # Please note, we do not use margin here.
        // # minus 0.1 to differ with 4S
        delay = std::max({mDelayData[A_AREG], mDelayData[AREG_MREG], mDelayData[MREG_PREG], mDelayData[PREG_P]}) - 0.1;
    } else if (opcode == "mul") {
        delay = muldelay;
    } else if (isPreAdder(core, opcode)) {
        double upper = clk_period - muldelay;
        delay = std::min(upper, mDelayData[A_AREG] + 3 * mDelayData[MARGIN]);
    }
    assert(delay > 0);
    return delay;
}

// ## require two functions
// ## 1. DSP_datasheet_query, takes two strings and return the delay
// ## strings can be A/B/C/AReg/BReg/CReg/MReg/PReg/P
// ## 2. DSP_margin, add some value to consider interconnect
double DSP48Querier::MUL1DSP_delay_lookup (const CoreInst* core, const std::string& opcode) {
    // # CR 861745. Don't use -operands, use -metadata instead
    // #set width_list [query_core $core -operands]
    // set paramItem [query_core $core -metadata]

    // set width_list [dict get $paramItem a]
    // lappend width_list [dict get $paramItem b]

    // set min_bit [min $width_list]
    // set max_bit [max $width_list]
    double delay = 0.0;
    auto ip = static_cast<const IPBlockInst*>(core);
    auto param = metatataToMap(ip->getMetadata());
    int max_bit = std::max(std::stoi(param["a"]), std::stoi(param["b"]));

    // # combinational
    if (opcode == "*" || opcode == "mul") {
        // # this hack exist in kintex7 and virtex6
        if (max_bit <= 8) {
            delay = 3.80;
        }
        // #puts "DEBUG: MUL1DSP_delay_lookup return [ expr [ DSP_datasheet_query A P ] + 2 * [ DSP_margin ] ]"
        delay = mDelayData[A_P] + 2 * mDelayData[MARGIN];
    }
    assert(delay > 0);
    return delay;
}

double DSP48Querier::MUL1DSP2S_delay_lookup (const CoreInst* core, const std::string& opcode) {
    if (opcode == "*" || opcode == "mul") {
        return std::max({mDelayData[A_MREG], mDelayData[MREG_P], mDelayData[A_PREG]}) + 1.5 * mDelayData[MARGIN];
    }
    return 0;
}

double DSP48Querier::MUL1DSP3S_delay_lookup (const CoreInst* core, const std::string& opcode) {
    double clk_period = core->getDelayBudget();
    if (clk_period < 0.1) {
        // #error "clock period not set correctly"
        clk_period = 5.0; 
    }

    if (opcode == "*" || opcode == "mul") {
        double max_mp = std::max(mDelayData[A_MREG], mDelayData[PREG_P]) + mDelayData[MARGIN];
        return std::max({max_mp, mDelayData[AREG_PREG], mDelayData[AREG_MREG]});
    }
    return 0;
}

double DSP48Querier::MUL1DSP4S_delay_lookup (const CoreInst* core, const std::string& opcode) {
    if (opcode == "*" || opcode == "mul") {
        return std::max({mDelayData[A_AREG], mDelayData[AREG_MREG], mDelayData[MREG_PREG], mDelayData[PREG_P]});
    }
    return 0;
}

int DSP48Querier::queryLatency(CoreInst *core) {
    auto ip = static_cast<IPBlockInst*>(core);
    auto param = metatataToMap(ip->getMetadata());
    int user_lat = core->getLatencyBudget();

    std::string opcode;
    if (mCurOper != AnyOperation) {
        opcode = PlatformBasic::getInstance()
            ->getOpName(static_cast<PlatformBasic::OP_TYPE>(mCurOper));
    } else {
        opcode = "*";
    } 
    if (opcode == "select") {
        return 0;
    }

    double clk_budget = core->getDelayBudget();
    if (param.size() == 0) {
        return 4;
    }

    int latency = 0;
    DSP48Querier::DSP48Type dsp48Type = getDSP48Type(param);

    if (user_lat >= 0) {
        if (user_lat == 0) {
            latency = 0;
        } else {
            user_lat = user_lat + 1;
            switch (dsp48Type) {
                // ama
                case AMA:
                    latency = AMA1DSPnS_latency_lookup(core, user_lat, opcode);
                break;
                // mac
                case MAC:
                    latency = MAC1DSPnS_latency_lookup(core, user_lat, opcode);
                break;
                // am
                case AM:
                    latency = AMnS_latency_lookup(core, user_lat, opcode);
                break;
                // mul
                case MUL:
                    latency = MUL1DSPnS_latency_lookup(core, user_lat, opcode);
                break;

                default:
                    latency = 0;
                break;
            }
        }
    } else {    // user latency < 0
        switch (dsp48Type) { 
            // ama
            case AMA:
                if (AMA1DSP_delay_lookup(core, opcode) <= clk_budget) {
                    latency = 0;
                } else if (AMA1DSP2S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = AMA1DSPnS_latency_lookup(core, 2, opcode);
                } else if (AMA1DSP3S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = AMA1DSPnS_latency_lookup(core, 3, opcode);
                } else if (AMA1DSP4S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = AMA1DSPnS_latency_lookup(core, 4, opcode);
                } else {
                    latency = AMA1DSPnS_latency_lookup(core, 5, opcode);
                }
            break;
            // mac
            case MAC:
                if (MAC1DSP_delay_lookup(core, opcode) <= clk_budget) {
                    latency = 0;
                } else if (MAC1DSP2S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = MAC1DSPnS_latency_lookup(core, 2, opcode);
                } else if (MAC1DSP3S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = MAC1DSPnS_latency_lookup(core, 3, opcode);
                } else if (MAC1DSP4S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = MAC1DSPnS_latency_lookup(core, 4, opcode);
                } else {
                    latency = MAC1DSPnS_latency_lookup(core, 5, opcode);
                }
            break;

            // am
            case AM:
                if (AM_delay_lookup(core, opcode) <= clk_budget) {
                    latency = 0;
                } else if (AM2S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = AMnS_latency_lookup(core, 2, opcode);
                } else if (AM3S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = AMnS_latency_lookup(core, 3, opcode);
                } else if (AM4S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = AMnS_latency_lookup(core, 4, opcode);
                } else {
                    latency = AMnS_latency_lookup(core, 5, opcode);
                }
            break;
            // mul
            case MUL:
                if (MUL1DSP_delay_lookup(core, opcode) <= clk_budget) {
                    latency = 0;
                } else if (MUL1DSP2S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = MUL1DSPnS_latency_lookup(core, 2, opcode);
                } else if (MUL1DSP3S_delay_lookup(core, opcode) <= clk_budget) {
                    latency = MUL1DSPnS_latency_lookup(core, 3, opcode);
                } else {
                    latency = MUL1DSPnS_latency_lookup(core, 4, opcode);
                }
            break;

            default:
                latency = 0;
            break;
        }
    }
    return latency;
}

int DSP48Querier::queryMaxLatency(const CoreInst* core) {
    auto ip = static_cast<const IPBlockInst*>(core);
    auto param = metatataToMap(ip->getMetadata());

    std::string opcode;
    if (mCurOper != AnyOperation) {
        opcode = PlatformBasic::getInstance()
            ->getOpName(static_cast<PlatformBasic::OP_TYPE>(mCurOper));
    } else {
        opcode = "*";
    } 
    if (opcode == "select") {
        return 0;
    }
    if (param.size() == 0) {
        return 4;
    }
    int latency = 0;
    DSP48Querier::DSP48Type dsp48Type = getDSP48Type(param);

    switch (dsp48Type) { 
        // ama
        case AMA:
            latency = AMA1DSPnS_latency_lookup(core, 5, opcode);
        break;
        // mac
        case MAC:
            latency = MAC1DSPnS_latency_lookup(core, 5, opcode);
        break;

        // am
        case AM:
            latency = AMnS_latency_lookup(core, 5, opcode);
        break;
        // mul
        case MUL:
            latency = MUL1DSPnS_latency_lookup(core, 4, opcode);
        break;

        default:
            latency = 0;
        break;
    }
    return latency;
}

double DSP48Querier::queryDelay(CoreInst* core){
    auto ip = static_cast<IPBlockInst*>(core);
    auto param = metatataToMap(ip->getMetadata());
    int user_lat = core->getLatencyBudget();
    
    std::string opcode;
    if (mCurOper != AnyOperation) {
        opcode = PlatformBasic::getInstance()
            ->getOpName(static_cast<PlatformBasic::OP_TYPE>(mCurOper));
    } else {
        opcode = "*";
    }
    // The query select/sext/zext delay is not supported
    if (opcode == "select" || opcode == "sext" || opcode == "zext") {
        return 0.0;
    }
    double clk_budget = core->getDelayBudget();

    if (param.size() == 0) {
        return std::min({MAC1DSP5S_delay_lookup(core, opcode), AMA1DSP5S_delay_lookup(core, opcode), 
                    AM5S_delay_lookup(core, opcode), MUL1DSP4S_delay_lookup(core, opcode)});
    }
    double delay = 0.0;
    DSP48Querier::DSP48Type dsp48Type = getDSP48Type(param);

    switch (dsp48Type) { 
        case AM:
            if (user_lat >= 0) {
                if (user_lat == 0) {
                    delay = AM_delay_lookup(core, opcode);
                } else {
                    user_lat = user_lat + 1;
                    switch (user_lat) {
                        case 2:
                            delay = AM2S_delay_lookup(core, opcode);
                        break;
                        case 3:
                            delay = AM3S_delay_lookup(core, opcode);
                        break;
                        case 4:
                            delay = AM4S_delay_lookup(core, opcode);
                        break;
                        case 5:
                            delay = AM5S_delay_lookup(core, opcode);
                        break;
                        default:

                        break;
                    }
                }
            } else {
                if (AM_delay_lookup(core, opcode) <= clk_budget) {
                    delay = AM_delay_lookup(core, opcode);
                } else if (AM2S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = AM2S_delay_lookup(core, opcode);
                } else if (AM3S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = AM3S_delay_lookup(core, opcode);
                } else if (AM4S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = AM4S_delay_lookup(core, opcode);
                } else {
                    delay = AM5S_delay_lookup(core, opcode);
                }
            }
        break;
        case AMA:
            if (user_lat >= 0) {
                if (user_lat == 0) {
                    delay = AMA1DSP_delay_lookup(core, opcode);
                } else {
                    user_lat = user_lat + 1;
                    switch (user_lat) {
                        case 2:
                            delay = AMA1DSP2S_delay_lookup(core, opcode);
                        break;
                        case 3:
                            delay = AMA1DSP3S_delay_lookup(core, opcode);
                        break;
                        case 4:
                            delay = AMA1DSP4S_delay_lookup(core, opcode);
                        break;
                        case 5:
                            delay = AMA1DSP5S_delay_lookup(core, opcode);
                        break;
                        default:

                        break;
                    }
                }
            } else {
                if (AMA1DSP_delay_lookup(core, opcode) <= clk_budget) {
                    delay = AMA1DSP_delay_lookup(core, opcode);
                } else if (AMA1DSP2S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = AMA1DSP2S_delay_lookup(core, opcode);
                } else if (AMA1DSP3S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = AMA1DSP3S_delay_lookup(core, opcode);
                } else if (AMA1DSP4S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = AMA1DSP4S_delay_lookup(core, opcode);
                } else {
                    delay = AMA1DSP5S_delay_lookup(core, opcode);
                }
            }
            
        break;

        case MAC:
            if (user_lat >= 0) {
                if (user_lat == 0) {
                    delay = MAC1DSP_delay_lookup(core, opcode);
                } else {
                    user_lat = user_lat + 1;
                    switch (user_lat) {
                        case 2:
                            delay = MAC1DSP2S_delay_lookup(core, opcode);
                        break;
                        case 3:
                            delay = MAC1DSP3S_delay_lookup(core, opcode);
                        break;
                        case 4:
                            delay = MAC1DSP4S_delay_lookup(core, opcode);
                        break;
                        case 5:
                            delay = MAC1DSP5S_delay_lookup(core, opcode);
                        break;
                        default:

                        break;
                    }
                }
            } else {
                if (MAC1DSP_delay_lookup(core, opcode) <= clk_budget) {
                    delay = MAC1DSP_delay_lookup(core, opcode);
                } else if (MAC1DSP2S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = MAC1DSP2S_delay_lookup(core, opcode);
                } else if (MAC1DSP3S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = MAC1DSP3S_delay_lookup(core, opcode);
                } else if (MAC1DSP4S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = MAC1DSP4S_delay_lookup(core, opcode);
                } else {
                    delay = MAC1DSP5S_delay_lookup(core, opcode);
                }
            }
            
        break;
        // mul
        case MUL:
            if (user_lat >= 0) {
                if (user_lat == 0) {
                    delay = MUL1DSP_delay_lookup(core, opcode);
                } else {
                    user_lat = user_lat + 1;
                    switch (user_lat) {
                        case 2:
                            delay = MUL1DSP2S_delay_lookup(core, opcode);
                        break;
                        case 3:
                            delay = MUL1DSP3S_delay_lookup(core, opcode);
                        break;
                        case 4:
                            delay = MUL1DSP4S_delay_lookup(core, opcode);
                        break;
                    
                        default:

                        break;
                    }
                }
            } else {
                if (MUL1DSP_delay_lookup(core, opcode) <= clk_budget) {
                    delay = MUL1DSP_delay_lookup(core, opcode);
                } else if (MUL1DSP2S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = MUL1DSP2S_delay_lookup(core, opcode);
                } else if (MUL1DSP3S_delay_lookup(core, opcode) <= clk_budget) {
                    delay = MUL1DSP3S_delay_lookup(core, opcode);
                } else {
                    delay = MUL1DSP4S_delay_lookup(core, opcode);
                }
            }
        break;

        default:
            delay = 0.0;
        break;
    }

    return delay;

}

std::vector<double> DSP48Querier::queryDelayList(CoreInst* core) {
    double delay = queryDelay(core);
    // assert(delay > 0);
    return {delay, delay, delay};
}

ResourceData DSP48Querier::queryResource(CoreInst* core) {
    ResourceData resource;
    resource.Dsp = 1;
    return resource;
}

// # check if post-adder can be absorbed
int DSP48Querier::mac_chk (int a_port, int b_port, int c_port, int o_port, int a_eff,
            int a_sign, int b_eff, int b_sign, int ab_bw, int ab_sign, int c_eff, int o_eff) {

    if (can_be_signed(a_sign) && can_be_signed(b_sign) && can_be_signed(ab_sign)) {
        // # signed multiplier
    } else if (can_be_unsigned(a_sign) && can_be_unsigned(b_sign) && can_be_unsigned(ab_sign)) {
        // # need additional bit for unsigned multiplier
        a_port -= 1;
        b_port -= 1;
    } else {
        // # mixture of signed and unsigned...
        // # FIXME: handle as if things are all signed. and ab has to be signed
        if (can_be_signed(ab_sign) == 0) {
            return 0;
        }
        // # whether increase a_eff or decrease a_port?
        if (can_be_signed(a_sign) == 0) {
            a_eff += 1;
            ab_bw += 1;
        }
        if (can_be_signed(b_sign) == 0) {
            b_eff += 1;
            ab_bw += 1;
        }
    }

    // # a_port is larger or equal to b_port
    if (a_port < b_port) { 
        // #puts "wrong data regarding port width in DSP library"
    }

    int req = o_eff;
    int ab_eff = a_eff + b_eff;

    // #puts "DEBUG: eff: $a_eff, $b_eff, $ab_eff, $ab_bw, $req"
    // #puts "DEBUG: port: $a_port, $b_port, $c_port, $o_port"

    // # A few notes. a. ISE does not think a 30x12->12 multiplier can fit into one
    // # DSP48, thus A/B port does not allow overflow. b. A 50+30->10 adder is
    // # regarded as a 10-bit adder, and higher bits are disregarded, thus it is OK
    // # for C bitwidth to go beyond 48 bit, as long as the final result is within
    // # 48 bits, higher bits will be discarded. Thus effectively, we do not need
    // # to check the c_port.
    if (a_eff > a_port || b_eff > b_port || req > o_port) {
        return 0;           // # cannot fit into one dsp
    }
    // # In theory a*b output bitwidth should be no less than sum(a_eff+b_eff)
    if (ab_bw < ab_eff) {
        if (ab_bw < req) {
            // #puts "DEBUG: efficient M bitwidth exceeds the actual M bitwidth, MAC cannot be inferred!"
            return 0;
        }
    }
    return 1;
}
// ######
// # metadata string should follow the format:
// #    "<keyword0> <value0> <keyword1> <value1> ..... "
// # allowed keywords:
// #    a : A input bitwidth
// #    b : B input bitwidth
// #    c : C input bitwidth
// #    ab : bitwidth (after possible truncation but not sext) of mul result
// #    cab : bitwidth (after possible truncation but not sext) of add/sub result
// #    output : bitwidth of output (including sext after add/sub)
int DSP48Querier::xil_gen_mac_chk (int a_port, int b_port, int c_port, int o_port, const std::map<std::string, std::string>& param) {
    static const std::vector<std::string> needKeys = {"a", "a_sign", 
        "b", "b_sign", "c", "ab", "ab_sign"
    };
    if (!isIncludeAllKeys(param, needKeys)) {
        return 0;
    }

    int a_eff = std::stoi(param.at("a"));
    int a_sign = std::stoi(param.at("a_sign"));
    int b_eff = std::stoi(param.at("b"));
    int b_sign = std::stoi(param.at("b_sign"));
    int c_eff = std::stoi(param.at("c"));
    int ab_bw = std::stoi(param.at("ab"));
    int ab_sign = std::stoi(param.at("ab_sign"));
    
    int cab_bw = 0;
    if (param.count("cab") > 0) {
        // # match c-a*b
        cab_bw = std::stoi(param.at("cab"));
    } else if (param.count("abc") > 0){
        // # match a*b-c
        cab_bw = std::stoi(param.at("abc"));
    } else {
        return 0;
    }
    int output_bw = cab_bw;
    if (param.count("output")) {
        output_bw = std::stoi(param.at("output"));
    }

    return mac_chk(a_port, b_port, c_port, o_port, a_eff, a_sign, b_eff, b_sign, ab_bw, ab_sign, c_eff, output_bw);

}

// ########### AMA ###########

// ######
// # metadata string should follow the format:
// #    "<keyword0> <value0> <keyword1> <value1> ..... "
// # allowed keywords:
// #    a : A input bitwidth
// #    b : B input bitwidth
// #    c : C input bitwidth
// #    d : D input bitwidth
// #    ab : bitwidth (after possible truncation but not sext) of mul result
// #    cab : bitwidth (after possible truncation but not sext) of add/sub result
// #    output : bitwidth of output (including sext after add/sub)
int DSP48Querier::xil_gen_ama_chk(int a_port, int b_port, int c_port, int d_port, int o_port, const std::map<std::string, std::string>& param) {
    // # a special check for two MACC pack
   
    if (param.count("RND") > 0) {
        if (!isIncludeAllKeys(param, {"a", "b", "d", "c"})) {
            return 0;
        }
        int a_eff = std::stoi(param.at("a"));
        int b_eff = std::stoi(param.at("b"));
        int d_eff = std::stoi(param.at("d"));
        int c_eff = std::stoi(param.at("c"));

        if (a_eff == 26 && b_eff == 8 && c_eff == 48 && d_eff == 8) {
            return 1;
        }
        return 0;
    }

    if (param.count("dada_sign") > 0) {
        if (!isIncludeAllKeys(param, {"a", "a_sign", "c", "d", "d_sign", "da", "da_sign", "dada", "dada_sign"})) {
            return 0;
        }
        int a_eff = std::stoi(param.at("a"));
        int c_eff = std::stoi(param.at("c"));
        int d_eff = std::stoi(param.at("d"));
        int da_bw = std::stoi(param.at("da"));
        int da_sign = std::stoi(param.at("da_sign"));
        int dada_bw = std::stoi(param.at("dada"));
        int dada_sign = std::stoi(param.at("dada_sign"));

        int cdada_bw = 0;
        if (param.count("cdada") > 0) {
            // # match c-(d+a)*b
            cdada_bw = std::stoi(param.at("cdada"));
        } else if (param.count("dadac") > 0){
            // # match (d+a)*b-c
            cdada_bw = std::stoi(param.at("dadac"));
        } else {
            return 0;
        }
        
        // TODO:  ?????????????
        // int output_bw = cdab_bw;
        int output_bw = cdada_bw;
        if (param.count("output")) {
            output_bw = std::stoi(param.at("output"));
        }
        
        // # effectively, A/B can only be 24 bits, at least for xst
        int da_eff = std::max(a_eff, d_eff) + 1; // # <= 25
        // # square for b
        int b_eff = da_eff;
        int b_sign = da_sign;
        int dab_bw = dada_bw;
        int dab_sign = dada_sign;

        if (da_bw > b_port) {
            return 0;
        }
        return mac_chk(a_port, b_port, c_port, o_port, da_bw, da_sign, b_eff, b_sign, dab_bw, dab_sign, c_eff, output_bw);
    } else {
        if (!isIncludeAllKeys(param, {"a", "a_sign", "b", "b_sign", "c", 
                "d", "d_sign", "da", "da_sign", "dab", "dab_sign"})) {
            return 0;
        }
        int b_eff = std::stoi(param.at("b"));
        int b_sign = std::stoi(param.at("b_sign"));
        int c_eff = std::stoi(param.at("c"));
        int da_bw = std::stoi(param.at("da"));
        int da_sign = std::stoi(param.at("da_sign"));
        int dab_bw = std::stoi(param.at("dab"));
        int dab_sign = std::stoi(param.at("dab_sign"));

        int cdab_bw = 0;
        if (param.count("cdab") > 0) {
            // # match c-(d+a)*b
            cdab_bw = std::stoi(param.at("cdab"));
        } else {
            // # match (d+a)*b-c
            cdab_bw = std::stoi(param.at("dabc"));
        }
        int output_bw = cdab_bw;
        if (param.count("output")) {
            output_bw = std::stoi(param.at("output"));
        }

        if (da_bw > a_port) {
            return 0;
        }
        return mac_chk(a_port, b_port, c_port, o_port, da_bw, da_sign, b_eff, b_sign, dab_bw, dab_sign, c_eff, output_bw);
    }
}


// #### Mult preadder
int DSP48Querier::xil_gen_am_chk (int a_port, int b_port, int d_port, int o_port, const std::map<std::string, std::string>& param) {    
    if (param.count("dada_sign") > 0) {
        if(!isIncludeAllKeys(param, {"a", "a_sign", "d", "d_sign", "da", "da_sign", "dada", "dada_sign"})) {
            return 0;
        }
        int a_eff = std::stoi(param.at("a"));
        int d_eff = std::stoi(param.at("d"));
        int da_bw = std::stoi(param.at("da"));
        int dada_bw = std::stoi(param.at("dada"));
        int output_bw = dada_bw;
        if (param.count("output") > 0) {
            output_bw = std::stoi(param.at("output"));
        }

        int da_eff = std::max(a_eff, d_eff) + 1; // # <= 25

        // Previous check (da_bw < da_eff && da_bw != a_port) conflict with FE's current behavior, for example:
        //     In the pattern r = (a + d) * b, where a, d, b, r has the same bitwidth n, the result of (a + d) has the same bitwidth n.
        // It has been proved that for a multiplication z = x * y, truncate x or y is equivalent to truncate the output z. 
        // Read the following page for more info: https://confluence.xilinx.com/display/~jinz/DSP+AM+model+mapping+problem
        if (da_bw < da_eff && da_bw < output_bw) {
            return 0;
        }

        // Check the bw upper bound to fit into exactly 1 dsp
        if (da_eff > a_port || da_eff > b_port) {
            // da_eff > a_port: The pre-adder result bw > input bw to multiplier (the one from pre-adder result, the other input is from b_port).
            //                  Here a_port bw == b_port bw == input to mul from pre-adder result
            // da_eff > b_port: The pre-adder result bw > b_port bw (This is specific for dada pattern)
            return 0;                      // # cannot fit into one dsp
        }

        // Check the lower bound
        if (da_eff < 5) {
            return 0;
        }
    } else {
        if(!isIncludeAllKeys(param, {"a", "a_sign", "b", "b_sign", 
            "d", "d_sign", "da", "da_sign", "dab", "dab_sign"})) {
            return 0;
        }
        int a_eff = std::stoi(param.at("a"));
        int a_sign = std::stoi(param.at("a_sign"));
        int b_eff = std::stoi(param.at("b"));
        int b_sign = std::stoi(param.at("b_sign"));
        int d_eff = std::stoi(param.at("d"));
        int da_bw = std::stoi(param.at("da"));
        int dab_bw = std::stoi(param.at("dab"));
        int output_bw = dab_bw;
        if (param.count("output") > 0) {
            output_bw = std::stoi(param.at("output"));
        } 
        // # CR 953029. We should check if a b is signed.
        if (can_be_unsigned(a_sign) && can_be_unsigned(b_sign)) {
            a_port -= 1;
            b_port -= 1;
        }

        int da_eff = std::max(a_eff, d_eff) + 1; // # <= 25

        // Previous check (da_bw < da_eff && da_bw != a_port) conflict with FE's current behavior, for example:
        //     In the pattern r = (a + d) * b, where a, d, b, r has the same bitwidth n, the result of (a + d) has the same bitwidth n.
        // It has been proved that for a multiplication z = x * y, truncate x or y is equivalent to truncate the output z. 
        // Read the following page for more info: https://confluence.xilinx.com/display/~jinz/DSP+AM+model+mapping+problem
        if (da_bw < da_eff && da_bw < output_bw) {
            return 0;
        }
        
        // Check the bw upper bound to fit into exactly 1 dsp
        if (da_eff > a_port || b_eff > b_port) {
            // da_eff > a_port: The pre-adder result bw > input bw to multiplier (the one from pre-adder result, the other input is from b_port).
            //                  Here a_port bw == b_port bw == input to mul from pre-adder result
            // da_eff > b_port: The b port data bw > b_port bw (This is specific for dada pattern)
            return 0;                       // # cannot fit into one dsp
        }

        // Check the bw lower bound
        if (da_eff < 5 || b_eff < 5) {
            return 0;
        }
    }
    return 1;
}


// ########### MUL ###########

// ######
// # metadata string should follow the format:
// #    "<keyword0> <value0> <keyword1> <value1> ..... "
// # allowed keywords:
// #    a : A input bitwidth
// #    b : B input bitwidth
// #    c : C input bitwidth
// #    d : D input bitwidth
// #    ab : bitwidth (after possible truncation but not sext) of mul result
// #    cab : bitwidth (after possible truncation but not sext) of add/sub result
// #    output : bitwidth of output (including sext after add/sub)
int DSP48Querier::xil_gen_mul_chk (int a_port, int b_port, int o_port, const std::map<std::string, std::string>& param) {
    static const std::vector<std::string> needKeys = {"a", "a_sign", 
        "b", "b_sign", "ab_sign", "ab"
    };
    if (!isIncludeAllKeys(param, needKeys)) {
        return 0;
    }
    int a_eff = std::stoi(param.at("a"));
    int a_sign = std::stoi(param.at("a_sign"));
    int b_eff = std::stoi(param.at("b"));
    int b_sign = std::stoi(param.at("b_sign"));
    int ab_bw = std::stoi(param.at("ab"));
    int output_bw = ab_bw;
    if (param.count("output") > 0) {
        output_bw = std::stoi(param.at("output"));
    }
    if (can_be_unsigned(a_sign)) {
        a_eff += 1;
    }

    if (can_be_unsigned(b_sign)) {
        b_eff += 1;
    }

    // # DSP could support 27*19 and 29*18 in the DSP48E2, 25*19 and 26*18 in the DSP48E1.
    if (!((a_eff <= a_port && b_eff <= b_port + 1) || (a_eff <= a_port + 1 && b_eff <= b_port))) {
        return 0;
    }

    // # If the input size is too small, we probably do not have to use the DSP.
    // # This will depend on the downstream tool and user options to the tool. The
    // # default behavior of ISE is not quite predictable. In the SphereDetectolr
    // # case, 49 MACs get inferred but eventually only 41 DSP blocks are used.
    // # Bitwidth does not seem the only factor in deciding whether to map to DSP.
    int wide =    std::max(a_eff, b_eff);
    int narrow =  std::min(a_eff, b_eff);
    if (wide <= 10 || narrow <= 5) {
        return 0;
    }
    return 1;
  
}

// # DSP48 legality check
bool DSP48Querier::getLegality(CoreInst* core) {

    auto ip = static_cast<IPBlockInst*>(core);
    auto param = metatataToMap(ip->getMetadata());

    DSP48Querier::DSP48Type dsp48Type = getDSP48Type(param);
    // std::cout << "DBG: DSP48Querier  dsp48Type : " << dsp48Type << " metadata : " << ip->getMetadata() << std::endl;
    switch (dsp48Type)
    {
        case AMA:
            return xil_gen_ama_chk(mPortList[A_PORT] - mPortList[DEC], mPortList[B_PORT], mPortList[C_PORT], mPortList[D_PORT]- mPortList[DEC], mPortList[O_PORT], param);
            break;
        case MAC:
            return xil_gen_mac_chk(mPortList[A_PORT], mPortList[B_PORT], mPortList[C_PORT], mPortList[O_PORT], param);
            break;
        case AM:
            return xil_gen_am_chk(mPortList[A_PORT]- mPortList[DEC], mPortList[B_PORT], mPortList[D_PORT]- mPortList[DEC], mPortList[O_PORT], param);
            break;
        case MUL:
            return xil_gen_mul_chk(mPortList[A_PORT]- mPortList[DEC], mPortList[B_PORT], mPortList[O_PORT], param);
            break;
        
        default:
            return false;
    }
}

//**********************DSP_Double_Pump_delay_lookup**************//

int DSPDoublePumpQuerier::queryLatency(CoreInst* core) {
    return 3;
}
std::vector<double> DSPDoublePumpQuerier::queryDelayList(CoreInst* core) {
    // I assume we use A/BReg and PReg here
    double delay = std::max({mDelayData[A_AREG], mDelayData[AREG_MREG],
                     mDelayData[MREG_PREG], mDelayData[PREG_P]});        
    
    return {delay,delay,delay};
}

// TODO: 
ResourceData DSPDoublePumpQuerier::queryResource(CoreInst* core) {
    ResourceData res;
    return res;   
}

//******************************QAddSub_DSP********************//

std::vector<double> DSPQAddSubQuerier::QAddSub_get_delay_by_latency(int latency, const std::string& opcode) {
    double whold_lat = 0.0;
    double op1_lat = 0.0;
    double op2_lat = 0.0;
    double op3_lat = 0.0;

    if (latency == 0) {
        whold_lat = mDelayData[A_P] + mDelayData[A_P] + 2 * mDelayData[MARGIN];
        op1_lat = mDelayData[A_P] + mDelayData[MARGIN];
        op2_lat = mDelayData[A_P] + mDelayData[MARGIN];
        op3_lat = mDelayData[A_P] + mDelayData[MARGIN];
    }
    if (latency == 1) {
        whold_lat = mDelayData[A_P] + mDelayData[A_PREG] + 2 * mDelayData[MARGIN];
        op1_lat = mDelayData[A_P] + mDelayData[MARGIN];
        op2_lat = mDelayData[A_PREG];
        op3_lat = mDelayData[A_PREG];
    }
    if (latency == 2) {
        whold_lat = std::min(mDelayData[A_PREG] + mDelayData[MARGIN], mDelayData[AREG_PREG]);
        op1_lat = mDelayData[A_PREG] + mDelayData[MARGIN];
        op2_lat = mDelayData[AREG_PREG];
        op3_lat = mDelayData[AREG_PREG];
    }
    if (latency == 3) {
        whold_lat = mDelayData[AREG_PREG];
        op1_lat   = mDelayData[AREG_PREG];
        op2_lat   = mDelayData[AREG_PREG];
        op3_lat   = mDelayData[AREG_PREG];
    }

    if (opcode == "*") {
        return {whold_lat, whold_lat, whold_lat};
    } else {
        return {op1_lat, op2_lat, op3_lat};
    }
}

std::vector<unsigned> DSPQAddSubQuerier::queryLatencyList(CoreInst* core, const std::string& opcode) {
    auto ip = static_cast<IPBlockInst*>(core);
    auto param = metatataToMap(ip->getMetadata());
    int user_lat = core->getLatencyBudget();
    
    double clk_budget = core->getDelayBudget();

    if (param.size() == 0) {
        return {3};
    }
    int lat_all = 0;
    int lat = 0;
    if (user_lat >= 0) {
        if (user_lat == 0) {
            lat_all = 0;
        }
        lat_all = user_lat;
    } else {
        for (lat=0; lat <= 3; ++ lat) {
            if (QAddSub_get_delay_by_latency(lat, "*")[0] <= clk_budget) {
                lat_all = lat;
                break;
            }
        }
    }

    if (lat > 3) {
        lat_all = 3;
    }
    if (opcode == "*") {
        return {(unsigned)lat_all};
    } else {
        // # The return value is a workaround base on Scheduler machanism
        switch(lat_all) {
            case 0:
                return {0, 0, 0};
            case 1:   
                return {0, 0, 1};
            case 2:       
                return {0, 0, 1};
            case 3:       
                return {1, 0, 2};
            default: 
                return {0, 0, 0};
        }
    }

    return {0, 0, 0};
}

std::vector<unsigned> DSPQAddSubQuerier::queryLatencyList(CoreInst* core) {
    std::string opcode;
    if ((mCurOper) != AnyOperation) {
        opcode = PlatformBasic::getInstance()
            ->getOpName(static_cast<PlatformBasic::OP_TYPE>(mCurOper));
    } else {
        opcode = "*";
    }

    return queryLatencyList(core, opcode);
}

// TODO: 
int DSPQAddSubQuerier::queryLatency(CoreInst* core) {
    return queryLatencyList(core, "*")[0];
}

std::vector<double> DSPQAddSubQuerier::queryDelayList(CoreInst* core) {

    std::string opcode;
    if (mCurOper != AnyOperation) {
        opcode = PlatformBasic::getInstance()
            ->getOpName(static_cast<PlatformBasic::OP_TYPE>(mCurOper));
    } else {
        opcode = "*";
    }
    auto lat = queryLatencyList(core, "*");
    auto ret = QAddSub_get_delay_by_latency(lat[0], opcode);
    return ret;
}

// TODO: 
ResourceData DSPQAddSubQuerier::queryResource(CoreInst* core) {
    ResourceData res;
    res.Dsp = 2;
    return res;
}

bool DSPQAddSubQuerier::getLegality(CoreInst* core) {
    auto ip = static_cast<IPBlockInst*>(core);
    auto param = metatataToMap(ip->getMetadata());

    static const std::vector<std::string> needKeys = {"a", "a_sign", 
        "b", "b_sign", "c", "c_sign", "d", "d_sign"
    };
    if (!isIncludeAllKeys(param, needKeys)) {
        return false;
    }

    int a_eff = std::stoi(param.at("a"));
    int a_sign = std::stoi(param.at("a_sign"));
    int b_eff = std::stoi(param.at("b"));
    int b_sign = std::stoi(param.at("b_sign"));
    int c_eff = std::stoi(param.at("c"));
    int c_sign = std::stoi(param.at("c_sign"));
    int d_eff = std::stoi(param.at("d"));
    int d_sign = std::stoi(param.at("d_sign"));

    if(can_be_unsigned(a_sign)) {
        a_eff ++;
    }
    if(can_be_unsigned(b_sign)) {
        b_eff ++;
    }
    if(can_be_unsigned(c_sign)) {
        c_eff ++;
    }
    if(can_be_unsigned(d_sign)) {
        d_eff ++;
    }
    if (a_eff > 46 || b_sign > 46 || c_sign > 46 || d_sign > 46 ) {
        return false;
    }

    return true;
}

// TripleMGEKeyQuerier 
int TripleMGEKeyQuerier::selectMinGEValue(const char *core_name,
                                             const char *column,
                                             int value, 
                                             bool& overLimit)
{
    int length = MAX_CMD_SIZE + std::strlen(core_name) + std::strlen(column);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select min(%s) from %s_%s "
             "where CORE_NAME = '%s' COLLATE NOCASE "
             "and %s >= %d",
             column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), core_name, column, value);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    int result = s.selectInt(cmd);
    // value is greater than max(column), then choose max(column)
    if(result == 0)
    {
        int n = snprintf(cmd,
                 length,
                 "select max(%s) from %s_%s "
                 "where CORE_NAME = '%s' COLLATE NOCASE ",
                 column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), core_name);
        assert(n > 0 && n < length);
        result = s.selectInt(cmd);
        overLimit = true;
    }
    delete[] cmd;
    return result;
}

DelayMap TripleMGEKeyQuerier::selectLatencyDelayMap(const char *name_db,
                                                 PlatformBasic::IMPL_TYPE impl,
                                                                   int key0,
                                                                   int key1, 
                                                                   int key2)
{
    int length = MAX_CMD_SIZE + std::strlen(name_db);
    char* cmd = new char[length];
    int n = snprintf(cmd,
            length,
            "select LATENCY, %s from %s_%s "
            "where "
            "%s = %d and %s = %d and %s = %d and CORE_NAME = '%s' COLLATE NOCASE ",
            getDelayColumn(), GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), 
            getKey0Type(), key0, getKey1Type(), key1, getKey2Type(), key2, name_db);
    assert(n > 0 && n < length);

    auto& s = Selector::getSelector();
    auto values = s.selectDelayMap(cmd);
    delete[] cmd;
    return values;
}

std::vector<double> TripleMGEKeyQuerier::selectDelayList(const char* core_name,
                                  int key0, 
                                  int key1, 
                                  int key2, 
                                  int latency)
{

    int length = MAX_CMD_SIZE + std::strlen(core_name);
    char* cmd = new char[length];
    int n = snprintf(cmd,
            length,
            "select %s from %s_%s "
            "where "
            "%s = %d and %s = %d and %s = %d and LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
            getDelayColumn(), GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), 
            getKey0Type(), key0, getKey1Type(), key1, getKey2Type(), key2, latency, core_name);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto values = s.selectDoubleList(cmd);
    delete[] cmd;
    return values;
}

ResourceData TripleMGEKeyQuerier::selectResource(const char* core_name,
                                   int key0, 
                                   int key1, 
                                   int key2,
                                   int latency)
{
    int length = MAX_CMD_SIZE + std::strlen(core_name);
    char* cmd = new char[length];
    int n = snprintf(cmd,
            length,
            "select LUT, FF, DSP, BRAM, URAM from %s_%s "
            "where "
            "%s = %d and %s = %d and %s = %d and LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
            GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), 
            getKey0Type(), key0, getKey1Type(), key1, getKey2Type(), key2, latency, core_name);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    auto value = s.selectInt2dList(cmd)[0];
    delete[] cmd;
    ResourceData resource;
    resource.Lut = value[0];
    resource.Ff = value[1];
    resource.Dsp = value[2];
    resource.Bram = value[3];
    resource.Uram = value[4];
    return resource;
}

void TripleMGEKeyQuerier::getQueryMinGEKeys(CoreInst *core, int& key0, int& key1, int& key2) {
    std::string nameDb = getNameInDB(core);
    bool overlimit = false;
    key0 = selectMinGEValue(nameDb.c_str(), getKey0Type(), getKey0(core), overlimit);
    key1 = selectMinGEValue(nameDb.c_str(), getKey1Type(), getKey1(core), overlimit);
    key2 = selectMinGEValue(nameDb.c_str(), getKey2Type(), getKey2(core), overlimit);
}

int TripleMGEKeyQuerier::queryLatency(CoreInst* core)
{
    int user_lat = core->getConfigedLatency();
    unsigned maxLat = core->getMaxLatency(core->getOp());
    double delay_budget = core->getDelayBudget();
    std::string name_db = getNameInDB(core);

    int key0MGE;
    int key1MGE;
    int key2MGE;
    getQueryMinGEKeys(core, key0MGE, key1MGE, key2MGE);
    auto lat_delay_map = selectLatencyDelayMap(name_db.c_str(), core->getImpl(), key0MGE, key1MGE, key2MGE);
    int latency = getLatency(user_lat, maxLat, delay_budget, lat_delay_map);
    return latency;
}

std::vector<double> TripleMGEKeyQuerier::queryDelayList(CoreInst* core)
{
    int latency = TripleMGEKeyQuerier::queryLatency(core);
    std::string name_db = getNameInDB(core);
    int key0MGE;
    int key1MGE;
    int key2MGE;
    getQueryMinGEKeys(core, key0MGE, key1MGE, key2MGE);

    return selectDelayList(name_db.c_str(), key0MGE, key1MGE, key2MGE, latency);
}

ResourceData TripleMGEKeyQuerier::queryResource(CoreInst* core)
{
    int latency = TripleMGEKeyQuerier::queryLatency(core);
    std::string name_db = getNameInDB(core);
    int key0MGE;
    int key1MGE;
    int key2MGE;
    getQueryMinGEKeys(core, key0MGE, key1MGE, key2MGE);

    auto value = selectResource(name_db.c_str(), key0MGE, key1MGE, key2MGE, latency);
    return value;
}

//  SparseMuxQuerier
int SparseMuxQuerier::getKey0(CoreInst* core) {
    // DATA WIDTH
    auto *fu = static_cast<FuncUnitInst *>(core);
    return fu->getOutputBW();
}

int SparseMuxQuerier::getKey1(CoreInst* core) {
    // ADDR WIDTH
    auto *fu = static_cast<FuncUnitInst *>(core);
    const auto &inputs = fu->getInputBWList();
    if (inputs.empty()) {
        return 0;
    } 
    return inputs[inputs.size()-1];
}

int SparseMuxQuerier::getKey2(CoreInst* core) {
    // INPUT NUMBER
    auto *fu = static_cast<FuncUnitInst *>(core);
    const auto &inputs = fu->getInputBWList();
    if (inputs.empty()) {
        return 0;
    }

    return inputs.size() - 1;
}

BinarySparseMuxQuerier::BinarySparseMuxQuerier() {
    mVarLabelMux = new VarLabelMuxQuerier();
    mFullcaseMux = new FullcaseMuxQuerier();
    mSimpleLabelMux = new SimpleLabelMuxQuerier();
}

BinarySparseMuxQuerier::~BinarySparseMuxQuerier() {
    delete mVarLabelMux;
    delete mFullcaseMux;
    delete mSimpleLabelMux;
    mVarLabelMux = nullptr;
    mFullcaseMux = nullptr;
    mSimpleLabelMux = nullptr;
}

std::string BinarySparseMuxQuerier::SimpleLabelMuxQuerier::getNameInDB(CoreInst* core) {
    std::string nameInDb;
    if (core->getName() == "BinarySparseMux_DontCare"){
        nameInDb = "BinarySparseMux_DontCare_IncrEncode";
    } else if (core->getName() == "BinarySparseMux_HasDef") {
        nameInDb = "BinarySparseMux_HasDef_IncrEncode";
    }
    assert(nameInDb.length() > 0);
    return nameInDb;
}

std::string BinarySparseMuxQuerier::VarLabelMuxQuerier::getNameInDB(CoreInst* core) {
    std::string nameInDb;
    if (core->getName() == "BinarySparseMux_DontCare"){
        nameInDb = "BinarySparseMux_DontCare_Label";
    } else if (core->getName() == "BinarySparseMux_HasDef") {
        nameInDb = "BinarySparseMux_HasDef_Label";
    }
    assert(nameInDb.length() > 0);
    return nameInDb;
}
std::string BinarySparseMuxQuerier::FullcaseMuxQuerier::getNameInDB(CoreInst* core) {
    std::string nameInDb;
    if (core->getName() == "BinarySparseMux_DontCare"){
        nameInDb = "BinarySparseMux_DontCare_IncrEncode";
    } else if (core->getName() == "BinarySparseMux_HasDef") {
        nameInDb = "BinarySparseMux_HasDef_IncrEncode";
    }
    assert(nameInDb.length() > 0);
    return nameInDb;
}

int BinarySparseMuxQuerier::FullcaseMuxQuerier::getKey2(CoreInst* core) {
    // INPUT NUMBER
    auto *fu = static_cast<FuncUnitInst *>(core);
    const auto &inputs = fu->getInputBWList();
    if (inputs.empty()) {
        return 0;
    } 
    int addrWidth = inputs[inputs.size()-1];
    assert(addrWidth > 0);
    int inputNum;
    if (addrWidth >= 31) {
        inputNum = INT32_MAX;
    } else {
        inputNum = std::pow(2, addrWidth);
    }
    return inputNum;
}


int BinarySparseMuxQuerier::queryLatency(CoreInst* core) {
    auto simpleLabelLat = mSimpleLabelMux->queryLatency(core);
    auto fu = static_cast<FuncUnitInst*>(core);
    if (fu->getSparseMuxIncrEncoding()) {
        return simpleLabelLat;
    }
    auto fullcaseDelay = mFullcaseMux->queryDelayList(core);
    auto varLabelDelay = mVarLabelMux->queryDelayList(core);
    auto fullcaseLat = mFullcaseMux->queryLatency(core);
    auto varLabelLat = mVarLabelMux->queryLatency(core);
    int latency = varLabelDelay[1] < fullcaseDelay[1] ? varLabelLat : fullcaseLat;
    return latency;
}

std::vector<double> BinarySparseMuxQuerier::queryDelayList(CoreInst* core) {
    auto simpleLabelDelay = mSimpleLabelMux->queryDelayList(core);
    auto fu = static_cast<FuncUnitInst*>(core);
    if (fu->getSparseMuxIncrEncoding()) {
        return simpleLabelDelay;
    }
    auto fullcaseDelay = mFullcaseMux->queryDelayList(core);
    auto varLabelDelay = mVarLabelMux->queryDelayList(core);
    std::vector<double> delay = varLabelDelay[1] < fullcaseDelay[1] ? varLabelDelay : fullcaseDelay;
    return delay;
}

ResourceData BinarySparseMuxQuerier::queryResource(CoreInst* core) {
    auto simpleLabelRes = mSimpleLabelMux->queryResource(core);
    auto fu = static_cast<FuncUnitInst*>(core);
    if (fu->getSparseMuxIncrEncoding()) {
        return simpleLabelRes;
    }
    
    auto fullcaseDelay = mFullcaseMux->queryDelayList(core);
    auto varLabelDelay = mVarLabelMux->queryDelayList(core);
    auto fullcaseRes = mFullcaseMux->queryResource(core);
    auto varLabelRes = mVarLabelMux->queryResource(core);

    ResourceData resData = varLabelDelay[1] < fullcaseDelay[1] ? varLabelRes : fullcaseRes;
    return resData;
}

// MultipleMGEKeyQuerier 
int MultipleMGEKeyQuerier::selectMinGEValue(const char *core_name,
                                             const char *column,
                                             int value, 
                                             bool& overLimit)
{
    int length = MAX_CMD_SIZE + std::strlen(core_name) + std::strlen(column);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select min(%s) from %s_%s "
             "where CORE_NAME = '%s' COLLATE NOCASE "
             "and %s >= %d",
             column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), core_name, column, value);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    int result = s.selectInt(cmd);
    // value is greater than max(column), then choose max(column)
    if(result == 0)
    {
        int n = snprintf(cmd,
                 length,
                 "select max(%s) from %s_%s "
                 "where CORE_NAME = '%s' COLLATE NOCASE ",
                 column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), getTableName(), core_name);
        assert(n > 0 && n < length);
        result = s.selectInt(cmd);
        overLimit = true;
    }
    delete[] cmd;
    return result;
}

DelayMap MultipleMGEKeyQuerier::selectLatencyDelayMap(CoreInst *core)
{
    std::string name_db = getNameInDB(core);
    auto keyTypes = getKeyTypes();
    auto MGEKeys = getMinGEKeys(core);

    std::stringstream ss;
    ss << "select LATENCY, " << getDelayColumn() << " from " 
        << GetTargetPlatform()->getFactory().getLibraryName() 
        << "_" << getTableName() << " where ";
    for (int i=0; i < keyTypes.size(); i ++) {
        ss << keyTypes[i] << " = " << MGEKeys[i] << " and ";
    }
    ss << "CORE_NAME = '" << name_db << "'  COLLATE NOCASE ";
    std::string cmd = ss.str();

    auto& s = Selector::getSelector();
    auto values = s.selectDelayMap(cmd.c_str());
    return values;
}

std::vector<double> MultipleMGEKeyQuerier::selectDelayList(CoreInst *core,
                                  int latency)
{
    std::string name_db = getNameInDB(core);
    auto keyTypes = getKeyTypes();
    auto MGEKeys = getMinGEKeys(core);

    std::stringstream ss;
    ss << "select " << getDelayColumn() << " from " 
        << GetTargetPlatform()->getFactory().getLibraryName() 
        << "_" << getTableName() << " where "; 
    for (int i=0; i < keyTypes.size(); i ++) {
        ss << keyTypes[i] << " = " << MGEKeys[i] << " and ";
    }
    ss << "LATENCY = " << latency << " and CORE_NAME = '" << name_db << "' COLLATE NOCASE ";  

    std::string cmd = ss.str();
    auto& s = Selector::getSelector();
    auto values = s.selectDoubleList(cmd.c_str());
    return values;
}

ResourceData MultipleMGEKeyQuerier::selectResource(CoreInst *core, int latency)
{
    std::string name_db = getNameInDB(core);
    auto keyTypes = getKeyTypes();
    auto MGEKeys = getMinGEKeys(core);

    std::stringstream ss;
    ss << "select LUT, FF, DSP, BRAM, URAM from "
        << GetTargetPlatform()->getFactory().getLibraryName() 
        << "_" << getTableName() << " where "; 
    for (int i=0; i < keyTypes.size(); i ++) {
        ss << keyTypes[i] << " = " << MGEKeys[i] << " and ";
    }
    ss << "LATENCY = " << latency << " and CORE_NAME = '" << name_db << "' COLLATE NOCASE ";  
    
    std::string cmd = ss.str();
    auto& s = Selector::getSelector();
    auto value = s.selectInt2dList(cmd.c_str())[0];
    ResourceData resource;
    resource.Lut = value[0];
    resource.Ff = value[1];
    resource.Dsp = value[2];
    resource.Bram = value[3];
    resource.Uram = value[4];
    return resource;
}

std::vector<int> MultipleMGEKeyQuerier::getMinGEKeys(CoreInst *core) {
    std::string nameDb = getNameInDB(core);
    std::vector<int> MGEKeys;
    bool overlimit = false;
    auto keyTypes = getKeyTypes();
    auto keys = getKeys(core);
    int len = keyTypes.size();
    for (int i = 0; i < len; i ++) {
        MGEKeys.push_back(selectMinGEValue(nameDb.c_str(), keyTypes[i].c_str(), keys[i], overlimit));
    }
    return MGEKeys;
}

int MultipleMGEKeyQuerier::queryLatency(CoreInst* core)
{
    int user_lat = core->getConfigedLatency();
    unsigned maxLat = core->getMaxLatency(core->getOp());
    double delay_budget = core->getDelayBudget();

    auto lat_delay_map = selectLatencyDelayMap(core);
    int latency = getLatency(user_lat, maxLat, delay_budget, lat_delay_map);
    return latency;
}

std::vector<double> MultipleMGEKeyQuerier::queryDelayList(CoreInst* core)
{
    int latency = MultipleMGEKeyQuerier::queryLatency(core);
    return selectDelayList(core, latency);
}

ResourceData MultipleMGEKeyQuerier::queryResource(CoreInst* core)
{
    int latency = MultipleMGEKeyQuerier::queryLatency(core);
    auto value = selectResource(core, latency);
    return value;
}

std::vector<int> QuadrupleAxiQuerier::getKeys(CoreInst* core) {
    auto maxi = static_cast<AdapterInst*>(core);
    const auto &paraMap = maxi->getMAXIParaMap();
    // key : 0          1               2               3
    //    bus_width burst_length    outstanding     user Latency
    int busWidth    = maxi->getBitWidth();
    // default value 
    int burstLength = 64;
    int outstanding = 64;
    int userLatency = 59;
    if (paraMap.count(AdapterInst::MAXIParaKey::MaxReadBurstLen)) {
        burstLength = paraMap.at(AdapterInst::MAXIParaKey::MaxReadBurstLen);
    } 
    if (paraMap.count(AdapterInst::MAXIParaKey::NumReadOutstanding)) {
        outstanding = paraMap.at(AdapterInst::MAXIParaKey::NumReadOutstanding);
    }
    if (paraMap.count(AdapterInst::MAXIParaKey::UserLatency)) {
        userLatency = paraMap.at(AdapterInst::MAXIParaKey::UserLatency);
    }
    return {busWidth, burstLength, outstanding, userLatency};
}

DSPBuiltinQuerier::DSPBuiltinQuerier(): useAreg(false),
                                        useBreg(false),
                                        useCreg(false),
                                        useDreg(false),
                                        useADreg(false),
                                        useMreg(false),
                                        usePreg(false),
                                        usePreAdder(false) {

}

void DSPBuiltinQuerier::decidePreAdder(CoreInst* core) {
    auto fu = static_cast<FuncUnitInst*>(core);
    int func_index = fu->getOutputBW();
    IPBlockInst::DSPBuiltinOpcode opcode = 
        static_cast<IPBlockInst::DSPBuiltinOpcode>(func_index);

    switch (opcode) {
        case IPBlockInst::DSP_mul_add    : 
        case IPBlockInst::DSP_mul_sub    : 
        case IPBlockInst::DSP_mul_rev_sub: 
        case IPBlockInst::DSP_mul_acc    : 
        case IPBlockInst::DSP_full_mul_add:
            usePreAdder = false;
        break;
        case IPBlockInst::DSP_add_mul_add    : 
        case IPBlockInst::DSP_add_mul_sub    : 
        case IPBlockInst::DSP_sub_mul_add    : 
        case IPBlockInst::DSP_sub_mul_sub    : 
        case IPBlockInst::DSP_add_mul_acc    : 
        case IPBlockInst::DSP_sub_mul_rev_sub: 
        case IPBlockInst::DSP_full_add_mul_add:
            usePreAdder = true;
        break;
    }
}

void DSPBuiltinQuerier::decideRegAlloca(CoreInst* core) {
    auto fu = static_cast<FuncUnitInst*>(core);
    auto regList = fu->getInputBWList();
    // Assume BE has configured registers info on InputBWList member variable. 
    // Register order is {A1, A2, B1, B2, D, AD, M, C, P}
    usePreg = (regList[8] == 1);
    useMreg = (regList[6] == 1);
    useADreg = (regList[5] == 1);
    useCreg = (regList[7] == 1);
    useDreg = (regList[4] == 1);
    useAreg = (regList[0] == 1 || regList[1] == 1);
    useBreg = (regList[2] == 1 || regList[3] == 1);
}

int DSPBuiltinQuerier::queryLatency(CoreInst* core) {
    auto fu = static_cast<FuncUnitInst*>(core);
    auto regList = fu->getInputBWList();
    std::vector<unsigned> dataPathRegList;
    dataPathRegList.push_back(regList[0]);
    dataPathRegList.push_back(regList[1]);
    dataPathRegList.push_back(regList[5]);
    dataPathRegList.push_back(regList[6]);
    dataPathRegList.push_back(regList[8]);
    return std::count(dataPathRegList.begin(), dataPathRegList.end(), 1);
}

ResourceData DSPBuiltinQuerier::queryResource(CoreInst* core) 
{
    ResourceData resource;
    resource.Dsp = 1;
    return resource;
}

/////////////////// DSP58BuiltinQuerier //////////////////
DSP58BuiltinQuerier::DSP58BuiltinQuerier() {
    std::string cmd = "SELECT core_name, DELAY1 FROM ";
    cmd += GetTargetPlatform()->getFactory().getLibraryName();
    cmd += "_DSP58";

    auto& select = Selector::getSelector();
    std::string dsp58Table = GetTargetPlatform()->getFactory().getLibraryName() + "_DSP58";
    if (Selector::getSelector().isExistTable(dsp58Table.c_str())) {
        mDelayData = select.selectStr2DoubleMap(cmd.c_str());
        assert(!mDelayData.empty());
    }
}

double DSP58BuiltinQuerier::decideAPortDelay(double margin)
{
    if(useAreg) {
        return margin + mDelayData.at("a2areg_Through");
    } else if (useADreg) {
        return margin + mDelayData.at("a2adreg_Adder");
    } else if (useMreg) {
        return margin + (usePreAdder ? mDelayData.at("a2mreg_AdderMul") : mDelayData.at("a2mreg_Mul"));
    } else if (usePreg) {
        return margin + (usePreAdder ? mDelayData.at("a2preg_AdderMulALU") : mDelayData.at("a2preg_MulALU"));
    } else {
        return margin + (usePreAdder ? mDelayData.at("a2p_AdderMulALU") : mDelayData.at("a2p_MulALU")) + margin;
    }
}

double DSP58BuiltinQuerier::decideBPortDelay(double margin)
{
    if (useBreg) {
        return margin + mDelayData.at("b2breg_Through");
    } else if(useMreg) {
        return margin + mDelayData.at("b2mreg_Mul");
    } else if(usePreg) {
        return margin + mDelayData.at("b2preg_MulALU");
    } else {
        return margin + mDelayData.at("b2p_MulALU") + margin;
    }

}

double DSP58BuiltinQuerier::decideCPortDelay(double margin)
{
    if (useCreg) {
        return margin + mDelayData.at("c2creg_Through");
    } else if (usePreg) {
        return margin + mDelayData.at("c2preg_ALU");
    } else {
        return margin + mDelayData.at("c2p_ALU") + margin;
    }
}

double DSP58BuiltinQuerier::decideDPortDelay(double margin)
{
    if (useDreg) {
        return margin + mDelayData.at("d2dreg_Through");
    } else if (useADreg) {
        return margin + mDelayData.at("d2adreg_Adder");
    } else if (useMreg) {
        return margin + mDelayData.at("d2mreg_AdderMul");
    } else if (usePreg) {
        return margin + mDelayData.at("d2preg_AdderMulALU");
    } else {
        return margin + mDelayData.at("d2p_AdderMulALU") + margin;
    }
}

double DSP58BuiltinQuerier::decidePPortDelay(double margin)
{
    if (usePreg) {
        return mDelayData.at("preg2p_Through") + margin;
    } else if (useMreg) {
        return mDelayData.at("mreg2p_ALU") + margin;
    } else if (useADreg) {
        return mDelayData.at("adreg2p_MulALU") + margin;
    } else if (useAreg) {
        return (usePreAdder ? mDelayData.at("areg2p_AdderMulALU") : mDelayData.at("areg2p_MulALU")) + margin;
    } else {
        return margin + (usePreAdder ? mDelayData.at("a2p_AdderMulALU") : mDelayData.at("a2p_MulALU")) + margin;
    }
}

double DSP58BuiltinQuerier::decideMregDelay(double margin) 
{
    if (useADreg) {
        return mDelayData.at("adreg2mreg_Mul");
    } else if (useAreg) {
        return usePreAdder ? mDelayData.at("areg2mreg_AdderMul") : mDelayData.at("areg2mreg_Mul");
    } else {
        return usePreAdder ? mDelayData.at("a2mreg_AdderMul") : mDelayData.at("a2mreg_Mul");
    }
}

double DSP58BuiltinQuerier::decidePregDelay(double margin)
{
    if (useMreg) {
        return mDelayData.at("mreg2preg_ALU");
    } else if (useADreg) {
        return mDelayData.at("adreg2preg_MulALU");
    } else if (useAreg) {
        return usePreAdder ? mDelayData.at("areg2preg_AdderMulALU") : mDelayData.at("areg2preg_MulALU");
    } else {
        return usePreAdder ? mDelayData.at("a2preg_AdderMulALU") : mDelayData.at("a2preg_MulALU");
    }
}

std::vector<double> DSP58BuiltinQuerier::queryDelayList(CoreInst* core) {
    double a_delay,b_delay,c_delay,d_delay;
    double p_delay;
    double mreg_delay,preg_delay;
    double delay0,delay1,delay2;

    double margin = 0;

    decideRegAlloca(core);
    decidePreAdder(core);

    a_delay = decideAPortDelay(margin);
    b_delay = decideBPortDelay(margin);
    c_delay = decideCPortDelay(margin);
    d_delay = decideDPortDelay(margin);
    p_delay = decidePPortDelay(margin);
    mreg_delay = decideMregDelay(margin);
    preg_delay = decidePregDelay(margin);

    delay0 = std::max({a_delay,b_delay,c_delay,d_delay});
    delay2 = p_delay;
    delay1=  std::max({delay0, mreg_delay, preg_delay, delay2});
    
    return {delay0,delay1,delay2};
}

/////////////////// DSP48BuiltinQuerier //////////////////
DSP48BuiltinQuerier::DSP48BuiltinQuerier() {
    // get DSP delay data
    static const std::vector<std::string> colNames = {"a_areg", "a_mreg", "a_preg", "a_p", 
                                         "areg_mreg", "areg_preg", "areg_p",  
                                         "mreg_preg", "mreg_p", "preg_p",     
                                         "c_creg", "c_preg", "c_p", "margin"};
    std::string cmd = "select ";
    for (int i=0; i < colNames.size()-1; i ++) {
        cmd.append(colNames[i]);
        cmd.append(",");
    }
    cmd.append(colNames[colNames.size()-1]);
    cmd.append(" from DSP_delays where core_name=");
    std::string familyName = GetTargetPlatform()->getFactory().getFamilyName();
    std::string speed = GetTargetPlatform()->getChipInfo()->getSpeedGrade();
    cmd += "\"" + familyName + "_" + speed + "\"";
    auto& select = Selector::getSelector();
    auto delays = select.selecDSPDelayList(cmd.c_str());
    assert(delays.size() == colNames.size());

    for (int i=0; i < delays.size(); i ++) {
        mDelayData.emplace(colNames[i], delays[i]);
    }
}

double DSP48BuiltinQuerier::decideAPortDelay(double margin)
{
    if(useAreg) {
        return margin + mDelayData.at("a_areg");
    } else if (useADreg) {
        return margin + mDelayData.at("a_mreg");
    } else if (useMreg) {
        return margin + mDelayData.at("a_mreg");
    } else if (usePreg) {
        return margin + mDelayData.at("a_preg");
    } else {
        return margin + mDelayData.at("a_p") + margin;
    }
}

double DSP48BuiltinQuerier::decideCPortDelay(double margin)
{
    if (useCreg) {
        return margin + mDelayData.at("c_creg");
    } else if (usePreg) {
        return margin + mDelayData.at("c_preg");
    } else {
        return margin + mDelayData.at("c_p") + margin;
    }
}

double DSP48BuiltinQuerier::decideDPortDelay(double margin)
{
    if (useDreg) {
        return margin + mDelayData.at("a_areg");
    } else if (useADreg) {
        return margin + mDelayData.at("a_mreg");
    } else if (useMreg) {
        return margin + mDelayData.at("a_mreg");
    } else if (usePreg) {
        return margin + mDelayData.at("a_preg");
    } else {
        return margin + mDelayData.at("a_p") + margin;
    }
}

double DSP48BuiltinQuerier::decidePPortDelay(double margin)
{
    if (usePreg) {
        return mDelayData.at("preg_p") + margin;
    } else if (useMreg) {
        return mDelayData.at("mreg_p") + margin;
    } else if (useADreg) {
        return mDelayData.at("areg_p") + margin;
    } else if (useAreg) {
        return mDelayData.at("areg_p") + margin;
    } else {
        return margin + mDelayData.at("a_p") + margin;
    }
}

double DSP48BuiltinQuerier::decideMregDelay(double margin) 
{
    if (useADreg) {
        return mDelayData.at("areg_mreg");
    } else if (useAreg) {
        return mDelayData.at("areg_mreg");
    } else {
        return mDelayData.at("a_mreg");
    }
}

double DSP48BuiltinQuerier::decidePregDelay(double margin)
{
    if (useMreg) {
        return mDelayData.at("mreg_preg");
    } else if (useADreg) {
        return mDelayData.at("areg_preg");
    } else if (useAreg) {
        return mDelayData.at("areg_preg");
    } else {
        return mDelayData.at("a_preg");
    }
}

std::vector<double> DSP48BuiltinQuerier::queryDelayList(CoreInst* core) {
    double a_delay,b_delay,c_delay,d_delay;
    double p_delay;
    double mreg_delay,preg_delay;
    double delay0,delay1,delay2;

    double margin = 0;

    decideRegAlloca(core);
    decidePreAdder(core);

    a_delay = decideAPortDelay(margin);
    c_delay = decideCPortDelay(margin);
    d_delay = decideDPortDelay(margin);
    p_delay = decidePPortDelay(margin);
    mreg_delay = decideMregDelay(margin);
    preg_delay = decidePregDelay(margin);

    delay0 = std::max({a_delay,c_delay,d_delay});
    delay2 = p_delay;
    delay1=  std::max({delay0, mreg_delay, preg_delay, delay2});
    
    return {delay0,delay1,delay2};
}

int QuadrupleKeyApFloatConversionQuerier::selectMinGEValueByFixOneKey(const char *core_name,
                                             const char *column,
                                             int value, 
                                             int fixKey,
                                             unsigned fixColumn,
                                             bool& overLimit)
{
    int length = MAX_CMD_SIZE + std::strlen(core_name) + std::strlen(column);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select min(%s) from %s_%s "
             "where CORE_NAME = '%s' COLLATE NOCASE and %s = %d "
             "and %s >= %d",
             column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), 
             getTableName(), core_name, getKeyTypes()[fixColumn].c_str(), fixKey, column, value);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    int result = s.selectInt(cmd);
    // value is greater than max(column), then choose max(column)
    if(result == 0)
    {
        int n = snprintf(cmd,
                 length,
                 "select max(%s) from %s_%s "
                 "where CORE_NAME = '%s' COLLATE NOCASE "
                 "and %s = %d",
                 column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), 
                 getTableName(), core_name, getKeyTypes()[fixColumn].c_str(), fixKey);
        assert(n > 0 && n < length);
        result = s.selectInt(cmd);
        overLimit = true;
    }
    delete[] cmd;
    return result;
}

int QuadrupleKeyApFloatConversionQuerier::selectMinGEValueByFixDoubleKeys(const char *core_name,
                                            const char* column,
                                            int value, 
                                            int fixKey0,
                                            unsigned fixColumn0,
                                            int fixKey1,
                                            unsigned fixColumn1,
                                            bool& overLimit)
{
    int length = MAX_CMD_SIZE + std::strlen(core_name) + std::strlen(column);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select min(%s) from %s_%s "
             "where CORE_NAME = '%s' COLLATE NOCASE and %s = %d and %s = %d "
             "and %s >= %d",
             column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), 
             getTableName(), core_name, getKeyTypes()[fixColumn0].c_str(), fixKey0, getKeyTypes()[fixColumn1].c_str(), fixKey1, column, value);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    int result = s.selectInt(cmd);
    // value is greater than max(column), then choose max(column)
    if(result == 0)
    {
        int n = snprintf(cmd,
                 length,
                 "select max(%s) from %s_%s "
                 "where CORE_NAME = '%s' COLLATE NOCASE "
                 "and %s = %d and %s = %d",
                 column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), 
                 getTableName(), core_name, getKeyTypes()[fixColumn0].c_str(), fixKey0, getKeyTypes()[fixColumn1].c_str(), fixKey1);
        assert(n > 0 && n < length);
        result = s.selectInt(cmd);
        overLimit = true;
    }
    delete[] cmd;
    return result;
}

int QuadrupleKeyApFloatConversionQuerier::selectMinGEValueByFixTripleKeys(const char *core_name,
                                            const char* column,
                                            int value, 
                                            int fixKey0,
                                            unsigned fixColumn0,
                                            int fixKey1,
                                            unsigned fixColumn1,
                                            int fixKey2,
                                            unsigned fixColumn2,
                                            bool& overLimit)
{
    int length = MAX_CMD_SIZE + std::strlen(core_name) + std::strlen(column);
    char* cmd = new char[length];
    int n = snprintf(cmd,
             length,
             "select min(%s) from %s_%s "
             "where CORE_NAME = '%s' COLLATE NOCASE and %s = %d and %s = %d and %s = %d "
             "and %s >= %d",
             column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), 
             getTableName(), core_name, getKeyTypes()[fixColumn0].c_str(), fixKey0, getKeyTypes()[fixColumn1].c_str(), fixKey1, getKeyTypes()[fixColumn2].c_str(), fixKey2, column, value);
    assert(n > 0 && n < length);
    auto& s = Selector::getSelector();
    int result = s.selectInt(cmd);
    // value is greater than max(column), then choose max(column)
    if(result == 0)
    {
        int n = snprintf(cmd,
                 length,
                 "select max(%s) from %s_%s "
                 "where CORE_NAME = '%s' COLLATE NOCASE "
                 "and %s = %d and %s = %d and %s = %d",
                 column, GetTargetPlatform()->getFactory().getLibraryName().c_str(), 
                 getTableName(), core_name, getKeyTypes()[fixColumn0].c_str(), fixKey0, getKeyTypes()[fixColumn1].c_str(), fixKey1,getKeyTypes()[fixColumn2].c_str(), fixKey2);
        assert(n > 0 && n < length);
        result = s.selectInt(cmd);
        overLimit = true;
    }
    delete[] cmd;
    return result;
}

std::vector<int> QuadrupleKeyApFloatConversionQuerier::getKeys(CoreInst* core) {
    auto operands = CoreQuerier::getFuncUnitOperands(core);

    std::vector<int> srcOperands = {operands[0],operands[1]};
    std::vector<int> desOperands = {operands[2],operands[3]};

    int srcExpo = *std::min_element(srcOperands.begin(), srcOperands.end());
    int srcTotal = *std::max_element(srcOperands.begin(), srcOperands.end());
    int srcFrac = srcTotal - srcExpo;

    int desExpo = *std::min_element(desOperands.begin(), desOperands.end());
    int desTotal = *std::max_element(desOperands.begin(), desOperands.end());
    int desFrac = desTotal - desExpo; 

    return {srcExpo,srcFrac,desExpo,desFrac};
}

std::vector<int> QuadrupleKeyApFloatConversionQuerier::getMinGEKeys(CoreInst *core) {
    std::string nameDb = getNameInDB(core);
    std::vector<int> MGEKeys;
    bool overlimit = false;
    auto keyTypes = getKeyTypes();
    auto keys = getKeys(core);
    int len = keyTypes.size();
    if (core->getName() == "ApFloatFixed2Float") {
        for (int i = 0; i < len; i ++) {
            if (i != 1) {
                MGEKeys.push_back(selectMinGEValue(nameDb.c_str(), keyTypes[i].c_str(), keys[i], overlimit));
            } else {
                int key0MGE = MGEKeys[0];
                int key1MGE = selectMinGEValueByFixOneKey(nameDb.c_str(), keyTypes[1].c_str(), keys[1], key0MGE, 0, overlimit);
                MGEKeys.push_back(key1MGE);
            }
        }
    }
    if (core->getName() == "ApFloatFloat2Fixed") {
        for (int i = 0; i < len; i ++) {
            if (i != 3) {
                MGEKeys.push_back(selectMinGEValue(nameDb.c_str(), keyTypes[i].c_str(), keys[i], overlimit));
            } else {
                int key2MGE = MGEKeys[2];
                int key3MGE = selectMinGEValueByFixOneKey(nameDb.c_str(), keyTypes[3].c_str(), keys[3], key2MGE, 2, overlimit);
                MGEKeys.push_back(key3MGE);
            }
        }
    }
    if (core->getName() == "ApFloatFloat2Float") {
        for (int i = 0; i < len; i ++) {
            if(i == 0) {
                MGEKeys.push_back(selectMinGEValue(nameDb.c_str(), keyTypes[i].c_str(), keys[i], overlimit));
            }
            if(i == 1) {
                int value0 = MGEKeys[0];
                MGEKeys.push_back(selectMinGEValueByFixOneKey(nameDb.c_str(), keyTypes[i].c_str(), keys[i], value0, 0, overlimit));
            }
            if(i == 2) {
                int value0 = MGEKeys[0];
                int value1 = MGEKeys[1];
                MGEKeys.push_back(selectMinGEValueByFixDoubleKeys(nameDb.c_str(), keyTypes[i].c_str(), keys[i], value0, 0, value1, 1, overlimit));
            }
            if(i == 3) {
                int value0 = MGEKeys[0];
                int value1 = MGEKeys[1];
                int value2 = MGEKeys[2];
                MGEKeys.push_back(selectMinGEValueByFixTripleKeys(nameDb.c_str(), keyTypes[i].c_str(), keys[i], value0, 0, value1, 1, value2, 2, overlimit));
            }
        }
    }

    return MGEKeys;
}

DSP58Querier::DSP58Querier() : MaxLatency(4) {
    std::string cmd = "SELECT core_name, DELAY1 FROM ";
    cmd += GetTargetPlatform()->getFactory().getLibraryName();
    cmd += "_DSP58";

    auto& select = Selector::getSelector();
    std::string dsp58Table = GetTargetPlatform()->getFactory().getLibraryName() + "_DSP58";
    if (Selector::getSelector().isExistTable(dsp58Table.c_str())) {
        mDelayData = select.selectStr2DoubleMap(cmd.c_str());
        assert(!mDelayData.empty());
    }
    // ports : a b c d p
    mPortList = {27, 24, 58, 27, 58};
}

int DSP58Querier::queryAmaLatency(DSPOpcode dspOp, int userLat, double clkBudget) {
    // column name: ALL PRE_ADD MUL POST_ADD
    std::vector<std::vector<int>> amaLatencyMat = {
        {0, 0, 0, 0},  // latency 0
        {1, 0, 1, 0},  // latency 1
        {2, 0, 2, 0},  // latency 2
        {3, 0, 2, 1},  // latency 3
        {4, 1, 1, 2}   // latency 4
    };
    int opIndex = static_cast<int>(dspOp);
    int opLatency = 0;
    if (userLat >= 0) {
        opLatency = amaLatencyMat[userLat][opIndex];
    } else {
        for (int tryLat=0; tryLat <= MaxLatency; ++ tryLat) {
            opLatency = amaLatencyMat[tryLat][opIndex];
            auto delayList= queryAmaDelayList(tryLat, ALL);
            if (*std::max_element(delayList.begin(), delayList.end()) <= clkBudget) {
                break;
            }
        }
    }
    
    return opLatency;
}

int DSP58Querier::queryAmLatency(DSPOpcode dspOp, int userLat, double clkBudget) {
    // column name: ALL PRE_ADD MUL POST_ADD
    std::vector<std::vector<int>> amLatencyMat = {
        {0, 0, 0, 0},  // latency 0
        {1, 0, 1, 0},  // latency 1
        {2, 0, 2, 0},  // latency 2
        {3, 0, 3, 0},  // latency 3
        {4, 1, 3, 0}   // latency 4
    };
    int opIndex = static_cast<int>(dspOp);
    int opLatency = 0;
    if (userLat >= 0) {
        opLatency = amLatencyMat[userLat][opIndex];
    } else {
        for (int tryLat=0; tryLat <= MaxLatency; ++ tryLat) {
            opLatency = amLatencyMat[tryLat][opIndex];
            auto delayList= queryAmDelayList(tryLat, ALL);
            if (*std::max_element(delayList.begin(), delayList.end()) <= clkBudget) {
                break;
            }
        }
    }
    
    return opLatency;
}

int DSP58Querier::queryMaLatency(DSPOpcode dspOp, int userLat, double clkBudget) {
    // column name: ALL PRE_ADD MUL POST_ADD
    std::vector<std::vector<int>> maLatencyMat = {
        {0, 0, 0, 0},  // latency 0
        {1, 0, 1, 0},  // latency 1
        {2, 0, 2, 0},  // latency 2
        {3, 0, 2, 1},  // latency 3
        {4, 0, 2, 2}   // latency 4
    };
    int opIndex = static_cast<int>(dspOp);
    int opLatency = 0;

    if (userLat >= 0) {
        opLatency = maLatencyMat[userLat][opIndex];
    } else {
        for (int tryLat=0; tryLat <= MaxLatency; ++ tryLat) {
            opLatency = maLatencyMat[tryLat][opIndex];
            auto delayList= queryMaDelayList(tryLat, ALL);
            if (*std::max_element(delayList.begin(), delayList.end()) <= clkBudget) {
                break;
            }
        }
    }

    return opLatency;
}

std::vector<double> DSP58Querier::to3Delay(const std::vector<double>& delay) {
    assert(!delay.empty());
    std::vector<double> res;
    double mDelay = *std::max_element(delay.begin(), delay.end());
    res = {delay[0], mDelay, delay.back()};
    return res;
}

std::vector<double> DSP58Querier::queryAmaDelayList(int latency, DSPOpcode dspOp) {
    std::vector<double> preAddDelay;
    std::vector<double> mulDelay;
    std::vector<double> postAddDelay;
    switch (latency) {
        case 0:
            preAddDelay = {std::max(mDelayData.at("a2adreg_Adder") -  mDelayData.at("adregSetup"), 
                            mDelayData.at("d2adreg_Adder") - mDelayData.at("adregSetup") )};
            mulDelay = {mDelayData.at("b2mreg_Mul") - mDelayData.at("bregSetup")};
            postAddDelay = {mDelayData.at("c2p_ALU")};
        break;

        case 1:
            preAddDelay = {std::max(mDelayData.at("a2adreg_Adder") - mDelayData.at("adregSetup"), 
                                mDelayData.at("d2adreg_Adder") - mDelayData.at("adregSetup"))};
            mulDelay = {mDelayData.at("b2mreg_Mul"), 
                        mDelayData.at("mregC2Q")};
            postAddDelay = {mDelayData.at("c2p_ALU")};
        break;

        case 2:
            preAddDelay = {std::max(mDelayData.at("a2adreg_Adder"), mDelayData.at("d2adreg_Adder"))};
            mulDelay = {mDelayData.at("b2breg_Through"), 
                        std::max({mDelayData.at("b2breg_Through"), mDelayData.at("breg2mreg_Mul"),
                                    mDelayData.at("adreg2mreg_Mul"), mDelayData.at("mregC2Q")}), 
                        mDelayData.at("mregC2Q")};
            postAddDelay = {mDelayData.at("c2p_ALU")};
        break;

        case 3:
            preAddDelay = {std::max(mDelayData.at("a2adreg_Adder"), mDelayData.at("d2adreg_Adder"))};
            mulDelay = {mDelayData.at("b2breg_Through"), 
                        std::max({mDelayData.at("b2breg_Through"), mDelayData.at("breg2mreg_Mul"),
                                    mDelayData.at("adreg2mreg_Mul"), mDelayData.at("mregC2Q")}), 
                        mDelayData.at("mregC2Q")};
            postAddDelay = {mDelayData.at("c2preg_ALU") , 
                            mDelayData.at("preg2p_Through")};
        break;

        case 4:
            preAddDelay = {std::max(mDelayData.at("d2dreg_Through"), mDelayData.at("a2areg_Through")), 
                           std::max(mDelayData.at("areg2adreg_Adder"), mDelayData.at("dreg2adreg_Adder"))};

            mulDelay = {mDelayData.at("b2breg_Through"), 
                        std::max(mDelayData.at("breg2mreg_Mul"), mDelayData.at("adreg2mreg_Mul"))};
            postAddDelay = {mDelayData.at("c2creg_Through"), 
                            std::max({mDelayData.at("c2creg_Through"), mDelayData.at("mreg2preg_ALU"), 
                                            mDelayData.at("creg2preg_ALU"), mDelayData.at("preg2p_Through")}), 
                            mDelayData.at("preg2p_Through")};
        break;
        default:
            assert(0);
        break;
    }
    std::vector<double> delay;
    if (dspOp == MUL) {
        delay = to3Delay(mulDelay);
    } else if (dspOp == PRE_ADD) {
        delay = to3Delay(preAddDelay);
    } else if (dspOp == POST_ADD) {
        delay = to3Delay(postAddDelay);
    } else {
        auto mul3Delay = to3Delay(mulDelay);
        auto preAdd3Delay = to3Delay(preAddDelay);
        auto postAdd3Delay = to3Delay(postAddDelay);

        delay = {std::max({mul3Delay[0], preAdd3Delay[0], postAdd3Delay[0]}),
                std::max({mul3Delay[1], preAdd3Delay[1], postAdd3Delay[1]}),
                std::max({mul3Delay[2], preAdd3Delay[2], postAdd3Delay[2]})};
    }
    return delay;
}

std::vector<double> DSP58Querier::queryAmDelayList(int latency, DSPOpcode dspOp) {
    std::vector<double> preAddDelay;
    std::vector<double> mulDelay;
    switch (latency) {
        case 0:
            preAddDelay = {std::max(mDelayData.at("a2adreg_Adder") - mDelayData.at("adregSetup"), 
                                        mDelayData.at("d2adreg_Adder") - mDelayData.at("adregSetup"))};
            mulDelay = {mDelayData.at("b2p_Mul")};
        break;

        case 1:
            preAddDelay = {std::max(mDelayData.at("a2adreg_Adder") - mDelayData.at("adregSetup"), 
                                    mDelayData.at("d2adreg_Adder") - mDelayData.at("adregSetup"))};
            mulDelay = {mDelayData.at("b2mreg_Mul"), mDelayData.at("mreg2p_Through")};
        break;

        case 2:
            preAddDelay = {std::max(mDelayData.at("a2adreg_Adder"), mDelayData.at("d2adreg_Adder"))};
            mulDelay = {mDelayData.at("b2breg_Through"), 
                        std::max(mDelayData.at("breg2mreg_Mul"), mDelayData.at("adreg2mreg_Mul")), 
                        mDelayData.at("mreg2p_Through")};
        break;

        case 3:
            preAddDelay = {std::max(mDelayData.at("a2adreg_Adder"), mDelayData.at("d2adreg_Adder"))};
            mulDelay = {mDelayData.at("b2breg_Through"), 
                std::max({mDelayData.at("breg2mreg_Mul"), mDelayData.at("adreg2mreg_Mul"), mDelayData.at("mreg2preg_Through")}), 
                std::max({mDelayData.at("breg2mreg_Mul"), mDelayData.at("adreg2mreg_Mul"), mDelayData.at("mreg2preg_Through")}), 
                mDelayData.at("preg2p_Through")};
    
        break;

        case 4:
            preAddDelay = {std::max(mDelayData.at("a2areg_Through"), mDelayData.at("d2dreg_Through")),
                           std::max(mDelayData.at("areg2adreg_Adder"), mDelayData.at("dreg2adreg_Adder"))};
            mulDelay = {mDelayData.at("b2breg_Through"), 
                std::max({mDelayData.at("breg2mreg_Mul"), mDelayData.at("adreg2mreg_Mul"), mDelayData.at("mreg2preg_Through")}), 
                std::max({mDelayData.at("breg2mreg_Mul"), mDelayData.at("adreg2mreg_Mul"), mDelayData.at("mreg2preg_Through")}), 
                mDelayData.at("preg2p_Through")};
    
        break;
        default:
            assert(0);
    }

    std::vector<double> delay;
    if (dspOp == MUL) {
        delay = to3Delay(mulDelay);
    } else if (dspOp == PRE_ADD) {
        delay = to3Delay(preAddDelay);
    } else {
        auto mul3Delay = to3Delay(mulDelay);
        auto preAdd3Delay = to3Delay(preAddDelay);
        delay = {std::max(mul3Delay[0], preAdd3Delay[0]),
                std::max(mul3Delay[1], preAdd3Delay[1]),
                std::max(mul3Delay[2], preAdd3Delay[2])};
    }
    return delay;
}

std::vector<double> DSP58Querier::queryMaDelayList(int latency, DSPOpcode dspOp) {
    std::vector<double> mulDelay;
    std::vector<double> postAddDelay;

    switch (latency) {
        case 0:
            mulDelay = {std::max(mDelayData.at("b2mreg_Mul") - mDelayData.at("mregSetup"), 
                                 mDelayData.at("a2mreg_Mul") - mDelayData.at("mregSetup"))};
            postAddDelay = {mDelayData.at("c2p_ALU")};
        break;

        case 1:
            mulDelay = {std::max(mDelayData.at("a2mreg_Mul"), mDelayData.at("b2mreg_Mul")), 
                        mDelayData.at("mregC2Q")};
            postAddDelay = {mDelayData.at("c2p_ALU")};
        break;

        case 2:
            mulDelay = {std::max(mDelayData.at("b2breg_Through"), mDelayData.at("a2areg_Through")), 
                    std::max(mDelayData.at("areg2mreg_Mul"), mDelayData.at("breg2mreg_Mul")), 
                    mDelayData.at("mregC2Q")};
            postAddDelay = {mDelayData.at("c2p_ALU")};
        break;

        case 3:
            mulDelay = {std::max(mDelayData.at("b2breg_Through"), mDelayData.at("a2areg_Through")), 
                std::max(mDelayData.at("areg2mreg_Mul"), mDelayData.at("breg2mreg_Mul")), 
                mDelayData.at("mregC2Q")};
            postAddDelay = {mDelayData.at("c2preg_ALU"), mDelayData.at("preg2p_Through")};
        break;

        case 4:
            mulDelay = {std::max(mDelayData.at("b2breg_Through"), mDelayData.at("a2areg_Through")), 
                std::max(mDelayData.at("areg2mreg_Mul"), mDelayData.at("breg2mreg_Mul")), 
                mDelayData.at("mregC2Q")};
            postAddDelay = {mDelayData.at("c2creg_Through"), 
                std::max(mDelayData.at("mreg2preg_ALU"), mDelayData.at("creg2preg_ALU")), 
                mDelayData.at("preg2p_Through")};
        break;
        default:
            assert(0);
    }

    std::vector<double> delay;
    if (dspOp == MUL) {
        delay = to3Delay(mulDelay);
    } else if (dspOp == POST_ADD) {
        delay = to3Delay(postAddDelay);
    } else {
        auto mul3Delay = to3Delay(mulDelay);
        auto postAdd3Delay = to3Delay(postAddDelay);
        delay = {std::max(mul3Delay[0], postAdd3Delay[0]),
                std::max(mul3Delay[1], postAdd3Delay[1]),
                std::max(mul3Delay[2], postAdd3Delay[2])};
    }
    return delay;
}

int DSP58Querier::queryMaxLatency(CoreInst* core) {  
    auto ip = static_cast<IPBlockInst*>(core);
    DSPOpcode dspOp = getDspOpcode(ip);
    auto param = metaDataToMap(ip->getMetadata());
    assert(dspOp != UNSUPPORTED);
    // FIXME: Is the "select" should be used in dsp?
    if (dspOp == SELECT) {
        return 0;
    }
    double clkBudget = core->getDelayBudget();
    if (param.size() == 0) {
        // assert(false && "param is invalid");
        return MaxLatency;
    }
    int latency = 0;
    DSPType dsp48Type = getDSPType(param);

    switch (dsp48Type) {
        // ama
        case AMA:
            latency = queryAmaLatency(dspOp, MaxLatency, clkBudget);
        break;
        // ma
        case MA:
            latency = queryMaLatency(dspOp, MaxLatency, clkBudget);
        break;
        // am
        case AM:
            latency = queryAmLatency(dspOp, MaxLatency, clkBudget);
        break;

        default:
            assert(false);
    }

    return latency;
}

int DSP58Querier::queryLatency(CoreInst *core) {
    auto ip = static_cast<IPBlockInst*>(core);

    DSPOpcode dspOp = getDspOpcode(ip);
    assert(dspOp != UNSUPPORTED);
    // FIXME: Is the "select" should be used in dsp?
    if (dspOp == SELECT) {
        return 0;
    }
    double clkBudget = ip->getDelayBudget();
    int userLat = ip->getConfigedLatency();

    auto param = metaDataToMap(ip->getMetadata());
    if (param.size() == 0) {
        // assert(false && "param is invalid");
        return MaxLatency;
    }

    int latency = 0;
    DSPType dspType = getDSPType(param);

    switch (dspType) {
        // ama
        case AMA:
            latency = queryAmaLatency(dspOp, userLat, clkBudget);
        break;
        // ma
        case MA:
            latency = queryMaLatency(dspOp, userLat, clkBudget);
        break;
        // am
        case AM:
            latency = queryAmLatency(dspOp, userLat, clkBudget);
        break;

        default:
            assert(false);
    }
    return latency;
}

std::vector<double> DSP58Querier::queryDelayList(CoreInst* core) {
    auto ip = static_cast<IPBlockInst*>(core);

    DSPOpcode dspOp = getDspOpcode(ip);
    assert(dspOp != UNSUPPORTED);
    // FIXME: Is the "select" should be used in dsp?
    if (dspOp == SELECT) {
        return {0.0, 0.0, 0.0};
    }
    double clkBudget = ip->getDelayBudget();
    int userLat = ip->getConfigedLatency();

    auto param = metaDataToMap(ip->getMetadata());
    std::vector<double> delay;
    if (param.size() == 0) {
        // assert(false && "param is invalid");
        auto amaDelay = queryAmaDelayList(MaxLatency, ALL);
        delay = amaDelay;
        auto maDelay = queryMaDelayList(MaxLatency, ALL);
        if (delay[1] > maDelay[1]) {
            delay = maDelay;
        }
        auto amDelay = queryAmDelayList(MaxLatency, ALL);
        if (delay[1] > amDelay[1]) {
            delay = amDelay;
        }
        return delay;
    }
    DSPType dspType = getDSPType(param);

    int allLatency = 0;
    switch (dspType) {
        // ama
        case AMA:
            allLatency = queryAmaLatency(ALL, userLat, clkBudget);
            delay = queryAmaDelayList(allLatency, dspOp);
        break;
        // ma
        case MA:
            allLatency = queryMaLatency(ALL, userLat, clkBudget);
            delay = queryMaDelayList(allLatency, dspOp);
        break;
        // am
        case AM:
            allLatency = queryAmLatency(ALL, userLat, clkBudget);
            delay = queryAmDelayList(allLatency, dspOp);
        break;

        default:
            assert(false);
    }
    return delay;
}

ResourceData DSP58Querier::queryResource(CoreInst* core) {
    ResourceData resource;
    resource.Dsp = 1;
    return resource;
}

DSP58Querier::DSPOpcode DSP58Querier::getDspOpcode(const CoreInst* dsp) {
    auto ip = static_cast<const IPBlockInst*>(dsp);
    DSPOpcode dspOp = UNSUPPORTED;
    switch (mCurOper) {
        case AnyOperation:
            dspOp = ALL;
        break;
        case PlatformBasic::OP_MUL:
            dspOp = MUL;
        break;
        case PlatformBasic::OP_ADD:
        case PlatformBasic::OP_SUB:
            if (isPostAdder(ip->getOutputBW())) {
                dspOp = POST_ADD;
            } else {
                dspOp = PRE_ADD;
            }
        break;

        case PlatformBasic::OP_SELECT:
            dspOp = SELECT;
        break;

        default:
            dspOp = UNSUPPORTED;
    }
    return dspOp;

}

std::map<std::string, std::string> DSP58Querier::metaDataToMap(const std::string& str) {
    std::map<std::string, std::string> res;
    std::vector<std::string> strVer;
    int strLen = str.length();
    int left = 0;
    while (left < strLen) {
        while (str[left] == ' ' || str[left] == '{' || str[left] == '}') {
            left ++;
        }
        int right = left;
        while (right < strLen && str[right] != ' ' 
            && str[right] != '{' && str[right] != '}') {
            right ++;
        }
        strVer.push_back(str.substr(left, right-left));
        left = right;
    }
    int size = strVer.size();
    if (size % 2 > 0) {
        size -= 1;
    }
    for (int i=0; i < size; i += 2) {
        res[strVer[i]] = strVer[i+1];
    }
    return res;
}

//#enum { Invalid = -1, Signed = 1, Unsigned = 2, Both = 3 } mSign;
int DSP58Querier::can_be_signed(int x) {
    if (x == 1 || x == 3) {
        return 1;
    }
    return 0;
}

int DSP58Querier::can_be_unsigned(int x) {
    if (x == 2 || x == 3) {
        return 1;
    }
    return 0;
}

bool DSP58Querier::isIncludeAllKeys(const std::map<std::string, std::string>& param, const std::vector<std::string>& keys) {
    for (const auto& key: keys) {
        if(param.count(key) == 0) {
            return false;
        }
    }
    return true;
}

bool DSP58Querier::isPostAdder(int outputBW) {
    // If the adder is post adder outputBW will be set to 48 by BE.
    if (outputBW > 40) {
        return true;
    }
    
    return false;
}

// dsp type 
//          c_sign      d_sign
// ama      y           y
// ma       y           n 
// am       n           y
DSP58Querier::DSPType DSP58Querier::getDSPType(const std::map<std::string, std::string>& opt) {
    DSPType type = AMA;
    if (opt.count("c_sign") > 0 && opt.count("d_sign") > 0) {
        type = AMA;
    } else if (opt.count("c_sign") > 0 && opt.count("d_sign") == 0) {
        type = MA;
    } else if (opt.count("c_sign") == 0 && opt.count("d_sign") > 0) {
        type = AM;
    } else {
        assert(0);
    }
    return type;
}

// # check if post-adder can be absorbed
int DSP58Querier::mac_chk (int a_port, int b_port, int c_port, int o_port, int a_eff,
            int a_sign, int b_eff, int b_sign, int ab_bw, int ab_sign, int c_eff, int o_eff) {

    if (can_be_signed(a_sign) && can_be_signed(b_sign) && can_be_signed(ab_sign)) {
        // # signed multiplier
    } else if (can_be_unsigned(a_sign) && can_be_unsigned(b_sign) && can_be_unsigned(ab_sign)) {
        // # need additional bit for unsigned multiplier
        a_port -= 1;
        b_port -= 1;
    } else {
        // # mixture of signed and unsigned...
        // # FIXME: handle as if things are all signed. and ab has to be signed
        if (can_be_signed(ab_sign) == 0) {
            return 0;
        }
        // # whether increase a_eff or decrease a_port?
        if (can_be_signed(a_sign) == 0) {
            a_eff += 1;
            ab_bw += 1;
        }
        if (can_be_signed(b_sign) == 0) {
            b_eff += 1;
            ab_bw += 1;
        }
    }

    int req = o_eff;
    int ab_eff = a_eff + b_eff;

    if (a_eff > a_port || b_eff > b_port || req > o_port) {
        return 0;           // # cannot fit into one dsp
    }
    // # In theory a*b output bitwidth should be no less than sum(a_eff+b_eff)
    if (ab_bw < ab_eff) {
        if (ab_bw < req) {
            return 0;
        }
    }
    return 1;
}
// ######
// # metadata string should follow the format:
// #    "<keyword0> <value0> <keyword1> <value1> ..... "
// # allowed keywords:
// #    a : A input bitwidth
// #    b : B input bitwidth
// #    c : C input bitwidth
// #    ab : bitwidth (after possible truncation but not sext) of mul result
// #    cab : bitwidth (after possible truncation but not sext) of add/sub result
// #    output : bitwidth of output (including sext after add/sub)
int DSP58Querier::xil_gen_ma_chk (int a_port, int b_port, int c_port, int o_port, const std::map<std::string, std::string>& param) {
    static const std::vector<std::string> needKeys = {"a", "a_sign", 
        "b", "b_sign", "c", "ab", "ab_sign"
    };
    if (!isIncludeAllKeys(param, needKeys)) {
        return 0;
    }

    int a_eff = std::stoi(param.at("a"));
    int a_sign = std::stoi(param.at("a_sign"));
    int b_eff = std::stoi(param.at("b"));
    int b_sign = std::stoi(param.at("b_sign"));
    int c_eff = std::stoi(param.at("c"));
    int ab_bw = std::stoi(param.at("ab"));
    int ab_sign = std::stoi(param.at("ab_sign"));
    
    int cab_bw = 0;
    if (param.count("cab") > 0) {
        // # match c-a*b
        cab_bw = std::stoi(param.at("cab"));
    } else if (param.count("abc") > 0){
        // # match a*b-c
        cab_bw = std::stoi(param.at("abc"));
    } else {
        return 0;
    }
    int output_bw = cab_bw;
    if (param.count("output")) {
        output_bw = std::stoi(param.at("output"));
    }

    return mac_chk(a_port, b_port, c_port, o_port, a_eff, a_sign, b_eff, b_sign, ab_bw, ab_sign, c_eff, output_bw);

}

// ########### AMA ###########

// ######
// # metadata string should follow the format:
// #    "<keyword0> <value0> <keyword1> <value1> ..... "
// # allowed keywords:
// #    a : A input bitwidth
// #    b : B input bitwidth
// #    c : C input bitwidth
// #    d : D input bitwidth
// #    ab : bitwidth (after possible truncation but not sext) of mul result
// #    cab : bitwidth (after possible truncation but not sext) of add/sub result
// #    output : bitwidth of output (including sext after add/sub)
int DSP58Querier::xil_gen_ama_chk(int a_port, int b_port, int c_port, int d_port, int o_port, const std::map<std::string, std::string>& param) {
    // # a special check for two MACC pack
    if (param.count("RND") > 0) {
        if (!isIncludeAllKeys(param, {"a", "b", "d", "c"})) {
            return 0;
        }
        int a_eff = std::stoi(param.at("a"));
        int b_eff = std::stoi(param.at("b"));
        int d_eff = std::stoi(param.at("d"));
        int c_eff = std::stoi(param.at("c"));

        if (a_eff == 26 && b_eff == 8 && c_eff == 48 && d_eff == 8) {
            return 1;
        }
        return 0;
    }

    if (param.count("dada_sign") > 0) {
        if (!isIncludeAllKeys(param, {"a", "a_sign", "c", "d", "d_sign", "da", "da_sign", "dada", "dada_sign"})) {
            return 0;
        }
        int a_eff = std::stoi(param.at("a"));
        int c_eff = std::stoi(param.at("c"));
        int d_eff = std::stoi(param.at("d"));
        int da_bw = std::stoi(param.at("da"));
        int da_sign = std::stoi(param.at("da_sign"));
        int dada_bw = std::stoi(param.at("dada"));
        int dada_sign = std::stoi(param.at("dada_sign"));

        int cdada_bw = 0;
        if (param.count("cdada") > 0) {
            // # match c-(d+a)*(d+a)
            cdada_bw = std::stoi(param.at("cdada"));
        } else if (param.count("dadac") > 0){
            // # match (d+a)*(d+a)-c
            cdada_bw = std::stoi(param.at("dadac"));
        } else {
            return 0;
        }
        
        int output_bw = cdada_bw;
        if (param.count("output")) {
            output_bw = std::stoi(param.at("output"));
        }
        
        // # effectively, A/B can only be 24 bits, at least for xst
        int da_eff = std::max(a_eff, d_eff) + 1; // 
        // # square for b
        int b_eff = da_eff;
        int b_sign = da_sign;
        int dab_bw = dada_bw;
        int dab_sign = dada_sign;

        if (da_bw > b_port) {
            return 0;
        }
        return mac_chk(a_port, b_port, c_port, o_port, da_bw, da_sign, b_eff, b_sign, dab_bw, dab_sign, c_eff, output_bw);
    } else {
        if (!isIncludeAllKeys(param, {"a", "a_sign", "b", "b_sign", "c", 
                "d", "d_sign", "da", "da_sign", "dab", "dab_sign"})) {
            return 0;
        }
        int b_eff = std::stoi(param.at("b"));
        int b_sign = std::stoi(param.at("b_sign"));
        int c_eff = std::stoi(param.at("c"));
        int da_bw = std::stoi(param.at("da"));
        int da_sign = std::stoi(param.at("da_sign"));
        int dab_bw = std::stoi(param.at("dab"));
        int dab_sign = std::stoi(param.at("dab_sign"));

        int cdab_bw = 0;
        if (param.count("cdab") > 0) {
            // # match c-(d+a)*b
            cdab_bw = std::stoi(param.at("cdab"));
        } else {
            // # match (d+a)*b-c
            cdab_bw = std::stoi(param.at("dabc"));
        }
        int output_bw = cdab_bw;
        if (param.count("output")) {
            output_bw = std::stoi(param.at("output"));
        }

        if (da_bw > a_port) {
            return 0;
        }
        return mac_chk(a_port, b_port, c_port, o_port, da_bw, da_sign, b_eff, b_sign, dab_bw, dab_sign, c_eff, output_bw);
    }
}


// #### Mult preadder
int DSP58Querier::xil_gen_am_chk (int a_port, int b_port, int d_port, int o_port, const std::map<std::string, std::string>& param) {    
    if (param.count("dada_sign") > 0) {
        // (a + d) * (a + d) 
        if(!isIncludeAllKeys(param, {"a", "a_sign", "d", "d_sign", "da", "da_sign", "dada", "dada_sign"})) {
            return 0;
        }
        int a_eff = std::stoi(param.at("a"));
        int d_eff = std::stoi(param.at("d"));
        int dada_bw = std::stoi(param.at("dada"));
        int output_bw = dada_bw;
        if (param.count("output") > 0) {
            output_bw = std::stoi(param.at("output"));
        }

        int da_eff = std::max(a_eff, d_eff) + 1;  

        // Check the bw upper bound to fit into exactly 1 dsp
        if (da_eff > a_port || da_eff > b_port) {
            // da_eff > a_port: The pre-adder result bw > input bw to multiplier (the one from pre-adder result, the other input is from b_port).
            //                  Here a_port bw == b_port bw == input to mul from pre-adder result
            // da_eff > b_port: The pre-adder result bw > b_port bw (This is specific for dada pattern)
            return 0;                      // # cannot fit into one dsp
        }

        // Check the lower bound
        if (da_eff < 5) {
            return 0;
        }
    } else {
        if(!isIncludeAllKeys(param, {"a", "a_sign", "b", "b_sign", 
            "d", "d_sign", "da", "da_sign", "dab", "dab_sign"})) {
            return 0;
        }
        int a_eff = std::stoi(param.at("a"));
        int a_sign = std::stoi(param.at("a_sign"));
        int b_eff = std::stoi(param.at("b"));
        int b_sign = std::stoi(param.at("b_sign"));
        int d_eff = std::stoi(param.at("d"));
        int dab_bw = std::stoi(param.at("dab"));
        int output_bw = dab_bw;
        if (param.count("output") > 0) {
            output_bw = std::stoi(param.at("output"));
        } 
        // # CR 953029. We should check if a b is signed.
        if (can_be_unsigned(a_sign) && can_be_unsigned(b_sign)) {
            a_port -= 1;
            b_port -= 1;
        }

        int da_eff = std::max(a_eff, d_eff) + 1; 

        // Previous check (da_bw < da_eff && da_bw != a_port) conflict with FE's current behavior, for example:
        //     In the pattern r = (a + d) * b, where a, d, b, r has the same bitwidth n, the result of (a + d) has the same bitwidth n.
        // It has been proved that for a multiplication z = x * y, truncate x or y is equivalent to truncate the output z. 
        // Read the following page for more info: https://confluence.xilinx.com/display/~jinz/DSP+AM+model+mapping+problem
        // if (da_bw < da_eff && da_bw != a_port) {
        //     return 0;
        // }
        
        // Check the bw upper bound to fit into exactly 1 dsp
        if (da_eff > a_port || b_eff > b_port) {
            // da_eff > a_port: The pre-adder result bw > input bw to multiplier (the one from pre-adder result, the other input is from b_port).
            //                  Here a_port bw == b_port bw == input to mul from pre-adder result
            // da_eff > b_port: The b port data bw > b_port bw (This is specific for dada pattern)
            return 0;                       // # cannot fit into one dsp
        }

        // Check the bw lower bound
        if (da_eff < 5 || b_eff < 5) {
            return 0;
        }
    }
    return 1;
}

// # DSP48 legality check
bool DSP58Querier::getLegality(CoreInst* core) {

    auto ip = static_cast<IPBlockInst*>(core);
    auto param = metaDataToMap(ip->getMetadata());

    DSPType dspType = getDSPType(param);
    
    switch (dspType)
    {
        case AMA:
            return  xil_gen_ama_chk(mPortList[A_PORT], 
                                mPortList[B_PORT], mPortList[C_PORT], 
                                mPortList[D_PORT], mPortList[P_PORT], param);
            break;
        case MA:
            return xil_gen_ma_chk(mPortList[A_PORT], mPortList[B_PORT], 
                                mPortList[C_PORT], mPortList[P_PORT], param);
            break;
        case AM: 
            return xil_gen_am_chk(mPortList[A_PORT], 
                            mPortList[B_PORT], mPortList[D_PORT], 
                            mPortList[P_PORT], param);
            break;
        
        default:
            return false;
    }
    return true;
}

}   // platform namespace
