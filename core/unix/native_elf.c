/* **********************************************************
 * Copyright (c) 2013-2022 Google, Inc.  All rights reserved.
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
#include "module_private.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "instrument.h" /* instrlist_meta_append */
#include "decode.h"
#include "disassemble.h"
#include "../hashtable.h"

#include <link.h>   /* for struct link_map */
#include <stddef.h> /* offsetof */

#define APP instrlist_meta_append

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

/* The loader's _dl_fixup.  For ia32 it uses d_r_regparms. */
typedef void *(*fixup_fn_t)(struct link_map *l_map, uint dynamic_index)
    IF_X86_32(__attribute__((regparm(3), stdcall, unused)));

app_pc app_dl_runtime_resolve;
fixup_fn_t app_dl_fixup;

enum { MAX_STUB_SIZE = 16 };

static byte plt_stub_template[MAX_STUB_SIZE];
static uint plt_stub_immed_offset;
static uint plt_stub_jmp_tgt_offset;
static size_t plt_stub_size = MAX_STUB_SIZE;
static app_pc plt_stub_heap;
#ifdef X64
static app_pc plt_reachability_stub;
#endif

/* stub code for transfer ret from native module to DR
 * 0x558ed060:  movabs %rax,%gs:0x0         // save xax
 * 0x558ed06b:  movabs $0x7f22caf2d5e3,%rax // put target into xax
 * 0x558ed075:  jmpq   0x558bfd80           // jmp to ibl_xfer
 * 0x558ed07a:  ...
 */
static const size_t ret_stub_size = 0x20;
static app_pc ret_stub_heap;
/* hashtable for native-exec return target
 * - key: the return target in the non-native module
 * - payload: code stub for the return target.
 *   The payload will not be freed until the corrsesponding module is unloaded,
 *   so we can use it (storing it on the app stack) without holding the table lock.
 */
static generic_table_t *native_ret_table;
#define INIT_HTABLE_SIZE_NERET 6 /* should remain small */

static generic_table_t *native_mbr_table;
#define INIT_HTABLE_SIZE_NEMBR 6 /* should remain small */

/* Finds the call to _dl_fixup in _dl_runtime_resolve from ld.so.  _dl_fixup is
 * not exported, but we need to call it.  We assume that _dl_runtime_resolve is
 * straightline code until the call to _dl_fixup.
 */
static app_pc
find_dl_fixup(dcontext_t *dcontext, app_pc resolver)
{
#ifdef X86
    instr_t instr;
    const int max_decodes = 225;
    int i = 0;
    app_pc pc = resolver;
    app_pc fixup = NULL;

    LOG(THREAD, LOG_LOADER, 5, "%s: scanning for _dl_fixup call:\n", __FUNCTION__);
    instr_init(dcontext, &instr);
    while (pc != NULL && i++ < max_decodes) {
        DOLOG(5, LOG_LOADER, { disassemble(dcontext, pc, THREAD); });
        pc = decode(dcontext, pc, &instr);
        if (instr_get_opcode(&instr) == OP_call) {
            opnd_t tgt = instr_get_target(&instr);
            fixup = opnd_get_pc(tgt);
            LOG(THREAD, LOG_LOADER, 1,
                "%s: found _dl_fixup call at " PFX ", _dl_fixup is " PFX ":\n",
                __FUNCTION__, pc, fixup);
            break;
        } else if (instr_is_return(&instr)) {
            break;
        }
        instr_reset(dcontext, &instr);
    }
    instr_free(dcontext, &instr);
    return fixup;
#elif defined(AARCHXX)
    /* FIXME i#1551, i#1569: NYI on ARM/AArch64 */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
#endif /* X86/ARM/RISCV64 */
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

    ASSERT(plt_stub_size == MAX_STUB_SIZE && "stub template should only be init once");
    /* %r11 is scratch on x64 and the PLT resolver uses it, so we do too.  For
     * ia32, there are scratch regs, but the loader doesn't use them.  Presumably
     * it doesn't want to break special calling conventions, so we follow suit
     * and push onto the stack.
     */
#ifdef X86
    IF_X64_ELSE(
        {
            instrlist_append(ilist,
                             INSTR_CREATE_mov_imm(dc, opnd_create_reg(DR_REG_R11),
                                                  OPND_CREATE_INTPTR(0)));
            instrlist_append(ilist,
                             INSTR_CREATE_jmp_ind(dc, opnd_create_rel_addr(0, OPSZ_PTR)));
        },
        {
            instrlist_append(ilist, INSTR_CREATE_push_imm(dc, OPND_CREATE_INTPTR(0)));
            instrlist_append(ilist, INSTR_CREATE_jmp(dc, opnd_create_pc(0)));
        });
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif
    next_pc =
        instrlist_encode_to_copy(dc, ilist, plt_stub_template, NULL, code_end, false);
    ASSERT(next_pc != NULL);
    plt_stub_size = next_pc - plt_stub_template;

    /* We need to get the offsets of the operands.  We assume the operands are
     * encoded as the last part of the instruction.
     */
    mov_len = instr_length(dc, instrlist_first(ilist));
    jmp_len = instr_length(dc, instrlist_last(ilist));
    plt_stub_immed_offset = mov_len - sizeof(void *);
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
    if (resolver == (app_pc)_dynamorio_runtime_resolve)
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
        app_dl_fixup = (fixup_fn_t)find_dl_fixup(dcontext, resolver);
        ASSERT_CURIOSITY(app_dl_fixup != NULL && "failed to find _dl_fixup");
    } else {
        ASSERT((app_pc)app_dl_fixup == find_dl_fixup(dcontext, resolver) &&
               "_dl_fixup should be the same for all modules");
    }

    if (app_dl_fixup != NULL) {
        LOG(THREAD, LOG_LOADER, 3,
            "%s: replacing _dl_runtime_resolve " PFX " with " PFX "\n", __FUNCTION__,
            resolver, _dynamorio_runtime_resolve);
        pltgot[DL_RUNTIME_RESOLVE_IDX] = (app_pc)_dynamorio_runtime_resolve;
    }
}

static bool
create_opt_plt_stub(app_pc plt_tgt, app_pc stub_pc)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    instr_t *instr;
    byte *pc;
    /* XXX i#1238-c#4: because we may continue in the code cache if the target
     * is found or back to d_r_dispatch otherwise, and we use the standard ibl
     * routine, we may not be able to update kstats correctly.
     */
    ASSERT_BUG_NUM(1238,
                   !(DYNAMO_OPTION(kstats) && DYNAMO_OPTION(native_exec_opt)) &&
                       "kstat is not compatible with ");

    /* mov plt_tgt => XAX */
    instr = XINST_CREATE_load_int(
        dcontext,
        opnd_create_reg(IF_X86_ELSE(REG_XAX, IF_RISCV64_ELSE(DR_REG_A0, DR_REG_R0))),
        OPND_CREATE_INTPTR(plt_tgt));
    pc = instr_encode(dcontext, instr, stub_pc);
    instr_destroy(dcontext, instr);
    if (pc == NULL)
        return false;
    /* jmp native_plt_call */
    instr = XINST_CREATE_jump(dcontext,
                              opnd_create_pc(get_native_plt_ibl_xfer_entry(dcontext)));
    pc = instr_encode(dcontext, instr, pc);
    instr_destroy(dcontext, instr);
    if (pc == NULL)
        return false;
    return true;
}

/* Allocates and initializes a stub of code for taking control after a PLT call. */
static app_pc
create_plt_stub(app_pc plt_target)
{
    app_pc stub_pc = special_heap_alloc(plt_stub_heap);
    app_pc *tgt_immed;
    app_pc jmp_tgt;

    if (DYNAMO_OPTION(native_exec_opt) && create_opt_plt_stub(plt_target, stub_pc))
        return stub_pc;

    memcpy(stub_pc, plt_stub_template, plt_stub_size);
    tgt_immed = (app_pc *)(stub_pc + plt_stub_immed_offset);
    jmp_tgt = stub_pc + plt_stub_jmp_tgt_offset;
    *tgt_immed = plt_target;
#ifdef X64
    /* This is a reladdr operand, which we patch in just the same way. */
    insert_relative_target(jmp_tgt, plt_reachability_stub, false /*!hotpatch*/);
#else
    insert_relative_target(jmp_tgt, (app_pc)native_plt_call, false /*!hotpatch*/);
#endif
    return stub_pc;
}

/* Deletes a PLT stub and returns the original target of the stub.
 */
static app_pc
destroy_plt_stub(app_pc stub_pc)
{
    app_pc orig_tgt;
    app_pc *tgt_immed = (app_pc *)(stub_pc + plt_stub_immed_offset);
    orig_tgt = *tgt_immed;
    special_heap_free(plt_stub_heap, stub_pc);
    return orig_tgt;
}

static size_t
plt_reloc_entry_size(ELF_WORD pltrel)
{
    switch (pltrel) {
    case DT_REL: return sizeof(ELF_REL_TYPE);
    case DT_RELA: return sizeof(ELF_RELA_TYPE);
    default: ASSERT(false);
    }
    return sizeof(ELF_REL_TYPE);
}

static bool
is_special_plt_stub(app_pc stub_pc)
{
    special_heap_iterator_t shi;
    bool found = false;
    /* fast check if pc is in dynamo address */
    if (!is_dynamo_address(stub_pc))
        return false;
    /* XXX: this acquires a lock in a nested loop. */
    special_heap_iterator_start(plt_stub_heap, &shi);
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
        ELF_REL_TYPE *rel = (ELF_REL_TYPE *)jmprel;
        app_pc *r_addr;
        app_pc gotval;
        r_addr = (app_pc *)(opd->load_delta + rel->r_offset);
        ASSERT(module_contains_addr(ma, (app_pc)r_addr));
        gotval = *r_addr;
        if (add_hooks) {
            /* If the PLT target is inside the current module, then it is either
             * a lazy resolution stub or was resolved to the current module.
             * Either way we ignore it.
             */
            /* We also ignore it if the PLT target is in a native module */
            if (!module_contains_addr(ma, gotval) && !is_stay_native_pc(gotval)) {
                LOG(THREAD_GET, LOG_LOADER, 4,
                    "%s: hooking cross-module PLT entry to " PFX "\n", __FUNCTION__,
                    gotval);
                *r_addr = create_plt_stub(gotval);
            }
        } else {
            /* XXX: pull the ranges out of the heap up front to avoid lock
             * acquisitions.
             */
            if (is_special_plt_stub(gotval)) {
                *r_addr = destroy_plt_stub(gotval);
            }
        }
    }
}

static void
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
    module_get_os_privmod_data(ma->start, ma->end - ma->start, !at_map /*relocated*/,
                               &opd);
    pltgot = (app_pc *)opd.pltgot;

    /* We can't hook modules that don't have a pltgot. */
    if (pltgot == NULL)
        return;

    /* If we are !at_map, then we assume the loader has already relocated the
     * module and applied protections for PT_GNU_RELRO.  _dl_runtime_resolve is
     * typically inside the relro region, so we must unprotect it.
     */
    if (!at_map && module_get_relro(ma->start, &relro_base, &relro_size)) {
        os_set_protection(relro_base, relro_size, MEMPROT_READ | MEMPROT_WRITE);
        got_unprotected = true;
    }

    /* Insert or remove our lazy dynamic resolver. */
    replace_module_resolver(ma, pltgot, add_hooks /*to_dr*/);
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
        module_change_hooks(ma, true /*add*/, at_map);
}

void
native_module_unhook(module_area_t *ma)
{
    if (DYNAMO_OPTION(native_exec_retakeover))
        module_change_hooks(ma, false /*remove*/, false /*!at_map*/);
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
            jmprel = (app_pc)dyn->d_un.d_ptr; /* relocated */
            break;
#ifdef X64
        case DT_PLTREL: pltrel = dyn->d_un.d_val; break;
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
    return (ELF_REL_TYPE *)(jmprel + relsz * reloc_arg);
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
        LOG(THREAD, LOG_LOADER, 4, "%s: resolved reloc index %d to " PFX "\n",
            __FUNCTION__, reloc_arg, res);
    });
    /* the target is in a native module, so no need to change */
    if (is_stay_native_pc(res))
        return res;
    app_pc stub = create_plt_stub(res);
    rel = find_plt_reloc(l_map, reloc_arg);
    ASSERT(rel != NULL); /* It has to be there if we're doing fixups. */
    r_addr = (app_pc *)(l_map->l_addr + rel->r_offset);
    *r_addr = stub;
    return stub;
}

static void
native_module_htable_init(void)
{
    native_ret_table = generic_hash_create(
        GLOBAL_DCONTEXT, INIT_HTABLE_SIZE_NERET, 50 /* load factor: perf-critical */,
        HASHTABLE_SHARED, NULL _IF_DEBUG("ne_ret table"));
    native_mbr_table = generic_hash_create(
        GLOBAL_DCONTEXT, INIT_HTABLE_SIZE_NEMBR, 50 /* load factor, perf-critical */,
        HASHTABLE_SHARED, NULL _IF_DEBUG("ne_mbr table"));
}

static void
native_module_htable_exit(generic_table_t *htable, app_pc stub_heap)
{
    ptr_uint_t key;
    void *stub_pc;
    int iter;
    if (htable == NULL)
        return;
    TABLE_RWLOCK(htable, write, lock);
    iter = 0;
    do {
        iter = generic_hash_iterate_next(GLOBAL_DCONTEXT, htable, iter, &key, &stub_pc);
        if (iter < 0)
            break;
        /* remove from hashtable */
        iter = generic_hash_iterate_remove(GLOBAL_DCONTEXT, htable, iter, key);
        /* free stub from special heap */
        special_heap_free(stub_heap, stub_pc);
    } while (true);
    TABLE_RWLOCK(htable, write, unlock);
    generic_hash_destroy(GLOBAL_DCONTEXT, htable);
}

static void
native_module_htable_module_unload(module_area_t *ma, generic_table_t *htable,
                                   app_pc stub_heap)
{
    int iter;
    app_pc stub_pc, pc;

    TABLE_RWLOCK(htable, write, lock);
    iter = 0;
    do {
        bool remove = false;
        os_module_data_t *os_data;
        iter = generic_hash_iterate_next(GLOBAL_DCONTEXT, htable, iter, (ptr_uint_t *)&pc,
                                         (void **)&stub_pc);
        if (iter < 0)
            break;
        if (pc < ma->start || pc >= ma->end)
            continue;
        os_data = &ma->os_data;
        if (os_data->contiguous)
            remove = true;
        else {
            int i;
            for (i = 0; i < os_data->num_segments; i++) {
                if (pc >= os_data->segments[i].start && pc < os_data->segments[i].end) {
                    remove = true;
                    break;
                }
            }
        }
        /* remove from hashtable */
        if (remove) {
            iter = generic_hash_iterate_remove(GLOBAL_DCONTEXT, htable, iter,
                                               (ptr_uint_t)pc);
            special_heap_free(stub_heap, pc);
        }
    } while (true);
    TABLE_RWLOCK(htable, write, unlock);
}

static void *
native_module_htable_add(generic_table_t *htable, app_pc stub_heap, ptr_uint_t key,
                         void *payload)
{
    void *stub_pc;
    TABLE_RWLOCK(htable, write, lock);
    /* lookup again */
    stub_pc = generic_hash_lookup(GLOBAL_DCONTEXT, htable, key);
    if (stub_pc != NULL) {
        TABLE_RWLOCK(htable, write, unlock);
        /* we found one, use it and delete the new one */
        special_heap_free(stub_heap, payload);
        return stub_pc;
    }
    generic_hash_add(GLOBAL_DCONTEXT, htable, key, payload);
    TABLE_RWLOCK(htable, write, unlock);
    return payload;
}

void
native_module_init(void)
{
    if (!DYNAMO_OPTION(native_exec_retakeover))
        return;
    ASSERT(plt_stub_heap == NULL && "init should only happen once");
    initialize_plt_stub_template();
    plt_stub_heap = special_heap_init(plt_stub_size, true /*locked*/, true /*executable*/,
                                      true /*persistent*/);
#ifdef X64
    /* i#719: native_plt_call may not be reachable from the stub heap, so we
     * indirect through this "stub".
     */
    plt_reachability_stub = special_heap_alloc(plt_stub_heap);
    *((app_pc *)plt_reachability_stub) = (app_pc)native_plt_call;
#endif

    ASSERT(ret_stub_heap == NULL && "init should only happen once");
    ret_stub_heap = special_heap_init(ret_stub_size, true /* locked */,
                                      true /* exectable */, true /* persistent */);
    native_module_htable_init();
}

void
native_module_exit(void)
{
    /* Make sure we can scan all modules on native_exec_areas and unhook them.
     * If this fails, we get special heap leak asserts.
     */
    module_iterator_t *mi;
    module_area_t *ma;

    if (native_exec_areas != NULL) {
        mi = module_iterator_start();
        while (module_iterator_hasnext(mi)) {
            ma = module_iterator_next(mi);
            if (vmvector_overlap(native_exec_areas, ma->start, ma->end))
                native_module_unhook(ma);
        }
        module_iterator_stop(mi);
    }

#ifdef X64
    if (plt_reachability_stub != NULL) {
        special_heap_free(plt_stub_heap, plt_reachability_stub);
        plt_reachability_stub = NULL;
    }
#endif

    /* free entries in plt_stub_heap */
    native_module_htable_exit(native_mbr_table, plt_stub_heap);
    native_mbr_table = NULL;
    /* destroy plt_stub_heap */
    if (plt_stub_heap != NULL) {
        special_heap_exit(plt_stub_heap);
        plt_stub_heap = NULL;
    }

    /* free ret_stub_heap */
    native_module_htable_exit(native_ret_table, ret_stub_heap);
    native_ret_table = NULL;
    /* destroy ret_stub_heap */
    if (ret_stub_heap != NULL) {
        special_heap_exit(ret_stub_heap);
        ret_stub_heap = NULL;
    }
}

/* called on unloading a non-native module */
void
native_module_nonnative_mod_unload(module_area_t *ma)
{
    ASSERT(DYNAMO_OPTION(native_exec_retakeover) && DYNAMO_OPTION(native_exec_opt));
    native_module_htable_module_unload(ma, native_ret_table, ret_stub_heap);
    native_module_htable_module_unload(ma, native_mbr_table, plt_stub_heap);
}

/* We create a ret_stub for each return target of the call site from non-native
 * module to native modue. The stub_pc will replace the real return target so that
 * DR can regain the control after the native module returns.
 */
static app_pc
special_ret_stub_create(dcontext_t *dcontext, app_pc tgt)
{
    instrlist_t ilist;
    app_pc stub_pc;

    /* alloc and encode special ret stub */
    stub_pc = special_heap_alloc(ret_stub_heap);

    instrlist_init(&ilist);
    /* we need to steal xax register, xax restore is in the ibl_xfer code from
     * emit_native_ret_ibl_xfer.
     */
    APP(&ilist, instr_create_save_to_tls(dcontext, SCRATCH_REG0, TLS_REG0_SLOT));
    /* the rest is similar to opt_plt_stub */
    /* mov tgt => XAX */
    APP(&ilist,
        XINST_CREATE_load_int(dcontext, opnd_create_reg(SCRATCH_REG0),
                              OPND_CREATE_INTPTR(tgt)));
    /* jmp native_ret_ibl */
    APP(&ilist,
        XINST_CREATE_jump(dcontext,
                          opnd_create_pc(get_native_ret_ibl_xfer_entry(dcontext))));
    instrlist_encode(dcontext, &ilist, stub_pc, false);
    instrlist_clear(dcontext, &ilist);

    return (app_pc)native_module_htable_add(native_ret_table, ret_stub_heap,
                                            (ptr_uint_t)tgt, (void *)stub_pc);
}

#ifdef DR_APP_EXPORTS
DR_APP_API void *
dr_app_handle_mbr_target(void *target)
{
    void *stub;
    if (!DYNAMO_OPTION(native_exec) || !DYNAMO_OPTION(native_exec_retakeover))
        return target;
    if (is_stay_native_pc(target))
        return target;
    stub = create_plt_stub(target);
    return native_module_htable_add(native_mbr_table, plt_stub_heap, (ptr_uint_t)target,
                                    stub);
}
#endif /* DR_APP_EXPORTS */

/* get (create if not exist) a ret_stub for the return target: tgt */
app_pc
native_module_get_ret_stub(dcontext_t *dcontext, app_pc tgt)
{
    app_pc stub_pc;

    TABLE_RWLOCK(native_ret_table, read, lock);
    stub_pc =
        (app_pc)generic_hash_lookup(GLOBAL_DCONTEXT, native_ret_table, (ptr_uint_t)tgt);
    TABLE_RWLOCK(native_ret_table, read, unlock);
    if (stub_pc == NULL)
        stub_pc = special_ret_stub_create(dcontext, tgt);
    ASSERT(stub_pc != NULL);
    return stub_pc;
}

/* xref i#1247: clean call right before dl_runtime_resolve return */
void
native_module_at_runtime_resolve_ret(app_pc xsp, int ret_imm)
{
    app_pc call_tgt, ret_tgt;

    if (!d_r_safe_read(xsp, sizeof(app_pc), &call_tgt) ||
        !d_r_safe_read(xsp + ret_imm + sizeof(XSP_SZ), sizeof(app_pc), &ret_tgt)) {
        ASSERT(false && "fail to read app stack!\n");
        return;
    }
    if (is_stay_native_pc(call_tgt) && !is_stay_native_pc(ret_tgt)) {
        /* replace the return target for regaining control later */
        dcontext_t *dcontext = get_thread_private_dcontext();
        app_pc stub_pc = native_module_get_ret_stub(dcontext, ret_tgt);
        DEBUG_DECLARE(bool ok =)
        safe_write_ex(xsp + ret_imm + sizeof(XSP_SZ), sizeof(app_pc), &stub_pc,
                      NULL /* bytes written */);
        ASSERT(stub_pc != NULL && ok);
        LOG(THREAD, LOG_ALL, 3, "replace return target " PFX " with " PFX " at " PFX "\n",
            ret_tgt, stub_pc, (xsp + ret_imm + sizeof(XSP_SZ)));
    }
}

static bool
is_special_ret_stub(app_pc pc)
{
    special_heap_iterator_t shi;
    bool found = false;
    /* fast check if pc is in dynamo address */
    if (!is_dynamo_address(pc))
        return false;
    /* XXX: this acquires a lock in a loop. */
    special_heap_iterator_start(ret_stub_heap, &shi);
    while (special_heap_iterator_hasnext(&shi)) {
        app_pc start, end;
        special_heap_iterator_next(&shi, &start, &end);
        if (pc >= start && pc < end) {
            found = true;
            break;
        }
    }
    special_heap_iterator_stop(&shi);
    return found;
}

/* i#1276: dcontext->next_tag could be special stub pc from special_ret_stub
 * for DR maintaining the control in hybrid execution, this routine is called
 * in d_r_dispatch to adjust the target if necessary.
 */
bool
native_exec_replace_next_tag(dcontext_t *dcontext)
{
    ASSERT(DYNAMO_OPTION(native_exec) && DYNAMO_OPTION(native_exec_opt));
    if (is_special_ret_stub(dcontext->next_tag)) {
#ifdef X86
        instr_t instr;
        app_pc pc;
        /* we assume the ret stub is
         *   save %xax
         *   mov tgt => %xax
         */
        instr_init(dcontext, &instr);
        /* skip save %rax */
        pc = decode(dcontext, dcontext->next_tag, &instr);
        ASSERT(instr_get_opcode(&instr) == OP_mov_st &&
               opnd_is_reg(instr_get_src(&instr, 0)) &&
               opnd_get_reg(instr_get_src(&instr, 0)) == DR_REG_XAX);
        instr_reset(dcontext, &instr);
        /* get target from mov tgt => %xax */
        pc = decode(dcontext, pc, &instr);
        ASSERT(instr_get_opcode(&instr) == OP_mov_imm &&
               opnd_is_immed_int(instr_get_src(&instr, 0)));
        dcontext->next_tag = (app_pc)opnd_get_immed_int(instr_get_src(&instr, 0));
        instr_free(dcontext, &instr);
        return true;
#elif defined(ARM)
        /* FIXME i#1551: NYI on ARM */
        ASSERT_NOT_REACHED();
#endif
    }
    return false;
}
