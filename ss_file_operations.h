#ifndef SS_FILE_OPERATIONS_H
#define SS_FILE_OPERATIONS_H

#include "common.h"
#include <sys/stat.h>

// Structure for file locking
typedef struct FileLock {
    char path[MAX_PATH_LEN];
    pthread_rwlock_t rwlock;
    struct FileLock *next; // For hash collisions
} FileLock;

// Function to initialize file locking mechanism
void init_file_locks();

// Function to acquire a read lock
int acquire_read_lock(const char *path);

// Function to release a read lock
void release_read_lock(const char *path);

// Function to acquire a write lock
int acquire_write_lock(const char *path);

// Function to release a write lock
void release_write_lock(const char *path);

// Function to create a file or directory
int ss_create(const char *path, FileType type);

// Function to delete a file or directory
int ss_delete(const char *path);

// Function to read a file
int ss_read(const char *path, char *buffer, int *bytes_read);

// Function to write to a file
int ss_write(const char *path, const char *data, int data_size, WriteMode mode);

// Function to get file information
int ss_get_info(const char *path, Message *response_msg);

// Function to clean up file locks
void destroy_file_locks();

#endif // SS_FILE_OPERATIONS_H
