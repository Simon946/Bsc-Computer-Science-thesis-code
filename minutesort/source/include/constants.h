#ifndef CONSTANTSH
#define CONSTANTSH

#define KEY_SIZE 10
#define VALUE_SIZE 90

#define PORT_MIN 1024 //reserved for OS
#define PORT_MAX 65535 //2^16 -1
#define SOCKET_CREATE_MAX_ATTEMPTS 100
#define CONNECT_MAX_ATTEMPTS 100

#define FILE_IO_SIZE 32768 //in bytes
#define NETWORK_IO_SIZE 2048 //in bytes
#define O_DIRECT_MEMORY_ALIGNMENT 512
//TODO look into why this works for 2048 but maybe not for larger sizes?
#endif