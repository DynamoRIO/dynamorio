/* **********************************************************
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

/* Tests dr_fragment_app_pc() and some of the module routines (dr_lookup_module(),
 * dr_lookup_module_by_name(), and dr_get_proc_address()). Also checks that the client
 * isn't seeing code from weird locations. */

static bool
is_in_known_module(app_pc pc, bool *found_section, IMAGE_SECTION_HEADER *section)
{
    bool found = false;
    module_data_t *data = dr_lookup_module(pc);
    if (data != NULL) {
        found = true;
        *found_section = dr_lookup_module_section(data->handle, pc, section);
    }
    dr_free_module_data(data);
    return found;
}

/* FIXME - is global because of compiler switching issues, can't find security_cookie
 * with new compiler which is triggered by having this as a local to bb_event.
 * FIXME - test target notepad.exe is single threaded so access is ok for now. */
IMAGE_SECTION_HEADER section;

app_pc exit_proc_addr;

static dr_emit_flags_t
bb_event(void *drcontext, app_pc tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    app_pc bb_addr, instr_addr;
    bool found_section;

    bb_addr = dr_fragment_app_pc(tag);

    /* vsyscall: some versions of windows jump to 0x7ffe0300 to
     * execute a syscall; this address is not contained in any module.
     */
    if ((ptr_uint_t)bb_addr == 0x7ffe0300 || (ptr_uint_t)bb_addr == 0x7ffe0302)
        return DR_EMIT_DEFAULT;

    if (!is_in_known_module(bb_addr, &found_section, &section)) {
        dr_fprintf(STDERR, "ERROR: BB addr " PFX " in unknown module\n", bb_addr);
    }
    if (!found_section) {
        dr_fprintf(STDERR, "ERROR: BB addr " PFX " isn't within a module section\n",
                   bb_addr);
    }
    if ((section.Characteristics & IMAGE_SCN_CNT_CODE) == 0) {
        dr_fprintf(STDERR, "ERROR: BB addr " PFX " isn't within a code section\n",
                   bb_addr);
    }

    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);

        instr_addr = instr_get_app_pc(instr);
        if (!is_in_known_module(instr_addr, &found_section, &section)) {
            dr_fprintf(STDERR, "ERROR: instr addr " PFX " in unknown module\n",
                       instr_addr);
        }
        if (!found_section) {
            dr_fprintf(STDERR,
                       "ERROR: instr addr " PFX " isn't within a module section\n",
                       instr_addr);
        }
        if ((section.Characteristics & IMAGE_SCN_CNT_CODE) == 0) {
            dr_fprintf(STDERR, "ERROR: instr addr " PFX " isn't within a code section\n",
                       instr_addr);
        }
        if (instr_addr == exit_proc_addr) {
            dr_fprintf(STDERR, "Hit kernel32!ExitProcess\n");
            exit_proc_addr = NULL;
        }
    }
    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    module_data_t *data;
    dr_fprintf(STDERR, "thank you for testing the client interface\n");
    dr_register_bb_event(bb_event);
    data = dr_lookup_module_by_name("kernel32.dll");
    if (data != NULL) {
        exit_proc_addr = (void *)dr_get_proc_address(data->handle, "ExitProcess");
        if (exit_proc_addr == NULL)
            dr_fprintf(STDERR, "ERROR: unable to find kernel32!ExitProcess\n");
        dr_free_module_data(data);
    } else {
        dr_fprintf(STDERR, "ERROR: unable to find ntdll.dll\n");
    }
}
