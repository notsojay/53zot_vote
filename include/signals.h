#ifndef SIGNALS_H
#define SIGNALS_H

#include "utilities.h"

#include <signal.h> 
#include <string.h> 
#include <errno.h>
#include <pthread.h>

extern volatile sig_atomic_t shutdown_signal;

extern pthread_rwlock_t shutdown_signal_rwlock;

void installSignals();

void sigint_handler(int sigNum);

#endif