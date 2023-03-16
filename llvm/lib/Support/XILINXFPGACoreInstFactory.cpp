// (C) Copyright 2016-2022 Xilinx, Inc.
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

#include <iomanip>
#include <iterator>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sqlite3.h>
#include <utility>

#include "llvm/Support/XILINXFPGAPlatformBasic.h"
#include "llvm/Support/XILINXFPGACoreInstFactory.h"

using std::string;
using std::map;
using std::pair;
using std::vector;
using std::set;

namespace pf_newFE
{
// max length of SQL command string
const int MAX_CMD_SIZE = 200;

static std::string default_db_file = "";
void SetPlatformDbFile( std::string db_file) 
{
    default_db_file = db_file;
}

namespace  // for SqliteSelector & Querier 
{
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

class CoreQuerier;

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
}; // end struct CoreDef

class Selector
{
public:
    static Selector& getInstance() { static Selector s; return s; }

private:
    Selector()
    {
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

    ~Selector()
    {
        sqlite3_close(mDb);
    }

    std::string safe_get_string (sqlite3_stmt* ppStmt, int col)
    {
        const unsigned char* result = sqlite3_column_text(ppStmt, col);
        std::string value;
        if(result)
        {   
            value = reinterpret_cast<const char*>(result);
        }
        return value;
    }
    
    std::string complete_cmd(const char* cmd)
{
    std::string libName = CoreInstFactory::getInstance()->getName();

    int length = std::strlen(cmd) + libName.size();
    char* c_cmd = new char[length];
    int n = snprintf(c_cmd, length, cmd, libName.c_str());
    assert(n > 0 && n < length);

    std::string rst = std::string(c_cmd);
    delete[] c_cmd;
    return rst;
}



private: 
    sqlite3* mDb;

public: 
    std::vector<CoreDef*> selectCoreDefs(const char* cmd)
    {
        std::vector<CoreDef*> definitions;
        sqlite3_stmt* ppStmt;
        const char* pzTail;
        int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
        assert(rc == SQLITE_OK);

        while(sqlite3_step(ppStmt) == SQLITE_ROW)
        {
            auto core = new CoreDef();
            definitions.push_back(core);

            core->name           = safe_get_string(ppStmt, 0);
            core->description    = safe_get_string(ppStmt, 1);
            core->type           = safe_get_string(ppStmt, 2);
            core->opers          = safe_get_string(ppStmt, 3);
            core->style          = safe_get_string(ppStmt, 4);
            core->delay          = sqlite3_column_double(ppStmt, 5);
            core->delayFunction  = safe_get_string(ppStmt, 6);
            core->isDelayList    = sqlite3_column_int(ppStmt, 7); 
            core->latency        = sqlite3_column_int(ppStmt, 8);
            core->latencyFunction= safe_get_string(ppStmt, 9);
            core->resourceMap    = safe_get_string(ppStmt, 10);
            core->usageFunction  = safe_get_string(ppStmt, 11);
            core->generator      = safe_get_string(ppStmt, 12);
            core->interval       = sqlite3_column_int(ppStmt, 13);
            core->intervalFunction=safe_get_string(ppStmt, 14);
            core->isHidden       = sqlite3_column_int(ppStmt, 15);
            core->legalityFunction=safe_get_string(ppStmt, 16);
            core->preference     = sqlite3_column_double(ppStmt, 17);
            core->depth          = sqlite3_column_int(ppStmt, 18);
            core->memPorts       = safe_get_string(ppStmt, 19);
            core->ifModes        = safe_get_string(ppStmt, 20);
            core->inputs         = sqlite3_column_int(ppStmt, 21);
            core->outputs        = sqlite3_column_int(ppStmt, 22);
            core->expression     = safe_get_string(ppStmt, 23);
            core->logic          = safe_get_string(ppStmt, 24);
            core->portStageFunction=safe_get_string(ppStmt, 25);
            core->targetComponent = safe_get_string(ppStmt, 26);
            core->impl = safe_get_string(ppStmt, 28);
        }
        sqlite3_finalize(ppStmt); 
        assert(!definitions.empty());
        return definitions;
    }
    
    std::string selectString(const char* cmd)
    {
        sqlite3_stmt *ppStmt;
        const char *pzTail;
        int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
        // if(rc), message out and exit.
        assert(rc == SQLITE_OK);
        sqlite3_step(ppStmt);
        assert(sqlite3_column_count(ppStmt) == 1);
        const unsigned char* result = sqlite3_column_text(ppStmt, 0);
        std::string value;
        if(result)
        {   
            value = reinterpret_cast<const char*>(result);
        }
        sqlite3_finalize(ppStmt);
    
        return value;
    }

    std::vector<int> selectIntList(const char* cmd)
    {
        sqlite3_stmt *ppStmt;
        const char *pzTail;
        std::string c_cmd = complete_cmd(cmd);
        int rc = sqlite3_prepare_v2(mDb, c_cmd.c_str(), -1, &ppStmt, &pzTail);
        // if(rc), message out and exit.
        assert(rc == SQLITE_OK);
        assert(sqlite3_column_count(ppStmt) == 1);
        std::vector<int> values;
        while(sqlite3_step(ppStmt) == SQLITE_ROW)
        {
            values.push_back(sqlite3_column_int(ppStmt, 0));
        }
        sqlite3_finalize(ppStmt); 
        assert(!values.empty());
    
        return values;
    }

    DelayMap selectDelayMap(const char* cmd)
    {
        sqlite3_stmt *ppStmt;
        const char *pzTail;
        std::string c_cmd = complete_cmd(cmd);
        int rc = sqlite3_prepare_v2(mDb, c_cmd.c_str(), -1, &ppStmt, &pzTail);
        // if(rc), message out and exit.
        assert(rc == SQLITE_OK);
        DelayMap values;
        while(sqlite3_step(ppStmt) == SQLITE_ROW)
        {
            int first = sqlite3_column_int(ppStmt, 0);
            int delayNum = sqlite3_column_count(ppStmt) - 1;
            assert(delayNum);
            std::vector<double> second;
            for(int i = 1; i <= delayNum; ++i)
            {
                double value = sqlite3_column_double(ppStmt, i);
                if(value > 0)
                {
                    second.push_back(value);
                }
            }
            values.insert(std::make_pair(first, second));
        }
        sqlite3_finalize(ppStmt);
    
        return values;
    }
    
    std::map<int, int> selectInt2IntMap(const char* cmd)
    {
        sqlite3_stmt *ppStmt;
        const char *pzTail;
        std::string c_cmd = complete_cmd(cmd);
        int rc = sqlite3_prepare_v2(mDb, c_cmd.c_str(), -1, &ppStmt, &pzTail);
        // if(rc), message out and exit.
        assert(rc == SQLITE_OK);
        assert(sqlite3_column_count(ppStmt) == 2);
        std::map<int, int> values;
        while(sqlite3_step(ppStmt) == SQLITE_ROW)
        {
            int first = sqlite3_column_int(ppStmt, 0);
            int second = sqlite3_column_int(ppStmt, 1);
            values.insert(std::make_pair(first, second));
        }
        sqlite3_finalize(ppStmt);
    
        return values; 
    }

    int selectInt(const char* cmd)
    {
        sqlite3_stmt *ppStmt;
        const char *pzTail;
        std::string c_cmd = complete_cmd(cmd);
        int rc = sqlite3_prepare_v2(mDb, c_cmd.c_str(), -1, &ppStmt, &pzTail);
        // if(rc), message out and exit.
        assert(rc == SQLITE_OK);
        sqlite3_step(ppStmt);
        assert(sqlite3_column_count(ppStmt) == 1);
        int value = sqlite3_column_int(ppStmt, 0);
        sqlite3_finalize(ppStmt);
    
        return value;
    }

    std::pair<int, int> selectIntPair(const char* cmd)
    {
        sqlite3_stmt *ppStmt;
        const char *pzTail;
        std::string c_cmd = complete_cmd(cmd);
        int rc = sqlite3_prepare_v2(mDb, c_cmd.c_str(), -1, &ppStmt, &pzTail);
        // if(rc), message out and exit.
        assert(rc == SQLITE_OK);
        assert(sqlite3_column_count(ppStmt) == 2);
        std::pair<int, int> values(0, 0);
        while(sqlite3_step(ppStmt) == SQLITE_ROW)
        {
            int first = sqlite3_column_int(ppStmt, 0);
            int second = sqlite3_column_int(ppStmt, 1);
            values = std::make_pair(first, second);
        }
        sqlite3_finalize(ppStmt);
    
        return values; 
    }

    std::vector<double> selectDoubleList(const char* cmd)
    {
        sqlite3_stmt *ppStmt;
        const char *pzTail;
        std::string c_cmd = complete_cmd(cmd);
        int rc = sqlite3_prepare_v2(mDb, c_cmd.c_str(), -1, &ppStmt, &pzTail);
        // if(rc), message out and exit.
        assert(rc == SQLITE_OK);
        sqlite3_step(ppStmt);
        int delayNum = sqlite3_column_count(ppStmt);
        assert(delayNum);
        std::vector<double> values;
        for(int i = 0; i < delayNum; ++i)
        {
            double value = sqlite3_column_double(ppStmt, i);
            if(value > 0)
            {
                values.push_back(value);
            }
        }
        sqlite3_finalize(ppStmt);
    
        return values;
    }
}; // end class Selector

class QuerierFactory
{
public: 
    static QuerierFactory& getInstance() { static QuerierFactory qf; return qf; }

public: 
    CoreQuerier* getCoreQuerier(CoreInst* core) const;
    
    std::string getNameInDB(CoreInst* core) const; 
    
private:
    enum CORE_TYPE
    {
        ARITHMETIC = 0,     // for arithmetic
        MUX,                // for mux
        DOUBLE_KEY_MUX,     // for Multiplexer
        DIVNS,              // for DivnS, DivnS_SEQ
        DOUBLE_KEY_ARITHMETIC, // for verlsal add/sub, mul
        DOUBLE_KEY_DIVNS,      // for verlsal div
        DSP_USAGE, 
        MEMORY,            // common 1d memory database
        DOUBLE_KEY_MEMORY,  // 2d memory database 
        ADAPTER,
        FIFO,
        DOUBLE_KEY_FIFO
    };

private: 
    QuerierFactory(); 
    
    ~QuerierFactory(); 
    
private:
    std::string selectCoreType(const char* name) const; 
    bool hasCoreData(const char* name) const { return !selectCoreType(name).empty(); }

private:
    std::vector<CoreQuerier*> mQueriers;
}; // end class QuerierFactory

class CoreQuerier
{
    friend class QuerierFactory;

protected:
    CoreQuerier() = default;
    virtual ~CoreQuerier() = default;

    std::vector<int> getFuncUnitOperands(CoreInst* core)
    {
        std::vector<int> operands;
        auto fu = static_cast<FuncUnitInst*>(core);
        auto bwList = fu->getInputBWList();
   
        for (auto a: bwList) {
            operands.push_back(a);
        }

        return operands; 
    }

public:
    virtual int queryLatency(CoreInst* core) = 0;
    virtual std::vector<double> queryDelayList(CoreInst* core) = 0;
    virtual double queryResource(CoreInst* core, const std::string& resourceType) = 0;

protected:
    double interpolate(int index, int leftIndex, int rightIndex, 
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

    std::vector<double> interpolate(int index, int leftIndex, int rightIndex,
                                        const std::vector<double>& leftValue,
                                        const std::vector<double>& rightValue)
    {
        std::vector<double> rst;
        for (unsigned i = 0; i < leftValue.size() && i < rightValue.size(); ++i)
        {
            rst.push_back(interpolate(index, leftIndex, rightIndex,leftValue[i], rightValue[i]));
        }
        return rst;
    }

    DelayMap interpolate(int index, int leftIndex, int rightIndex,
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

    int getLatency(unsigned max_lat, double delay_budget,const DelayMap &lat_delay_map)
    {
        assert(!lat_delay_map.empty());
    
        int latency = 0;
        //if (user_lat < 0)
        //{
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
                //latency = lat_delay_map.rbegin()->first;
            }
        //}
        return latency;
    }

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

    template <class T>
    T getValue(const std::map<int, T> &tmap, int key)
    {
        T value{};
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
};

class AdapterQuerier : public CoreQuerier 
{
    friend class QuerierFactory;  

protected:
    AdapterQuerier() = default;
    ~AdapterQuerier() = default;

public: 
    virtual int queryLatency(CoreInst* core)
    {
        if (core->getName() == "s_axilite")
        {
            return 0;
        }
        else if (core->getName() == "m_axi")
        {
            //auto widthList = core->getWidthList();
            //unsigned user_dw = (widthList.size() >= 2) ? widthList[1] : 0;
            
            switch (core->getOp()) 
            {
                //case PlatformBasic::OP_READ     : return (user_dw >=128) ? 5 : 6 ; 
                //case PlatformBasic::OP_WRITE    : return (user_dw >=128) ? 5 : 6 ;
                //case PlatformBasic::OP_WRITEREQ : return (user_dw >=128) ? 4 : 5 ;
                //case PlatformBasic::OP_READREQ  : return (user_dw >=128) ? 4 : 5 ;
                case platform::PlatformBasic::OP_READ     : return 6 ; 
                case platform::PlatformBasic::OP_WRITE    : return 6 ;
                case platform::PlatformBasic::OP_WRITEREQ : return 5 ;
                case platform::PlatformBasic::OP_READREQ  : return 5 ;
                case platform::PlatformBasic::OP_WRITERESP: return 3 ;
                case platform::PlatformBasic::OP_ADAPTER  : return 0 ;
                default : assert(0); 
            }
        }
        return 0;
    }

    virtual std::vector<double> queryDelayList(CoreInst* core) 
    {
        if (core->getName() == "s_axilite" || core->getName() == "m_axi")
        {
            return { 1.0 };
        }
        return { 1.0 };
    }

    virtual double queryResource(CoreInst* core, const std::string& resourceType)
    {
        return 0;
    }
};


class SingleKeyQuerier: public CoreQuerier
{
    friend class QuerierFactory;
protected:
    SingleKeyQuerier() = default;
    ~SingleKeyQuerier() = default;

private:
    virtual int getKey(CoreInst* core) = 0;
    virtual const char* getKeyType() = 0;
    virtual const char* getTableName() = 0;
    virtual const char* getDelayColumn() = 0;

public:
    virtual int queryLatency(CoreInst* core)
    {
        int key = getKey(core);
        double delay_budget = core->getDelayBudget();
        unsigned maxLat = core->getMaxLatency();
        std::string nameDb = QuerierFactory::getInstance().getNameInDB(core);
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
    
            latency = getLatency(maxLat, delay_budget, intersection_map);
        }
        else
        {
            auto lat_delay_map = selectLatencyDelayMap(name_db, core->getImpl(), key);
            latency = getLatency(maxLat, delay_budget, lat_delay_map);
        }
    
        return latency; 
    }

    virtual std::vector<double> queryDelayList(CoreInst* core)
    {
        int key = getKey(core);
        int latency = SingleKeyQuerier::queryLatency(core);
        std::string name_db = QuerierFactory::getInstance().getNameInDB(core);
        auto map = selectKeyDelayMap(name_db.c_str(), latency);
        auto delayList = getValue<std::vector<double>>(map, key);
        if(core->getName() == "TAddSub")
        {
            for(unsigned i = 0; i < delayList.size(); ++i)
            {
                delayList[i] /= 2;
            }
        }
        return delayList;
    }

    virtual double queryResource(CoreInst* core, const std::string& resourceType)
    {
        int key = getKey(core);
        int latency = SingleKeyQuerier::queryLatency(core);
        std::string name_db = QuerierFactory::getInstance().getNameInDB(core);
        auto map = selectKeyResourceMap(name_db.c_str(), latency, resourceType.c_str());
        double value = getValue<int>(map, key);
        return value;
    }

private:
    std::vector<int> selectKeyList(const char* name_db)
    {
        const char* table_name = getTableName();
        int length = MAX_CMD_SIZE + std::strlen(name_db) + std::strlen(table_name);
        char* cmd = new char[length];
        int n = snprintf(cmd,
                 length,
                 "select distinct(%s) from %s_%s where CORE_NAME = '%s' COLLATE NOCASE ",
                  getKeyType(), "%s", table_name, name_db);
        assert(n > 0 && n < length);
        auto& s = Selector::getInstance();
        auto keys = s.selectIntList(cmd);
    
        delete [] cmd;
        return keys;
    }
    
    DelayMap selectLatencyDelayMap(const char *name_db,
                                   platform::PlatformBasic::IMPL_TYPE impl,
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
                 delay_column, "%s", table_name, getKeyType(), key, name_db);
        assert(n > 0 && n < length);
    
        if((std::strcmp(name_db, "Adder") == 0 && impl == platform::PlatformBasic::FABRIC_COMB) ||    // AddSub
           (std::strcmp(name_db, "Multiplier") == 0 && impl == platform::PlatformBasic::AUTO_COMB) || // Mul
           (std::strcmp(name_db, "Multiplexer") == 0 && impl == platform::PlatformBasic::AUTO_MUX))   // Mux
        {
            strcat(cmd, " and LATENCY = 0");
        }
        else if((std::strcmp(name_db, "Adder") == 0 && impl == platform::PlatformBasic::FABRIC_SEQ) ||  // AddSubnS
                (std::strcmp(name_db, "Multiplier") == 0 && impl == platform::PlatformBasic::AUTO_PIPE)) // MulnS
        {
            strcat(cmd, " and LATENCY > 0");
        }
        auto& s = Selector::getInstance();
        auto values = s.selectDelayMap(cmd);
        delete [] cmd;
        return values;
    }
    
    DelayMap selectKeyDelayMap(const char *name_db,
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
                 getKeyType(), delay_column, "%s", table_name, latency, name_db);
        assert(n > 0 && n < length);
        auto& s = Selector::getInstance();
        auto values = s.selectDelayMap(cmd);
        delete [] cmd;
        return values;
    }
    
    std::map<int, int> selectKeyResourceMap(const char *name_db,
                                                                 int latency,
                                                                 const char *value_type)
    {
        const char* table_name = getTableName();
        int length = MAX_CMD_SIZE + std::strlen(table_name) + std::strlen(name_db) + std::strlen(value_type);
        char* cmd = new char[length];
        int n = snprintf(cmd,
                 length,
                 "select %s, %s from %s_%s "
                 "where LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
                 getKeyType(), value_type, "%s", table_name, latency, name_db);
        assert(n > 0 && n < length);
        auto& s = Selector::getInstance();
        auto values = s.selectInt2IntMap(cmd);
        delete [] cmd;
        return values;
    }
};

class ArithmeticQuerier : public SingleKeyQuerier
{
    friend class QuerierFactory;
protected:
    ArithmeticQuerier() = default;
    ~ArithmeticQuerier() = default;

private:
    virtual const char* getKeyType() { return "OPERANDS"; }
    virtual const char* getTableName() { return "Arithmetic"; } 
    virtual const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; }

    virtual int getKey(CoreInst* core)
    {
        std::vector<int> operands;

        auto fu = static_cast<FuncUnitInst*>(core);
        auto bwList = fu->getInputBWList();

        for(unsigned bitwidth : bwList)
        {
            operands.push_back(static_cast<int>(bitwidth));
        }

        return operands.empty() ? 0 : (*std::max_element(operands.begin(), operands.end()));
    }
};

class MemoryQuerier : public SingleKeyQuerier
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

private:
    virtual const char* getKeyType() { return "BITWIDTH"; }
    virtual const char* getTableName() { return "Memory"; } 
    virtual const char* getDelayColumn() { return "DELAY"; }
    virtual int getKey(CoreInst* core) { return static_cast<StorageInst*>(core)->getBitWidth(); }
};

class FIFOQuerier: public MemoryQuerier 
{
    friend class QuerierFactory;
protected:
    FIFOQuerier() = default;
    ~FIFOQuerier() = default;

public:
    virtual double queryResource(CoreInst* core, const std::string& resourceType)
    {
        std::string coreName = core->getName();
        if((resourceType == "BRAM" && (coreName == "FIFO" || coreName == "FIFO_BRAM")) || (resourceType == "URAM" && coreName == "FIFO_URAM"))
        {
            return MemoryQuerier::queryResource(core, resourceType);
        }
        return SingleKeyQuerier::queryResource(core, resourceType);
    }

private:
    virtual int getKey(CoreInst* core) { return static_cast<StorageInst*>(core)->getDepth(); }
    virtual const char* getKeyType() { return "DEPTH"; }
    virtual const char* getTableName() { return "FIFO"; } 
    virtual const char* getDelayColumn() { return "DELAY"; }
};


class MuxQuerier: public SingleKeyQuerier
{
    friend class QuerierFactory;
protected:
    MuxQuerier() = default;
    ~MuxQuerier() = default;

private:
    virtual int getKey(CoreInst* core)
    {
        int inputs = 0; 
        auto fu = static_cast<FuncUnitInst*>(core);
        inputs = fu->getInputBWList().size() - 1;
        return inputs;
    }
    virtual const char* getKeyType() { return "INPUT_NUMBER"; }
    virtual const char* getTableName() { return "Mux"; } 
    virtual const char* getDelayColumn() { return "DELAY"; }
};

class DivnsQuerier : public ArithmeticQuerier 
{
    friend class QuerierFactory;
protected:
    DivnsQuerier() = default;
    ~DivnsQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core)
    {
        auto operands = CoreQuerier::getFuncUnitOperands(core);
        return operands[0] + 3;
    }
};

class DSPUsageQuerier : public ArithmeticQuerier
{
    friend class QuerierFactory;
protected:
    DSPUsageQuerier() = default;
    ~DSPUsageQuerier() = default;

public:
    virtual double queryResource(CoreInst* core, const std::string& resourceType)
    {
        if(resourceType != "DSP")
        {
            return ArithmeticQuerier::queryResource(core, resourceType);
        }
        return queryDSP(core);
    }
    unsigned queryDSP(CoreInst* core)
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
        if(input0 <= 4 || input1 <= 4)
        {
            return 0;
        }
        // proc y
        int baseUnit = 27;
        int shiftUnit = 23;
        int threshold4Fabric = 26;
        // proc u
        int msb0 = (input0 > baseUnit) ? ((input0 - baseUnit) % shiftUnit) : input0;
        int length0 = (input0 > baseUnit) ? (((input0 - baseUnit) + shiftUnit - 1) / shiftUnit + 1) : 1;
        int msb1 = (input1 > baseUnit) ? ((input1 - baseUnit) % shiftUnit) : input1;
        int length1 = (input1 > baseUnit) ? (((input1 - baseUnit) + shiftUnit - 1) / shiftUnit + 1) : 1;
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
};

class DoubleKeyQuerier : public CoreQuerier
{
    friend class QuerierFactory;
protected:
    DoubleKeyQuerier() = default;
    ~DoubleKeyQuerier() = default;

private: 
    virtual const char* getTableName() = 0;
    virtual const char* getDelayColumn() = 0;
    virtual int getKey0(CoreInst* core) = 0;
    virtual int getKey1(CoreInst* core) = 0;
    virtual const char* getKey0Type() = 0;
    virtual const char* getKey1Type() = 0;

public:
    virtual int queryLatency(CoreInst* core)
    {
        double delay_budget = core->getDelayBudget();
        unsigned maxLat = core->getMaxLatency();
        std::string name_db = QuerierFactory::getInstance().getNameInDB(core);
    
        auto lat_delay_map = selectLatencyDelayMap(name_db.c_str(), core->getImpl(), getKey0(core), getKey1(core));
        int latency = getLatency(maxLat, delay_budget, lat_delay_map);
        return latency;
    }

    std::vector<double> queryDelayList(CoreInst* core)
    {
        int latency = DoubleKeyQuerier::queryLatency(core);
        std::string name_db = QuerierFactory::getInstance().getNameInDB(core);

        return selectDelayList(name_db.c_str(), getKey0(core), getKey1(core), latency);   
    }

    virtual double queryResource(CoreInst* core, const std::string& resourceType)
    {
        int latency = DoubleKeyQuerier::queryLatency(core);
        std::string name_db = QuerierFactory::getInstance().getNameInDB(core);
        double value = selectResource(name_db.c_str(), getKey0(core), getKey1(core), latency, resourceType.c_str());
        return value;
    }

private:
    std::vector<double> selectDelayList(const char* core_name, int key0, int key1, int latency)
    {
        int length = MAX_CMD_SIZE + std::strlen(core_name);
        char* cmd = new char[length];
        auto keyPair = selectMinGEValue(core_name, getKey0Type(), key0, getKey1Type(), key1);
        int queryKey0 = keyPair.first;
        int queryKey1 = keyPair.second;
        int n = snprintf(cmd,
                 length,
                 "select %s from %s_%s "
                 "where "
                 "%s = %d and %s = %d and LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
                 getDelayColumn(), "%s", getTableName(), 
                 getKey0Type(), queryKey0, getKey1Type(), queryKey1, latency, core_name);
        assert(n > 0 && n < length);
        auto& s = Selector::getInstance();
        auto values = s.selectDoubleList(cmd);
        delete [] cmd;
        return values;
    }

    DelayMap selectLatencyDelayMap(const char* name_db, platform::PlatformBasic::IMPL_TYPE impl, int key0, int key1)
    {
        int length = MAX_CMD_SIZE + std::strlen(name_db);
        char* cmd = new char[length];
        auto keyPair = selectMinGEValue(name_db, getKey0Type(), key0, getKey1Type(), key1);
        int queryKey0 = keyPair.first;
        int queryKey1 = keyPair.second;
        int n = snprintf(cmd,
                 length,
                 "select LATENCY, %s from %s_%s "
                 "where "
                 "%s = %d and %s = %d and CORE_NAME = '%s' COLLATE NOCASE ",
                 getDelayColumn(), "%s", getTableName(), 
                 getKey0Type(), queryKey0, getKey1Type(), queryKey1, name_db);
        assert(n > 0 && n < length);
        if(std::strcmp(name_db, "Multiplexer") == 0 && impl == platform::PlatformBasic::AUTO_MUX)
        {
            strcat(cmd, " and LATENCY = 0");
        }
        auto& s = Selector::getInstance();
        auto values = s.selectDelayMap(cmd);
        delete [] cmd;
        return values;
    }

    int selectMinGEValue(const char* core_name, const char* column, int value)
    {
        int length = MAX_CMD_SIZE + std::strlen(core_name) + std::strlen(column);
        char* cmd = new char[length];
        int n = snprintf(cmd,
                 length,
                 "select min(%s) from %s_%s "
                 "where CORE_NAME = '%s' COLLATE NOCASE "
                 "and %s >= %d",
                 column, "%s", getTableName(), core_name, column, value);
        assert(n > 0 && n < length);
        auto& s = Selector::getInstance();
        int result = s.selectInt(cmd);
        // value is greater than max(column), then choose max(column)
        if(result == 0)
        {
            int n = snprintf(cmd,
                     length,
                     "select max(%s) from %s_%s "
                     "where CORE_NAME = '%s' COLLATE NOCASE ",
                     column, "%s", getTableName(), core_name);
            assert(n > 0 && n < length);
            result = s.selectInt(cmd);
        }
        delete [] cmd;
        return result;
    }

    std::pair<int, int> selectMinGEValue(const char *core_name, const char* column0, int value0, const char *column1, int value1)
    {
        value0 = selectMinGEValue(core_name, column0, value0);
        value1 = selectMinGEValue(core_name, column1, value1); 
        int length = MAX_CMD_SIZE + std::strlen(core_name) + std::strlen(column0)*3 + std::strlen(column1)*3;
        char* cmd = new char[length];
        int n = snprintf(cmd,
                 length,
                 "select %s, %s from %s_%s "
                 "where CORE_NAME = '%s' COLLATE NOCASE "
                 "and %s >= %d and %s >= %d "
                 "ORDER BY %s * %s ASC LIMIT 1",
                 column0, column1, "%s", getTableName(), core_name, 
                 column0, value0, column1, value1, column0, column1);
        assert(n > 0 && n < length);
        auto& s = Selector::getInstance();
        auto result = s.selectIntPair(cmd);
        // value is greater than max, then choose max
        if(result.first == 0)
        {
            int n = snprintf(cmd,
                    length,
                    "select %s, %s from %s_%s "
                    "where CORE_NAME = '%s' COLLATE NOCASE "
                    "ORDER BY %s * %s DESC LIMIT 1",
                    column0, column1, "%s", getTableName(), core_name,
                    column0, column1);
            assert(n > 0 && n < length);
            result = s.selectIntPair(cmd);
        }
        delete [] cmd;
        return result;
    }

protected:
    int selectResource(const char* core_name, int key0, int key1, int latency, const char* value_type)
    {
        int length = MAX_CMD_SIZE + std::strlen(core_name);
        char* cmd = new char[length];
        auto keyPair = selectMinGEValue(core_name, getKey0Type(), key0, getKey1Type(), key1);
        int queryKey0 = keyPair.first;
        int queryKey1 = keyPair.second;
        int n = snprintf(cmd,
                 length,
                 "select %s from %s_%s "
                 "where "
                 "%s = %d and %s = %d and LATENCY = %d and CORE_NAME = '%s' COLLATE NOCASE ",
                 value_type, "%s", getTableName(), 
                 getKey0Type(), queryKey0, getKey1Type(), queryKey1, latency, core_name);
        assert(n > 0 && n < length);
        auto& s = Selector::getInstance();
        int value = s.selectInt(cmd);
        delete [] cmd;
        return value;
    }
};

class DoubleKeyFIFOQuerier: public DoubleKeyQuerier 
{
    friend class QuerierFactory;
protected:
    DoubleKeyFIFOQuerier() = default;
    ~DoubleKeyFIFOQuerier() = default;

private:
    virtual int getKey0(CoreInst* core) { return static_cast<StorageInst*>(core)->getBitWidth(); }
    virtual int getKey1(CoreInst* core) { return static_cast<StorageInst*>(core)->getDepth(); }
    virtual const char* getKey0Type() { return "BITWIDTH"; }
    virtual const char* getKey1Type() { return "DEPTH"; }

    const char* getTableName() { return "2D_FIFO"; }
    const char* getDelayColumn() { return "DELAY"; }
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
    virtual int getKey0(CoreInst* core)
    {
        auto operands = CoreQuerier::getFuncUnitOperands(core);
        return *std::min_element(operands.begin(), operands.end());
    }
    virtual int getKey1(CoreInst* core)
    {
        auto operands = CoreQuerier::getFuncUnitOperands(core);
        return *std::max_element(operands.begin(), operands.end());
    }
    virtual const char*  getKey0Type() { return "OPERANDS0"; }
    virtual const char*  getKey1Type() { return "OPERANDS1"; }
};

class DoubleKeyMuxQuerier : public DoubleKeyQuerier 
{
    friend class QuerierFactory;
protected:
    DoubleKeyMuxQuerier() = default;
    ~DoubleKeyMuxQuerier() = default;
private:
    virtual int getKey0(CoreInst* core)
    {
        int width = 0; 
        auto fu = static_cast<FuncUnitInst*>(core);
        width = fu -> getOutputBW();
        return width;
    }
    virtual int getKey1(CoreInst* core)
    {
        int inputs = 0; 
        auto fu = static_cast<FuncUnitInst*>(core);
        inputs = fu -> getInputBWList().size() - 1;
        return inputs;
    }
    virtual const char* getKey0Type() { return "BITWIDTH"; }
    virtual const char* getKey1Type() { return "INPUT_NUMBER"; }

    const char* getTableName() { return "2D_Mux"; }
    const char* getDelayColumn() { return "DELAY"; }
};

class DoubleKeyMemoryQuerier : public DoubleKeyQuerier
{
    friend class QuerierFactory;
protected:
    DoubleKeyMemoryQuerier() = default;
    ~DoubleKeyMemoryQuerier() = default;
private: 
    virtual int getKey0(CoreInst* core) { return static_cast<StorageInst*>(core)->getBitWidth(); }
    virtual int getKey1(CoreInst* core) { return static_cast<StorageInst*>(core)->getDepth();};
    virtual const char* getKey0Type() { return "BITWIDTH"; };
    virtual const char* getKey1Type() { return "DEPTH"; }
    const char* getTableName() { return "2D_Memory"; }
    const char* getDelayColumn() { return "DELAY0, DELAY1, DELAY2"; };
};

class DoubleKeyDivnsQuerier : public DoubleKeyArithmeticQuerier
{
    friend class QuerierFactory;
protected:
    DoubleKeyDivnsQuerier() = default;
    ~DoubleKeyDivnsQuerier() = default;

public:
    virtual int queryLatency(CoreInst* core)
    {
        auto operands = CoreQuerier::getFuncUnitOperands(core);
        return operands[0] + 3;
    }
private:
    virtual int getKey0(CoreInst* core)
    {
        auto operands = CoreQuerier::getFuncUnitOperands(core);
        // 0: diveidend, 1: divisor, 2: output
        return operands[1];
    }
    virtual int getKey1(CoreInst* core)
    {
        auto operands = CoreQuerier::getFuncUnitOperands(core);
        // 0: diveidend, 1: divisor, 2: output
        return operands[0];
    }
};



// QuerierFactory
CoreQuerier* QuerierFactory::getCoreQuerier(CoreInst* core) const
{
    CoreQuerier* querier = NULL;
    std::string nameInDB = getNameInDB(core);
    std::string typeStr = selectCoreType(nameInDB.c_str());
    // Following cores can be auto-characterized, therefore assign its querier by its type, refer to CoreType table in SQLite DB.
    if(typeStr == "Arithmetic")
    {
        querier = mQueriers[ARITHMETIC];
        // Following special case exist for hard-code alogrithm, rather than pure data driven. 
        if (nameInDB == "Multiplier" || nameInDB == "Mul_DSP" ) 
        {
            querier = mQueriers[DSP_USAGE];
        } 
        else if (nameInDB == "Divider" || nameInDB == "DivnS_SEQ")
        {
            querier = mQueriers[DIVNS];
        }
    }
    else if(typeStr == "2D_Arithmetic")
    {
        querier = mQueriers[DOUBLE_KEY_ARITHMETIC];
        // Following special case exist for hard-code alogrithm, rather than pure data driven. 
        if (nameInDB == "Divider" || nameInDB == "DivnS_SEQ")
        {
            querier = mQueriers[DOUBLE_KEY_DIVNS];
        }
    }
    else if(typeStr == "Mux")
    {
        querier = mQueriers[MUX];
    }
    else if(typeStr == "2D_Mux")
    {
        querier = mQueriers[DOUBLE_KEY_MUX];
    }
    else if(typeStr == "Memory")
    {
        querier = mQueriers[MEMORY];
    }
    else if(typeStr == "2D_Memory")
    {
        querier = mQueriers[DOUBLE_KEY_MEMORY];
    }
    else if(typeStr == "FIFO")
    {
        querier = mQueriers[FIFO];   
    }
    else if(typeStr == "2D_FIFO")
    {
        querier = mQueriers[DOUBLE_KEY_FIFO];
    }
    
    if (nameInDB == "s_axilite" || nameInDB == "m_axi" || nameInDB == "axis" )
    {
        querier = mQueriers[ADAPTER];
    }
    
    return querier;
}

std::string QuerierFactory::getNameInDB(CoreInst* core) const 
{
    std::string coreName = core->getName();
    std::string nameInDB(coreName);
    if(core->getType() == CoreInst::Type::Storage)
    {
        auto storageInst = static_cast<StorageInst*>(core);
        if(nameInDB == "RAM_S2P_BRAM_ECC")
        {
            nameInDB = hasCoreData("BRAMECC") ? "BRAMECC" : "BRAM";
        }
        else if(nameInDB == "RAM_S2P_URAM_ECC")
        {
            nameInDB = hasCoreData("URAMECC") ? "URAMECC" : "URAM";
        }
        // old device has no FIFO_URAM data, use FIFO_BRAM instead
        else if(nameInDB == "FIFO_URAM")
        {
            nameInDB = hasCoreData("FIFO_URAM") ? "FIFO_URAM" : "FIFO_BRAM";
        }
        else if (storageInst->isBRAM())
        {
            nameInDB = hasCoreData("RAMBlock") ? "RAMBlock" : "BRAM";
        }
        else if (storageInst->isDRAM())
        {
            nameInDB = hasCoreData("RAMDistributed") ? "RAMDistributed" : "DRAM";
        }
        else if(storageInst->isURAM())
        {
            nameInDB = hasCoreData("RAMUltra") ? "RAMUltra" : "BRAM";
        }
        else if(storageInst->isVivadoAuto())
        {
            nameInDB = (storageInst->getBitWidth() * storageInst->getDepth() >= 1024) ? "BRAM" : "DRAM";
            if(hasCoreData("RAMVivadoDo"))
            {
                nameInDB = "RAMVivadoDo";
            }
        }
    }
    if(core->getType() == CoreInst::Type::FunctionUnit)
    {
        if(nameInDB == "Mux")
        {
          nameInDB = "Multiplexer";
        }
        else if ((core->getOp() == platform::PlatformBasic::OP_TYPE::OP_UREM || core->getOp() == platform::PlatformBasic::OP_TYPE::OP_UDIV) && nameInDB == "Divider_IP") 
        {
          nameInDB = "UnsignedDivider_IP";
        }
    }
    return nameInDB;
}

QuerierFactory::QuerierFactory() 
{
    mQueriers.push_back(new ArithmeticQuerier());
    mQueriers.push_back(new MuxQuerier());
    mQueriers.push_back(new DoubleKeyMuxQuerier());
    mQueriers.push_back(new DivnsQuerier());
    mQueriers.push_back(new DoubleKeyArithmeticQuerier());
    mQueriers.push_back(new DoubleKeyDivnsQuerier());
    mQueriers.push_back(new DSPUsageQuerier());
    mQueriers.push_back(new MemoryQuerier());
    mQueriers.push_back(new DoubleKeyMemoryQuerier());
    mQueriers.push_back(new AdapterQuerier());
    mQueriers.push_back(new FIFOQuerier());
    mQueriers.push_back(new DoubleKeyFIFOQuerier());
}

QuerierFactory::~QuerierFactory() 
{
    for (auto querier : mQueriers) 
    {
        delete querier;
    }
}

std::string QuerierFactory::selectCoreType(const char* name) const 
{
    int length = MAX_CMD_SIZE + std::strlen(name);
    char* cmd = new char[length];
    CoreInstFactory* cf = CoreInstFactory::getInstance();
    std::string libName = cf->getName();
    int n = snprintf(cmd, 
        length, 
        "select CORE_TYPE from %s_CoreType where CORE_NAME = '%s' COLLATE NOCASE ",
        libName.c_str(), name);
    assert(n > 0 && n < length);
    auto& s = Selector::getInstance();
    std::string value = s.selectString(cmd);
    delete [] cmd;
    return value;
}



} // end namespace


// CoreInst
CoreInst::CoreInst(pf_internal::Core* core, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl)
    : mCore(core),
    mResUsage(core->getInitResourceUsage()),
    mOp(op),
    mImpl(impl),
    mClock(-1)
{
}

CoreInst::~CoreInst()
{
}

std::string CoreInst::getName() const
{
    return mCore->getName();
}

platform::PlatformBasic::OP_TYPE CoreInst::getOp() const
{
    return mOp;
}

platform::PlatformBasic::IMPL_TYPE CoreInst::getImpl() const
{
    return mImpl;
}

CoreInst::Type CoreInst::getType() const
{
    auto type = mCore->getType();
    return static_cast<CoreInst::Type>(type);
}

float CoreInst::getPreference() const
{
    return mCore->getFixedPreference(mOp);
}

unsigned CoreInst::getId() const
{
    return mCore->getId();
}

unsigned CoreInst::getMaxLatency() const
{
    auto latencyRange = platform::PlatformBasic::getInstance()->verifyLatency(mOp, mImpl);
    return latencyRange.second;
}

// API
void CoreInst::configDelayBudget(double delayBudget)
{
    mClock = delayBudget;
}

double CoreInst::getDelayBudget()
{
    return mClock;
}

double CoreInst::getResourceUsageByName(std::string name)
{
    if(mResUsage.count(name) == 0)
    {
        return 0.0;
    }

    if(!mCore->hasResUsageFunction())
    {
        return mCore->getInitResourceUsageByName(name);
    }
    
    CoreQuerier* coreinstQuerier = QuerierFactory::getInstance().getCoreQuerier(this);
    return coreinstQuerier->queryResource(this, name);
}

// API
const ResUsageMap& CoreInst::getResourceUsage()
{
    for(std::pair<std::string, double> resUsage : mResUsage)
    {
        std::string res_name = resUsage.first;
        mResUsage[res_name] = getResourceUsageByName(res_name);
    }
    return mResUsage;
}

// API 
std::vector<double> CoreInst::getDelayList()
{
    if (getName() == "Divider_IP") 
    {
        FuncUnitInst* fu = static_cast<FuncUnitInst*>(this);        
        std::vector<unsigned> inputOperands = fu->getInputBWList();
        for (unsigned operands : inputOperands) {
            if (operands > 64) {
                std::vector<double> artificialDelayList = { 1000.0, 1000.0, 1000.0 };
                return artificialDelayList; 
           }
        }
    }

    if (!mCore->hasDelayFunction()) {
        return mCore->getFixedDelayList(mOp);
    }
    
    CoreQuerier* coreinstQuerier = QuerierFactory::getInstance().getCoreQuerier(this);
    auto delay = coreinstQuerier->queryDelayList(this);
      
    return delay;
}

// API
unsigned CoreInst::getPipeLatency()
{
    assert((getType() == Type::FunctionUnit || getType() == Type::Storage) && "getPipeLatency() should only be used by FunctionUnit and Storage");

    if (getName() == "Divider_IP") 
    {
        FuncUnitInst* fu = static_cast<FuncUnitInst*>(this);
        std::vector<unsigned> inputOperands = fu->getInputBWList();
        for (unsigned operands : inputOperands) {
            if (operands > 64) {
                return 1000;
            }
        }
    }

    if (!mCore->hasLatencyFunction()) {
        return  mCore->getFixedPipeLatency();
    }

    CoreQuerier* coreinstQuerier = QuerierFactory::getInstance().getCoreQuerier(this);
    unsigned latency = coreinstQuerier->queryLatency(this);
        
    return latency;
}

// API
unsigned CoreInst::getPipeInterval()
{
    if(getName() == "DivnS_SEQ") 
    {
        FuncUnitInst* fu = static_cast<FuncUnitInst*>(this);
        return fu->getOutputBW();
    }
    return 1;
}

// FuncUnitInst
FuncUnitInst::FuncUnitInst(pf_internal::FuncUnit* fu, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl) :
    CoreInst(fu, op, impl)
{}

FuncUnitInst::~FuncUnitInst() {}

void FuncUnitInst::configInputBWList(const std::vector<unsigned>& inputList) 
{
    mInputList = inputList;
}

std::vector<unsigned> FuncUnitInst::getInputBWList()
{
    return mInputList;
}

void FuncUnitInst::configOutputBW(const int bw)
{
    mOutput = bw;
}

int FuncUnitInst::getOutputBW()
{
    return mOutput;
}

// StorageInst
StorageInst::StorageInst(pf_internal::Storage* st, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl) : 
    CoreInst(st, op, impl)
{}

StorageInst::~StorageInst() {}

void StorageInst::configBitWidth(int bw)
{
    mBitWidth = bw;
}

int StorageInst::getBitWidth() const
{
    return mBitWidth;
}

void StorageInst::configDepth(int depth)
{
    mDepth = depth;
}

int StorageInst::getDepth() const 
{
    return mDepth;
}

void StorageInst::setMemUsedPorts(const std::vector<unsigned>& ports)
{
    std::vector<unsigned> usedPorts = ports;
    usedPorts.erase(std::remove_if(usedPorts.begin(), usedPorts.end(), [](unsigned i) { return i > MemoryQuerier::PORT_TYPE::READ_WRITE;}),usedPorts.end());
    mMemUsedPorts = usedPorts;
}

const std::vector<unsigned>& StorageInst::getMemUsedPorts() const
{
    return mMemUsedPorts;
}

platform::PlatformBasic::MEMORY_IMPL StorageInst::getMemoryImpl() const 
{
    auto storage = static_cast<pf_internal::Storage*>(mCore);
    return storage->getMemoryImpl();
}


bool StorageInst::isURAM() const
{
    return getMemoryImpl() == platform::PlatformBasic::MEMORY_IMPL::MEMORY_IMPL_URAM;
}

bool StorageInst::isBRAM() const
{
    return (getMemoryImpl() == platform::PlatformBasic::MEMORY_IMPL::MEMORY_IMPL_BLOCK ||
            getMemoryImpl() == platform::PlatformBasic::MEMORY_IMPL::MEMORY_IMPL_BRAM);
}

bool StorageInst::isDRAM() const
{
    return (getMemoryImpl() == platform::PlatformBasic::MEMORY_IMPL::MEMORY_IMPL_DISTRIBUTE ||
            getMemoryImpl() == platform::PlatformBasic::MEMORY_IMPL::MEMORY_IMPL_LUTRAM);
}

bool StorageInst::isVivadoAuto() const
{
    return getMemoryImpl() == platform::PlatformBasic::MEMORY_IMPL::MEMORY_IMPL_AUTO;
}

// AdapterInst
AdapterInst::AdapterInst(pf_internal::Adapter* ada, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl) :
    CoreInst(ada, op, impl)
{}

AdapterInst::~AdapterInst() {}

unsigned AdapterInst::getPipeLatency(OperType PTy)
{
    platform::PlatformBasic::OP_TYPE mOpBak = mOp;

    mOp = static_cast<platform::PlatformBasic::OP_TYPE>(PTy);

    unsigned latency;

    CoreQuerier* coreinstQuerier = QuerierFactory::getInstance().getCoreQuerier(this);
    latency = coreinstQuerier->queryLatency(this);

    mOp = mOpBak;

    return latency;
}

// CoreInstFactory
CoreInstFactory* CoreInstFactory::getInstance()
{
    static CoreInstFactory* p = NULL;
    if ( p ) {
        return p;
    } else {
        p = new CoreInstFactory();
        return p;  
    }
}

CoreInstFactory::CoreInstFactory() : mCoreId(0), mName("")
{ 
    clear();
}

CoreInstFactory::~CoreInstFactory() {

}

void CoreInstFactory::setName(std::string name) {
    mName = name;
}

std::string CoreInstFactory::getName() {
    return mName;
}

std::shared_ptr<CoreInst> CoreInstFactory::createCoreInst(pf_internal::Core* core, platform::PlatformBasic::OP_TYPE op, platform::PlatformBasic::IMPL_TYPE impl)
{
    if(core == 0) return 0;
    switch(core->getType())
    {
        case pf_internal::Core::Any:
        case pf_internal::Core::Generic:
            break;
        case pf_internal::Core::FunctionUnit:
            return std::shared_ptr<FuncUnitInst>(new FuncUnitInst(static_cast<pf_internal::FuncUnit*>(core), op, impl));
        case pf_internal::Core::Storage:
            return std::shared_ptr<StorageInst>(new StorageInst(static_cast<pf_internal::Storage*>(core), op, impl));
        case pf_internal::Core::Connector:
            break;
            //return std::shared_ptr<ConnectorInst>(new ConnectorInst(static_cast<pf_internal::Connector*>(core), op, impl));
        case pf_internal::Core::Channel:
            break;
            //return std::shared_ptr<ChannelInst>(new ChannelInst(static_cast<pf_internal::Channel*>(core), op, impl));
        case pf_internal::Core::IPBlock:
            break;
            //return std::shared_ptr<IPBlockInst>(new IPBlockInst(static_cast<pf_internal::IPBlock*>(core), op, impl));
        case pf_internal::Core::Adapter:
            return std::shared_ptr<AdapterInst>(new AdapterInst(static_cast<pf_internal::Adapter*>(core), op, impl));
    }

    return 0;
}

CoreInstList CoreInstFactory::createCoreInst(pf_internal::Core* core, OperType opcode)
{
    CoreInstList insts;
    auto pb = platform::PlatformBasic::getInstance();
    auto pairs = pb->getOpImplFromCoreName(core->getName());
    // if no Node::OP, use PlatformBasic::OP instead
    if(opcode == AnyOperation)
    {
        for(auto pair : pairs)
        {
            std::shared_ptr<CoreInst> newCoreInst = createCoreInst(core, pair.first, pair.second);
            insts.push_back(newCoreInst);
        }
    }
    else
    {
        assert(!pairs.empty());
        insts.push_back(createCoreInst(core, static_cast<platform::PlatformBasic::OP_TYPE>(opcode), pairs[0].second));
    }
    return insts;
}

// API
CoreInstList CoreInstFactory::getCoreInstsByOper(
    OperType opcode,
    int type_mask) const
{
    auto cores = getFuncCores(opcode, type_mask);
    CoreInstList coreInsts;
    for (auto& core: cores)
    {   
        auto insts = createCoreInst(core, opcode);
        coreInsts.insert(coreInsts.end(), insts.begin(), insts.end());
    }
    return coreInsts;
}

// API
bool CoreInstFactory::requestFuncUnitInstList(
    FuncUnitInstList& funcList, 
    OperType opcode, 
    double clock, 
    std::vector<unsigned>& inputBWList, 
    int outputBW) const
{
    CoreInstList coreList;
    coreList = getCoreInstsByOper(opcode);

    for(auto& core : coreList) 
    {
        auto func = dyn_cast<FuncUnitInst>(core);

        func->configDelayBudget(clock);
        func->configInputBWList(inputBWList);
        func->configOutputBW(outputBW);

        funcList.push_back(func);
    }

    return funcList.size() > 0;
}

// API 
CoreInstList CoreInstFactory::getMemoryInsts() const
{
    CoreInstList memInsts;

    CoreList memCores = getMemCores();
    for (auto& core: memCores) {
        auto cInsts = createCoreInst(core);
        memInsts.insert(memInsts.end(), cInsts.begin(), cInsts.end());
    }

    return memInsts;
}

// API 
CoreInstList CoreInstFactory::getCoreInstsByInterfaceMode(
    InterfaceMode ifmode,
    int type_mask) const 
{
    CoreInstList ifmodeInsts;
    
    CoreList ifmodeCores = getInterfaceCores(ifmode, type_mask);

    for (auto& core : ifmodeCores) {
        auto cInsts = createCoreInst(core);
        ifmodeInsts.insert(ifmodeInsts.end(), cInsts.begin(), cInsts.end());
    }

    return ifmodeInsts;
}

unsigned CoreInstFactory::string2CoreType(string ty_str)
{
    static const char* StrFuncUnit = "FUNCTIONAL_UNIT";
    static const char* StrFU = "FU"; // FU alias

    static const char* StrStorage = "STORAGE";
    static const char* StrMEM = "MEM"; // Memory alias

    static const char* StrConnector = "CONNECTOR";
    static const char* StrLINK = "LINK"; // Connector alias

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

    if (ty_str == StrConnector || ty_str == StrLINK)
        return pf_internal::Core::Connector;

    if (ty_str == StrChannel)
        return pf_internal::Core::Channel;

    if (ty_str == StrAdapter || ty_str == StrIF)
       return pf_internal::Core::Adapter;

    if (ty_str == StrUserIP || ty_str == StrIP)
        return pf_internal::Core::IPBlock;

    //return Core::Any;
    return pf_internal::Core::Generic;
}

// Role is similar with PlatformBasic::load(const std::string& libraryName)
int CoreInstFactory::createCores(const std::string& libraryName)
{
    clear();
    setName(libraryName);
    mCoreId = 0;
    addGenericCore();
    // mCoreId = 1; the first is generic core
    
    std::vector<CoreDef*> defitions;
    if(!libraryName.empty())
    {
        std::string cmd = "select * from " + libraryName + "_CoreDef ";
        auto& s = Selector::getInstance();
        defitions = s.selectCoreDefs(cmd.c_str());
    }

    for(auto definition : defitions)
    {
        auto def = *definition;
        switch(string2CoreType(def.type))
        {
            case pf_internal::Core::Type::FunctionUnit: 
            {
                pf_internal::Core* core = new pf_internal::FuncUnit(mCoreId, def.name, def.description, def.opers, 
                                    def.style, def.delay, def.delayFunction, def.isDelayList, 
                                    def.latency, def.latencyFunction, 
                                    def.resourceMap, def.usageFunction,
                                    def.generator, def.interval, def.intervalFunction,
                                    def.isHidden, def.legalityFunction, def.preference,
                                    // FuncUnit Only 
                                    def.inputs, def.outputs, def.portStageFunction);
                addCore(core);
                break;
            }
            case pf_internal::Core::Type::Storage:
            {
                pf_internal::Core* core = new pf_internal::Storage(mCoreId, def.name, def.description, def.opers, 
                                    def.style, def.delay, def.delayFunction, def.isDelayList, 
                                    def.latency, def.latencyFunction, 
                                    def.resourceMap, def.usageFunction,
                                    def.generator, def.interval, def.intervalFunction,
                                    def.isHidden, def.legalityFunction, def.preference,
                                    // Storage Only
                                    def.depth, def.ifModes, def.memPorts);
                addCore(core);
                break;
            }
            case pf_internal::Core::Type::Adapter:
            {
                pf_internal::Core* core = new pf_internal::Adapter(mCoreId, def.name, def.description, def.opers,
                                    def.style, def.delay, def.delayFunction, def.isDelayList, 
                                    def.latency, def.latencyFunction, 
                                    def.resourceMap, def.usageFunction,
                                    def.generator, def.interval, def.intervalFunction,
                                    def.isHidden, def.legalityFunction, def.preference,
                                    // Adapter Only
                                    def.ifModes);
                addCore(core);
                break;
            }
            default: 
                break;
        };
    }

    for(auto definition : defitions)
    {
        delete definition;
    }

    return 0;
}

void CoreInstFactory::addCore(pf_internal::Core* core)
{
    assert(core);
    core->setId(mCoreId);
    mId2Core[mCoreId] = core;

    // FIXME: Check the name uniqueness?
    string coreName = core->getName();
    std::transform(coreName.begin(),
                   coreName.end(),
                   coreName.begin(),
                   (int(*)(int))toupper);

    assert(mName2Id.count(coreName) == 0);

    mName2Id[coreName] = mCoreId;
    mId2Names[mCoreId].insert(coreName);
    ++mCoreId;

    mCores.push_back(core);
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

CoreList CoreInstFactory::getFuncCores(OperType opcode, int type_mask) const
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

CoreList CoreInstFactory::getMemCores() const 
{
    CoreList memCoreList;

    for (CoreIter ci = mCores.begin(); ci != mCores.end(); ++ci)
    {
        pf_internal::Core* core = *ci;
        if (core->matchOper(platform::PlatformBasic::OP_TYPE::OP_LOAD) && core->matchOper(platform::PlatformBasic::OP_TYPE::OP_STORE)) 
        {
            memCoreList.push_back(core);
        }
    }

    return memCoreList;
}

CoreList CoreInstFactory::getInterfaceCores(InterfaceMode ifmode, int type_mask) const
{
    CoreList ifmodeCoreList;

    for (CoreIter ci = mCores.begin(); ci != mCores.end(); ++ci)
    {
        pf_internal::Core* core = *ci;
        if (core->getType() & type_mask)
            if (core->matchInterfaceMode(ifmode) && !core->isHidden()) // Skip the hidden cores
                ifmodeCoreList.push_back(core);
    }

    return ifmodeCoreList;
}

}//< namespace pf_newFE

namespace pf_internal
{
using namespace pf_newFE;
// Core class member functions
Core::Core()
{
    // Name and type
    mId = 0;
    mName = "";
    mType = Generic;
    mIsHidden = false;
    mIsDelayList = false;

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
{
            mId = id;
            mName = name;
            mDelayList = {delay, delay, delay};
            mDelayFunction = delayFunction;
            mIsDelayList = isDelayList;
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
                mOperSet.insert(opcode);
            }
            // resource: string to map
            // if is str, it's type; if is digit, it's num of previous type
            auto wordList = getTclList(resource, " {}");
            for(unsigned i = 0; i < wordList.size(); ++i)
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

Core::~Core() {}

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

unsigned Core::string2Mode(std::string mode_str)
{
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

FuncUnit::FuncUnit(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
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
     }

FuncUnit::~FuncUnit() {}

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
    preference) 
    {
        mType = Core::Storage;
        for(const auto& modeStr : getTclList(interfaces, " {}"))
        {
            InterfaceMode mode = string2Mode(modeStr);
            assert(mode > 0);
            mIfModes.insert(mode);
        } 
    }

Storage::~Storage() {}

Adapter::Adapter(unsigned id, const std::string& name, const std::string& description, const std::string& opers,
    const std::string& style, double delay, const std::string& delayFunction, bool isDelayList, 
    int latency, const std::string& LatencyFunction, 
    const std::string& resource, const std::string& resUsageFunction,
    const std::string& generator, int interval, const std::string& intervalFunction,
    bool isHidden, const std::string& legalityFunction, double preference,
    // storage
    const std::string& interfaces):
    Core(id, name, description, opers, style, delay, delayFunction, isDelayList, 
    latency, LatencyFunction, resource, resUsageFunction,
    generator, interval, intervalFunction, isHidden, legalityFunction,
    preference) 
    {
        mType = Core::Adapter;
        for(const auto& modeStr : getTclList(interfaces, " {}"))
        {
            InterfaceMode mode = string2Mode(modeStr);
            assert(mode > 0);
            mIfModes.insert(mode);
        } 
    }

Adapter::~Adapter() {}

platform::PlatformBasic::MEMORY_IMPL Storage::getMemoryImpl() const
{
    auto pb = platform::PlatformBasic::getInstance();
    auto opImpls = pb->getOpImplFromCoreName(getName()); 
    return pb->getCoreFromOpImpl(opImpls[0].first, opImpls[0].second)->getMemoryImpl();
}

} // namespace pf_internal
