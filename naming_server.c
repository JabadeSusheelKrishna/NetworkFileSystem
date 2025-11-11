#include "common.h"
#include "utils.h"
#include "trie.h"
#include "lru_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

// Global array to store registered Storage Servers
StorageServerInfo registered_ss[MAX_SS];
int num_registered_ss = 0;
pthread_mutex_t ss_list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global Trie for path management
TrieNode *path_trie_root;

// Global LRU Cache for path lookups
LRUCache path_lru_cache;

// Helper to find an available Storage Server for creation
// For now, just picks the first registered SS
StorageServerInfo *get_available_ss_for_creation() {
    pthread_mutex_lock(&ss_list_mutex);
    if (num_registered_ss > 0) {
        StorageServerInfo *ss = &registered_ss[0]; // Simple selection
        pthread_mutex_unlock(&ss_list_mutex);
        return ss;
    }
    pthread_mutex_unlock(&ss_list_mutex);
    return NULL;
}

// Helper function to remove a path from a StorageServerInfo's accessible_paths
void remove_path_from_ss_info(StorageServerInfo *ss_info, const char *path) {
    for (int i = 0; i < ss_info->num_accessible_paths; i++) {
        if (strcmp(ss_info->accessible_paths[i], path) == 0) {
            // Shift elements to the left
            for (int j = i; j < ss_info->num_accessible_paths - 1; j++) {
                strcpy(ss_info->accessible_paths[j], ss_info->accessible_paths[j+1]);
            }
            ss_info->num_accessible_paths--;
            break;
        }
    }
}


// Function to handle each client/SS connection
void *handle_connection(void *socket_desc) {
    int client_sock = *(int *)socket_desc;
    free(socket_desc);
    Message msg;
    Message response_msg;

    log_message("NM: New connection established.");

    // Receive message from client/SS
    if (receive_message(client_sock, &msg) != 0) {
        log_message("NM: Failed to receive message or connection closed.");
        close(client_sock);
        return NULL;
    }

    memset(&response_msg, 0, sizeof(Message));

    switch (msg.operation) {
        case REGISTER_SS: {
            pthread_mutex_lock(&ss_list_mutex);
            if (num_registered_ss < MAX_SS) {
                // Store SS information
                StorageServerInfo *new_ss_info = &registered_ss[num_registered_ss];
                strcpy(new_ss_info->ip, msg.ss_ip);
                new_ss_info->nm_port = msg.ss_nm_port;
                new_ss_info->client_port = msg.ss_client_port;
                new_ss_info->num_accessible_paths = msg.num_accessible_paths;
                for (int i = 0; i < msg.num_accessible_paths; i++) {
                    strcpy(new_ss_info->accessible_paths[i], msg.accessible_paths[i]);
                    // Insert accessible paths into the Trie
                    trie_insert(path_trie_root, new_ss_info->accessible_paths[i], new_ss_info);
                    log_message("NM: Added path '%s' from SS %s to Trie.", new_ss_info->accessible_paths[i], new_ss_info->ip);
                }
                num_registered_ss++;
                log_message("NM: Registered Storage Server: IP=%s, NM_Port=%d, Client_Port=%d, Paths: %d",
                            msg.ss_ip, msg.ss_nm_port, msg.ss_client_port, msg.num_accessible_paths);

                // Send ACK
                response_msg.operation = ACK;
                response_msg.error_code = SUCCESS;
            } else {
                log_message("NM: Max number of Storage Servers reached. Cannot register %s.", msg.ss_ip);
                // Send NACK
                response_msg.operation = NACK;
                response_msg.error_code = SS_UNAVAILABLE;
            }
            pthread_mutex_unlock(&ss_list_mutex);
            break;
        }
        case LIST_PATHS: {
            char collected_paths[MAX_ACCESSIBLE_PATHS * MAX_PATH_LEN]; // Buffer to hold all paths
            char temp_path[MAX_PATH_LEN];
            int count = 0;
            // Allocate dynamically to avoid stack overflow for large MAX_ACCESSIBLE_PATHS
            char (*paths_array)[MAX_PATH_LEN] = malloc(sizeof(char) * MAX_ACCESSIBLE_PATHS * MAX_PATH_LEN);
            if (!paths_array) {
                log_message("NM: Failed to allocate memory for path collection.");
                response_msg.operation = NACK;
                response_msg.error_code = NETWORK_ERROR; // Or a more specific error
                break;
            }

            trie_collect_paths(path_trie_root, temp_path, paths_array, &count);

            // Concatenate all paths into the data field of the response message
            collected_paths[0] = '\0';
            for (int i = 0; i < count; i++) {
                strcat(collected_paths, paths_array[i]);
                if (i < count - 1) {
                    strcat(collected_paths, "\n"); // Separator
                }
            }
            free(paths_array);

            strcpy(response_msg.data, collected_paths);
            response_msg.data_size = strlen(collected_paths);
            response_msg.operation = ACK;
            response_msg.error_code = SUCCESS;
            log_message("NM: Sent list of accessible paths to client.");
            break;
        }
        case CREATE_FILE: {
            // First, ensure the path does not already exist in the trie
            if (trie_search(path_trie_root, msg.path) != NULL) {
                response_msg.operation = NACK;
                response_msg.error_code = FILE_EXISTS;
                log_message("NM: Path '%s' already exists, cannot create.", msg.path);
                break;
            }

            StorageServerInfo *target_ss = get_available_ss_for_creation();
            if (!target_ss) {
                response_msg.operation = NACK;
                response_msg.error_code = SS_UNAVAILABLE;
                log_message("NM: No Storage Server available for CREATE_FILE operation.");
                break;
            }

            // Connect to the target SS's NM communication port
            int ss_sockfd = create_socket();
            if (ss_sockfd < 0) {
                response_msg.operation = NACK;
                response_msg.error_code = NETWORK_ERROR;
                break;
            }
            if (connect_to_server(ss_sockfd, target_ss->ip, target_ss->nm_port) < 0) {
                close(ss_sockfd);
                response_msg.operation = NACK;
                response_msg.error_code = SS_UNAVAILABLE;
                log_message("NM: Failed to connect to SS %s:%d for CREATE_FILE.", target_ss->ip, target_ss->nm_port);
                break;
            }

            // Forward the CREATE_FILE request to the SS
            Message ss_request_msg = msg; // Copy client's request
            log_message("NM: Forwarding CREATE_FILE request for '%s' to SS %s:%d.", msg.path, target_ss->ip, target_ss->nm_port);
            if (send_message(ss_sockfd, &ss_request_msg) < 0) {
                close(ss_sockfd);
                response_msg.operation = NACK;
                response_msg.error_code = NETWORK_ERROR;
                break;
            }

            Message ss_response_msg;
            if (receive_message(ss_sockfd, &ss_response_msg) != 0) {
                log_message("NM: Failed to receive response from SS for CREATE_FILE or connection closed.");
                close(ss_sockfd);
                response_msg.operation = NACK;
                response_msg.error_code = NETWORK_ERROR;
                break;
            }
            close(ss_sockfd);

            if (ss_response_msg.operation == ACK && ss_response_msg.error_code == SUCCESS) {
                // Update NM's internal state (Trie and SS accessible paths)
                pthread_mutex_lock(&ss_list_mutex);
                trie_insert(path_trie_root, msg.path, target_ss);
                if (target_ss->num_accessible_paths < MAX_ACCESSIBLE_PATHS) {
                    strcpy(target_ss->accessible_paths[target_ss->num_accessible_paths], msg.path);
                    target_ss->num_accessible_paths++;
                } else {
                    log_message("NM: Warning: SS %s accessible paths limit reached. Path '%s' not added to SS info list.", target_ss->ip, msg.path);
                }
                pthread_mutex_unlock(&ss_list_mutex);
                response_msg.operation = ACK;
                response_msg.error_code = SUCCESS;
                log_message("NM: Successfully created '%s' on SS %s.", msg.path, target_ss->ip);
            } else {
                response_msg.operation = NACK;
                response_msg.error_code = ss_response_msg.error_code;
                log_message("NM: SS %s failed to create '%s'. Error: %d", target_ss->ip, msg.path, ss_response_msg.error_code);
            }
            break;
        }
        case DELETE_FILE: {
            StorageServerInfo *target_ss = trie_search(path_trie_root, msg.path);
            if (!target_ss) {
                response_msg.operation = NACK;
                response_msg.error_code = FILE_NOT_FOUND;
                log_message("NM: Path '%s' not found for DELETE_FILE operation.", msg.path);
                break;
            }
            
            // Connect to the target SS's NM communication port
            int ss_sockfd = create_socket();
            if (ss_sockfd < 0) {
                response_msg.operation = NACK;
                response_msg.error_code = NETWORK_ERROR;
                break;
            }
            if (connect_to_server(ss_sockfd, target_ss->ip, target_ss->nm_port) < 0) {
                close(ss_sockfd);
                response_msg.operation = NACK;
                response_msg.error_code = SS_UNAVAILABLE;
                log_message("NM: Failed to connect to SS %s:%d for DELETE_FILE.", target_ss->ip, target_ss->nm_port);
                break;
            }

            // Forward the DELETE_FILE request to the SS
            Message ss_request_msg = msg; // Copy client's request
            log_message("NM: Forwarding DELETE_FILE request for '%s' to SS %s:%d.", msg.path, target_ss->ip, target_ss->nm_port);
            if (send_message(ss_sockfd, &ss_request_msg) < 0) {
                close(ss_sockfd);
                response_msg.operation = NACK;
                response_msg.error_code = NETWORK_ERROR;
                break;
            }

            Message ss_response_msg;
            if (receive_message(ss_sockfd, &ss_response_msg) != 0) {
                log_message("NM: Failed to receive response from SS for DELETE_FILE or connection closed.");
                close(ss_sockfd);
                response_msg.operation = NACK;
                response_msg.error_code = NETWORK_ERROR;
                break;
            }
            close(ss_sockfd);

            if (ss_response_msg.operation == ACK && ss_response_msg.error_code == SUCCESS) {
                // Update NM's internal state (Trie and SS accessible paths)
                pthread_mutex_lock(&ss_list_mutex);
                trie_delete(path_trie_root, msg.path);
                lru_remove(&path_lru_cache, msg.path); // Invalidate/remove from cache
                remove_path_from_ss_info(target_ss, msg.path);
                pthread_mutex_unlock(&ss_list_mutex);
                response_msg.operation = ACK;
                response_msg.error_code = SUCCESS;
                log_message("NM: Successfully deleted '%s' from SS %s.", msg.path, target_ss->ip);
            } else {
                response_msg.operation = NACK;
                response_msg.error_code = ss_response_msg.error_code;
                log_message("NM: SS %s failed to delete '%s'. Error: %d", target_ss->ip, msg.path, ss_response_msg.error_code);
            }
            break;
        }
        case COPY_FILE: {
            // NM needs to find source SS and destination SS
            // Then instruct source SS to send to dest SS, or NM mediates
            // For now, just a placeholder.
            log_message("NM: Received COPY_FILE request for '%s' to '%s'. Not yet implemented.", msg.path, msg.path2);
            response_msg.operation = NACK;
            response_msg.error_code = INVALID_OPERATION;
            break;
        }
        case READ_FILE:
        case WRITE_FILE:
        case GET_FILE_INFO: {
            StorageServerInfo *ss_info = lru_get(&path_lru_cache, msg.path);
            if (ss_info == NULL) {
                ss_info = trie_search(path_trie_root, msg.path);
                if (ss_info) {
                    lru_put(&path_lru_cache, msg.path, ss_info);
                }
            }

            if (ss_info) {
                response_msg.operation = ACK;
                response_msg.error_code = SUCCESS;
                strcpy(response_msg.ss_ip, ss_info->ip);
                response_msg.ss_client_port = ss_info->client_port;
                log_message("NM: Found path '%s' on SS %s:%d. Directing client.", msg.path, ss_info->ip, ss_info->client_port);
            } else {
                response_msg.operation = NACK;
                response_msg.error_code = FILE_NOT_FOUND;
                log_message("NM: Path '%s' not found.", msg.path);
            }
            break;
        }
        default: {
            log_message("NM: Received unknown operation %d.", msg.operation);
            response_msg.operation = NACK;
            response_msg.error_code = INVALID_OPERATION;
            break;
        }
    }

    send_message(client_sock, &response_msg);
    close(client_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    int listen_sock = create_socket();
    if (listen_sock < 0) {
        return EXIT_FAILURE;
    }

    if (bind_socket(listen_sock, port) < 0) {
        close(listen_sock);
        return EXIT_FAILURE;
    }

    if (listen_socket(listen_sock, 5) < 0) {
        close(listen_sock);
        return EXIT_FAILURE;
    }

    // Initialize Trie and LRU Cache
    path_trie_root = create_trie_node();
    lru_init(&path_lru_cache);

    log_message("Naming Server listening on port %d", port);

    while (1) {
        int *client_sock = malloc(sizeof(int));
        if (!client_sock) {
            perror("Failed to allocate memory for client socket");
            continue;
        }
        *client_sock = accept_connection(listen_sock);
        if (*client_sock < 0) {
            free(client_sock);
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_connection, (void *)client_sock) < 0) {
            perror("Could not create thread");
            close(*client_sock);
            free(client_sock);
            continue;
        }
        pthread_detach(thread_id); // Detach thread so resources are automatically released
    }

    close(listen_sock);
    pthread_mutex_destroy(&ss_list_mutex);
    free_trie(path_trie_root);
    lru_destroy(&path_lru_cache);
    return EXIT_SUCCESS;
}
