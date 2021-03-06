#include <iostream>
#include "threadPool.h"
#include "mergeSort.h"
#include "globalLogger.h"

/***************************************************************
FUNCTION: threadPool::dispatchTask
IN: ty Task function type.
IN: taskArgs Task arguments.
OUT: tId Task Id.

Dispatch the task to a thread in the pool.
****************************************************************/

void threadPool::dispatchTask(char ty, const strVec& taskArgs, 
                              std::string& tId)
{
    tId = std::to_string(currentId++);
    struct subTask st = {tId, nullptr, taskArgs, 10};
    st.func = functionTable.find(ty)->second;

    std::lock_guard<std::mutex> lck(stMtx);
    totalIn++;
    if (st.func != nullptr) {
        sQ.push(st);
    } else {
        st.result = 1;
        resQ.push(st);
    }
}


/***************************************************************
FUNCTION: threadPool::queryThreads() 

Enqueue tasks to the thread pool and retrieve results.
Runs in its own thread.
The two loops should be merged into one.
****************************************************************/
void threadPool::queryThreads() 
{
    while (workersAlive == true) {
        for (auto mem:threadVec) {
            std::lock_guard<std::mutex> lck(stMtx);
            if ((mem->ready() == true) && (sQ.size() > 0)){
                subTask sT = sQ.front();
                mem->addTask(sT);
                sQ.pop();
            }
        }

        for (auto mem:threadVec) {
            struct subTask r;
            bool avail = mem->getResult(r);
            if (avail == true) {
                std::lock_guard<std::mutex> lck(stMtx);
                std::string times = r.id + " " + std::to_string(r.startTime) + 
                                    " " + std::to_string(r.endTime - r.startTime);
                globalLogger::logEntry(times);
                resQ.push(r);
                if (resQ.size() == totalIn)
                    endCond.notify_one();
            }
        }
    }
}


/***************************************************************
FUNCTION: threadPool::startDispatch()
Start the all the threads in the pool along with the query thread.


****************************************************************/
bool threadPool::startDispatch()
{
    for (uint32_t i = 0; i < maxThreads; i++)
        threadVec.push_back(new worker());

    quThr = new std::thread(&threadPool::queryThreads, this);
    
    return true;
}

/***************************************************************
FUNCTION: threadPool::waitForCompletion
IN: failedIds List of failed task ids.

Blocks until all the tasks finish.
****************************************************************/

void threadPool::waitForCompletion(strVec& failedIds)
{
    std::unique_lock<std::mutex> lck(stMtx);
    while(totalIn > resQ.size())
        endCond.wait(lck);

    while (resQ.size() != 0) {
        subTask r = resQ.front();
        if (r.result != 0)
            failedIds.push_back(r.id);
        resQ.pop();
    }
}

/***************************************************************
FUNCTION: threadPool::terminate()
Terminate all threads and delete all data structures.

****************************************************************/
void threadPool::terminate()
{
    workersAlive = false;
    quThr->join();
    for (auto mem:threadVec) {
        mem->terminate();
        delete mem;
    }

    delete quThr;
}
