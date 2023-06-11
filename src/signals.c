#include "signals.h"

volatile sig_atomic_t shutdown_signal = 0;

pthread_rwlock_t shutdown_signal_rwlock;

void
installSignals()
{
    struct sigaction myaction = {{0}};
    myaction.sa_handler = sigint_handler;

    if(sigaction(SIGINT, &myaction, NULL) == -1) 
        printf("signal handler failed to install\n");
}

void
sigint_handler(int sigNum)
{
    int olderrno = errno;
    sigset_t mask_all, prev_one;

    pthread_rwlock_wrlock(&shutdown_signal_rwlock);
	sigfillset(&mask_all);
	sigprocmask(SIG_BLOCK, &mask_all, &prev_one);
    
    shutdown_signal = 1;

    sigprocmask(SIG_SETMASK, &prev_one, NULL);
    pthread_rwlock_unlock(&shutdown_signal_rwlock);

	errno = olderrno;
}