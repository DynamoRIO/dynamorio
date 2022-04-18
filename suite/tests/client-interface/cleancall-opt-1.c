/* *******************************************************************************
 * Copyright (c) 2017 ARM Limited. All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * *******************************************************************************/

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
 * * Neither the name of MIT nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL MIT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "tools.h"

/* Export instrumented functions so we can easily find them in client.  */
#ifdef WINDOWS
#    define EXPORT __declspec(dllexport)
#else /* UNIX */
#    define EXPORT __attribute__((visibility("default")))
#endif

/* List of instrumented functions. */
#define FUNCTIONS()             \
    FUNCTION(empty)             \
    FUNCTION(out_of_line)       \
    FUNCTION(modify_gprs)       \
    FUNCTION(inscount)          \
    FUNCTION(compiler_inscount) \
    FUNCTION(bbcount)           \
    FUNCTION(aflags_clobber)    \
    LAST_FUNCTION()

/* Definitions for every function. */
volatile int val;
#define FUNCTION(FUNCNAME)              \
    EXPORT NOINLINE void FUNCNAME(void) \
    {                                   \
        val = 4;                        \
    }
#define LAST_FUNCTION()
FUNCTIONS()
#undef FUNCTION
#undef LAST_FUNCTION

int
main(void)
{
#ifdef __AVX512F__
    /* For the AVX-512 version, make sure the lazy AVX-512 detection actually kicks in. */
    __asm__ __volatile__("vmovups %zmm0, %zmm0");
#endif
    /* Calls to every function. */
#define FUNCTION(FUNCNAME) FUNCNAME();
#define LAST_FUNCTION()
    FUNCTIONS()
#undef FUNCTION
#undef LAST_FUNCTION
}
