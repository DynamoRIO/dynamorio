/* **********************************************************
 * Copyright (c) 2012-2013 Google, Inc.  All rights reserved.
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

/* Simple main that just calls each unit test in turn.
 */

#include "globals.h"
#include "arch.h"

void
unit_test_io(void);
#ifdef UNIX
void
unit_test_string(void);
void
unit_test_os(void);
void
unit_test_memquery(void);
#endif
void
unit_test_options(void);
void
unit_test_vmareas(void);
void
unit_test_utils(void);
#ifdef WINDOWS
void
unit_test_drwinapi(void);
#endif
void
unit_test_asm(dcontext_t *dc);
void
unit_test_atomic_ops(void);
void
unit_test_jit_fragment_tree(void);

int
main(int argc, char **argv, char **envp)
{
    dcontext_t *dc = standalone_init();

    /* Each test will abort if it fails, so we just call each in turn and return
     * 0 for success.  If we want to be able to call each test independently, it
     * might be worth looking into gtest, which already does this.
     */
    unit_test_io();
#ifdef UNIX
    unit_test_string();
    unit_test_os();
    unit_test_memquery();
#endif
    unit_test_utils();
    unit_test_options();
    unit_test_vmareas();
#ifdef WINDOWS
    unit_test_drwinapi();
#endif
    unit_test_asm(dc);
    unit_test_atomic_ops();
    unit_test_jit_fragment_tree();
    print_file(STDERR, "all done\n");
    standalone_exit();
    return 0;
}
