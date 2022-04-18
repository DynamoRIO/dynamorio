/* **********************************************************
 * Copyright (c) 2012-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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
 * instrcalls.c
 *
 * Instruments direct calls, indirect calls, and returns in the target
 * application.  For each dynamic execution, the call target and other
 * key information is written to a log file.  Note that this log file
 * can become quite large, and this client incurs more overhead than
 * the other clients due to its log file.
 *
 * If the SHOW_SYMBOLS define is on, this sample uses the drsyms
 * DynamoRIO Extension to obtain symbol information from raw
 * addresses.  This requires a relatively recent copy of dbghelp.dll
 * (6.0+), which is not available in the system directory by default
 * on Windows 2000.  To use this sample with SHOW_SYMBOLS on Windows
 * 2000, download the Debugging Tools for Windows package from
 * http://www.microsoft.com/whdc/devtools/debugging/default.mspx and
 * place dbghelp.dll in the same directory as either drsyms.dll or as
 * this sample client library.
 */

#include "dr_api.h"
#include "drmgr.h"
#ifdef SHOW_SYMBOLS
#    include "drsyms.h"
#endif
#include "utils.h"

static void
event_exit(void);
static void
event_thread_init(void *drcontext);
static void
event_thread_exit(void *drcontext);
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data);
static int tls_idx;

static client_id_t my_id;

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'instrcalls'",
                       "http://dynamorio.org/issues");
    drmgr_init();
    my_id = id;
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'instrcalls' initializing\n");
    /* also give notification to stderr */
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client instrcalls is running\n");
    }
#endif
    dr_register_exit_event(event_exit);
    drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL);
    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);
#ifdef SHOW_SYMBOLS
    if (drsym_init(0) != DRSYM_SUCCESS) {
        dr_log(NULL, DR_LOG_ALL, 1, "WARNING: unable to initialize symbol translation\n");
    }
#endif
    tls_idx = drmgr_register_tls_field();
    DR_ASSERT(tls_idx > -1);
}

static void
event_exit(void)
{
#ifdef SHOW_SYMBOLS
    if (drsym_exit() != DRSYM_SUCCESS) {
        dr_log(NULL, DR_LOG_ALL, 1, "WARNING: error cleaning up symbol library\n");
    }
#endif
    drmgr_unregister_tls_field(tls_idx);
    drmgr_exit();
}

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#else
#    define IF_WINDOWS(x) /* nothing */
#endif

static void
event_thread_init(void *drcontext)
{
    file_t f;
    /* We're going to dump our data to a per-thread file.
     * On Windows we need an absolute path so we place it in
     * the same directory as our library. We could also pass
     * in a path as a client argument.
     */
    f = log_file_open(my_id, drcontext, NULL /* client lib path */, "instrcalls",
#ifndef WINDOWS
                      DR_FILE_CLOSE_ON_FORK |
#endif
                          DR_FILE_ALLOW_LARGE);
    DR_ASSERT(f != INVALID_FILE);

    /* store it in the slot provided in the drcontext */
    drmgr_set_tls_field(drcontext, tls_idx, (void *)(ptr_uint_t)f);
}

static void
event_thread_exit(void *drcontext)
{
    log_file_close((file_t)(ptr_uint_t)drmgr_get_tls_field(drcontext, tls_idx));
}

#ifdef SHOW_SYMBOLS
#    define MAX_SYM_RESULT 256
static void
print_address(file_t f, app_pc addr, const char *prefix)
{
    drsym_error_t symres;
    drsym_info_t sym;
    char name[MAX_SYM_RESULT];
    char file[MAXIMUM_PATH];
    module_data_t *data;
    data = dr_lookup_module(addr);
    if (data == NULL) {
        dr_fprintf(f, "%s " PFX " ? ??:0\n", prefix, addr);
        return;
    }
    sym.struct_size = sizeof(sym);
    sym.name = name;
    sym.name_size = MAX_SYM_RESULT;
    sym.file = file;
    sym.file_size = MAXIMUM_PATH;
    symres = drsym_lookup_address(data->full_path, addr - data->start, &sym,
                                  DRSYM_DEFAULT_FLAGS);
    if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
        const char *modname = dr_module_preferred_name(data);
        if (modname == NULL)
            modname = "<noname>";
        dr_fprintf(f, "%s " PFX " %s!%s+" PIFX, prefix, addr, modname, sym.name,
                   addr - data->start - sym.start_offs);
        if (symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
            dr_fprintf(f, " ??:0\n");
        } else {
            dr_fprintf(f, " %s:%" UINT64_FORMAT_CODE "+" PIFX "\n", sym.file, sym.line,
                       sym.line_offs);
        }
    } else
        dr_fprintf(f, "%s " PFX " ? ??:0\n", prefix, addr);
    dr_free_module_data(data);
}
#endif

static void
at_call(app_pc instr_addr, app_pc target_addr)
{
    file_t f =
        (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
    dr_mcontext_t mc = { sizeof(mc), DR_MC_CONTROL /*only need xsp*/ };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);
#ifdef SHOW_SYMBOLS
    print_address(f, instr_addr, "CALL @ ");
    print_address(f, target_addr, "\t to ");
    dr_fprintf(f, "\tTOS is " PFX "\n", mc.xsp);
#else
    dr_fprintf(f, "CALL @ " PFX " to " PFX ", TOS is " PFX "\n", instr_addr, target_addr,
               mc.xsp);
#endif
}

static void
at_call_ind(app_pc instr_addr, app_pc target_addr)
{
    file_t f =
        (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
#ifdef SHOW_SYMBOLS
    print_address(f, instr_addr, "CALL INDIRECT @ ");
    print_address(f, target_addr, "\t to ");
#else
    dr_fprintf(f, "CALL INDIRECT @ " PFX " to " PFX "\n", instr_addr, target_addr);
#endif
}

static void
at_return(app_pc instr_addr, app_pc target_addr)
{
    file_t f =
        (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
#ifdef SHOW_SYMBOLS
    print_address(f, instr_addr, "RETURN @ ");
    print_address(f, target_addr, "\t to ");
#else
    dr_fprintf(f, "RETURN @ " PFX " to " PFX "\n", instr_addr, target_addr);
#endif
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
#ifdef VERBOSE
    if (drmgr_is_first_instr(drcontext, instr)) {
        dr_printf("in dr_basic_block(tag=" PFX ")\n", tag);
#    if VERBOSE_VERBOSE
        instrlist_disassemble(drcontext, tag, bb, STDOUT);
#    endif
    }
#endif
    /* instrument calls and returns -- ignore far calls/rets */
    if (instr_is_call_direct(instr)) {
        dr_insert_call_instrumentation(drcontext, bb, instr, (app_pc)at_call);
    } else if (instr_is_call_indirect(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc)at_call_ind,
                                      SPILL_SLOT_1);
    } else if (instr_is_return(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc)at_return,
                                      SPILL_SLOT_1);
    }
    return DR_EMIT_DEFAULT;
}
