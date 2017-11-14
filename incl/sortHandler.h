#include <queue>

#include "logger.h"
#include "config.h"
#include "localTypes.h"
#include "msgHandler.h"
#include "threadPool.h"

#ifndef SORTHANDLER_H
#define SORTHANDLER_H

typedef std::vector<std::pair<std::string, std::string>> strPairVec;

class sortHandler:public msgHandlerBase {
    public:
    sortHandler(mrConfig* config, logger* log):msgHandlerBase(config,log) {}
    virtual bool handler(const strVec& args);

    private:
    void printToLog(const std::string prefix, char type, 
                    const struct subTask& sT);

    void printToLog(const std::string prefix, char type, uint32_t id, 
                    const strVec& args);

    void reduceFileQ(std::queue<std::string>& fileQ, strPairVec& filePairs);
    void addMergeTaskToPool(std::string& m1, std::string& m2);
};

#endif /*SORTHANDLER_H*/