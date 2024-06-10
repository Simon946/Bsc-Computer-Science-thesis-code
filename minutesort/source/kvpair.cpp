#include <fstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <assert.h>
#include <byteswap.h>

#include "include/kvpair.h"

using namespace std;

bool lhsSmallerRhs(struct SortElement* lhs, struct SortElement* rhs, struct KVPair* KVArray){//returns true if lhs < rhs //bit wasteful to pass as pointer

    if(lhs->partialKey != rhs->partialKey){
        return lhs->partialKey < rhs->partialKey;
    }
    return lhsSmallerRhs(KVArray[lhs->valueIndex], KVArray[rhs->valueIndex]);
}

bool lhsSmallerRhs(struct KVPair& lhs, struct KVPair& rhs){
    int i = 0;
    return memcmp((char*)lhs.key, (char*)rhs.key, KEY_SIZE) < 0;



    while(lhs.key[i] == rhs.key[i] && i < KEY_SIZE){
        i++;
    }
    return lhs.key[i] < rhs.key[i];
}

bool lhsSmallerEqualRhs(struct KVPair& lhs, struct KVPair& rhs){

    int i = 0;
    return memcmp((char*)lhs.key, (char*)rhs.key, KEY_SIZE) <= 0;

    while(lhs.key[i] == rhs.key[i] && i < KEY_SIZE){
        i++;
    }
    return lhs.key[i] <= rhs.key[i];
}

bool lhsSmallerRhs(uint8_t* lhsKey, uint8_t* rhsKey){
    int i = 0;
    return memcmp((char*)lhsKey, (char*)rhsKey, KEY_SIZE) < 0;

    while(lhsKey[i] == rhsKey[i] && i < KEY_SIZE){
        i++;
    }
    return lhsKey[i] < rhsKey[i];
}

bool lhsSmallerEqualRhs(uint8_t* lhsKey, uint8_t* rhsKey){
        int i = 0;
        return memcmp((char*)lhsKey, (char*)rhsKey, KEY_SIZE) <= 0;

    while(lhsKey[i] == rhsKey[i] && i < KEY_SIZE){
        i++;
    }
    return lhsKey[i] <= rhsKey[i];
}

SortElement makeSortElement(KVPair* pair, uint32_t index){
    return {.partialKey = bswap_32(*(uint32_t*)&pair->key), .valueIndex = index};
}

void swap(struct SortElement* lhs, struct SortElement* rhs){
    struct SortElement tmp = *lhs;
    *lhs = *rhs;
    *rhs = tmp;
}

void swap(KVPair* lhs, KVPair* rhs){
    KVPair tmp = *lhs;
    *lhs = *rhs;
    *rhs = tmp;
}
