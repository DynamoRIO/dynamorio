/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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

/* Intercepts module transitions for native execution for ELF modules.
 */

#include "../globals.h"
#include "../native_exec.h"
#include "module.h"
#include "instr.h"
#include "decode.h"
#include "disassemble.h"

/* According to the SysV amd64 psABI docs[1], there are three reserved entries
 * in the PLTGOT:
 * 1. offset to .dynamic section
 * 2. available for loader data, used for link map
 * 3. pointer to resolution stub, used for _dl_runtime_resolve
 *
 * 1: http://refspecs.linuxfoundation.org/elf/x86_64-abi-0.95.pdf
 *
 * We want to replace 3, _dl_runtime_resolve, with a stub in x86.asm.  Here is
 * what the PLT generally looks like, as specified by Figure 5.2 of the ABI
 * docs:
 *
 * .PLT0:   pushq GOT+8(%rip) # GOT[1]
 *          jmp *GOT+16(%rip) # GOT[2]  # _dl_runtime_resolve here
 *          nop ; nop ; nop ; nop
 *
 * .PLT1:   jmp *name1@GOTPCREL(%rip) # 16 bytes from .PLT0
 *          pushq $index1
 *          jmp .PLT0
 * .PLT2:   jmp *name2@GOTPCREL(%rip) # 16 bytes from .PLT1
 *          pushq $index2
 *          jmp .PLT0
 * .PLT3:   ...
 *
 * Testing shows that this is the same on ia32, but I wasn't able to find
 * support for that in the docs.
 */
enum { DL_RUNTIME_RESOLVE_IDX = 2 };

/* The loader's _dl_fixup.  For ia32 it uses regparms. */
typedef void *(*fixup_fn_t)(void *link_map, uint dynamic_index)
    IF_NOT_X64(__attribute__((regparm (3), stdcall, unused)));

fixup_fn_t app_dl_fixup;

/* Finds the call to _dl_fixup in _dl_runtime_resolve from ld.so.  _dl_fixup is
 * not exported, but we need to call it.  We assume that _dl_runtime_resolve is
 * straightline code until the call to _dl_fixup.
 */
static void
find_dl_fixup(dcontext_t *dcontext, app_pc resolver)
{
    instr_t instr;
    int max_decodes = 30;
    int i = 0;
    app_pc pc = resolver;

    LOG(THREAD, 5, LOG_LOADER, "%s: scanning for _dl_fixup call:\n",
        __FUNCTION__);
    instr_init(dcontext, &instr);
    while (pc != NULL && i < max_decodes) {
        DOLOG(5, LOG_LOADER, { disassemble(dcontext, pc, THREAD); });
        pc = decode(dcontext, pc, &instr);
        if (instr_get_opcode(&instr) == OP_call) {
            opnd_t tgt = instr_get_target(&instr);
            app_dl_fixup = (fixup_fn_t) opnd_get_pc(tgt);
            LOG(THREAD, 1, LOG_LOADER,
                "%s: found _dl_fixup call at "PFX", _dl_fixup is "PFX":\n",
                __FUNCTION__, pc, app_dl_fixup);
            break;
        } else if (instr_is_cti(&instr)) {
            break;
        }
        instr_reset(dcontext, &instr);
    }
    instr_free(dcontext, &instr);
}

static void
replace_module_resolver(module_area_t *ma, app_pc *pltgot)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    app_pc resolver;
    ASSERT_CURIOSITY(pltgot != NULL && "unable to locate DT_PLTGOT");
    if (pltgot == NULL)
        return;

    resolver = pltgot[DL_RUNTIME_RESOLVE_IDX];
    ASSERT_CURIOSITY(resolver != NULL && "loader will overwrite our stub");
    if (resolver != NULL && app_dl_fixup == NULL) {
        /* _dl_fixup is not exported, so we have to go find it. */
        find_dl_fixup(dcontext, resolver);
        ASSERT_CURIOSITY(app_dl_fixup != NULL && "failed to find _dl_fixup");
    }

    if (app_dl_fixup != NULL) {
        LOG(THREAD, LOG_LOADER, 3,
            "%s: replacing _dl_runtime_resolve "PFX" with "PFX"\n",
            __FUNCTION__, resolver, _dynamorio_runtime_resolve);
        pltgot[DL_RUNTIME_RESOLVE_IDX] = (app_pc) _dynamorio_runtime_resolve;
    }
}

/* Hooks all module transitions through the PLT.  If we are not at_map, then we
 * assume the module has been relocated.
 */
void
module_hook_transitions(module_area_t *ma, bool at_map)
{
    os_privmod_data_t opd;
    app_pc relro_base;
    size_t relro_size;
    bool got_unprotected = false;

    /* FIXME: We can't handle un-relocated modules yet. */
    if (at_map)
        return;

    memset(&opd, 0, sizeof(opd));
    module_get_os_privmod_data(ma->start, ma->end - ma->start,
                               !at_map/*relocated*/, &opd);

    /* If we are !at_map, then we assume the loader has already relocated the
     * module and applied protections for PT_GNU_RELRO.  _dl_runtime_resolve is
     * typically inside the relro region, so we must unprotect it.
     */
    if (!at_map && module_get_relro(ma->start, &relro_base, &relro_size)) {
        os_set_protection(relro_base, relro_size, MEMPROT_READ|MEMPROT_WRITE);
        got_unprotected = true;
    }

    replace_module_resolver(ma, (app_pc *) opd.pltgot);

    if (got_unprotected) {
        /* XXX: This may not be symmetric, but we trust PT_GNU_RELRO for now. */
        os_set_protection(relro_base, relro_size, MEMPROT_READ);
    }
}

/* Our replacement for _dl_fixup.
 * FIXME i#978: currently this exists only for logging.  Eventually it will
 * replace the relocation with our own stub that enters the cache.
 */
void *
dynamorio_dl_fixup(void *link_map, uint dynamic_index)
{
    app_pc res;
    ASSERT(app_dl_fixup != NULL);
    /* i#978: depending on the needs of the client, they may want to run the
     * loader natively or through the code cache.  We might want to provide that
     * support by entering the fcache for this call here.
     */
    res = app_dl_fixup(link_map, dynamic_index);
    DOLOG(4, LOG_LOADER, {
        dcontext_t *dcontext = get_thread_private_dcontext();
        LOG(THREAD, LOG_LOADER, 4,
            "%s: resolved dynamic index %d to "PFX"\n",
            __FUNCTION__, dynamic_index, res);
    });
    return res;
}
