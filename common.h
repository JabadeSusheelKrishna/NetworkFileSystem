#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_PATH_LEN 1024
#define MAX_DATA_SIZE 8192
#define MAX_MESSAGE_SIZE (sizeof(OperationType) + 2 * MAX_PATH_LEN + MAX_DATA_SIZE + sizeof(int) * 3)
#define MAX_ACCESSIBLE_PATHS 10
#define MAX_SS 100

typedef enum {
    REGISTER_SS,
    LIST_PATHS,
    READ_FILE,
    WRITE_FILE,
    CREATE_FILE,
    DELETE_FILE,
    GET_FILE_INFO,
    COPY_FILE,
    ACK,
    NACK,
    STOP,
    // Add more operations as needed
} OperationType;

typedef enum {
    FILE_TYPE_FILE,
    FILE_TYPE_DIR,
    FILE_TYPE_UNKNOWN
} FileType;

typedef enum {
    WRITE_MODE_SYNC,
    WRITE_MODE_ASYNC
} WriteMode;

typedef struct {
    OperationType operation;
    char path[MAX_PATH_LEN];
    char path2[MAX_PATH_LEN]; // For copy operations
    char data[MAX_DATA_SIZE];
    int data_size;
    int error_code;
    WriteMode write_mode;
    FileType file_type;
    char ss_ip[INET_ADDRSTRLEN];
    int ss_nm_port;
    int ss_client_port;
    char accessible_paths[MAX_ACCESSIBLE_PATHS][MAX_PATH_LEN];
    int num_accessible_paths;
} Message;

// Error Codes
#define SUCCESS 0
#define FILE_NOT_FOUND 1
#define FILE_IN_USE 2
#define PERMISSION_DENIED 3
#define INVALID_PATH 4
#define SS_UNAVAILABLE 5
#define NETWORK_ERROR 6
#define FILE_EXISTS 7
#define INVALID_OPERATION 8
#define ASYNC_WRITE_FAILED 9
#define TIMEOUT 10

#endif // COMMON_H
