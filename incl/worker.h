#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "localTypes.h"

#ifndef WORKER_H
#define WORKER_H

class worker {
    public:
    worker():finish(false), exTh(0) {}

    void addTask(subTask& nextTask);
    bool getResult(struct subTask& res);
    bool ready(); 
    void terminate();

    private:
    void threadFunc();

    private:
    std::queue<struct subTask> taskQ;
    std::queue<struct subTask> resultsQ;
    bool finish = false;
    std::thread* exTh;
    std::mutex runMtx;
    std::mutex resMtx;
    std::condition_variable condVar;
    const uint32_t maxInQ = 1;

};
#endif /* WORKER_H */