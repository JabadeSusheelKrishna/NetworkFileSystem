#include "ss_file_operations.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#define MAX_FILE_LOCKS 1024 // Max number of concurrent file locks

static FileLock *file_locks[MAX_FILE_LOCKS];
pthread_mutex_t file_locks_mutex = PTHREAD_MUTEX_INITIALIZER;

// Simple hash function for file paths
static unsigned int hash_path_for_lock(const char *path) {
    unsigned int hash = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        hash = (hash * 31) + path[i];
    }
    return hash % MAX_FILE_LOCKS;
}

// Function to initialize file locking mechanism
void init_file_locks() {
    for (int i = 0; i < MAX_FILE_LOCKS; i++) {
        file_locks[i] = NULL;
    }
}

// Helper to find or create a FileLock node
static FileLock *get_or_create_file_lock_node(const char *path) {
    unsigned int index = hash_path_for_lock(path);
    FileLock *current = file_locks[index];

    while (current != NULL) {
        if (strcmp(current->path, path) == 0) {
            return current;
        }
        current = current->next;
    }

    // Not found, create a new one
    FileLock *new_lock = (FileLock *)malloc(sizeof(FileLock));
    if (!new_lock) {
        log_message("SS_FILE_OPS: Failed to allocate memory for file lock.");
        return NULL;
    }
    strcpy(new_lock->path, path);
    pthread_rwlock_init(&new_lock->rwlock, NULL);
    new_lock->next = file_locks[index];
    file_locks[index] = new_lock;
    return new_lock;
}

// Function to acquire a read lock
int acquire_read_lock(const char *path) {
    pthread_mutex_lock(&file_locks_mutex);
    FileLock *lock_node = get_or_create_file_lock_node(path);
    pthread_mutex_unlock(&file_locks_mutex);

    if (!lock_node) return -1;

    log_message("SS_FILE_OPS: Attempting to acquire read lock for %s", path);
    if (pthread_rwlock_rdlock(&lock_node->rwlock) != 0) {
        log_message("SS_FILE_OPS: Failed to acquire read lock for %s", path);
        return -1;
    }
    log_message("SS_FILE_OPS: Acquired read lock for %s", path);
    return 0;
}

// Function to release a read lock
void release_read_lock(const char *path) {
    pthread_mutex_lock(&file_locks_mutex);
    unsigned int index = hash_path_for_lock(path);
    FileLock *current = file_locks[index];
    while (current != NULL) {
        if (strcmp(current->path, path) == 0) {
            pthread_rwlock_unlock(&current->rwlock);
            log_message("SS_FILE_OPS: Released read lock for %s", path);
            break;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&file_locks_mutex);
}

// Function to acquire a write lock
int acquire_write_lock(const char *path) {
    pthread_mutex_lock(&file_locks_mutex);
    FileLock *lock_node = get_or_create_file_lock_node(path);
    pthread_mutex_unlock(&file_locks_mutex);

    if (!lock_node) return -1;

    log_message("SS_FILE_OPS: Attempting to acquire write lock for %s", path);
    if (pthread_rwlock_wrlock(&lock_node->rwlock) != 0) {
        log_message("SS_FILE_OPS: Failed to acquire write lock for %s", path);
        return -1;
    }
    log_message("SS_FILE_OPS: Acquired write lock for %s", path);
    return 0;
}

// Function to release a write lock
void release_write_lock(const char *path) {
    pthread_mutex_lock(&file_locks_mutex);
    unsigned int index = hash_path_for_lock(path);
    FileLock *current = file_locks[index];
    while (current != NULL) {
        if (strcmp(current->path, path) == 0) {
            pthread_rwlock_unlock(&current->rwlock);
            log_message("SS_FILE_OPS: Released write lock for %s", path);
            break;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&file_locks_mutex);
}

// Function to create a file or directory
int ss_create(const char *path, FileType type) {
    if (type == FILE_TYPE_FILE) {
        FILE *fp = fopen(path, "w");
        if (fp) {
            fclose(fp);
            log_message("SS_FILE_OPS: Created file: %s", path);
            return SUCCESS;
        } else {
            log_message("SS_FILE_OPS: Failed to create file %s: %s", path, strerror(errno));
            return PERMISSION_DENIED; // Or other appropriate error
        }
    } else if (type == FILE_TYPE_DIR) {
        if (mkdir(path, 0777) == 0) {
            log_message("SS_FILE_OPS: Created directory: %s", path);
            return SUCCESS;
        } else {
            log_message("SS_FILE_OPS: Failed to create directory %s: %s", path, strerror(errno));
            return PERMISSION_DENIED; // Or other appropriate error
        }
    }
    return INVALID_OPERATION;
}

// Function to delete a file or directory
int ss_delete(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        log_message("SS_FILE_OPS: Path %s not found for deletion.", path);
        return FILE_NOT_FOUND;
    }

    if (S_ISDIR(st.st_mode)) {
        // Check if directory is empty before removing
        DIR *d = opendir(path);
        if (d) {
            struct dirent *dir;
            int is_empty = 1;
            while ((dir = readdir(d)) != NULL) {
                if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                    is_empty = 0;
                    break;
                }
            }
            closedir(d);
            if (!is_empty) {
                log_message("SS_FILE_OPS: Cannot delete non-empty directory: %s", path);
                return PERMISSION_DENIED; // Or a more specific error like DIR_NOT_EMPTY
            }
        }
        if (rmdir(path) == 0) {
            log_message("SS_FILE_OPS: Deleted directory: %s", path);
            return SUCCESS;
        } else {
            log_message("SS_FILE_OPS: Failed to delete directory %s: %s", path, strerror(errno));
            return PERMISSION_DENIED;
        }
    } else if (S_ISREG(st.st_mode)) {
        if (remove(path) == 0) {
            log_message("SS_FILE_OPS: Deleted file: %s", path);
            return SUCCESS;
        } else {
            log_message("SS_FILE_OPS: Failed to delete file %s: %s", path, strerror(errno));
            return PERMISSION_DENIED;
        }
    }
    return INVALID_OPERATION;
}

// Function to read a file
int ss_read(const char *path, char *buffer, int *bytes_read) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        log_message("SS_FILE_OPS: Failed to open file %s for reading: %s", path, strerror(errno));
        return FILE_NOT_FOUND;
    }

    *bytes_read = fread(buffer, 1, MAX_DATA_SIZE - 1, fp); // Leave space for null terminator
    buffer[*bytes_read] = '\0';
    fclose(fp);
    log_message("SS_FILE_OPS: Read %d bytes from file: %s", *bytes_read, path);
    return SUCCESS;
}

// Function to write to a file
int ss_write(const char *path, const char *data, int data_size, WriteMode mode) {
    // For now, async mode is treated as sync. Will implement true async later.
    FILE *fp = fopen(path, "w");
    if (!fp) {
        log_message("SS_FILE_OPS: Failed to open file %s for writing: %s", path, strerror(errno));
        return PERMISSION_DENIED;
    }

    int bytes_written = fwrite(data, 1, data_size, fp);
    fclose(fp);

    if (bytes_written != data_size) {
        log_message("SS_FILE_OPS: Partial write to file %s. Wrote %d of %d bytes.", path, bytes_written, data_size);
        return ASYNC_WRITE_FAILED; // Can be more specific for sync writes too
    }
    log_message("SS_FILE_OPS: Wrote %d bytes to file: %s (Mode: %s)", bytes_written, path, (mode == WRITE_MODE_SYNC ? "SYNC" : "ASYNC"));
    return SUCCESS;
}

// Function to get file information
int ss_get_info(const char *path, Message *response_msg) {
    struct stat st;
    if (stat(path, &st) == -1) {
        log_message("SS_FILE_OPS: Path %s not found for info: %s", path, strerror(errno));
        return FILE_NOT_FOUND;
    }

    // Populate response_msg with info (for now, just size and type)
    sprintf(response_msg->data, "Size: %ld bytes\n", st.st_size);
    if (S_ISREG(st.st_mode)) {
        strcat(response_msg->data, "Type: File\n");
        response_msg->file_type = FILE_TYPE_FILE;
    } else if (S_ISDIR(st.st_mode)) {
        strcat(response_msg->data, "Type: Directory\n");
        response_msg->file_type = FILE_TYPE_DIR;
    } else {
        strcat(response_msg->data, "Type: Other\n");
        response_msg->file_type = FILE_TYPE_UNKNOWN;
    }
    response_msg->data_size = strlen(response_msg->data);
    log_message("SS_FILE_OPS: Retrieved info for %s", path);
    return SUCCESS;
}

// Function to clean up file locks
void destroy_file_locks() {
    pthread_mutex_lock(&file_locks_mutex);
    for (int i = 0; i < MAX_FILE_LOCKS; i++) {
        FileLock *current = file_locks[i];
        while (current != NULL) {
            FileLock *next = current->next;
            pthread_rwlock_destroy(&current->rwlock);
            free(current);
            current = next;
        }
        file_locks[i] = NULL;
    }
    pthread_mutex_unlock(&file_locks_mutex);
    pthread_mutex_destroy(&file_locks_mutex);
}
