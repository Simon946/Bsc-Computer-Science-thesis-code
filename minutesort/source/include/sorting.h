#ifndef SORTINGH
#define SORTINGH

#include <cstdint>
#include "kvpair.h"
#include "filebuffer.h"
#include "constants.h"
#include "tournamenttree.h"
#include "measure.h"

using namespace std;


void sortKVarray(KVPair* array, KVPair* destination, uint32_t arraySize, SortElement* sortedElements);



//quicksort:
void swap(struct SortElement* lhs, struct SortElement* rhs);
void swap(KVPair* lhs, KVPair* rhs);
size_t partition(SortElement* elementArray, size_t low, size_t high, KVPair* KVArray);
size_t partition(KVPair* array, size_t low, size_t high);
void quickSort(SortElement* elementArray, size_t low, size_t high, KVPair* KVArray);
void quickSort(KVPair* array, size_t low, size_t high);
void sort(KVPair* array, size_t size);
void sort(RingQueue<KVPair>* queue);
bool isOrdered(KVPair* array, size_t size);
bool isOrdered(RingQueue<KVPair>* queue);
size_t kWayMerge(vector<VirtualBuffer<KVPair>*> input, VirtualBuffer<KVPair>* target);
size_t merge(VirtualBuffer<KVPair>* leftArray, VirtualBuffer<KVPair>* rightArray, VirtualBuffer<KVPair>* targetArray);
size_t merge(VirtualBuffer<KVPair>* leftArray, VirtualBuffer<KVPair>* rightArray, VirtualBuffer<KVPair>* targetArray, KVPair& initialLhs, KVPair& initialRhs);
//templates:

template<typename T> size_t copy(VirtualBuffer<T>* from, VirtualBuffer<T>* to){
    size_t pairsCopied = 0;
    T tmp;
    unsigned long long startCopy = epoch_time();

    //size_t res = from->move(*to);//sometimes slow maybe?
    //timeSpentCopying += epoch_time() - startCopy;
    //return res;


    while(from->dequeue(&tmp)){

        if(!to->enqueue(&tmp)){
            cerr << "in copy, read success but write to target failed" << endl;
            assert(false);
            break;
        }
        pairsCopied++;
    }
    timeSpentCopying += epoch_time() - startCopy;
    return pairsCopied;
}



template<typename T> vector<pair<T, T>> splitRange(T startValue, T endValue, size_t numberOfSplits){
    assert(endValue - startValue >= numberOfSplits -1);
    assert(endValue > startValue);

    vector<pair<T, T>> splits(numberOfSplits);
    T sizePerSplit = ((endValue - startValue -1) / numberOfSplits) + 1;

    for(size_t i = 0; i < numberOfSplits -1; i++){
        splits.at(i) = {startValue, startValue + sizePerSplit -1};
        startValue += sizePerSplit;
    }
    splits.at(numberOfSplits -1) = {startValue, endValue};
    return splits;
}



#endif