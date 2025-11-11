#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <nm_ip> <nm_port> LIST\n", prog_name);
    fprintf(stderr, "  %s <nm_ip> <nm_port> READ <path>\n", prog_name);
    fprintf(stderr, "  %s <nm_ip> <nm_port> WRITE <path> <data> [--SYNC]\n", prog_name);
    fprintf(stderr, "  %s <nm_ip> <nm_port> CREATE <path> [--DIR]\n", prog_name);
    fprintf(stderr, "  %s <nm_ip> <nm_port> DELETE <path>\n", prog_name);
    fprintf(stderr, "  %s <nm_ip> <nm_port> INFO <path>\n", prog_name);
    fprintf(stderr, "  %s <nm_ip> <nm_port> COPY <source_path> <dest_path>\n", prog_name);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    char *nm_ip = argv[1];
    int nm_port = atoi(argv[2]);
    char *command = argv[3];

    int nm_sockfd = create_socket();
    if (nm_sockfd < 0) {
        return EXIT_FAILURE;
    }

    log_message("Client: Connecting to Naming Server at %s:%d", nm_ip, nm_port);
    if (connect_to_server(nm_sockfd, nm_ip, nm_port) < 0) {
        close(nm_sockfd);
        return EXIT_FAILURE;
    }

    Message request_msg;
    memset(&request_msg, 0, sizeof(Message));

    if (strcmp(command, "LIST") == 0) {
        request_msg.operation = LIST_PATHS;
    } else if (strcmp(command, "READ") == 0) {
        if (argc != 5) { print_usage(argv[0]); close(nm_sockfd); return EXIT_FAILURE; }
        request_msg.operation = READ_FILE;
        strcpy(request_msg.path, argv[4]);
    } else if (strcmp(command, "WRITE") == 0) {
        if (argc < 6) { print_usage(argv[0]); close(nm_sockfd); return EXIT_FAILURE; }
        request_msg.operation = WRITE_FILE;
        strcpy(request_msg.path, argv[4]);
        strcpy(request_msg.data, argv[5]);
        request_msg.data_size = strlen(argv[5]);
        request_msg.write_mode = WRITE_MODE_ASYNC; // Default to async
        if (argc == 7 && strcmp(argv[6], "--SYNC") == 0) {
            request_msg.write_mode = WRITE_MODE_SYNC;
        }
    } else if (strcmp(command, "CREATE") == 0) {
        if (argc < 5) { print_usage(argv[0]); close(nm_sockfd); return EXIT_FAILURE; }
        request_msg.operation = CREATE_FILE;
        strcpy(request_msg.path, argv[4]);
        request_msg.file_type = FILE_TYPE_FILE; // Default to file
        if (argc == 6 && strcmp(argv[5], "--DIR") == 0) {
            request_msg.file_type = FILE_TYPE_DIR;
        }
    } else if (strcmp(command, "DELETE") == 0) {
        if (argc != 5) { print_usage(argv[0]); close(nm_sockfd); return EXIT_FAILURE; }
        request_msg.operation = DELETE_FILE;
        strcpy(request_msg.path, argv[4]);
    } else if (strcmp(command, "INFO") == 0) {
        if (argc != 5) { print_usage(argv[0]); close(nm_sockfd); return EXIT_FAILURE; }
        request_msg.operation = GET_FILE_INFO;
        strcpy(request_msg.path, argv[4]);
    } else if (strcmp(command, "COPY") == 0) {
        if (argc != 6) { print_usage(argv[0]); close(nm_sockfd); return EXIT_FAILURE; }
        request_msg.operation = COPY_FILE;
        strcpy(request_msg.path, argv[4]); // Source path
        strcpy(request_msg.path2, argv[5]); // Destination path
    } else {
        log_message("Client: Unknown command: %s", command);
        print_usage(argv[0]);
        close(nm_sockfd);
        return EXIT_FAILURE;
    }

    log_message("Client: Sending %s request to NM.", command);
    if (send_message(nm_sockfd, &request_msg) < 0) {
        close(nm_sockfd);
        return EXIT_FAILURE;
    }

    Message nm_response_msg;
    if (receive_message(nm_sockfd, &nm_response_msg) != 0) {
        log_message("Client: Failed to receive response from NM or connection closed.");
        close(nm_sockfd);
        return EXIT_FAILURE;
    }
    close(nm_sockfd); // Close connection to NM after getting response

    if (nm_response_msg.operation == ACK && nm_response_msg.error_code == SUCCESS) {
        if (request_msg.operation == LIST_PATHS) {
            log_message("Client: Received list of accessible paths from NM:\n%s", nm_response_msg.data);
        } else if (request_msg.operation == CREATE_FILE || request_msg.operation == DELETE_FILE || request_msg.operation == COPY_FILE) {
            // For CREATE/DELETE/COPY, NM directly handles it or coordinates with SS
            // For now, NM just sends ACK/NACK for these. Actual SS interaction is in SS.
            log_message("Client: NM acknowledged %s operation for path %s.", command, request_msg.path);
            // For CREATE/DELETE, NM would ideally tell us if it succeeded on the SS.
            // For COPY, NM would coordinate between SSs.
            // For now, we assume NM's ACK means it's handled.
        } else { // READ_FILE, WRITE_FILE, GET_FILE_INFO
            // NM provides SS info, now connect to SS
            log_message("Client: NM directed to SS at %s:%d for %s operation.", nm_response_msg.ss_ip, nm_response_msg.ss_client_port, command);

            int ss_sockfd = create_socket();
            if (ss_sockfd < 0) {
                return EXIT_FAILURE;
            }

            if (connect_to_server(ss_sockfd, nm_response_msg.ss_ip, nm_response_msg.ss_client_port) < 0) {
                close(ss_sockfd);
                log_message("Client: Failed to connect to Storage Server.");
                return EXIT_FAILURE;
            }

            log_message("Client: Sending %s request to SS.", command);
            if (send_message(ss_sockfd, &request_msg) < 0) {
                close(ss_sockfd);
                return EXIT_FAILURE;
            }

            Message ss_response_msg;
            if (receive_message(ss_sockfd, &ss_response_msg) != 0) {
                log_message("Client: Failed to receive response from SS or connection closed.");
                close(ss_sockfd);
                return EXIT_FAILURE;
            }
            close(ss_sockfd);

            if (ss_response_msg.operation == ACK && ss_response_msg.error_code == SUCCESS) {
                if (request_msg.operation == READ_FILE) {
                    log_message("Client: Read from file %s:\n%s", request_msg.path, ss_response_msg.data);
                } else if (request_msg.operation == WRITE_FILE) {
                    log_message("Client: Successfully wrote to file %s.", request_msg.path);
                } else if (request_msg.operation == GET_FILE_INFO) {
                    log_message("Client: Info for file %s:\n%s", request_msg.path, ss_response_msg.data);
                }
            } else {
                log_message("Client: SS returned NACK for %s. Error code: %d", command, ss_response_msg.error_code);
            }
        }
    } else if (nm_response_msg.operation == NACK) {
        log_message("Client: NM returned NACK for %s. Error code: %d", command, nm_response_msg.error_code);
    } else {
        log_message("Client: Received unexpected response from NM. Operation: %d, Error: %d", nm_response_msg.operation, nm_response_msg.error_code);
    }

    log_message("Client: Operation completed.");

    return EXIT_SUCCESS;
}