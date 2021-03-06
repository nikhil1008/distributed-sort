#include <iostream>
#include <fstream>
#include <queue>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "threadPool.h"
#include "config.h"
#include "mergeSort.h"
#include "sortMerge.h"
#include "filePartition.h"
#include "sortHandler.h"
#include "dispatchIterations.h"
#include "globalLogger.h"
#include "globalConfig.h"

static const std::string sortFileStub = "/s";
static const std::string mergeFileStub = "/m";
typedef std::pair<std::string, std::string> strPair;
typedef std::queue<std::string> strQ;

/***************************************************************
FUNCTION: sortHandler::handler
IN: args Input arguments. 
arg[0] Input file.
arg[1] Start range in the input file.
arg[2] End range in the input file.
arg[3] ouput file.

Sorts the input range and writes to an output file.

****************************************************************/


bool sortHandler::handler(const strVec& args) 
{
    if (args.size() != 4)
        return false;

    std::string inputFile = args[0];
    uint64_t beginRange = std::stoull(args[1]);
    uint64_t endRange = std::stoull(args[2]);
    std::string outputFile = args[3];
    
    std::string scratchDir;
    globalConfig::getValueByKey("scratchLocation", scratchDir);
    uint64_t blockSizeLimit;
    globalConfig::getValueByKey("sortBlockLimit", blockSizeLimit);

    if ((blockSizeLimit == 0) || (scratchDir == "")) {
        globalLogger::logEntry("Config File Invalid");
        return false;
    }
    blockSizeLimit *= 1024 * 1024;
    uint32_t numCores = std::thread::hardware_concurrency();
    
    uint32_t blockSize = std::min(blockSizeLimit, 
                                 (endRange - beginRange)/numCores);

    globalLogger::logEntry("Sort block size is " + std::to_string(blockSize));
    filePartition fP(inputFile, scratchDir + sortFileStub);
    fP.setInterval(blockSize, beginRange, endRange);

    funcMap fTab = {{'s', mergeSort::sortBlock}};
    threadPool tP(fTab);
    tP.startDispatch();

    strVec mergeArgs;
    dispatchIters(tP, fP, 's', mergeArgs);
    if (mergeArgs.size() <= 1)  {
        globalLogger::logEntry("Not enough files to merge or too many sort errors");
        tP.terminate();
        return false;
    }
    
    globalLogger::logEntry("Starting merge");
    mergeArgs.push_back(outputFile);
    if (multiMerge(mergeArgs) == false)
        globalLogger::logEntry("Merge Failed");
    
    globalLogger::logEntry("Merge Complete");
    for (uint32_t i = 0; i < mergeArgs.size() - 1; i++)
        unlink(mergeArgs[i].c_str());

    tP.terminate();
    return true;
}
