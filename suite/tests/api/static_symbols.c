/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Ensures that static DR does not produce symbol conflicts with the application.
 * We build with -Wl,--warn-common to get warnings about common symbols too.
 */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

/* We can't really test individual symbols very well here as a future-changes test
 * because we can only test those that have conflicted before, but we at least
 * test libc startup conflicts and general linking of the no-hide library.
 * CMake_symbol_check.cmake does more systematic checks.
 */

#if 0 /* i#3348: Disabling until we rename these symbols. */
int *d_r_initstack = NULL;
byte *initstack_app_xsp = NULL;

bool
is_on_initstack(void)
{
    print("in app's %s\n", __FUNCTION__);
}

void
add_thread(void)
{
    print("in app's %s\n", __FUNCTION__);
}
#endif

int
pathcmp(void)
{
    print("in app's %s\n", __FUNCTION__);
    return 0;
}

void
test_symbol_conflicts(void)
{
#if 0 /* i#3348: Disabling until we rename these symbols. */
    print("d_r_initstack is %p\n", d_r_initstack);
    d_r_initstack = NULL;
#endif
    pathcmp();
}

int
main(int argc, const char *argv[])
{
    print("pre-DR init\n");
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());

    print("pre-DR start\n");
    dr_app_start();
    assert(dr_app_running_under_dynamorio());

    test_symbol_conflicts();

    print("pre-DR stop\n");
    dr_app_stop_and_cleanup();
    print("all done\n");
    return 0;
}
