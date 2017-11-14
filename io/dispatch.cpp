#include <sstream>
#include <iomanip>
#include <algorithm>

#include <poll.h>

#include "dispatch.h"
#include "tcpUtil.h"
#include "protocol.h"


void dispatch::manageQs()
{
    lookForNewClients();
    handleReads();
    if (taskQ.size() > 0) 
        handleWrites();
}

void dispatch::dispatchTask(task& newTask)
{
    taskQ.push(newTask);
}

void dispatch::lookForNewClients() 
{
    struct pollfd monFd[1];
    monFd[0].fd = servSock;
    monFd[0].events = readMask;

    if ((poll(monFd, 1, 0) == 1)) {
        struct sockaddr_in cliAddr;
        socklen_t cliAddrLen = sizeof(sockaddr_in);
        int cliSock;
        if ((cliSock = accept(servSock, (struct sockaddr *)&cliAddr,
                               &cliAddrLen)) > 0)  {
            clientList.insert({cliSock,{cliSock}});
        }
    }
}

void dispatch::handleReads()
{
    struct pollfd rdFd[16];
    uint16_t currIn = 0;
    for (auto mem:clientList) {
        if (mem.second.active == true) {
            rdFd[currIn].fd = mem.first;
            rdFd[currIn++].events = readMask;
        }
    }
    
    int rc = poll(rdFd, currIn, 0);
    if (rc <= 0)
        return;
    
    std::set<int> rdSet;
    for (uint32_t i = 0; i < currIn; i++)
        if (rdFd[i].revents & readMask) 
            rdSet.insert(rdFd[i].fd);

    const std::string logPrf = "Result: ";
    for (auto mem:rdSet) {
        struct result tR;
        if (protocol::readResult(mem, tR) == true) {
            resultQ.push(tR);
            logResult(logPrf + std::to_string(mem), tR);
            clientList.find(mem)->second.active = false;
            clientList.find(mem)->second.lastReply = time(0);
       } else {
           logSink->addEntry("Error! on client " + std::to_string(mem));
       }
    }
}

void dispatch::handleWrites()
{
    struct pollfd wrFd[16];
    uint16_t currIn = 0;
    for (auto mem:clientList) {
        if (mem.second.active == false)  {
            wrFd[currIn].fd = mem.first;
            wrFd[currIn++].events = writeMask;
        }
    }

    int rc = poll(wrFd, currIn, 0);
    if (rc <= 0)
        return;
    
    std::vector<struct cliState> cliVec;
    for (uint32_t i = 0; i < currIn; i++) 
        if (wrFd[i].revents & writeMask) 
            cliVec.push_back(clientList.find(wrFd[i].fd)->second);
    

    std::sort(cliVec.begin(), cliVec.end(), 
              [](cliState& a, cliState& b){ return a.lastReply < b.lastReply;});

    std::string logStr = "Task Dispatch";
    for (auto mem:cliVec) {
        if (taskQ.size() == 0)
            break;

        struct task t = taskQ.front();
        protocol::writeTask(mem.fd, t);
        clientList.find(mem.fd)->second.active = true;
        std::string wrLine = " " + std::to_string(mem.fd) + " ";
        logTask(logStr + wrLine, t);
        taskQ.pop();
    }
}


bool dispatch::fetchResults(result& rc)
{
    if (resultQ.size() != 0) {
        rc = resultQ.front();
        resultQ.pop();
        return true;
    }
    return false;
}

void dispatch::terminate() 
{
    for (auto mem:clientList) {
        std::string tL = "client " + std::to_string(mem.first) + 
                         " " + "Finished";
        logSink->addEntry(tL);
        protocol::terminateClient(mem.first);
    }
}

void dispatch::logTask(const std::string& prefix, const struct task& tsk)
{
    std::string line = prefix + " " + tsk.id + " " + tsk.type;
    logSink->addEntry(line);
     
}

void dispatch::logResult(const std::string& prefix, 
                         const struct result& rslt)
{
    std::string line = prefix + " " + rslt.id + " RC " 
                       + std::to_string(rslt.rc);
    logSink->addEntry(line);
}

void dispatch::getNewTask(char tType, const strVec& tArgs, struct task& newTask)
{
    std::string tid;
    getTaskId(tid);

    newTask.type = tType;
    newTask.id = tid;
    newTask.args = tArgs;
}

void dispatch::getTaskId(std::string& id)
{
    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << taskId;
    id = 'T' + ss.str();
    taskId++;
}
