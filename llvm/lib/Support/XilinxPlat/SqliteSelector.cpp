// (C) Copyright 2016-2022 Xilinx, Inc.
// (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
#include <cstring>
#include <cassert>
#include <sqlite3.h>
#include <limits>
#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XilinxPlat/SqliteSelector.h"
#include "llvm/Support/XilinxPlat/TargetPlatform.h"
#else
#include "SqliteSelector.h"
#include "TargetPlatform.h" 
#endif


namespace platform
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
  if( rc==SQLITE_OK ) {
    sqlite3_busy_timeout(pFile, 10000);

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

int Selector::init(const std::string& dbPath) {
    int rc = sqlite3_open(":memory:", &mDb);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    rc = loadOrSaveDb(mDb, dbPath.c_str(), 0);
    return rc;
}

Selector::~Selector() { sqlite3_close(mDb); }

std::string Selector::safe_get_string (sqlite3_stmt* ppStmt, int col)
{
    const unsigned char* result = sqlite3_column_text(ppStmt, col);
    std::string value;
    if(result)
    {   
        value = reinterpret_cast<const char*>(result);
    }
    return value;
}

std::vector<CoreDef*> Selector::selectCoreDefs(const char* cmd)
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

DataOr<int> Selector::selectInt(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    rc = sqlite3_step(ppStmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(ppStmt);
        return {false, {}};  // return false if no row is found
    }
    assert(sqlite3_column_count(ppStmt) == 1);
    if (sqlite3_column_type(ppStmt, 0) == SQLITE_NULL) {
        sqlite3_finalize(ppStmt);
        return {false, {}};  // return false if the value is NULL
    }
    int value = sqlite3_column_int(ppStmt, 0);
    sqlite3_finalize(ppStmt);

    return {true, value};
}

DataOr<double> Selector::selectDouble(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    rc = sqlite3_step(ppStmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(ppStmt);
        return {false, {}};  // return false if no row is found
    }
    assert(sqlite3_column_count(ppStmt) == 1);
    if (sqlite3_column_type(ppStmt, 0) == SQLITE_NULL) {
        sqlite3_finalize(ppStmt);
        return {false, {}};  // return false if the value is NULL
    }
    double value = sqlite3_column_double(ppStmt, 0);
    sqlite3_finalize(ppStmt);

    return {true, value};
}

std::string Selector::selectString(const char* cmd)
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

std::vector<double> Selector::selectDoubleList(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    sqlite3_step(ppStmt);
    int delayNum = sqlite3_column_count(ppStmt);
    assert(delayNum);
    std::vector<double> values;
    for(int i = 0; i < delayNum; ++i)
    {
        double value = GetTargetPlatform()->getCoreInstFactory()->getDelayFactor() * sqlite3_column_double(ppStmt, i);
        values.push_back(value);
    }
    sqlite3_finalize(ppStmt);

    return values;
}

std::vector<double> Selector::selecDSPDelayList(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    sqlite3_step(ppStmt);
    int delayNum = sqlite3_column_count(ppStmt);
    assert(delayNum);
    std::vector<double> values;
    for(int i = 0; i < delayNum; ++i)
    {
        double value = GetTargetPlatform()->getCoreInstFactory()->getDelayFactor() * sqlite3_column_double(ppStmt, i);
        if(value > 0)
        {
            values.push_back(value);
        }
    }
    sqlite3_finalize(ppStmt);

    return values;
}

std::vector<int> Selector::selectDSPPortList(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    sqlite3_step(ppStmt);
    int num = sqlite3_column_count(ppStmt);
    assert(num);
    std::vector<int> values;
    for(int i = 0; i < num; ++i)
    {
        int value = sqlite3_column_int(ppStmt, i);
        values.push_back(value);
    }
    sqlite3_finalize(ppStmt);
    return values;
}

std::vector<int> Selector::selectIntList(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
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

DelayMap Selector::selectDelayMap(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
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
            double value = GetTargetPlatform()->getCoreInstFactory()->getDelayFactor() * sqlite3_column_double(ppStmt, i);
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

std::map<int, int> Selector::selectInt2IntMap(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
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

std::map<std::string, std::string> Selector::selectStr2StrMap(const char* cmd) {
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    // std::string c_cmd = complete_cmd(cmd);
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    assert(sqlite3_column_count(ppStmt) == 2);
    std::map<std::string, std::string> values;

    while(sqlite3_step(ppStmt) == SQLITE_ROW)
    {
        auto first = safe_get_string (ppStmt, 0);
        auto second = safe_get_string(ppStmt, 1);
        values.insert(std::make_pair(first, second));
    }
    sqlite3_finalize(ppStmt);

    return values; 
}

std::vector<std::vector<int>> Selector::selectInt2dList(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
    assert(rc == SQLITE_OK);
    std::vector<std::vector<int>> values;
    int colLen = sqlite3_column_count(ppStmt);
    while(sqlite3_step(ppStmt) == SQLITE_ROW)
    {
        std::vector<int> line;
        for (int i=0; i < colLen; i ++) {
            line.push_back(sqlite3_column_int(ppStmt, i));
        }
        values.push_back(line);
    }
    sqlite3_finalize(ppStmt);

    return values; 
}

std::pair<int, int> Selector::selectIntPair(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
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

std::pair<std::string, std::string> Selector::selectStrPair(const char* cmd)
{
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    assert(sqlite3_column_count(ppStmt) == 2);
    std::pair<std::string, std::string> values("", "");
    while(sqlite3_step(ppStmt) == SQLITE_ROW)
    {
        std::string first = safe_get_string(ppStmt, 0);
        std::string second = safe_get_string(ppStmt, 1);
        values = std::make_pair(first, second);
    }
    sqlite3_finalize(ppStmt);
    return values; 
}

bool Selector::isExistTable(const char* tableName) {
    bool ret = false;
    std::string cmd = "SELECT name FROM sqlite_master WHERE type='table' AND name='";
    cmd = cmd + tableName + "'";
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd.c_str(), -1, &ppStmt, &pzTail);
    assert(rc == SQLITE_OK);
    if (sqlite3_step(ppStmt) == SQLITE_ROW) {
        ret = true;
    }
    sqlite3_finalize(ppStmt);
    return ret;
}

std::map<std::string, double> Selector::selectStr2DoubleMap(const char* cmd) {
    sqlite3_stmt *ppStmt;
    const char *pzTail;
    // std::string c_cmd = complete_cmd(cmd);
    int rc = sqlite3_prepare_v2(mDb, cmd, -1, &ppStmt, &pzTail);
    // if(rc), message out and exit.
    assert(rc == SQLITE_OK);
    assert(sqlite3_column_count(ppStmt) == 2);
    std::map<std::string, double> values;

    while(sqlite3_step(ppStmt) == SQLITE_ROW) {
        auto first = safe_get_string (ppStmt, 0);
        auto second = sqlite3_column_double(ppStmt, 1);
        values.insert(std::make_pair(first, second));
    }
    sqlite3_finalize(ppStmt);

    return values; 
}


std::vector<CoreBasicDef*> Selector::selectCoreBasics(std::string cmd)
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

std::map<int, std::string> Selector::selectEncode(std::string cmd)
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

std::map<std::string, std::string> Selector::selectAliasCores(std::string cmd)
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

std::vector<Core1DParam> Selector::selectCore1DParams(std::string cmd) {
    std::vector<Core1DParam> core1DParams;
    sqlite3_stmt* ppStmt;
    const char* pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd.c_str(), -1, &ppStmt, &pzTail);
    assert(rc == SQLITE_OK);

    while(sqlite3_step(ppStmt) == SQLITE_ROW)
    {
        Core1DParam param;
        param.name = safe_get_string(ppStmt, 0);
        param.latency = sqlite3_column_int(ppStmt, 1);
        param.kind = safe_get_string(ppStmt, 2);
        param.xx = sqlite3_column_double(ppStmt, 3);
        param.log = sqlite3_column_double(ppStmt, 4);
        param.x = sqlite3_column_double(ppStmt, 5);
        param.intercept = sqlite3_column_double(ppStmt, 6);
        core1DParams.push_back(param);
    }
    sqlite3_finalize(ppStmt);
    return core1DParams;
}

std::vector<Core2DParam> Selector::selectCore2DParams(std::string cmd) {
    std::vector<Core2DParam> core2DParams;
    sqlite3_stmt* ppStmt;
    const char* pzTail;
    int rc = sqlite3_prepare_v2(mDb, cmd.c_str(), -1, &ppStmt, &pzTail);
    assert(rc == SQLITE_OK);

    while(sqlite3_step(ppStmt) == SQLITE_ROW)
    {
        Core2DParam param;
        param.name = safe_get_string(ppStmt, 0);
        param.latency = sqlite3_column_int(ppStmt, 1);
        param.kind = safe_get_string(ppStmt, 2);
        param.x0x0 = sqlite3_column_double(ppStmt, 3);
        param.logx0 = sqlite3_column_double(ppStmt, 4);
        param.x0 = sqlite3_column_double(ppStmt, 5);
        param.x1x1 = sqlite3_column_double(ppStmt, 6);
        param.logx1 = sqlite3_column_double(ppStmt, 7);
        param.x1 = sqlite3_column_double(ppStmt, 8);
        param.x0x1 = sqlite3_column_double(ppStmt, 9);
        param.intercept = sqlite3_column_double(ppStmt, 10);
        core2DParams.push_back(param);
    }
    sqlite3_finalize(ppStmt);
    return core2DParams;
}

}  // end of namaspace platform 
