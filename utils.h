#ifndef UTILS_H
#define UTILS_H

#include "common.h"

// Function to create a socket
int create_socket();

// Function to bind a socket to a given port
int bind_socket(int sockfd, int port);

// Function to listen for incoming connections
int listen_socket(int sockfd, int backlog);

// Function to accept an incoming connection
int accept_connection(int sockfd);

// Function to connect to a server
int connect_to_server(int sockfd, const char *ip, int port);

// Function to send a message
int send_message(int sockfd, const Message *msg);

// Function to receive a message
int receive_message(int sockfd, Message *msg);

// Function for logging messages
void log_message(const char *format, ...);

#endif // UTILS_H
