#ifndef TARGETH
#define TARGETH

#include <string>
#include <string.h>
#include "constants.h"

using namespace std;

struct Target{
    uint8_t lowKey[KEY_SIZE] = {0};//both inclusive
    uint8_t highKey[KEY_SIZE] = {0};
    std::string ip;
    uint16_t port;
};

size_t getSize(Target t);

void toArray(Target t, uint8_t* destination);

Target targetfromArray(uint8_t* source);

#endif