#include "utils.h"
#include <stdarg.h>
#include <time.h>

// Function to create a socket
int create_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
    }
    return sockfd;
}

// Function to bind a socket to a given port
int bind_socket(int sockfd, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Socket bind failed");
        return -1;
    }
    return 0;
}

// Function to listen for incoming connections
int listen_socket(int sockfd, int backlog) {
    if (listen(sockfd, backlog) < 0) {
        perror("Socket listen failed");
        return -1;
    }
    return 0;
}

// Function to accept an incoming connection
int accept_connection(int sockfd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_sockfd < 0) {
        perror("Accept connection failed");
    }
    return client_sockfd;
}

// Function to connect to a server
int connect_to_server(int sockfd, const char *ip, int port) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }
    return 0;
}

// Function to send a message
int send_message(int sockfd, const Message *msg) {
    if (send(sockfd, msg, sizeof(Message), 0) < 0) {
        perror("Send failed");
        return -1;
    }
    return 0;
}

// Function to receive a message
int receive_message(int sockfd, Message *msg) {
    ssize_t bytes_received = recv(sockfd, msg, sizeof(Message), 0);
    if (bytes_received < 0) {
        perror("Receive failed");
        return -1;
    } else if (bytes_received == 0) {
        // Connection closed by peer
        return 1; 
    }
    return 0;
}

// Function for logging messages
void log_message(const char *format, ...) {
    time_t timer;
    char buffer[26];
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);

    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] ", buffer);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}
