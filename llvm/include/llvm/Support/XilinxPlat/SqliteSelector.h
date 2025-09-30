// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689

#ifndef _PLATFORM_SqliteSelector_H
#define _PLATFORM_SqliteSelector_H
#include <map>
#include <string>
#include <vector>
#include <utility>

// key: latency or bitwidth, value: delay list
typedef std::map<int, std::vector<double>> DelayMap;
struct sqlite3;
struct sqlite3_stmt;

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

struct Core1DParam{
    std::string name;
    int latency;
    std::string kind;
    double xx;
    double log;
    double x;
    double intercept;
};

struct Core2DParam {
    std::string name;
    int latency;
    std::string kind;
    double x0x0;
    double logx0;
    double x0;
    double x1x1;
    double logx1;
    double x1;
    double x0x1;
    double intercept;
};

template <typename T>
struct DataOr {
    bool valid;
    T data;
};


class Selector
{
private:
    Selector() = default;
    ~Selector();
public:
    static Selector& getSelector() { static Selector s; return s; }
    int init(const std::string& dbPath); 
    DataOr<int> selectInt(const char* cmd);
    DataOr<double> selectDouble(const char* cmd);
    std::string selectString(const char* cmd);
    std::vector<double> selectDoubleList(const char* cmd);
    std::vector<int> selectIntList(const char* cmd);
    DelayMap selectDelayMap(const char* cmd);
    std::map<int, int> selectInt2IntMap(const char* cmd);
    std::map<std::string, std::string> selectStr2StrMap(const char* cmd);
    std::pair<int, int> selectIntPair(const char* cmd);
    std::pair<std::string, std::string> selectStrPair(const char* cmd);

    // read core definitions
    std::vector<CoreDef*> selectCoreDefs(const char* cmd);
    // read 
    std::vector<double> selecDSPDelayList(const char* cmd);
    std::vector<int> selectDSPPortList(const char* cmd);
    std::vector<std::vector<int>> selectInt2dList(const char* cmd);

    bool isExistTable(const char* tableName);
    std::map<std::string, double> selectStr2DoubleMap(const char* cmd);
    std::vector<CoreBasicDef*> selectCoreBasics(std::string cmd);
    std::map<int, std::string> selectEncode(std::string cmd);
    std::map<std::string, std::string> selectAliasCores(std::string cmd);
    std::vector<Core1DParam> selectCore1DParams(std::string cmd);
    std::vector<Core2DParam> selectCore2DParams(std::string cmd);

private:
    std::string safe_get_string(sqlite3_stmt* ppStmt, int col);
    sqlite3* mDb;
};

}
#endif
