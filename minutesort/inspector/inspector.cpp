#include <iostream>
#include <fstream>
#include <cstring>

using namespace std;

bool lhsSmallerEqualRhs(uint8_t* lhsKey, uint8_t* rhsKey){
        int i = 0;

    while(lhsKey[i] == rhsKey[i] && i < 10){
        i++;
    }
    return lhsKey[i] <= rhsKey[i];
}


void debugPrintHex(uint8_t* array, int size){
    int i = 0;
    while(i < size ) {
        printf("%02x", array[i]);
        i++;
    }
    printf("\n");
}



int main(int argc, char* argv[]){

    if(argc != 2){
        cerr << "Usage: ./inspector [filename]" << endl;
        return -1;
    }
    ifstream file;
    file.open(argv[1], ios::in | ios::binary);
    
    if(!file.is_open()){
        cerr << "Failed to open file: " << argv[1] << endl;
        return -1;
    }
    uint8_t previousKey[10] = {0};
    uint8_t firstKey[10] = {0};
    uint8_t* key = new uint8_t[10];
    uint8_t* value = new uint8_t[90];

    uint64_t KVpairCount = 0;
    bool ordered = true;
    bool first = true;
    
    while(true){
        file.read((char*)key, 10);

        if(file.gcount() != 10){

            if(file.gcount() != 0){
                cerr << "Read partial key" << endl;
            }
            break;
        }
        if(first){
            for(int i = 0; i < 10; i++){
                firstKey[i] = key[i];
            }
            first = false;
        }
        
        file.read((char*)value, 90);
        
        if(file.gcount() != 90){
            cerr << "Value is incomplete" << endl;
            break;
        }
        
        if(!lhsSmallerEqualRhs((uint8_t*)&previousKey, key)){
            cerr << "File is unordered at:" << KVpairCount << endl;
            cout << "Pair: " <<  KVpairCount -1 << " ";
            debugPrintHex((uint8_t*)&previousKey, 10);
            cout << "Pair: " <<  KVpairCount << " ";
            debugPrintHex(key, 10);
            ordered = false;
        }
        for(int i = 0; i < 10; i++){
            previousKey[i] = key[i];
        }
        KVpairCount++;
    }
    file.close();
    cout << "Found a total of: " << KVpairCount << " pairs" << endl;
    ordered? cout << "OK, all in order" << endl : cout << "ERROR, some KVPairs are in the wrong order" << endl;
    
    cout << "First:";
    debugPrintHex(firstKey, 10);
    cout << "Last:";
    debugPrintHex(key, 10);
    
    return 0;
}
