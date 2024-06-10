#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <queue>
#include <mutex>

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <byteswap.h>
#include <assert.h>
#include <fcntl.h>

#include "include/task.h"
#include "include/constants.h"
#include "include/network.h"

#include "include/measure.h"
#include "include/filebuffer.h"
#include "include/ringqueue.h"
#include "include/sorting.h"

using namespace std;

mutex logLock;
unsigned long long timeSpentEnqueuingToFile = 0;
unsigned long long timeSpentDequeuingFromFile = 0;
unsigned long long timeSpentEnqueuingToNetwork = 0;
unsigned long long timeSpentDequeuingFromNetwork = 0;

void debugPrintHex(uint8_t* array, int size){
    int i = 0;
    while(i < size ) {
        printf("%02x", array[i]);
        i++;
    }
    printf("\n"); 
}

struct WorkerConfig{
    string masterIP = "127.0.0.1";
    uint16_t masterPort = 5678;
    int threadCount = 4;
    unsigned int receiveMergeCount = 1000; //the number of incoming KVpairs to merge before writing to a file
    unsigned int memoryCount = 1;//how many KVpairs to store in a memory buffer
};

WorkerConfig parseConfiguration(){
    ifstream file("./worker.CONFIG");
    WorkerConfig config;

    while(file.good() && !file.eof()){
        string optionName;
        char equals = '=';
        file >> optionName;
        
        if(optionName.empty()){
            break;
        }
        file >> equals;
        assert(equals == '=' && !optionName.empty());

        if(optionName == "MASTERIP"){
            file >> config.masterIP;
        }
        else if(optionName == "MASTERPORT"){
            file >> config.masterPort;
        }
        else if(optionName == "THREADCOUNT"){
            file >> config.threadCount;
        }
        else if(optionName == "RECEIVEMERGECOUNT"){
            file >> config.receiveMergeCount;
        }
        else if(optionName == "MEMORYCOUNT"){
            file >> config.memoryCount;
        }
        else{
            cerr << "unknown option: " << optionName << endl;
        }
    }
    file.close();
    assert(config.threadCount > 0);
    assert(config.receiveMergeCount > 0);

    assert(FILE_IO_SIZE >= (KEY_SIZE + VALUE_SIZE));//TODO make it work without some of these asserts
    assert(NETWORK_IO_SIZE >= (KEY_SIZE + VALUE_SIZE));
    return config;
}

struct mergeIncomingArgs{
    FileBuffer<KVPair>* incoming;
    VirtualBuffer<KVPair>* result;
    unsigned int sortedSectionSize;
};

void* mergeIncoming(void* a){
    struct mergeIncomingArgs* args = (mergeIncomingArgs*)a;
    RingQueue<KVPair> sortingBuffer(args->sortedSectionSize);
    size_t pairsCopied = 0;
    //cout << "in mergeincoming, limit dequeue size to:" << args->sortedSectionSize << endl;
    do{
        assert(sortingBuffer.size() == 0);
        args->incoming->limitDequeue(args->sortedSectionSize);
        pairsCopied = copy(args->incoming, &sortingBuffer);
        sort(&sortingBuffer);
        bool ok = copy(&sortingBuffer, args->result) == pairsCopied;
        assert(ok);
        assert(sortingBuffer.size() == 0);
    }
    while(pairsCopied == args->sortedSectionSize);

    assert(sortingBuffer.size() == 0);
    return nullptr;
}

struct handleIncomingArgs{
    int incomingfd;
    vector<VirtualBuffer<KVPair>*> sortedBuffers;
    unsigned int receiveMergeCount = 1;
    size_t memoryBufferSize = 0;
    int numberOfWorkers;
};

void* handleIncoming(void* a){
    struct handleIncomingArgs* args = (handleIncomingArgs*)a;

    if (listen(args->incomingfd, args->numberOfWorkers) != 0){
        cerr << "Failed to listen for incoming connections." << endl;
        return nullptr;
    }
    vector<int> workerConnections;
    workerConnections.reserve(args->numberOfWorkers);

    while(workerConnections.size() < args->numberOfWorkers){
        struct sockaddr_in address;
        socklen_t addrlen = 0;
        int newConnection = accept(args->incomingfd, (struct sockaddr *)&address, (socklen_t*)&addrlen);       

        if(newConnection >= 0){
            workerConnections.push_back(newConnection);
        }
        else{
            cerr << "accept failed!" << endl;
        }
    }
    pthread_t threads[args->numberOfWorkers];
    mergeIncomingArgs mergeIncomingArgs[args->numberOfWorkers];

    for(int i = 0; i < args->numberOfWorkers; i++){
        mergeIncomingArgs[i] = {
            .incoming = new FileBuffer<KVPair>(args->memoryBufferSize, workerConnections.at(i), NETWORK_IO_SIZE),
            .result = args->sortedBuffers.at(i),
            .sortedSectionSize = args->receiveMergeCount
        };
        pthread_create(&threads[i], NULL, mergeIncoming, &mergeIncomingArgs[i]);
    }
    for(int i = 0; i < args->numberOfWorkers; i++){
        pthread_join(threads[i], NULL);
    }
    for(int i = 0; i < args->numberOfWorkers; i++){
        delete mergeIncomingArgs[i].incoming;
    }
    return nullptr;
}

struct bucketizeArgs{
    string fileName;
    size_t offset;
    size_t size;
    size_t memoryBufferSize;
    vector<Target> targets;
    vector<FileBuffer<KVPair>*> targetBuckets;
    vector<mutex*> bucketLocks;
};

void* bucketize(void* a){
    struct bucketizeArgs* args = (struct bucketizeArgs*)a;
    assert(args->size % sizeof(KVPair) == 0);//cannot bucketize partial KVpairs

    int inputFD = open(args->fileName.c_str(), O_RDWR | O_DIRECT);

    if(inputFD < 0){
        cerr << "Failed to open file: \"" << args->fileName << "\"" << endl;
        return nullptr;
    }
    
    FileBuffer<KVPair> inputFile(args->memoryBufferSize, inputFD, FILE_IO_SIZE);
    bool ok = lseek(inputFile.getFD(), args->offset, SEEK_SET) == args->offset;
    assert(ok);
    inputFile.limitDequeue(args->size / sizeof(KVPair));
    
    //cout << "opened file: " << args->fileName << " at: " << args->offset << " reading size: " << args->size << endl;
    size_t bytesRead = 0;

    while(bytesRead < args->size){

        if((bytesRead * 100) % args->size == 0){
            //cout << "bytesRead: " << bytesRead << endl;
        }
        KVPair current;
        unsigned long long startDequeue = epoch_time();

        if(inputFile.dequeue(&current)){
            bytesRead += sizeof(KVPair);
        }
        else{
            cerr << "failed to read all bytes from file: " << args->fileName << endl;
            break;
        }
        timeSpentDequeuingFromFile += epoch_time() - startDequeue;

        uint32_t partialKey = bswap_32(*(uint32_t*)&current.key);
        uint32_t lowKey = bswap_32(*(uint32_t*)args->targets.front().lowKey);
        uint32_t highKey = bswap_32(*(uint32_t*)args->targets.back().highKey);
        size_t bucketIndex = (size_t)(partialKey - lowKey) * args->targets.size() / (size_t)(highKey - lowKey);

        Target& target = args->targets.at(bucketIndex);

        if(lhsSmallerRhs((uint8_t*)&(current.key), (uint8_t*)&(target.lowKey))){
            assert(bucketIndex > 0);
            bucketIndex--;
            target = args->targets.at(bucketIndex);
        }
        if(lhsSmallerRhs((uint8_t*)&(target.highKey), (uint8_t*)&(current.key))){
            assert(bucketIndex + 1 < args->targets.size());
            bucketIndex++;
            target = args->targets.at(bucketIndex);
        }            
        if(!lhsSmallerEqualRhs((uint8_t*)&(current.key), (uint8_t*)&(target.highKey)) 
            || !lhsSmallerEqualRhs((uint8_t*)&(target.lowKey), (uint8_t*)&(current.key))){
            cerr << "still does not fit in the target. something went wrong!" << endl;
            assert(false);
        }
        args->bucketLocks.at(bucketIndex)->lock();
        //TODO sort the bucket before sending over the network
        unsigned long long enqueueStart = epoch_time();

        if(!args->targetBuckets.at(bucketIndex)->enqueue(&current)){
            cerr << "peer is no longer available for data, unlocking lock:" << bucketIndex << endl;
            args->bucketLocks.at(bucketIndex)->unlock();
            break;
        }
        timeSpentEnqueuingToNetwork += epoch_time() - enqueueStart;
        args->bucketLocks.at(bucketIndex)->unlock();
    }
    close(inputFile.getFD());
    return nullptr;
}

template<typename T> void mergeSortedBuffers(vector<VirtualBuffer<T>*> sortedBuffers, WorkerConfig config, string outputFileName){

    vector<VirtualBuffer<KVPair>*> finalBuffers;
    size_t sortedPairs = 0;
    unsigned long long startTime = epoch_time();

    for(VirtualBuffer<KVPair>* buffer : sortedBuffers){
        FileBuffer<KVPair>* fileBuffer = (FileBuffer<KVPair>*)buffer;

        if(lseek(fileBuffer->getFD(), 0, SEEK_CUR) != 0){//only flush if some data is already on disk
            fileBuffer->flushBuffer(false);
            bool ok = lseek(fileBuffer->getFD(), 0, SEEK_SET) == 0;
            assert(ok);
        }
    }
    int fileID = 0;

    logDuration("sort phase - flushing buffers", startTime, &logLock);

    do{
        int fd = open(".", O_RDWR | O_DIRECT | O_TMPFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

        if(fd < 0){
            cerr << "Failed to create temporary file in working directory" << endl;
        }
        FileBuffer<KVPair>* tmp = new FileBuffer<KVPair>(config.memoryCount, fd, FILE_IO_SIZE);
        finalBuffers.push_back(tmp);
        
        for(VirtualBuffer<KVPair>* buffer : sortedBuffers){
            buffer->limitDequeue(config.receiveMergeCount);
        }
        sortedPairs = kWayMerge(sortedBuffers, tmp);
        fileID++;
    }while(sortedPairs > 0);



    //TODO possibly have a thread pool here do the first kWayMerge

    FileBuffer<KVPair>* tmp = (FileBuffer<KVPair>*)finalBuffers.back();
    close(tmp->getFD());
    delete tmp;//the back is always empty
    finalBuffers.pop_back();

    logDuration("sort phase - first merge", startTime, &logLock);
    unsigned long long endOfFirstMerge = epoch_time();
    int outputFD = open(outputFileName.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_DIRECT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    if(outputFD < 0){
        cerr << "Failed to open or create file: \"" << outputFileName << "\" for output" << endl;
    }

    FileBuffer<KVPair> finalFile(config.memoryCount, outputFD, FILE_IO_SIZE);

    for(VirtualBuffer<KVPair>* buffer : finalBuffers){
        FileBuffer<KVPair>* fileBuffer = (FileBuffer<KVPair>*)buffer;

        if(lseek(fileBuffer->getFD(), 0, SEEK_CUR) != 0){//only flush if some data is already on disk
            fileBuffer->flushBuffer(false);
            bool ok = lseek(fileBuffer->getFD(), 0, SEEK_SET) == 0;
            assert(ok);
        }
    }
    size_t finalSortedPairs = kWayMerge(finalBuffers, (VirtualBuffer<KVPair>*)&finalFile);
    unsigned long long beginClosingTmpBuffers = epoch_time();

    for(VirtualBuffer<KVPair>* buffer : finalBuffers){
        FileBuffer<KVPair>* tmp = (FileBuffer<KVPair>*)buffer;
        close(tmp->getFD());
        delete tmp;
    }
    logDuration("sort phase - closing tmpBuffers", beginClosingTmpBuffers, &logLock);
    finalFile.flushBuffer(false);
    close(finalFile.getFD());
    logDuration("sort phase - second merge", endOfFirstMerge, &logLock);
    //cout << "DONE, sorted: " << finalSortedPairs << " pairs to: " << outputFileName << endl;
}   

int main(){
    WorkerConfig config = parseConfiguration();

    uint16_t listeningPort = rand() % (PORT_MAX - PORT_MIN + 1);
    int incomingfd = -1;
    
    for(int i = 0; i < SOCKET_CREATE_MAX_ATTEMPTS; i++){
        incomingfd = createSocket(listeningPort);

        if(incomingfd > 0){
            break;
        }
        else{
            listeningPort++;
            listeningPort % (PORT_MAX - PORT_MIN + 1);
        }
    }
    if(incomingfd < 0){
        cerr << "Failed to create listening socket" << endl;
        return -1;
    }
    //cout << "Connecting to master at ip: " << config.masterIP << " port:" << config.masterPort << endl;

    tuple<int, int> connectionInfo = connectToMaster(config.masterIP, config.masterPort, listeningPort);
    int masterConnection = get<0>(connectionInfo);
    int numberOfWorkers = get<1>(connectionInfo);

    if(masterConnection < 0){
        cerr << "Failed to connect to master" << endl;
        return -1;
    }
    vector<VirtualBuffer<KVPair>*> sortedBuffers(numberOfWorkers, nullptr);

    for(int i = 0; i < numberOfWorkers; i++){
        int fd = open(".", O_RDWR | O_DIRECT | O_TMPFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

        if(fd < 0){
            cerr << "Failed to create temporary file in working directory" << endl;
        }
        
        FileBuffer<KVPair>* newFileBuf = new FileBuffer<KVPair>(config.memoryCount, fd, FILE_IO_SIZE);
        assert(newFileBuf->getFD() >= 0);
        sortedBuffers.at(i) = newFileBuf;
    }
    pthread_t incomingHandler;

    struct handleIncomingArgs args = {
        .incomingfd = incomingfd,
        .sortedBuffers = sortedBuffers,
        .receiveMergeCount = config.receiveMergeCount,
        .memoryBufferSize = config.memoryCount,
        .numberOfWorkers = numberOfWorkers};
    pthread_create(&incomingHandler, NULL, handleIncoming, &args);
    
    Task connectToPeers = receiveTask(masterConnection);

    assert(connectToPeers.type == CONNECT_TO_PEERS);
    assert(!connectToPeers.targets.empty());
    vector<Target> peers = connectToPeers.targets;
    vector<int> peerConnections;
    peerConnections.reserve(peers.size());

    for(Target peer : peers){
        int peerfd = connectToPeerWorker(peer.ip, peer.port);
        
        if(peerfd < 0){
            cerr << "failed to connect to peer:" << peer.ip << " port:" << peer.port << endl;
            return -1;
        }
        peerConnections.push_back(peerfd);
        //cout << "Connected to peer at:" << peer.ip << ' ' << peer.port << " sendingfd: " << peerfd << endl;
    }
    //cout << "Connected to all peers, sending OK to master" << endl;
    if(!sendOK(masterConnection)){
        cerr << "Failed to send OK command" << endl;
    }
    Task bucketizeTask = receiveTask(masterConnection);
    //cout << "Second task received." << endl;

    unsigned long long startTime = epoch_time();

    assert(bucketizeTask.type == BUCKETIZE);
    assert(!bucketizeTask.fileName.empty());
    assert(bucketizeTask.targets.empty());

    //cout << "bucketize offset: " << bucketizeTask.offset << " bucketize size;" << bucketizeTask.size << endl;

    vector<pair<size_t, size_t>> splits = splitRange(bucketizeTask.offset / sizeof(KVPair), (bucketizeTask.offset + bucketizeTask.size -1) / sizeof(KVPair), config.threadCount);//-1 because inclusive
    pthread_t bucketizeThreads[config.threadCount];
    bucketizeArgs bucketizeArgs[config.threadCount];

    vector<mutex*> bucketLocks(peers.size(), nullptr);
    vector<FileBuffer<KVPair>*> targetBuckets(peers.size(), nullptr);

    for(size_t i = 0; i < targetBuckets.size(); i++){
        targetBuckets.at(i) = new FileBuffer<KVPair>(NETWORK_IO_SIZE / sizeof(KVPair) + 1, peerConnections.at(i), NETWORK_IO_SIZE);
        bucketLocks.at(i) = new mutex;
    }

    for(int i = 0; i < config.threadCount; i++){
        bucketizeArgs[i] = {
            .fileName = bucketizeTask.fileName,
            .offset = splits.at(i).first * sizeof(KVPair), 
            .size = (splits.at(i).second - splits.at(i).first + 1) * sizeof(KVPair), 
            .memoryBufferSize = config.memoryCount,
            .targets = peers, 
            .targetBuckets = targetBuckets,
            .bucketLocks = bucketLocks,
        };
        //cout << "bucketize size; " << bucketizeArgs[i].size << "offset" << bucketizeArgs[i].offset << endl;
        pthread_create(&bucketizeThreads[i], NULL, bucketize, &bucketizeArgs[i]);
    }
    for(int i = 0; i < config.threadCount; i++){
        pthread_join(bucketizeThreads[i], NULL);
    }

    for(FileBuffer<KVPair>* bucket : targetBuckets){
        bucket->flushBuffer(true);
        close(bucket->getFD());
        delete bucket;
    }
    for(mutex* lock : bucketLocks){
        delete lock;
    }
    pthread_join(incomingHandler, NULL);
    logDuration("bucketize phase", startTime, &logLock);

    if(!sendOK(masterConnection)){
        cerr << "Failed to send finish command" << endl;
    } 
    unsigned long long endBucketizeTime = epoch_time();

    Task sort = receiveTask(masterConnection);
    
    assert(sort.type == SORT);
    assert(!sort.fileName.empty());
    cout << "sorting to output file: " << sort.fileName << endl;
    mergeSortedBuffers(sortedBuffers, config, sort.fileName);

    logDuration("sort phase", endBucketizeTime, &logLock);
    unsigned long long startClosingSortedBuffers = epoch_time();

    for(VirtualBuffer<KVPair>* buffer : sortedBuffers){
        FileBuffer<KVPair>* fileBuffer = (FileBuffer<KVPair>*)buffer;
        close(fileBuffer->getFD());
        delete fileBuffer;
    }
    logDuration("sort phase - closing sortedBuffers", startClosingSortedBuffers, &logLock);
    if(!sendOK(masterConnection)){
        cerr << "Failed to send finish command" << endl;
    }
    unsigned long long endSortTime = epoch_time();

    logDuration("Total time", startTime, &logLock);
    cout << "time spent copying, " << timeSpentCopying << endl;
    return 0;
}
