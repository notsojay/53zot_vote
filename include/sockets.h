#ifndef SOCKETS_H
#define SOCKETS_H

#include "utilities.h"
#include "signals.h"
#include "protocol.h"

#include <sys/socket.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int initSocket(unsigned int portNum);

int createSocket(int domain, int type, int protocol);

void setSocketOption(int socket_fd, int level, int optname, const void *optval, socklen_t optlen);

void bindSocket(int socket_fd, const struct sockaddr *addr, socklen_t addrlen);

void listenSocket(int socket_fd, int backlog);

int acceptClient(int listen_fd, struct sockaddr *clientAddr, socklen_t *clientAddrLen);

int sendMessage(int socket_fd, char *msgBody, uint32_t msgLen, uint8_t msgType);

#endif 