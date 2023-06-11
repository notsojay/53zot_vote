#ifndef SERVER_H
#define SERVER_H

#include "sockets.h"
#include "signals.h"
#include "utilities.h"
#include "protocol.h"
#include "linkedlist.h"

#include <signal.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <semaphore.h>
#include <errno.h>
#include <stdint.h>

#define BUFFER_SIZE 1024
#define SA struct sockaddr

//#define DEBUG_PRINT
#ifdef DEBUG_PRINT
#define debug_print(fmt, ...) fprintf(stdout, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define debug_print(fmt, ...) do {} while(0)
#endif

#define USAGE_MSG "./bin/zoteVote_server [-h] portNum POLL_FILENAME LOG_FILENAME"\
                  "\n  -h                 Displays this help menu and returns EXIT_SUCCESS."\
                  "\n  portNum        Port number to listen on."\
                  "\n  POLL_FILENAME      File to read poll information from at the start of the server"\
                  "\n  LOG_FILENAME       File to output server actions into. Create/overwrite, if exists\n"

#define SEND_ERROR_TO_CLIENT(client_fd, error, return_val) do { sendMessage(client_fd, NULL, 0, error); return return_val; } while(0)

#define SEND_SATISFY_TO_CLIENT(client_fd) do { sendMessage(client_fd, NULL, 0, OK); } while(0)

typedef struct {
    int clientCnt;  // # of attempted logins (successful and unsuccessful) 
    int threadCnt;  // # of threads created (successful login)
    int totalVotes; // # of votes placed on any poll - updated by all client threads
} stats_t;   // Stats collected since server start-up

typedef struct {
    char* username;	// Pointer to dynamically allocated string
    int socket_fd;		// >= 0 if connection active on server, set to -1 if not active
    pthread_t tid;		// Current thread id, only if active on server
    uint32_t pollVotes;	// Bit vector storage for polls that user has voted on
				// Only updated when the user logout/disconnect from server
} user_t;

typedef struct {
    char* text;	// Pointer to dynamically allocated string
    int voteCnt;  // Count for the choice
} choice_t;

typedef struct {
    char* question;         // Pointer to dynamically allocated string
    choice_t options[4];    // At most 4 options per poll. Stored low to high index.
    int numChoices;
                            // choice_t text pointer set to NULL if the option is not used. 
} poll_t; 


extern poll_t pollArray[32]; // Global variable
                             // One poll per index, stored lowest to highest index.  
                             // Set question pointer to NULL if poll does not exist
                             // Maximum of 32 polls on the server.

extern atomic_int numPolls;

extern list_t *userList;

extern stats_t currStats;

extern sem_t currStats_mutex;

extern pthread_rwlock_t userList_rwlock;

extern pthread_rwlock_t pollArray_rwlock[32];

extern sem_t votingLog_mutex; 

extern volatile sig_atomic_t shutdown_signal;

extern pthread_rwlock_t shutdown_signal_rwlock;

extern FILE *log_fp;


void initServer(unsigned int portNum, const char * const pollFilename, const char * const logFilename, int *listen_fd);

void initPolls(const char * const pollFilename);

int userListComparator(const void *data1, const void *data2);

void userListPrinter(void *data, void *fp);

void userListDeleter(void *data);

void runServer(int listen_fd);

user_t* verifyUser(int clientFd);

bool receiveShutdown();

void shutdownServer(int listen_fd);

void killClientThreads();

void reapClientThreads();

void printPollsResult();

void* handleClient(void *currUser);

void pollListToClient(int client_fd, uint32_t pollVotes, char *username);

void voteByClient(int client_fd, uint32_t *pollVotes, uint32_t msgLen, char *username);

void sendStatsToClient(int client_fd, uint32_t pollVotes, uint32_t msgLen, char *username);

void buildStatsForClient(char buff[], int pollIndex);


#endif
