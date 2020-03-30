#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>

volatile unsigned i = 0;

static void *
thread(void *unused)
{
    while (1) {
        ++i;
    }
    return NULL;
}

int
main()
{
    pthread_t t;
    pthread_create(&t, NULL, thread, NULL);
    pid_t pid = fork();
    if (pid > 0) {
        wait(NULL); /* for reproducible output */
    }
}
