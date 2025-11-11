#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include "common.h"
#include "trie.h"

#define LRU_CACHE_SIZE 100

// LRU Cache Node
typedef struct LRUCacheNode {
    char path[MAX_PATH_LEN];
    StorageServerInfo ss_info; // Copy of SS info
    struct LRUCacheNode *prev;
    struct LRUCacheNode *next;
} LRUCacheNode;

// LRU Cache structure
typedef struct LRUCache {
    LRUCacheNode *head;
    LRUCacheNode *tail;
    LRUCacheNode *map[LRU_CACHE_SIZE]; // Simple hash map for quick lookup (using path hash)
    int count;
    pthread_mutex_t lock;
} LRUCache;

// Function to initialize the LRU cache
void lru_init(LRUCache *cache);

// Function to get SS info from cache
StorageServerInfo *lru_get(LRUCache *cache, const char *path);

// Function to put SS info into cache
void lru_put(LRUCache *cache, const char *path, const StorageServerInfo *ss_info);

// Function to remove SS info from cache
void lru_remove(LRUCache *cache, const char *path);

// Function to destroy the LRU cache
void lru_destroy(LRUCache *cache);

#endif // LRU_CACHE_H
