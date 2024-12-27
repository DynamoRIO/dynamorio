/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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

#define START_DYNAMO 1      /* start dynamo on preload */
#define VERBOSE_INIT_FINI 0 /* notification for _init and _fini  */
#define VERBOSE 0
#define INIT_BEFORE_LIBC 0

#include "configure.h"
#include "globals_shared.h"
#include "../config.h"
#include <stdio.h>
/* for getpid */
#include <unistd.h>
/* for getenv */
#include <stdlib.h>
/* for rindex or strstr */
#include <string.h>

#if VERBOSE
#    define pf(fmt, args...) printf(fmt, ##args)
#else
#    define pf(...) /* C99 requires ... to mean at least one argument */
#endif              /* VERBOSE */

#if START_DYNAMO
#    ifdef VMX86_SERVER
/* This function is not statically linked so as avoid duplicating or compiling
 * DR code into libdrpreload.so, which is messy.  As libdynamorio.so is
 * already loaded into the process and avaiable, it is cleaner to just use
 * functions from it, i.e., dynamic linking.  See PR 212034.
 */
void
vmk_init_lib(void);
#    endif
char *
get_application_short_name(void);
void
dynamorio_set_envp(char **envp);
int
dynamorio_app_init(void);
void
dynamorio_app_take_over(void);
#endif /* START_DYNAMO */

#define MAX_COMMAND_LENGTH 1024

/* Give a global variable definition. */
int nothing = 0;

/* Tells whether or not to take over a process.  PR 212034.  We use env vars to
 * decide this; longer term we want to switch to config files.
 *
 * If include list exists then it acts as an allow list, i.e., take over
 * only if pname is on it, not otherwise.  If the list doesn't exist then
 * act normal, i.e., take over.  Ditto but reversed for exclude list as it is a
 * blocklist.  If both lists exist, then the allow list gets preference.
 */
static bool
take_over(const char *pname)
{
    char *plist;
    const char *runstr;
    bool app_specific, from_env, rununder_on;
#ifdef INTERNAL
    /* HACK just for our benchmark scripts:
     * do not take over a process whose executable is named "texec"
     */
    if (strcmp(pname, "texec") == 0) {
        pf("running texec, NOT taking over!\n");
        return false;
    }
#endif

    /* Guard against "" pname because strstr will return plist if so! */
    if (pname[0] == '\0')
        return true;

    /* i#85/PR 212034: use config files */
    d_r_config_init();
    runstr = get_config_val_ex(DYNAMORIO_VAR_RUNUNDER, &app_specific, &from_env);
    if (!should_inject_from_rununder(runstr, app_specific, from_env, &rununder_on) ||
        !rununder_on)
        return false;

    /* FIXME PR 546894: eliminate once all users are updated to use config files */
    plist = getenv("DYNAMORIO_INCLUDE");
    if (plist != NULL)
        return strstr(plist, pname) ? true : false;

    /* FIXME PR 546894: eliminate once all users are updated to use config files */
    plist = getenv("DYNAMORIO_EXCLUDE");
    if (plist != NULL)
        return strstr(plist, pname) ? false : true;

    return true;
}

int
#if INIT_BEFORE_LIBC
_init(int argc, char *arg0, ...)
{
    char **argv = &arg0, **envp = &argv[argc + 1];
#else
_init(int argc, char **argv, char **envp)
{
#endif
    const char *name;
#if VERBOSE_INIT_FINI
    fprintf(stderr, "preload initialized\n");
#endif /* VERBOSE_INIT_FINI */
#ifdef VMX86_SERVER
    vmk_init_lib();
#endif

#if VERBOSE
    {
        int i;
        for (i = 0; i < argc; i++)
            fprintf(stderr, "\targ %d = %s\n", i, argv[i]);
        fprintf(stderr, "env 0 is %s\n", envp[0]);
        fprintf(stderr, "env 1 is %s\n", envp[1]);
        fprintf(stderr, "env 2 is %s\n", envp[2]);
    }
#endif

#if START_DYNAMO
    pf("ready to start dynamo\n");
    name = get_application_short_name();
    pf("preload _init: running %s\n", name);
    if (!take_over(name))
        return 0;
    /* i#46: Get env from loader directly. */
    dynamorio_set_envp(envp);
    /* FIXME i#287/PR 546544: now load DYNAMORIO_AUTOINJECT DR .so
     * and only LD_PRELOAD the preload lib itself
     */
#    if VERBOSE
    int init =
#    endif
        dynamorio_app_init();
    pf("dynamorio_app_init() returned %d\n", init);
    dynamorio_app_take_over();
    pf("dynamo started\n");
#endif /* START_DYNAMO */

    return 0;
}

int
_fini()
{
#if VERBOSE_INIT_FINI
    fprintf(stderr, "preload finalized\n");
#endif /* VERBOSE_INIT_FINI */

    /* since we're using dynamorio_app_take_over we do not need to call dr_app_stop
     * or dynamorio_app_exit
     */

    return 0;
}
