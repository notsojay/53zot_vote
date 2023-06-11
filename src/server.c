#include "server.h"


poll_t pollArray[32];

atomic_int numPolls = ATOMIC_VAR_INIT(0);

list_t *userList = NULL;

stats_t currStats;

sem_t currStats_mutex;

pthread_rwlock_t userList_rwlock;

pthread_rwlock_t pollArray_rwlock[32];

sem_t votingLog_mutex;

FILE *log_fp = NULL;


int
main(int argc, char *argv[])
{
    unsigned int portNum = 0;
    char *pollFilename = NULL;
    char *logFilename = NULL;
    int listen_fd = 0, opt = 0;

    while( (opt = getopt(argc, argv, "h") ) != -1 ) 
    {
        switch (opt) 
        {
            case 'h':
                reportUnixError(USAGE_MSG, EXIT_FAILURE);   
        }
    }

    // 3 positional arguments necessary
    if(argc != 4) reportUnixError(USAGE_MSG, EXIT_FAILURE);

    portNum = atoi(argv[1]);
    pollFilename = argv[2];
    logFilename = argv[3];
    
    initServer(portNum, pollFilename, logFilename, &listen_fd);
    runServer(listen_fd);
    shutdownServer(listen_fd);
    
    debug_print("EXIT SUCC!!!\n");

    return EXIT_SUCCESS;
}

void
initServer(unsigned int portNum, const char * const pollFilename, const char * const logFilename, int *listen_fd)
{
    initPolls(pollFilename);

    *listen_fd = initSocket(portNum);

    memset(&currStats, 0, sizeof(stats_t));
    sem_init(&currStats_mutex, 0, 1);

    userList = CreateList(&userListComparator, &userListPrinter, &userListDeleter);
    pthread_rwlock_init(&userList_rwlock, NULL);

    log_fp = openFile(logFilename, "wrt");
    sem_init(&votingLog_mutex, 0, 1);
    
    installSignals();
    pthread_rwlock_init(&shutdown_signal_rwlock, NULL);
}

void
initPolls(const char * const pollFilename)
{
    FILE *poll_fp = openFile(pollFilename, "rt");
    char buff[BUFFER_SIZE+1], *start = NULL, *end = NULL, delim = '\0';
    int numChoices = 0;

    if(isEmptyFile(poll_fp)) reportUnixError("Invalid file\n", 2);

    for(int i = 0; fgets(buff, BUFFER_SIZE, poll_fp) != NULL && i < 32; ++i)
    {
        start = end = buff;
        pollArray[i].question = extractField(&start, &end, ';');

        numChoices = *end - '0';
        pollArray[i].numChoices = numChoices;
        end += 2;
        
        for(int j = 0; j < numChoices; ++j)
        {
            start = end;
            pollArray[i].options[j].text = extractField(&start, &end, ',');

            start = end;
            pollArray[i].options[j].voteCnt = atoi(start);
            delim = (j+1 < numChoices) ? ';' : '\n';
            findChar(&end, delim);
        }
        for(int j = numChoices; j < 4; ++j)
        {
            pollArray[i].options[j].text = NULL;
            pollArray[i].options[j].voteCnt = 0;
        }
        atomic_fetch_add(&numPolls, 1);
        pthread_rwlock_init(&pollArray_rwlock[i], NULL);
    }
    for(int i = atomic_load(&numPolls); i < 32; ++i)
    {
        pollArray[i].question = NULL;
        pollArray[i].numChoices = 0;
    }

    fprintf(stdout, "Server initialized with %d polls.\n", atomic_load(&numPolls));
}

int
userListComparator(const void *data1, const void *data2)
{
    return strcmp( ((user_t*)data1)->username, (char*)data2 );
}

void
userListPrinter(void *data, void *fp)
{
    fprintf(fp, "%s, %u", ((user_t*)data)->username, ((user_t*)data)->pollVotes);
}

void
userListDeleter(void *data)
{
    if(!data) return;
    if( ((user_t*)data)->username )
    {
        free( (void*)(((user_t*)data)->username) );
        ((user_t*)data)->username = NULL;
    }
    free(data);
    data = NULL;
}

void
runServer(int listen_fd)
{
    struct sockaddr_in clientAddr;
    int client_fd = -1;
    socklen_t clientAddrLen = 0;
    user_t *currUser = NULL;
    memset(&clientAddr, 0, sizeof(struct sockaddr_in));

    while(!receiveShutdown())
    {
        debug_print("Server currently listening...\n");

        clientAddrLen = sizeof(clientAddr);
        client_fd = accept(listen_fd, (struct sockaddr*)&clientAddr, &clientAddrLen);

        debug_print("Server accept return %d, SIGINT_FLAG = %d errno = %d\n", client_fd, shutdown_signal, errno);

        if(client_fd < 0)
        {
            if(receiveShutdown() && errno == EINTR) break;
            else continue;
        }

        sem_wait(&currStats_mutex);
        ++currStats.clientCnt;
        sem_post(&currStats_mutex);

        if( !(currUser = verifyUser(client_fd)) )
        {
            close(client_fd);
            continue;
        }

        pthread_create(&(currUser->tid), NULL, handleClient, (void*)currUser);
        debug_print("%s connetion accepted\n", currUser->username);

        sem_wait(&currStats_mutex);
        ++currStats.threadCnt;
        sem_post(&currStats_mutex);
    }
}

user_t*
verifyUser(int client_fd)
{
    char buff[BUFFER_SIZE+1];
    ssize_t receivedSize = 0;
    node_t *currNode = NULL;
    petrV_header msgHeader;
    memset(&msgHeader, 0, sizeof(petrV_header));

    if( rd_msgheader(client_fd, &msgHeader) == -1 || 
        msgHeader.msg_type != LOGIN || 
        (receivedSize = read(client_fd, buff, msgHeader.msg_len)) <= 0 )
    {
        return NULL;
    }

    buff[receivedSize] = '\0';

    pthread_rwlock_rdlock(&userList_rwlock);
    currNode = findNode(userList, (void*)buff, 0);
    pthread_rwlock_unlock(&userList_rwlock);

    if(!currNode)
    {
        user_t *newUser = (user_t*)malloc(sizeof(user_t));
        memset(newUser, 0, sizeof(user_t));
        newUser->username = (char*)malloc((receivedSize+1) * sizeof(char));
        strncpy(newUser->username , buff, receivedSize);
        newUser->username[receivedSize] = '\0';
        newUser->socket_fd = client_fd;

        pthread_rwlock_wrlock(&userList_rwlock);
        InsertAtHead(userList, (void*)newUser);
        pthread_rwlock_unlock(&userList_rwlock);
        
        sprintf(buff, "CONNECTED %s\n", newUser->username);
        sem_wait(&votingLog_mutex);
        fputs(buff, log_fp);
        sem_post(&votingLog_mutex);

        SEND_SATISFY_TO_CLIENT(client_fd);
        return newUser;
    }
    else
    {
        pthread_rwlock_rdlock(&userList_rwlock);

        if( ((user_t*)currNode->data)->socket_fd >= 0 )
        {
            pthread_rwlock_unlock(&userList_rwlock);

            sprintf(buff, "REJECT %s\n", ((user_t*)currNode->data)->username);
            sem_wait(&votingLog_mutex);
            fputs(buff, log_fp);
            sem_post(&votingLog_mutex);
            SEND_ERROR_TO_CLIENT(client_fd, EUSRLGDIN, NULL);
        }

        pthread_rwlock_unlock(&userList_rwlock);
    }

    pthread_rwlock_wrlock(&userList_rwlock);
    ((user_t*)currNode->data)->socket_fd = client_fd;
    pthread_rwlock_unlock(&userList_rwlock);

    sprintf(buff, "RECONNECTED %s\n", ((user_t*)currNode->data)->username);

    sem_wait(&votingLog_mutex);
    fputs(buff, log_fp);
    sem_post(&votingLog_mutex);
    
    SEND_SATISFY_TO_CLIENT(client_fd);

    return (user_t*)(currNode->data);    
}

bool
receiveShutdown()
{
    pthread_rwlock_rdlock(&shutdown_signal_rwlock);
    volatile sig_atomic_t temp = shutdown_signal;
    pthread_rwlock_unlock(&shutdown_signal_rwlock);
    return temp;
}

void
shutdownServer(int listen_fd)
{
    debug_print("Server shutdowning...\n");

    close(listen_fd);

    killClientThreads(); // send SIGINT to cach of the threads using pthread_kill
    reapClientThreads();

    printPollsResult();

    fclose(log_fp);
    sem_destroy(&votingLog_mutex);

    fprintf(stderr,
            "%d, %d, %d\n",
            currStats.clientCnt,
            currStats.threadCnt,
            currStats.totalVotes);
    sem_destroy(&currStats_mutex);

    pthread_rwlock_destroy(&shutdown_signal_rwlock);
}

void
killClientThreads()
{
    pthread_rwlock_rdlock(&userList_rwlock);

    if(!userList->head) 
    {
        pthread_rwlock_unlock(&userList_rwlock);
        return;
    }

    node_t *currNode = userList->head;

    while(currNode)
    {
        user_t *currUser = (user_t*)currNode->data;
        if(currUser->tid > 0) pthread_kill(currUser->tid, SIGINT);
        debug_print("Send SIGINT to %s\n", currUser->username);
        currNode = currNode->next;
    }

    pthread_rwlock_unlock(&userList_rwlock);
}

void
reapClientThreads()
{ 
    node_t *current = userList->head, *previous = NULL;

    while(current)
    {
        if( ((user_t*)current->data)->tid > 0 )
        {
            pthread_join( ((user_t*)current->data)->tid, NULL );
            debug_print("Reaped %s\n", ((user_t*)current->data)->username);
        }
        current = current->next;
    }

    current = userList->head;

    while(current)
    {
        previous = current;
        current = current->next;

        userList->printer( (void*)(((user_t*)previous->data)), stderr );
        fprintf(stderr, "\n");

        userList->deleter( (void*)(((user_t*)previous->data)) );
        free(previous);
        previous = NULL;
    }

    userList->head = NULL;
    userList->length = 0;
    free(userList);
    userList = NULL;
    pthread_rwlock_destroy(&userList_rwlock);
}

void
printPollsResult()
{
    char buff[BUFFER_SIZE+1];
    int j = 0;

    for(int i = 0; i < atomic_load(&numPolls); ++i)
    {
        memset(buff, 0, sizeof(buff));
        buff[0] = '0', buff[1] = ';', buff[2] = '\0';

        fprintf(stdout, "%s;", pollArray[i].question);
        free( (void*)(pollArray[i].question) );

        for(j = 0; j < pollArray[i].numChoices; ++j)
        {
            strcat(buff, pollArray[i].options[j].text);

            strcat(buff, ",\0");

            sprintf(buff + strlen(buff),
                    "%d",
                    pollArray[i].options[j].voteCnt); 

            if(j+1 >= pollArray[i].numChoices) strcat(buff, "\n\0");
            else strcat(buff, ";\0");

            free( (void*)(pollArray[i].options[j].text) );
        }
        buff[0] = j + '0';
        fprintf(stdout, "%s", buff);
        pthread_rwlock_destroy(&pollArray_rwlock[i]);
    }
}

void*
handleClient(void *currUser)
{   
    petrV_header msgHeader;
    pthread_rwlock_rdlock(&userList_rwlock);
    int client_fd  = ((user_t*)currUser)->socket_fd;
    uint32_t pollVotes = ((user_t*)currUser)->pollVotes;
    char *username = (char*)malloc((strlen(((user_t*)currUser)->username)+10) * sizeof(char));
    strcpy(username, ((user_t*)currUser)->username);
    pthread_rwlock_unlock(&userList_rwlock);

    while(!receiveShutdown())
    {
        memset(&msgHeader, 0, sizeof(petrV_header));

        if( rd_msgheader(client_fd , &msgHeader) < 0  )
        {
            debug_print("Client %s rd_msgheader return < 0, SIGINT_FLAG = %d, errno = %d\n", username, shutdown_signal, errno);

            if( receiveShutdown() || errno == EBADF || 
                errno == EPIPE || errno == EINTR || 
                errno == ECONNRESET || errno == EINVAL ) 
                break;
            else
                continue;
        }
        if(msgHeader.msg_type == LOGOUT) 
        {
            strcat(username, " LOGOUT\n\0");
            sem_wait(&votingLog_mutex);
            fputs(username, log_fp);
            debug_print("%s", username);
            sem_post(&votingLog_mutex);
            break;
        }

        switch(msgHeader.msg_type)
        {
            case PLIST:
                pollListToClient(client_fd, pollVotes, username);
                break;
            case VOTE:
                voteByClient(client_fd, &pollVotes, msgHeader.msg_len, username);
                break;
            case STATS:
                sendStatsToClient(client_fd, pollVotes, msgHeader.msg_len, username);
                break;
            default:
                sendMessage(client_fd, NULL, 0, ESERV);
                continue;
        }
    }

    SEND_SATISFY_TO_CLIENT(client_fd);

    pthread_rwlock_wrlock(&userList_rwlock);
    debug_print("%s disconnected\n", ((user_t*)currUser)->username);
    ((user_t*)currUser)->socket_fd = -1;
    ((user_t*)currUser)->pollVotes = pollVotes;
    pthread_rwlock_unlock(&userList_rwlock);

    free(username);
    close(client_fd);
    debug_print("%s exit!\n", ((user_t*)currUser)->username);

    return NULL;
}

void
pollListToClient(int client_fd, uint32_t pollVotes, char *username)
{
    char buff[32*BUFFER_SIZE+1];
    memset(buff, 0, sizeof(buff));

    for(int i = 0; i < atomic_load(&numPolls); ++i)
    {
        pthread_rwlock_rdlock(&pollArray_rwlock[i]);

        sprintf(buff + strlen(buff),
                "Poll %d - %s - ",
                i,
                pollArray[i].question);

        if(getBit(pollVotes, i) == 1)
        {
            pthread_rwlock_unlock(&pollArray_rwlock[i]);
            strcat(buff, "\n\0");
            continue;
        }

        for(int j = 0; j < pollArray[i].numChoices; ++j)
        {
            sprintf(buff + strlen(buff),
                    "%d:%s",
                    j,
                    pollArray[i].options[j].text);

            if(j+1 >= pollArray[i].numChoices) strcat(buff, "\n\0");
            else strcat(buff, ", \0");
        }
        pthread_rwlock_unlock(&pollArray_rwlock[i]);
    }

    sendMessage(client_fd, buff, strlen(buff)+1, PLIST);

    sprintf(buff, "%s PLIST\n", username);

    sem_wait(&votingLog_mutex);
    fputs(buff, log_fp);
    debug_print("%s", buff);
    sem_post(&votingLog_mutex);
}

void
voteByClient(int client_fd, uint32_t *pollVotes, uint32_t msgLen, char *username)
{
    char buff[BUFFER_SIZE+1], *temp = NULL;
    int pollIndex = 0, optionId = 0, offset = 0;

    if( read(client_fd, buff, msgLen) <= 0 || 
        *buff < '0' || *buff > '9' )
    {
        SEND_ERROR_TO_CLIENT(client_fd, EPNOTFOUND, ;);
    }

    temp = buff;
    pollIndex = atoi(temp);

    if( pollIndex < 0 ||
        pollIndex >= atomic_load(&numPolls) ) 
    {
        SEND_ERROR_TO_CLIENT(client_fd, EPNOTFOUND, ;);
    }
    else if(getBit(*pollVotes, pollIndex) == 1) 
    {
        SEND_ERROR_TO_CLIENT(client_fd, EPDENIED, ;);
    }

    findChar(&temp, ' ');
    optionId = atoi(temp);

    while(*temp >= '0' && *temp <= '9') 
    {
        ++temp;
    }

    pthread_rwlock_wrlock(&pollArray_rwlock[pollIndex]);

    if( *temp != '\0' ||
        optionId < 0 || 
        optionId >= pollArray[pollIndex].numChoices )
    {
        pthread_rwlock_unlock(&pollArray_rwlock[pollIndex]);
        SEND_ERROR_TO_CLIENT(client_fd, ECNOTFOUND, ;);
    }

    setBit(pollVotes, pollIndex, 1);
    ++pollArray[pollIndex].options[optionId].voteCnt;

    pthread_rwlock_unlock(&pollArray_rwlock[pollIndex]);

    sem_wait(&currStats_mutex);
    ++currStats.totalVotes;
    sem_post(&currStats_mutex);

    SEND_SATISFY_TO_CLIENT(client_fd);

    sprintf(buff,
            "%s VOTE %d %d %d\n",
            username,
            pollIndex,
            optionId,
            *pollVotes);
            
    sem_wait(&votingLog_mutex);
    fputs(buff, log_fp);
    debug_print("%s", buff);
    sem_post(&votingLog_mutex);
}

void
sendStatsToClient(int client_fd, uint32_t pollVotes, uint32_t msgLen, char *username)
{
    char buff[32*BUFFER_SIZE+1];
    int pollIndex = 0;
    bool hasVotedAny = false;

    if( read(client_fd, buff, msgLen) <= 0 ||
        (*buff != '-' && (*buff < '0' || *buff > '9' )) )
    {
        SEND_ERROR_TO_CLIENT(client_fd, EPDENIED, ;);
    }

    if(*buff == '-') 
    {
        pollIndex = atoi(buff+1);
        if(pollIndex != 1) SEND_ERROR_TO_CLIENT(client_fd, EPDENIED, ;);
        memset(buff, 0, sizeof(buff));
        for(pollIndex = 0; pollIndex < atomic_load(&numPolls); ++pollIndex)
        {
            if(getBit(pollVotes, pollIndex) == 1)
            {
                hasVotedAny = true;
                buildStatsForClient(buff, pollIndex);
            }
        }
        if(!hasVotedAny) SEND_ERROR_TO_CLIENT(client_fd, EPDENIED, ;);
    }
    else
    {
        pollIndex = atoi(buff);
        if( pollIndex >= atomic_load(&numPolls) ||
            getBit(pollVotes, pollIndex) != 1 ) 
        {
            SEND_ERROR_TO_CLIENT(client_fd, EPDENIED, ;);
        }
        memset(buff, 0, sizeof(buff));
        buildStatsForClient(buff, pollIndex); 
    }

    sendMessage(client_fd, buff, strlen(buff)+1, STATS);

    sprintf(buff,
            "%s STATS %d\n",
            username,
            pollVotes);
            
    sem_wait(&votingLog_mutex);
    fputs(buff, log_fp);
    debug_print("%s", buff);
    sem_post(&votingLog_mutex);
}

void
buildStatsForClient(char buff[], int pollIndex)
{
    pthread_rwlock_rdlock(&pollArray_rwlock[pollIndex]);

    sprintf(buff + strlen(buff), "Poll %d - ", pollIndex);

    for(int i = 0; i < pollArray[pollIndex].numChoices; ++i)
    {
        sprintf(buff + strlen(buff), 
                "%s:%d",
                pollArray[pollIndex].options[i].text,
                pollArray[pollIndex].options[i].voteCnt);
                    
        if(i+1 >= pollArray[pollIndex].numChoices) strcat(buff, "\n\0");
        else strcat(buff, ",\0");
    }

    pthread_rwlock_unlock(&pollArray_rwlock[pollIndex]);
}
