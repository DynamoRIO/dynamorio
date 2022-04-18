/* **********************************************************
 * Copyright (c) 2013 Google, Inc.   All rights reserved.
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

/* kernel32.dll and kernelbase.dll memory-related redirection routines */

#include "kernel32_redir.h" /* must be included first */
#include "../../globals.h"

DWORD
WINAPI
redirect_GetLastError(VOID)
{
    return get_last_error();
}

VOID WINAPI
redirect_SetLastError(__in DWORD dwErrCode)
{
    set_last_error(dwErrCode);
}

/* FIXME i#1063: add the rest of the routines in kernel32_redir.h under
 * Miscellaneous
 */

/***************************************************************************
 * TESTS
 */

#ifdef STANDALONE_UNIT_TEST
void
unit_test_drwinapi_kernel32_misc(void)
{
    print_file(STDERR, "testing drwinapi kernel32 miscellaneous routines\n");

    redirect_SetLastError(ERROR_PRINT_CANCELLED);
    EXPECT(redirect_GetLastError(), ERROR_PRINT_CANCELLED);
}
#endif
