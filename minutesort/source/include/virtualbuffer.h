#ifndef VIRTUALBUFFERH
#define VIRTUALBUFFERH

#include <iostream>
#include <string>
#include <tuple>

using namespace std;

template<typename T> class VirtualBuffer{
    public:
        virtual bool enqueue(T* source) = 0;
        virtual bool dequeue(T* destination) = 0;
        virtual void limitDequeue(size_t maxElements) = 0;
        virtual pair<T*, size_t> directFutureEnqueue();//Data must be confirmed before it can be dequeued. size_t == 0 does NOT mean enqueue() fails.
        virtual pair<T*, size_t> directPeek();//dangerous. Data must be read from the returned T* before it can be overwritten by enqueue size_t == 0 does NOT mean dequeue() fails
        virtual void confirmDirectEnqueue(size_t actualElementsEnqueued);//call this when directFutureEnqueue() writes data
        virtual void confirmDirectDequeue(size_t actualElementsDequeued);//call this when directPeek() reads data.
        size_t move(VirtualBuffer<T>& destination);
};

template<typename T> size_t VirtualBuffer<T>::move(VirtualBuffer<T>& destination){
    
    size_t totalElementsMoved = 0;
    size_t elementsMoved = 0;

    do{
        pair<T*, size_t> target = destination.directFutureEnqueue();
        pair<T*, size_t> source = this->directPeek();

        elementsMoved = min(source.second, target.second);
        //cout << "in move, ready to move:" << elementsMoved << "source:" << source.second << " target: " << target.second << endl;

        if(elementsMoved > 0){
            memcpy(target.first, source.first, elementsMoved * sizeof(T));
            destination.confirmDirectEnqueue(elementsMoved);
            this->confirmDirectDequeue(elementsMoved);
        }
        else if(target.second >= 1 && this->dequeue(target.first)){
            destination.confirmDirectEnqueue(1);
            elementsMoved = 1;
        }
        else{
            T tmp;
            if(this->dequeue(&tmp)){
                destination.enqueue(&tmp);
                elementsMoved = 1;
            }
        }
        totalElementsMoved += elementsMoved;

        
    }while(elementsMoved > 0);
    //cout << "elementsmoved:" << totalElementsMoved << endl;
    return totalElementsMoved;
}


#endif
