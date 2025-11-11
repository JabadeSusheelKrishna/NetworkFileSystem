#include "common.h"
#include "utils.h"
#include "ss_file_operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>

// Global variables for SS info
char ss_ip[INET_ADDRSTRLEN];
int ss_nm_port;
int ss_client_port;
char accessible_paths[MAX_ACCESSIBLE_PATHS][MAX_PATH_LEN];
int num_accessible_paths = 0;

// Function to get local IP address
void get_local_ip(char *buffer) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed for IP lookup");
        strcpy(buffer, "127.0.0.1"); // Fallback to localhost
        return;
    }

    const char* google_dns_server = "8.8.8.8";
    int dns_port = 53;

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(google_dns_server);
    serv.sin_port = htons(dns_port);

    int err = connect(sock, (const struct sockaddr*)&serv, sizeof(serv));
    if (err < 0) {
        perror("Connect failed for IP lookup");
        close(sock);
        strcpy(buffer, "127.0.0.1"); // Fallback to localhost
        return;
    }

    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (struct sockaddr*)&name, &namelen);
    if (err < 0) {
        perror("Getsockname failed for IP lookup");
        close(sock);
        strcpy(buffer, "127.0.0.1"); // Fallback to localhost
        return;
    }

    inet_ntop(AF_INET, &name.sin_addr, buffer, INET_ADDRSTRLEN);
    close(sock);
}

// Function to handle client connections
void *handle_client_connection(void *socket_desc) {
    int client_sock = *(int *)socket_desc;
    free(socket_desc);
    Message msg;
    Message response_msg;
    memset(&response_msg, 0, sizeof(Message));

    log_message("SS: New client connection established on client port.");

    if (receive_message(client_sock, &msg) != 0) {
        log_message("SS: Failed to receive message from client or connection closed.");
        close(client_sock);
        return NULL;
    }

    response_msg.operation = ACK;
    response_msg.error_code = SUCCESS;

    switch (msg.operation) {
        case READ_FILE: {
            int bytes_read = 0;
            if (acquire_read_lock(msg.path) == 0) {
                response_msg.error_code = ss_read(msg.path, response_msg.data, &bytes_read);
                response_msg.data_size = bytes_read;
                release_read_lock(msg.path);
            } else {
                response_msg.error_code = FILE_IN_USE;
            }
            break;
        }
        case WRITE_FILE: {
            if (acquire_write_lock(msg.path) == 0) {
                response_msg.error_code = ss_write(msg.path, msg.data, msg.data_size, msg.write_mode);
                release_write_lock(msg.path);
            } else {
                response_msg.error_code = FILE_IN_USE;
            }
            break;
        }
        case GET_FILE_INFO: {
            if (acquire_read_lock(msg.path) == 0) {
                response_msg.error_code = ss_get_info(msg.path, &response_msg);
                release_read_lock(msg.path);
            } else {
                response_msg.error_code = FILE_IN_USE;
            }
            break;
        }
        default: {
            log_message("SS: Received unknown operation %d from client.", msg.operation);
            response_msg.operation = NACK;
            response_msg.error_code = INVALID_OPERATION;
            break;
        }
    }

    send_message(client_sock, &response_msg);
    close(client_sock);
    return NULL;
}

// Function to handle NM connections
void *handle_nm_connection(void *socket_desc) {
    int nm_sock = *(int *)socket_desc;
    free(socket_desc);
    Message msg;
    Message response_msg;
    memset(&response_msg, 0, sizeof(Message));

    log_message("SS: New NM connection established on NM service port.");

    if (receive_message(nm_sock, &msg) != 0) {
        log_message("SS: Failed to receive message from NM or connection closed.");
        close(nm_sock);
        return NULL;
    }

    response_msg.operation = ACK;
    response_msg.error_code = SUCCESS;

    switch (msg.operation) {
        case CREATE_FILE: {
            if (acquire_write_lock(msg.path) == 0) {
                response_msg.error_code = ss_create(msg.path, msg.file_type);
                release_write_lock(msg.path);
            } else {
                response_msg.error_code = FILE_IN_USE;
            }
            break;
        }
        case DELETE_FILE: {
            if (acquire_write_lock(msg.path) == 0) {
                response_msg.error_code = ss_delete(msg.path);
                release_write_lock(msg.path);
            }
            else {
                response_msg.error_code = FILE_IN_USE;
            }
            break;
        }
        case COPY_FILE: {
            // Placeholder for copy operation. Will implement later.
            log_message("SS: Received COPY_FILE request for %s to %s. Not yet implemented.", msg.path, msg.path2);
            response_msg.operation = NACK;
            response_msg.error_code = INVALID_OPERATION; // Or a specific error for not implemented
            break;
        }
        default: {
            log_message("SS: Received unknown operation %d from NM.", msg.operation);
            response_msg.operation = NACK;
            response_msg.error_code = INVALID_OPERATION;
            break;
        }
    }

    send_message(nm_sock, &response_msg);
    close(nm_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <nm_ip> <nm_port> <ss_client_port> <ss_nm_service_port> [path1] [path2] ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *nm_ip = argv[1];
    int nm_port = atoi(argv[2]);
    ss_client_port = atoi(argv[3]);
    ss_nm_port = atoi(argv[4]); // This is the port the NM will use to connect to this SS

    get_local_ip(ss_ip);

    // Parse accessible paths
    for (int i = 5; i < argc && num_accessible_paths < MAX_ACCESSIBLE_PATHS; i++) {
        strcpy(accessible_paths[num_accessible_paths], argv[i]);
        num_accessible_paths++;
    }

    // Initialize file locking mechanism
    init_file_locks();

    // 1. Connect to Naming Server and Register
    int nm_sockfd = create_socket();
    if (nm_sockfd < 0) {
        destroy_file_locks();
        return EXIT_FAILURE;
    }

    log_message("SS: Connecting to Naming Server at %s:%d", nm_ip, nm_port);
    if (connect_to_server(nm_sockfd, nm_ip, nm_port) < 0) {
        close(nm_sockfd);
        destroy_file_locks();
        return EXIT_FAILURE;
    }

    Message register_msg;
    memset(&register_msg, 0, sizeof(Message));
    register_msg.operation = REGISTER_SS;
    strcpy(register_msg.ss_ip, ss_ip);
    register_msg.ss_nm_port = ss_nm_port;
    register_msg.ss_client_port = ss_client_port;
    register_msg.num_accessible_paths = num_accessible_paths;
    for (int i = 0; i < num_accessible_paths; i++) {
        strcpy(register_msg.accessible_paths[i], accessible_paths[i]);
    }

    log_message("SS: Sending registration message to NM.");
    if (send_message(nm_sockfd, &register_msg) < 0) {
        close(nm_sockfd);
        destroy_file_locks();
        return EXIT_FAILURE;
    }

    Message response_msg;
    if (receive_message(nm_sockfd, &response_msg) != 0) {
        log_message("SS: Failed to receive registration response from NM or connection closed.");
        close(nm_sockfd);
        destroy_file_locks();
        return EXIT_FAILURE;
    }

    if (response_msg.operation == ACK && response_msg.error_code == SUCCESS) {
        log_message("SS: Successfully registered with Naming Server.");
    } else {
        log_message("SS: Failed to register with Naming Server. Error code: %d", response_msg.error_code);
        close(nm_sockfd);
        destroy_file_locks();
        return EXIT_FAILURE;
    }
    close(nm_sockfd);

    // 2. Set up listening socket for NM commands
    int nm_listen_sock = create_socket();
    if (nm_listen_sock < 0) {
        destroy_file_locks();
        return EXIT_FAILURE;
    }
    if (bind_socket(nm_listen_sock, ss_nm_port) < 0) {
        close(nm_listen_sock);
        destroy_file_locks();
        return EXIT_FAILURE;
    }
    if (listen_socket(nm_listen_sock, 5) < 0) {
        close(nm_listen_sock);
        destroy_file_locks();
        return EXIT_FAILURE;
    }
    log_message("SS: Listening for Naming Server commands on port %d", ss_nm_port);

    // 3. Set up listening socket for Client connections
    int client_listen_sock = create_socket();
    if (client_listen_sock < 0) {
        destroy_file_locks();
        return EXIT_FAILURE;
    }
    if (bind_socket(client_listen_sock, ss_client_port) < 0) {
        close(client_listen_sock);
        destroy_file_locks();
        return EXIT_FAILURE;
    }
    if (listen_socket(client_listen_sock, 5) < 0) {
        close(client_listen_sock);
        destroy_file_locks();
        return EXIT_FAILURE;
    }
    log_message("SS: Listening for Client connections on port %d", ss_client_port);

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(nm_listen_sock, &read_fds);
        FD_SET(client_listen_sock, &read_fds);

        int max_sd = (nm_listen_sock > client_listen_sock) ? nm_listen_sock : client_listen_sock;

        int activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno!=EINTR)) {
            perror("select error");
        }

        // If something happened on the NM listening socket, then it's an incoming NM connection
        if (FD_ISSET(nm_listen_sock, &read_fds)) {
            int *new_sock = malloc(sizeof(int));
            if (!new_sock) {
                perror("Failed to allocate memory for NM socket");
                continue;
            }
            *new_sock = accept_connection(nm_listen_sock);
            if (*new_sock < 0) {
                free(new_sock);
                continue;
            }
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_nm_connection, (void *)new_sock) < 0) {
                perror("Could not create NM handler thread");
                close(*new_sock);
                free(new_sock);
                continue;
            }
            pthread_detach(thread_id);
        }

        // If something happened on the client listening socket, then it's an incoming client connection
        if (FD_ISSET(client_listen_sock, &read_fds)) {
            int *new_sock = malloc(sizeof(int));
            if (!new_sock) {
                perror("Failed to allocate memory for client socket");
                continue;
            }
            *new_sock = accept_connection(client_listen_sock);
            if (*new_sock < 0) {
                free(new_sock);
                continue;
            }
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client_connection, (void *)new_sock) < 0) {
                perror("Could not create client handler thread");
                close(*new_sock);
                free(new_sock);
                continue;
            }
            pthread_detach(thread_id);
        }
    }

    close(nm_listen_sock);
    close(client_listen_sock);
    destroy_file_locks();
    return EXIT_SUCCESS;
}