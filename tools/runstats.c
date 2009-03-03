/* **********************************************************
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* runstats.c
 * by Derek Bruening
 *
 * similar to /usr/bin/time, but adds maximum memory
 * usage statistics, implemented via sampling every 500ms
 *
 * also has texec functionality: will kill child after specified
 * time limit expires
 */

#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h> /* for wait and mmap */
#include <sys/wait.h>  /* for wait */
#include <assert.h>
#include <stdio.h>
#include <sys/time.h> /* itimer */
#include <stdlib.h> /* getenv */
#include <string.h> /* strstr */

#define VERBOSE 0

#define FP stderr

/* just use single-arg handlers */
typedef void (*handler_t)(int);
typedef void (*handler_3_t)(int, struct siginfo *, void *);

typedef struct {
    unsigned long VmSize;
    unsigned long VmLck;
    unsigned long VmRSS;
    unsigned long VmData;
    unsigned long VmStk;
    unsigned long VmExe;
    unsigned long VmLib;
} vmstats_t;

/* global vars for access in signal handler */
static pid_t child;
static vmstats_t vmstats;
static int limit; /* in seconds */
struct timeval start, end;
static int silent; /* whether to print anything */

/***************************************************************************/
/* /proc/self/status reader */

/* these are defined in /usr/src/linux/fs/proc/array.c */
#define STATUS_LINE_LENGTH        4096
#define STATUS_LINE_FORMAT         "%s %d"

void
get_mem_stats(pid_t pid)
{
    char proc_pid_status[64];      /* file name */
    FILE *status;
    char line[STATUS_LINE_LENGTH];
    // open file's /proc/id/status file
    int n = snprintf(proc_pid_status, sizeof(proc_pid_status),
                     "/proc/%d/status", pid);
    if (n<0 || n==sizeof(proc_pid_status))
        assert(0); /* paranoia */
    status = fopen(proc_pid_status,"r");
    if (status == NULL)
        return; /* child already exited */
    while(!feof(status)){
        unsigned val = 0;
        char prefix[STATUS_LINE_LENGTH];
        int len;
        if (NULL==fgets(line, sizeof(line), status))
            break;
        if (strstr(line, "VmSize") != 0) {
            len = sscanf(line, STATUS_LINE_FORMAT, prefix, &val);
            assert(len == 2);
            if (val > vmstats.VmSize)
                vmstats.VmSize = val;
#if VERBOSE
            fprintf(FP, "VmSize is %d kB\n", val);
#endif
        } else if (strstr(line, "VmLck") != 0) {
            len = sscanf(line, STATUS_LINE_FORMAT, prefix, &val);
            assert(len == 2);
            if (val > vmstats.VmLck)
                vmstats.VmLck = val;
#if VERBOSE
            fprintf(FP, "VmLck is %d kB\n", val);
#endif
        } else if (strstr(line, "VmRSS") != 0) {
            len = sscanf(line, STATUS_LINE_FORMAT, prefix, &val);
            assert(len == 2);
            if (val > vmstats.VmRSS)
                vmstats.VmRSS = val;
#if VERBOSE
            fprintf(FP, "VmRSS is %d kB\n", val);
#endif
        } else if (strstr(line, "VmData") != 0) {
            len = sscanf(line, STATUS_LINE_FORMAT, prefix, &val);
            assert(len == 2);
            if (val > vmstats.VmData)
                vmstats.VmData = val;
#if VERBOSE
            fprintf(FP, "VmData is %d kB\n", val);
#endif
        } else if (strstr(line, "VmStk") != 0) {
            len = sscanf(line, STATUS_LINE_FORMAT, prefix, &val);
            assert(len == 2);
            if (val > vmstats.VmStk)
                vmstats.VmStk = val;
#if VERBOSE
            fprintf(FP, "VmStk is %d kB\n", val);
#endif
        } else if (strstr(line, "VmExe") != 0) {
            len = sscanf(line, STATUS_LINE_FORMAT, prefix, &val);
            assert(len == 2);
            if (val > vmstats.VmExe)
                vmstats.VmExe = val;
#if VERBOSE
            fprintf(FP, "VmExe is %d kB\n", val);
#endif
        } else if (strstr(line, "VmLib") != 0) {
            len = sscanf(line, STATUS_LINE_FORMAT, prefix, &val);
            assert(len == 2);
            if (val > vmstats.VmLib)
                vmstats.VmLib = val;
#if VERBOSE
            fprintf(FP, "VmLib is %d kB\n", val);
#endif
            /* final one, so break */
            break;
        }
    }
    fclose(status);
}

/***************************************************************************/

static void
signal_handler(int sig)
{
    if (sig == SIGALRM) {
#if VERBOSE
        fprintf(FP, "just got SIGALRM for %d  =>  ", child);
#endif
        if (limit > 0) {
            gettimeofday(&end, (struct timezone *) 0);
#if VERBOSE
            fprintf(FP, "SIGALRM: comparing limit %d s vs. elapsed %d s\n",
                    limit, end.tv_sec - start.tv_sec);
#endif
            /* for limit, ignore fractions of second */
            if (end.tv_sec - start.tv_sec > limit) {
                /* could use SIGTERM, but on win32 cygwin need SIGKILL */
                kill(child, SIGKILL);
                fprintf(FP, "Timeout after %d seconds\n", limit);
                exit(-1);
            }
        }
        get_mem_stats(child);
    } else if (sig == SIGCHLD) {
#if VERBOSE
        fprintf(FP, "just got SIGCHLD for %d\n", child);
#endif
    }
}

/* set up signal_handler as the handler for signal "sig" */
static void
intercept_signal(int sig, handler_t handler)
{
    int rc;
    struct sigaction act;

    act.sa_sigaction = (handler_3_t) handler;
#if BLOCK_IN_HANDLER
    rc = sigfillset(&act.sa_mask); /* block all signals within handler */
#else
    rc = sigemptyset(&act.sa_mask); /* no signals are blocked within handler */
#endif
    assert(rc == 0);
    act.sa_flags = SA_ONSTACK;
    
    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    assert(rc == 0);
}

#define TV_MSEC tv_usec / 1000

/* some of this code is based on /usr/bin/time source code */
void
print_stats(struct timeval *start, struct timeval *end,
            struct rusage *ru, int status)
{
    unsigned long r;            /* Elapsed real milliseconds.  */
    unsigned long v;            /* Elapsed virtual (CPU) milliseconds.  */
    end->tv_sec -= start->tv_sec;
    if (end->tv_usec < start->tv_usec) {
        /* Manually carry a one from the seconds field.  */
        end->tv_usec += 1000000;
        --end->tv_sec;
    }
    end->tv_usec -= start->tv_usec;

    if (WIFSTOPPED (status)) {
        fprintf(FP, "Command stopped by signal %d\n",
                WSTOPSIG (status));
    } else if (WIFSIGNALED (status)) {
        fprintf(FP, "Command terminated by signal %d\n",
                WTERMSIG (status));
    } else if (WIFEXITED (status) && WEXITSTATUS (status)) {
        fprintf(FP, "Command exited with non-zero status %d\n",
                WEXITSTATUS (status));
    }

    /* Convert all times to milliseconds.  Occasionally, one of these values
       comes out as zero.  Dividing by zero causes problems, so we first
       check the time value.  If it is zero, then we take `evasive action'
       instead of calculating a value.  */
    
    r = end->tv_sec * 1000 + end->tv_usec / 1000;
    
    v = ru->ru_utime.tv_sec * 1000 + ru->ru_utime.TV_MSEC +
        ru->ru_stime.tv_sec * 1000 + ru->ru_stime.TV_MSEC;

    /* Elapsed real (wall clock) time.  */
    if (end->tv_sec >= 3600) {  /* One hour -> h:m:s.  */
        fprintf(FP, "%ld:%02ld:%02ldelapsed ",
                 end->tv_sec / 3600,
                 (end->tv_sec % 3600) / 60,
                 end->tv_sec % 60);
    } else {
        fprintf(FP, "%ld:%02ld.%02ldelapsed ",  /* -> m:s.  */
                 end->tv_sec / 60,
                 end->tv_sec % 60,
                 end->tv_usec / 10000);
    }
    /* % cpu is (total cpu time)/(elapsed time).  */
    if (r > 0)
        fprintf(FP, "%lu%%CPU ", (v * 100 / r));
    else
        fprintf(FP, "?%%CPU ");
    fprintf(FP, "%ld.%02lduser ",
            ru->ru_utime.tv_sec, ru->ru_utime.TV_MSEC / 10);
    fprintf(FP, "%ld.%02ldsystem ",
            ru->ru_stime.tv_sec, ru->ru_stime.TV_MSEC / 10);
    fprintf(FP, "(%ldmajor+%ldminor)pagefaults %ldswaps\n",
            ru->ru_majflt,/* Major page faults.  */
            ru->ru_minflt,/* Minor page faults.  */
            ru->ru_nswap); /* times swapped out */
    fprintf(FP, "(%lu tot, %lu RSS, %lu data, %lu stk, %lu exe, %lu lib)k\n",
            vmstats.VmSize, vmstats.VmRSS, vmstats.VmData,
            vmstats.VmStk, vmstats.VmExe, vmstats.VmLib);

#if VERBOSE
    fprintf(FP, "Memory usage:\n");
    fprintf(FP, "\tVmSize: %d kB\n", vmstats.VmSize);
    fprintf(FP, "\tVmLck: %d kB\n", vmstats.VmLck);
    fprintf(FP, "\tVmRSS: %d kB\n", vmstats.VmRSS);
    fprintf(FP, "\tVmData: %d kB\n", vmstats.VmData);
    fprintf(FP, "\tVmStk: %d kB\n", vmstats.VmStk);
    fprintf(FP, "\tVmExe: %d kB\n", vmstats.VmExe);
    fprintf(FP, "\tVmLib: %d kB\n", vmstats.VmLib);
#endif
}

int usage(char *us)
{
    fprintf(FP, "Usage: %s [-s limit_sec | -m limit_min | -h limit_hr]\n"
                "       [-silent] [-env var value] <program> <args...>\n", us);
    return 0;
}

int main(int argc, char *argv[])
{
    int rc;
    struct itimerval t;
    struct rusage ru;
    int arg_offs = 1;

    if (argc < 2) {
        return usage(argv[0]);
    }
    while (argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-s") == 0) {
            if (argc <= arg_offs+1)
                return usage(argv[0]);
            limit = atoi(argv[arg_offs+1]);
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-m") == 0) {
            if (argc <= arg_offs+1)
                return usage(argv[0]);
            limit = atoi(argv[arg_offs+1])*60;
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-h") == 0) {
            if (argc <= arg_offs+1)
                return usage(argv[0]);
            limit = atoi(argv[arg_offs+1])*3600;
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-v") == 0) {
            /* just ignore -v, only here for compatibility with texec */
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-silent") == 0) {
            silent = 1;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-env") == 0) {
            if (argc <= arg_offs+2)
                return usage(argv[0]);
#if VERBOSE
            fprintf(FP, "setting env var \"%s\" to \"%s\"\n",
                    argv[arg_offs+1], argv[arg_offs+2]);
#endif
            rc = setenv(argv[arg_offs+1], argv[arg_offs+2], 1/*overwrite*/);
            if (rc != 0 ||
                strcmp(getenv(argv[arg_offs+1]), argv[arg_offs+2]) != 0) {
                fprintf(FP, "error in setenv of \"%s\" to \"%s\"\n",
                        argv[arg_offs+1], argv[arg_offs+2]);
                fprintf(FP, "setenv returned %d\n", rc);
                fprintf(FP, "env var \"%s\" is now \"%s\"\n",
                        argv[arg_offs+1], getenv(argv[arg_offs+1]));
                exit(-1);
            }
            arg_offs += 3;
        } else {
            return usage(argv[0]);
        }
        if (limit < 0) {
            return usage(argv[0]);
        }
        if (argc - arg_offs < 1)
            return usage(argv[0]);
    }

    gettimeofday(&start, (struct timezone *) 0);

    child = fork();
    if (child < 0) {
        perror("ERROR on fork");
    } else if (child > 0) {
        pid_t result;
        int status;

        get_mem_stats(child);

        intercept_signal(SIGALRM, (handler_t) signal_handler);
        intercept_signal(SIGCHLD, (handler_t) signal_handler);

        t.it_interval.tv_sec = 0;
        t.it_interval.tv_usec = 500000;
        t.it_value.tv_sec = 0;
        t.it_value.tv_usec = 500000;
        /* must use real timer so timer decrements while process suspended */
        rc = setitimer(ITIMER_REAL, &t, NULL);
        assert(rc == 0);

#if VERBOSE
        fprintf(FP, "parent waiting for child\n");
#endif
        /* need loop since SIGALRM will interrupt us */
        do {
            result = wait3(&status, 0, &ru);
        } while (result != child);
        gettimeofday(&end, (struct timezone *) 0);
#if VERBOSE
        fprintf(FP, "child has exited\n");
#endif
    
        /* turn off timer */
        t.it_interval.tv_usec = 0;
        t.it_value.tv_usec = 0;
        rc = setitimer(ITIMER_REAL, &t, NULL);
        assert(rc == 0);
        
        if (!silent)
            print_stats(&start, &end, &ru, status);
    } else {
        int result;
        result = execvp(argv[arg_offs], argv+arg_offs);
        if (result < 0)
            perror("ERROR in execvp");
    }
    return 0;
}
