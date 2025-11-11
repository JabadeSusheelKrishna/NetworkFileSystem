#include "trie.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

// Function to create a new Trie node
TrieNode *create_trie_node() {
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    if (node) {
        memset(node->children, 0, sizeof(node->children));
        node->ss_info = NULL;
        pthread_rwlock_init(&node->lock, NULL);
    }
    return node;
}

// Function to insert a path into the Trie
void trie_insert(TrieNode *root, const char *path, StorageServerInfo *ss_info) {
    TrieNode *current = root;
    pthread_rwlock_wrlock(&current->lock);

    for (int i = 0; i < strlen(path); i++) {
        int index = (unsigned char)path[i];
        if (!current->children[index]) {
            current->children[index] = create_trie_node();
        }
        pthread_rwlock_unlock(&current->lock);
        current = current->children[index];
        pthread_rwlock_wrlock(&current->lock);
    }
    current->ss_info = ss_info; // Store SS info at the end of the path
    pthread_rwlock_unlock(&current->lock);
}

// Function to search for a path in the Trie
StorageServerInfo *trie_search(TrieNode *root, const char *path) {
    TrieNode *current = root;
    pthread_rwlock_rdlock(&current->lock);

    for (int i = 0; i < strlen(path); i++) {
        int index = (unsigned char)path[i];
        if (!current->children[index]) {
            pthread_rwlock_unlock(&current->lock);
            return NULL; // Path not found
        }
        pthread_rwlock_unlock(&current->lock);
        current = current->children[index];
        pthread_rwlock_rdlock(&current->lock);
    }
    StorageServerInfo *info = current->ss_info;
    pthread_rwlock_unlock(&current->lock);
    return info;
}

// Function to free the Trie (recursive)
void free_trie(TrieNode *root) {
    if (!root) return;

    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (root->children[i]) {
            free_trie(root->children[i]);
        }
    }
    pthread_rwlock_destroy(&root->lock);
    free(root);
}

// Helper for trie_collect_paths
static void _trie_collect_paths_recursive(TrieNode *node, char *current_path, int level, char paths[][MAX_PATH_LEN], int *count) {
    if (!node) return;

    pthread_rwlock_rdlock(&node->lock);

    if (node->ss_info != NULL) {
        // This node marks the end of a valid path
        if (*count < MAX_ACCESSIBLE_PATHS) { // Using MAX_ACCESSIBLE_PATHS as a temporary limit for collected paths
            current_path[level] = '\0';
            strcpy(paths[*count], current_path);
            (*count)++;
        }
    }

    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (node->children[i]) {
            current_path[level] = (char)i;
            _trie_collect_paths_recursive(node->children[i], current_path, level + 1, paths, count);
        }
    }
    pthread_rwlock_unlock(&node->lock);
}

// Function to collect all paths from the Trie (for LIST_PATHS operation)
void trie_collect_paths(TrieNode *root, char *current_path, char paths[][MAX_PATH_LEN], int *count) {
    *count = 0;
    _trie_collect_paths_recursive(root, current_path, 0, paths, count);
}

// Function to delete a path from the Trie
void trie_delete(TrieNode *root, const char *path) {
    TrieNode *current = root;
    pthread_rwlock_wrlock(&current->lock);

    for (int i = 0; i < strlen(path); i++) {
        int index = (unsigned char)path[i];
        if (!current->children[index]) {
            pthread_rwlock_unlock(&current->lock);
            return; // Path not found
        }
        pthread_rwlock_unlock(&current->lock);
        current = current->children[index];
        pthread_rwlock_wrlock(&current->lock);
    }
    current->ss_info = NULL; // Mark as deleted
    pthread_rwlock_unlock(&current->lock);
    log_message("TRIE: Path '%s' marked for deletion.", path);
}
