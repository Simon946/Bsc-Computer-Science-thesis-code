#include <iostream>
#include <string>
#include <tuple>

#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>

#include "include/task.h"
#include "include/target.h"
#include "include/network.h"
#include "include/sorting.h"
#include "include/virtualbuffer.h"

using namespace std;

tuple<int, int> connectToMaster(string masterIP, uint16_t masterPort, uint16_t workerListenPort){
    struct sockaddr_in masterAddress;
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if(socketfd < 0){
        cerr << "Socket creation error" << endl;
        return {-1, 0};
    }
    memset(&masterAddress, '0', sizeof(masterAddress));
    masterAddress.sin_family = AF_INET;
    masterAddress.sin_port = htons(masterPort);
    
    if(inet_pton(AF_INET, masterIP.c_str(), &masterAddress.sin_addr) <= 0){
        cerr << "Invalid address type" << endl;
        return {-1, 0};
    }
    int res = -1;

    for(int i = 0; i < CONNECT_MAX_ATTEMPTS; i++){
        res = connect(socketfd, (struct sockaddr *)&masterAddress, sizeof(masterAddress));

        if(res >= 0){
            break;
        }
        if(errno != ECONNREFUSED && errno != ETIMEDOUT){
            cerr << "Failed to connect to the master" << endl;
            return {-1, 0};
        }
        sleep(1);
    }
    if(res < 0){
        cerr << "Failed to connect to the master after many attempts" << endl;
        return {-1, 0};
    }
    ssize_t response = write(socketfd, &workerListenPort, sizeof(workerListenPort));

    if(response != sizeof(workerListenPort)){
        cerr << "Failed to send listen port to master" << endl;
        return {-1, 0};
    }
    int numberOfWorkers;
    response = read(socketfd, &numberOfWorkers, sizeof(numberOfWorkers));

    if(response != sizeof(numberOfWorkers)){
        cerr << "Failed to receive numberOfWorkers from master" << endl;
        return {-1, 0};
    }
    return {socketfd, numberOfWorkers};
}

int connectToPeerWorker(string peerIP, uint16_t peerPort){
    struct sockaddr_in peerAddress;
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd < 0){
        cerr << "Socket creation error" << endl;
        return -1;
    }
    memset(&peerAddress, '0', sizeof(peerAddress));
    peerAddress.sin_family = AF_INET;
    peerAddress.sin_port = htons(peerPort);
    
    if(inet_pton(AF_INET, peerIP.c_str(), &peerAddress.sin_addr) <= 0){
        cerr << "Invalid address type" << endl;
        return -1;
    }
    for(int i = 0; i < CONNECT_MAX_ATTEMPTS; i++){
        int res = connect(socketfd, (struct sockaddr *)&peerAddress, sizeof(peerAddress));

        if(res >= 0){
            return socketfd;
        }
        if(errno != ECONNREFUSED && errno != ETIMEDOUT){
            cerr << "Failed to connect" << endl;
            return -1;
        }
        sleep(1);
    }
    cerr << "out of attempts, connect failed!" << endl;
    return -1;
}

int createSocket(uint16_t port){
    struct sockaddr_in address;

    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY;//use any interface IP, wifi, ethernet
    address.sin_port = htons(port);
    memset(address.sin_zero, '\0', sizeof(address.sin_zero));

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if(socketfd < 0){ //AF_INET = IPv4
        cerr << "Cannot create socket" << endl;
        return -1;
    }
    if (bind(socketfd, (struct sockaddr *)&address, sizeof(address)) < 0){
        //cerr << "Cannot bind socket" << endl;
        return -1;
    }
    return socketfd;
}

struct Task receiveTask(int socketfd){
    uint32_t taskSize;
    ssize_t response = read(socketfd, &taskSize, sizeof(uint32_t));

    if(response != sizeof(taskSize)){
        cerr << "Failed to receive task size" << endl;
        return Task();
    }
    uint8_t* taskBytes = new uint8_t[taskSize];
    memcpy(taskBytes, &taskSize, sizeof(taskSize));

    response = read(socketfd, taskBytes + sizeof(taskSize), taskSize - sizeof(uint32_t));

    if(response != taskSize - sizeof(uint32_t)){
        cerr << "failed to read full response" << endl;
    }
    Task result = taskFromArray(taskBytes);

    if(result.targets.empty() && result.type == CONNECT_TO_PEERS){
        cerr << "no targets read" << endl;
    }   
    delete[] taskBytes;
    return result;
}

bool sendTask(int socketfd, Task task){
    uint32_t taskSize = getSize(task);
    uint8_t* taskBytes = new uint8_t[taskSize];//add tasksize and number of targets
    toArray(task, taskBytes);

    ssize_t response = write(socketfd, taskBytes, taskSize);
    delete[] taskBytes;
    return (response == taskSize);
}

bool receiveOK(int socketfd){
    char response[3];
    ssize_t size = read(socketfd, &response, 3);
    return size == 3 && strcmp(response, "OK\0") == 0;
}

bool sendOK(int socketfd){
    char ok[] = "OK\0";
    ssize_t size = write(socketfd, &ok, 3);
    return size == 3;
}



