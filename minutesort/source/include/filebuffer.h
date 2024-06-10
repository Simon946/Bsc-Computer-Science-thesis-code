#ifndef FILEBUFFERH
#define FILEBUFFERH

#include <iostream>
#include <string>
#include <tuple>

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <malloc.h>

#include "virtualbuffer.h"
#include "ringqueue.h"
#include "constants.h"

using namespace std;

template<typename T> class FileBuffer : public VirtualBuffer<T> {
    public:
        FileBuffer(size_t maxElements, int newFD, size_t newIOSize);
        bool enqueue(T* source) override;
        bool dequeue(T* destination) override;
        int getFD(){return fd;};
        void limitDequeue(size_t maxElements) override;
        void flushBuffer(bool allowPartialIOSize = true);
        pair<T*, size_t> directFutureEnqueue() override;
        pair<T*, size_t> directPeek() override;
        void confirmDirectEnqueue(size_t actualElementsEnqueued) override;
        void confirmDirectDequeue(size_t actualElementsDequeued) override;
        
    private:
        int fd = -1;
        RingQueue<uint8_t> buffer;
        size_t bufferSize = 0;//in bytes
        size_t readBudget = SIZE_MAX;
        size_t IOSize = 0;//how many bytes to read/write at once to the fd
        bool writeIOSize();
        bool readIOSize();
};


template<typename T> FileBuffer<T>::FileBuffer(size_t maxElements, int newFD, size_t newIOSize){
    assert(newFD >= 0);
    fd = newFD;
    IOSize = newIOSize;
    bufferSize = max(maxElements * sizeof(T), IOSize + sizeof(T));
    buffer.init(bufferSize);
}

template<typename T> bool FileBuffer<T>::enqueue(T* source){

    for(int i = 0; i < 1; i++){
        uint8_t* ptr = (uint8_t*)source;
        //cout << (int)ptr[i];
    }
    //cout << endl;


    if(buffer.size() >= IOSize){
        bool ok = writeIOSize();
        assert(ok);
    }
    pair<uint8_t*, size_t> direct = buffer.directFutureEnqueue();

    if(direct.second == 0){
        cerr << "no space in internal buffer. weird" << endl;
        return false;
    }
    if(direct.second >= sizeof(T)){
        memcpy(direct.first, source, sizeof(T));
        buffer.confirmDirectEnqueue(sizeof(T));
        return true;
    }
    size_t bytesCopied = direct.second;
    memcpy(direct.first, source, bytesCopied);
    buffer.confirmDirectEnqueue(bytesCopied);
    direct = buffer.directFutureEnqueue();
    assert(direct.second >= sizeof(T) - bytesCopied);
    memcpy(direct.first, (uint8_t*)source + bytesCopied, sizeof(T) - bytesCopied);
    buffer.confirmDirectEnqueue(sizeof(T) - bytesCopied);
    return true;
}

template<typename T> bool FileBuffer<T>::dequeue(T* destination){
    //cout << "++++++++++++ start dequeue, buffersize: " << buffer.size() << endl;
    if(readBudget == 0){
        return false;
    }
    if(buffer.size() < sizeof(T)){
        //cout << "buffer size is too small, executing readIOSize now" << endl;

        if(!readIOSize()){
            //cout << "not enough in buffer. buffer size:" << buffer.size() <<  "and readIO failed" << endl;
            return false;
        }
    }
    //cout << "++++++++ about to do first directpeeL" << endl;
    pair<uint8_t*, size_t> direct = buffer.directPeek();

    if(direct.second == 0){
        cerr << "somehow direct peek 0" << endl;
        return false;
    }
    if(direct.second >= sizeof(T)){
        memcpy(destination, direct.first, sizeof(T));
        buffer.confirmDirectDequeue(sizeof(T));
        readBudget--;
        return true;
    }
    //cout << "+++ first direct peek is not enough, only got: " << direct.second << " bytes available " << endl;
    size_t firstBytes = direct.second;
    memcpy(destination, direct.first, firstBytes);
    buffer.confirmDirectDequeue(firstBytes);
    direct = buffer.directPeek();
    assert(direct.second >= sizeof(T) - firstBytes);
    memcpy((uint8_t*)destination + firstBytes, direct.first, sizeof(T) - firstBytes);
    buffer.confirmDirectDequeue(sizeof(T) - firstBytes);
    readBudget--;
    return true;
}

template<typename T> void FileBuffer<T>::limitDequeue(size_t maxElements){
    readBudget = maxElements;
}

template<typename T> void FileBuffer<T>::flushBuffer(bool allowPartialIOSize){

    while(buffer.size() >= IOSize){
        writeIOSize();
    }
    if(buffer.size() == 0){
        return;
    }

    uint8_t* writeBuffer = (uint8_t*)memalign(O_DIRECT_MEMORY_ALIGNMENT, IOSize);
    size_t writePos = 0;

    while(buffer.size() > 0){
        pair<uint8_t*, size_t> direct = buffer.directPeek();
        memcpy((uint8_t*)writeBuffer + writePos, direct.first, direct.second);
        buffer.confirmDirectDequeue(direct.second);
        writePos += direct.second;
        assert(writePos <= IOSize);
    }
    if(writePos == 0){
        cerr << "Failed to flush buffer! Unable to take bytes out of internal buffer" << endl;
        free(writeBuffer);
        return;
    }
    if(allowPartialIOSize){
        bool ok = write(fd, writeBuffer, writePos) == writePos;
        assert(ok);
    }
    else{
        memset(writeBuffer + writePos, 'x', IOSize - writePos);//padding
        size_t currentFileSize = lseek(fd, 0, SEEK_CUR);

        ssize_t writtenSize = write(fd, writeBuffer, IOSize);

        if(writtenSize == IOSize){
            bool ok = ftruncate(fd, currentFileSize + writePos) == 0;
            assert(ok);
        }
        else{
            cerr << "Write failed while flushing to file!";
            cerr << "Iosize: " << IOSize << " errno=" << errno  << " written size:" << writtenSize << " alignment:" << O_DIRECT_MEMORY_ALIGNMENT << " fd:" << fd << endl;
        }
    }
    free(writeBuffer);
}

template<typename T> pair<T*, size_t> FileBuffer<T>::directFutureEnqueue(){
    
    if(bufferSize - buffer.size() < sizeof(T)){
        bool ok = writeIOSize();
        assert(ok);
    }
    pair<uint8_t*, size_t> direct = buffer.directFutureEnqueue();
    return {(T*)direct.first, direct.second / sizeof(T)};
}

template<typename T> pair<T*, size_t> FileBuffer<T>::directPeek(){
    //cout << "in diretctPeek, buffersize: " << buffer.size() << endl;
    
    if(buffer.size() < sizeof(T)){
        //cout << "in direct peek, buffersize; " << buffer.size() << " trying to readIOSize" << endl;
        if(!readIOSize()){
            assert(buffer.size() == 0);
            //cout << "in directPeek, readIOsize failed" << endl;
            return {nullptr, 0};
        }
    }
    pair<uint8_t*, size_t> direct = buffer.directPeek();
    //cout << "direct size: " << direct.second << "bytes, readbudget: " << readBudget << endl;
    return {(T*)direct.first, min(direct.second / sizeof(T), readBudget)};
}

template<typename T> void FileBuffer<T>::confirmDirectEnqueue(size_t actualElementsEnqueued){
   buffer.confirmDirectEnqueue(actualElementsEnqueued * sizeof(T));
}

template<typename T> void FileBuffer<T>::confirmDirectDequeue(size_t actualElementsDequeued){
    readBudget -= actualElementsDequeued;
    buffer.confirmDirectDequeue(actualElementsDequeued * sizeof(T));
}

template<typename T> bool FileBuffer<T>::writeIOSize(){

    if(buffer.size() < IOSize){
        return false;
    }
    pair<uint8_t*, size_t> internalBuffer = buffer.directPeek();

    if(internalBuffer.second < IOSize){
        uint8_t* writeBuffer = (uint8_t*)memalign(O_DIRECT_MEMORY_ALIGNMENT, IOSize);
        memcpy(writeBuffer, internalBuffer.first, internalBuffer.second);
        buffer.confirmDirectDequeue(internalBuffer.second);

        pair<uint8_t*, size_t> remainder = buffer.directPeek();
        assert(remainder.second + internalBuffer.second >= IOSize);//sanity check
        memcpy((uint8_t*)writeBuffer + internalBuffer.second, remainder.first, IOSize - internalBuffer.second);
        buffer.confirmDirectDequeue(IOSize - internalBuffer.second);
        bool ok = write(fd, writeBuffer, IOSize) == IOSize;
        assert(ok);
        free(writeBuffer);
    }
    else{
        uint8_t* source = internalBuffer.first;

        if((uint64_t)internalBuffer.first % O_DIRECT_MEMORY_ALIGNMENT != 0){
            source = (uint8_t*)memalign(O_DIRECT_MEMORY_ALIGNMENT, IOSize);
            memcpy(source, internalBuffer.first, IOSize);
        }
        bool ok = write(fd, source, IOSize) == IOSize;
        assert(ok);

        if(source != internalBuffer.first){
            free(source);
        }
        buffer.confirmDirectDequeue(IOSize);
    }
    return true;
}

template<typename T> bool FileBuffer<T>::readIOSize(){
    //cout << "start here in readiOSize" << endl;

    if(buffer.size() >= IOSize){
        cerr << "no iosize read when buffer has more dataa " << endl;
        return false;
    }
    off_t filePos = lseek(fd, 0, SEEK_CUR);
    //cout << "the filepos is: " << filePos << " iosize: " << IOSize << endl;

    if(filePos > 0 && filePos % IOSize != 0){
        //fix offset
        //cout << "we need to fix the offset" << endl;
        uint8_t* readBuffer = (uint8_t*)memalign(O_DIRECT_MEMORY_ALIGNMENT, IOSize);

        off_t fileBlockStart = filePos - (filePos % IOSize);
        lseek(fd, fileBlockStart, SEEK_SET);
        ssize_t size = read(fd, readBuffer, IOSize);
        //cout << "read: " << size << endl;

        if(size <= 0 || size <= filePos % IOSize){
            free(readBuffer);
            //cout << "read is smaller than the data we care about: read: " << size << " filepos: " << filePos << " FILEIOSIZE: " << FILE_IO_SIZE << endl;
            return false;
        }
        for(size_t i = filePos % IOSize; i < size; i++){
            
            if(!buffer.enqueue(readBuffer + i)){
                free(readBuffer);
                cerr << "1 byte enqueue failed" << endl;
                assert(false);
                return false;
            }
            //cout << "1 byte enqueued <)" << endl;
        }
        free(readBuffer);
        return true;
    }
    //cout << "buffer size: " << buffer.size() << endl;
    pair<uint8_t*, size_t> spaceAvailable = buffer.directFutureEnqueue();
    //cout << "space available: " << spaceAvailable.second << endl;

    if(spaceAvailable.second >= IOSize){
       //cout << "enough available for full iosize, reading directly to it" << endl;
        uint8_t* destination = spaceAvailable.first;

        if((uint64_t)spaceAvailable.first % O_DIRECT_MEMORY_ALIGNMENT != 0){
            //fix misaligned
            destination = (uint8_t*)memalign(O_DIRECT_MEMORY_ALIGNMENT, IOSize);
            //cout << "the destination because spaceAvailable.First is misaligned." << endl;
            
        }
        ssize_t bytesRead = read(fd, destination, IOSize);
            
        if(bytesRead <= 0){
            
            if(destination != spaceAvailable.first){
                free(destination);
            }
            if(bytesRead == -1){
                assert(false);
            }
            //cerr << "no bytes read " << endl;
            return false;
        }
        
        if(destination != spaceAvailable.first){
            memcpy(spaceAvailable.first, destination, bytesRead);
            free(destination);
        }
        //cout << "added: " << bytesRead << " bytes to the buffer, done here. size before: " << buffer.size() << "location: " << (int*)spaceAvailable.first << endl;
        buffer.confirmDirectEnqueue(bytesRead);
        //cout << "size after: " << buffer.size() << endl;
        return true;
    }
    uint8_t* readBuffer = (uint8_t*)memalign(O_DIRECT_MEMORY_ALIGNMENT, IOSize);
    ssize_t bytesRead = read(fd, readBuffer, IOSize);

    if(bytesRead <= 0){
        free(readBuffer);
        return false;
    }
    assert(spaceAvailable.first != nullptr);
    size_t bytesCopied = min((size_t)bytesRead, spaceAvailable.second);
    //cout << " bytesCOpied here: " << bytesCopied << endl;
    memcpy(spaceAvailable.first, readBuffer, bytesCopied);

    buffer.confirmDirectEnqueue(bytesCopied);

    if(bytesCopied == bytesRead){
        free(readBuffer);
        //cout << "added all read bytes to the buffer. done" << endl;
        return true;
    }
    size_t remainingBytes = bytesRead - bytesCopied;
    spaceAvailable = buffer.directFutureEnqueue();
    //cout << "remainingbytes:" << remainingBytes << " bytes available in buffer; " << spaceAvailable.second << endl;
    assert(spaceAvailable.second >= remainingBytes);
    memcpy(spaceAvailable.first, readBuffer + bytesCopied, remainingBytes);
    buffer.confirmDirectEnqueue(remainingBytes);       
    free(readBuffer);
    return true;
}
#endif
