#include "lru_cache.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

// Simple hash function for path
static unsigned int hash_path(const char *path) {
    unsigned int hash = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        hash = (hash * 31) + path[i];
    }
    return hash % LRU_CACHE_SIZE;
}

// Function to initialize the LRU cache
void lru_init(LRUCache *cache) {
    cache->head = NULL;
    cache->tail = NULL;
    cache->count = 0;
    for (int i = 0; i < LRU_CACHE_SIZE; i++) {
        cache->map[i] = NULL;
    }
    pthread_mutex_init(&cache->lock, NULL);
}

// Helper function to move a node to the front (most recently used)
static void move_to_front(LRUCache *cache, LRUCacheNode *node) {
    if (node == cache->head) {
        return; // Already at the front
    }

    if (node == cache->tail) {
        cache->tail = node->prev;
        if (cache->tail) {
            cache->tail->next = NULL;
        }
    }

    if (node->prev) {
        node->prev->next = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    }

    node->prev = NULL;
    node->next = cache->head;
    if (cache->head) {
        cache->head->prev = node;
    }
    cache->head = node;
    if (!cache->tail) {
        cache->tail = node;
    }
}

// Function to get SS info from cache
StorageServerInfo *lru_get(LRUCache *cache, const char *path) {
    pthread_mutex_lock(&cache->lock);
    unsigned int index = hash_path(path);
    LRUCacheNode *node = cache->map[index];

    // Simple linear scan in case of hash collision (for this basic implementation)
    while (node != NULL && strcmp(node->path, path) != 0) {
        node = node->next; // This is not ideal for LRU, but for a simple map it works
    }

    if (node) {
        move_to_front(cache, node);
        pthread_mutex_unlock(&cache->lock);
        return &node->ss_info;
    }
    pthread_mutex_unlock(&cache->lock);
    return NULL;
}

// Function to put SS info into cache
void lru_put(LRUCache *cache, const char *path, const StorageServerInfo *ss_info) {
    pthread_mutex_lock(&cache->lock);
    unsigned int index = hash_path(path);
    LRUCacheNode *node = cache->map[index];

    // Check if already in cache
    while (node != NULL && strcmp(node->path, path) != 0) {
        node = node->next; // This is not ideal for LRU, but for a simple map it works
    }

    if (node) {
        // Update existing node and move to front
        memcpy(&node->ss_info, ss_info, sizeof(StorageServerInfo));
        move_to_front(cache, node);
    } else {
        // New entry
        if (cache->count == LRU_CACHE_SIZE) {
            // Cache is full, remove LRU item (tail)
            LRUCacheNode *temp = cache->tail;
            if (temp) {
                unsigned int tail_index = hash_path(temp->path);
                if (cache->map[tail_index] == temp) {
                    cache->map[tail_index] = NULL; // Remove from map
                }
                cache->tail = temp->prev;
                if (cache->tail) {
                    cache->tail->next = NULL;
                }
                free(temp);
                cache->count--;
            }
        }

        // Create new node
        node = (LRUCacheNode *)malloc(sizeof(LRUCacheNode));
        if (!node) {
            log_message("LRU Cache: Failed to allocate memory for new node.");
            pthread_mutex_unlock(&cache->lock);
            return;
        }
        strcpy(node->path, path);
        memcpy(&node->ss_info, ss_info, sizeof(StorageServerInfo));
        node->prev = NULL;
        node->next = cache->head;
        if (cache->head) {
            cache->head->prev = node;
        }
        cache->head = node;
        if (!cache->tail) {
            cache->tail = node;
        }
        cache->count++;

        // Add to map (simple replacement for now, collisions will overwrite)
        cache->map[index] = node;
    }
    pthread_mutex_unlock(&cache->lock);
}

// Function to remove SS info from cache
void lru_remove(LRUCache *cache, const char *path) {
    pthread_mutex_lock(&cache->lock);
    unsigned int index = hash_path(path);
    LRUCacheNode *node = cache->map[index];
    LRUCacheNode *prev_node = NULL;

    // Find the node in the hash map's linked list
    while (node != NULL && strcmp(node->path, path) != 0) {
        prev_node = node;
        node = node->next;
    }

    if (node) {
        // Remove from hash map's linked list
        if (prev_node) {
            prev_node->next = node->next;
        } else {
            cache->map[index] = node->next;
        }

        // Remove from LRU doubly linked list
        if (node->prev) {
            node->prev->next = node->next;
        }
        if (node->next) {
            node->next->prev = node->prev;
        }
        if (node == cache->head) {
            cache->head = node->next;
        }
        if (node == cache->tail) {
            cache->tail = node->prev;
        }

        free(node);
        cache->count--;
        log_message("LRU Cache: Removed path '%s'.", path);
    }
    pthread_mutex_unlock(&cache->lock);
}

// Function to destroy the LRU cache
void lru_destroy(LRUCache *cache) {
    pthread_mutex_lock(&cache->lock);
    LRUCacheNode *current = cache->head;
    while (current) {
        LRUCacheNode *next = current->next;
        free(current);
        current = next;
    }
    cache->head = NULL;
    cache->tail = NULL;
    cache->count = 0;
    for (int i = 0; i < LRU_CACHE_SIZE; i++) {
        cache->map[i] = NULL;
    }
    pthread_mutex_unlock(&cache->lock);
    pthread_mutex_destroy(&cache->lock);
}
