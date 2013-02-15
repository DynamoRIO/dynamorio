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
#include "instr_create.h"
#include "decode.h"
#include "disassemble.h"

#include <link.h>  /* for struct link_map */

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
typedef void *(*fixup_fn_t)(struct link_map *l_map, uint dynamic_index)
    IF_NOT_X64(__attribute__((regparm (3), stdcall, unused)));

app_pc app_dl_runtime_resolve;
fixup_fn_t app_dl_fixup;

enum { MAX_STUB_SIZE = 16 };

static byte plt_stub_template[MAX_STUB_SIZE];
static uint plt_stub_immed_offset;
static uint plt_stub_jmp_tgt_offset;
static size_t plt_stub_size;
static app_pc stub_heap;

static bool
module_contains_pc(module_area_t *ma, app_pc pc)
{
    return (pc >= ma->start && pc < ma->end);
}

/* Finds the call to _dl_fixup in _dl_runtime_resolve from ld.so.  _dl_fixup is
 * not exported, but we need to call it.  We assume that _dl_runtime_resolve is
 * straightline code until the call to _dl_fixup.
 */
static app_pc
find_dl_fixup(dcontext_t *dcontext, app_pc resolver)
{
    instr_t instr;
    int max_decodes = 30;
    int i = 0;
    app_pc pc = resolver;
    app_pc fixup = NULL;

    LOG(THREAD, 5, LOG_LOADER, "%s: scanning for _dl_fixup call:\n",
        __FUNCTION__);
    instr_init(dcontext, &instr);
    while (pc != NULL && i < max_decodes) {
        DOLOG(5, LOG_LOADER, { disassemble(dcontext, pc, THREAD); });
        pc = decode(dcontext, pc, &instr);
        if (instr_get_opcode(&instr) == OP_call) {
            opnd_t tgt = instr_get_target(&instr);
            fixup = opnd_get_pc(tgt);
            LOG(THREAD, 1, LOG_LOADER,
                "%s: found _dl_fixup call at "PFX", _dl_fixup is "PFX":\n",
                __FUNCTION__, pc, fixup);
            break;
        } else if (instr_is_cti(&instr)) {
            break;
        }
        instr_reset(dcontext, &instr);
    }
    instr_free(dcontext, &instr);
    return fixup;
}

/* Creates a template stub copied repeatedly for each stub we need to create.
 */
static void
initialize_plt_stub_template(void)
{
    dcontext_t *dc = GLOBAL_DCONTEXT;
    instrlist_t *ilist = instrlist_create(dc);
    app_pc code_end = plt_stub_template + BUFFER_SIZE_BYTES(plt_stub_template);
    app_pc next_pc;
    uint mov_len, jmp_len;

    ASSERT(plt_stub_size == 0 && "stub template should only be init once");
    /* %r11 is scratch on x64 and the PLT resolver uses it, so we do too.  For
     * ia32, there are scratch regs, but the loader doesn't use them.  Presumably
     * it doesn't want to break special calling conventions, so we follow suit
     * and push onto the stack.
     */
#ifdef X64
    instrlist_append(ilist, INSTR_CREATE_mov_imm
                     (dc, opnd_create_reg(DR_REG_R11), OPND_CREATE_INTPTR(0)));
#else
    instrlist_append(ilist, INSTR_CREATE_push_imm(dc, OPND_CREATE_INTPTR(0)));
#endif
    instrlist_append(ilist, INSTR_CREATE_jmp (dc, opnd_create_pc(0)));
    next_pc = instrlist_encode_to_copy(dc, ilist, plt_stub_template, NULL,
                                       code_end, false);
    plt_stub_size = next_pc - plt_stub_template;

    /* We need to get the offsets of the operands.  We assume the operands are
     * encoded as the last part of the instruction.
     */
    mov_len = instr_length(dc, instrlist_first(ilist));
    jmp_len = instr_length(dc, instrlist_last(ilist));
    plt_stub_immed_offset = mov_len - sizeof(void*);
    plt_stub_jmp_tgt_offset = mov_len + jmp_len - sizeof(uint);
    DOLOG(4, LOG_LOADER, {
        LOG(THREAD_GET, 4, LOG_LOADER, "plt_stub_template code:\n");
        instrlist_disassemble(dc, NULL, ilist, THREAD_GET);
    });
    instrlist_clear_and_destroy(dc, ilist);
}

/* Replaces the resolver with our own or the app's original resolver.
 * XXX: We assume there is only one loader in the app and hence only one
 * resolver, but conceivably there could be two separate loaders.
 */
static void
replace_module_resolver(module_area_t *ma, app_pc *pltgot, bool to_dr)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    app_pc resolver;
    bool already_hooked = false;
    ASSERT_CURIOSITY(pltgot != NULL && "unable to locate DT_PLTGOT");
    if (pltgot == NULL)
        return;
    resolver = pltgot[DL_RUNTIME_RESOLVE_IDX];

    /* If the module is eagerly bound due to LD_BIND_NOW, RTLD_NOW, or
     * DT_BIND_NOW, then the resolver will be NULL and we don't need to do any
     * lazy resolution.
     */
    if (resolver == NULL)
        return;

    /* Make this somewhat idempotent.  We shouldn't re-hook if we're already
     * hooked, and we shouldn't remove hooks if we haven't hooked already.
     */
    if (resolver == (app_pc) _dynamorio_runtime_resolve)
        already_hooked = true;
    if (to_dr && already_hooked)
        return;
    if (!to_dr && !already_hooked)
        return;

    if (!to_dr) {
        ASSERT(app_dl_runtime_resolve != NULL);
        pltgot[DL_RUNTIME_RESOLVE_IDX] = app_dl_runtime_resolve;
        return;
    }

    if (app_dl_runtime_resolve == NULL) {
        app_dl_runtime_resolve = resolver;
    } else {
        ASSERT(resolver == app_dl_runtime_resolve &&
               "app has multiple resolvers: multiple loaders?");
    }
    if (app_dl_fixup == NULL) {
        /* _dl_fixup is not exported, so we have to go find it. */
        app_dl_fixup = (fixup_fn_t) find_dl_fixup(dcontext, resolver);
        ASSERT_CURIOSITY(app_dl_fixup != NULL && "failed to find _dl_fixup");
    } else {
        ASSERT((app_pc) app_dl_fixup == find_dl_fixup(dcontext, resolver) &&
               "_dl_fixup should be the same for all modules");
    }

    if (app_dl_fixup != NULL) {
        LOG(THREAD, LOG_LOADER, 3,
            "%s: replacing _dl_runtime_resolve "PFX" with "PFX"\n",
            __FUNCTION__, resolver, _dynamorio_runtime_resolve);
        pltgot[DL_RUNTIME_RESOLVE_IDX] = (app_pc) _dynamorio_runtime_resolve;
    }
}

/* Allocates and initializes a stub of code for taking control after a PLT call.
 */
static app_pc
create_plt_stub(app_pc plt_target)
{
    app_pc stub_pc = special_heap_alloc(stub_heap);
    app_pc *tgt_immed;
    app_pc jmp_tgt;

    memcpy(stub_pc, plt_stub_template, plt_stub_size);
    tgt_immed = (app_pc *) (stub_pc + plt_stub_immed_offset);
    jmp_tgt = stub_pc + plt_stub_jmp_tgt_offset;
    *tgt_immed = plt_target;
    insert_relative_target(jmp_tgt, (app_pc) native_plt_call,
                           false/*!hotpatch*/);
    return stub_pc;
}

/* Deletes a PLT stub and returns the original target of the stub.
 */
static app_pc
destroy_plt_stub(app_pc stub_pc)
{
    app_pc orig_tgt;
    app_pc *tgt_immed = (app_pc *) (stub_pc + plt_stub_immed_offset);
    orig_tgt = *tgt_immed;
    special_heap_free(stub_heap, stub_pc);
    return orig_tgt;
}

static size_t
plt_reloc_entry_size(ELF_WORD pltrel)
{
    switch (pltrel) {
    case DT_REL:
        return sizeof(ELF_REL_TYPE);
    case DT_RELA:
        return sizeof(ELF_RELA_TYPE);
    default:
        ASSERT(false);
    }
    return sizeof(ELF_REL_TYPE);
}

static bool
is_special_stub(app_pc stub_pc)
{
    special_heap_iterator_t shi;
    bool found = false;
    /* XXX: this acquires a lock in a nested loop. */
    special_heap_iterator_start(stub_heap, &shi);
    while (special_heap_iterator_hasnext(&shi)) {
        app_pc start, end;
        special_heap_iterator_next(&shi, &start, &end);
        if (stub_pc >= start && stub_pc < end) {
            found = true;
            break;
        }
    }
    special_heap_iterator_stop(&shi);
    return found;
}

/* Iterates all PLT relocations and either inserts or removes our own PLT
 * takeover stubs.
 */
static void
update_plt_relocations(module_area_t *ma, os_privmod_data_t *opd, bool add_hooks)
{
    app_pc jmprel;
    app_pc jmprelend = opd->jmprel + opd->pltrelsz;
    size_t entry_size = plt_reloc_entry_size(opd->pltrel);
    for (jmprel = opd->jmprel; jmprel != jmprelend; jmprel += entry_size) {
        ELF_REL_TYPE *rel = (ELF_REL_TYPE *) jmprel;
        app_pc *r_addr;
        app_pc gotval;
        r_addr = (app_pc *) (opd->load_delta + rel->r_offset);
        ASSERT(module_contains_pc(ma, (app_pc)r_addr));
        gotval = *r_addr;
        if (add_hooks) {
            /* If the PLT target is inside the current module, then it is either
             * a lazy resolution stub or was resolved to the current module.
             * Either way we ignore it.
             */
            if (!module_contains_pc(ma, gotval)) {
                LOG(THREAD_GET, LOG_LOADER, 4,
                    "%s: hooking cross-module PLT entry to "PFX"\n",
                    __FUNCTION__, gotval);
                *r_addr = create_plt_stub(gotval);
            }
        } else {
            /* XXX: pull the ranges out of the heap up front to avoid lock
             * acquisitions.
             */
            if (is_special_stub(gotval)) {
                *r_addr = destroy_plt_stub(gotval);
            }
        }
    }
}

void
module_change_hooks(module_area_t *ma, bool add_hooks, bool at_map)
{
    os_privmod_data_t opd;
    app_pc relro_base;
    size_t relro_size;
    bool got_unprotected = false;
    app_pc *pltgot;

    /* FIXME: We can't handle un-relocated modules yet. */
    ASSERT_CURIOSITY(!at_map && "hooking at map NYI");
    if (add_hooks && at_map)
        return;

    memset(&opd, 0, sizeof(opd));
    module_get_os_privmod_data(ma->start, ma->end - ma->start,
                               !at_map/*relocated*/, &opd);
    pltgot = (app_pc *) opd.pltgot;

    /* We can't hook modules that don't have a pltgot. */
    if (pltgot == NULL)
        return;

    /* If we are !at_map, then we assume the loader has already relocated the
     * module and applied protections for PT_GNU_RELRO.  _dl_runtime_resolve is
     * typically inside the relro region, so we must unprotect it.
     */
    if (!at_map && module_get_relro(ma->start, &relro_base, &relro_size)) {
        os_set_protection(relro_base, relro_size, MEMPROT_READ|MEMPROT_WRITE);
        got_unprotected = true;
    }

    /* Insert or remove our lazy dynamic resolver. */
    replace_module_resolver(ma, pltgot, add_hooks/*to_dr*/);
    /* Insert or remove our PLT stubs. */
    update_plt_relocations(ma, &opd, add_hooks);

    if (got_unprotected) {
        /* XXX: This may not be symmetric, but we trust PT_GNU_RELRO for now. */
        os_set_protection(relro_base, relro_size, MEMPROT_READ);
    }
}

/* Hooks all module transitions through the PLT.  If we are not at_map, then we
 * assume the module has been relocated.
 */
void
native_module_hook(module_area_t *ma, bool at_map)
{
    if (DYNAMO_OPTION(native_exec_retakeover))
        module_change_hooks(ma, true/*add*/, at_map);
}

void
native_module_unhook(module_area_t *ma)
{
    if (DYNAMO_OPTION(native_exec_retakeover))
        module_change_hooks(ma, false/*remove*/, false/*!at_map*/);
}

static ELF_REL_TYPE *
find_plt_reloc(struct link_map *l_map, uint reloc_arg)
{
    ELF_DYNAMIC_ENTRY_TYPE *dyn = l_map->l_ld;
    app_pc jmprel = NULL;
    size_t relsz;
    IF_X64(uint pltrel = 0;)

    /* XXX: We can avoid the scan if we rely on internal details of link_map,
     * which keeps a mapping of DT_TAG to .dynamic index.
     */
    while (dyn->d_tag != DT_NULL) {
        switch (dyn->d_tag) {
        case DT_JMPREL:
            jmprel = (app_pc) dyn->d_un.d_ptr; /* relocated */
            break;
#ifdef X64
        case DT_PLTREL:
            pltrel = dyn->d_un.d_val;
            break;
#endif
        }
        dyn++;
    }

#ifdef X64
    relsz = plt_reloc_entry_size(pltrel);
#else
    /* reloc_arg is an index on x64 and an offset on ia32. */
    relsz = 1;
#endif
    return (ELF_REL_TYPE *) (jmprel + relsz * reloc_arg);
}

/* Our replacement for _dl_fixup.
 */
void *
dynamorio_dl_fixup(struct link_map *l_map, uint reloc_arg)
{
    app_pc res;
    ELF_REL_TYPE *rel;
    app_pc *r_addr;

    ASSERT(app_dl_fixup != NULL);
    /* i#978: depending on the needs of the client, they may want to run the
     * loader natively or through the code cache.  We might want to provide that
     * support by entering the fcache for this call here.
     */
    res = app_dl_fixup(l_map, reloc_arg);
    DOLOG(4, LOG_LOADER, {
        dcontext_t *dcontext = get_thread_private_dcontext();
        LOG(THREAD, LOG_LOADER, 4,
            "%s: resolved reloc index %d to "PFX"\n",
            __FUNCTION__, reloc_arg, res);
    });
    app_pc stub = create_plt_stub(res);
    rel = find_plt_reloc(l_map, reloc_arg);
    ASSERT(rel != NULL);  /* It has to be there if we're doing fixups. */
    r_addr = (app_pc *) (l_map->l_addr + rel->r_offset);
    *r_addr = stub;
    return stub;
}

void
native_module_init(void)
{
    if (!DYNAMO_OPTION(native_exec_retakeover))
        return;
    ASSERT(stub_heap == NULL && "init should only happen once");
    initialize_plt_stub_template();
    stub_heap = special_heap_init(plt_stub_size, true/*locked*/,
                                  true/*executable*/, true/*persistent*/);
}

void
native_module_exit(void)
{
    /* Make sure we can scan all modules on native_exec_areas and unhook them.
     * If this fails, we get special heap leak asserts.
     */
    module_iterator_t *mi;
    module_area_t *ma;
    mi = module_iterator_start();
    while (module_iterator_hasnext(mi)) {
         ma = module_iterator_next(mi);
         if (vmvector_overlap(native_exec_areas, ma->start, ma->end)) {
             native_module_unhook(ma);
         }
    }
    module_iterator_stop(mi);

    if (stub_heap != NULL) {
        special_heap_exit(stub_heap);
        stub_heap = NULL;
    }
}

