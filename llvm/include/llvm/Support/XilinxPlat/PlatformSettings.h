#ifndef _PLATFORM_PlatformSettings_H
#define _PLATFORM_PlatformSettings_H

#include <string> 
#include <vector>

namespace platform {

using OperType = int;
class IPBlockInst;

struct BlackBoxInfo {
    int pipeInterval;
    double inputPortDelay;
    double outputPortDelay;
    int inputPortStage;
    int outputPortStage;
};

class PFSettings {
public:
    using GetBlackBoxInfoFromJson_t = BlackBoxInfo(*)(const std::string&, const int portId );
    using SendInvalidLatencyMsgFunction = void(*)(const std::string&, int, 
                            const std::string&, const std::string&, const std::string&);
    using RawLatencyChangedMsgFunction = void(*)(int, int, const std::string&);
    static PFSettings& getInstance() {
        static PFSettings pfSettings;
        return pfSettings;
    }

    void setBlackBoxInfoFunction(GetBlackBoxInfoFromJson_t func) { mGetIntervalFromJson = func; }
    GetBlackBoxInfoFromJson_t getBlackBoxInfoFunction() { return mGetIntervalFromJson; }

    void setSendInvalidLatencyMsgFunc(SendInvalidLatencyMsgFunction func) { mSendInvalidLatencyMsgFunc = func; }
    SendInvalidLatencyMsgFunction getSendInvalidLatencyMsgFunc() { return mSendInvalidLatencyMsgFunc; }

    void setRawLatencyChangedMsgFunc(RawLatencyChangedMsgFunction func) { mRawLatencyChangedMsgFunc = func; }
    RawLatencyChangedMsgFunction getRawLatencyChangedMsgFunc() { return mRawLatencyChangedMsgFunc; }

    void setRamFifoDefaultRawLatency(int lat) { mRamFifoDefaultRawLatency = lat; }
    int getRamFifoDefaultRawLatency() { return mRamFifoDefaultRawLatency; }

    void setSrlFifoDefaultRawLatency(int lat) { mSrlFifoDefaultRawLatency = lat; }
    int getSrlFifoDefaultRawLatency() { return mSrlFifoDefaultRawLatency; }
private:
    GetBlackBoxInfoFromJson_t mGetIntervalFromJson = nullptr;
    SendInvalidLatencyMsgFunction mSendInvalidLatencyMsgFunc = nullptr; 
    RawLatencyChangedMsgFunction mRawLatencyChangedMsgFunc = nullptr;
    int mRamFifoDefaultRawLatency = 2;
    int mSrlFifoDefaultRawLatency = 1;
};

}

#endif      // end of _PLATFORM_PlatformSettings_H

