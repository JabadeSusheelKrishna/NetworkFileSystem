#ifndef TRIE_H
#define TRIE_H

#include "common.h"

#define MAX_CHILDREN 256 // For ASCII characters

// Forward declaration of StorageServerInfo from naming_server.c
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int nm_port; // Port for NM to communicate with SS
    int client_port; // Port for clients to communicate with SS
    char accessible_paths[MAX_ACCESSIBLE_PATHS][MAX_PATH_LEN];
    int num_accessible_paths;
} StorageServerInfo;

// Trie node structure
typedef struct TrieNode {
    struct TrieNode *children[MAX_CHILDREN];
    StorageServerInfo *ss_info; // Points to SS info if this node marks the end of a path
    pthread_rwlock_t lock; // Read-write lock for concurrent access
} TrieNode;

// Function to create a new Trie node
TrieNode *create_trie_node();

// Function to insert a path into the Trie
void trie_insert(TrieNode *root, const char *path, StorageServerInfo *ss_info);

// Function to search for a path in the Trie
StorageServerInfo *trie_search(TrieNode *root, const char *path);

// Function to free the Trie (recursive)
void free_trie(TrieNode *root);

// Function to collect all paths from the Trie (for LIST_PATHS operation)
void trie_collect_paths(TrieNode *root, char *current_path, char paths[][MAX_PATH_LEN], int *count);

// Function to delete a path from the Trie
void trie_delete(TrieNode *root, const char *path);

#endif // TRIE_H
