#ifndef NETWORKH
#define NETWORKH

#include <iostream>
#include <string>
#include <tuple>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "task.h"
#include "target.h"
#include "filebuffer.h"
#include "sorting.h"
#include "kvpair.h"
#include "virtualbuffer.h"

using namespace std;

tuple<int, int> connectToMaster(string masterIP, uint16_t masterPort, uint16_t workerListenPort);
int connectToPeerWorker(string peerIP, uint16_t peerPort);
    
int createSocket(uint16_t port);

struct Task receiveTask(int socketfd);
bool sendTask(int socketfd, Task task);

bool receiveOK(int socketfd);
bool sendOK(int socketfd);

#endif