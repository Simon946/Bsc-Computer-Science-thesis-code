#include <string>
#include <vector>
#include <iostream>
#include <assert.h>
#include "include/target.h"
#include "include/task.h"


using namespace std;

void printHex(uint8_t* array, int size){
    int i = 0;
    while(i < size ) {
        printf("%02x", array[i]);
        i++;
    }
    printf("\n");
}


size_t getSize(Task t){
    size_t size = sizeof(uint32_t) + sizeof(uint8_t) + t.fileName.size() + 1 + sizeof(t.offset) + sizeof(t.size);
    
    for(Target target : t.targets){
        size += getSize(target);
    }
    return size + sizeof(uint32_t);
}

Task taskFromArray(uint8_t* source){

    uint8_t* readPtr = source;
    uint32_t taskSize = *(uint32_t*)readPtr;
    readPtr += sizeof(uint32_t);
    Task result;

    result.type = (TaskType)*readPtr;
    readPtr += sizeof(uint8_t); 
    result.fileName = string((char*)readPtr);
    readPtr += result.fileName.length() + 1;//add 1 for null terminator
    assert(readPtr - source <= taskSize);
    result.offset = *(size_t*)readPtr;

    readPtr += sizeof(result.offset);
    assert(readPtr - source <= taskSize);

    result.size = *(size_t*)readPtr;
    readPtr += sizeof(result.size);
    assert(readPtr - source <= taskSize);

    uint32_t numberOfTargets = *(uint32_t*)readPtr;
    readPtr += sizeof(numberOfTargets);

    for(uint32_t i = 0; i < numberOfTargets; i++){
        Target t = targetfromArray(readPtr);

        readPtr += getSize(t);
        result.targets.push_back(t);
        assert(readPtr - source <= taskSize);
    }
    return result;
}

void toArray(Task t, uint8_t* destination){
    uint32_t taskSize = getSize(t);

    uint8_t* debug = destination;
    memcpy(destination, &taskSize, sizeof(uint32_t));
    destination += sizeof(uint32_t);

    *destination = (uint8_t)t.type;
    destination += sizeof(uint8_t);

    memcpy(destination, t.fileName.c_str(), t.fileName.length() + 1);
    destination += t.fileName.length() + 1;

    mempcpy(destination, &t.offset, sizeof(t.offset));
    destination += sizeof(t.offset);

    mempcpy(destination, &t.size, sizeof(t.size));
    destination += sizeof(t.size);

    uint32_t targetSize = t.targets.size();
    memcpy(destination, &targetSize, sizeof(uint32_t));
    destination += sizeof(uint32_t);

    for(Target target : t.targets){
        toArray(target, destination);
        destination += getSize(target);
    }
}

void printTask(Task t){
    cout << "size is: " << getSize(t) << endl;
    cout << "type is: " << t.type << endl;
    cout << "filename:" << t.fileName << endl;
    cout << "offset: " << t.offset << endl;
    cout << "size: " << t.size << endl;
    
    for(Target target : t.targets){
        cout << target.ip << ' ' << target.port << " [" << (unsigned int )target.lowKey[0] << ',' << (unsigned int)target.highKey[0] << ']' << endl;
    }
    cout << endl;

}