#ifndef KVPAIRH
#define KVPAIRH

#include <cstdint>
#include "constants.h"

using namespace std;

struct KVPair{
    uint8_t key[KEY_SIZE];
    uint8_t value[VALUE_SIZE];
} __attribute__((packed));

struct SortElement{
    uint32_t partialKey;
    uint32_t valueIndex;
} __attribute__((packed));

bool lhsSmallerRhs(struct SortElement* lhs, struct SortElement* rhs, struct KVPair* KVArray);
bool lhsSmallerRhs(struct KVPair& lhs, struct KVPair& rhs);
bool lhsSmallerEqualRhs(struct KVPair& lhs, struct KVPair& rhs);
bool lhsSmallerRhs(uint8_t* lhsKey, uint8_t* rhsKey);
bool lhsSmallerEqualRhs(uint8_t* lhsKey, uint8_t* rhsKey);

SortElement makeSortElement(KVPair* pair, uint32_t index);

void swap(struct SortElement* lhs, struct SortElement* rhs);
void swap(KVPair* lhs, KVPair* rhs);

#endif