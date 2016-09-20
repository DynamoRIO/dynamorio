/* **********************************************************
 * Copyright (c) 2011-2016 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* drutil: DynamoRIO Instrumentation Utilities
 * Derived from Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* DynamoRIO Instrumentation Utilities Extension */

#include "dr_api.h"
#include "drmgr.h"
#include "../ext_utils.h"

/* currently using asserts on internal logic sanity checks (never on
 * input from user)
 */
#ifdef DEBUG
# define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#else
# define ASSERT(x, msg) /* nothing */
#endif

/* There are cases where notifying the user is the right thing, even for a library.
 * Xref i#1055 where w/o visible notification the user might not know what's
 * going on.
 */
#ifdef WINDOWS
# define USAGE_ERROR(msg) do { \
    dr_messagebox("FATAL USAGE ERROR: %s", msg); \
    dr_abort(); \
} while (0);
#else
# define USAGE_ERROR(msg) do { \
    dr_fprintf(STDERR, "FATAL USAGE ERROR: %s\n", msg); \
    dr_abort(); \
} while (0);
#endif

#define PRE instrlist_meta_preinsert
/* for inserting an app instruction, which must have a translation ("xl8") field */
#define PREXL8 instrlist_preinsert

/***************************************************************************
 * INIT
 */

static int drutil_init_count;

DR_EXPORT
bool
drutil_init(void)
{
    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drutil_init_count, 1);
    if (count > 1)
        return true;

    /* nothing yet: but putting in API up front in case need later */

    return true;
}

DR_EXPORT
void
drutil_exit(void)
{
    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drutil_init_count, -1);
    if (count != 0)
        return;

    /* nothing yet: but putting in API up front in case need later */
}

/***************************************************************************
 * MEMORY TRACING
 */
#ifdef X86
static bool
drutil_insert_get_mem_addr_x86(void *drcontext, instrlist_t *bb, instr_t *where,
                               opnd_t memref, reg_id_t dst, reg_id_t scratch);
#elif defined(AARCHXX)
static bool
drutil_insert_get_mem_addr_arm(void *drcontext, instrlist_t *bb, instr_t *where,
                               opnd_t memref, reg_id_t dst, reg_id_t scratch);
#endif /* X86/ARM */


/* Could be optimized to have scratch==dst for many common cases, but
 * need way to get a 2nd reg for corner cases: simpler to ask caller
 * to give us scratch reg distinct from dst
 * XXX: however, this means that a client must spill the scratch reg
 * every time, even though it's only used for far or xlat memref.
 *
 * XXX: provide a version that calls clean call?  would have to hardcode
 * what gets included: memory size?  perhaps should try to create a
 * vararg clean call arg feature to chain things together.
 */
DR_EXPORT
bool
drutil_insert_get_mem_addr(void *drcontext, instrlist_t *bb, instr_t *where,
                           opnd_t memref, reg_id_t dst, reg_id_t scratch)
{
#if defined(X86)
    return drutil_insert_get_mem_addr_x86(drcontext, bb, where, memref, dst, scratch);
#elif defined(AARCHXX)
    return drutil_insert_get_mem_addr_arm(drcontext, bb, where, memref, dst, scratch);
#endif
}

#ifdef X86
static bool
drutil_insert_get_mem_addr_x86(void *drcontext, instrlist_t *bb, instr_t *where,
                               opnd_t memref, reg_id_t dst, reg_id_t scratch)
{
    if (opnd_is_far_base_disp(memref) &&
        /* We assume that far memory references via %ds and %es are flat,
         * i.e. the segment base is 0, so we only handle %fs and %gs here.
         * The assumption is consistent with dr_insert_get_seg_base,
         * which does say for windows it only supports TLS segment,
         * and inserts "mov 0 => reg" for %ds and %es instead.
         */
        opnd_get_segment(memref) != DR_SEG_ES &&
        opnd_get_segment(memref) != DR_SEG_DS) {
        /* get segment base into scratch, then add to memref base and lea */
        instr_t *near_in_dst = NULL;
        if (opnd_uses_reg(memref, scratch) ||
            (opnd_get_base(memref) != DR_REG_NULL &&
             opnd_get_index(memref) != DR_REG_NULL)) {
            /* have to take two steps */
            opnd_set_size(&memref, OPSZ_lea);
            near_in_dst = INSTR_CREATE_lea(drcontext, opnd_create_reg(dst), memref);
            PRE(bb, where, near_in_dst);
        }
        if (!dr_insert_get_seg_base(drcontext, bb, where, opnd_get_segment(memref),
                                    scratch)) {
            if (near_in_dst != NULL) {
                instrlist_remove(bb, near_in_dst);
                instr_destroy(drcontext, near_in_dst);
            }
            return false;
        }
        if (near_in_dst != NULL) {
            PRE(bb, where,
                INSTR_CREATE_lea(drcontext, opnd_create_reg(dst),
                                 opnd_create_base_disp(dst, scratch, 1, 0, OPSZ_lea)));
        } else {
            reg_id_t base = opnd_get_base(memref);
            reg_id_t index = opnd_get_index(memref);
            int scale = opnd_get_scale(memref);
            int disp = opnd_get_disp(memref);
            if (opnd_get_base(memref) == DR_REG_NULL) {
                base = scratch;
            } else if (opnd_get_index(memref) == DR_REG_NULL) {
                index = scratch;
                scale = 1;
            } else {
                ASSERT(false, "memaddr internal error");
            }
            PRE(bb, where,
                INSTR_CREATE_lea(drcontext, opnd_create_reg(dst),
                                 opnd_create_base_disp(base, index, scale, disp,
                                                       OPSZ_lea)));
        }
    } else if (opnd_is_base_disp(memref)) {
        /* special handling for xlat instr, [%ebx,%al]
         * - save %eax
         * - movzx %al => %eax
         * - lea [%ebx, %eax] => dst
         * - restore %eax
         */
        bool is_xlat = false;
        if (opnd_get_index(memref) == DR_REG_AL) {
            is_xlat = true;
            if (scratch != DR_REG_XAX && dst != DR_REG_XAX) {
                /* we do not have to save xax if it is saved by caller */
                PRE(bb, where,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(scratch),
                                        opnd_create_reg(DR_REG_XAX)));
            }
            PRE(bb, where,
                INSTR_CREATE_movzx(drcontext, opnd_create_reg(DR_REG_XAX),
                                   opnd_create_reg(DR_REG_AL)));
            memref = opnd_create_base_disp(DR_REG_XBX, DR_REG_XAX, 1, 0,
                                           OPSZ_lea);
        }
        /* lea [ref] => reg */
        opnd_set_size(&memref, OPSZ_lea);
        PRE(bb, where,
            INSTR_CREATE_lea(drcontext, opnd_create_reg(dst), memref));
        if (is_xlat && scratch != DR_REG_XAX && dst != DR_REG_XAX) {
            PRE(bb, where,
                INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_XAX),
                                    opnd_create_reg(scratch)));
        }
    } else if (IF_X64(opnd_is_rel_addr(memref) ||) opnd_is_abs_addr(memref)) {
        /* mov addr => reg */
        PRE(bb, where,
            INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(dst),
                                 OPND_CREATE_INTPTR(opnd_get_addr(memref))));
    } else {
        /* unhandled memory reference */
        return false;
    }
    return true;
}
#elif defined(AARCHXX)

# ifdef ARM
static bool
instr_has_opnd(instr_t *instr, opnd_t opnd)
{
    int i;
    if (instr == NULL)
        return false;
    for (i = 0; i < instr_num_srcs(instr); i++) {
        if (opnd_same(opnd, instr_get_src(instr, i)))
            return true;
    }
    for (i = 0; i < instr_num_dsts(instr); i++) {
        if (opnd_same(opnd, instr_get_dst(instr, i)))
            return true;
    }
    return false;
}

static instr_t *
instrlist_find_app_instr(instrlist_t *ilist, instr_t *where, opnd_t opnd)
{
    instr_t *app;
    /* looking for app instr at/after where */
    for (app  = instr_is_app(where) ? where : instr_get_next_app(where);
         app != NULL;
         app  = instr_get_next_app(app)) {
        if (instr_has_opnd(app, opnd))
            return app;
    }
    /* looking for app instr before where */
    for (app  = instr_get_prev_app(where);
         app != NULL;
         app  = instr_get_prev_app(app)) {
        if (instr_has_opnd(app, opnd))
            return app;
    }
    return NULL;
}
# endif /* ARM */

static reg_id_t
replace_stolen_reg(void *drcontext, instrlist_t *bb, instr_t *where,
                   opnd_t memref, reg_id_t dst, reg_id_t scratch)
{
    reg_id_t reg;
    reg = opnd_uses_reg(memref, dst) ? scratch : dst;
    DR_ASSERT(!opnd_uses_reg(memref, reg));
    dr_insert_get_stolen_reg_value(drcontext, bb, where, reg);
    return reg;
}

static bool
drutil_insert_get_mem_addr_arm(void *drcontext, instrlist_t *bb, instr_t *where,
                               opnd_t memref, reg_id_t dst, reg_id_t scratch)
{
    if (!opnd_is_base_disp(memref) IF_AARCH64(&& !opnd_is_rel_addr(memref)))
        return false;
# ifdef ARM
    if (opnd_get_base(memref) == DR_REG_PC) {
        app_pc target;
        /* We need the app instr for getting the rel_addr_target.
         * XXX: add drutil_insert_get_mem_addr_ex to let client provide app instr.
         */
        instr_t *app = instrlist_find_app_instr(bb, where, memref);
        if (app == NULL)
            return false;
        if (!instr_get_rel_addr_target(app, &target))
            return false;
        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)target,
                                         opnd_create_reg(dst), bb, where,
                                         NULL, NULL);
    }
# else /* AARCH64 */
    if (opnd_is_rel_addr(memref)) {
        instrlist_insert_mov_immed_ptrsz(drcontext,
                                         (ptr_int_t)opnd_get_addr(memref),
                                         opnd_create_reg(dst), bb, where,
                                         NULL, NULL);
        return true;
    }
# endif /* ARM/AARCH64 */
    else {
        instr_t *instr;
        reg_id_t base   = opnd_get_base(memref);
        reg_id_t index  = opnd_get_index(memref);
        bool negated    = TEST(DR_OPND_NEGATED, opnd_get_flags(memref));
        int      disp   = opnd_get_disp(memref);
        reg_id_t stolen = dr_get_stolen_reg();
        /* On ARM, disp is never negative; on AArch64, we do not use DR_OPND_NEGATED. */
        ASSERT(IF_ARM_ELSE(disp >= 0, !negated), "DR_OPND_NEGATED internal error");
        if (disp < 0) {
            disp = -disp;
            negated = !negated;
        }
        if (dst == stolen || scratch == stolen)
            return false;
        if (base == stolen)
            base = replace_stolen_reg(drcontext, bb, where, memref, dst, scratch);
        else if (index == stolen)
            index = replace_stolen_reg(drcontext, bb, where, memref, dst, scratch);
        if (index == REG_NULL && opnd_get_disp(memref) != 0) {
            /* first try "add dst, base, #disp" */
            instr = negated ?
                INSTR_CREATE_sub(drcontext,
                                 opnd_create_reg(dst),
                                 opnd_create_reg(base),
                                 OPND_CREATE_INT(disp)) :
                XINST_CREATE_add_2src(drcontext,
                                      opnd_create_reg(dst),
                                      opnd_create_reg(base),
                                      OPND_CREATE_INT(disp));
#           define MAX_ADD_IMM_DISP (1 << 12)
            if (IF_ARM_ELSE(instr_is_encoding_possible(instr),
                            disp < MAX_ADD_IMM_DISP)) {
                PRE(bb, where, instr);
                return true;
            }
            instr_destroy(drcontext, instr);
            /* The memref may have a disp that cannot be directly encoded into an
             * add_imm instr, so we use movw to put disp into the scratch instead
             * and fake it as an index reg to insert an add instr later.
             */
            /* if dst is used in memref, we use scratch instead */
            index = (base == dst) ? scratch : dst;
            PRE(bb, where, XINST_CREATE_load_int(drcontext,
                                                 opnd_create_reg(index),
                                                 OPND_CREATE_INT(disp)));
            /* "add" instr is inserted below with a fake index reg added here */
        }
        if (index != REG_NULL) {
# ifdef ARM
            uint amount;
            dr_shift_type_t shift = opnd_get_index_shift(memref, &amount);
            instr = negated ?
                INSTR_CREATE_sub_shimm(drcontext,
                                       opnd_create_reg(dst),
                                       opnd_create_reg(base),
                                       opnd_create_reg(index),
                                       OPND_CREATE_INT(shift),
                                       OPND_CREATE_INT(amount)) :
                INSTR_CREATE_add_shimm(drcontext,
                                       opnd_create_reg(dst),
                                       opnd_create_reg(base),
                                       opnd_create_reg(index),
                                       OPND_CREATE_INT(shift),
                                       OPND_CREATE_INT(amount));
# else /* AARCH64 */
            uint amount;
            dr_extend_type_t extend = opnd_get_index_extend(memref, NULL, &amount);
            instr = negated ?
                INSTR_CREATE_sub_extend(drcontext,
                                        opnd_create_reg(dst),
                                        opnd_create_reg(base),
                                        opnd_create_reg(index),
                                        OPND_CREATE_INT(extend),
                                        OPND_CREATE_INT(amount)) :
                INSTR_CREATE_add_extend(drcontext,
                                        opnd_create_reg(dst),
                                        opnd_create_reg(base),
                                        opnd_create_reg(index),
                                        OPND_CREATE_INT(extend),
                                        OPND_CREATE_INT(amount));
# endif /* ARM/AARCH64 */
            PRE(bb, where, instr);
        } else if (base != dst) {
            PRE(bb, where, XINST_CREATE_move(drcontext,
                                             opnd_create_reg(dst),
                                             opnd_create_reg(base)));
        }
    }
    return true;
}
#endif /* X86/AARCHXX */

DR_EXPORT
uint
drutil_opnd_mem_size_in_bytes(opnd_t memref, instr_t *inst)
{
#ifdef X86
    if (inst != NULL && instr_get_opcode(inst) == OP_enter) {
        uint extra_pushes = (uint) opnd_get_immed_int(instr_get_src(inst, 1));
        uint sz = opnd_size_in_bytes(opnd_get_size(instr_get_dst(inst, 1)));
        ASSERT(opnd_is_immed_int(instr_get_src(inst, 1)), "malformed OP_enter");
        return sz*extra_pushes;
    } else
#endif /* X86 */
        return opnd_size_in_bytes(opnd_get_size(memref));
}

#ifdef X86
static bool
opc_is_stringop_loop(uint opc)
{
    return (opc == OP_rep_ins || opc == OP_rep_outs || opc == OP_rep_movs ||
            opc == OP_rep_stos || opc == OP_rep_lods || opc == OP_rep_cmps ||
            opc == OP_repne_cmps || opc == OP_rep_scas || opc == OP_repne_scas);
}

static instr_t *
create_nonloop_stringop(void *drcontext, instr_t *inst)
{
    instr_t *res;
    int nsrc = instr_num_srcs(inst);
    int ndst = instr_num_dsts(inst);
    uint opc = instr_get_opcode(inst);
    int i;
    ASSERT(opc_is_stringop_loop(opc), "invalid param");
    switch (opc) {
    case OP_rep_ins:    opc = OP_ins; break;;
    case OP_rep_outs:   opc = OP_outs; break;;
    case OP_rep_movs:   opc = OP_movs; break;;
    case OP_rep_stos:   opc = OP_stos; break;;
    case OP_rep_lods:   opc = OP_lods; break;;
    case OP_rep_cmps:   opc = OP_cmps; break;;
    case OP_repne_cmps: opc = OP_cmps; break;;
    case OP_rep_scas:   opc = OP_scas; break;;
    case OP_repne_scas: opc = OP_scas; break;;
    default: ASSERT(false, "not a stringop loop opcode"); return NULL;
    }
    res = instr_build(drcontext, opc, ndst - 1, nsrc - 1);
    /* We assume xcx is last src and last dst */
    ASSERT(opnd_is_reg(instr_get_src(inst, nsrc - 1)) &&
           opnd_uses_reg(instr_get_src(inst, nsrc - 1), DR_REG_XCX),
           "rep opnd order assumption violated");
    ASSERT(opnd_is_reg(instr_get_dst(inst, ndst - 1)) &&
           opnd_uses_reg(instr_get_dst(inst, ndst - 1), DR_REG_XCX),
           "rep opnd order assumption violated");
    for (i = 0; i < nsrc - 1; i++)
        instr_set_src(res, i, instr_get_src(inst, i));
    for (i = 0; i < ndst - 1; i++)
        instr_set_dst(res, i, instr_get_dst(inst, i));
    instr_set_translation(res, instr_get_app_pc(inst));
    return res;
}
#endif /* X86 */

DR_EXPORT
bool
drutil_expand_rep_string_ex(void *drcontext, instrlist_t *bb, bool *expanded OUT,
                            instr_t **stringop OUT)
{
#ifdef X86
    instr_t *inst, *next_inst, *first_app = NULL;
    bool delete_rest = false;
    uint opc;
#endif

    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_APP2APP) {
        USAGE_ERROR("drutil_expand_rep_string* must be called from "
                    "drmgr's app2app phase");
        return false;
    }

#ifdef X86
    /* Make a rep string instr be its own bb: the loop is going to
     * duplicate the tail anyway, and have to terminate at the added cbr.
     */
    for (inst = instrlist_first(bb);
         inst != NULL;
         inst = next_inst) {
        next_inst = instr_get_next(inst);
        if (delete_rest) {
            instrlist_remove(bb, inst);
            instr_destroy(drcontext, inst);
        } else if (instr_is_app(inst)) {
            /* We have to handle meta instrs, as drwrap_replace_native() and
             * some other app2app xforms use them.
             */
            if (first_app == NULL)
                first_app = inst;
            opc = instr_get_opcode(inst);
            if (opc_is_stringop_loop(opc)) {
                delete_rest = true;
                if (inst != first_app) {
                    instrlist_remove(bb, inst);
                    instr_destroy(drcontext, inst);
                }
            }
        }
    }

    /* Convert to a regular loop if it's the sole instr */
    inst = first_app;
    opc = (inst == NULL) ? OP_INVALID : instr_get_opcode(inst);
    if (opc_is_stringop_loop(opc)) {
        /* A rep string instr does check for 0 up front.  DR limits us
         * to 1 cbr but drmgr will mark the extras as meta later.  If ecx is uninit
         * the loop* will catch it so we're ok not instrumenting this.
         * I would just jecxz to loop, but w/ instru it can't reach so
         * I have to add yet more internal jmps that will execute each
         * iter.  We use drmgr's feature of allowing extra non-meta instrs.
         * Our "mov $1,ecx" will remain non-meta.
         * Note that we do not want any of the others to have xl8 as its
         * translation as that could trigger duplicate clean calls from
         * other passes looking for post-call or other addresses so we use
         * xl8+1 which will always be mid-instr.  NULL is another possibility,
         * but it results in meta-may-fault instrs that need a translation
         * and naturally want to use the app instr's translation.
         *
         * So we have:
         *    rep movs
         * =>
         *    jecxz  zero
         *    jmp    iter
         *  zero:
         *    mov    $0x00000001 -> %ecx
         *    jmp    pre_loop
         *  iter:
         *    movs   %ds:(%esi) %esi %edi -> %es:(%edi) %esi %edi
         *  pre_loop:
         *    loop
         *
         * XXX: this non-linear code can complicate subsequent
         * analysis routines.  Perhaps we should consider splitting
         * into multiple bbs?
         *
         * XXX i#1460: the jecxz is marked meta by drmgr (via i#676) and is
         * thus not mangled by DR, resulting in just an 8-bit reach.
         */
        app_pc xl8 = instr_get_app_pc(inst);
        app_pc fake_xl8 = xl8 + 1;
        opnd_t xcx = instr_get_dst(inst, instr_num_dsts(inst) - 1);
        instr_t *loop, *pre_loop, *jecxz, *zero, *iter, *string;
        ASSERT(opnd_uses_reg(xcx, DR_REG_XCX), "rep string opnd order mismatch");
        ASSERT(inst == instrlist_last(bb), "repstr not alone in bb");

        pre_loop = INSTR_CREATE_label(drcontext);
        /* hack to handle loop decrementing xcx: simpler if could have 2 cbrs! */
        if (opnd_get_size(xcx) == OPSZ_8) {
            /* rely on setting upper 32 bits to zero */
            zero = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_ECX),
                                        OPND_CREATE_INT32(1));
        } else {
            zero = INSTR_CREATE_mov_imm(drcontext, xcx,
                                        opnd_create_immed_int(1, opnd_get_size(xcx)));
        }
        iter = INSTR_CREATE_label(drcontext);

        jecxz = INSTR_CREATE_jecxz(drcontext, opnd_create_instr(zero));
        /* be sure to match the same counter reg width */
        instr_set_src(jecxz, 1, xcx);
        PREXL8(bb, inst, INSTR_XL8(jecxz, fake_xl8));
        PREXL8(bb, inst, INSTR_XL8
               (INSTR_CREATE_jmp_short(drcontext, opnd_create_instr(iter)), fake_xl8));
        PREXL8(bb, inst, INSTR_XL8(zero, fake_xl8));
        /* target the instrumentation for the loop, not loop itself */
        PREXL8(bb, inst, INSTR_XL8
               (INSTR_CREATE_jmp(drcontext, opnd_create_instr(pre_loop)), fake_xl8));
        PRE(bb, inst, iter);

        string = INSTR_XL8(create_nonloop_stringop(drcontext, inst), xl8);
        if (stringop != NULL)
            *stringop = string;
        PREXL8(bb, inst, string);

        PRE(bb, inst, pre_loop);
        if (opc == OP_rep_cmps || opc == OP_rep_scas) {
            loop = INSTR_CREATE_loope(drcontext, opnd_create_pc(xl8));
        } else if (opc == OP_repne_cmps || opc == OP_repne_scas) {
            loop = INSTR_CREATE_loopne(drcontext, opnd_create_pc(xl8));
        } else {
            loop = INSTR_CREATE_loop(drcontext, opnd_create_pc(xl8));
        }
        /* be sure to match the same counter reg width */
        instr_set_src(loop, 1, xcx);
        instr_set_dst(loop, 0, xcx);
        PREXL8(bb, inst, INSTR_XL8(loop, fake_xl8));

        /* now throw out the orig instr */
        instrlist_remove(bb, inst);
        instr_destroy(drcontext, inst);

        if (expanded != NULL)
            *expanded = true;
    } else if (expanded != NULL)
        *expanded = false;
#elif defined(ARM)
    if (expanded != NULL)
        *expanded = false;
    if (stringop != NULL)
        *stringop = NULL;
#endif

    return true;
}

DR_EXPORT
bool
drutil_expand_rep_string(void *drcontext, instrlist_t *bb)
{
    return drutil_expand_rep_string_ex(drcontext, bb, NULL, NULL);
}
