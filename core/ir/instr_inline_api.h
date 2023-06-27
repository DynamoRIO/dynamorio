/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
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

#ifndef _DR_IR_INSTR_INLINE_H_
#define _DR_IR_INSTR_INLINE_H_ 1

/**************************************************
 * INSTRUCTION ROUTINE INLINING SUPPORT
 */
/**
 * @file dr_ir_instr_inline.h
 * @brief Instruction routine inlining support.
 */

#ifdef DR_FAST_IR /* Around whole file. */

#    ifdef DYNAMORIO_INTERNAL
/* Use different names to avoid conflicting with an including project */
#        define DR_IF_DEBUG IF_DEBUG
#        define DR_IF_DEBUG_ IF_DEBUG_

#        define MAKE_OPNDS_VALID(instr)                       \
            (void)(TEST(INSTR_OPERANDS_VALID, (instr)->flags) \
                       ? (instr)                              \
                       : instr_decode_with_current_dcontext(instr))
/* CLIENT_ASSERT with a trailing comma in a debug build, otherwise nothing. */
#        define CLIENT_ASSERT_(cond, msg) DR_IF_DEBUG_(CLIENT_ASSERT(cond, msg))
#    else
#        define DR_IF_DEBUG(stmt)
#        define DR_IF_DEBUG_(stmt)
/* Internally DR has multiple levels of IR, but once it gets to a client, we
 * assume it's already level 3 or higher, and we don't need to do any checks.
 * Furthermore, instr_decode() and get_thread_private_dcontext() are not
 * exported.
 */
#        define MAKE_OPNDS_VALID(instr) ((void)0)
/* Turn off checks if a client includes us with DR_FAST_IR.  We can't call the
 * internal routines we'd use for these checks anyway.
 */
#        define CLIENT_ASSERT(cond, msg)
#        define CLIENT_ASSERT_(cond, msg)
#    endif

/* Any function that takes or returns an opnd_t by value should be a macro,
 * *not* an inline function.  Most widely available versions of gcc have trouble
 * optimizing structs that have been passed by value, even after inlining.
 */

/* opnd_t predicates */

/* Simple predicates */
#    define OPND_IS_NULL(op) ((op).kind == NULL_kind)
#    define OPND_IS_IMMED_INT(op) ((op).kind == IMMED_INTEGER_kind)
#    define OPND_IS_IMMED_FLOAT(op) ((op).kind == IMMED_FLOAT_kind)
#    define OPND_IS_IMMED_DOUBLE(op) ((op).kind == IMMED_DOUBLE_kind)
#    define OPND_IS_NEAR_PC(op) ((op).kind == PC_kind)
#    define OPND_IS_NEAR_INSTR(op) ((op).kind == INSTR_kind)
#    define OPND_IS_REG(op) ((op).kind == REG_kind)
#    define OPND_IS_BASE_DISP(op) ((op).kind == BASE_DISP_kind)
#    define OPND_IS_FAR_PC(op) ((op).kind == FAR_PC_kind)
#    define OPND_IS_FAR_INSTR(op) ((op).kind == FAR_INSTR_kind)
#    define OPND_IS_MEM_INSTR(op) ((op).kind == MEM_INSTR_kind)
#    define OPND_IS_VALID(op) ((op).kind < LAST_kind)

#    define opnd_is_null OPND_IS_NULL
#    define opnd_is_immed_int OPND_IS_IMMED_INT
#    define opnd_is_immed_float OPND_IS_IMMED_FLOAT
#    define opnd_is_immed_double OPND_IS_IMMED_DOUBLE
#    define opnd_is_near_pc OPND_IS_NEAR_PC
#    define opnd_is_near_instr OPND_IS_NEAR_INSTR
#    define opnd_is_reg OPND_IS_REG
#    define opnd_is_base_disp OPND_IS_BASE_DISP
#    define opnd_is_far_pc OPND_IS_FAR_PC
#    define opnd_is_far_instr OPND_IS_FAR_INSTR
#    define opnd_is_mem_instr OPND_IS_MEM_INSTR
#    define opnd_is_valid OPND_IS_VALID

/* Compound predicates */
INSTR_INLINE
bool
opnd_is_immed(opnd_t op)
{
    return op.kind == IMMED_INTEGER_kind || op.kind == IMMED_FLOAT_kind ||
        op.kind == IMMED_DOUBLE_kind;
}

INSTR_INLINE
bool
opnd_is_pc(opnd_t op)
{
    return op.kind == PC_kind || op.kind == FAR_PC_kind;
}

INSTR_INLINE
bool
opnd_is_instr(opnd_t op)
{
    return op.kind == INSTR_kind || op.kind == FAR_INSTR_kind;
}

INSTR_INLINE
bool
opnd_is_near_base_disp(opnd_t op)
{
    return op.kind == BASE_DISP_kind IF_X86(&&op.aux.segment == DR_REG_NULL);
}

INSTR_INLINE
bool
opnd_is_far_base_disp(opnd_t op)
{
    return IF_X86_ELSE(op.kind == BASE_DISP_kind && op.aux.segment != DR_REG_NULL, false);
}

INSTR_INLINE
bool
opnd_is_element_vector_reg(opnd_t op)
{
    return op.kind == REG_kind && ((op.aux.flags & DR_OPND_IS_VECTOR) != 0);
}

INSTR_INLINE
bool
opnd_is_predicate_reg(opnd_t op)
{
    /* TODO i#5488: support for x86 AVC512 mask registers */
    return IF_AARCH64_ELSE(op.kind == REG_kind &&
                               op.value.reg_and_element_size.reg >= DR_REG_P0 &&
                               op.value.reg_and_element_size.reg <= DR_REG_P15,
                           /* Silence an x86 only warning about an unused `op`.*/
                           false && op.kind == REG_kind);
}

INSTR_INLINE
bool
opnd_is_predicate_merge(opnd_t op)
{
    return opnd_is_predicate_reg(op) &&
        ((op.aux.flags & DR_OPND_IS_MERGE_PREDICATE) != 0);
}

INSTR_INLINE
bool
opnd_is_predicate_zero(opnd_t op)
{
    return opnd_is_predicate_reg(op) && ((op.aux.flags & DR_OPND_IS_ZERO_PREDICATE) != 0);
}

#    if defined(X64) || defined(ARM)
#        ifdef X86
#            define OPND_IS_REL_ADDR(op) ((op).kind == REL_ADDR_kind)
#            define opnd_is_rel_addr OPND_IS_REL_ADDR

INSTR_INLINE
bool
opnd_is_near_rel_addr(opnd_t opnd)
{
    return opnd.kind == REL_ADDR_kind IF_X86(&&opnd.aux.segment == DR_REG_NULL);
}

INSTR_INLINE
bool
opnd_is_far_rel_addr(opnd_t opnd)
{
    return IF_X86_ELSE(opnd.kind == REL_ADDR_kind && opnd.aux.segment != DR_REG_NULL,
                       false);
}
#        elif defined(AARCHXX)
#            ifdef ARM
#                define OPND_IS_REL_ADDR(op)       \
                    ((op).kind == REL_ADDR_kind || \
                     (opnd_is_base_disp(op) && opnd_get_base(op) == DR_REG_PC))
#            else
#                define OPND_IS_REL_ADDR(op) ((op).kind == REL_ADDR_kind)
#            endif
#            define opnd_is_rel_addr OPND_IS_REL_ADDR

INSTR_INLINE
bool
opnd_is_near_rel_addr(opnd_t opnd)
{
    return opnd_is_rel_addr(opnd);
}

INSTR_INLINE
bool
opnd_is_far_rel_addr(opnd_t opnd)
{
    return false;
}
#        elif defined(RISCV64)
#            define OPND_IS_REL_ADDR(op) ((op).kind == REL_ADDR_kind)
#            define opnd_is_rel_addr OPND_IS_REL_ADDR

INSTR_INLINE
bool
opnd_is_near_rel_addr(opnd_t opnd)
{
    return opnd_is_rel_addr(opnd);
}

INSTR_INLINE
bool
opnd_is_far_rel_addr(opnd_t opnd)
{
    /* FIXME i#3544: Decide if this should differentiate between JAL and AUIPC+JALR. */
    return false;
}
#        endif /* RISCV64 */
#    endif     /* X64 || ARM */

/* opnd_t constructors */

/* XXX: How can we macro-ify these?  We can use C99 initializers or a copy from
 * a constant, but that implies a full initialization, when we could otherwise
 * partially intialize.  Do we care?
 */
INSTR_INLINE
opnd_t
opnd_create_null(void)
{
    opnd_t opnd;
    opnd.kind = NULL_kind;
    return opnd;
}

INSTR_INLINE
opnd_t
opnd_create_reg(reg_id_t r)
{
    opnd_t opnd DR_IF_DEBUG(= { 0 }); /* FIXME: Needed until i#417 is fixed. */
    CLIENT_ASSERT(r <= DR_REG_LAST_ENUM && r != DR_REG_INVALID,
                  "opnd_create_reg: invalid register");
    opnd.kind = REG_kind;
    opnd.value.reg_and_element_size.reg = r;
    opnd.value.reg_and_element_size.element_size = OPSZ_NA;
    opnd.size = 0; /* indicates full size of reg */
    opnd.aux.flags = 0;
    return opnd;
}

INSTR_INLINE
opnd_t
opnd_create_reg_partial(reg_id_t r, opnd_size_t subsize)
{
    opnd_t opnd DR_IF_DEBUG(= { 0 }); /* FIXME: Needed until i#417 is fixed. */
#    ifdef X86
    CLIENT_ASSERT(subsize == 0 || (r >= DR_REG_MM0 && r <= DR_REG_XMM31) ||
                      (r >= DR_REG_YMM0 && r <= DR_REG_ZMM31),
                  "opnd_create_reg_partial: non-multimedia register");
#    endif
    opnd.kind = REG_kind;
    opnd.value.reg_and_element_size.reg = r;
    opnd.value.reg_and_element_size.element_size = OPSZ_NA;
    opnd.size = subsize;
    opnd.aux.flags = 0;
    return opnd;
}

INSTR_INLINE
opnd_t
opnd_create_reg_element_vector(reg_id_t r, opnd_size_t element_size)
{
    opnd_t opnd DR_IF_DEBUG(= { 0 }); /* FIXME: Needed until i#417 is fixed. */
    CLIENT_ASSERT(element_size == 0 || (r <= DR_REG_LAST_ENUM && r != DR_REG_INVALID),
                  "opnd_create_reg_element_vector: invalid register or no size");
    opnd.kind = REG_kind;
    opnd.value.reg_and_element_size.reg = r;
    opnd.value.reg_and_element_size.element_size = element_size;
    opnd.aux.flags = DR_OPND_IS_VECTOR;
    return opnd;
}

#    ifdef AARCH64
INSTR_INLINE
opnd_t
opnd_create_predicate_reg(reg_id_t r, bool is_merge)
{
    opnd_t opnd DR_IF_DEBUG(= { 0 }); /* FIXME: Needed until i#417 is fixed. */
    CLIENT_ASSERT(r >= DR_REG_P0 && r <= DR_REG_P15,
                  "opnd_create_predicate_reg: invalid predicate register");

    opnd.kind = REG_kind;
    opnd.value.reg_and_element_size.reg = r;
    opnd.aux.flags =
        (ushort)(is_merge ? DR_OPND_IS_MERGE_PREDICATE : DR_OPND_IS_ZERO_PREDICATE);
    return opnd;
}
#    endif

INSTR_INLINE
opnd_t
opnd_create_reg_ex(reg_id_t r, opnd_size_t subsize, dr_opnd_flags_t flags)
{
    opnd_t opnd = opnd_create_reg_partial(r, subsize);
    opnd.aux.flags = (ushort)flags;
    return opnd;
}

INSTR_INLINE
opnd_t
opnd_create_pc(app_pc pc)
{
    opnd_t opnd;
    opnd.kind = PC_kind;
    opnd.value.pc = pc;
    return opnd;
}

/* opnd_t accessors */

#    define OPND_GET_REG(opnd)                                                          \
        (CLIENT_ASSERT_(opnd_is_reg(opnd), "opnd_get_reg called on non-reg opnd")(opnd) \
             .value.reg_and_element_size.reg)
#    define opnd_get_reg OPND_GET_REG

#    if defined(X86)
#        define OPND_GET_FLAGS(opnd)                                                     \
            (CLIENT_ASSERT_(                                                             \
                opnd_is_reg(opnd) || opnd_is_base_disp(opnd) || opnd_is_immed_int(opnd), \
                "opnd_get_flags called on non-reg non-base-disp non-immed-int opnd") 0)
#    elif defined(AARCHXX) || defined(RISCV64)
#        define OPND_GET_FLAGS(opnd)                                                   \
            (CLIENT_ASSERT_(                                                           \
                 opnd_is_reg(opnd) || opnd_is_base_disp(opnd) ||                       \
                     opnd_is_immed_int(opnd),                                          \
                 "opnd_get_flags called on non-reg non-base-disp non-immed-int opnd")( \
                 opnd)                                                                 \
                 .aux.flags)
#    endif
#    define opnd_get_flags OPND_GET_FLAGS

#    define GET_BASE_DISP(opnd)                                                 \
        (CLIENT_ASSERT_(opnd_is_base_disp(opnd),                                \
                        "opnd_get_base_disp called on invalid opnd type")(opnd) \
             .value.base_disp)
#    define OPND_GET_BASE(opnd) (GET_BASE_DISP(opnd).base_reg)
#    define OPND_GET_DISP(opnd) (GET_BASE_DISP(opnd).disp)
#    ifdef X86
#        define OPND_GET_INDEX(opnd)                                \
            (GET_BASE_DISP(opnd).index_reg_is_zmm                   \
                 ? DR_REG_START_ZMM + GET_BASE_DISP(opnd).index_reg \
                 : GET_BASE_DISP(opnd).index_reg)
#        define OPND_GET_SCALE(opnd) (GET_BASE_DISP(opnd).scale)
#    else
#        define OPND_GET_INDEX(opnd) (GET_BASE_DISP(opnd).index_reg)
#        define OPND_GET_SCALE(opnd) 0
#    endif

#    define opnd_get_base OPND_GET_BASE
#    define opnd_get_disp OPND_GET_DISP
#    define opnd_get_index OPND_GET_INDEX
#    define opnd_get_scale OPND_GET_SCALE

#    ifdef X86
#        define OPND_GET_SEGMENT(opnd)                                                  \
            (CLIENT_ASSERT_(opnd_is_base_disp(opnd) IF_X64(|| opnd_is_abs_addr(opnd) || \
                                                           opnd_is_rel_addr(opnd)),     \
                            "opnd_get_segment called on invalid opnd type")(opnd)       \
                 .aux.segment)
#    elif defined(AARCHXX) || defined(RISCV64)
#        define OPND_GET_SEGMENT(opnd) DR_REG_NULL
#    endif
#    define opnd_get_segment OPND_GET_SEGMENT

/* instr_t accessors */

INSTR_INLINE
bool
instr_is_app(instr_t *instr)
{
    CLIENT_ASSERT(instr != NULL, "instr_is_app: passed NULL");
    return ((instr->flags & INSTR_DO_NOT_MANGLE) == 0);
}

INSTR_INLINE
bool
instr_ok_to_mangle(instr_t *instr)
{
    return instr_is_app(instr);
}

INSTR_INLINE
bool
instr_is_meta(instr_t *instr)
{
    CLIENT_ASSERT(instr != NULL, "instr_is_meta: passed NULL");
    return ((instr->flags & INSTR_DO_NOT_MANGLE) != 0);
}

#    ifdef DYNAMORIO_INTERNAL
/* These are hot internally, but unlikely to be used by clients. */
INSTR_INLINE
bool
instr_operands_valid(instr_t *instr)
{
    return TEST(INSTR_OPERANDS_VALID, instr->flags);
}

INSTR_INLINE
bool
instr_raw_bits_valid(instr_t *instr)
{
    return TEST(INSTR_RAW_BITS_VALID, instr->flags);
}

INSTR_INLINE
bool
instr_has_allocated_bits(instr_t *instr)
{
    return TEST(INSTR_RAW_BITS_ALLOCATED, instr->flags);
}

INSTR_INLINE
bool
instr_needs_encoding(instr_t *instr)
{
    return !TEST(INSTR_RAW_BITS_VALID, instr->flags);
}

INSTR_INLINE
bool
instr_ok_to_emit(instr_t *instr)
{
    CLIENT_ASSERT(instr != NULL, "instr_ok_to_emit: passed NULL");
    return ((instr->flags & INSTR_DO_NOT_EMIT) == 0);
}
#    endif

INSTR_INLINE
int
instr_num_srcs(instr_t *instr)
{
    MAKE_OPNDS_VALID(instr);
    return instr->num_srcs;
}

INSTR_INLINE
int
instr_num_dsts(instr_t *instr)
{
    MAKE_OPNDS_VALID(instr);
    return instr->num_dsts;
}

/* src0 is static, rest are dynamic. */
/* FIXME: Double evaluation. */
#    define INSTR_GET_SRC(instr, pos)                        \
        (MAKE_OPNDS_VALID(instr),                            \
         CLIENT_ASSERT_(pos >= 0 && pos < (instr)->num_srcs, \
                        "instr_get_src: ordinal invalid")(   \
             (pos) == 0 ? (instr)->src0 : (instr)->srcs[(pos)-1]))

#    define INSTR_GET_DST(instr, pos)                            \
        (MAKE_OPNDS_VALID(instr),                                \
         CLIENT_ASSERT_(pos >= 0 && pos < (instr)->num_dsts,     \
                        "instr_get_dst: ordinal invalid")(instr) \
             ->dsts[pos])

/* Assumes that if an instr has a jump target, it's stored in the 0th src
 * location.
 */
#    define INSTR_GET_TARGET(instr)                                                 \
        (MAKE_OPNDS_VALID(instr),                                                   \
         CLIENT_ASSERT_(instr_is_cti(instr), "instr_get_target: called on non-cti") \
             CLIENT_ASSERT_((instr)->num_srcs >= 1,                                 \
                            "instr_get_target: instr has no sources")(instr)        \
                 ->src0)

#    define instr_get_src INSTR_GET_SRC
#    define instr_get_dst INSTR_GET_DST
#    define instr_get_target INSTR_GET_TARGET

INSTR_INLINE
void
instr_set_note(instr_t *instr, void *value)
{
    instr->note = value;
}

INSTR_INLINE
void *
instr_get_note(instr_t *instr)
{
    return instr->note;
}

INSTR_INLINE
instr_t *
instr_get_next(instr_t *instr)
{
    return instr->next;
}

INSTR_INLINE
instr_t *
instr_get_next_app(instr_t *instr)
{
    CLIENT_ASSERT(instr != NULL, "instr_get_next_app: passed NULL");
    for (instr = instr->next; instr != NULL; instr = instr->next) {
        if (instr_is_app(instr))
            return instr;
    }
    return NULL;
}

INSTR_INLINE
instr_t *
instr_get_prev(instr_t *instr)
{
    return instr->prev;
}

INSTR_INLINE
instr_t *
instr_get_prev_app(instr_t *instr)
{
    CLIENT_ASSERT(instr != NULL, "instr_get_prev_app: passed NULL");
    for (instr = instr->prev; instr != NULL; instr = instr->prev) {
        if (instr_is_app(instr))
            return instr;
    }
    return NULL;
}

INSTR_INLINE
void
instr_set_next(instr_t *instr, instr_t *next)
{
    instr->next = next;
}

INSTR_INLINE
void
instr_set_prev(instr_t *instr, instr_t *prev)
{
    instr->prev = prev;
}

INSTR_INLINE
instr_t *
instr_from_noalloc(instr_noalloc_t *noalloc)
{
    return &noalloc->instr;
}

#endif /* DR_FAST_IR */

#endif /* _DR_IR_INSTR_INLINE_H_ */
