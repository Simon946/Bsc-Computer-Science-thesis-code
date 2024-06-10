#include <string>
#include <string.h>
#include "include/target.h"
#include "include/constants.h"

using namespace std;

size_t getSize(Target t){
    return KEY_SIZE + KEY_SIZE + sizeof(t.port) + t.ip.size() + 1;
}

void toArray(Target t, uint8_t* destination){
    memcpy(destination, t.lowKey, KEY_SIZE);
    destination += KEY_SIZE;

    memcpy(destination, t.highKey, KEY_SIZE);
    destination += KEY_SIZE;

    memcpy(destination, t.ip.c_str(), t.ip.length() + 1);
    destination += (t.ip.length() + 1);

    memcpy(destination, &t.port, sizeof(t.port));
}

Target targetfromArray(uint8_t* source){
    Target result;
    memcpy(&result.lowKey, source, KEY_SIZE);
    source += KEY_SIZE;

    memcpy(&result.highKey, source, KEY_SIZE);
    source += KEY_SIZE;

    result.ip = string((char*)source);
    source += result.ip.size() + 1;//add 1 for null terminator
    result.port = *(uint16_t*)source;
    return result;
}
