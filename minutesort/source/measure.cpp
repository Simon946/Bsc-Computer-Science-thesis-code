#include <iostream>
#include <string>
#include <mutex>

#include <time.h>
#include <sys/time.h>
#include "include/measure.h"

using namespace std;

unsigned long long timeSpentCopying = 0;

unsigned long long epoch_time(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long usec = (unsigned long long) (tv.tv_sec) * 1000000 + (unsigned long long) (tv.tv_usec);
    return usec;
}

string printTime(unsigned long long time){
    if(time > 1000000){
        return to_string(time / 1000000) + "s";
    }
    else if(time > 1000){
       return to_string(time / 1000) + "ms";
    }
    else{
        return to_string(time) + "us";
    }
    return "no time";
}

void logDuration(string functionName, unsigned long long startTime, mutex* lock){
    unsigned long long endTime = epoch_time();
    lock->lock();
    cout << functionName << ',' << endTime - startTime << endl;
    lock->unlock();
}