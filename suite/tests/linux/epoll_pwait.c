#include "tools.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <time.h>

static void
signalHandler(int sig, siginfo_t *siginfo, void *context)
{
    print("signal received: %d\n", sig);
}

int
main(int argc, char *argv[])
{
    struct sigaction act;
    sigset_t new_set;

    INIT();
    memset(&act, '\0', sizeof(act));

    act.sa_sigaction = &signalHandler;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("sigaction failed\n");
        return 1;
    }
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        perror("sigaction failed\n");
        return 1;
    }
    print("handlers for signals: %d, %d\n", SIGUSR1, SIGUSR2);
    sigemptyset(&new_set);
    sigaddset(&new_set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &new_set, NULL);
    print("signal blocked: %d\n", SIGUSR1);

    int pid = fork();
    if (pid < 0) {
        perror("fork error");
    } else if (pid == 0) {
        struct timespec sleeptime;
        sleeptime.tv_sec = 1;
        sleeptime.tv_nsec = 0;
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR2);
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR1);
        /* waste some time */
        nanosleep(&sleeptime, NULL);
        kill(getppid(), SIGUSR1);
        return 0;
    }

    int epollFD = epoll_create1(02000000);
    struct epoll_event events;

    int count = 0;
    while (count++ < 3) {
        sigset_t empty_set;
        sigemptyset(&empty_set);
        /* XXX i#3240: DR currently does not handle the atomicity aspect of this system
         * call. Once it does, please include this in this test or add a new test. */
        epoll_pwait(epollFD, &events, 24, -1, &empty_set);
    };
    return 0;
}
