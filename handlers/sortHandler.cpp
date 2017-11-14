#include <iostream>
#include <fstream>
#include <queue>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "threadPool.h"
#include "config.h"
#include "mergeSort.h"
#include "fileUtil.h"
#include "sortHandler.h"

static const std::string sortFileStub = "/s.";
static const std::string mergeFileStub = "/m.";
typedef std::pair<std::string, std::string> strPair;
typedef std::queue<std::string> strQ;

bool sortHandler::handler(const strVec& args) 
{
    if (args.size() != 4)
        return false;

    std::string inputFile = args[0];
    uint64_t beginRange = std::stoull(args[1]);
    uint64_t endRange = std::stoull(args[2]);
    std::string outputFile = args[3];
    
    std::string scratchDir;
    cfg->getValue("scratchLocation", scratchDir);
    std::string sortBlockSize;
    cfg->getValue("sortBlockSize", sortBlockSize);

    if ((sortBlockSize == "") || (scratchDir == "")) {
        logSink->addEntry("Config File Invalid");
        return false;
    }

    uint32_t blockSize = std::stoi(sortBlockSize) * 1024 * 1024;
    offPairVec fileOffsets;
    fileUtil::splitFileBySize(inputFile, beginRange, endRange, blockSize, 
                              fileOffsets);

    uint32_t currentTaskId = 0;
    #if 0
    tP = new threadPool(4); //Get num cores for num threads; OR do it in the 
                            //cstor
    #endif

    threadPool tP; 
    std::string sortBase = scratchDir + sortFileStub; 
    for (auto mem:fileOffsets) {
        std::string sortOut = sortBase + std::to_string(currentTaskId);
        strVec taskArgs = {inputFile, std::to_string(mem.first),
                            std::to_string(mem.second), sortOut};

        struct subTask currTask = {currentTaskId++, mergeSort::sortBlock, 
                                   taskArgs, 10};
        printToLog("SCH", 's', currTask.id, taskArgs);

        tP.addToPool(currTask);
    }
    
    tP.startWorkers();

    bool success = true;
    strQ postSortQ, postMergeQ;
    uint32_t totalReturns = currentTaskId * 2 - 1;
    uint32_t lastId = totalReturns - 1;
    uint32_t sortCount = currentTaskId;
    uint32_t postSortTasks = 0;

    while (totalReturns > 0) {
        taskVec retTasks;
        tP.getTaskResults(retTasks);
        totalReturns -= retTasks.size();
        for (auto mem:retTasks) {

            if (mem.result != 0) {
                success = false;
                break;
            }
    
            char ty = 's';
            if (mem.func == mergeSort::mergeFiles) {
                unlink(mem.args[0].c_str());
                unlink(mem.args[1].c_str());
                postMergeQ.push(mem.args[2]);
                ty = 'm';
            } else if (mem.func == mergeSort::sortBlock) {
                postSortQ.push(mem.args[3]);
                postSortTasks++;
            }
            
            printToLog("RET RC " + std::to_string(mem.result), ty, mem);
        }

        if (success == false)
            break;
        
        if ((postSortTasks == sortCount) && (postSortQ.size() == 1)) {
            postMergeQ.push(postSortQ.front());
            postSortQ.pop();
        }
        
        strPairVec mergeFilePairs;
        if (postSortQ.size() != 0)
            reduceFileQ(postSortQ, mergeFilePairs);
        if (postMergeQ.size() != 0)
            reduceFileQ(postMergeQ, mergeFilePairs);

        std::string mergeBase = scratchDir + mergeFileStub;
        for (auto mem:mergeFilePairs) {

            std::string mergeFile = mergeBase + std::to_string(currentTaskId);
            if (currentTaskId == lastId)
                mergeFile = outputFile;

            strVec tArgs = {mem.first, mem.second, mergeFile};
            subTask sT = {currentTaskId, mergeSort::mergeFiles, tArgs, 10};
            tP.addToPool(sT);
            printToLog("SCH", 'm', currentTaskId, tArgs);

            currentTaskId++;
        }
    }
    
    tP.stopAllThreads();
    return success;
}

void sortHandler::reduceFileQ(strQ& fileQ, strPairVec& filePairs)
{
    while(fileQ.size() > 1) {
        std::string s1 = fileQ.front();
        fileQ.pop();
        std::string s2 = fileQ.front();
        fileQ.pop();
        filePairs.push_back({s1,s2});
    }
}

void sortHandler::printToLog(const std::string prefix, char type,
                             const struct subTask& sT)
{
    printToLog(prefix, type, sT.id, sT.args);
}

void sortHandler::printToLog(const std::string prefix, char type, 
                             uint32_t id, const strVec& args)
{
    std::string line = prefix + " " + type + " " + std::to_string(id);

    for (auto mem:args)
        line += " " + mem;

    logSink->addEntry(line);
}