#ifndef MEASUREH
#define MEASUREH

#include <string>
#include <mutex>
#include <time.h>
#include <sys/time.h>

using namespace std;

unsigned long long epoch_time();
string printTime(unsigned long long time);
void logDuration(string functionName, unsigned long long startTime, mutex* lock);

extern unsigned long long timeSpentCopying;

#endif