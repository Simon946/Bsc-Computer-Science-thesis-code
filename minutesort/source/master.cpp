#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <tuple>
#include <bit>
#include <cstdint>

#include <assert.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <byteswap.h>

#include "include/target.h"
#include "include/task.h"
#include "include/constants.h"
#include "include/network.h"
#include "include/measure.h"
#include "include/sorting.h"
#include "include/kvpair.h"

using namespace std;

struct MasterConfig{
    int numberOfWorkers = 1;
    size_t numberOfKVPairs = 1;
    size_t memoryBufferSize = 1;
    string inputFileName = "";
    string outputFileName = "";

    int workerThreadCount = 1;
    unsigned int workerReceiveMergeCount = 1;
    unsigned int workerMemoryCount = 1;
};


MasterConfig parseConfig(){
    ifstream file("./master.CONFIG");
    MasterConfig config;

    vector<string> optionNames = {"NUMBEROFWORKERS", "NUMBEROFKVPAIRS", "INPUTFILENAME", "OUTPUTFILENAME", "WORKERTHREADCOUNT", "WORKERRECEIVEMERGECOUNT", "WORKERMEMORYCOUNT" };


    if(!file.is_open()){
        cerr << "Failed to open: master.CONFIG" << endl;
    }

    vector<bool>optionSet(optionNames.size(), false);

    while(file.good() && !file.eof()){
        string optionName;
        char equals = '=';
        file >> optionName;
        
        if(optionName.empty()){
            break;
        }
        file >> equals;
        assert(equals == '=' && file.good());

        for(int i = 0; i < optionNames.size(); i++){
            
            if(optionName == optionNames.at(i)){
                optionSet.at(i) = true;
            }
        }
        if(optionName == "NUMBEROFWORKERS"){
            file >> config.numberOfWorkers;
        }
        else if(optionName == "NUMBEROFKVPAIRS"){
            file >> config.numberOfKVPairs;
        }
        else if(optionName == "INPUTFILENAME"){
            file >> config.inputFileName;
        }
        else if(optionName == "OUTPUTFILENAME"){
            file >> config.outputFileName;
        }
        else if(optionName == "WORKERTHREADCOUNT"){
            file >> config.workerThreadCount;
        }
        else if(optionName == "WORKERRECEIVEMERGECOUNT"){
            file >> config.workerReceiveMergeCount;
        }
        else if(optionName == "WORKERMEMORYCOUNT"){
            file >> config.workerMemoryCount;
        }
        else{
            cerr << "Unknown option: \"" << optionName << "\"" << endl; 
        }
    }
    file.close(); 

    for(int i = 0; i < optionSet.size(); i++){

        if(!optionSet.at(i)){
            cerr << "Missing option: " << optionNames.at(i) << endl; 
        }
    }

    return config;
}
//TODO remove asserts if they increase performance. they are only for debugging

void createWorkerConfig(MasterConfig config, string ownIP, uint16_t ownPort){
    ofstream workerConfig("./worker.CONFIG", ios::out);

    if(!workerConfig.is_open()){
        cerr << "Failed to create file: ./worker.CONFIG" << endl;
        return;
    }
    workerConfig << "MASTERPORT = " << ownPort << endl; 
    workerConfig << "THREADCOUNT = " << config.workerThreadCount << endl;
    workerConfig << "RECEIVEMERGECOUNT = " << config.workerReceiveMergeCount << endl;
    workerConfig << "MEMORYCOUNT = " << config.workerMemoryCount << endl;
    workerConfig.close();
}

vector<tuple<int, string, uint16_t>> acceptWorkers(int socketfd, MasterConfig config){//each pair of the worker is socketfd, worker port

    vector<tuple<int, string, uint16_t>> workerConnections;
    workerConnections.reserve(config.numberOfWorkers);
    
    if(listen(socketfd, config.numberOfWorkers) != 0){
        cerr << "Cannot listen to socket" << endl;
        return workerConnections;
    }
    while(workerConnections.size() < (size_t)config.numberOfWorkers){
        struct sockaddr_in address;
        socklen_t addrlen = sizeof(sockaddr_in);
        int newConnection = accept(socketfd, (struct sockaddr *)&address, (socklen_t*)&addrlen);

        if(newConnection >= 0){
            uint16_t workerListenPort = 0;
            ssize_t size = read(newConnection, &workerListenPort, sizeof(workerListenPort));

            if(size != sizeof(workerListenPort)){
                cerr << "Failed to read worker listening port from worker" << endl;
                close(newConnection);
                continue;
            }
            char cString[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(address.sin_addr), cString, INET_ADDRSTRLEN);
            string ipString = string(cString);

            size = write(newConnection, &config.numberOfWorkers, sizeof(config.numberOfWorkers));

            if(size != sizeof(config.numberOfWorkers)){
                cerr << "Failed to send worker the number of workers" << endl;
                close(newConnection);
                continue;
            }
            workerConnections.push_back({newConnection, ipString, workerListenPort});
            cout << "accepted incoming connection from:" << address.sin_addr.s_addr << " aka:" << cString << " That worker is listening at port: " << workerListenPort << endl;
        }	
    }
    return workerConnections;
}

void divideWork(vector<Target>& targets){
    assert(!targets.empty());
    vector<pair<uint64_t, uint64_t>> splits = splitRange((uint64_t)0, (uint64_t)UINT64_MAX, (size_t)targets.size());

    for(size_t i = 0; i < targets.size(); i++){
        Target& target = targets.at(i);

        uint64_t lowSwapped = bswap_64(splits.at(i).first);
        memcpy(&(target.lowKey), &lowSwapped, sizeof(lowSwapped));
        memset(&(target.lowKey[sizeof(lowSwapped)]), 0x00, KEY_SIZE - sizeof(lowSwapped));

        uint64_t highSwapped = bswap_64(splits.at(i).second);
        mempcpy(&(target.highKey), &highSwapped, sizeof(highSwapped));
        memset(&(target.highKey[sizeof(highSwapped)]), 0xFF, KEY_SIZE - sizeof(highSwapped));//set remaining bytes of high to 0xFF.
    }
}

int main(){
    MasterConfig config = parseConfig(); 
    uint16_t randomPort = rand() % (PORT_MAX - PORT_MIN + 1);
    int socketfd = -1;

    for(int i = 0; i < SOCKET_CREATE_MAX_ATTEMPTS; i++){
        socketfd = createSocket(randomPort);

        if(socketfd > 0){
            break;
        }
        else{
            randomPort++;
            randomPort = randomPort % (PORT_MAX - PORT_MIN + 1);
        }
    }
    if(socketfd < 0){
        cerr << "Tried many times but cannot create socket" << endl;
        return -1;
    }
    struct sockaddr_in ownAddress;
    socklen_t addressSize = sizeof(ownAddress);

    if(getsockname(socketfd, (struct sockaddr *)&ownAddress, &addressSize) < 0){
        cerr << "Failed to get socket info" << endl;
        return -1;
    }
    cout << "returned addfressSize: " << addressSize << endl;
    char cString[INET_ADDRSTRLEN];
    if(inet_ntop(AF_INET, &(ownAddress.sin_addr), cString, INET_ADDRSTRLEN) == NULL){
        cerr << "failed" << endl;
    }
    createWorkerConfig(config, string(cString), randomPort);
    cout << "Master is listening at ip: " << cString << " port: " << randomPort << endl;

    vector<tuple<int, string, uint16_t>> workerConnections = acceptWorkers(socketfd, config);
    cout << "all workers are connected" << endl;

    vector<Target> targets;
    targets.reserve(config.numberOfWorkers);

    for(tuple<int, string, uint16_t> workerData : workerConnections){
        Target newTarget;
        newTarget.ip = get<string>(workerData);
        newTarget.port = get<uint16_t>(workerData);
        memset(&newTarget.lowKey, 0, KEY_SIZE);
        memset(&newTarget.highKey, 0xFF, KEY_SIZE);
        cout << "target info: " << newTarget.ip << ' ' << newTarget.port << endl;
        targets.push_back(newTarget);
    }
    divideWork(targets);
    Task connectToPeers;
    connectToPeers.type = CONNECT_TO_PEERS;
    connectToPeers.targets = targets;

    for(tuple<int, string, uint16_t> workerData : workerConnections){

        if(!sendTask(get<int>(workerData), connectToPeers)){
            cerr << "failed to send task to worker" << endl;
            return -1;
        }
    }
    cout << "sent first task to all workers" << endl;

    assert(workerConnections.size() == (size_t)config.numberOfWorkers);

    for(tuple<int, string, uint16_t> workerData : workerConnections){

        if(!receiveOK(get<int>(workerData))){
            cerr << "failed to receive OK" << endl;
            return -1;
        }
    }
    cout << "all workers respond OK on first task" << endl;
    vector<Task> bucketizeTasks(workerConnections.size());

    vector<pair<size_t, size_t>> fileSplits = splitRange((size_t)0, config.numberOfKVPairs -1, workerConnections.size());

    for(pair<size_t, size_t>& split : fileSplits){
        split.second = (split.second - split.first + 1) * sizeof(KVPair);//split.first the offset, split.second becomes the size
        split.first *= sizeof(KVPair);
        cout << "bucketize task, offset:" << split.first << " size; " << split.second << endl;
    }
    for(size_t i = 0; i < workerConnections.size(); i++){
        string workerFileName = config.inputFileName + "/" + to_string(i) + "/random.bin";
        bucketizeTasks.at(i) = {.type = BUCKETIZE, .fileName = workerFileName, .offset = 0, .size = fileSplits.at(i).second};
    }
    //start timer
    unsigned long long startTime = epoch_time();
    cout << "timer started. speed up" << endl;

    for(size_t i = 0; i < workerConnections.size(); i++){

        if(!sendTask(get<int>(workerConnections.at(i)), bucketizeTasks.at(i))){
            cerr << "failed to send task to worker" << endl;
            return -1;
        }
    }
    cout << "sent second task (bucketize) to all workers" << endl;

    for(tuple<int, string, uint16_t> workerData : workerConnections){

        if(!receiveOK(get<int>(workerData))){
            cerr << "failed to receive OK" << endl;
            return -1;
        }
    }
    vector<Task> sortTasks(workerConnections.size());

    for(size_t i = 0; i < workerConnections.size(); i++){
        sortTasks.at(i) = {
            .type = SORT,
            .fileName = config.outputFileName + "/" + to_string(i) + "/sorted.bin"
        };
        tuple<int, string, uint16_t> connectionInfo = workerConnections.at(i);

        if(!sendTask(get<int>(connectionInfo), sortTasks.at(i))){
            cerr << "failed to send task to worker" << endl;
            return -1;
        }
    }
    cout << "sent third task (sort) to all workers" << endl;

    for(tuple<int, string, uint16_t> workerData : workerConnections){

        if(!receiveOK(get<int>(workerData))){
            cerr << "failed to receive OK" << endl;
            return -1;
        }
    }
    unsigned long long endTime = epoch_time();

    cout << "benchmark time: " << endTime - startTime << "us, " << (endTime - startTime) / 1000 << "ms, " << (endTime - startTime) / 1000000 << "s" << endl;
    cout << "all workers respond OK on sorting task" << endl;
    
    //stop timer

    return 0;
}
