#include <fstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <assert.h>
#include <byteswap.h>

#include "include/constants.h"
#include "include/sorting.h"
#include "include/tournamenttree.h"
#include "include/kvpair.h"

using namespace std;

void debugPrintHex2(uint8_t* array, int size){
    int i = 0;
    while(i < size ) {
        printf("%02x", array[i]);
        i++;
    }
    printf("\n"); 
}

void sortKVarray(KVPair* array, KVPair* destination, uint32_t arraySize, SortElement* sortedElements){

    for(uint32_t i = 0; i < arraySize; i++){
        destination[i] = array[sortedElements[i].valueIndex];
    }
}

size_t kWayMerge(vector<VirtualBuffer<KVPair>*> input, VirtualBuffer<KVPair>* target){
    size_t pairsMerged = 0;

    if(input.size() == 1){
        return copy(input.front(), target);
    }
    if(input.size() == 2){
        return merge(input.front(), input.back(), target);
    }
    queue<TournamentNode*> nodeQueue;
    vector<TournamentNode*> nodeVector(input.size(), nullptr);
    vector<size_t> pairsRead(input.size(), 0);
    KVPair* readyPairs = new KVPair[input.size()];

    for(int i = 0; i < input.size(); i++){
        TournamentNode* node = new TournamentNode(i);

        if(input.at(i)->dequeue(&readyPairs[i])){
            nodeVector.at(i) = node;
            nodeQueue.push(node);
            node->value = &readyPairs[i];
            node->hasValue = true;
            pairsRead.at(i)++;
        }
        else{
            delete node;
        }  
    }
    if(nodeQueue.empty()){
        
        for(TournamentNode* node : nodeVector){
            delete node;
        }
        delete[] readyPairs;
        return 0;
    }
    if(nodeQueue.size() == 1){
        int updateIndex = nodeQueue.front()->leafID;
        bool ok = target->enqueue(nodeQueue.front()->value);
        assert(ok);
        pairsMerged++;

        for(TournamentNode* node : nodeVector){
            delete node;
        }
        delete[] readyPairs;
        return pairsMerged + copy(input.at(updateIndex), target);
    }
    if(nodeQueue.size() == 2){
        int lhsIndex = nodeQueue.front()->leafID;
        int rhsIndex = nodeQueue.back()->leafID;

        pairsMerged = merge(input.at(lhsIndex), input.at(rhsIndex), target, *nodeQueue.front()->value, *nodeQueue.back()->value);
        delete nodeQueue.front();
        delete nodeQueue.back();
        delete[] readyPairs;
        return pairsMerged;
    }
    TournamentNode* winner = new TournamentNode(0);
    winner->buildTree(nodeQueue);

    while(winner->hasValue){
        bool ok = target->enqueue(winner->value);
        assert(ok);
        pairsMerged++;

        int updateIndex = winner->leafID;
        TournamentNode* updatedNode = nodeVector.at(updateIndex);
        updatedNode->hasValue = input.at(updateIndex)->dequeue(updatedNode->value);
        updatedNode->updateTree();
    }
    delete winner;
    delete[] readyPairs;
    return pairsMerged;
}

size_t merge(VirtualBuffer<KVPair>* leftArray, VirtualBuffer<KVPair>* rightArray, VirtualBuffer<KVPair>* targetArray){
    KVPair lhs, rhs;
    bool lhsHasValue = leftArray->dequeue(&lhs);
    bool rhsHasValue = rightArray->dequeue(&rhs);

    if(lhsHasValue && rhsHasValue){
        return merge(leftArray, rightArray, targetArray, lhs, rhs);
    }
    if(lhsHasValue){
        bool ok = targetArray->enqueue(&lhs);
        assert(ok);
        return 1 + copy(leftArray, targetArray);
    }
    if(rhsHasValue){
        bool ok = targetArray->enqueue(&rhs);
        assert(ok);
        return 1 + copy(rightArray, targetArray);
    }
    return 0;
}

size_t merge(VirtualBuffer<KVPair>* leftArray, VirtualBuffer<KVPair>* rightArray, VirtualBuffer<KVPair>* targetArray, KVPair& initialLhs, KVPair& initalRhs){

    size_t pairsMerged = 0;
    KVPair lhs = initialLhs;
    KVPair rhs = initalRhs;
    bool lhsHasValue = true;
    bool rhsHasValue = true;

    while(lhsHasValue && rhsHasValue){

        if(lhsSmallerRhs(lhs, rhs)){
            bool ok = targetArray->enqueue(&lhs);
            assert(ok);
            
            lhsHasValue = leftArray->dequeue(&lhs);
        }
        else{
            bool ok = targetArray->enqueue(&rhs);
            assert(ok);
            rhsHasValue = rightArray->dequeue(&rhs);
        }
        pairsMerged++;
    }

    if(lhsHasValue){
        bool ok = targetArray->enqueue(&lhs);
        assert(ok);
        return pairsMerged + 1 + copy(leftArray, targetArray);;
    }
    if(rhsHasValue){
        bool ok = targetArray->enqueue(&rhs);
        assert(ok);
        return pairsMerged + 1 + copy(rightArray, targetArray);;
    }
    return pairsMerged;
}





///////////////////////////////////////QUICKSORT///////////////////////////////////////

size_t partition(SortElement* elementArray, size_t low, size_t high, KVPair* KVArray){

    struct SortElement pivot = elementArray[high];
    size_t i = low;
   
    for(size_t j = low; j < high; j++){

        if(lhsSmallerRhs(&elementArray[j], &pivot, KVArray)){
            swap(&elementArray[i], &elementArray[j]);
            i++;
        }
    }
    swap(&elementArray[i], &elementArray[high]);
    return i;
}

size_t partition(KVPair* array, size_t low, size_t high){

    KVPair pivot = array[high];
    size_t i = low;
   
    for(size_t j = low; j < high; j++){

        if(lhsSmallerRhs(array[j], pivot)){
            swap(&array[i], &array[j]);
            i++;
        }
    }
    swap(&array[i], &array[high]);
    return i;
}

void quickSort(SortElement* elementArray, size_t low, size_t high, KVPair* KVArray){
    
    if(low < high){       
        size_t partitionIndex = partition(elementArray, low, high, KVArray);

        if(partitionIndex > 1){
            quickSort(elementArray, low, partitionIndex - 1, KVArray);
        }
        quickSort(elementArray, partitionIndex + 1, high, KVArray);
    }
}

void quickSort(KVPair* array, size_t low, size_t high){
    
    if(low < high){       
        size_t partitionIndex = partition(array, low, high);

        if(partitionIndex > 1){
            quickSort(array, low, partitionIndex - 1);
        }
        quickSort(array, partitionIndex + 1, high);
    }
}

void sort(KVPair* array, size_t size){
    quickSort(array, 0, size -1);
    assert(isOrdered(array, size));
}

void sort(RingQueue<KVPair>* queue){//TODO possibly make this an internal RingQueue function to speed things up.
    size_t originalSize = queue->size();
    
    if(originalSize < 2){
        return;
    }
    KVPair* tmpArray = new KVPair[originalSize];

    for(size_t i = 0; i < originalSize; i++){
        bool ok = queue->dequeue(&tmpArray[i]);
        assert(ok);
    }
    sort(tmpArray, originalSize);
    assert(isOrdered(tmpArray, originalSize));

    for(size_t i = 0; i < originalSize; i++){
        bool ok = queue->enqueue(&tmpArray[i]);
        assert(ok);
    }
    delete[] tmpArray;
}


bool isOrdered(KVPair* array, size_t size){
    bool sorted = true;

    for(size_t i = 1; i < size; i++){

        if(!lhsSmallerEqualRhs(array[i -1], array[i])){
            sorted = false;
            break;
        }
    }
    return sorted;
}


bool isOrdered(RingQueue<KVPair>* queue){
    

    size_t oldSize = queue->size();
    KVPair* tmpArray = new KVPair[oldSize];

    for(size_t i = 0; i < oldSize; i++){
        bool ok = queue->dequeue(&tmpArray[i]);
        assert(ok);
    }
    bool ordered = isOrdered(tmpArray, oldSize);

    for(size_t i = 0; i < oldSize; i++){
        bool ok = queue->enqueue(&tmpArray[i]);
        assert(ok);
    }
    delete[] tmpArray;
    return ordered;
}
