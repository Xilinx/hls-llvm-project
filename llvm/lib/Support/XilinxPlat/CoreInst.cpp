

#include <cassert>
#include <iomanip>
#include <iterator>
#include <fstream>
#include <algorithm>
#include <cctype>
#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XilinxPlat/CoreQuerier.h"
#include "llvm/Support/XilinxPlat/TargetPlatform.h"
#include "llvm/Support/XilinxPlat/ChipInfo.h"
#include "llvm/Support/XilinxPlat/CoreInst.h"
#include "llvm/Support/XilinxPlat/PlatformSettings.h"
#else
#include "CoreQuerier.h"
#include "TargetPlatform.h"
#include "ChipInfo.h"
#include "CoreInst.h"
#include "PlatformSettings.h"
#endif

using std::string;
using std::map;
using std::vector;
using std::set;

namespace platform
{
class ChipInfo;

double CoreCost::getDelay(OperType oper) {
    return mCoreCostDelayList[1];
}

double CoreCost::getFirstStageDelay(OperType oper) {
    return mCoreCostDelayList[0];
}

double CoreCost::getLastStageDelay(OperType oper) {
    return mCoreCostDelayList[2];
}

unsigned CoreCost::getPipeLatency (OperType oper) {
    return mCoreCostPipeLatency;
}

const ResUsageMap& CoreCost::getResourceUsage() {
    return mCoreCostResUsage;
}

double CoreCost::getResourceUsageByName(std::string name) {
    if (mCoreCostResUsage.count(name)) {
        return mCoreCostResUsage[name];
    } else {
        return 0.0;
    }
}

void CoreCost::setResUsage(const ResUsageMap& resUsage) {
    mCoreCostResUsage = resUsage;
}

void CoreCost::setDelayList(const std::vector<double>& delayList) {
    mCoreCostDelayList = delayList;
}

void CoreCost::setPipeLatency(const int latency) {
    mCoreCostPipeLatency = latency;
}

std::vector<double> QueryCoreCost::getCoreCostDelayList(CoreInst *core, OperType oper) {
    if (core->getName() == "Divider_IP") 
    {
        FuncUnitInst* fu = static_cast<FuncUnitInst*>(core);        
        std::vector<unsigned> inputOperands = fu->getInputBWList();
        for (unsigned operands : inputOperands) {
            if (operands > 64) {
                std::vector<double> artificialDelayList = { 1000000.0, 1000000.0, 1000000.0 };
                return artificialDelayList; 
           }
        }
    }
    if (core->getName() == "Divider") 
    {
        FuncUnitInst* fu = static_cast<FuncUnitInst*>(core);        
        std::vector<unsigned> inputOperands = fu->getInputBWList();
        for (unsigned operands : inputOperands) {
            if (operands >= 1000 ) {
                std::vector<double> artificialDelayList = { 1000000.0, 1000000.0, 1000000.0 };
                return artificialDelayList; 
           }
        }
    }

    if (!core->mCore->hasDelayFunction()) {
        return core->mCore->getFixedDelayList(oper);
    }

    auto delay = QuerierFactory::queryDelayList(core, oper);
    return delay;
}
unsigned QueryCoreCost::getCoreCostPipeLatency(CoreInst *core, OperType oper) {
    if (core->getName() == "Divider_IP") 
    {
        FuncUnitInst* fu = static_cast<FuncUnitInst*>(core);
        std::vector<unsigned> inputOperands = fu->getInputBWList();
        for (unsigned operands : inputOperands) {
            if (operands > 64) {
                return 1000000;
            }
        }
    }
    if (core->getName() == "Divider") 
    {
        FuncUnitInst* fu = static_cast<FuncUnitInst*>(core);
        std::vector<unsigned> inputOperands = fu->getInputBWList();
        for (unsigned operands : inputOperands) {
            if (operands >= 1000) {
                return 1000000;
            }
        }
    }
    //configAttributes();
    if (!core->mCore->hasLatencyFunction()) {
        return  core->mCore->getFixedPipeLatency();
    }

    unsigned latency;
    // TODO: 
    int userLat = core->getConfigedLatency();
    // DSP58 is an exception since it has a special post-processing in queryLatency()
    if(userLat >= 0 && core->getName() != "DSP58_DotProduct" && 
            core->getName() != "DSP48" && core->getName() != "DSP_Double_Pump_Mac16" && 
            core->getName() != "DSP_Double_Pump_Mac8" && core->getName() != "QAddSub_DSP")
    {
        // This if-statement is a patch for userLat > max(latencies available in db), which means incomplete database.
        // In this circumstance, getPipeLatency() should return userLat;
        // but queryLatency() returns max(latencies available in db).
        latency = userLat;
    }
    else
    {
        latency = QuerierFactory::queryLatency(core, oper);
    }
    return latency;
}
ResUsageMap QueryCoreCost::getCoreCostResourceUsage(CoreInst *core) {
    if(!core->mCore->hasResUsageFunction()) {
        return core->mCore->getInitResourceUsage();
    }
    auto resource = QuerierFactory::queryResource(core);
    ResUsageMap resMap;
    if (core->mCoreCostResUsage.count("LUT")) {
        resMap["LUT"] = resource.Lut;
    }
    if (core->mCoreCostResUsage.count("FF")) {
        resMap["FF"] = resource.Ff;
    }
    if (core->mCoreCostResUsage.count("DSP")) {
        resMap["DSP"] = resource.Dsp;
    }
    if (core->mCoreCostResUsage.count("BRAM")) {
        resMap["BRAM"] = resource.Bram;
    }
    if (core->mCoreCostResUsage.count("URAM")) {
        resMap["URAM"] = resource.Uram;
    }
    return resMap;
}


// CoreInst
CoreInst::CoreInst(pf_internal::Core* core, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl)
    : mCore(core),
    mOp(op),
    mImpl(impl),
    // attributes
    mClock(-1),
    mPreferredLatency(-1)
    
{
    mCoreCostDelayList = core->getFixedDelayList(op);
    mCoreCostPipeLatency = core->getFixedPipeLatency();
    mCoreCostResUsage = core->getInitResourceUsage();
}

CoreInst::~CoreInst()
{
}

unsigned CoreInst::getId() const
{
    return mCore->getId();
}

std::string CoreInst::getName() const
{
    return mCore->getName();
}

bool CoreInst::isEquivalent(std::shared_ptr<CoreInst> other) const
{
    if (!other) return false;
    return this->mCore == other->mCore;
}

CoreInst::Type CoreInst::getType() const
{
    auto type = mCore->getType();
    return static_cast<CoreInst::Type>(type);
}

std::string CoreInst::getTypeStr() const
{
    return mCore->getTypeStr();
}

bool CoreInst::isGeneric() const
{
    return mCore->isGeneric();
}

unsigned CoreInst::getMaxLatency(OperType opcode)
{
    if (getName() == "DSP48") {
        if (GetTargetPlatform()->getFactory().isVersal()) {
            auto querier = QuerierFactory::getInstance().getCoreQuerier(this);
            auto dsp58Querier = static_cast<DSP58Querier*>(querier);
            dsp58Querier->setCurOper(opcode);
            unsigned maxLat = dsp58Querier->queryMaxLatency(this);
            return maxLat;
        } else {
            auto querier = QuerierFactory::getInstance().getCoreQuerier(this);
            auto dsp48Querier = static_cast<DSP48Querier*>(querier);
            dsp48Querier->setCurOper(opcode);
            unsigned maxLat = dsp48Querier->queryMaxLatency(this);
            return maxLat;
        }
    }
    auto latencyRange = PlatformBasic::getInstance()->verifyLatency(mOp, mImpl);
    return latencyRange.second;
}

unsigned CoreInst::getMinLatency(OperType opcode) const
{
    auto latencyRange = PlatformBasic::getInstance()->verifyLatency(mOp, mImpl);
    return latencyRange.first;
}

const OperSet& CoreInst::getOpers() const
{
    return mCore->getOpers(); 
}

bool CoreInst::matchOper(OperType op) const
{
    return mCore->matchOper(op);
}

bool CoreInst::isValidLatency(int latency) const
{
    auto latencyRange = PlatformBasic::getInstance()->verifyLatency(mOp, mImpl);
    return latency < 0 || (latencyRange.first <= latency && latencyRange.second >= latency);
}

const InterfaceModes& CoreInst::getInterfaceModes() const
{
    return mCore->getInterfaceModes();
}

bool CoreInst::matchInterfaceMode(InterfaceMode mode) const
{
    return mCore->matchInterfaceMode(mode);
}

std::string CoreInst::getResUsageFunction() const
{
    return mCore->getResUsageFunction();
}

std::string CoreInst::getLatencyFunction() const
{
    return mCore->getLatencyFunction();

}
bool CoreInst::getLegality()
{
    return QuerierFactory::getLegality(this, mOp);
}

std::string CoreInst::getGenerator() const
{
    return mCore->getGenerator();
}

bool CoreInst::hasGenerator() const
{
    // FIXME, special for adder with latency = 0
    auto inst = const_cast<CoreInst*>(this);
    if(getName() == "Adder" && inst->getPipeLatency() == 0)
    {
        return false;
    }
    if(getName() == "Shifter" && inst->getPipeLatency() == 0)
    {
        return false;
    }
    if((getName() == "Cmp" || getName() == "ICmp_EQ") && inst->getPipeLatency() == 0)
    {
        return false;
    }


    return mCore->hasGenerator();
}

const std::map<std::string, std::string>& CoreInst::getPortNameMap() const
{
    return mCore->getPortNameMap();
}

void CoreInst::configImplStyle(std::string implStyle)
{
    mCore->configImplStyle(implStyle);
}

std::string CoreInst::getImplStyle() const
{
    return mCore->getImplStyle();
}

std::string CoreInst::getDescription() const
{
    return mCore->getDescription();
}

std::string CoreInst::getTargetComponent() const
{
    return mCore->getTargetComponent();
}

float CoreInst::getPreference(OperType Pty)
{
    //configAttributes();
    return mCore->getFixedPreference(Pty);
}

unsigned CoreInst::getPipeInterval(OperType Pty)
{
    if(getName() == "DivnS_SEQ") 
    {
        FuncUnitInst* fu = static_cast<FuncUnitInst*>(this);
        return fu->getOutputBW();
    }
    return 1;
}

std::vector<unsigned> CoreInst::getPortStages(OperType Pty)
{
    //configAttributes();
    //return mCore->getPortStages(Pty);
    return {0,0,0,0,0,0,0,0,0};
}

double CoreInst::getDynamicPower(OperType Pty)
{
    return 0.0;
}

double CoreInst::getLeakagePower(OperType Pty)
{
    return 0.0;
}

bool CoreInst::isBlackBoxIPCore() const
{
    return mCore->isBlackBoxIPCore();
}

unsigned CoreInst::string2ResourceTp(std::string res_name) const
{
    return mCore->string2ResourceTp(res_name);
}

#if 0
void CoreInst::print(std::ostream& out) 
{
    out << "CORE: " << mCore->getName() << "\n";
    out << "ATTRIBUTES: " << "\n"
        << "-type: " << CoreInstFactory::coreType2String(mCore->getType()) << "\n";

    // Operations
    out << "-operation: ";
    for (OperType op : mCore->getOpers())
        out << PlatformBasic::getInstance()
            ->getOpName(static_cast<PlatformBasic::OP_TYPE>(opcode)) << " ";
    out << "\n";

    // resources
    out << "-resource: ";
    for (pair<string, double> resUsage : mCore->getInitResourceUsage())
        out << resUsage.first;
    out << "\n";

    switch (getType())
    {
        case pf_internal::Core::Any: break;
        case pf_internal::Core::Generic: break;
        case pf_internal::Core::FunctionUnit: break;
        case pf_internal::Core::Storage:
        {
            StorageInst* st = static_cast<StorageInst*>(this);           
            out << "-ports: ";
            //CPortIter pi = st->getMemPorts().begin();
            CPortList::const_iterator pi = st->getMemPorts().begin();
            for ( ; pi != st->getMemPorts().end(); ++pi)
            {
                const CPort& port = *pi;
                out << MemPort::dirType2String(port.getDirection()) << " ";
            }
            out << "\n";
        }
        case pf_internal::Core::Channel:
        {
            ChannelInst* co = static_cast<ChannelInst*>(this);
            out << "-inputs: " << co->getNumInputs() << "\n";
            out << "-outputs: " << co->getNumOutputs() << "\n";
        }
        case pf_internal::Core::IPBlock: break;
        case pf_internal::Core::Adapter: break;
        default: break;
    }
}

#endif

std::string CoreInst::print(int user_lat) 
{
    int latency = getPipeLatency();
    double delay = getDelay();

    string tmp = "Core " + std::to_string(mCore->getId()) + " '" + mCore->getName() + "'";

    tmp += " <Latency = " + std::to_string(user_lat != -1 ? user_lat : latency) + ">";
//    tmp += " <LatencyBudget = " + std::to_string(getLatencyBudget()) + ">";
    tmp += " <II = " + std::to_string(getPipeInterval()) + ">";
    tmp += " <Delay = " + std::to_string(delay).substr(0, 4) + ">";
//    tmp += " <DelayBudget = " + std::to_string(getDelayBudget()).substr(0, 4) + ">";

    // common info in base class
    tmp += " <" + mCore->getTypeStr() + ">";

    tmp += " <Opcode : ";
    int j = 0;
    for (auto opcode : mCore->getOpers())
    {
        if (!mCore->isValidOper(opcode))
            continue;

        if (j > 0)
            tmp += " ";
        tmp += "'" + PlatformBasic::getInstance()
            ->getOpName(static_cast<PlatformBasic::OP_TYPE>(opcode)) + "'";
        j++;
    }
    tmp += ">";

    switch (mCore->getType())
    {
        case pf_internal::Core::Any: break;
        case pf_internal::Core::Generic: break;
        case pf_internal::Core::FunctionUnit:
        {
            FuncUnitInst* fu = static_cast<FuncUnitInst*>(this);
            tmp += " <InPorts = " + std::to_string(fu->getNumInPorts()) + ">";
            tmp += " <OutPorts = " + std::to_string(fu->getNumOutPorts()) + ">";
            break;
        }
        case pf_internal::Core::Storage:
        {
            StorageInst* st = static_cast<StorageInst*>(this);
            tmp += " <Ports = " + std::to_string(st->getNumPorts()) + ">";
            tmp += " <Width = " + std::to_string(st->getBitWidth()) + ">";
            tmp += " <Depth = " + std::to_string(st->getDepth()) + ">";
            if (st->isROM())
                tmp += " <ROM>";
            if (st->isRAM())
                tmp += " <RAM>";
            if (st->isRegister())
                tmp += " <Register>";
            if (st->isShiftMem())
                tmp += " <ShiftMem>";
            if (st->isFifo())
                tmp += " <FIFO>";
            break;
        }
        case pf_internal::Core::Channel:
        {
            ChannelInst* co = static_cast<ChannelInst*>(this);
            tmp += " <Inputs = " + std::to_string(co->getNumInputs()) + ">";
            tmp += " <Outputs = " + std::to_string(co->getNumOutputs()) + ">";
            break;
        }
        case pf_internal::Core::IPBlock:
        {
            IPBlockInst* ip = static_cast<IPBlockInst*>(this);
            tmp += " <InPorts = " + std::to_string(ip->getNumInPorts()) + ">";
            tmp += " <OutPorts = " + std::to_string(ip->getNumOutPorts()) + ">";
            if (ip->isAsyncLogic())
                tmp += " <Async>";
            if (ip->isSyncLogic())
                tmp += " <Sync>";
            if (ip->isVivadoIP())
                tmp += " <VivadoIP>";
            if (ip->getCReg())
                tmp += " <CReg>";
            if (ip->getAcc())
                tmp += " <Acc>";
            break;
        }
        case pf_internal::Core::Adapter: break;
        default: break;
    }

    return tmp;
}

std::string CoreInst::selectRTLTemplate(std::string op, int in0Signed, int in1Signed, int stageNum) const
{
    auto getSigned = [](int portSigned){
        if(portSigned == 0) return "unsigned";
        if(portSigned == 1) return "signed";
        return "";
    };
    std::string signedString0 = getSigned(in0Signed);
    std::string signedString1 = getSigned(in1Signed);

    auto& s = Selector::getSelector();
    std::string libraryName = GetTargetPlatform()->getFactory().getLibraryName();
    // table {library_name}_CoreGen looks like: 
    // CORE_NAME OP LATENCY TEMPLATE
    std::string cmd = "select TEMPLATE from " + 
                      libraryName + "_CoreGen " +
                      "where '" + getName() +
                      "' like CORE_NAME and '" + op + 
                      "' like op and '" + signedString0 +
                      "' like SIGNED0 and '" + signedString1 +
                      "' like SIGNED1 and '" + std::to_string(stageNum) + 
                      "' like LATENCY;";
    return s.selectString(cmd.c_str());
}

// StorageInst
StorageInst::StorageInst(pf_internal::Storage* storage, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl)
    :CoreInst(storage, op, impl),
    mDepth(-1),
    mBitWidth(-1),
    mBanksNum(-1),
    mHasInit(false),
    mReset(false),
    m1WNRRequiredNumPorts(0)
{
    assert(PlatformBasic::isMemoryOp(op));
}

StorageInst::~StorageInst()
{
}

const CPortList& StorageInst::getMemPorts() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->getMemPorts();
}

const std::vector<unsigned>& StorageInst::getMemUsedPorts() const
{
    return mMemUsedPorts;
}

void StorageInst::setMemUsedPorts(const std::vector<unsigned>& ports)
{
    std::vector<unsigned> usedPorts = ports;
    usedPorts.erase(std::remove_if(usedPorts.begin(), usedPorts.end(), [](unsigned i) { return i > MemoryQuerier::PORT_TYPE::READ_WRITE;}),usedPorts.end());
    mMemUsedPorts = usedPorts;
}

unsigned StorageInst::getNumPorts() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->getNumPorts();
}

MemPort* StorageInst::getPortByIndex(unsigned index) const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->getPortByIndex(index);
}

int StorageInst::getDepth() const
{
    return mDepth;
}


bool StorageInst::isReadOnly() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->isReadOnly();
}

bool StorageInst::isROM() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->isROM();
}

bool StorageInst::isNPROM() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->isNPROM();
}


bool StorageInst::isRAM() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->isRAM();
}

bool StorageInst::isNPRAM() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->isNPRAM();
}

bool StorageInst::isRegister() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->isRegister();
}

bool StorageInst::isFifo() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->isFifo();
}

bool StorageInst::isShiftMem() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->isShiftMem();
}

bool StorageInst::is1wnrRAM() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->is1wnrRAM();
}

bool StorageInst::isEccRAM() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->isEccRAM();
}

bool StorageInst::supportByteEnable() const
{   
    // support ByteWriteEnable : NON-ECC BRAM, NON-ECC URAM, LUTRAM, AUTORAM.
    // Note : byte write enable is not supported with ECC
    return  (this->isRAM() && this->getMemoryType() != PlatformBasic::MEMORY_XPM_MEMORY) && (
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_AUTO ||
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BRAM || 
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_LUTRAM ||
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_URAM    );
}

bool StorageInst::isInitializable() const
{
    // support initialization : all RAMs/ROMs except URAM/ECC URAM
    return  this->isROM() || ( this->isRAM() && (
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_AUTO || 
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BRAM || 
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BRAM_ECC ||
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_LUTRAM ||
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BLOCK ||
                this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_DISTRIBUTE  ));
}

PlatformBasic::MEMORY_TYPE StorageInst::getMemoryType() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->getMemoryType();
}

PlatformBasic::MEMORY_IMPL StorageInst::getMemoryImpl() const
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->getMemoryImpl();
}

bool StorageInst::isURAM() const
{
    return getMemoryImpl() == PlatformBasic::MEMORY_IMPL_URAM && 
           getMemoryType() != PlatformBasic::MEMORY_FIFO;
}

bool StorageInst::isBRAM() const
{
    return (getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BLOCK ||
           getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BRAM)  &&
           getMemoryType() != PlatformBasic::MEMORY_FIFO;
}

bool StorageInst::isDRAM() const
{
    return (getMemoryImpl() == PlatformBasic::MEMORY_IMPL_DISTRIBUTE ||
           getMemoryImpl() == PlatformBasic::MEMORY_IMPL_LUTRAM)  &&
           getMemoryType() != PlatformBasic::MEMORY_FIFO;
}

bool StorageInst::isAuto() const
{
    return getMemoryImpl() == PlatformBasic::MEMORY_IMPL_AUTO && 
           getMemoryType() != PlatformBasic::MEMORY_FIFO &&
           !isShiftMem() &&
           !isRegister();
}

StorageInstList StorageInst::getSupportedInnerMemInstList() const
{
    StorageInstList insts;
    auto& fac = GetTargetPlatform()->getFactory();
   
    if (getMemoryType() == PlatformBasic::MEMORY_NP_MEMORY)
    {   
        std::vector<PlatformBasic::IMPL_TYPE> memImpls;

        if (getMemoryImpl() == PlatformBasic::MEMORY_IMPL_AUTO)
            memImpls = {    
                    PlatformBasic::RAM_1P_AUTO, PlatformBasic::RAM_2P_AUTO 
                }; 
        else if (getMemoryImpl() == PlatformBasic::MEMORY_IMPL_LUTRAM) 
            memImpls = {    
                    PlatformBasic::RAM_1P_LUTRAM, PlatformBasic::RAM_2P_LUTRAM, PlatformBasic::RAM_S2P_LUTRAM 
                };
        else if (getMemoryImpl() == PlatformBasic::MEMORY_IMPL_BRAM) 
            memImpls = { 
                    PlatformBasic::RAM_1P_BRAM, PlatformBasic::RAM_2P_BRAM, PlatformBasic::RAM_S2P_BRAM, PlatformBasic::RAM_T2P_BRAM
                };
        else if (getMemoryImpl() == PlatformBasic::MEMORY_IMPL_URAM) 
            memImpls = { 
                    PlatformBasic::RAM_1P_URAM, PlatformBasic::RAM_2P_URAM, PlatformBasic::RAM_S2P_URAM, PlatformBasic::RAM_T2P_URAM
                };

        for (auto & memImpl : memImpls ) { 
            auto memInst = fac.getCoreInst(PlatformBasic::OP_MEMORY, memImpl);
            if (memInst != NULL) {
                insts.push_back(std::static_pointer_cast<StorageInst>(memInst));
            }
        }

    } else {
        assert(0);
    }
    return insts;
}

void StorageInst::pushBackInnerMemInst(std::shared_ptr<StorageInst> inst)
{
    mBundledMemories.push_back(inst);
}

const StorageInstList& StorageInst::getInnerMemInstList() const
{
    return mBundledMemories;
}

int StorageInst::getFIFORawLatency(bool showMsg) const {
    bool isSrl = this->getMemoryImpl() == PlatformBasic::MEMORY_IMPL_SRL;

    int rawLatency = isSrl ? PFSettings::getInstance().getSrlFifoDefaultRawLatency() 
        : PFSettings::getInstance().getRamFifoDefaultRawLatency();
    // Get user configed read after write latency 
    const ConfigedLib& configLib = ConfigedLib::getInstance();
    int configedRawLatency = configLib.getConfigedLatency(this->getMemoryType());
    if (configedRawLatency > 0) {
        rawLatency = configedRawLatency;
    }
    // read after write latency needs to be less than depth
    int depth = getDepth();
    if (rawLatency > depth) {
        auto msgFunc = PFSettings::getInstance().getRawLatencyChangedMsgFunc();
        if (msgFunc && showMsg) {
            msgFunc(rawLatency, depth, getName());
        }
        rawLatency = depth;
    }
    return rawLatency;
}

// FuncUnitInst
FuncUnitInst::FuncUnitInst(pf_internal::FuncUnit* fu, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) :
    CoreInst(fu, op, impl), 
    mSparseMuxIncrEncoding(false)
{
    mInPorts = fu->getInPorts();
    mOutPorts = fu->getOutPorts();
}

FuncUnitInst::~FuncUnitInst() {}

CPortList& FuncUnitInst::getInPorts()
{
    return mInPorts;
}

const CPortList& FuncUnitInst::getInPorts() const
{
    return mInPorts;
}

CPortList& FuncUnitInst::getOutPorts()
{
    return mOutPorts;
}

const CPortList& FuncUnitInst::getOutPorts() const 
{
    return mOutPorts;
}

FuPort* FuncUnitInst::getInPortByIndex(unsigned index)
{
    assert (index <= mInPorts.size());
    return static_cast<FuPort*>(&mInPorts[index]);
}

FuPort* FuncUnitInst::getOutPortByIndex(unsigned index)
{
    assert (index <= mOutPorts.size());
    return static_cast<FuPort*>(&mOutPorts[index]);
}

unsigned FuncUnitInst::getNumInPorts() const
{
    return mInPorts.size();
}

unsigned FuncUnitInst::getNumOutPorts() const
{
    return mOutPorts.size();
}

// ChannelInst
ChannelInst::ChannelInst(pf_internal::Channel* chann, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) :
    CoreInst(chann, op, impl),
    mBitWidth(-1),
    mNumInputs(-1),
    mNumOutputs(-1),
    mDepthIn(-1),
    mDepthOut(-1)
{}

PlatformBasic::MEMORY_IMPL ChannelInst::getChannelMemoryImplBySize(int depth)
{
    unsigned threshold = 1024;
    if (depth * getBitWidth() < threshold)
        return PlatformBasic::MEMORY_IMPL_SRL;
    else
        return PlatformBasic::MEMORY_IMPL_MEMORY;
}
    
StorageInstList ChannelInst::getSupportedInnerMemInstList() const
{
    StorageInstList insts;
     auto& fac = GetTargetPlatform()->getFactory();
    // current only support the following two FIFO impl
    std::vector<PlatformBasic::IMPL_TYPE> fifoImpls = {PlatformBasic::FIFO_SRL, PlatformBasic::FIFO_MEMORY }; 
    for (auto & fifoImpl : fifoImpls ) 
    {
        auto fifoInst = fac.getCoreInst(PlatformBasic::OP_MEMORY, fifoImpl);
        if (fifoInst != NULL)
            insts.push_back(std::static_pointer_cast<StorageInst>(fifoInst));
    }
    return insts;  
}

// IPBlockInst
IPBlockInst::IPBlockInst(pf_internal::IPBlock* ip, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) :
    FuncUnitInst(ip, op, impl),
    mIsVivadoIP(false),
    mIPBType(IPB_Default),
    mNodeRef(0),
    mCReg(true),
    mAcc(false),
    mMetadata(""),
    mBlackBoxName("")
{}

IPBlockInst::~IPBlockInst() {}

IPBlockInst::LogicType IPBlockInst::getLogicType() const
{
    auto ipblock = static_cast<pf_internal::IPBlock*>(mCore);
    return static_cast<LogicType>(ipblock->getLogicType());
}

bool IPBlockInst::hasUnknownDelay()
{
    return getDelay() <= 0;
}

bool IPBlockInst::hasUnknownLatency()
{
    if (mCore->isBlackBoxIPCore())
        return false;//blackbox must has a fixed latency now.
    else
        return (isAsyncLogic() && getPipeLatency() <= 0);
}

ExprTreeNode_shared_ptr IPBlockInst::getExpression(unsigned i)
{
    auto ipblock = static_cast<pf_internal::IPBlock*>(mCore);
    return ipblock->getExpression(i);
}



const std::vector<ExprTreeNode_shared_ptr>& IPBlockInst::getExpressions() const
{
    auto ipblock = static_cast<pf_internal::IPBlock*>(mCore);
    return ipblock->getExpressions();
}

bool IPBlockInst::isFuncIP() const
{
    std::string name = getName();
    return name == "DSP48" || 
           name == "TAddSub" ||
           name == "QAddSub_DSP";
}

bool IPBlockInst::isDSPIP() const
{
    return getName() == "QAddSub_DSP";
}

std::string IPBlockInst::getIPBTypeName() const
{
    unsigned ip_type = getIPBType();
    std::string name;
    switch(ip_type)
    {
        case IPB_Default: 
            name = "IPB_Default";
            break;
        case DSP_Mul: 
            name = "DSP_Mul";
            break;
        case DSP_AM: 
            name = "DSP_AM";
            break;
        case DSP_MAC: 
            name = "DSP_MAC";
            break;
        case DSP_AMA: 
            name = "DSP_AMA";
            break;
        case DSP_Sel_MAC: 
            name = "DSP_Sel_MAC";
            break;
        case DSP_Square_AM: 
            name = "DSP_Square_AM";
            break;
        case DSP_Square_AMA: 
            name = "DSP_Square_AMA";
            break;
        case DSP_Acc_MAC: 
            name = "DSP_Acc_MAC";
            break;
        case DSP_Acc_AMA: 
            name = "DSP_Acc_AMA";
            break;
        case DSP_Tenary: 
            name = "DSP_Tenary";
            break;
        case DSP_QAddSub: 
            name = "DSP_QAddSub";
            break;
        case DSP_Dot: 
            name = "DSP_Dot";
            break;
        case DSP_DotAdd: 
            name = "DSP_DotAdd";
            break;
        default: 
            name = "IPB_Default";
            break;
    }
    return name;
}

/**
 * for QAddSub_DSP
 * Only for QAddSub_DSP.
 */
std::vector<unsigned> IPBlockInst::getPipeLatencyList(OperType Pty)
{
    auto querier = static_cast<DSPQAddSubQuerier*>(QuerierFactory::getInstance().getCoreQuerier(this));
    querier->setCurOper(Pty);
    auto res = querier->queryLatencyList(this);
    return res;
}

double IPBlockInst::getDelay(OperType oper) {
    std::vector<double> delayList = QuerierFactory::queryDelayList(this, oper);
    assert(delayList.size() == 3);
    return delayList[1];
}

double IPBlockInst::getFirstStageDelay(OperType oper) {
    std::vector<double> delayList = QuerierFactory::queryDelayList(this, oper);
    assert(delayList.size() == 3);

    return delayList[0];
}

double IPBlockInst::getLastStageDelay(OperType oper) {
    std::vector<double> delayList = QuerierFactory::queryDelayList(this, oper);
    assert(delayList.size() == 3);
    return delayList[2];
}

unsigned IPBlockInst::getPipeLatency(OperType oper) {
    if (!mCore->hasLatencyFunction()) {
        return  mCore->getFixedPipeLatency();
    }
    unsigned latency;
    int userLat = getConfigedLatency();
    // DSP58 is an exception since it has a special post-processing in queryLatency()
    if(userLat >= 0 && getName() != "DSP58_DotProduct" && 
            getName() != "DSP48" && getName() != "DSP_Double_Pump_Mac16" && 
            getName() != "DSP_Double_Pump_Mac8" && getName() != "QAddSub_DSP") {
        latency = userLat;
    } else {
        latency = QuerierFactory::queryLatency(this, oper);
    }
    return latency;
}

const ResUsageMap& IPBlockInst::getResourceUsage() {
    mCoreCostResUsage  = QueryCoreCost::getCoreCostResourceUsage(this);
    return mCoreCostResUsage;
}
double IPBlockInst::getResourceUsageByName(std::string name) {
    mCoreCostResUsage = QueryCoreCost::getCoreCostResourceUsage(this);
    if (mCoreCostResUsage.count(name)) {
        return mCoreCostResUsage.at(name);
    } else {
        return 0.0;
    }
}

unsigned IPBlockInst::getPipeInterval(OperType PTy) {
    // for blackbox, read json file; for others, return 1
    if (isBlackBoxIPCore())
    {
        auto bbInfoFromJsonFunc 
            = PFSettings::getInstance().getBlackBoxInfoFunction();
        if (getBlackBoxName() == "" || bbInfoFromJsonFunc == nullptr) {
            return mCore->getFixedPipeInterval();
        }
        return bbInfoFromJsonFunc(getBlackBoxName(), -1).pipeInterval;
    }
    return 1;
}

double IPBlockInst::getInputPortDelay(unsigned portId) {
    assert(isBlackBoxIPCore());
    auto bbInfoFromJsonFunc 
            = PFSettings::getInstance().getBlackBoxInfoFunction();
    if (getBlackBoxName() == "" || bbInfoFromJsonFunc == nullptr) {
        return 0.0;
    }
    return bbInfoFromJsonFunc(getBlackBoxName(), portId).inputPortDelay;
}
double IPBlockInst::getOutputPortDelay(unsigned portId) {
    assert(isBlackBoxIPCore());
    auto bbInfoFromJsonFunc 
            = PFSettings::getInstance().getBlackBoxInfoFunction();
    if (getBlackBoxName() == "" || bbInfoFromJsonFunc == nullptr) {
        return 0.0;
    }
    return bbInfoFromJsonFunc(getBlackBoxName(), portId).outputPortDelay;
}
unsigned IPBlockInst::getInputPortStage(unsigned portId) {
    assert(isBlackBoxIPCore());
    auto bbInfoFromJsonFunc 
            = PFSettings::getInstance().getBlackBoxInfoFunction();
    if (getBlackBoxName() == "" || bbInfoFromJsonFunc == nullptr) {
        return -1;
    }
    return bbInfoFromJsonFunc(getBlackBoxName(), portId).inputPortStage;
}
unsigned IPBlockInst::getOutputPortStage(unsigned portId) {
    assert(isBlackBoxIPCore());
    auto bbInfoFromJsonFunc 
            = PFSettings::getInstance().getBlackBoxInfoFunction();
    if (getBlackBoxName() == "" || bbInfoFromJsonFunc == nullptr) {
        return -1;
    }
    return bbInfoFromJsonFunc(getBlackBoxName(), portId).outputPortStage;
}

// AdapterInst
AdapterInst::AdapterInst(pf_internal::Adapter* adapter, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) :
    CoreInst(adapter, op, impl),
    mBitWidth(-1),
    mIORegslice(false)
{}

AdapterInst::~AdapterInst()
{
}

StorageInstList AdapterInst::getSupportedInnerMemInstList() const
{
    // assert(getName() == "s_axilite");
    StorageInstList insts;
    auto& fac = GetTargetPlatform()->getFactory();
    if (getName() == "s_axilite") 
    {
        // RAM_1P with latency=1
        auto inst = fac.getCoreInst(PlatformBasic::OP_MEMORY, PlatformBasic::RAM_1P_AUTO);
        inst->configLatencyBudget(1);
        insts.push_back(std::static_pointer_cast<StorageInst>(inst));
    }
    else if (getName() == "m_axi") 
    {
        std::vector<PlatformBasic::IMPL_TYPE> fifoImpls = { PlatformBasic::FIFO_MEMORY, PlatformBasic::FIFO_BRAM, PlatformBasic::FIFO_LUTRAM, PlatformBasic::FIFO_URAM };

        for (auto & fifoImpl : fifoImpls ) {
                auto fifoInst = fac.getCoreInst(PlatformBasic::OP_MEMORY, fifoImpl);
                if (fifoInst != NULL) {
                    insts.push_back(std::static_pointer_cast<StorageInst>(fifoInst));
                }
            }
    }
    else
    {
        assert(0);
    }

    return insts;
}

void AdapterInst::pushBackInnerMemInst(std::shared_ptr<StorageInst> inst)
{
    //assert(getName() == "s_axilite");
    mBundledMemories.push_back(inst);
}

const StorageInstList& AdapterInst::getInnerMemInstList() const
{
    //assert(getName() == "s_axilite");
    return mBundledMemories;
}

bool AdapterInst::isMultiChannelMAXI() const
{
    const CPortList& ports = getAdapterPorts();
    return (ports.size() > 1);
}

bool AdapterInst::enableReadOnlyCache() const
{
    const auto& paraMap = getMAXIParaMap();
    if (paraMap.count(CacheType) == 1)
    {
        return (paraMap.at(CacheType) == ReadOnlyCache);
    }
    return false;
}

double AdapterInst::getDelay(OperType oper) {
    std::vector<double> adapterDelayList = QuerierFactory::queryDelayList(this,oper);
    return adapterDelayList[1];
}

double AdapterInst::getFirstStageDelay(OperType oper) {
    std::vector<double> adapterDelayList = QuerierFactory::queryDelayList(this,oper);
    return adapterDelayList[0];
}

double AdapterInst::getLastStageDelay(OperType oper) {
    std::vector<double> adapterDelayList = QuerierFactory::queryDelayList(this,oper);
    return adapterDelayList[2];
}

unsigned AdapterInst::getPipeLatency(OperType oper) {
    if (!mCore->hasLatencyFunction()) {
        return  mCore->getFixedPipeLatency();
    }
    unsigned latency;
    int userLat = getConfigedLatency();
    // DSP58 is an exception since it has a special post-processing in queryLatency()
    if(userLat >= 0) {
        latency = userLat;
    } else {
        if (oper == AnyOperation) {
            latency = QuerierFactory::queryLatency(this, mOp);
        } else {
            latency = QuerierFactory::queryLatency(this, oper);
        }
    }
    return latency;
}

const ResUsageMap& AdapterInst::getResourceUsage() {
    mCoreCostResUsage = QueryCoreCost::getCoreCostResourceUsage(this);
    return mCoreCostResUsage;
}
double AdapterInst::getResourceUsageByName(std::string name) {
    ResUsageMap mCoreCostResUsage = QueryCoreCost::getCoreCostResourceUsage(this);
    if (mCoreCostResUsage.count(name)) {
        return mCoreCostResUsage.at(name);
    } else {
        return 0.0;
    }
}

CoreInstFactory::CoreInstFactory() : mCoreId(0)
{ 
    mCfgLib = ConfigedLib::getPointer(); 
    mCfgLib->mFac = this;

    clear();

    addGenericCore();

    // FIXME: Currently fixed to Tcl-based helper.
    //setHelper(new pf_internal::TclResLibHelper(this));
}

CoreInstFactory::~CoreInstFactory() {

}

std::shared_ptr<CoreInst> CoreInstFactory::createCoreInst(pf_internal::Core* core, PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl)
{
    if(core == 0) return 0;
    switch(core->getType())
    {
        case pf_internal::Core::Any:
        case pf_internal::Core::Generic:
            return std::shared_ptr<CoreInst>(new CoreInst(core, op, impl));
        case pf_internal::Core::FunctionUnit:
            return std::shared_ptr<FuncUnitInst>(new FuncUnitInst(static_cast<pf_internal::FuncUnit*>(core), op, impl));
        case pf_internal::Core::Storage:
            return std::shared_ptr<StorageInst>(new StorageInst(static_cast<pf_internal::Storage*>(core), op, impl));
        case pf_internal::Core::Channel:
            return std::shared_ptr<ChannelInst>(new ChannelInst(static_cast<pf_internal::Channel*>(core), op, impl));
        case pf_internal::Core::IPBlock:
            return std::shared_ptr<IPBlockInst>(new IPBlockInst(static_cast<pf_internal::IPBlock*>(core), op, impl));
        case pf_internal::Core::Adapter:
            return std::shared_ptr<AdapterInst>(new AdapterInst(static_cast<pf_internal::Adapter*>(core), op, impl));
        default: assert(0); break; 
    }

    return 0;
}

CoreInstList CoreInstFactory::createCoreInst(pf_internal::Core* core, OperType opcode)
{
    //std::cout << "CoreInstFactory::createCoreInst : pf_internal->core is " << core->getName() << " input opcode is " << opcode << std::endl;
    CoreInstList insts;
    auto platformBasic = PlatformBasic::getInstance();
    auto pairs = platformBasic->getOpImplFromCoreName(core->getName());
    // if no Node::OP, use PlatformBasic::OP instead
    if(opcode == AnyOperation)
    {
        for(auto pair : pairs)
        {
            std::shared_ptr<CoreInst> newCoreInst = createCoreInst(core, pair.first, pair.second);
            //std::cout << "CoreInstFactory::createCoreInst :     pair.first is " << pair.first << " and pair.second is " << pair.second << ". " << newCoreInst << std::endl;
            insts.push_back(newCoreInst);
        }
    }
    else
    {
        assert(!pairs.empty());
        insts.push_back(createCoreInst(core, static_cast<PlatformBasic::OP_TYPE>(opcode), pairs[0].second));
    }
    return insts;
}

std::shared_ptr<CoreInst> CoreInstFactory::getGenericCoreInst()
{
    static std::shared_ptr<CoreInst> inst = createCoreInst(getGenericCore(), PlatformBasic::OP_UNSUPPORTED, PlatformBasic::UNSUPPORTED);
    return inst;
}

std::shared_ptr<CoreInst> CoreInstFactory::getCoreInst(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) const
{
    std::shared_ptr<CoreInst> inst = NULL;
    auto platformBasic = PlatformBasic::getInstance();
    // TBD
    // if OP_MEMORY is removed, and replace with real memory ops(load, store...), this if check will no need.
    if(PlatformBasic::isMemoryOp(op))
    {
        op = PlatformBasic::OP_MEMORY;
    }
    auto coreBasic = platformBasic->getCoreFromOpImpl(op, impl);

    if(coreBasic != NULL) 
    {

        pf_internal::Core *core;
        auto coreName = coreBasic->getName();
        if(coreName == "AddSub" || coreName == "AddSubnS")
        {
            core = getCore("Adder");
        }
        else if(coreName == "Mul" || coreName == "MulnS")
        {
            core = getCore("Multiplier");
        } else {
            core = getCore(coreName);
        }
        inst= createCoreInst(core, op, impl); 
    }
    return inst;
}

std::string CoreInstFactory::getImplString(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) const
{
    bool isMemory = PlatformBasic::isMemoryOp(op);
    bool isNOIMPL = PlatformBasic::isNOIMPL(impl);
    auto pb = PlatformBasic::getInstance();
    if(isNOIMPL)
    {
        if(isMemory)
            impl = mCfgLib->getConfigedImpl(static_cast<PlatformBasic::MEMORY_TYPE>(impl));
        else
            impl = mCfgLib->getConfigedImpl(op);
    }
    
    std::string implName = isMemory ? pb->getMemImplName(pb->getMemImplEnum(impl)) : pb->getImplName(impl);
    return implName;
}

std::string CoreInstFactory::getOpString(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl) const
{
    bool isMemory = PlatformBasic::isMemoryOp(op);
    auto pb = PlatformBasic::getInstance();
    std::string opString = isMemory ? pb->getMemTypeName(pb->getMemTypeEnum(impl)) : pb->getOpName(op);
    return opString;
}

CoreInstList CoreInstFactory::getPragmaCoreInsts(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl, int latency) const
{
    // unite config_op/config_storage
    bool isMemory = PlatformBasic::isMemoryOp(op);
    bool isNOIMPL = PlatformBasic::isNOIMPL(impl);
    auto pb = PlatformBasic::getInstance();
    std::string name = isMemory ? pb->getMemTypeName(pb->getMemTypeEnum(impl)) : pb->getOpName(op);
    std::string opOrStorageStr = isMemory ? "storage type" : "operator";
    if(isNOIMPL)
    {
        if(isMemory)
            impl = mCfgLib->getConfigedImpl(static_cast<PlatformBasic::MEMORY_TYPE>(impl));
        else
            impl = mCfgLib->getConfigedImpl(op);
    }
    
    bool hasPragmaLatency = true;
    if(latency < 0)
    {
        hasPragmaLatency = false;
        if(isMemory)
            latency = mCfgLib->getConfigedLatency(static_cast<PlatformBasic::MEMORY_TYPE>(impl));
        else
            latency = mCfgLib->getConfigedLatency(op);
    }

    if(latency >= 0 && !pb->isValidLatency(op, impl, latency))
    {
        std::string cmd = hasPragmaLatency ? "bind_" : "config_";
        cmd += isMemory ? "storage" : "op";
        std::string implstr;
        if (!isNOIMPL)
        {
            implstr = std::string(" with impl ") + (isMemory ? pb->getMemImplName(pb->getMemImplEnum(impl)) : pb->getImplName(impl));
        }
        auto sendInvalidLatencyMsgFunc = PFSettings::getInstance().getSendInvalidLatencyMsgFunc();
        if (sendInvalidLatencyMsgFunc) {
            sendInvalidLatencyMsgFunc(cmd, latency, opOrStorageStr, name, implstr);
        }
    }

    CoreInstList insts;
    auto coreBasics = pb->getPragmaCoreBasics(op, impl, latency);
    for(auto coreBasic : coreBasics)
    {
        auto coreName = coreBasic->getName();
        auto core = getCore(coreName);
        if (core)
        {
            insts.push_back(createCoreInst(core, coreBasic->getOp(), coreBasic->getImpl()));
        }
        // AddSub, AddSubnS -> Adder; Mul, MulnS -> Multiplier
        else if(coreName == "AddSub" || coreName == "AddSubnS")
        {
            core = getCore("Adder");
            insts.push_back(createCoreInst(core, op, impl));
        }
        else if(coreName == "Mul" || coreName == "MulnS")
        {
            core = getCore("Multiplier");
            insts.push_back(createCoreInst(core, op, impl)); 
        }
    }
    return insts;
}

CoreInstList CoreInstFactory::getCoreInstsByOper(
    OperType opcode,
    int type_mask) const
{
    auto cores = getCoresByOper(opcode, type_mask);
    CoreInstList coreInsts;
    for (auto& core: cores)
    {   
        auto insts = createCoreInst(core, opcode);
        coreInsts.insert(coreInsts.end(), insts.begin(), insts.end());
    }
    mCfgLib->filterCoreInstList(coreInsts, static_cast<PlatformBasic::OP_TYPE>(opcode));

    if (opcode == PlatformBasic::OP_TYPE::OP_MUL) {
        makeVivadoAutoMultiplierAsDefault(coreInsts);
    }
    return coreInsts;   
}

void CoreInstFactory::getCoreInstsByInterfaceMode(
    CoreInstList& insts,
    InterfaceMode mode,
    int type_mask) const
{
    insts.clear();
    CoreList refList;
    getCoresByInterfaceMode(refList, mode, type_mask);
    for (auto& ref: refList) {
        auto cInsts = createCoreInst(ref);
        insts.insert(insts.end(), cInsts.begin(), cInsts.end());
    }
    mCfgLib->filterCoreInstList(insts);
}

void CoreInstFactory::getROMInsts(CoreInstList& list) const
{
    list.clear();
    CoreList refList;
    getROMs(refList);
    for (auto& ref: refList) {
        auto cInsts = createCoreInst(ref);
        list.insert(list.end(), cInsts.begin(), cInsts.end());
    }
    mCfgLib->filterMemoryInstList(list);
}

void CoreInstFactory::getRAMInsts(CoreInstList& list) const
{
    list.clear();
    CoreList refList;
    getRAMs(refList);
    for (auto& ref: refList) {
        auto cInsts = createCoreInst(ref);
        list.insert(list.end(), cInsts.begin(), cInsts.end());
    }
    mCfgLib->filterMemoryInstList(list);
}

void CoreInstFactory::getNPRAMInsts(CoreInstList& list) const
{
    list.clear();
    CoreList refList;
    getNPRAMs(refList);
    for (auto& ref: refList) {
        auto cInsts = createCoreInst(ref);
        list.insert(list.end(), cInsts.begin(), cInsts.end());
    }
    mCfgLib->filterMemoryInstList(list);
}

void CoreInstFactory::getRegisterInsts(CoreInstList& list) const
{
    list.clear();
    CoreList refList;
    getRegisters(refList);
    for (auto& ref: refList) {
        auto cInsts = createCoreInst(ref);
        list.insert(list.end(), cInsts.begin(), cInsts.end());
    }
    mCfgLib->filterMemoryInstList(list);
}

void CoreInstFactory::getShiftMemInsts(CoreInstList& list) const
{
    list.clear();
    CoreList refList;
    getShiftMems(refList);
    for (auto& ref: refList) {
        auto cInsts = createCoreInst(ref);
        list.insert(list.end(), cInsts.begin(), cInsts.end());
    }
    mCfgLib->filterMemoryInstList(list);
}

void CoreInstFactory::getIPBlockInsts(IPBlockInstList& list) const
{
    list.clear();
    IPBlockList refList;
    getIPBlocks(refList);
    CoreInstList coreInsts;
    for (auto& ref: refList) 
    {
        auto cInsts = createCoreInst(ref);
        coreInsts.insert(coreInsts.end(), cInsts.begin(), cInsts.end());
    }
    mCfgLib->filterCoreInstList(coreInsts);

    for(auto inst : coreInsts)
    {
        list.push_back(std::static_pointer_cast<IPBlockInst>(inst));
    }
}

std::string CoreInstFactory::getLibraryName() const
{
    return getName();
}

bool CoreInstFactory::isVersal() const
{
    auto family = getFamilyName();
    return family.find("versal") != std::string::npos;
}

void CoreInstFactory::requestCorebyOpAndImpl(
                          CoreInstList& list, 
                          PlatformBasic::OP_TYPE op,
                          PlatformBasic::IMPL_TYPE impl,
                          int typeMask) {
    // 1. find cores by op and impl
    if (impl == AnyImpl) {
        // op 
        auto coreInstTemps = getCoreInstsByOper(op, typeMask);
        for (auto& coreInst: coreInstTemps) {
            if (coreInst->getType() & typeMask) {
                list.push_back(coreInst);
            }
        }
    } else {
        // op and impl
        auto coreInstTemp = getCoreInst(op, impl);
        if (coreInstTemp == nullptr) {
            auto coreInstTemps = getCoreInstsByOper(op, typeMask);
            for (auto& coreInst: coreInstTemps) {
                if ((coreInst->getType() & typeMask) && (coreInst->getImpl() == impl)) {
                    list.push_back(coreInst);
                }
            }
        } else {
            list.push_back(coreInstTemp);
        }
    }
}

bool CoreInstFactory::requestFuncUnitInstList(
                        FuncUnitInstList& list,
                        PlatformBasic::OP_TYPE op,
                        double delayBudget,
                        const std::vector<unsigned>& inputBWList,
                        unsigned outputBW,

                        int preferredLatency,
                        PlatformBasic::IMPL_TYPE preferredImpl,
                        bool sparseMuxIncrEncoding
                        ) {
#if 0
    std::cout << "op " << op << "\n";
    std::cout << "delayBudget " << delayBudget << "\n";
    std::cout << "inputBWList";
    for (auto p: inputBWList) {
        std::cout << " " << p;
    }
    std::cout << "\n";
    std::cout << "outputBW " << outputBW << "\n";
    std::cout << "preferredLatency " << preferredLatency << "\n";
    std::cout << "preferredImpl " << preferredImpl << "\n";
#endif 

    assert((op > 0) && "Must be specified op");
    assert(list.size() == 0 && "List size must be 0");
    list.clear();

    // 1. Arguments check
    // if (op < 0 || delayBudget < 0 || inputBWList.size() == 0 || 
    //     outputBW == 0 || preferredLatency < AnyLatency || preferredImpl < AnyImpl) {
    //     return false;
    // }
    if (op < 0 || preferredLatency < AnyLatency || preferredImpl < AnyImpl) {
        return false;
    }
    CoreInstList coreList;
    // 2. find FuncUnits by op and impl
    requestCorebyOpAndImpl(coreList, op, preferredImpl, CoreInst::FunctionUnit);
    for (auto& core : coreList) {
        auto funcUnit = dyn_cast<FuncUnitInst>(core);
        // 3. filter FuncInst based on some limits. 
        //    a. "Diider_IP" has bit width limit
        //    b. If BE already has Preferred Latency, re-check whether it exceeds support range

        // if (funcUnit->getName() == "Divider_IP" &&  
        //     (*std::max_element(inputBWList.begin(), inputBWList.end()) > 64 || outputBW > 64)) {
        //    continue;
        // }

        // 4. setting 
        funcUnit->configDelayBudget(delayBudget);
        funcUnit->configLatencyBudget(preferredLatency);
        funcUnit->configInputBWList(inputBWList);
        funcUnit->configOutputBW(outputBW);
        if (funcUnit->getOp() == PlatformBasic::OP_SPARSEMUX) {
            funcUnit->configSparseMuxIncrEncoding(sparseMuxIncrEncoding);
        }
        
        // 5. query responed latency/delay/resource and save it
        setCoreCost(funcUnit);
        // 6. write result
        list.push_back(funcUnit);
    }

    return list.size() > 0;
}

bool CoreInstFactory::requestStorageInstList(
                    StorageInstList& list,
                    PlatformBasic::MEMORY_TYPE memType,         
                    double delayBudget,
                    int bitWidth,
                    int depth,
                    
                    bool hasInit,
                    bool needReset,
                    const std::vector<unsigned> portTypeVec,
                    int preferredLatency,
                    unsigned required1WNRNumPorts,
                    PlatformBasic::MEMORY_IMPL preferredImpl   
                ) {

    assert((memType > 0) && "Must be specified op");
    assert(list.size() == 0 && "List size must be 0");
    list.clear();

    // 1. Arguments check
    if (memType < 0 || preferredLatency < AnyLatency || preferredImpl < AnyImpl) {
        return false;
    }
    CoreInstList coreList;
    // 2. find Storage by op and impl

    // All Storage op are MOMORY 
    auto coreOp = PlatformBasic::OP_MEMORY;
    // FIXME: There is no MEMORY_IMPL_AUTO in the FIFO impl, only MEMORY_IMPL_MEMORY, but in fact their meanings are exactly the same.
    if (memType == PlatformBasic::MEMORY_FIFO && preferredImpl == PlatformBasic::MEMORY_IMPL_AUTO) {
        preferredImpl = PlatformBasic::MEMORY_IMPL_MEMORY;
    }
    auto resCore = PlatformBasic::getInstance()->getMemoryFromTypeImpl(memType, preferredImpl);
    if (!resCore)
      return false;
    auto coreImpl = resCore->getImpl();

    requestCorebyOpAndImpl(coreList, coreOp, coreImpl, CoreInst::Storage);
    for (auto& core : coreList) {
        auto storage = dyn_cast<StorageInst>(core);
        // 3. filter FuncInst based on some limits.

        // 4. setting 
        storage->configDelayBudget(delayBudget);
        storage->configLatencyBudget(preferredLatency);

        storage->configBitWidth(bitWidth);
        storage->configDepth(depth);   
        storage->configInit(hasInit);
        storage->configReset(needReset);
        storage->setMemUsedPorts(portTypeVec);
        storage->set1WNRRequiredNumPorts(required1WNRNumPorts);
        // 5. query responed latency/delay/resource and save it
        setCoreCost(storage);
        // 6. write result
        list.push_back(storage);
    }

    return list.size() > 0;

}

bool CoreInstFactory::requestChannelInstList(
                    ChannelInstList& list,
                    PlatformBasic::OP_TYPE op,
                    double delayBudget,
                    int bitWidth,
                    int numInputs,
                    int numOutputs,
                    int depthIn,
                    int depthOut,
                    int preferredLatency,
                    PlatformBasic::IMPL_TYPE preferredImpl    
                ) {
    
    assert((op > 0) && "Must be specified op");
    assert(list.size() == 0 && "List size must be 0");
    list.clear();

    // 1. Arguments check
    if (op < 0 || preferredLatency < AnyLatency || preferredImpl < AnyImpl) {
        return false;
    }
    CoreInstList coreList;
    // 2. find Channel by op and impl
    requestCorebyOpAndImpl(coreList, op, preferredImpl, CoreInst::Channel);
    for (auto& core : coreList) {
        auto ch = dyn_cast<ChannelInst>(core);
        // 3. filter FuncInst based on some limits.

        // 4. setting 
        ch->configDelayBudget(delayBudget);
        ch->configLatencyBudget(preferredLatency);
        ch->configBitWidth(bitWidth);
        ch->configNumInputs(numInputs);
        ch->configNumOutputs(numOutputs);
        ch->configDepthIn(depthIn);
        ch->configDepthOut(depthOut);

        // 5. query responed latency/delay/resource and save it
        setCoreCost(ch);
        // 6. write result
        list.push_back(ch);
    }

    return list.size() > 0;
}

bool CoreInstFactory::requestVivadoIpInst (
    std::shared_ptr<IPBlockInst> &ip,
    double delayBudget,
    const std::string& metadata,
    PlatformBasic::IMPL_TYPE impl                  //  Vivado_DDS, Vivado_FFT, Vivado_FIR
) {
    assert(impl == PlatformBasic::VIVADO_FFT || impl == PlatformBasic::VIVADO_FIR || impl == PlatformBasic::VIVADO_DDS);
    
    auto core = getCoreInst(PlatformBasic::OP_VIVADO_IP, impl);
    ip = dyn_cast<IPBlockInst>(core);
    if (ip == nullptr) {
        return false;    
    }

    // Config 
    ip->configDelayBudget(delayBudget);
    ip->configMetadata(metadata);

    // setCoreCost(ip);
    return true;
}

// IPBlock ?? 
// DSP48 and DSP58 
bool CoreInstFactory::requestDspInst (
    std::shared_ptr<IPBlockInst> &dsp,
    double delayBudget,
    const std::string& metadata,
    const std::vector<unsigned>& inputBWList,
    unsigned outputBW,

    PlatformBasic::IMPL_TYPE impl,                   //  DSP48, DSP58_DP,  QADDER
    bool creg,
    bool acc, 
    unsigned ipType,
    unsigned nodeRef,
    int preferredLatency
) {
    CoreInstList ips;
    requestCorebyOpAndImpl(ips, PlatformBasic::OP_ALL, impl, CoreInst::IPBlock);
    if (ips.empty()) {
        return false;
    }
    dsp = dyn_cast<IPBlockInst>(ips[0]); 
    if (dsp == nullptr) {
        return false;
    }
    // TODO: bit-width ?
    dsp->configDelayBudget(delayBudget);
    dsp->configMetadata(metadata);
    dsp->configInputBWList(inputBWList);
    dsp->configOutputBW(outputBW);
    dsp->configCReg(creg);
    dsp->configAcc(acc);
    dsp->configLatencyBudget(preferredLatency);
    dsp->setIPBType(ipType);
    dsp->setNodeRef(nodeRef);

    // setCoreCost(dsp);
    return true;
}

bool CoreInstFactory::requestBlackBoxInst (
    std::shared_ptr<IPBlockInst> &blackBox,
    PlatformBasic::OP_TYPE op,
    double delayBudget,
    const std::string& bbName
) {
    assert(op == PlatformBasic::OP_CALL || op == PlatformBasic::OP_BLACKBOX);
    auto core = getCoreInst(op, PlatformBasic::AUTO);
    blackBox = dyn_cast<IPBlockInst>(core);
    if (blackBox == nullptr) {
        return false;    
    }

    // Config 
    blackBox->configDelayBudget(delayBudget);
    blackBox->setBlackBoxName(bbName);

    // setCoreCost(blackBox);
    return true;
}

bool CoreInstFactory::requestTAddSubInst(
    std::shared_ptr<IPBlockInst> &tAddSub,
    PlatformBasic::OP_TYPE op,
    double delayBudget,
    const std::vector<unsigned>& inputBWList,
    unsigned outputBW
) {
    assert(op == PlatformBasic::OP_ALL || op == PlatformBasic::OP_ADD || op == PlatformBasic::OP_SUB);
    auto core = getCoreInst(op, PlatformBasic::TADDER);
    tAddSub = dyn_cast<IPBlockInst>(core);
    if (tAddSub == nullptr) {
        return false;    
    }

    // config 
    tAddSub->configDelayBudget(delayBudget);
    tAddSub->configInputBWList(inputBWList);
    tAddSub->configOutputBW(outputBW);

    // setCoreCost(tAddSub);
    return true;
}

bool CoreInstFactory::requestAdapterInst(
    std::shared_ptr<AdapterInst> &axi,
    PlatformBasic::OP_TYPE op,
    PlatformBasic::IMPL_TYPE impl,
    double delayBudget,
    int bitWidth,

    std::map<unsigned, unsigned> maxiParaMap,
    std::vector<std::vector<unsigned>> axiLitePortsVec,
    CPortList ports,
    StorageInstList bundledMemories,
    bool enableIORegslice,
    std::map<unsigned, std::map<unsigned, unsigned> > maxiChanParaMap
) {
    assert(impl == PlatformBasic::REG_SLICE || impl == PlatformBasic::S_AXILITE || impl == PlatformBasic::M_AXI);
    auto core = getCoreInst(op, impl);
    axi = dyn_cast<AdapterInst>(core);
    if (axi == nullptr) {
        return false;    
    }

    // config 
    axi->configDelayBudget(delayBudget);
    axi->configBitWidth(bitWidth);
    axi->setMAXIParaMap(maxiParaMap);
    axi->setMAXIChanParaMap(maxiChanParaMap);
    axi->setAXILitePortsVec(axiLitePortsVec);
    axi->setAdapterPorts(ports);
    axi->mBundledMemories = bundledMemories;
    axi->configIORegslice(enableIORegslice);

    // setCoreCost(axi);
    return true;
}

void CoreInstFactory::setCoreCost(std::shared_ptr<CoreInst> core) {    

    auto delayList = QueryCoreCost::getCoreCostDelayList(core.get(), AnyOperation);
    core->setDelayList(delayList);
    auto latency = QueryCoreCost::getCoreCostPipeLatency(core.get(), AnyOperation);
    core->setPipeLatency(latency);
    ResUsageMap resMap = QueryCoreCost::getCoreCostResourceUsage(core.get());
    core->setResUsage(resMap);
}


bool CoreInstFactory::isUltrascale() const
{
    auto family = getFamilyName();
    return family.find("kintexu") != std::string::npos ||
           family.find("virtexu") != std::string::npos ||
           family.find("zynqu") != std::string::npos;
}

bool CoreInstFactory::isVirtex4() const
{
    auto family = getFamilyName();
    return family.find("virtex4") != std::string::npos;
}

bool CoreInstFactory::isVirtex5() const
{
    auto family = getFamilyName();
    return family.find("virtex5") != std::string::npos;
}

bool CoreInstFactory::isSpartan() const
{
    auto family = getFamilyName();
    return family.find("spartan") != std::string::npos;
}

bool CoreInstFactory::is7Series() const
{
    auto family = getFamilyName();
    return family.find("7") != std::string::npos ||
           //ends wtih "zynq"
           (family.size() >= 4 && family.substr(family.size() - 4) == "zynq");
}

bool CoreInstFactory::is8Series() const
{
    auto family = getFamilyName();
    // ends with 'u'
    return family[family.size() - 1] == 'u';
}

bool CoreInstFactory::is9Series() const
{
    auto family = getFamilyName();
    return family.find("uplus") != std::string::npos;
}

double CoreInstFactory::getCore2CoreDelay(const std::string& coreA, const std::string& coreB, int bit) {
    
    auto& s = Selector::getSelector();
    std::string libName = GetTargetPlatform()->getFactory().getLibraryName();
    // std::string libName = "versal_medium";
    std::string mapCmd = "select CORE_DELAY_MAP from " + libName + "_CoreDef where CORE_NAME='" + coreA + "' ";
    std::string coreFrom = s.selectString(mapCmd.c_str());
    mapCmd = "select CORE_DELAY_MAP from " + libName + "_CoreDef where CORE_NAME='" + coreB + "' ";
    std::string coreTo = s.selectString(mapCmd.c_str());

    assert(!coreFrom.empty());
    assert(!coreTo.empty());

    std::string cmd = "select BITS from " + libName + "_WireDelay where CORE_FROM='" + coreFrom + "' and CORE_TO='" + coreTo
                        + "'";
    std::vector<int> qBits = s.selectIntList(cmd.c_str());
    auto queryData = [coreFrom, coreTo, libName](int qBit) {
        std::string cmd = "select DELAY from " + libName + "_WireDelay where CORE_FROM='" + coreFrom + "' and CORE_TO='" + coreTo
                        + "' and BITS=" + std::to_string(qBit);
        auto& s = Selector::getSelector();
        return s.selectDouble(cmd.c_str());
    };

    int index = 0;
    for (index = 0; index < qBits.size(); index ++) {
        if (qBits[index] > bit) {
            break;
        }
    }
    if (index == 0) {
        return queryData(qBits[0]);
    } else if (index == qBits.size()) {
        return queryData(qBits[qBits.size() - 1]);
    } else {
        double ldata = queryData(qBits[index - 1]);
        double rdata = queryData(qBits[index]);

        return ldata + (rdata - ldata) / (qBits[index] - qBits[index - 1]) * (bit - qBits[index - 1]);
    }
    
}

void ConfigedLib::setConfigOp(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl, int latency)
{
    mConfigedOpMap[op] = std::make_pair(impl, latency);
}

void ConfigedLib::resetConfigOp(PlatformBasic::OP_TYPE op)
{
    mConfigedOpMap.erase(op);
}

void ConfigedLib::setConfigStorage(PlatformBasic::MEMORY_TYPE type, PlatformBasic::MEMORY_IMPL impl, int latency)
{
    mConfigedStorageMap[type] = std::make_pair(impl, latency);
}

void ConfigedLib::resetConfigStorage(PlatformBasic::MEMORY_TYPE type)
{
    mConfigedStorageMap.erase(type);
}

void ConfigedLib::setConfigCore(const std::string& coreName, int config_lat) {
    mConfigedCoreMap[coreName] = config_lat;
}

PlatformBasic::IMPL_TYPE ConfigedLib::getConfigedImpl(PlatformBasic::OP_TYPE op) const
{
    PlatformBasic::IMPL_TYPE impl = PlatformBasic::NOIMPL;
    auto it = mConfigedOpMap.find(op);
    if(it != mConfigedOpMap.end())
    {
        impl = it->second.first;
    }
    return impl;
}

PlatformBasic::IMPL_TYPE ConfigedLib::getConfigedImpl(PlatformBasic::MEMORY_TYPE type) const
{
    // default is NOIMPL_type
    PlatformBasic::IMPL_TYPE impl = static_cast<PlatformBasic::IMPL_TYPE>(type);
    auto it = mConfigedStorageMap.find(type);
    if(it != mConfigedStorageMap.end())
    {
        auto memImpl = it->second.first;
        if(memImpl != PlatformBasic::MEMORY_NOIMPL)
        {
            impl = PlatformBasic::getInstance()->getMemoryFromTypeImpl(type, memImpl)->getImpl();
        } else {
            if (type == PlatformBasic::MEMORY_FIFO) {
                impl = PlatformBasic::NOIMPL_FIFO;
            }
        }
    }
    return impl;
}

int ConfigedLib::getConfigedLatency(PlatformBasic::OP_TYPE op) const
{
    int latency = -1;

    auto it = mConfigedOpMap.find(op);
    if(it != mConfigedOpMap.end())
    {
        latency = it->second.second;
    }
    return latency;
}

int ConfigedLib::getConfigedLatency(PlatformBasic::MEMORY_TYPE type) const
{
    int latency = -1;

    auto it = mConfigedStorageMap.find(type);
    if(it != mConfigedStorageMap.end())
    {
        latency = it->second.second;
    }
    return latency;
}

int ConfigedLib::getConfigedLatency(const std::string& coreName) const {
    if(mConfigedCoreMap.count(coreName)) {
        return mConfigedCoreMap.at(coreName);
    } else {
        return -1;
    }
}

void ConfigedLib::filterCoreInstList(CoreInstList& coreInstList, PlatformBasic::OP_TYPE op) const
{
    if(PlatformBasic::isMemoryOp(op))
    {
        filterMemoryInstList(coreInstList);
    }
    // for normal op
    else
    {
        auto it = mConfigedOpMap.find(op);
        if(it != mConfigedOpMap.end())
        {
            auto impl = it->second.first;
            auto latency = it->second.second;
            // match op but not impl or latecny
            auto matchOpWithoutImpl = [op, impl, latency](std::shared_ptr<CoreInst> coreInst) 
            { 
                if(coreInst->getOp() == op)
                {
                    if(impl != PlatformBasic::NOIMPL && impl != coreInst->getImpl()) 
                    {
                        return true;
                    }
                    if(!coreInst->isValidLatency(latency))
                    {
                        return true;
                    }
                }
                return false;
            };
            coreInstList.erase(std::remove_if(coreInstList.begin(), coreInstList.end(), matchOpWithoutImpl), coreInstList.end());
        }
    }
    return;
}

void ConfigedLib::filterCoreInstList(CoreInstList& coreInstList) const
{
    for(auto pair : mConfigedOpMap)
    {
        filterCoreInstList(coreInstList, pair.first);
    }

    filterMemoryInstList(coreInstList);
}

void ConfigedLib::filterMemoryInstList(CoreInstList& coreInstList) const
{
    for(auto pair : mConfigedStorageMap)
    {
        auto type = pair.first;
        auto impl = pair.second.first;
        auto latency = pair.second.second;
        // match type but not impl or latecny
        auto matchTypeWithoutImpl = [type, impl, latency](std::shared_ptr<CoreInst> coreInst)
        {
            if(coreInst->getType() == CoreInst::Storage)
            {
                auto storage = std::static_pointer_cast<StorageInst>(coreInst);
                if(storage->getMemoryType() == type)
                { 
                    if(impl != PlatformBasic::MEMORY_NOIMPL && impl != storage->getMemoryImpl())
                    {
                        return true;
                    }
                    if(!storage->isValidLatency(latency))
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        coreInstList.erase(std::remove_if(coreInstList.begin(), coreInstList.end(), matchTypeWithoutImpl), coreInstList.end());
    }
}

bool ConfigedLib::checkOpSupport(PlatformBasic::OP_TYPE op, PlatformBasic::IMPL_TYPE impl, int latency) {
    bool error = false;
    auto pb = PlatformBasic::getInstance();
    if(!PlatformBasic::isNOIMPL(impl)) {
        auto coreBasics = pb->getPragmaCoreBasics(op, impl, -1);
        for(auto coreBasic : coreBasics) {
            auto core = mFac->getCore(coreBasic->getName());
            if(!core) {
                error = true;
            }
        } 
    }
    return !error;
}

bool ConfigedLib::checkStorageSupport(PlatformBasic::MEMORY_TYPE type, PlatformBasic::MEMORY_IMPL impl, int latenc) {
    bool error = false;
    auto pb = PlatformBasic::getInstance();
    if(PlatformBasic::MEMORY_NOIMPL != impl) {
        auto coreBasic = pb->getMemoryFromTypeImpl(type, impl);
        auto core = mFac->getCore(coreBasic->getName());
        if(!core) {
            error = true;
        }
    } 
    return !error;
}

////
unsigned CoreInstFactory::string2CoreType(string ty_str)
{
    static const char* StrFuncUnit = "FUNCTIONAL_UNIT";
    static const char* StrFU = "FU"; // FU alias

    static const char* StrStorage = "STORAGE";
    static const char* StrMEM = "MEM"; // Memory alias

    static const char* StrChannel = "CHANNEL";

    static const char* StrAdapter = "ADAPTER";
    static const char* StrIF = "IF"; // Adapter alias

    static const char* StrUserIP = "IP_BLOCK";
    static const char* StrIP = "IP"; // IP alias

    std::transform(ty_str.begin(), ty_str.end(),
                   ty_str.begin(), (int(*)(int))toupper);

    if (ty_str == StrFuncUnit || ty_str == StrFU)
        return pf_internal::Core::FunctionUnit;

    if (ty_str == StrStorage || ty_str == StrMEM)
        return pf_internal::Core::Storage;

    if (ty_str == StrChannel)
        return pf_internal::Core::Channel;

    if (ty_str == StrAdapter || ty_str == StrIF)
       return pf_internal::Core::Adapter;

    if (ty_str == StrUserIP || ty_str == StrIP)
        return pf_internal::Core::IPBlock;

    //return Core::Any;
    return pf_internal::Core::Generic;
}

#if 0
string CoreInstFactory::coreType2String(unsigned type)
{
    static const char* StrFuncUnit = "Functional unit";
    static const char* StrStorage = "Storage";
    static const char* StrChannel = "Channel";
    static const char* StrAdapter = "Adapter";
    static const char* StrUserIP = "User IP";
    static const char* StrGeneric = "Generic";

    switch (type)
    {
      case pf_internal::Core::FunctionUnit:
          return StrFuncUnit;
      case pf_internal::Core::Storage:
          return StrStorage;
      case pf_internal::Core::Channel:
          return StrChannel;
      case pf_internal::Core::Adapter:
          return StrAdapter;
      case pf_internal::Core::IPBlock:
          return StrUserIP;
      default:
          return StrGeneric;
    }
}

#endif

std::vector<CoreDef*> CoreInstFactory::selectCoreDefs() const
{
    std::vector<CoreDef*> defs;
    if(!mName.empty())
    {
        std::string cmd = "select * from " + mName + "_CoreDef ";
        auto& s = Selector::getSelector();
        defs = s.selectCoreDefs(cmd.c_str());
    }
    return defs;
}

int CoreInstFactory::createCores()
{
    clear();
    mCoreId = 0;
    addGenericCore();
    // read core definitions from platform.db
    // mCoreId = 1; the first is generic core
    std::vector<CoreDef*> definitions = selectCoreDefs();
    for(auto def : definitions)
    {
        auto core = createCore(*def, mCoreId);
        //mCores.push_back(core); 
        addCore(core);
        //mCoreId++;
        //delete def;
    }

    auto bramNum = mTargetPlatform->getChipInfo()->getResourceBudgetByName("BRAM");
    if(bramNum == 0)
    {
        for (auto def : definitions)
        {
            if (def->impl.find("bram") != std::string::npos)
            {
                std::string coreName = def->name;
                auto core = getCore(coreName);
                if(core == NULL) continue;
                removeCore(core);
            }
        }
    }
    auto uramNum = mTargetPlatform->getChipInfo()->getResourceBudgetByName("URAM");
    if(uramNum == 0)
    {
        for (auto def : definitions)
        {
            if (def->impl.find("uram") != std::string::npos)
            {
                std::string coreName = def->name;
                auto core = getCore(coreName);
                if(core == NULL) continue;
                removeCore(core);
            }
        }
    }
    auto dspNum = mTargetPlatform->getChipInfo()->getResourceBudgetByName("DSP");
    if(dspNum == 0)
    {
        for (auto def : definitions)
        {
            if (def->impl.find("dsp") != std::string::npos || def->impl.find("vivado_dds") != std::string::npos || def->impl.find("vivado_fir") != std::string::npos || def->impl.find("vivado_fft") != std::string::npos || def->impl.find("qadder") != std::string::npos)
            {
                std::string coreName = def->name;
                auto core = getCore(coreName);
                if(core == NULL) continue;
                removeCore(core);
            }
        }
    }
    
    for(auto def : definitions)
    {
        delete def;
    }

    return 0;
}

pf_internal::Core* CoreInstFactory::createCore(const CoreDef& def, unsigned id)
{
    pf_internal::Core* core = NULL;
    switch(string2CoreType(def.type))
    {
        case pf_internal::Core::FunctionUnit:
            core = new pf_internal::FuncUnit(id, def.name, def.description, def.opers, 
                                def.style, def.delay, def.delayFunction, def.isDelayList, 
                                def.latency, def.latencyFunction, 
                                def.resourceMap, def.usageFunction,
                                def.generator, def.interval, def.intervalFunction,
                                def.isHidden, def.legalityFunction, def.preference,
                                def.inputs, def.outputs, def.portStageFunction);
            break;
        case pf_internal::Core::Storage:
            core = new pf_internal::Storage( id, def.name, def.description, def.opers, 
                                def.style, def.delay, def.delayFunction, def.isDelayList, 
                                def.latency, def.latencyFunction, 
                                def.resourceMap, def.usageFunction,
                                def.generator, def.interval, def.intervalFunction,
                                def.isHidden, def.legalityFunction, def.preference,
                                def.depth, def.ifModes, def.memPorts);
            break;
        case pf_internal::Core::Channel:
            core = new pf_internal::Channel(id, def.name, def.description, def.opers, 
                               def.style, def.delay, def.delayFunction, def.isDelayList, 
                               def.latency, def.latencyFunction, 
                               def.resourceMap, def.usageFunction,
                               def.generator, def.interval, def.intervalFunction,
                               def.isHidden, def.legalityFunction, def.preference,
                               def.inputs, def.outputs, def.depth);          
           break;
        case pf_internal::Core::IPBlock:
            core = new pf_internal::IPBlock(id, def.name, def.description, def.opers, 
                                def.style, def.delay, def.delayFunction, def.isDelayList, 
                                def.latency, def.latencyFunction, 
                                def.resourceMap, def.usageFunction,
                                def.generator, def.interval, def.intervalFunction,
                                def.isHidden, def.legalityFunction, def.preference,
                                def.inputs, def.outputs,
                                def.expression, def.logic,
                                def.portStageFunction, def.targetComponent);
            break;
        case pf_internal::Core::Adapter:
            core = new pf_internal::Adapter(id, def.name, def.description, def.opers, 
                                def.style, def.delay, def.delayFunction, def.isDelayList, 
                                def.latency, def.latencyFunction, 
                                def.resourceMap, def.usageFunction,
                                def.generator, def.interval, def.intervalFunction,
                                def.isHidden, def.legalityFunction, def.preference,
                                def.ifModes);
            break;
        default: assert(0); break;
    };
    return core;
}

void CoreInstFactory::setName(const std::string& libName)
{
    mName = libName;
    // to get libName(family name) from library name
    // for example, get virtexuplus from virtexuplus_medium
    mFamilyName = mName.substr(0, mName.find('_'));
}

void CoreInstFactory::addCore(pf_internal::Core* core)
{
    assert(core);
    //DEBUG_ASSERT(core);
    core->setId(mCoreId);
    mId2Core[mCoreId] = core;

    // FIXME: Check the name uniqueness?
    string coreName = core->getName();
    std::transform(coreName.begin(),
                   coreName.end(),
                   coreName.begin(),
                   (int(*)(int))toupper);

    assert(mName2Id.count(coreName) == 0);
    //DEBUG_ASSERT(mName2Id.count(coreName) == 0);

    mName2Id[coreName] = mCoreId;
    mId2Names[mCoreId].insert(coreName);
    ++mCoreId;

    mCores.push_back(core);

#ifdef PLATFORM_DEBUG
    SS << "Core \"" << BEDebug::getString_Core(core) << "\" is added";
    DEBUG_LOG;
#endif
}

void CoreInstFactory::clear()
{
    mId2Core.clear();
    mName2Id.clear();
    mId2Names.clear();

    mCores.clear();
}

pf_internal::Core* CoreInstFactory::getGenericCore()
{
    static pf_internal::Core generic_core;
    return &generic_core;
}

void CoreInstFactory::addGenericCore()
{
    pf_internal::Core* core = getGenericCore();
    // Dont add the same generic core twice.
    if (!mCores.empty())
        if (core == mCores[0])
            return;

    mCores.push_back(core);

    core->setId(mCoreId);
    core->setType(pf_internal::Core::Generic);

    mId2Core[mCoreId] = core;
    // FIXME: Name is an empty string here?
    // Normalize to the upper case
    string norm_name = core->getName();
    std::transform(norm_name.begin(), norm_name.end(),
                   norm_name.begin(), (int(*)(int))toupper);
    mName2Id[norm_name] = mCoreId;
    mId2Names[mCoreId].insert(norm_name);
    ++mCoreId;
}

pf_internal::Core* CoreInstFactory::getCore(unsigned id) const
{
    assert(mId2Core.count(id) > 0 && "Invalid core ID");
    return mId2Core.at(id);
}

pf_internal::Core* CoreInstFactory::getCore(string name) const
{
    std::transform(name.begin(), name.end(),
                   name.begin(), (int(*)(int))toupper);
    if (mName2Id.count(name) <= 0)
        return 0;

    return getCore(mName2Id.at(name));
}

CoreList CoreInstFactory::getCoresByOper(OperType opcode, int type_mask) const
{
    CoreList core_list;
    for (CoreIter ci = mCores.begin(); ci != mCores.end(); ++ci)
    {
        pf_internal::Core* core = *ci;
        if (core->getType() & type_mask)
            if (core->matchOper(opcode) && !core->isHidden()) // Skip the hidden cores
                core_list.push_back(core);
    }

    return core_list;
}

void CoreInstFactory::makeVivadoAutoMultiplierAsDefault(CoreInstList& coreList) const
{
    if (coreList.size() > 1) {
        std::shared_ptr<CoreInst> mulAuto = nullptr;
        for (auto core: coreList) {
            if (core->getImpl() == PlatformBasic::IMPL_TYPE::AUTO) {
                mulAuto = core;
                break;
            }
        }
        if (mulAuto) {
            coreList = {mulAuto};
        }
    }
}

void CoreInstFactory::getCoresByInterfaceMode(CoreList& core_list,InterfaceMode mode,int type_mask) const
{
    core_list.clear();

    for (pf_internal::Core* core : mCores)
    {
        if (core->getType() & type_mask)
        {
            if (core->matchInterfaceMode(mode))
                core_list.push_back(core);
        }
    }
}

void CoreInstFactory::getIPBlocks(IPBlockList& ip_list) const
{
    ip_list.clear();
    for(auto it = mCores.begin(); it != mCores.end(); ++it)
    {
        if((*it)->getType() == pf_internal::Core::IPBlock)
        {
            ip_list.push_back(static_cast<pf_internal::IPBlock*>(*it));
        }
    }
}

void CoreInstFactory::getROMs(CoreList& rom_list) const
{
    rom_list.clear();
    for(auto it = mCores.begin(); it != mCores.end(); ++it)
    {
        if((*it)->getType() == pf_internal::Core::Storage)
        {
            auto strg = static_cast<pf_internal::Storage*>(*it);
            if(strg->isROM())
            {
                rom_list.push_back(strg);
            }
        }
    }
}

void CoreInstFactory::getRAMs(CoreList& ram_list) const
{
    ram_list.clear();

    for(auto it = mCores.begin(); it != mCores.end(); ++it)
    {
        if((*it)->getType() == pf_internal::Core::Storage)
        {
            auto strg = static_cast<pf_internal::Storage*>(*it);
            if(strg->isRAM())
            {
                ram_list.push_back(strg);
            }
        }
    }
}

void CoreInstFactory::getNPRAMs(CoreList& ram_list) const
{
    ram_list.clear();
    
    for(auto it = mCores.begin(); it != mCores.end(); ++it)
    {
        if((*it)->getType() == pf_internal::Core::Storage)
        {
            auto strg = static_cast<pf_internal::Storage*>(*it);
            if(strg->isNPRAM())
            {
                ram_list.push_back(strg);
            }
        }
    }
}

void CoreInstFactory::getRegisters(CoreList& reg_list) const
{
    reg_list.clear();

    for(auto it = mCores.begin(); it != mCores.end(); ++it)
    {
        if((*it)->getType() == pf_internal::Core::Storage)
        {
            auto strg = static_cast<pf_internal::Storage*>(*it);
            if(strg->isRegister())
            {
                reg_list.push_back(strg);
            }
        }
    }
}

void CoreInstFactory::getShiftMems(CoreList& mem_list) const
{
    mem_list.clear();
    for(auto it = mCores.begin(); it != mCores.end(); ++it)
    {
        if((*it)->getType() == pf_internal::Core::Storage)
        {
            auto strg = static_cast<pf_internal::Storage*>(*it);
            if(strg->isShiftMem())
            {
                mem_list.push_back(strg);
            }
        }
    }
}

CoreIter CoreInstFactory::locateCore(pf_internal::Core* core)
{
    assert(core);

    CoreIter ci;

    for (ci = mCores.begin(); ci != mCores.end(); ++ci)
        if (*ci == core)
            break;

    return ci;
}

void CoreInstFactory::removeCore(pf_internal::Core* core)
{
    assert(core != 0 && getCore(core->getId()) == core);

    CoreIter ci = locateCore(core);
    if (ci == mCores.end())
        return;
    mCores.erase(ci);

    // Clear all aliases
    unsigned id = core->getId();
    set<string>& names = mId2Names[id];

    for (auto ni = names.begin(); ni != names.end(); ++ni)
    {
        mName2Id.erase(*ni);
    }

    mId2Names.erase(id);
    mId2Core.erase(id);

    delete core;
}

int CoreInst::getConfigedLatency() const
{
    int latency = -1;
    ConfigedLib& configLib = ConfigedLib::getInstance();
    // fixed latency
    if (!mCore->hasLatencyFunction())
    {
        latency = mCore->getFixedPipeLatency();
    }
    // pragma bind_op
    else if(getLatencyBudget()>= 0)
    {
        latency = getLatencyBudget();
    }
    //config_core
    else if(configLib.getConfigedLatency(getName()) >= 0)
    {
        latency = configLib.getConfigedLatency(getName());
    }
    // config_op 
    else
    {
        latency = configLib.getConfigedLatency(mOp);
    }
    return latency;
}

int StorageInst::getConfigedLatency() const
{
    int latency = -1;
    ConfigedLib& configLib = ConfigedLib::getInstance();
    // pragma bind_storage
    if(getLatencyBudget()>= 0)
    {
        latency = getLatencyBudget();
    }
    // config_core
    else if(configLib.getConfigedLatency(getName()) >= 0)
    {
        latency = configLib.getConfigedLatency(getName());
    }
    // config_storage 
    else
    {
        ConfigedLib& configLib = ConfigedLib::getInstance();
        latency = configLib.getConfigedLatency(getMemoryType());
    }
    return latency;
}

}//< namespace platform

namespace pf_internal
{
using namespace platform;

// Core class member functions
Core::Core()
{
    // Name and type
    mId = 0;
    mName = "";
    mType = Generic;
    mIsHidden = false;

    // Preference
    mPreference = 0.0;
    // Timing
    mDelayList = {1.0,1.0,1.0};
    mPipeInterval = 1;
    mPipeLatency = 0;
    mPipeLatencyList  = {};
    mPortStages = {};

    // Implementation style
    mImplStyle = "";
 
    // Description
    mDescription = "";
    // Target component
    mTargetComponent = "";

    // Lookup functions
    mDelayFunction = "";
    mLatencyFunction = "";
    mIntervalFunction = "";
    mResUsageFunction = "";
    mLegalityFunction = "";
    mPreferenceFunction = "";
    mGenerator = "";

    // clear();
}

Core::Core(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference): Core()
         //mId(id), mName(name), /*mDescription(description),*/
         ///*mImplStyle(style),*/ mDelay(delay), mDelayFunction(delayFunction),
         //mPipeLatency(latency), mLatencyFunction(LatencyFunction),
         //mResUsageFunction(resUsageFunction),
         //mGenerator(generator), mPipeInterval(interval), mIntervalFunction(intervalFunction),
         //mIsHidden(isHidden), mLegalityFunction(legalityFunction), mPreference(preference)
        {
            mId = id;
            mName = name;
            mDelayList = {delay, delay, delay};
            mDelayFunction = delayFunction;
            mPipeLatency = latency;
            mLatencyFunction = LatencyFunction;
            mResUsageFunction = resUsageFunction;
            mGenerator = generator;
            mPipeInterval = interval;
            mIntervalFunction = intervalFunction;
            mIsHidden = isHidden;
            mLegalityFunction = legalityFunction;
            mPreference = preference;
            // operations: string to OperSet
            for(const auto& opStr : getTclList(opers, " {}"))
            {
                auto pb = platform::PlatformBasic::getInstance();
                OperType opcode = static_cast<OperType>(pb->getOpFromName(opStr));
                assert(Core::isValidOper(opcode) || opStr == "all");
                if (opStr == "all") {
                    mOperSet.insert(platform::AnyOperation);
                } else {
                    mOperSet.insert(opcode);
                }
            }
            // resource: string to map
            // if is str, it's type; if is digit, it's num of previous type
            auto wordList = getTclList(resource, " {}");
            for(int i = 0; i < wordList.size(); ++i)
            {
                // default is 1.0 from ResLibCmd.cpp
                if(isNumber(wordList[i]))
                {
                    assert(i);
                    mInitResUsage[wordList[i - 1]] = std::stoi(wordList[i]);
                }
                else
                {
                    mInitResUsage[wordList[i]] = 1.0;
                }
            }
            mDescription = description;
            configImplStyle(style);
        }

// Core::~Core()
// {
//     clear();
// }


// void Core::clear()
// {
//     mOperSet.clear();
//     mIfModes.clear();
//     mInitResUsage.clear();
//     mPortNameMap.clear();
// }

std::vector<std::string> Core::getTclList(const std::string& str, const std::string& sep)
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

bool Core::isNumber(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

bool Core::isValidOper(OperType opc)
{
    if (opc == AnyOperation)
        return true;

    return (opc > 0 && opc != platform::PlatformBasic::OP_UNSUPPORTED);
}


bool Core::matchOper(OperType opcode) const
{
    if (opcode == AnyOperation)
        return true;

    return (mOperSet.count(opcode) > 0);
}

PlatformBasic::IMPL_TYPE Core::getImpl() const
{
    if(isGeneric())
    {
        return PlatformBasic::UNSUPPORTED;
    }
    auto platformBasic = PlatformBasic::getInstance();
    auto opImplPairs = platformBasic->getOpImplFromCoreName(getName());
    assert(!opImplPairs.empty());
    // all impls are the same one, get first one is OK.
    return opImplPairs[0].second;
}

bool Core::matchInterfaceMode(InterfaceMode mode) const
{
    if (mIfModes.empty()) // Auto mode
        return false;

    return (mIfModes.count(mode) > 0);
}

double Core::getInitResourceUsageByName(string res_name) const
{
    if (mInitResUsage.count(res_name) <= 0)
        return 0;

    return mInitResUsage.find(res_name)->second;
}

const map<string, string>& Core::getPortNameMap() const
{
    return mPortNameMap;
}

bool Core::isBlackBoxIPCore() const
{
    if (mName == "BlackBox")
        return true;
    else
        return false;
}

unsigned Core::string2ResourceTp(std::string res_name) 
{
    if (res_name == "FF")
        return FF;
    if (res_name == "LUT")
        return LUT;
    if (res_name == "BRAM")
        return BRAM;
    if (res_name == "URAM")
        return URAM;
    if (res_name == "DSP")
        return DSP;
    // branch for unknown resource type
    return UNKNOWN;
}

Channel::Channel(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // Channel
         int inputs, int outputs, unsigned depth):
         Core(id, name, description, opers, style, delay, delayFunction, isDelayList,
         latency, LatencyFunction, resource, resUsageFunction,
         generator, interval, intervalFunction, isHidden, legalityFunction,
         preference)
        {
            mType = Core::Channel;
        }

Adapter::Adapter(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // adapter
         const std::string& interfaces):
         Core(id, name, description, opers, style, delay, delayFunction, isDelayList, 
         latency, LatencyFunction, resource, resUsageFunction,
         generator, interval, intervalFunction, isHidden, legalityFunction,
         preference)
         /*mIfModes(interfaces)*/
        {
            mType = Core::Adapter;
            // interface: string to InterfaceModes
            for(const auto& modeStr : getTclList(interfaces, " {}"))
            {
                InterfaceMode mode = IOModeType::string2Mode(modeStr);
                assert(mode > 0);
                mIfModes.insert(mode);
            } 
        } 

// Adapter::~Adapter()
// {

// }

IPBlock::LogicType IPBlock::string2LogicType(string logic_str)
{
    static const char* StrSync = "SYNC";
    static const char* StrAsync = "ASYNC";
    std::transform(logic_str.begin(), logic_str.end(),
                   logic_str.begin(), (int(*)(int))toupper);

    if (logic_str == StrSync)
        return LogicSync;
    else if (logic_str == StrAsync)
        return LogicAsync;

    return LogicUndef;
}

// IPBlock::~IPBlock()
// {
//     clear();
// }

IPBlock::IPBlock(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
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
         ):
         FuncUnit(id, name, description, opers, style, delay, delayFunction, isDelayList,
         latency, LatencyFunction, resource, resUsageFunction,
         generator, interval, intervalFunction, isHidden, legalityFunction,
         preference, inputs, outputs, portStageFunction),  
         mLogicType(string2LogicType(logic))
         /*mPortstageFunction(portStageFunction)*/
         /*mTargetComponent(targetComponent)*/
         {
            mType = Core::IPBlock;
            // expression: string to ExprTreeNode_shared_ptr 
            auto words = getTclList(expression);
            auto trees = getTrees(words.begin(), words.end());
            for(auto& tree : trees)
            {
                std::map<std::string, ExprTreeNode_shared_ptr> leafMap;
                auto root = buildTree(tree.begin(), tree.end(), leafMap);
                assert(root);
                assert(root->isBinaryOp());
                mExprTrees.push_back(root);
            }
            mTargetComponent = targetComponent;
            // mIsVivadoIP = false;
         }

std::vector<std::vector<std::string>> IPBlock::getTrees(InputIt first, InputIt last)
{
    std::vector<std::vector<std::string>> trees;
    int count = 0; // if '{' + 1; if '}' -1
    for(auto pos = first; pos != last; ++pos)
    {
        if(*pos == "{")
            ++count;
        else if(*pos == "}")
            --count;
        else if(count == 0)
        {
            // not in brace, treat it as a root node
            std::vector<std::string> tree;
            tree.push_back(*pos);
            trees.push_back(tree);
            first = pos + 1;
            continue;
        }

        if(count == 0 && pos != first)
        {
            std::vector<std::string> tree;
            for(auto it = first; it <= pos; ++it)
                tree.push_back(*it);
            trees.push_back(tree);
            first = pos + 1;
        }
    }
    return trees;
}

ExprTreeNode_shared_ptr IPBlock::buildTree(InputIt first, InputIt last, std::map<std::string, ExprTreeNode_shared_ptr>& leafMap)
{
    auto size = std::distance(first, last);
    ExprTreeNode_shared_ptr root = NULL;
    if(size == 1)
    {
        auto it = leafMap.find(*first);
        if(it != leafMap.end())
        {
            root = it->second;
        }
        else
        {
            root = std::make_shared<ExprTreeNode>(*first);
            leafMap[*first] = root;
        }
    }
    else if(size >= 3)
    {
        if(*first == "{")
        {
            --last;
            ++first;
            assert(*last == "}");
            root = buildTree(first, last, leafMap);
        }
        else
        {
            auto opName = *first;
            auto pb = platform::PlatformBasic::getInstance();
            OperType opCode = static_cast<OperType>(pb->getOpFromName(opName));
            root = std::make_shared<ExprTreeNode>(opName, opCode);
            auto children = getTrees(++first, last);
            assert(children.size() == 2);
            root->mLHS = buildTree(children[0].begin(), children[0].end(), leafMap);
            root->mRHS = buildTree(children[1].begin(), children[1].end(), leafMap);
        }
    }
    else
    {
        assert(0);
    }
    return root;
}

bool IPBlock::matchOper(OperType op) const
{
    if (Core::matchOper(op))
        return true;
    else 
        return false;
    
}


ExprTreeNode_shared_ptr IPBlock::getExpression(unsigned i) const
{
    if (i >= mExprTrees.size())
        return 0;
    return mExprTrees[i];
}

const vector<ExprTreeNode_shared_ptr>& IPBlock::getExpressions() const
{
    return mExprTrees;
}


// Storage::~Storage()
// {
//     clear();
// }

Storage::Storage(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
         const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
         int latency, const std::string& LatencyFunction, 
         const std::string& resource, const std::string& resUsageFunction,
         const std::string& generator, int interval, const std::string& intervalFunction,
         bool isHidden, const std::string& legalityFunction, double preference,
         // storage
         int depth, const std::string& interfaces, const std::string& ports):
         Core(id, name, description, opers, style, delay, delayFunction, isDelayList, 
         latency, LatencyFunction, resource, resUsageFunction,
         generator, interval, intervalFunction, isHidden, legalityFunction,
         preference) /*mDepth(depth), mIfModes(interfaces)*/
        {
            mType = Core::Storage;
            // interface: string to InterfaceModes
            for(const auto& modeStr : getTclList(interfaces, " {}"))
            {
                InterfaceMode mode = IOModeType::string2Mode(modeStr);
                assert(mode > 0);
                mIfModes.insert(mode);
            }
            // ports: string to memory ports
            auto portVec = getTclList(ports, " {}");
            for(int i = 0; i < portVec.size(); ++i)
            {
                std::string portName = "p" + std::to_string(i);    
                auto dir = MemPort::string2DirType(portVec[i]);
                assert(dir != CPort::CPortEnd);
                mMemPorts.push_back(MemPort(portName, dir, 1));
            }
        }

// void Storage::clear()
// {
//     ///mMemPorts.clear();
// }

PlatformBasic::MEMORY_TYPE Storage::getMemoryType() const
{
    auto platformBasic = PlatformBasic::getInstance();
    auto opImpls = platformBasic->getOpImplFromCoreName(getName());
    assert(!opImpls.empty());
    return platformBasic->getCoreFromOpImpl(opImpls[0].first, opImpls[0].second)->getMemoryType();
}

PlatformBasic::MEMORY_IMPL Storage::getMemoryImpl() const
{
    auto platformBasic = PlatformBasic::getInstance();
    auto opImpls = platformBasic->getOpImplFromCoreName(getName());
    assert(!opImpls.empty());
    return platformBasic->getCoreFromOpImpl(opImpls[0].first, opImpls[0].second)->getMemoryImpl();
}

bool Storage::isReadOnly() const
{
    if (getOpers().size() != 1)
        return false;

    // operation = {load}
    return matchOper(PlatformBasic::OP_LOAD);
}

bool Storage::isROM() const
{
    if (isNPRAM())
        return false;

    // Note: Need other check?
    return isReadOnly();
}

bool Storage::isNPROM() const
{
    return (this->getImpl() == PlatformBasic::ROM_AUTO ||
            this->getImpl() == PlatformBasic::ROM_NP_LUTRAM ||
            this->getImpl() == PlatformBasic::ROM_NP_BRAM);
}

bool Storage::isRAM() const
{
    if (isNPRAM())
        return false;

    if (isRegister())
        return false;

    if (getOpers().size() != 2)
        return false;

    // operation = {load store}
    return (matchOper(PlatformBasic::OP_LOAD) &&
            matchOper(PlatformBasic::OP_STORE));
}

bool Storage::isNPRAM() const
{
    return (this->getImpl() == PlatformBasic::NP_MEMORY ||
            this->getImpl() == PlatformBasic::NP_MEMORY_LUTRAM ||
            this->getImpl() == PlatformBasic::NP_MEMORY_BRAM ||
            this->getImpl() == PlatformBasic::NP_MEMORY_URAM);
}


bool Storage::isRegister() const
{
   //return (hasFixedDepth() && getDepth() == 1 && !isReadOnly());
   // FIXME, remove concept of "fixed"
   return getName() == "Register";
}

bool Storage::isShiftMem() const
{
    if (isNPRAM())
        return false;

    return matchOper(PlatformBasic::OP_MEMSHIFTREAD);
}

bool Storage::is1wnrRAM() const
{
    return (this->getImpl() == PlatformBasic::RAM_1WNR ||
            this->getImpl() == PlatformBasic::RAM_1WNR_LUTRAM ||
            this->getImpl() == PlatformBasic::RAM_1WNR_BRAM ||
            this->getImpl() == PlatformBasic::RAM_1WNR_URAM);
}

bool Storage::isEccRAM() const
{
    return (this->getImpl() == PlatformBasic::RAM_S2P_BRAM_ECC ||
            this->getImpl() == PlatformBasic::RAM_S2P_URAM_ECC);
}

bool Storage::isFifo() const
{
    return (mIfModes.count(IOModeType::ModeFifo) > 0);
}

// FuncUnit::~FuncUnit()
// {
//     clear();
// }


// void FuncUnit::clear()
// {
//     mInPorts.clear();
//     mOutPorts.clear();
// }


FuPort* FuncUnit::getPortByIndex(CPortList& port_list, unsigned index)
{
    assert (index <= port_list.size());
    return static_cast<FuPort*>(&port_list[index]);
}


FuPort* FuncUnit::getInPortByIndex(unsigned index)
{
    return getPortByIndex(mInPorts, index);
}


FuPort* FuncUnit::getOutPortByIndex(unsigned index)
{
    return getPortByIndex(mOutPorts, index);
}

} // namespace platform
