/* **********************************************************
 * Copyright (c) 2012-2020 Google, Inc.  All rights reserved.
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

/* Tests that raw TLS slots are initialised to zero.
 * Also tests static TLS in private libraries.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "client_tools.h"
#include <vector>

/* For static TLS testing: */
#ifdef WINDOWS
#    define TLS_ATTR __declspec(thread)
#    pragma warning(disable : 4100) /* Unreferenced formal parameter. */
#else
#    define TLS_ATTR __thread
#endif

/* Test a simple initialized value. */
#define STATIC_TLS_INIT_VAL 0xdeadbeef
static TLS_ATTR unsigned int static_tls_test = STATIC_TLS_INIT_VAL;

/* Test a constructor. */
class foo_t {
public:
    foo_t()
    {
        dr_fprintf(STDERR, "in foo_t::foo_t\n");
    }
    unsigned int val = STATIC_TLS_INIT_VAL;
};
/* VS2013 does not support 'thread_local'.
 * TODO i#2924: Remove these ifdefs (here and in .template) once we drop VS2013.
 */
#if !defined(WINDOWS) || _MSC_VER >= 1900
static thread_local foo_t foo;
/* XXX i#4034: Fix Linux crash with TLS vector.  For now we exclude. */
#    ifdef WINDOWS
static thread_local std::vector<unsigned int> static_tls_vector;
#    endif
#endif

static bool thread_init_called = false;
static bool insert_called = false;
static reg_id_t tls_raw_reg;
static uint tls_raw_base;

static void **
get_tls_addr(int slot_idx)
{
    byte *base = (byte *)dr_get_dr_segment_base(tls_raw_reg);
    byte *addr = (byte *)(base + tls_raw_base + slot_idx * sizeof(void *));
    return *((void ***)addr);
}

static void
check(void)
{
    if (get_tls_addr(0) != NULL || get_tls_addr(1) != NULL || get_tls_addr(2) != NULL ||
        get_tls_addr(3) != NULL)
        dr_fprintf(STDERR, "raw TLS should be NULL\n");
}

static dr_emit_flags_t
insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr, bool for_trace,
       bool translating, void *user_data)
{
    insert_called = true;
    check();

    if (drmgr_is_first_instr(drcontext, instr))
        dr_insert_clean_call(drcontext, bb, instr, (void *)check, false, 0);

    return DR_EMIT_DEFAULT;
}

static void
event_thread_init(void *drcontext)
{
    ASSERT(static_tls_test == STATIC_TLS_INIT_VAL);
    if (!thread_init_called)
        static_tls_test++;
#if !defined(WINDOWS) || _MSC_VER >= 1900
    ASSERT(foo.val == STATIC_TLS_INIT_VAL);
    if (!thread_init_called)
        foo.val--;
#    ifdef WINDOWS /* XXX i#: Fix Linux crash with TLS vector. */
    ASSERT(static_tls_vector.empty());
    static_tls_vector.push_back(foo.val);
#    endif
#endif
    thread_init_called = true;
    check();

    /* Just a sanity check that these functions operate.  We do not take the
     * time to set up racing threads or sthg.
     * This is a duplicate of the test in thread.dll.c, placed here b/c that test
     * is not yet enabled for AArchXX.
     */
    static int counter32;
    int count = dr_atomic_add32_return_sum(&counter32, 1);
    ASSERT(count > 0 && count <= counter32);
    int local_counter;
    dr_atomic_store32(&local_counter, 42);
    count = dr_atomic_load32(&local_counter);
    ASSERT(count == 42);
    ASSERT(local_counter == 42);
#ifdef X64
    static int64 counter64;
    int64 count64 = dr_atomic_add64_return_sum(&counter64, 1);
    ASSERT(count64 > 0 && count64 <= counter64);
    int64 local_counter64;
    dr_atomic_store64(&local_counter64, 42);
    count64 = dr_atomic_load64(&local_counter64);
    ASSERT(count64 == 42);
    ASSERT(local_counter64 == 42);
#endif
}

static void
event_thread_exit(void *drcontext)
{
    dr_fprintf(STDERR, "static TLS is 0x%08x\n", static_tls_test);
#if !defined(WINDOWS) || _MSC_VER >= 1900
    dr_fprintf(STDERR, "foo.val is 0x%08x\n", foo.val);
#    ifdef WINDOWS /* XXX i#: Fix Linux crash with TLS vector. */
    for (unsigned int val : static_tls_vector)
        dr_fprintf(STDERR, "vector holds 0x%08x\n", val);
#    endif
#endif
}

static void
event_exit(void)
{
    if (!drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_bb_insertion_event(insert))
        dr_fprintf(STDERR, "error\n");

    if (!insert_called || !thread_init_called)
        dr_fprintf(STDERR, "not called\n");

    dr_raw_tls_cfree(tls_raw_base, 4);

    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drmgr_init();

    dr_register_exit_event(event_exit);

    dr_raw_tls_calloc(&(tls_raw_reg), &(tls_raw_base), 4, 0);

    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_bb_instrumentation_event(NULL, insert, NULL))
        dr_fprintf(STDERR, "error\n");
}
