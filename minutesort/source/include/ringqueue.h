#ifndef RINGQUEUEH
#define RINGQUEUEH

#include <iostream>
#include <string>
#include <tuple>

#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "virtualbuffer.h"

using namespace std;

template<typename T> class RingQueue : public VirtualBuffer<T> {
    public:
        RingQueue(size_t maxElements);
        RingQueue();
        void init(size_t maxElements);
        ~RingQueue();
        bool enqueue(T* source) override;
        bool dequeue(T* destination) override;
        bool peek(T* destination);
        size_t size(){
            //cout << "calculating size, head = " << head << " tail: " << tail << "bufferSIze:" << bufferSize << endl;
            return (head >= tail)? head - tail : bufferSize - (tail - head);};
        void limitDequeue(size_t maxElements) override;
        pair<T*, size_t> directFutureEnqueue() override;
        pair<T*, size_t> directPeek() override;
        void confirmDirectEnqueue(size_t actualElementsEnqueued) override;
        void confirmDirectDequeue(size_t actualElementsDequeued) override;
        void move(RingQueue<T>& destination);
    private:
        T* buffer = nullptr;
        size_t head = 0;
        size_t tail = 0;
        size_t bufferSize = 0;
        size_t readBudget = SIZE_MAX;
};

template<typename T> RingQueue<T>::RingQueue(size_t maxElements){
    assert(maxElements > 0);
    
    bufferSize = maxElements + 1;//1 position is always empty.
    buffer = new T[bufferSize];
}

template<typename T> RingQueue<T>::RingQueue(){
    //do nothing, leave buffer= nullptr, enqueue and dequeue both return false.
}

template<typename T> void RingQueue<T>::init(size_t maxElements){
    assert(maxElements > 0);
    assert(buffer == nullptr);

    head = 0;
    tail = 0;
    bufferSize = maxElements + 1;//1 position is always empty.
    buffer = new T[bufferSize];
}

template<typename T> RingQueue<T>::~RingQueue(){
    //assert(buffer != nullptr);
    delete[] buffer;
}

template<typename T> bool RingQueue<T>::enqueue(T* source){
    
    if((head + 1) % bufferSize == tail){
        //cerr << "enqueue failed, buffer full" << endl;
        return false;
    }
    buffer[head] = *source;
    //cout << "enqueue at: " << (int*)&(buffer[head]) << "size: " << sizeof(T) << endl;
    head = (head + 1) % bufferSize;
    return true;
}

template<typename T> bool RingQueue<T>::dequeue(T* destination){

    if(!peek(destination) || readBudget == 0){
        return false;
    }
    tail = (tail + 1) % bufferSize;
    readBudget--;
    return true;
}

template<typename T> bool RingQueue<T>::peek(T* destination){

    if(head == tail){
        return false;
    }
    //cout << "head::" << head << "tail: " << tail << "bufferSIze:" << bufferSize << " pos;:" << (int*)&(buffer[tail]) << endl;
    *destination = buffer[tail];
    return true;
}

template<typename T> void RingQueue<T>::limitDequeue(size_t maxElements){
    readBudget = maxElements;
}

template<typename T> pair<T*, size_t> RingQueue<T>::directFutureEnqueue(){
    //cout << "in direct future enqueue, head=" << head << " tail= " << tail << "size() " << size() << "buffersize: " << bufferSize << endl;
    if(buffer == nullptr){
        return {nullptr, 0};
    }
    size_t elementsEnqueued = 0;

    if(head >= tail){
        if(tail == 0){
            elementsEnqueued = bufferSize - head -1;
        }
        else{
            elementsEnqueued = bufferSize - head;
        }
    }
    else{
        elementsEnqueued = tail - head -1;
    }
    assert(elementsEnqueued > 0 || size() + 1 == bufferSize);
    assert(elementsEnqueued < bufferSize);
    //cout << "calculated elementsenqueued as: " << elementsEnqueued << endl;
    return {&(buffer[head]), elementsEnqueued};
}

template<typename T> pair<T*, size_t> RingQueue<T>::directPeek(){

    if(buffer == nullptr){
        return {nullptr, 0};
    }
    size_t bytesAvailable = 0;
    if(tail <= head){
        bytesAvailable = head - tail;
    }
    else{//head < tail
        bytesAvailable = bufferSize - tail;
    }
    assert(bytesAvailable > 0 || size() == 0);
    assert(bytesAvailable <= size());
    //cout << "in directPeek, head=" << head << "tail: " << tail << "size: " << size() << "bytesAvailable: " << bytesAvailable << "buffersize:" << bufferSize << endl;
    for(int i = 0; i < bytesAvailable; i++){
        uint8_t* ptr = (uint8_t*)(&buffer[tail]);
        if(ptr[i] == 0){
            //cout << "0";
        }

    }
    //cout << "in directpeek, at: " << (int*)&(buffer[tail]) << endl;
    return {&(buffer[tail]), min(bytesAvailable, readBudget)};
}

template<typename T> void RingQueue<T>::confirmDirectEnqueue(size_t actualElementsEnqueued){
    size_t previousSize = size();
    head = (head + actualElementsEnqueued) % bufferSize;
    assert(size() == previousSize + actualElementsEnqueued);
}

template<typename T> void RingQueue<T>::confirmDirectDequeue(size_t actualElementsDequeued){
    size_t previousSize = size();
    tail = (tail + actualElementsDequeued) % bufferSize;
    readBudget -= actualElementsDequeued;
    assert(size() == previousSize - actualElementsDequeued);
}

template<typename T> void RingQueue<T>::move(RingQueue<T>& destination){
    assert(false);
    return;
    size_t elementsToMove = min(this->readBudget, min(this->size(), destination.bufferSize - destination.size() -1));

    pair<T*, size_t> target = destination.directEnqueue();
    pair<T*, size_t> source = this->directDequeue();

    while(elementsToMove > 0){

        if(target.second == 0){
            target = destination.directEnqueue();
            destination.confirmDirectEnqueue(target.second);
        }
        if(source.second == 0){
            source = this->directDequeue();
            this->confirmDirectEnqueue(source.second);
        }
        size_t elementsMoved = min(source.second, target.second);
        memcpy(target.first, source.first, elementsMoved * sizeof(T));
        target.first += elementsMoved;
        target.second -= elementsMoved;
        source.first += elementsMoved;
        source.second -= elementsMoved;
        elementsToMove -= elementsMoved;
    }
}

#endif
