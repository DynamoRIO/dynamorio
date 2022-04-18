/* *******************************************************************************
 * Copyright (c) 2013-2022 Google, Inc.  All rights reserved.
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

#ifndef DR_CLIENT_TOOLS_H
#define DR_CLIENT_TOOLS_H

/* Common definitions for test suite clients. */

/* Ignore any PAGE_SIZE provided by the tool chain and define a new version
 * using dr_api.h.
 */
#undef PAGE_SIZE
#define PAGE_SIZE dr_page_size()

/* Some tests want to define a static array that contains a whole page. This
 * should be large enough, but a careful user may wish to
 * ASSERT(dr_page_size() <= PAGE_SIZE_MAX).
 */
#define PAGE_SIZE_MAX (64 * 1024)

/* Provide assertion macros that only use dr_fprintf.  The asserts provided by
 * dr_api.h cannot be used in the test suite because they pop up message boxes
 * on Windows.
 */
#define ASSERT_MSG(x, msg)                                                           \
    ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s (%s)", __FILE__, \
                                 __LINE__, #x, msg),                                 \
                      dr_abort(), 0)                                                 \
                   : 0))
#define ASSERT(x) ASSERT_MSG(x, "")
/* Same as ASSERT_MSG, but kept separate due to existing uses across many files. */
#define CHECK(x, msg) ASSERT_MSG(x, msg)

/* Redefine DR_ASSERT* to alias ASSERT*.  This makes it easier to import sample
 * clients into the test suite. */
#undef DR_ASSERT_MSG
#undef DR_ASSERT
#define DR_ASSERT_MSG ASSERT_MSG
#define DR_ASSERT ASSERT

/* Standard pointer-width integer alignment macros.  Not provided by dr_api.h.
 */
#define ALIGN_BACKWARD(x, alignment) (((ptr_uint_t)x) & (~((ptr_uint_t)(alignment)-1)))
#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + (((ptr_uint_t)alignment) - 1)) & (~(((ptr_uint_t)alignment) - 1)))
#define ALIGNED(x, alignment) ((((ptr_uint_t)x) & ((alignment)-1)) == 0)

/* Xref i#302 */
#define POINTER_OVERFLOW_ON_ADD(ptr, add) \
    (((ptr_uint_t)(ptr)) + (add) < ((ptr_uint_t)(ptr)))
#define POINTER_UNDERFLOW_ON_SUB(ptr, sub) \
    (((ptr_uint_t)(ptr)) - (sub) > ((ptr_uint_t)(ptr)))

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf) buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0

/* check if all bits in mask are set in var */
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
/* check if any bit in mask is set in var */
#define TESTANY(mask, var) (((mask) & (var)) != 0)
/* check if a single bit is set in var */
#define TEST TESTANY

#ifdef WINDOWS
#    define IF_WINDOWS_ELSE(x, y) x
#    define IF_WINDOWS(x) x
#else
#    define IF_WINDOWS_ELSE(x, y) y
#    define IF_WINDOWS(x)
#endif

static inline void
check_stack_alignment(void)
{
#if defined(X86) && defined(UNIX)
    reg_t sp;
    __asm__ __volatile__("mov %%" IF_X64_ELSE("rsp", "esp") ", %0" : "=m"(sp));
#    define STACK_ALIGNMENT 16
    ASSERT(ALIGNED(sp, STACK_ALIGNMENT));
#elif defined(AARCH64)
    reg_t sp;
    __asm__ __volatile__("mov %0, sp" : "=r"(sp));
#    define STACK_ALIGNMENT 16
    ASSERT(ALIGNED(sp, STACK_ALIGNMENT));
#elif defined(ARM)
    reg_t sp;
    __asm__ __volatile__("str sp, %0" : "=m"(sp));
#    define STACK_ALIGNMENT 8
    ASSERT(ALIGNED(sp, STACK_ALIGNMENT));
#else
    /* TODO i#4267: If we change Windows to be more than 4-byte alignment we should
     * add a separate-file asm routine to check alignment there.
     */
#endif
}

#endif /* DR_CLIENT_TOOLS_H */
