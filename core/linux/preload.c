/* **********************************************************
 * Copyright (c) 2001-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */

/* preload.c - preload library,
 *    used to launch dynamo on legacy binaries
 *
 * Note: preload is called last, so all threads started by other
 *    libraries will NOT be caught, which is essentially the same
 *    behavior as calling dynamorio_app_init() in main()
 */

#define START_DYNAMO 1          /* start dynamo on preload */
#define VERBOSE_INIT_FINI 0     /* notification for _init and _fini  */
#define VERBOSE 0
#define INIT_BEFORE_LIBC 0

#include <stdio.h>
/* for open */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* for getpid */
#include <unistd.h>
/* for rindex */
#include <string.h>

#if VERBOSE
#define pf(fmt, args...) printf(fmt , ## args)
#else 
#define pf(...) /* C99 requires ... to mean at least one argument */
#endif /* VERBOSE */

#if START_DYNAMO
void dynamorio_app_take_over(void);
int dynamorio_app_init(void);
#endif /* START_DYNAMO */

#define MAX_COMMAND_LENGTH 1024

/* Give a global variable definition. */
int nothing = 0;

/* this routine is straight from utils.c 
 * except it uses libc routines open, read, and close, to avoid
 * having to do syscall wrappers here, and since usage is prior
 * to DR taking over and so we don't care about pthreads interference
 */
static void 
getnamefrompid(int pid, char *name, uint maxlen)
{
    int fd, n;
    char tempstring[200+1],*lastpart;
    /*this is a shitty way of getting the process name,
    but i can't think of anything better... */

    snprintf(tempstring, 200+1, "/proc/%d/cmdline", pid);
    fd = open(tempstring, O_RDONLY);
    n = read(fd, tempstring, 200);
    tempstring[n] = '\0';
    lastpart = rindex(tempstring, '/');

    if (lastpart == NULL)
      lastpart = tempstring;
    else
      lastpart++; /*don't include last '/' in name*/ 

    strncpy(name, lastpart, maxlen-1);
    name[maxlen-1]  = '\0'; /* if max no null */

    close(fd);
}

int
#if INIT_BEFORE_LIBC
_init(int argc, char *arg0, ...)
{
  char **argv = &arg0, **envp = &argv[argc + 1];
#else
_init ()
{
#endif
    int init;
    char name[MAX_COMMAND_LENGTH];
#if VERBOSE_INIT_FINI
    fprintf (stderr, "preload initialized\n");
#endif /* VERBOSE_INIT_FINI */

#if INIT_BEFORE_LIBC
  {
      int i;
      for (i=0; i<argc; i++)
          fprintf(stderr, "\targ %d = %s\n", i, argv[i]);
      fprintf(stderr, "env 0 is %s\n", envp[0]);
      fprintf(stderr, "env 1 is %s\n", envp[1]);
      fprintf(stderr, "env 2 is %s\n", envp[2]);
  }
#endif

#if START_DYNAMO
    pf("ready to start dynamo\n");
    getnamefrompid(getpid(), name, MAX_COMMAND_LENGTH);
    pf("preload _init: running %s\n", name);
# ifdef INTERNAL
    /* HACK just for our benchmark scripts: 
     * do not take over a process whose executable is named "texec"
     */
    if (strcmp(name, "texec") == 0) {
        pf("running texec, NOT taking over!\n");
        return 0;
    }
# endif
    init = dynamorio_app_init();
    pf("dynamorio_app_init() returned %d\n", init);
    dynamorio_app_take_over();
    pf("dynamo started\n");
#endif /* START_DYNAMO */

    return 0;
}

int
_fini ()
{
#if VERBOSE_INIT_FINI
    fprintf (stderr, "preload finalized\n");
#endif /* VERBOSE_INIT_FINI */

    /* since we're using dynamorio_app_take_over we do not need to call dr_app_stop
     * or dynamorio_app_exit
     */

    return 0;
}
