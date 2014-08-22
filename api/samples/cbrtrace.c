/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * cbrtrace.c
 *
 * Collects the conditional branch address, fall-through address,
 * target address, and taken information.
 * Writes that info into per-thread files named cbrtrace.<pid>.<tid>.log
 * in the client library directory.
 *
 * Illustrates how to use dr_insert_cbr_instrument_ex().
 */

#include "dr_api.h"
#include "utils.h"

client_id_t client_id;

/* Clean call for the cbr */
static void
at_cbr(app_pc inst_addr, app_pc targ_addr, app_pc fall_addr, int taken, void *bb_addr)
{
    void *drcontext = dr_get_current_drcontext();
    file_t log = (file_t)(ptr_uint_t)dr_get_tls_field(drcontext);
    dr_fprintf(log, ""PFX" ["PFX", "PFX", "PFX"] => "PFX"\n",
               bb_addr, inst_addr, fall_addr, targ_addr,
               taken == 0 ? fall_addr : targ_addr);
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *instr;
    for (instr  = instrlist_first_app(bb);
         instr != NULL;
         instr  = instr_get_next_app(instr)) {
        if (instr_is_cbr(instr)) {
            dr_insert_cbr_instrumentation_ex
                (drcontext, bb, instr, (void *)at_cbr,
                 OPND_CREATE_INTPTR(dr_fragment_app_pc(tag)));
        }
    }
    return DR_EMIT_DEFAULT;
}

static void
event_thread_init(void *drcontext)
{
    int len;
    file_t log;
    char name[MAXIMUM_PATH];
    len = dr_snprintf(name, BUFFER_SIZE_ELEMENTS(name),
                      "cbrtrace.%d.%d.log",
                      dr_get_process_id(), dr_get_thread_id(drcontext));
    DR_ASSERT(len > 0);
    NULL_TERMINATE_BUFFER(name);
    log = log_file_open(client_id, drcontext, NULL /* using client lib path */,
                        name, DR_FILE_WRITE_OVERWRITE);
    DR_ASSERT(log != INVALID_FILE);
    dr_set_tls_field(drcontext, (void *)(ptr_uint_t)log);
}

static void
event_thread_exit(void *drcontext)
{
    log_file_close((file_t)(ptr_uint_t)dr_get_tls_field(drcontext));
}

static void
event_exit(void)
{
    int len;
    char msg[MAXIMUM_PATH];
    len = dr_snprintf(msg, BUFFER_SIZE_ELEMENTS(msg), "Client 'cbrtrace' exiting");
    DR_ASSERT(len > 0);
    NULL_TERMINATE_BUFFER(msg);
    dr_log(NULL, LOG_ALL, 1, "%s", msg);
#ifdef SHOW_RESULTS
    DISPLAY_STRING(msg);
#endif
}

DR_EXPORT
void dr_init(client_id_t id)
{
    int len;
    char msg[MAXIMUM_PATH];
    len = dr_snprintf(msg, BUFFER_SIZE_ELEMENTS(msg), "Client 'cbrtrace' initializing");
    DR_ASSERT(len > 0);
    NULL_TERMINATE_BUFFER(msg);
    dr_log(NULL, LOG_ALL, 1, "%s", msg);
#ifdef SHOW_RESULTS
    DISPLAY_STRING(msg);
#endif
    client_id = id;
    dr_register_thread_init_event(event_thread_init);
    dr_register_thread_exit_event(event_thread_exit);
    dr_register_bb_event(event_basic_block);
    dr_register_exit_event(event_exit);
}
