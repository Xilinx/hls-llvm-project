

#ifndef _PLATFORM_SqliteSelector_H
#define _PLATFORM_SqliteSelector_H
#include <map>
#include <string>
#include <vector>
#include <utility>

// key: latency or bitwidth, value: delay list
typedef std::map<int, std::vector<double>> DelayMap;
struct sqlite3;

namespace platform
{

// th definition of one Core, for ResLib
struct CoreDef
{
    std::string name;
    std::string description;
    std::string type;
    std::string opers;
    std::string style;
    double delay;
    std::string delayFunction;
    bool isDelayList;
    int latency;
    std::string latencyFunction;
    std::string resourceMap;
    std::string usageFunction;
    std::string generator;
    int interval;
    std::string intervalFunction;
    bool isHidden;
    std::string legalityFunction;
    double preference;
    // strorage
    int depth;
    std::string ifModes; // FIFO
    std::string memPorts;
    // functional_unit
    int inputs;
    int outputs;
    // ip_block
    std::string expression;
    std::string logic;
    std::string portStageFunction;
    std::string targetComponent;
    std::string impl;
    // connector, inputs, outputs
    // adapter, interface
};


class Selector
{
private:
    Selector() = default;
    ~Selector();
public:
    static Selector& getSelector() { static Selector s; return s; }
    bool init(const std::string& dbPath); 
    int selectInt(const char* cmd);
    double selectDouble(const char* cmd);
    std::string selectString(const char* cmd);
    std::vector<double> selectDoubleList(const char* cmd);
    std::vector<int> selectIntList(const char* cmd);
    DelayMap selectDelayMap(const char* cmd);
    std::map<int, int> selectInt2IntMap(const char* cmd);
    std::map<std::string, std::string> selectStr2StrMap(const char* cmd);
    std::pair<int, int> selectIntPair(const char* cmd);

    // read core definitions
    std::vector<CoreDef*> selectCoreDefs(const char* cmd);
    // read 
    std::vector<double> selecDSPDelayList(const char* cmd);
    std::vector<int> selectDSPPortList(const char* cmd);
    std::vector<std::vector<int>> selectInt2dList(const char* cmd);

    bool isExistTable(const char* tableName);
    std::map<std::string, double> selectStr2DoubleMap(const char* cmd);

private:
    sqlite3* mDb;
};

}
#endif
