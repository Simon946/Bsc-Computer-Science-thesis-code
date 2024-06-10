#ifndef TASKH
#define TASKH

#include <string>
#include <vector>
#include "target.h"

using namespace std;

enum TaskType{
    CONNECT_TO_PEERS = 0,
    BUCKETIZE = 1,
    SORT = 2
};


struct Task{
    enum TaskType type = CONNECT_TO_PEERS; 
    string fileName = "";
    size_t offset = 0;
    size_t size = 0;
    vector<Target> targets = {};
};



//task types: connect to peers, bucketize, sort,
size_t getSize(Task t);

Task taskFromArray(uint8_t* source);

void toArray(Task t, uint8_t* destination);

void printTask(Task t);
#endif