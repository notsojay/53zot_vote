#include "sockets.h"

int
initSocket(unsigned int portNum)
{
    int socket_fd = 0, opt = 1;;
    struct sockaddr_in servAddr;

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(portNum);

    socket_fd = createSocket(AF_INET, SOCK_STREAM, 0);
    setSocketOption( socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt) );
    bindSocket(socket_fd, (struct sockaddr*)&servAddr, sizeof(servAddr));
    listenSocket(socket_fd, 1);
    fprintf(stdout, "Currently listening on port %d.\n", portNum);

    return socket_fd;
}

int
createSocket(int domain, int type, int protocol) 
{
    int socket_fd = socket(domain, type, protocol);
    if(socket_fd == -1) 
    {
        reportUnixError("Socket creation failed\n", EXIT_FAILURE);
    }

    return socket_fd;
}

void
setSocketOption(int socket_fd, int level, int optname, const void *optval, socklen_t optlen) 
{
    if( setsockopt(socket_fd, level, optname, optval, optlen) < 0 ) 
    {
        reportUnixError("Socket Setsockopt failed\n", EXIT_FAILURE);
    }
}

void
bindSocket(int socket_fd, const struct sockaddr *addr, socklen_t addrlen) 
{
    if( bind(socket_fd, addr, addrlen) != 0 ) 
    {
        reportUnixError("Socket bind failed\n", EXIT_FAILURE);
    }
}

void
listenSocket(int socket_fd, int backlog) 
{
    if( listen(socket_fd, backlog) != 0) 
    {
        reportUnixError("Socket listen failed\n", EXIT_FAILURE);
    }
}

int
sendMessage(int socket_fd, char *msgBody, uint32_t msgLen, uint8_t msgType)
{
    petrV_header msgHeader;
    memset(&msgHeader, 0, sizeof(petrV_header));
    msgHeader.msg_type = msgType;
    msgHeader.msg_len = msgLen;

    return wr_msg(socket_fd, &msgHeader, msgBody);
}