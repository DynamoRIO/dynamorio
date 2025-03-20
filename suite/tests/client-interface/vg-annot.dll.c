/* **********************************************************
 * Copyright (c) 2014-2025 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"
#include <string.h>

/* Stats to check on test exit */
typedef struct _test_stats_t {
    uint num_bytes_made_defined_if_addressable;
    uint num_define_memory_if_addressable_requests;
    uint num_bbs_truncated;
    uint num_instructions_truncated;
    uint num_bytes_made_undefined;
    uint num_bytes_made_defined;
    uint num_bytes_checked_addressable;
    uint num_bytes_checked_defined;
    uint num_malloclike_requests;
    uint num_freelike_requests;
} test_stats_t;

static test_stats_t test_stats;
static bool bb_truncation_mode;
static uint bb_truncation_length;

static ptr_uint_t
handle_running_on_valgrind(dr_vg_client_request_t *request)
{
    return 1;
}

/* Trivial handler for DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE */
static ptr_uint_t
handle_make_mem_defined_if_addressable(dr_vg_client_request_t *request)
{
    dr_printf("Make %d bytes defined if addressable.\n", request->args[1]);

    test_stats.num_bytes_made_defined_if_addressable += request->args[1];
    test_stats.num_define_memory_if_addressable_requests++;

    return 0;
}

static ptr_uint_t
handle_make_mem_undefined(dr_vg_client_request_t *request)
{
    dr_printf("Make %d bytes undefined.\n", request->args[1]);
    test_stats.num_bytes_made_undefined += request->args[1];
    return 0;
}

static ptr_uint_t
handle_make_mem_defined(dr_vg_client_request_t *request)
{
    dr_printf("Make %d bytes defined.\n", request->args[1]);
    test_stats.num_bytes_made_defined += request->args[1];
    return 0;
}

static ptr_uint_t
handle_check_mem_is_addressable(dr_vg_client_request_t *request)
{
    dr_printf("Checking whether %d bytes are addressable.\n", request->args[1]);
    test_stats.num_bytes_checked_addressable += request->args[1];
    return 0;
}

static ptr_uint_t
handle_check_mem_is_defined(dr_vg_client_request_t *request)
{
    dr_printf("Checking whether %d bytes are defined.\n", request->args[1]);
    test_stats.num_bytes_checked_defined += request->args[1];
    return 0;
}

static ptr_uint_t
handle_malloclike_block(dr_vg_client_request_t *request)
{
    /* Parameters are: addr, size, redzone_size, is_zeroed. */
    dr_printf("Malloclike %d bytes.\n", request->args[1]);
    test_stats.num_malloclike_requests++;
    return 0;
}

static ptr_uint_t
handle_freelike_block(dr_vg_client_request_t *request)
{
    /* Parameters are: addr, redzone_size. */
    dr_printf("Freelike.\n");
    test_stats.num_freelike_requests++;
    return 0;
}

/* This trivial bb event enables full decoding for all app instructions */
dr_emit_flags_t
empty_bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
               bool translating)
{
    return DR_EMIT_DEFAULT;
}

/* Truncates every basic block at 2 app instructions (or less), to test for annotation
 * issues caused by client instrumentation.
 */
dr_emit_flags_t
bb_event_truncate(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating)
{
    bool truncated = false;
    uint app_instruction_count = 0;
    instr_t *next, *instr = instrlist_first(bb);

    while (instr != NULL) {
        next = instr_get_next(instr);
        if (!instr_is_meta(instr)) {
            if (app_instruction_count == bb_truncation_length) {
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
                test_stats.num_instructions_truncated++;
                truncated = true;
            } else {
                app_instruction_count++;
            }
        }
        instr = next;
    }

    if (truncated)
        test_stats.num_bbs_truncated++;

    return DR_EMIT_DEFAULT;
}

void
exit_event(void)
{
    if (bb_truncation_mode)
        ASSERT(test_stats.num_instructions_truncated > 0);

    dr_printf(
        "Received %d 'define memory if addressable' requests for a total of %d bytes.\n",
        test_stats.num_define_memory_if_addressable_requests,
        test_stats.num_bytes_made_defined_if_addressable);
    dr_printf("Received requests for %d bytes bytes made undefined.\n",
              test_stats.num_bytes_made_undefined);
    dr_printf("Received requests for %d bytes bytes made defined.\n",
              test_stats.num_bytes_made_defined);
    dr_printf("Received requests for %d bytes bytes checked addressable.\n",
              test_stats.num_bytes_checked_addressable);
    dr_printf("Received requests for %d bytes bytes checked defined.\n",
              test_stats.num_bytes_checked_defined);
    dr_printf("Received %d malloclike requests.\n", test_stats.num_malloclike_requests);
    dr_printf("Received %d freelike requests.\n", test_stats.num_freelike_requests);
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    memset(&test_stats, 0, sizeof(test_stats_t));

    /* This client supports 3 modes via command-line options:
     *   <default>: fast decoding (by not registering a bb event)
     *   full-decode: registers a bb event to enable full decoding of app instructions
     *   truncate: registers a bb event that truncates basic blocks to max length 2
     */
    /* XXX: should use droption */
    if (argc > 1 && strcmp(argv[1], "full-decode") == 0) {
        dr_printf("Init vg-annot with full decoding.\n");
        dr_register_bb_event(empty_bb_event);
    } else if (argc > 1 && strlen(argv[1]) >= 8 && strncmp(argv[1], "truncate", 8) == 0) {
        bb_truncation_length = (argv[1][9] - '0'); /* format is "truncate@n" (0<n<10) */
        ASSERT(bb_truncation_length < 10 && bb_truncation_length > 0);
        dr_printf("Init vg-annot with bb truncation.\n");
        dr_register_bb_event(bb_event_truncate);
        bb_truncation_mode = true;
    } else {
        dr_printf("Init vg-annot with fast decoding.\n");
    }

    dr_register_exit_event(exit_event);

    dr_annotation_register_valgrind(DR_VG_ID__RUNNING_ON_VALGRIND,
                                    handle_running_on_valgrind);
    dr_annotation_register_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                    handle_make_mem_defined_if_addressable);
    dr_annotation_register_valgrind(DR_VG_ID__MAKE_MEM_UNDEFINED,
                                    handle_make_mem_undefined);
    dr_annotation_register_valgrind(DR_VG_ID__MAKE_MEM_DEFINED, handle_make_mem_defined);
    dr_annotation_register_valgrind(DR_VG_ID__CHECK_MEM_IS_ADDRESSABLE,
                                    handle_check_mem_is_addressable);
    dr_annotation_register_valgrind(DR_VG_ID__CHECK_MEM_IS_DEFINED,
                                    handle_check_mem_is_defined);
    dr_annotation_register_valgrind(DR_VG_ID__MALLOCLIKE_BLOCK, handle_malloclike_block);
    dr_annotation_register_valgrind(DR_VG_ID__FREELIKE_BLOCK, handle_freelike_block);
}
