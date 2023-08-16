/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* file "opnd_shared.c" -- IR opnd utilities */

#include "../globals.h"
#include "opnd.h"
#include "arch.h"
/* FIXME i#1551: refactor this file and avoid this x86-specific include in base arch/ */
#ifndef AARCH64
#    include "x86/decode_private.h"
#endif

#if defined(DEBUG) && !defined(STANDALONE_DECODER)
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
#    undef ASSERT_TRUNCATE
#    undef ASSERT_BITFIELD_TRUNCATE
#    undef ASSERT_NOT_REACHED
#    define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

#undef opnd_is_null
#undef opnd_is_immed_int
#undef opnd_is_immed_float
#undef opnd_is_immed_double
#undef opnd_is_near_pc
#undef opnd_is_near_instr
#undef opnd_is_reg
#undef opnd_is_base_disp
#undef opnd_is_far_pc
#undef opnd_is_far_instr
#undef opnd_is_mem_instr
#undef opnd_is_valid
bool
opnd_is_null(opnd_t op)
{
    return OPND_IS_NULL(op);
}
bool
opnd_is_immed_int(opnd_t op)
{
    return OPND_IS_IMMED_INT(op);
}
bool
opnd_is_immed_float(opnd_t op)
{
    return OPND_IS_IMMED_FLOAT(op);
}
bool
opnd_is_immed_double(opnd_t op)
{
    return OPND_IS_IMMED_DOUBLE(op);
}
bool
opnd_is_near_pc(opnd_t op)
{
    return OPND_IS_NEAR_PC(op);
}
bool
opnd_is_near_instr(opnd_t op)
{
    return OPND_IS_NEAR_INSTR(op);
}
bool
opnd_is_reg(opnd_t op)
{
    return OPND_IS_REG(op);
}
bool
opnd_is_base_disp(opnd_t op)
{
    return OPND_IS_BASE_DISP(op);
}
bool
opnd_is_far_pc(opnd_t op)
{
    return OPND_IS_FAR_PC(op);
}
bool
opnd_is_far_instr(opnd_t op)
{
    return OPND_IS_FAR_INSTR(op);
}
bool
opnd_is_mem_instr(opnd_t op)
{
    return OPND_IS_MEM_INSTR(op);
}
bool
opnd_is_valid(opnd_t op)
{
    return OPND_IS_VALID(op);
}
#define opnd_is_null OPND_IS_NULL
#define opnd_is_immed_int OPND_IS_IMMED_INT
#define opnd_is_immed_float OPND_IS_IMMED_FLOAT
#define opnd_is_immed_double OPND_IS_IMMED_DOUBLE
#define opnd_is_near_pc OPND_IS_NEAR_PC
#define opnd_is_near_instr OPND_IS_NEAR_INSTR
#define opnd_is_reg OPND_IS_REG
#define opnd_is_base_disp OPND_IS_BASE_DISP
#define opnd_is_far_pc OPND_IS_FAR_PC
#define opnd_is_far_instr OPND_IS_FAR_INSTR
#define opnd_is_mem_instr OPND_IS_MEM_INSTR
#define opnd_is_valid OPND_IS_VALID

#if defined(X64) || defined(ARM)
#    undef opnd_is_rel_addr
bool
opnd_is_rel_addr(opnd_t op)
{
    return OPND_IS_REL_ADDR(op);
}
#    define opnd_is_rel_addr OPND_IS_REL_ADDR
#endif

/* We allow overlap between ABS_ADDR_kind and BASE_DISP_kind w/ no base or index */
bool
opnd_is_abs_base_disp(opnd_t opnd)
{
    return (opnd_is_base_disp(opnd) && opnd_get_base(opnd) == REG_NULL &&
            opnd_get_index(opnd) == REG_NULL);
}
bool
opnd_is_abs_addr(opnd_t opnd)
{
    return IF_X64(opnd.kind == ABS_ADDR_kind ||) opnd_is_abs_base_disp(opnd);
}
bool
opnd_is_near_abs_addr(opnd_t opnd)
{
    return opnd_is_abs_addr(opnd) IF_X86(&&opnd.aux.segment == REG_NULL);
}
bool
opnd_is_far_abs_addr(opnd_t opnd)
{
    return IF_X86_ELSE(opnd_is_abs_addr(opnd) && opnd.aux.segment != REG_NULL, false);
}

bool
opnd_is_vsib(opnd_t op)
{
    return (opnd_is_base_disp(op) &&
            (reg_is_strictly_xmm(opnd_get_index(op)) ||
             reg_is_strictly_ymm(opnd_get_index(op)) ||
             reg_is_strictly_zmm(opnd_get_index(op))));
}

bool
opnd_is_reg_32bit(opnd_t opnd)
{
    if (opnd_is_reg(opnd))
        return reg_is_32bit(opnd_get_reg(opnd));
    return false;
}

bool
reg_is_32bit(reg_id_t reg)
{
    return (reg >= REG_START_32 && reg <= REG_STOP_32);
}

#if defined(X86) || defined(AARCH64)
bool
opnd_is_reg_64bit(opnd_t opnd)
{
    if (opnd_is_reg(opnd))
        return reg_is_64bit(opnd_get_reg(opnd));
    return false;
}

bool
reg_is_64bit(reg_id_t reg)
{
    return (reg >= REG_START_64 && reg <= REG_STOP_64);
}
#endif /* !ARM */

bool
opnd_is_reg_pointer_sized(opnd_t opnd)
{
    if (opnd_is_reg(opnd))
        return reg_is_pointer_sized(opnd_get_reg(opnd));
    return false;
}

bool
opnd_is_reg_partial(opnd_t opnd)
{
    return (opnd_is_reg(opnd) && opnd.size != 0 &&
            opnd_get_size(opnd) != reg_get_size(opnd_get_reg(opnd)));
}

bool
reg_is_pointer_sized(reg_id_t reg)
{
#ifdef X64
    return (reg >= REG_START_64 && reg <= REG_STOP_64);
#else
    return (reg >= REG_START_32 && reg <= REG_STOP_32);
#endif
}

#undef opnd_get_reg
reg_id_t
opnd_get_reg(opnd_t opnd)
{
    return OPND_GET_REG(opnd);
}
#define opnd_get_reg OPND_GET_REG

#undef opnd_get_flags
dr_opnd_flags_t
opnd_get_flags(opnd_t opnd)
{
    return OPND_GET_FLAGS(opnd);
}
#define opnd_get_flags OPND_GET_FLAGS

void
opnd_set_flags(opnd_t *opnd, dr_opnd_flags_t flags)
{
    CLIENT_ASSERT(opnd_is_reg(*opnd) || opnd_is_base_disp(*opnd) ||
                      opnd_is_immed_int(*opnd),
                  "opnd_set_flags called on non-reg non-base-disp non-immed-int opnd");
    opnd->aux.flags = flags;
}

opnd_t
opnd_add_flags(opnd_t opnd, dr_opnd_flags_t flags)
{
    opnd_set_flags(&opnd, flags | opnd.aux.flags);
    return opnd;
}

opnd_size_t
opnd_get_size(opnd_t opnd)
{
    switch (opnd.kind) {
    case REG_kind: return (opnd.size == 0 ? reg_get_size(opnd_get_reg(opnd)) : opnd.size);
    case IMMED_INTEGER_kind:
    case IMMED_FLOAT_kind:
    case IMMED_DOUBLE_kind:
    case BASE_DISP_kind:
#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
#endif
#ifdef X64
    case ABS_ADDR_kind:
#endif
    case MEM_INSTR_kind:
    case INSTR_kind: return opnd.size;
    case PC_kind: return OPSZ_PTR;
    case FAR_PC_kind:
    case FAR_INSTR_kind: return OPSZ_6_irex10_short4;
    case NULL_kind: return OPSZ_NA;
    default: CLIENT_ASSERT(false, "opnd_get_size: unknown opnd type"); return OPSZ_NA;
    }
}

void
opnd_set_size(opnd_t *opnd, opnd_size_t newsize)
{
    switch (opnd->kind) {
    case IMMED_INTEGER_kind:
    case BASE_DISP_kind:
#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
#endif
#ifdef X64
    case ABS_ADDR_kind:
#endif
    case REG_kind:
    case MEM_INSTR_kind:
    case INSTR_kind: opnd->size = newsize; return;
    default: CLIENT_ASSERT(false, "opnd_set_size: unknown opnd type");
    }
}

#if defined(AARCH64)
/* Possible values of opnd.value.base_disp.element_size. */
enum {
    ELEMENT_SIZE_SINGLE = 0,
    ELEMENT_SIZE_DOUBLE = 1,
};
#endif

opnd_size_t
opnd_get_vector_element_size(opnd_t opnd)
{
    if (!TEST(DR_OPND_IS_VECTOR, opnd.aux.flags))
        return OPSZ_NA;

    switch (opnd.kind) {
    case REG_kind: return opnd.value.reg_and_element_size.element_size;
#if defined(AARCH64)
    case BASE_DISP_kind:
        switch (opnd.value.base_disp.element_size) {
        case ELEMENT_SIZE_SINGLE: return OPSZ_4;
        case ELEMENT_SIZE_DOUBLE: return OPSZ_8;
        }
#endif
    default: return OPSZ_NA;
    }
}

/* immediate operands */

#if defined(DEBUG) && !defined(STANDALONE_DECODER)
static void
opnd_check_immed_size(int64 i, opnd_size_t size)
{
    uint sz = opnd_size_in_bytes(size);
    if (sz == 1) {
        CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_sbyte(i) || CHECK_TRUNCATE_TYPE_byte(i),
                      "opnd_create_immed_int: value too large for 8-bit size");
    } else if (sz == 2) {
        CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_short(i) || CHECK_TRUNCATE_TYPE_ushort(i),
                      "opnd_create_immed_int: value too large for 16-bit size");
    } else if (sz == 4) {
        CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_int(i) || CHECK_TRUNCATE_TYPE_uint(i),
                      "opnd_create_immed_int: value too large for 32-bit size");
    }
}
#endif

opnd_t
opnd_create_immed_int(ptr_int_t i, opnd_size_t size)
{
    opnd_t opnd;
    opnd.kind = IMMED_INTEGER_kind;
    CLIENT_ASSERT(size < OPSZ_LAST_ENUM, "opnd_create_immed_int: invalid size");
    opnd.size = size;
    opnd.value.immed_int = i;
    opnd.aux.flags = 0;
    DOCHECK(1, { opnd_check_immed_size(i, size); });
    return opnd;
}

opnd_t
opnd_create_immed_uint(ptr_uint_t i, opnd_size_t size)
{
    opnd_t opnd;
    opnd.kind = IMMED_INTEGER_kind;
    CLIENT_ASSERT(size < OPSZ_LAST_ENUM, "opnd_create_immed_uint: invalid size");
    opnd.size = size;
    opnd.value.immed_int = (ptr_int_t)i;
    opnd.aux.flags = 0;
    DOCHECK(1, { opnd_check_immed_size(i, size); });
    return opnd;
}

opnd_t
opnd_create_immed_int64(int64 i, opnd_size_t size)
{
    opnd_t opnd;
    opnd.kind = IMMED_INTEGER_kind;
    IF_X64(CLIENT_ASSERT(false, "32-bit only"));
    CLIENT_ASSERT(size < OPSZ_LAST_ENUM, "opnd_create_immed_uint: invalid size");
    opnd.size = size;
    opnd.value.immed_int_multi_part.low = (uint)i;
    opnd.value.immed_int_multi_part.high = (uint)((uint64)i >> 32);
    opnd.aux.flags = DR_OPND_MULTI_PART;
    DOCHECK(1, { opnd_check_immed_size(i, size); });
    return opnd;
}

opnd_t
opnd_invert_immed_int(opnd_t opnd)
{
    CLIENT_ASSERT(opnd.kind == IMMED_INTEGER_kind, "opnd_invert_immed_int: invalid kind");

    const int bit_size = opnd_size_in_bits(opnd.size);
    const uint64 mask =
        (bit_size < 64) ? ((uint64)1 << opnd_size_in_bits(opnd.size)) - 1 : ~((uint64)0);
    if (opnd.aux.flags & DR_OPND_MULTI_PART) {
        opnd.value.immed_int_multi_part.low &= mask;
        opnd.value.immed_int_multi_part.high &= mask >> 32;
    } else {
        opnd.value.immed_int = ~opnd.value.immed_int & mask;
    }

    return opnd;
}

bool
opnd_is_immed_int64(opnd_t opnd)
{
    return (opnd_is_immed_int(opnd) && TEST(DR_OPND_MULTI_PART, opnd_get_flags(opnd)));
}

/* NOTE: requires caller to be under PRESERVE_FLOATING_POINT_STATE */
opnd_t
opnd_create_immed_float(float i)
{
    opnd_t opnd;
    opnd.kind = IMMED_FLOAT_kind;
    /* Note that manipulating floats and doubles by copying in this way can
     * result in using FP load/store instructions which can trigger any pending
     * FP exception (i#386).
     */
    opnd.value.immed_float = i;
    /* currently only used for implicit constants that have no size */
    opnd.size = OPSZ_0;
    return opnd;
}

#ifndef WINDOWS
/* XXX i#4488: x87 floating point immediates should be double precision.
 * Type double currently not included for Windows because sizeof(opnd_t) does
 * not equal EXPECTED_SIZEOF_OPND, triggering the ASSERT in d_r_arch_init().
 */
/* NOTE: requires caller to be under PRESERVE_FLOATING_POINT_STATE */
opnd_t
opnd_create_immed_double(double i)
{
    opnd_t opnd;
    opnd.kind = IMMED_DOUBLE_kind;
    /* Note that manipulating floats and doubles by copying in this way can
     * result in using FP load/store instructions which can trigger any pending
     * FP exception (i#386).
     */
    opnd.value.immed_double = i;
    /* currently only used for implicit constants that have no size */
    opnd.size = OPSZ_0;
    return opnd;
}
#endif

#ifdef AARCH64
opnd_t
opnd_create_immed_pred_constr(dr_pred_constr_type_t p)
{
    opnd_t opnd;
    opnd.kind = IMMED_INTEGER_kind;
    opnd.aux.flags = DR_OPND_IS_PREDICATE_CONSTRAINT;
    opnd.value.immed_int = p;
    /* all predicate constraints have 5 bits*/
    opnd.size = OPSZ_5b;
    return opnd;
}
#endif

opnd_t
opnd_create_immed_float_for_opcode(uint opcode)
{
    opnd_t opnd;
    uint float_value;
    opnd.kind = IMMED_FLOAT_kind;
    /* avoid any fp instrs (xref i#386) */
    float_value = opnd_immed_float_arch(opcode);
    *(uint *)(&opnd.value.immed_float) = float_value;
    /* currently only used for implicit constants that have no size */
    opnd.size = OPSZ_0;
    return opnd;
}

ptr_int_t
opnd_get_immed_int(opnd_t opnd)
{
    CLIENT_ASSERT(opnd_is_immed_int(opnd), "opnd_get_immed_int called on non-immed-int");
    return opnd.value.immed_int;
}

int64
opnd_get_immed_int64(opnd_t opnd)
{
    IF_X64(CLIENT_ASSERT(false, "32-bit only"));
    CLIENT_ASSERT(opnd_is_immed_int64(opnd),
                  "opnd_get_immed_int64 called on non-multi-part-immed-int");
    return (((uint64)(uint)opnd.value.immed_int_multi_part.high) << 32) |
        (uint64)(uint)opnd.value.immed_int_multi_part.low;
}

/* NOTE: requires caller to be under PRESERVE_FLOATING_POINT_STATE */
float
opnd_get_immed_float(opnd_t opnd)
{
    CLIENT_ASSERT(opnd_is_immed_float(opnd),
                  "opnd_get_immed_float called on non-immed-float");
    /* note that manipulating floats is dangerous - see case 4360
     * this return shouldn't require any fp state, though
     */
    return opnd.value.immed_float;
}

#ifndef WINDOWS
/* XXX i#4488: x87 floating point immediates should be double precision.
 * Type double currently not included for Windows because sizeof(opnd_t) does
 * not equal EXPECTED_SIZEOF_OPND, triggering the ASSERT in d_r_arch_init().
 */
double
opnd_get_immed_double(opnd_t opnd)
{
    CLIENT_ASSERT(opnd_is_immed_double(opnd),
                  "opnd_get_immed_double called on non-immed-float");
    return opnd.value.immed_double;
}
#endif

/* address operands */

/* N.B.: seg_selector is a segment selector, not a SEG_ constant */
opnd_t
opnd_create_far_pc(ushort seg_selector, app_pc pc)
{
    opnd_t opnd;
    opnd.kind = FAR_PC_kind;
    opnd.aux.far_pc_seg_selector = seg_selector;
    opnd.value.pc = pc;
    return opnd;
}

opnd_t
opnd_create_instr_ex(instr_t *instr, opnd_size_t size, ushort shift)
{
    opnd_t opnd;
    opnd.kind = INSTR_kind;
    opnd.value.instr = instr;
    opnd.aux.shift = shift;
    opnd.size = size;
    return opnd;
}

opnd_t
opnd_create_instr(instr_t *instr)
{
    return opnd_create_instr_ex(instr, OPSZ_PTR, 0);
}

opnd_t
opnd_create_far_instr(ushort seg_selector, instr_t *instr)
{
    opnd_t opnd;
    opnd.kind = FAR_INSTR_kind;
    opnd.aux.far_pc_seg_selector = seg_selector;
    opnd.value.instr = instr;
    return opnd;
}

DR_API
opnd_t
opnd_create_mem_instr(instr_t *instr, short disp, opnd_size_t data_size)
{
    opnd_t opnd;
    opnd.kind = MEM_INSTR_kind;
    opnd.size = data_size;
    opnd.aux.disp = disp;
    opnd.value.instr = instr;
    return opnd;
}

app_pc
opnd_get_pc(opnd_t opnd)
{
    if (opnd_is_pc(opnd))
        return opnd.value.pc;
    else {
        SYSLOG_INTERNAL_ERROR("opnd type is %d", opnd.kind);
        CLIENT_ASSERT(false, "opnd_get_pc called on non-pc");
        return NULL;
    }
}

ushort
opnd_get_segment_selector(opnd_t opnd)
{
    if (opnd_is_far_pc(opnd) || opnd_is_far_instr(opnd)) {
        return opnd.aux.far_pc_seg_selector;
    }
    CLIENT_ASSERT(false, "opnd_get_segment_selector called on invalid opnd type");
    return REG_INVALID;
}

instr_t *
opnd_get_instr(opnd_t opnd)
{
    CLIENT_ASSERT(opnd_is_instr(opnd) || opnd_is_mem_instr(opnd),
                  "opnd_get_instr called on non-instr");
    return opnd.value.instr;
}

DR_API
ushort
opnd_get_shift(opnd_t opnd)
{
    CLIENT_ASSERT(opnd_is_near_instr(opnd), "opnd_get_shift called on non-near-instr");
    return opnd.aux.shift;
}

short
opnd_get_mem_instr_disp(opnd_t opnd)
{
    CLIENT_ASSERT(opnd_is_mem_instr(opnd),
                  "opnd_get_mem_instr_disp called on non-mem-instr");
    return opnd.aux.disp;
}

/* Base+displacement+scaled index operands */

opnd_t
opnd_create_base_disp_ex(reg_id_t base_reg, reg_id_t index_reg, int scale, int disp,
                         opnd_size_t size, bool encode_zero_disp, bool force_full_disp,
                         bool disp_short_addr)
{
    return opnd_create_far_base_disp_ex(REG_NULL, base_reg, index_reg, scale, disp, size,
                                        encode_zero_disp, force_full_disp,
                                        disp_short_addr);
}

opnd_t
opnd_create_base_disp(reg_id_t base_reg, reg_id_t index_reg, int scale, int disp,
                      opnd_size_t size)
{
    return opnd_create_far_base_disp_ex(REG_NULL, base_reg, index_reg, scale, disp, size,
                                        false, false, false);
}

static inline void
opnd_set_disp_helper(opnd_t *opnd, int disp)
{
    IF_ARM_ELSE(
        {
            if (disp < 0) {
                opnd->aux.flags |= DR_OPND_NEGATED;
                opnd->value.base_disp.disp = -disp;
            } else
                opnd->value.base_disp.disp = disp;
        },
        { opnd->value.base_disp.disp = disp; });
}

opnd_t
opnd_create_far_base_disp_ex(reg_id_t seg, reg_id_t base_reg, reg_id_t index_reg,
                             int scale, int disp, opnd_size_t size, bool encode_zero_disp,
                             bool force_full_disp, bool disp_short_addr)
{
    opnd_t opnd;
    opnd.kind = BASE_DISP_kind;
    CLIENT_ASSERT(size < OPSZ_LAST_ENUM, "opnd_create_*base_disp*: invalid size");
    opnd.size = size;
    CLIENT_ASSERT(scale == 0 || scale == 1 || scale == 2 || scale == 4 || scale == 8,
                  "opnd_create_*base_disp*: invalid scale");
    IF_X86(CLIENT_ASSERT(index_reg == REG_NULL || scale > 0,
                         "opnd_create_*base_disp*: index requires scale"));
    CLIENT_ASSERT(
        seg == REG_NULL IF_X86(|| (seg >= REG_START_SEGMENT && seg <= REG_STOP_SEGMENT)),
        "opnd_create_*base_disp*: invalid segment");
    CLIENT_ASSERT(base_reg <= REG_LAST_ENUM, "opnd_create_*base_disp*: invalid base");
    CLIENT_ASSERT(index_reg <= REG_LAST_ENUM, "opnd_create_*base_disp*: invalid index");
    CLIENT_ASSERT_BITFIELD_TRUNCATE(SCALE_SPECIFIER_BITS, scale,
                                    "opnd_create_*base_disp*: invalid scale");
    /* reg_id_t is now a ushort, but we can only accept low values */
    CLIENT_ASSERT_BITFIELD_TRUNCATE(REG_SPECIFIER_BITS, base_reg,
                                    "opnd_create_*base_disp*: invalid base");
    CLIENT_ASSERT_BITFIELD_TRUNCATE(REG_SPECIFIER_BITS, index_reg,
                                    "opnd_create_*base_disp*: invalid index");
    IF_X86_ELSE({ opnd.aux.segment = seg; },
                {
                    opnd.aux.flags = 0;
                    CLIENT_ASSERT(
                        disp == 0 || index_reg == REG_NULL,
                        "opnd_create_*base_disp*: cannot have both disp and index");
                });
    opnd_set_disp_helper(&opnd, disp);
    opnd.value.base_disp.base_reg = base_reg;
#ifdef X86
    if (reg_is_strictly_zmm(index_reg)) {
        opnd.value.base_disp.index_reg = index_reg - DR_REG_START_ZMM;
        opnd.value.base_disp.index_reg_is_zmm = 1;
    } else {
        opnd.value.base_disp.index_reg = index_reg;
        opnd.value.base_disp.index_reg_is_zmm = 0;
    }
#else
    opnd.value.base_disp.index_reg = index_reg;
#endif
#if defined(ARM)
    if (scale > 1) {
        opnd.value.base_disp.shift_type = DR_SHIFT_LSL;
        opnd.value.base_disp.shift_amount_minus_1 =
            /* we store the amount minus one */
            (scale == 2 ? 0 : (scale == 4 ? 1 : 2));
    } else {
        opnd.value.base_disp.shift_type = DR_SHIFT_NONE;
        opnd.value.base_disp.shift_amount_minus_1 = 0;
    }
#elif defined(AARCH64)
    opnd.value.base_disp.pre_index = true;
    opnd.value.base_disp.extend_type = DR_EXTEND_UXTX;
    opnd.value.base_disp.scaled = false;
#elif defined(X86)
    opnd.value.base_disp.scale = (byte)scale;
    opnd.value.base_disp.encode_zero_disp = (byte)encode_zero_disp;
    opnd.value.base_disp.force_full_disp = (byte)force_full_disp;
    opnd.value.base_disp.disp_short_addr = (byte)disp_short_addr;
#endif
    return opnd;
}

opnd_t
opnd_create_far_base_disp(reg_id_t seg, reg_id_t base_reg, reg_id_t index_reg, int scale,
                          int disp, opnd_size_t size)
{
    return opnd_create_far_base_disp_ex(seg, base_reg, index_reg, scale, disp, size,
                                        false, false, false);
}

#ifdef ARM
opnd_t
opnd_create_base_disp_arm(reg_id_t base_reg, reg_id_t index_reg,
                          dr_shift_type_t shift_type, uint shift_amount, int disp,
                          dr_opnd_flags_t flags, opnd_size_t size)
{
    opnd_t opnd;
    opnd.kind = BASE_DISP_kind;
    CLIENT_ASSERT(size < OPSZ_LAST_ENUM, "opnd_create_*base_disp*: invalid size");
    opnd.size = size;
    CLIENT_ASSERT(disp == 0 || index_reg == REG_NULL,
                  "opnd_create_base_disp_arm: cannot have both disp and index");
    CLIENT_ASSERT(base_reg <= REG_LAST_ENUM, "opnd_create_base_disp_arm: invalid base");
    CLIENT_ASSERT(index_reg <= REG_LAST_ENUM, "opnd_create_base_disp_arm: invalid index");
    /* reg_id_t is now a ushort, but we can only accept low values */
    CLIENT_ASSERT_BITFIELD_TRUNCATE(REG_SPECIFIER_BITS, base_reg,
                                    "opnd_create_base_disp_arm: invalid base");
    CLIENT_ASSERT_BITFIELD_TRUNCATE(REG_SPECIFIER_BITS, index_reg,
                                    "opnd_create_base_disp_arm: invalid index");
    opnd.value.base_disp.base_reg = base_reg;
    opnd.value.base_disp.index_reg = index_reg;
    opnd_set_disp_helper(&opnd, disp);
    /* Set the flags before the shift as the shift will change the flags */
    opnd.aux.flags = flags;
    if (!opnd_set_index_shift(&opnd, shift_type, shift_amount))
        CLIENT_ASSERT(false, "opnd_create_base_disp_arm: invalid shift type/amount");
    return opnd;
}
#endif

#ifdef AARCH64

opnd_t
opnd_create_base_disp_aarch64_common(reg_id_t base_reg, reg_id_t index_reg,
                                     byte element_size, dr_extend_type_t extend_type,
                                     bool scaled, int disp, dr_opnd_flags_t flags,
                                     opnd_size_t size, uint shift)

{
    opnd_t opnd;
    opnd.kind = BASE_DISP_kind;
    CLIENT_ASSERT(size < OPSZ_LAST_ENUM, "opnd_create_*base_disp*: invalid size");
    opnd.size = size;
    CLIENT_ASSERT(disp == 0 || index_reg == REG_NULL,
                  "opnd_create_base_disp_aarch64: cannot have both disp and index");
    CLIENT_ASSERT(base_reg <= REG_LAST_ENUM,
                  "opnd_create_base_disp_aarch64: invalid base");
    CLIENT_ASSERT(index_reg <= REG_LAST_ENUM,
                  "opnd_create_base_disp_aarch64: invalid index");
    /* reg_id_t is now a ushort, but we can only accept low values */
    CLIENT_ASSERT_BITFIELD_TRUNCATE(REG_SPECIFIER_BITS, base_reg,
                                    "opnd_create_base_disp_aarch64: invalid base");
    CLIENT_ASSERT_BITFIELD_TRUNCATE(REG_SPECIFIER_BITS, index_reg,
                                    "opnd_create_base_disp_aarch64: invalid index");
    opnd.value.base_disp.base_reg = base_reg;
    opnd.value.base_disp.index_reg = index_reg;
    opnd.value.base_disp.pre_index = false;
    opnd.value.base_disp.element_size = element_size;
    opnd_set_disp_helper(&opnd, disp);
    opnd.aux.flags = flags;
    if (!opnd_set_index_extend_value(&opnd, extend_type, scaled, shift))
        CLIENT_ASSERT(false, "opnd_create_base_disp_aarch64: invalid extend type");
    return opnd;
}

opnd_t
opnd_create_vector_base_disp_aarch64(reg_id_t base_reg, reg_id_t index_reg,
                                     opnd_size_t element_size,
                                     dr_extend_type_t extend_type, bool scaled, int disp,
                                     dr_opnd_flags_t flags, opnd_size_t size, uint shift)
{
    byte internal_element_size = 0;
    switch (element_size) {
    case OPSZ_4: internal_element_size = ELEMENT_SIZE_SINGLE; break;
    case OPSZ_8: internal_element_size = ELEMENT_SIZE_DOUBLE; break;
    default:
        CLIENT_ASSERT(false,
                      "opnd_create_vector_base_disp_aarch64: invalid element size");
    }

    CLIENT_ASSERT(reg_is_z(base_reg) || reg_is_z(index_reg),
                  "opnd_create_vector_base_disp_aarch64: at least one of the base "
                  "register and index register must be a vector register");

    flags |= DR_OPND_IS_VECTOR;

    return opnd_create_base_disp_aarch64_common(base_reg, index_reg,
                                                internal_element_size, extend_type,
                                                scaled, disp, flags, size, shift);
}

opnd_t
opnd_create_base_disp_shift_aarch64(reg_id_t base_reg, reg_id_t index_reg,
                                    dr_extend_type_t extend_type, bool scaled, int disp,
                                    dr_opnd_flags_t flags, opnd_size_t size, uint shift)
{
    return opnd_create_base_disp_aarch64_common(base_reg, index_reg, 0, extend_type,
                                                scaled, disp, flags, size, shift);
}

opnd_t
opnd_create_base_disp_aarch64(reg_id_t base_reg, reg_id_t index_reg,
                              dr_extend_type_t extend_type, bool scaled, int disp,
                              dr_opnd_flags_t flags, opnd_size_t size)
{
    const uint shift = scaled ? opnd_size_to_shift_amount(size) : 0;
    return opnd_create_base_disp_aarch64_common(base_reg, index_reg, 0, extend_type,
                                                scaled, disp, flags, size, shift);
}
#endif

#undef opnd_get_base
#undef opnd_get_disp
#undef opnd_get_index
#undef opnd_get_scale
#undef opnd_get_segment
reg_id_t
opnd_get_base(opnd_t opnd)
{
    return OPND_GET_BASE(opnd);
}
int
opnd_get_disp(opnd_t opnd)
{
    return OPND_GET_DISP(opnd);
}
reg_id_t
opnd_get_index(opnd_t opnd)
{
    return OPND_GET_INDEX(opnd);
}
int
opnd_get_scale(opnd_t opnd)
{
    return OPND_GET_SCALE(opnd);
}
reg_id_t
opnd_get_segment(opnd_t opnd)
{
    return OPND_GET_SEGMENT(opnd);
}
#define opnd_get_base OPND_GET_BASE
#define opnd_get_disp OPND_GET_DISP
#define opnd_get_index OPND_GET_INDEX
#define opnd_get_scale OPND_GET_SCALE
#define opnd_get_segment OPND_GET_SEGMENT

#ifdef ARM
dr_shift_type_t
opnd_get_index_shift(opnd_t opnd, uint *amount OUT)
{
    if (amount != NULL)
        *amount = 0;
    if (!opnd_is_base_disp(opnd)) {
        CLIENT_ASSERT(false, "opnd_get_index_shift called on invalid opnd type");
        return DR_SHIFT_NONE;
    }
    if (amount != NULL && opnd.value.base_disp.shift_type != DR_SHIFT_NONE)
        *amount = opnd.value.base_disp.shift_amount_minus_1 + 1;
    return opnd.value.base_disp.shift_type;
}

bool
opnd_set_index_shift(opnd_t *opnd, dr_shift_type_t shift, uint amount)
{
    if (!opnd_is_base_disp(*opnd)) {
        CLIENT_ASSERT(false, "opnd_set_index_shift called on invalid opnd type");
        return false;
    }
    switch (shift) {
    case DR_SHIFT_NONE:
        if (amount != 0) {
            /* Called from opnd_create_base_disp_arm() so we have a generic msg */
            CLIENT_ASSERT(false, "opnd index shift: invalid shift amount");
            return false;
        }
        opnd->value.base_disp.shift_amount_minus_1 = 0; /* so opnd_same matches */
        break;
    case DR_SHIFT_LSL:
    case DR_SHIFT_ROR:
        /* XXX: T32 only allows shift value [1, 3] */
        if (amount < 1 || amount > 31) {
            CLIENT_ASSERT(false, "opnd  index shift: invalid shift amount");
            return false;
        }
        opnd->value.base_disp.shift_amount_minus_1 = (byte)amount - 1;
        break;
    case DR_SHIFT_LSR:
    case DR_SHIFT_ASR:
        if (amount < 1 || amount > 32) {
            CLIENT_ASSERT(false, "opnd index shift: invalid shift amount");
            return false;
        }
        opnd->value.base_disp.shift_amount_minus_1 = (byte)amount - 1;
        break;
    case DR_SHIFT_RRX:
        if (amount != 1) {
            CLIENT_ASSERT(false, "opnd index shift: invalid shift amount");
            return false;
        }
        opnd->value.base_disp.shift_amount_minus_1 = (byte)amount - 1;
        break;
    default: CLIENT_ASSERT(false, "opnd index shift: invalid shift type"); return false;
    }
    if (shift == DR_SHIFT_NONE)
        opnd->aux.flags &= ~DR_OPND_SHIFTED;
    else
        opnd->aux.flags |= DR_OPND_SHIFTED;
    opnd->value.base_disp.shift_type = shift;
    return true;
}
#endif /* ARM */

#ifdef AARCH64
uint
opnd_size_to_shift_amount(opnd_size_t size)
{
    switch (size) {
    default:
        ASSERT(false);
        /* fall-through */
    case OPSZ_1: return 0;
    case OPSZ_2: return 1;
    case OPSZ_4: return 2;
    case OPSZ_0: /* fall-through */
    case OPSZ_8: return 3;
    case OPSZ_16: return 4;
    case OPSZ_32: return 5;
    case OPSZ_64: return 6;
    }
}

dr_extend_type_t
opnd_get_index_extend(opnd_t opnd, OUT bool *scaled, OUT uint *amount)
{
    dr_extend_type_t extend = DR_EXTEND_UXTX;
    bool scaled_out = false;
    uint amount_out = 0;
    if (!opnd_is_base_disp(opnd))
        CLIENT_ASSERT(false, "opnd_get_index_shift called on invalid opnd type");
    else {
        extend = opnd.value.base_disp.extend_type;
        scaled_out = opnd.value.base_disp.scaled;
        if (scaled_out)
            amount_out = opnd.value.base_disp.scaled_value;
    }
    if (scaled != NULL)
        *scaled = scaled_out;
    if (amount != NULL)
        *amount = amount_out;
    return extend;
}

bool
opnd_set_index_extend_value(opnd_t *opnd, dr_extend_type_t extend, bool scaled,
                            uint scaled_value)
{
    if (!opnd_is_base_disp(*opnd)) {
        CLIENT_ASSERT(false, "opnd_set_index_shift called on invalid opnd type");
        return false;
    }
    if (extend > 7) {
        CLIENT_ASSERT(false, "opnd index extend: invalid extend type");
        return false;
    }
    if (scaled_value > 7) {
        CLIENT_ASSERT(false, "opnd index extend: invalid scaled value");
        return false;
    }
    opnd->value.base_disp.extend_type = extend;
    opnd->value.base_disp.scaled = scaled;
    opnd->value.base_disp.scaled_value = scaled_value;
    return true;
}

bool
opnd_set_index_extend(opnd_t *opnd, dr_extend_type_t extend, bool scaled)
{
    const uint value = scaled ? opnd_size_to_shift_amount(opnd_get_size(*opnd)) : 0;
    return opnd_set_index_extend_value(opnd, extend, scaled, value);
}
#endif /* AARCH64 */

bool
opnd_is_disp_encode_zero(opnd_t opnd)
{
    if (opnd_is_base_disp(opnd))
        return IF_X86_ELSE(opnd.value.base_disp.encode_zero_disp, false);
    CLIENT_ASSERT(false, "opnd_is_disp_encode_zero called on invalid opnd type");
    return false;
}

bool
opnd_is_disp_force_full(opnd_t opnd)
{
    if (opnd_is_base_disp(opnd))
        return IF_X86_ELSE(opnd.value.base_disp.force_full_disp, false);
    CLIENT_ASSERT(false, "opnd_is_disp_force_full called on invalid opnd type");
    return false;
}

bool
opnd_is_disp_short_addr(opnd_t opnd)
{
    if (opnd_is_base_disp(opnd))
        return IF_X86_ELSE(opnd.value.base_disp.disp_short_addr, false);
    CLIENT_ASSERT(false, "opnd_is_disp_short_addr called on invalid opnd type");
    return false;
}

void
opnd_set_disp(opnd_t *opnd, int disp)
{
    if (opnd_is_base_disp(*opnd))
        opnd_set_disp_helper(opnd, disp);
    else
        CLIENT_ASSERT(false, "opnd_set_disp called on invalid opnd type");
}

#ifdef X86
void
opnd_set_disp_ex(opnd_t *opnd, int disp, bool encode_zero_disp, bool force_full_disp,
                 bool disp_short_addr)
{
    if (opnd_is_base_disp(*opnd)) {
        opnd->value.base_disp.encode_zero_disp = (byte)encode_zero_disp;
        opnd->value.base_disp.force_full_disp = (byte)force_full_disp;
        opnd->value.base_disp.disp_short_addr = (byte)disp_short_addr;
        opnd_set_disp_helper(opnd, disp);
    } else
        CLIENT_ASSERT(false, "opnd_set_disp_ex called on invalid opnd type");
}
#endif

opnd_t
opnd_create_abs_addr(void *addr, opnd_size_t data_size)
{
    return opnd_create_far_abs_addr(REG_NULL, addr, data_size);
}

opnd_t
opnd_create_far_abs_addr(reg_id_t seg, void *addr, opnd_size_t data_size)
{
    /* PR 253327: For x64, there's no way to create 0xa0-0xa3 w/ addr
     * prefix since we'll make a base-disp instead: but our IR is
     * supposed to be at a higher abstraction level anyway, though w/
     * the sib byte the base-disp ends up being one byte longer.
     */
    if (IF_X64_ELSE((ptr_uint_t)addr <= UINT_MAX, true)) {
        bool need_addr32 = false;
        CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_uint((ptr_uint_t)addr),
                      "internal error: abs addr too large");
#ifdef X64
        /* To reach the high 2GB of the lower 4GB we need the addr32 prefix */
        if ((ptr_uint_t)addr > INT_MAX)
            need_addr32 = X64_MODE_DC(get_thread_private_dcontext());
#endif
        return opnd_create_far_base_disp_ex(seg, REG_NULL, REG_NULL, 0,
                                            (int)(ptr_int_t)addr, data_size, false, false,
                                            need_addr32);
    }
#ifdef X64
    else {
        opnd_t opnd;
        opnd.kind = ABS_ADDR_kind;
        CLIENT_ASSERT(data_size < OPSZ_LAST_ENUM, "opnd_create_base_disp: invalid size");
        opnd.size = data_size;
        CLIENT_ASSERT(
            seg ==
                REG_NULL IF_X86(|| (seg >= REG_START_SEGMENT && seg <= REG_STOP_SEGMENT)),
            "opnd_create_far_abs_addr: invalid segment");
        IF_X86(opnd.aux.segment = seg);
        opnd.value.addr = addr;
        return opnd;
    }
#endif
}

#if defined(X64) || defined(ARM)
opnd_t
opnd_create_rel_addr(void *addr, opnd_size_t data_size)
{
    return opnd_create_far_rel_addr(REG_NULL, addr, data_size);
}

/* PR 253327: We represent rip-relative w/ an address-size prefix
 * (i.e., 32 bits instead of 64) as simply having the top 32 bits of
 * "addr" zeroed out.  This means that we never encode an address
 * prefix, and we if one already exists in the raw bits we have to go
 * looking for it at encode time.
 */
opnd_t
opnd_create_far_rel_addr(reg_id_t seg, void *addr, opnd_size_t data_size)
{
    opnd_t opnd;
    opnd.kind = REL_ADDR_kind;
    CLIENT_ASSERT(data_size < OPSZ_LAST_ENUM, "opnd_create_base_disp: invalid size");
    opnd.size = data_size;
    CLIENT_ASSERT(
        seg == REG_NULL IF_X86(|| (seg >= REG_START_SEGMENT && seg <= REG_STOP_SEGMENT)),
        "opnd_create_far_rel_addr: invalid segment");
    IF_X86(opnd.aux.segment = seg);
    opnd.value.addr = addr;
    return opnd;
}
#endif /* X64 */

void *
opnd_get_addr(opnd_t opnd)
{
    /* check base-disp first since opnd_is_abs_addr() says yes for it */
    if (opnd_is_abs_base_disp(opnd))
        return (void *)(ptr_int_t)opnd_get_disp(opnd);
#if defined(X64) || defined(ARM)
    if (IF_X64(opnd_is_abs_addr(opnd) ||) opnd_is_rel_addr(opnd))
        return opnd.value.addr;
#endif
    CLIENT_ASSERT(false, "opnd_get_addr called on invalid opnd type");
    return NULL;
}

bool
opnd_is_memory_reference(opnd_t opnd)
{
    return (opnd_is_base_disp(opnd) IF_X86_64(|| opnd_is_abs_addr(opnd)) ||
#if defined(X64) || defined(ARM)
            opnd_is_rel_addr(opnd) ||
#endif
            opnd_is_mem_instr(opnd));
}

bool
opnd_is_far_memory_reference(opnd_t opnd)
{
    return (opnd_is_far_base_disp(opnd)
                IF_X64(|| opnd_is_far_abs_addr(opnd) || opnd_is_far_rel_addr(opnd)));
}

bool
opnd_is_near_memory_reference(opnd_t opnd)
{
    return (opnd_is_near_base_disp(opnd)
                IF_X64(|| opnd_is_near_abs_addr(opnd) || opnd_is_near_rel_addr(opnd)) ||
            IF_ARM(opnd_is_near_rel_addr(opnd) ||) opnd_is_mem_instr(opnd));
}

int
opnd_num_regs_used(opnd_t opnd)
{
    switch (opnd.kind) {
    case NULL_kind:
    case IMMED_INTEGER_kind:
    case IMMED_FLOAT_kind:
    case IMMED_DOUBLE_kind:
    case PC_kind:
    case FAR_PC_kind:
    case INSTR_kind:
    case FAR_INSTR_kind:
    case MEM_INSTR_kind: return 0;

    case REG_kind: return 1;

    case BASE_DISP_kind:
        return (((opnd_get_base(opnd) == REG_NULL) ? 0 : 1) +
                ((opnd_get_index(opnd) == REG_NULL) ? 0 : 1) +
                ((opnd_get_segment(opnd) == REG_NULL) ? 0 : 1));

#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
#endif
#ifdef X64
    case ABS_ADDR_kind: return ((opnd_get_segment(opnd) == REG_NULL) ? 0 : 1);
#endif
    default:
        CLIENT_ASSERT(false, "opnd_num_regs_used called on invalid opnd type");
        return 0;
    }
}

reg_id_t
opnd_get_reg_used(opnd_t opnd, int index)
{
    switch (opnd.kind) {
    case NULL_kind:
    case IMMED_INTEGER_kind:
    case IMMED_FLOAT_kind:
    case IMMED_DOUBLE_kind:
    case PC_kind:
    case FAR_PC_kind:
    case MEM_INSTR_kind:
        CLIENT_ASSERT(false, "opnd_get_reg_used called on invalid opnd type");
        return REG_NULL;

    case REG_kind:
        if (index == 0)
            return opnd_get_reg(opnd);
        else {
            CLIENT_ASSERT(false, "opnd_get_reg_used called on invalid opnd type");
            return REG_NULL;
        }

    case BASE_DISP_kind:
        if (index == 0) {
            if (opnd_get_base(opnd) != REG_NULL)
                return opnd_get_base(opnd);
            else if (opnd_get_index(opnd) != REG_NULL)
                return opnd_get_index(opnd);
            else
                return opnd_get_segment(opnd);
        } else if (index == 1) {
            if (opnd_get_index(opnd) != REG_NULL)
                return opnd_get_index(opnd);
            else
                return opnd_get_segment(opnd);
        } else if (index == 2)
            return opnd_get_segment(opnd);
        else {
            CLIENT_ASSERT(false, "opnd_get_reg_used called on invalid opnd type");
            return REG_NULL;
        }

#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
#endif
#ifdef X64
    case ABS_ADDR_kind:
        if (index == 0)
            return opnd_get_segment(opnd);
        else {
            /* We only assert if beyond the number possible: not if beyond the
             * number present.  Should we assert on the latter?
             */
            CLIENT_ASSERT(false, "opnd_get_reg_used called on invalid opnd type");
            return REG_NULL;
        }
#endif

    default:
        CLIENT_ASSERT(false, "opnd_get_reg_used called on invalid opnd type");
        return REG_NULL;
    }
}

/***************************************************************************/
/* utility routines */

const reg_id_t d_r_regparms[] = {
#ifdef X86
#    ifdef X64
    REGPARM_0,  REGPARM_1, REGPARM_2, REGPARM_3,
#        ifdef UNIX
    REGPARM_4,  REGPARM_5,
#        endif
#    endif
#elif defined(AARCHXX)
    REGPARM_0,  REGPARM_1, REGPARM_2, REGPARM_3,
#    ifdef X64
    REGPARM_4,  REGPARM_5, REGPARM_6, REGPARM_7,
#    endif
#elif defined(RISCV64)
    REGPARM_0,  REGPARM_1, REGPARM_2, REGPARM_3, REGPARM_4, REGPARM_5,
#endif
    REG_INVALID
};

/*
   opnd_uses_reg is now changed so that it does consider 8/16 bit
   register overlaps.  i think this change is OK and correct, but not
   sure.  as far as I'm aware, only my optimization stuff and the
   register stealing code (which is now not used, right?) relies on
   this code ==> but we now export it via CI API */

bool
opnd_uses_reg(opnd_t opnd, reg_id_t reg)
{
    if (reg == REG_NULL)
        return false;
    switch (opnd.kind) {
    case NULL_kind:
    case IMMED_INTEGER_kind:
    case IMMED_FLOAT_kind:
    case IMMED_DOUBLE_kind:
    case PC_kind:
    case FAR_PC_kind:
    case INSTR_kind:
    case FAR_INSTR_kind:
    case MEM_INSTR_kind: return false;

    case REG_kind: return (dr_reg_fixer[reg] == dr_reg_fixer[opnd_get_reg(opnd)]);

    case BASE_DISP_kind:
        return (dr_reg_fixer[reg] == dr_reg_fixer[opnd_get_base(opnd)] ||
                dr_reg_fixer[reg] == dr_reg_fixer[opnd_get_index(opnd)] ||
                dr_reg_fixer[reg] == dr_reg_fixer[opnd_get_segment(opnd)]);

#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
#endif
#ifdef X64
    case ABS_ADDR_kind:
        return (dr_reg_fixer[reg] == dr_reg_fixer[opnd_get_segment(opnd)]);
#endif

    default: CLIENT_ASSERT(false, "opnd_uses_reg: unknown opnd type"); return false;
    }
}

bool
opnd_replace_reg(opnd_t *opnd, reg_id_t old_reg, reg_id_t new_reg)
{
    switch (opnd->kind) {
    case NULL_kind:
    case IMMED_INTEGER_kind:
    case IMMED_FLOAT_kind:
    case IMMED_DOUBLE_kind:
    case PC_kind:
    case FAR_PC_kind:
    case INSTR_kind:
    case FAR_INSTR_kind:
    case MEM_INSTR_kind: return false;

    case REG_kind:
        if (old_reg == opnd_get_reg(*opnd)) {
            *opnd = opnd_create_reg_ex(
                new_reg, opnd_is_reg_partial(*opnd) ? opnd_get_size(*opnd) : 0,
                opnd_get_flags(*opnd));
            return true;
        }
        return false;

    case BASE_DISP_kind: {
        reg_id_t ob = opnd_get_base(*opnd);
        reg_id_t oi = opnd_get_index(*opnd);
        reg_id_t os = opnd_get_segment(*opnd);
        opnd_size_t size = opnd_get_size(*opnd);
        if (old_reg == ob || old_reg == oi || old_reg == os) {
            reg_id_t b = (old_reg == ob) ? new_reg : ob;
            reg_id_t i = (old_reg == oi) ? new_reg : oi;
            int d = opnd_get_disp(*opnd);
#if defined(AARCH64)
            bool scaled = false;
            dr_extend_type_t extend = opnd_get_index_extend(*opnd, &scaled, NULL);
            dr_opnd_flags_t flags = opnd_get_flags(*opnd);
            *opnd = opnd_create_base_disp_aarch64(b, i, extend, scaled, d, flags, size);
#elif defined(ARM)
            uint amount;
            dr_shift_type_t shift = opnd_get_index_shift(*opnd, &amount);
            dr_opnd_flags_t flags = opnd_get_flags(*opnd);
            *opnd = opnd_create_base_disp_arm(b, i, shift, amount, d, flags, size);
#elif defined(X86)
            int sc = opnd_get_scale(*opnd);
            reg_id_t s = (old_reg == os) ? new_reg : os;
            *opnd = opnd_create_far_base_disp_ex(
                s, b, i, sc, d, size, opnd_is_disp_encode_zero(*opnd),
                opnd_is_disp_force_full(*opnd), opnd_is_disp_short_addr(*opnd));
#elif defined(RISCV64)
            /* FIXME i#3544: RISC-V has no support for base + idx * scale + disp.
             * We could support base + disp as long as disp == +/-1MB.
             * If needed, instructions with this operand should be transformed
             * to:
             *   mul idx, idx, scale # or slli if scale is immediate
             *   add base, base, idx
             *   addi base, base, disp
             */
            CLIENT_ASSERT(false, "Not implemented");
            /* Marking as unused to silence -Wunused-variable. */
            (void)size;
            (void)b;
            (void)i;
            (void)d;
            return false;
#endif
            return true;
        }
    }
        return false;

#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
        if (old_reg == opnd_get_segment(*opnd)) {
            *opnd = opnd_create_far_rel_addr(new_reg, opnd_get_addr(*opnd),
                                             opnd_get_size(*opnd));
            return true;
        }
        return false;
#endif
#ifdef X64
    case ABS_ADDR_kind:
        if (old_reg == opnd_get_segment(*opnd)) {
            *opnd = opnd_create_far_abs_addr(new_reg, opnd_get_addr(*opnd),
                                             opnd_get_size(*opnd));
            return true;
        }
        return false;
#endif

    default: CLIENT_ASSERT(false, "opnd_replace_reg: invalid opnd type"); return false;
    }
}

opnd_t
opnd_create_increment_reg(opnd_t opnd, uint increment)
{
    opnd_t inc_opnd DR_IF_DEBUG(= { 0 }); /* FIXME: Needed until i#417 is fixed. */
    CLIENT_ASSERT(opnd_is_reg(opnd), "opnd_create_increment_reg: not a register");

    reg_id_t reg = opnd.value.reg_and_element_size.reg;
    reg_id_t min_reg = DR_REG_INVALID;
    reg_id_t max_reg = DR_REG_INVALID;
#ifdef AARCH64
    if (reg >= DR_REG_W0 && reg <= DR_REG_W30) {
        min_reg = DR_REG_W0;
        max_reg = DR_REG_W30;
    } else if (reg >= DR_REG_X0 && reg <= DR_REG_X30) {
        min_reg = DR_REG_X0;
        max_reg = DR_REG_X30;
    } else if (reg >= DR_REG_B0 && reg <= DR_REG_B31) {
        min_reg = DR_REG_B0;
        max_reg = DR_REG_B31;
    } else if (reg >= DR_REG_H0 && reg <= DR_REG_H31) {
        min_reg = DR_REG_H0;
        max_reg = DR_REG_H31;
    } else if (reg >= DR_REG_S0 && reg <= DR_REG_S31) {
        min_reg = DR_REG_S0;
        max_reg = DR_REG_S31;
    } else if (reg >= DR_REG_D0 && reg <= DR_REG_D31) {
        min_reg = DR_REG_D0;
        max_reg = DR_REG_D31;
    } else if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31) {
        min_reg = DR_REG_Q0;
        max_reg = DR_REG_Q31;
    } else if (reg >= DR_REG_Z0 && reg <= DR_REG_Z31) {
        min_reg = DR_REG_Z0;
        max_reg = DR_REG_Z31;
    } else if (reg >= DR_REG_P0 && reg <= DR_REG_P15) {
        min_reg = DR_REG_P0;
        max_reg = DR_REG_P15;
    }
#else
    ASSERT_NOT_IMPLEMENTED(false);
#endif

    CLIENT_ASSERT(min_reg != DR_REG_INVALID && max_reg != DR_REG_INVALID,
                  "opnd_create_increment_reg: reg not incrementable");

    reg_id_t new_reg = (reg - min_reg + increment) % (max_reg - min_reg + 1) + min_reg;

    inc_opnd.kind = REG_kind;
    inc_opnd.value.reg_and_element_size.reg = new_reg;
    inc_opnd.value.reg_and_element_size.element_size =
        opnd.value.reg_and_element_size.element_size;
    inc_opnd.size = opnd.size; /* indicates full size of reg */
    inc_opnd.aux.flags = opnd.aux.flags;
    return inc_opnd;
}

static reg_id_t
reg_match_size_and_type(reg_id_t new_reg, opnd_size_t size, reg_id_t old_reg)
{
    reg_id_t sized_reg = reg_resize_to_opsz(new_reg, size);
#ifdef X86
    /* Convert from L to H version of 8-bit regs. */
    if (old_reg >= DR_REG_START_x86_8 && old_reg <= DR_REG_STOP_x86_8) {
        sized_reg = (sized_reg - DR_REG_START_8HL) + DR_REG_START_x86_8;
        ASSERT(sized_reg <= DR_REG_STOP_x86_8);
    }
#endif
    return sized_reg;
}

bool
opnd_replace_reg_resize(opnd_t *opnd, reg_id_t old_reg, reg_id_t new_reg)
{
    switch (opnd->kind) {
    case NULL_kind:
    case IMMED_INTEGER_kind:
    case IMMED_FLOAT_kind:
    case IMMED_DOUBLE_kind:
    case PC_kind:
    case FAR_PC_kind:
    case INSTR_kind:
    case FAR_INSTR_kind:
    case MEM_INSTR_kind: return false;

    case REG_kind:
        if (reg_overlap(old_reg, opnd_get_reg(*opnd))) {
            reg_id_t sized_reg = reg_match_size_and_type(new_reg, opnd_get_size(*opnd),
                                                         opnd_get_reg(*opnd));
            *opnd = opnd_create_reg_ex(
                sized_reg, opnd_is_reg_partial(*opnd) ? opnd_get_size(*opnd) : 0,
                opnd_get_flags(*opnd));
            return true;
        }
        return false;

    case BASE_DISP_kind: {
        reg_id_t ob = opnd_get_base(*opnd);
        reg_id_t oi = opnd_get_index(*opnd);
        reg_id_t os = opnd_get_segment(*opnd);
        opnd_size_t size = opnd_get_size(*opnd);
        bool found = false;
        reg_id_t new_b = ob;
        reg_id_t new_i = oi;
        IF_X86(reg_id_t new_s = os;)
        if (reg_overlap(old_reg, ob)) {
            found = true;
            new_b = reg_match_size_and_type(new_reg, reg_get_size(ob), ob);
        }
        if (reg_overlap(old_reg, oi)) {
            found = true;
            new_i = reg_match_size_and_type(new_reg, reg_get_size(oi), oi);
        }
        if (reg_overlap(old_reg, os)) {
            found = true;
            IF_X86(new_s = reg_match_size_and_type(new_reg, reg_get_size(os), os);)
        }
        if (found) {
            int disp = opnd_get_disp(*opnd);
#if defined(AARCH64)
            bool scaled = false;
            dr_extend_type_t extend = opnd_get_index_extend(*opnd, &scaled, NULL);
            dr_opnd_flags_t flags = opnd_get_flags(*opnd);
            *opnd = opnd_create_base_disp_aarch64(new_b, new_i, extend, scaled, disp,
                                                  flags, size);
#elif defined(ARM)
            uint amount;
            dr_shift_type_t shift = opnd_get_index_shift(*opnd, &amount);
            dr_opnd_flags_t flags = opnd_get_flags(*opnd);
            *opnd =
                opnd_create_base_disp_arm(new_b, new_i, shift, amount, disp, flags, size);
#elif defined(X86)
            int sc = opnd_get_scale(*opnd);
            *opnd = opnd_create_far_base_disp_ex(
                new_s, new_b, new_i, sc, disp, size, opnd_is_disp_encode_zero(*opnd),
                opnd_is_disp_force_full(*opnd), opnd_is_disp_short_addr(*opnd));
#elif defined(RISCV64)
            /* FIXME i#3544: RISC-V has no support for base + idx * scale + disp.
             * We could support base + disp as long as disp == +/-1MB.
             * If needed, instructions with this operand should be transformed
             * to:
             *   mul idx, idx, scale # or slli if scale is immediate
             *   add base, base, idx
             *   addi base, base, disp
             */
            CLIENT_ASSERT(false, "Not implemented");
            /* Marking as unused to silence -Wunused-variable. */
            (void)disp;
            (void)size;
            return false;
#endif
            return true;
        }
    }
        return false;

#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
        if (reg_overlap(old_reg, opnd_get_segment(*opnd))) {
            reg_id_t new_s = reg_match_size_and_type(
                new_reg, reg_get_size(opnd_get_segment(*opnd)), opnd_get_segment(*opnd));
            *opnd = opnd_create_far_rel_addr(new_s, opnd_get_addr(*opnd),
                                             opnd_get_size(*opnd));
            return true;
        }
        return false;
#endif
#ifdef X64
    case ABS_ADDR_kind:
        if (reg_overlap(old_reg, opnd_get_segment(*opnd))) {
            reg_id_t new_s = reg_match_size_and_type(
                new_reg, reg_get_size(opnd_get_segment(*opnd)), opnd_get_segment(*opnd));
            *opnd = opnd_create_far_abs_addr(new_s, opnd_get_addr(*opnd),
                                             opnd_get_size(*opnd));
            return true;
        }
        return false;
#endif

    default: CLIENT_ASSERT(false, "opnd_replace_reg: invalid opnd type"); return false;
    }
}

/* this is not conservative -- only considers two memory references to
 * be the same if their constituent components (registers, displacement)
 * are the same.
 * different from opnd_same b/c this routine ignores data size!
 */
bool
opnd_same_address(opnd_t op1, opnd_t op2)
{
    if (op1.kind != op2.kind)
        return false;
    if (!opnd_is_memory_reference(op1) || !opnd_is_memory_reference(op2))
        return false;
    if (opnd_get_segment(op1) != opnd_get_segment(op2))
        return false;
    if (opnd_is_base_disp(op1)) {
#ifdef ARM
        uint amount1, amount2;
#endif
        if (!opnd_is_base_disp(op2))
            return false;
        if (opnd_get_base(op1) != opnd_get_base(op2))
            return false;
        if (opnd_get_index(op1) != opnd_get_index(op2))
            return false;
        if (opnd_get_scale(op1) != opnd_get_scale(op2))
            return false;
        if (opnd_get_disp(op1) != opnd_get_disp(op2))
            return false;
#ifdef ARM
        if (opnd_get_index_shift(op1, &amount1) != opnd_get_index_shift(op2, &amount2) ||
            amount1 != amount2)
            return false;
        if (opnd_get_flags(op1) != opnd_get_flags(op2))
            return false;
#endif
    } else {
#if defined(X64) || defined(ARM)
        CLIENT_ASSERT(IF_X64(opnd_is_abs_addr(op1) ||) opnd_is_rel_addr(op1),
                      "internal type error in opnd_same_address");
        if (opnd_get_addr(op1) != opnd_get_addr(op2))
            return false;
#else
        CLIENT_ASSERT(false, "internal type error in opnd_same_address");
#endif
    }

    /* we ignore size */

    return true;
}

bool
opnd_same(opnd_t op1, opnd_t op2)
{
    if (op1.kind != op2.kind)
        return false;
    else if (!opnd_same_sizes_ok(opnd_get_size(op1), opnd_get_size(op2),
                                 opnd_is_reg(op1)) &&
             (IF_X86(opnd_is_immed_int(op1) ||) /* on ARM we ignore immed sizes */
              opnd_is_reg(op1) ||
              opnd_is_memory_reference(op1)))
        return false;
    /* If we could rely on unused bits being 0 could avoid dispatch on type.
     * Presumably not on critical path, though, so not bothering to try and
     * asssert that those bits are 0.
     */
    switch (op1.kind) {
    case NULL_kind: return true;
    case IMMED_INTEGER_kind: return op1.value.immed_int == op2.value.immed_int;
    case IMMED_FLOAT_kind:
        /* avoid any fp instrs (xref i#386) */
        return *(int *)(&op1.value.immed_float) == *(int *)(&op2.value.immed_float);
#ifndef WINDOWS
        /* XXX i#4488: x87 floating point immediates should be double precision.
         * Type double currently not included for Windows because sizeof(opnd_t) does
         * not equal EXPECTED_SIZEOF_OPND, triggering the ASSERT in d_r_arch_init().
         */
    case IMMED_DOUBLE_kind:
        return *(int64 *)(&op1.value.immed_double) == *(int64 *)(&op2.value.immed_double);
#endif
    case PC_kind: return op1.value.pc == op2.value.pc;
    case FAR_PC_kind:
        return (op1.aux.far_pc_seg_selector == op2.aux.far_pc_seg_selector &&
                op1.value.pc == op2.value.pc);
    case INSTR_kind:
        return (op1.value.instr == op2.value.instr && op1.aux.shift == op2.aux.shift &&
                op1.size == op2.size);
    case FAR_INSTR_kind: return op1.value.instr == op2.value.instr;
    case REG_kind:
        return op1.value.reg_and_element_size.reg == op2.value.reg_and_element_size.reg &&
            op1.value.reg_and_element_size.element_size ==
            op2.value.reg_and_element_size.element_size;
    case BASE_DISP_kind:
        return (IF_X86(op1.aux.segment == op2.aux.segment &&)
                        op1.value.base_disp.base_reg == op2.value.base_disp.base_reg &&
                op1.value.base_disp.index_reg == op2.value.base_disp.index_reg &&
#ifdef X86
                op1.value.base_disp.index_reg_is_zmm ==
                    op2.value.base_disp.index_reg_is_zmm &&
#endif
                IF_X86(op1.value.base_disp.scale == op2.value.base_disp.scale &&) IF_ARM(
                    op1.value.base_disp.shift_type == op2.value.base_disp.shift_type &&
                    op1.value.base_disp.shift_amount_minus_1 ==
                        op2.value.base_disp.shift_amount_minus_1 &&)
                        op1.value.base_disp.disp == op2.value.base_disp.disp &&
                IF_X86(op1.value.base_disp.encode_zero_disp ==
                           op2.value.base_disp.encode_zero_disp &&
                       op1.value.base_disp.force_full_disp ==
                           op2.value.base_disp.force_full_disp &&
                       /* disp_short_addr only matters if no registers are set */
                       (((op1.value.base_disp.base_reg != REG_NULL ||
                          op1.value.base_disp.index_reg != REG_NULL) &&
                         (op2.value.base_disp.base_reg != REG_NULL ||
                          op2.value.base_disp.index_reg != REG_NULL)) ||
                        op1.value.base_disp.disp_short_addr ==
                            op2.value.base_disp.disp_short_addr) &&) true);
#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
#endif
#ifdef X64
    case ABS_ADDR_kind:
        return (IF_X86(op1.aux.segment == op2.aux.segment &&)
                    op1.value.addr == op2.value.addr);
#endif
    case MEM_INSTR_kind:
        return (op1.value.instr == op2.value.instr && op1.aux.disp == op2.aux.disp);
    default: CLIENT_ASSERT(false, "opnd_same: invalid opnd type"); return false;
    }
}

bool
opnd_share_reg(opnd_t op1, opnd_t op2)
{
    switch (op1.kind) {
    case NULL_kind:
    case IMMED_INTEGER_kind:
    case IMMED_FLOAT_kind:
    case IMMED_DOUBLE_kind:
    case PC_kind:
    case FAR_PC_kind:
    case INSTR_kind:
    case FAR_INSTR_kind:
    case MEM_INSTR_kind: return false;
    case REG_kind: return opnd_uses_reg(op2, opnd_get_reg(op1));
    case BASE_DISP_kind:
        return (opnd_uses_reg(op2, opnd_get_base(op1)) ||
                opnd_uses_reg(op2, opnd_get_index(op1)) ||
                opnd_uses_reg(op2, opnd_get_segment(op1)));
#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
#endif
#ifdef X64
    case ABS_ADDR_kind: return (opnd_uses_reg(op2, opnd_get_segment(op1)));
#endif
    default: CLIENT_ASSERT(false, "opnd_share_reg: invalid opnd type"); return false;
    }
}

static bool
range_overlap(ptr_uint_t a1, ptr_uint_t a2, size_t s1, size_t s2)
{
    ptr_uint_t min, max;
    size_t min_plus;
    if (a1 < a2) {
        min = a1;
        min_plus = s1;
        max = a2;
    } else {
        min = a2;
        min_plus = s2;
        max = a1;
    }
    return (min + min_plus > max); /* open-ended */
}

/* Returns true if def, considered as a write, affects use.
 * Is conservative, so if both def and use are memory references,
 * will return true unless it can disambiguate them.
 */
bool
opnd_defines_use(opnd_t def, opnd_t use)
{
    switch (def.kind) {
    case NULL_kind:
    case IMMED_INTEGER_kind:
    case IMMED_FLOAT_kind:
    case IMMED_DOUBLE_kind:
    case PC_kind:
    case FAR_PC_kind:
    case INSTR_kind:
    case FAR_INSTR_kind: return false;
    case REG_kind: return opnd_uses_reg(use, opnd_get_reg(def));
    case BASE_DISP_kind: {
#ifdef ARM
        uint amount1, amount2;
#endif
        if (!opnd_is_memory_reference(use))
            return false;
#ifdef X64
        if (!opnd_is_base_disp(use))
            return true;
#endif
        /* try to disambiguate the two memory references
         * for now, only consider identical regs and different disp
         */
        if (opnd_get_base(def) != opnd_get_base(use))
            return true;
        if (opnd_get_index(def) != opnd_get_index(use))
            return true;
        if (opnd_get_scale(def) != opnd_get_scale(use))
            return true;
        if (opnd_get_segment(def) != opnd_get_segment(use))
            return true;
#ifdef ARM
        if (opnd_get_index_shift(def, &amount1) != opnd_get_index_shift(use, &amount2) ||
            amount1 != amount2)
            return true;
        if (opnd_get_flags(def) != opnd_get_flags(use))
            return true;
#endif
        /* everything is identical, now make sure disps don't overlap */
        return range_overlap(opnd_get_disp(def), opnd_get_disp(use),
                             opnd_size_in_bytes(opnd_get_size(def)),
                             opnd_size_in_bytes(opnd_get_size(use)));
    }
#if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
#endif
#ifdef X64
    case ABS_ADDR_kind:
        if (!opnd_is_memory_reference(use))
            return false;
        if (opnd_is_base_disp(use))
            return true;
        if (opnd_get_segment(def) != opnd_get_segment(use))
            return true;
        return range_overlap((ptr_uint_t)opnd_get_addr(def),
                             (ptr_uint_t)opnd_get_addr(use),
                             opnd_size_in_bytes(opnd_get_size(def)),
                             opnd_size_in_bytes(opnd_get_size(use)));
#endif
    case MEM_INSTR_kind:
        if (!opnd_is_memory_reference(use))
            return false;
        /* we don't know our address so we have to assume true */
        return true;
    default: CLIENT_ASSERT(false, "opnd_defines_use: invalid opnd type"); return false;
    }
}

uint
opnd_size_in_bytes(opnd_size_t size)
{
    CLIENT_ASSERT(size >= OPSZ_FIRST, "opnd_size_in_bytes: invalid size");
    switch (size) {
    case OPSZ_0: return 0;
    case OPSZ_1:
    case OPSZ_1_reg4: /* mem size */
    case OPSZ_1_of_4:
    case OPSZ_1_of_8:
    case OPSZ_1_of_16:
    case OPSZ_1b: /* round up */
    case OPSZ_2b:
    case OPSZ_3b:
    case OPSZ_4b:
    case OPSZ_5b:
    case OPSZ_6b:
    case OPSZ_7b: return 1;
    case OPSZ_2_of_4:
    case OPSZ_2_of_8:
    case OPSZ_2_of_16:
    case OPSZ_2_short1: /* default size */
    case OPSZ_2:
    case OPSZ_2_reg4: /* mem size */
    case OPSZ_9b:     /* round up */
    case OPSZ_10b:
    case OPSZ_11b:
    case OPSZ_12b:
    case OPSZ_eighth_16_vex32:
    case OPSZ_eighth_16_vex32_evex64: return 2;
    case OPSZ_20b: /* round up */
    case OPSZ_3: return 3;
    case OPSZ_4_of_8:
    case OPSZ_4_of_16:
    case OPSZ_4_rex8_of_16:
    case OPSZ_4_short2: /* default size */
#ifndef X64
    case OPSZ_4x8:           /* default size */
    case OPSZ_4x8_short2:    /* default size */
    case OPSZ_4x8_short2xi8: /* default size */
#endif
    case OPSZ_4_short2xi4:   /* default size */
    case OPSZ_4_rex8_short2: /* default size */
    case OPSZ_4_rex8:
    case OPSZ_4:
    case OPSZ_4_reg16: /* mem size */
    case OPSZ_25b:     /* round up */
    case OPSZ_quarter_16_vex32:
    case OPSZ_quarter_16_vex32_evex64: return 4;
    case OPSZ_6_irex10_short4: /* default size */
    case OPSZ_6: return 6;
    case OPSZ_8_of_16:
    case OPSZ_half_16_vex32:
    case OPSZ_8_short2:
    case OPSZ_8_short4:
    case OPSZ_8:
#ifdef X64
    case OPSZ_4x8:           /* default size */
    case OPSZ_4x8_short2:    /* default size */
    case OPSZ_4x8_short2xi8: /* default size */
#endif
    case OPSZ_8_rex16:        /* default size */
    case OPSZ_8_rex16_short4: /* default size */
#ifndef X64
    case OPSZ_8x16: /* default size */
#endif
        return 8;
    case OPSZ_16:
    case OPSZ_16_vex32:
    case OPSZ_16_of_32:
    case OPSZ_16_vex32_evex64:
#ifdef X64
    case OPSZ_8x16: /* default size */
#endif
        return 16;
    case OPSZ_vex32_evex64: return 32;
    case OPSZ_6x10:
        /* table base + limit; w/ addr16, different format, but same total footprint */
        return IF_X64_ELSE(6, 10);
    case OPSZ_10: return 10;
    case OPSZ_12:
    case OPSZ_12_of_16:
    case OPSZ_12_rex8_of_16:
    case OPSZ_12_rex40_short6: /* default size */ return 12;
    case OPSZ_14_of_16:
    case OPSZ_14: return 14;
    case OPSZ_15_of_16:
    case OPSZ_15: return 15;
    case OPSZ_20: return 20;
    case OPSZ_24: return 24;
    case OPSZ_28_short14: /* default size */
    case OPSZ_28: return 28;
    case OPSZ_32:
    case OPSZ_32_short16: /* default size */ return 32;
    case OPSZ_36: return 36;
    case OPSZ_40: return 40;
    case OPSZ_44: return 44;
    case OPSZ_48: return 48;
    case OPSZ_52: return 52;
    case OPSZ_56: return 56;
    case OPSZ_60: return 60;
    case OPSZ_64: return 64;
    case OPSZ_68: return 68;
    case OPSZ_72: return 72;
    case OPSZ_76: return 76;
    case OPSZ_80: return 80;
    case OPSZ_84: return 84;
    case OPSZ_88: return 88;
    case OPSZ_92: return 92;
    case OPSZ_94: return 94;
    case OPSZ_96: return 96;
    case OPSZ_100: return 100;
    case OPSZ_104: return 104;
    case OPSZ_108_short94: /* default size */
    case OPSZ_108: return 108;
    case OPSZ_112: return 112;
    case OPSZ_116: return 116;
    case OPSZ_120: return 120;
    case OPSZ_124: return 124;
    case OPSZ_128: return 128;
    case OPSZ_512: return 512;
    case OPSZ_VAR_REGLIST: return 0; /* varies to match reglist operand */
    case OPSZ_xsave:
        return 0; /* > 512 bytes: client to use drutil_opnd_mem_size_in_bytes */
    default: CLIENT_ASSERT(false, "opnd_size_in_bytes: invalid opnd type"); return 0;
    }
}

DR_API
uint
opnd_size_in_bits(opnd_size_t size)
{
    switch (size) {
    case OPSZ_1b: return 1;
    case OPSZ_2b: return 2;
    case OPSZ_3b: return 3;
    case OPSZ_4b: return 4;
    case OPSZ_5b: return 5;
    case OPSZ_6b: return 6;
    case OPSZ_7b: return 7;
    case OPSZ_9b: return 9;
    case OPSZ_10b: return 10;
    case OPSZ_11b: return 11;
    case OPSZ_12b: return 12;
    case OPSZ_20b: return 20;
    case OPSZ_25b: return 25;
    default: return opnd_size_in_bytes(size) * 8;
    }
}

DR_API
opnd_size_t
opnd_size_from_bytes(uint bytes)
{
    switch (bytes) {
    case 0: return OPSZ_0;
    case 1: return OPSZ_1;
    case 2: return OPSZ_2;
    case 3: return OPSZ_3;
    case 4: return OPSZ_4;
    case 6: return OPSZ_6;
    case 8: return OPSZ_8;
    case 10: return OPSZ_10;
    case 12: return OPSZ_12;
    case 14: return OPSZ_14;
    case 15: return OPSZ_15;
    case 16: return OPSZ_16;
    case 20: return OPSZ_20;
    case 24: return OPSZ_24;
    case 28: return OPSZ_28;
    case 32: return OPSZ_32;
    case 36: return OPSZ_36;
    case 40: return OPSZ_40;
    case 44: return OPSZ_44;
    case 48: return OPSZ_48;
    case 52: return OPSZ_52;
    case 56: return OPSZ_56;
    case 60: return OPSZ_60;
    case 64: return OPSZ_64;
    case 68: return OPSZ_68;
    case 72: return OPSZ_72;
    case 76: return OPSZ_76;
    case 80: return OPSZ_80;
    case 84: return OPSZ_84;
    case 88: return OPSZ_88;
    case 92: return OPSZ_92;
    case 94: return OPSZ_94;
    case 96: return OPSZ_96;
    case 100: return OPSZ_100;
    case 104: return OPSZ_104;
    case 108: return OPSZ_108;
    case 112: return OPSZ_112;
    case 116: return OPSZ_116;
    case 120: return OPSZ_120;
    case 124: return OPSZ_124;
    case 128: return OPSZ_128;
    case 512: return OPSZ_512;
    default: return OPSZ_NA;
    }
}

/* shrinks all 32-bit registers in opnd to 16 bits.  also shrinks the size of
 * immed ints and mem refs from OPSZ_4 to OPSZ_2.
 */
opnd_t
opnd_shrink_to_16_bits(opnd_t opnd)
{
    int i;
    for (i = 0; i < opnd_num_regs_used(opnd); i++) {
        reg_id_t reg = opnd_get_reg_used(opnd, i);
        if (reg >= REG_START_32 && reg <= REG_STOP_32) {
            opnd_replace_reg(&opnd, reg, reg_32_to_16(reg));
        }
    }
    if ((opnd_is_immed_int(opnd) || opnd_is_memory_reference(opnd)) &&
        opnd_get_size(opnd) == OPSZ_4) /* OPSZ_*_short2 will shrink at encode time */
        opnd_set_size(&opnd, OPSZ_2);
    return opnd;
}

#ifdef X64
/* shrinks all 64-bit registers in opnd to 32 bits.  also shrinks the size of
 * immed ints and mem refs from OPSZ_8 to OPSZ_4.
 */
opnd_t
opnd_shrink_to_32_bits(opnd_t opnd)
{
    int i;
    for (i = 0; i < opnd_num_regs_used(opnd); i++) {
        reg_id_t reg = opnd_get_reg_used(opnd, i);
        if (reg >= REG_START_64 && reg <= REG_STOP_64) {
            opnd_replace_reg(&opnd, reg, reg_64_to_32(reg));
        }
    }
    if ((opnd_is_immed_int(opnd) || opnd_is_memory_reference(opnd)) &&
        opnd_get_size(opnd) == OPSZ_8)
        opnd_set_size(&opnd, OPSZ_4);
    return opnd;
}

#endif

static reg_t
reg_get_value_helper(reg_id_t reg, priv_mcontext_t *mc)
{
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "reg_get_value_helper(): internal error non-ptr sized reg");
    if (reg == REG_NULL)
        return 0;

    return *(reg_t *)((byte *)mc + opnd_get_reg_mcontext_offs(reg));
}

/* Returns the value of the register reg, selected from the passed-in
 * register values.
 */
reg_t
reg_get_value_priv(reg_id_t reg, priv_mcontext_t *mc)
{
    if (reg == REG_NULL)
        return 0;
#ifdef X64
    if (reg >= REG_START_64 && reg <= REG_STOP_64)
        return reg_get_value_helper(reg, mc);
    if (reg >= REG_START_32 && reg <= REG_STOP_32) {
        reg_t val = reg_get_value_helper(dr_reg_fixer[reg], mc);
        return (val & 0x00000000ffffffff);
    }
#else
    if (reg >= REG_START_32 && reg <= REG_STOP_32) {
        return reg_get_value_helper(reg, mc);
    }
#endif
#ifdef X86
    if (reg >= REG_START_8 && reg <= REG_STOP_8) {
        reg_t val = reg_get_value_helper(dr_reg_fixer[reg], mc);
        if (reg >= REG_AH && reg <= REG_BH)
            return ((val & 0x0000ff00) >> 8);
        else /* all others are the lower 8 bits */
            return (val & 0x000000ff);
    }
    if (reg >= REG_START_16 && reg <= REG_STOP_16) {
        reg_t val = reg_get_value_helper(dr_reg_fixer[reg], mc);
        return (val & 0x0000ffff);
    }
#endif
    /* mmx and segment cannot be part of address.
     * xmm/ymm/zmm can with VSIB, but we'd have to either return a larger type,
     * or take in an offset within the xmm/ymm/zmm register -- so we leave this
     * routine supporting only GPR and have a separate routine for VSIB
     * (opnd_compute_VSIB_index()).
     * if want to use this routine for more than just effective address
     * calculations, need to pass in mmx/xmm state, or need to grab it
     * here.  would then need to check dr_mcontext_t.size.
     */
    CLIENT_ASSERT(false, "reg_get_value: unsupported register");
    return 0;
}

DR_API
reg_t
reg_get_value(reg_id_t reg, dr_mcontext_t *mc)
{
    /* only supports GPRs so we ignore mc.size */
    return reg_get_value_priv(reg, dr_mcontext_as_priv_mcontext(mc));
}

DR_API
/* Supports all but floating-point */
bool
reg_get_value_ex(reg_id_t reg, dr_mcontext_t *mc, OUT byte *val)
{
#ifdef X86
    if (reg >= DR_REG_START_MMX && reg <= DR_REG_STOP_MMX) {
        get_mmx_val((uint64 *)val, reg - DR_REG_START_MMX);
    } else if (reg >= DR_REG_START_XMM && reg <= DR_REG_STOP_XMM) {
        if (!TEST(DR_MC_MULTIMEDIA, mc->flags) || mc->size != sizeof(dr_mcontext_t))
            return false;
        memcpy(val, &mc->simd[reg - DR_REG_START_XMM], XMM_REG_SIZE);
    } else if (reg >= DR_REG_START_YMM && reg <= DR_REG_STOP_YMM) {
        if (!TEST(DR_MC_MULTIMEDIA, mc->flags) || mc->size != sizeof(dr_mcontext_t))
            return false;
        memcpy(val, &mc->simd[reg - DR_REG_START_YMM], YMM_REG_SIZE);
    } else if (reg >= DR_REG_START_ZMM && reg <= DR_REG_STOP_ZMM) {
        if (!TEST(DR_MC_MULTIMEDIA, mc->flags) || mc->size != sizeof(dr_mcontext_t))
            return false;
        memcpy(val, &mc->simd[reg - DR_REG_START_ZMM], ZMM_REG_SIZE);
    } else if (reg >= DR_REG_START_OPMASK && reg <= DR_REG_STOP_OPMASK) {
        if (!TEST(DR_MC_MULTIMEDIA, mc->flags) || mc->size != sizeof(dr_mcontext_t))
            return false;
        memcpy(val, &mc->opmask[reg - DR_REG_START_OPMASK], OPMASK_AVX512BW_REG_SIZE);
    } else {
        reg_t regval = reg_get_value(reg, mc);
        *(reg_t *)val = regval;
    }
#else
    CLIENT_ASSERT(false, "NYI i#1551");
#endif
    return true;
}

/* Sets the register reg in the passed in mcontext to value.  Currently only works
 * with ptr sized registers. See reg_set_value_ex to handle other sized registers. */
void
reg_set_value_priv(reg_id_t reg, priv_mcontext_t *mc, reg_t value)
{
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "reg_get_value_helper(): internal error non-ptr sized reg");

    if (reg == REG_NULL)
        return;

    *(reg_t *)((byte *)mc + opnd_get_reg_mcontext_offs(reg)) = value;
}

bool
reg_set_value_ex_priv(reg_id_t reg, priv_mcontext_t *mc, byte *val_buf)
{
#ifdef X86
    CLIENT_ASSERT(reg != REG_NULL, "REG_NULL was passed.");

    dr_zmm_t *simd = (dr_zmm_t *)((byte *)mc + SIMD_OFFSET);

    if (reg_is_gpr(reg)) {
        reg_t *value = (reg_t *)val_buf;
        reg_set_value_priv(reg, mc, *value);
    } else if (reg >= DR_REG_START_XMM && reg <= DR_REG_STOP_XMM) {
        memcpy(&(simd[reg - DR_REG_START_XMM]), val_buf, XMM_REG_SIZE);
    } else if (reg >= DR_REG_START_YMM && reg <= DR_REG_STOP_YMM) {
        memcpy(&(simd[reg - DR_REG_START_YMM]), val_buf, YMM_REG_SIZE);
    } else if (reg >= DR_REG_START_ZMM && reg <= DR_REG_STOP_ZMM) {
        memcpy(&(simd[reg - DR_REG_START_ZMM]), val_buf, ZMM_REG_SIZE);
    } else {
        /* Note, we can reach here for MMX register */
        CLIENT_ASSERT(false, "NYI i#3504");
        return false;
    }

    return true;
#else
    CLIENT_ASSERT(false, "NYI  i#1551, i#3504");
    return false;
#endif
}

DR_API
void
reg_set_value(reg_id_t reg, dr_mcontext_t *mc, reg_t value)
{
    /* only supports GPRs so we ignore mc.size */
    reg_set_value_priv(reg, dr_mcontext_as_priv_mcontext(mc), value);
}

DR_API
bool
reg_set_value_ex(reg_id_t reg, dr_mcontext_t *mc, IN byte *val_buf)
{
    return reg_set_value_ex_priv(reg, dr_mcontext_as_priv_mcontext(mc), val_buf);
}

/* helper for sharing w/ VSIB computations */
app_pc
opnd_compute_address_helper(opnd_t opnd, priv_mcontext_t *mc, ptr_int_t scaled_index)
{
    reg_id_t base;
    int disp;
    app_pc seg_base = NULL;
    app_pc addr = NULL;
    CLIENT_ASSERT(opnd_is_memory_reference(opnd),
                  "opnd_compute_address: must pass memory reference");
    if (opnd_is_far_base_disp(opnd)) {
#ifdef X86
#    ifdef STANDALONE_DECODER
        seg_base = NULL; /* not supported */
#    else
        seg_base = get_app_segment_base(opnd_get_segment(opnd));
        if (seg_base == (app_pc)POINTER_MAX) /* failure */
            seg_base = NULL;
#    endif
#endif
    }
#if defined(X64) || defined(ARM)
    if (IF_X64(opnd_is_abs_addr(opnd) ||) opnd_is_rel_addr(opnd)) {
        return (app_pc)opnd_get_addr(opnd) + (ptr_uint_t)seg_base;
    }
#endif
    addr = seg_base;
    base = opnd_get_base(opnd);
    disp = opnd_get_disp(opnd);
    d_r_logopnd(get_thread_private_dcontext(), 4, opnd, "opnd_compute_address for");
    addr += reg_get_value_priv(base, mc);
    LOG(THREAD_GET, LOG_ALL, 4, "\tbase => " PFX "\n", addr);
    addr += scaled_index;
    LOG(THREAD_GET, LOG_ALL, 4, "\tindex,scale => " PFX "\n", addr);
    addr += disp;
    LOG(THREAD_GET, LOG_ALL, 4, "\tdisp => " PFX "\n", addr);
    return addr;
}

/* Returns the effective address of opnd, computed using the passed-in
 * register values.  If opnd is a far address, ignores that aspect
 * except for TLS references on Windows (fs: for 32-bit, gs: for 64-bit)
 * or typical fs: or gs: references on Linux.  For far addresses the
 * calling thread's segment selector is used.
 *
 * XXX: this does not support VSIB.  All callers should really be switched to
 * use instr_compute_address_ex_priv().
 */
app_pc
opnd_compute_address_priv(opnd_t opnd, priv_mcontext_t *mc)
{
    ptr_int_t scaled_index = 0;
    if (opnd_is_base_disp(opnd)) {
        reg_id_t index = opnd_get_index(opnd);
#if defined(X86)
        ptr_int_t scale = opnd_get_scale(opnd);
        scaled_index = scale * reg_get_value_priv(index, mc);
#elif defined(AARCH64)
        bool scaled = false;
        uint amount = 0;
        dr_extend_type_t type = opnd_get_index_extend(opnd, &scaled, &amount);
        reg_t index_val = reg_get_value_priv(index, mc);
        reg_t extended = 0;
        uint msb = 0;
        switch (type) {
        default: CLIENT_ASSERT(false, "Unsupported extend type"); return NULL;
        case DR_EXTEND_UXTW: extended = (index_val << (63u - 31u)) >> (63u - 31u); break;
        case DR_EXTEND_SXTW:
            extended = (index_val << (63u - 31u)) >> (63u - 31u);
            msb = extended >> 31u;
            if (msb == 1) {
                extended = ((~0ull) << 32u) | extended;
            }
            break;
        case DR_EXTEND_UXTX:
        case DR_EXTEND_SXTX: extended = index_val; break;
        }
        if (scaled) {
            scaled_index = extended << amount;
        } else {
            scaled_index = extended;
        }
#elif defined(ARM)
        uint amount;
        dr_shift_type_t type = opnd_get_index_shift(opnd, &amount);
        reg_t index_val = reg_get_value_priv(index, mc);
        switch (type) {
        case DR_SHIFT_LSL: scaled_index = index_val << amount; break;
        case DR_SHIFT_LSR: scaled_index = index_val >> amount; break;
        case DR_SHIFT_ASR: scaled_index = (ptr_int_t)index_val << amount; break;
        case DR_SHIFT_ROR:
            scaled_index =
                (index_val >> amount) | (index_val << (sizeof(reg_t) * 8 - amount));
            break;
        case DR_SHIFT_RRX:
            scaled_index = (index_val >> 1) |
                (TEST(EFLAGS_C, mc->cpsr) ? (1 << (sizeof(reg_t) * 8 - 1)) : 0);
            break;
        default: scaled_index = index_val;
        }
#elif defined(RISCV64)
        /* FIXME i#3544: Not implemented */
        /* Marking as unused to silence -Wunused-variable. */
        CLIENT_ASSERT(false, "Not implemented");
        (void)index;
        return NULL;
#endif
    }
    return opnd_compute_address_helper(opnd, mc, scaled_index);
}

DR_API
app_pc
opnd_compute_address(opnd_t opnd, dr_mcontext_t *mc)
{
    /* only uses GPRs so we ignore mc.size */
    return opnd_compute_address_priv(opnd, dr_mcontext_as_priv_mcontext(mc));
}

/***************************************************************************
 ***      Register utility functions
 ***************************************************************************/

const char *
get_register_name(reg_id_t reg)
{
    return reg_names[reg];
}

reg_id_t
reg_to_pointer_sized(reg_id_t reg)
{
    return dr_reg_fixer[reg];
}

reg_id_t
reg_32_to_16(reg_id_t reg)
{
#ifdef X86
    CLIENT_ASSERT(reg >= REG_START_32 && reg <= REG_STOP_32,
                  "reg_32_to_16: passed non-32-bit reg");
    return (reg - REG_START_32) + REG_START_16;
#elif defined(AARCHXX)
    CLIENT_ASSERT(false, "reg_32_to_16 not supported on ARM");
    return REG_NULL;
#elif defined(RISCV64)
    /* FIXME i#3544: There is no separate addressing for half registers.
     * Semantics are part of the opcode.
     */
    return reg;
#endif
}

reg_id_t
reg_32_to_8(reg_id_t reg)
{
#ifdef X86
    reg_id_t r8;
    CLIENT_ASSERT(reg >= REG_START_32 && reg <= REG_STOP_32,
                  "reg_32_to_16: passed non-32-bit reg");
    r8 = (reg - REG_START_32) + REG_START_8;
    if (r8 >= REG_START_x86_8 && r8 <= REG_STOP_x86_8) {
#    ifdef X64
        r8 += (REG_START_x64_8 - REG_START_x86_8);
#    else
        r8 = REG_NULL;
#    endif
    }
    return r8;
#elif defined(AARCHXX)
    CLIENT_ASSERT(false, "reg_32_to_8 not supported on ARM");
    return REG_NULL;
#elif defined(RISCV64)
    /* FIXME i#3544: There is no separate addressing for half registers.
     * Semantics are part of the opcode.
     */
    return reg;
#endif
}

#ifdef X64
reg_id_t
reg_32_to_64(reg_id_t reg)
{
#    ifdef AARCH64
    if (reg == DR_REG_WZR)
        return DR_REG_XZR;
#    endif
    CLIENT_ASSERT(reg >= REG_START_32 && reg <= REG_STOP_32,
                  "reg_32_to_64: passed non-32-bit reg");
    return (reg - REG_START_32) + REG_START_64;
}

reg_id_t
reg_64_to_32(reg_id_t reg)
{
#    ifdef AARCH64
    if (reg == DR_REG_XZR)
        return DR_REG_WZR;
#    endif
    CLIENT_ASSERT(reg >= REG_START_64 && reg <= REG_STOP_64,
                  "reg_64_to_32: passed non-64-bit reg");
    return (reg - REG_START_64) + REG_START_32;
}

#    ifdef X86
bool
reg_is_extended(reg_id_t reg)
{
    /* Note that we do consider spl, bpl, sil, and dil to be "extended" */
    return ((reg >= REG_START_64 + 8 && reg <= REG_STOP_64) ||
            (reg >= REG_START_32 + 8 && reg <= REG_STOP_32) ||
            (reg >= REG_START_16 + 8 && reg <= REG_STOP_16) ||
            (reg >= REG_START_8 + 8 && reg <= REG_STOP_8) ||
            (reg >= REG_START_x64_8 && reg <= REG_STOP_x64_8) ||
            ((reg >= DR_REG_START_XMM + 8 && reg <= DR_REG_START_XMM + 15) ||
             (reg >= DR_REG_START_XMM + 24 && reg <= DR_REG_STOP_XMM)) ||
            ((reg >= DR_REG_START_YMM + 8 && reg <= DR_REG_START_YMM + 15) ||
             (reg >= DR_REG_START_YMM + 24 && reg <= DR_REG_STOP_YMM)) ||
            ((reg >= DR_REG_START_ZMM + 8 && reg <= DR_REG_START_ZMM + 15) ||
             (reg >= DR_REG_START_ZMM + 24 && reg <= DR_REG_STOP_ZMM)) ||
            (reg >= REG_START_DR + 8 && reg <= REG_STOP_DR) ||
            (reg >= REG_START_CR + 8 && reg <= REG_STOP_CR));
}

bool
reg_is_avx512_extended(reg_id_t reg)
{
    /* Note that we do consider spl, bpl, sil, and dil to be "extended" */
    return ((reg >= DR_REG_START_XMM + 16 && reg <= DR_REG_STOP_XMM) ||
            (reg >= DR_REG_START_YMM + 16 && reg <= DR_REG_STOP_YMM) ||
            (reg >= DR_REG_START_ZMM + 16 && reg <= DR_REG_STOP_ZMM));
}
#    endif
#endif

reg_id_t
reg_32_to_opsz(reg_id_t reg, opnd_size_t sz)
{
    CLIENT_ASSERT((reg >= REG_START_32 && reg <= REG_STOP_32)
                      IF_AARCH64(|| reg == DR_REG_XZR || reg == DR_REG_WZR),
                  "reg_32_to_opsz: passed non-32-bit reg");
    /* On ARM, we use the same reg for the size of 8, 16, and 32 bit */
    if (sz == OPSZ_4)
        return reg;
    else if (sz == OPSZ_2)
        return IF_AARCHXX_ELSE(reg, reg_32_to_16(reg));
    else if (sz == OPSZ_1)
        return IF_AARCHXX_ELSE(reg, reg_32_to_8(reg));
#ifdef X64
    else if (sz == OPSZ_8)
        return reg_32_to_64(reg);
#endif
    else
        CLIENT_ASSERT(false, "reg_32_to_opsz: invalid size parameter");
    return reg;
}

static reg_id_t
reg_resize_to_zmm(reg_id_t simd_reg)
{
#ifdef X86
    if (reg_is_strictly_xmm(simd_reg)) {
        return simd_reg - DR_REG_START_XMM + DR_REG_START_ZMM;
    } else if (reg_is_strictly_ymm(simd_reg)) {
        return simd_reg - DR_REG_START_YMM + DR_REG_START_ZMM;
    } else if (reg_is_strictly_zmm(simd_reg)) {
        return simd_reg;
    }
    CLIENT_ASSERT(false, "Not a simd register.");
#endif
    return DR_REG_INVALID;
}

static reg_id_t
reg_resize_to_ymm(reg_id_t simd_reg)
{
#ifdef X86
    if (reg_is_strictly_xmm(simd_reg)) {
        return simd_reg - DR_REG_START_XMM + DR_REG_START_YMM;
    } else if (reg_is_strictly_ymm(simd_reg)) {
        return simd_reg;
    } else if (reg_is_strictly_zmm(simd_reg)) {
        return simd_reg - DR_REG_START_ZMM + DR_REG_START_YMM;
    }
    CLIENT_ASSERT(false, "not a simd register.");
#endif
    return DR_REG_INVALID;
}

static reg_id_t
reg_resize_to_xmm(reg_id_t simd_reg)
{
#ifdef X86
    if (reg_is_strictly_xmm(simd_reg)) {
        return simd_reg;
    } else if (reg_is_strictly_ymm(simd_reg)) {
        return simd_reg - DR_REG_START_YMM + DR_REG_START_XMM;
    } else if (reg_is_strictly_zmm(simd_reg)) {
        return simd_reg - DR_REG_START_ZMM + DR_REG_START_XMM;
    }
    CLIENT_ASSERT(false, "not a simd register");
#endif
    return DR_REG_INVALID;
}

reg_id_t
reg_resize_to_opsz(reg_id_t reg, opnd_size_t sz)
{
    if (reg_is_gpr(reg) IF_AARCH64(|| reg == DR_REG_XZR || reg == DR_REG_WZR)) {
        reg = reg_to_pointer_sized(reg);
        return reg_32_to_opsz(IF_X64_ELSE(reg_64_to_32(reg), reg), sz);
    } else if (reg_is_strictly_xmm(reg) || reg_is_strictly_ymm(reg) ||
               reg_is_strictly_zmm(reg)) {
        if (sz == OPSZ_16) {
            return reg_resize_to_xmm(reg);
        } else if (sz == OPSZ_32) {
            return reg_resize_to_ymm(reg);
        } else if (sz == OPSZ_64) {
            return reg_resize_to_zmm(reg);
        } else {
            CLIENT_ASSERT(false, "invalid size for simd register");
        }
    } else if (reg_is_simd(reg)) {
        if (reg_get_size(reg) == sz)
            return reg;
        /* XXX i#1569: Add aarchxx SIMD conversions here. */
        CLIENT_ASSERT(false, "reg_resize_to_opsz: unsupported reg");
    } else {
        CLIENT_ASSERT(false, "reg_resize_to_opsz: unsupported reg");
    }
    return DR_REG_INVALID;
}

int
reg_parameter_num(reg_id_t reg)
{
    int r;
    for (r = 0; r < NUM_REGPARM; r++) {
        if (reg == d_r_regparms[r])
            return r;
    }
    return -1;
}

int
opnd_get_reg_mcontext_offs(reg_id_t reg)
{
    return opnd_get_reg_dcontext_offs(reg) - MC_OFFS;
}

bool
reg_overlap(reg_id_t r1, reg_id_t r2)
{
    if (r1 == REG_NULL || r2 == REG_NULL)
        return false;
#ifdef X86
    /* The XH registers do NOT overlap with the XL registers; else, the
     * dr_reg_fixer is the answer.
     */
    if ((r1 >= REG_START_8HL && r1 <= REG_STOP_8HL) &&
        (r2 >= REG_START_8HL && r2 <= REG_STOP_8HL) && r1 != r2)
        return false;
#endif
    return (dr_reg_fixer[r1] == dr_reg_fixer[r2]);
}

/* returns the register's representation as 3 bits in a modrm byte,
 * callers do not expect it to fail
 */
enum { REG_INVALID_BITS = 0x0 }; /* returns a valid register nevertheless */
byte
reg_get_bits(reg_id_t reg)
{
#ifdef X86
#    ifdef X64
    if (reg >= REG_START_64 && reg <= REG_STOP_64)
        return (byte)((reg - REG_START_64) % 8);
#    endif
    if (reg >= REG_START_32 && reg <= REG_STOP_32)
        return (byte)((reg - REG_START_32) % 8);
    if (reg >= REG_START_8 && reg <= REG_R15L)
        return (byte)((reg - REG_START_8) % 8);
#    ifdef X64
    if (reg >= REG_START_x64_8 && reg <= REG_STOP_x64_8) /* alternates to AH-BH */
        return (byte)((reg - REG_START_x64_8 + 4) % 8);
#    endif
    if (reg >= REG_START_16 && reg <= REG_STOP_16)
        return (byte)((reg - REG_START_16) % 8);
    if (reg >= REG_START_MMX && reg <= REG_STOP_MMX)
        return (byte)((reg - REG_START_MMX) % 8);
    if (reg >= DR_REG_START_XMM && reg <= DR_REG_STOP_XMM)
        return (byte)((reg - DR_REG_START_XMM) % 8);
    if (reg >= DR_REG_START_YMM && reg <= DR_REG_STOP_YMM)
        return (byte)((reg - DR_REG_START_YMM) % 8);
    if (reg >= DR_REG_START_ZMM && reg <= DR_REG_STOP_ZMM)
        return (byte)((reg - DR_REG_START_ZMM) % 8);
    if (reg >= DR_REG_START_BND && reg <= DR_REG_STOP_BND)
        return (byte)((reg - DR_REG_START_BND) % 4);
    if (reg >= DR_REG_START_OPMASK && reg <= DR_REG_STOP_OPMASK)
        return (byte)((reg - DR_REG_START_OPMASK) % 8);
    if (reg >= REG_START_SEGMENT && reg <= REG_STOP_SEGMENT)
        return (byte)((reg - REG_START_SEGMENT) % 8);
    if (reg >= REG_START_DR && reg <= REG_STOP_DR)
        return (byte)((reg - REG_START_DR) % 8);
    if (reg >= REG_START_CR && reg <= REG_STOP_CR)
        return (byte)((reg - REG_START_CR) % 8);
#else
    CLIENT_ASSERT(false, "i#1551: NYI");
#endif
    CLIENT_ASSERT(false, "reg_get_bits: invalid register");
    return REG_INVALID_BITS; /* callers don't expect a failure - return some value */
}

/* returns the OPSZ_ field appropriate for the register */
opnd_size_t
reg_get_size(reg_id_t reg)
{
#ifdef X64
    if (reg >= REG_START_64 && reg <= REG_STOP_64)
        return OPSZ_8;
#endif
    if (reg >= REG_START_32 && reg <= REG_STOP_32)
        return OPSZ_4;
#ifdef X86
    if (reg >= REG_START_8 && reg <= REG_STOP_8)
        return OPSZ_1;
#endif
#if defined(X86) && defined(X64)
    if (reg >= REG_START_x64_8 && reg <= REG_STOP_x64_8) /* alternates to AH-BH */
        return OPSZ_1;
#endif
#ifdef X86
    if (reg >= REG_START_16 && reg <= REG_STOP_16)
        return OPSZ_2;
    if (reg >= REG_START_MMX && reg <= REG_STOP_MMX)
        return OPSZ_8;
    if (reg >= DR_REG_START_XMM && reg <= DR_REG_STOP_XMM)
        return OPSZ_16;
    if (reg >= DR_REG_START_YMM && reg <= DR_REG_STOP_YMM)
        return OPSZ_32;
    if (reg >= DR_REG_START_ZMM && reg <= DR_REG_STOP_ZMM)
        return OPSZ_64;
    if (reg >= DR_REG_START_OPMASK && reg <= DR_REG_STOP_OPMASK) {
        /* The default is 16 bits wide. The register may be up to 64 bits wide with
         * the AVX-512BW extension, which depends on the processor. The number of
         * bits actually used depends on the vector type of the instruction.
         */
        return OPSZ_8;
    }
    if (reg >= DR_REG_START_BND && reg <= DR_REG_STOP_BND)
        return IF_X64_ELSE(OPSZ_16, OPSZ_8);
    if (reg >= REG_START_SEGMENT && reg <= REG_STOP_SEGMENT)
        return OPSZ_2;
    if (reg >= REG_START_DR && reg <= REG_STOP_DR)
        return IF_X64_ELSE(OPSZ_8, OPSZ_4);
    if (reg >= REG_START_CR && reg <= REG_STOP_CR)
        return IF_X64_ELSE(OPSZ_8, OPSZ_4);
    /* i#176 add reg size handling for floating point registers */
    if (reg >= REG_START_FLOAT && reg <= REG_STOP_FLOAT)
        return OPSZ_10;
#elif defined(AARCHXX)
    if (reg >= DR_REG_Q0 && reg <= DR_REG_Q31)
        return OPSZ_16;
    if (reg >= DR_REG_D0 && reg <= DR_REG_D31)
        return OPSZ_8;
    if (reg >= DR_REG_S0 && reg <= DR_REG_S31)
        return OPSZ_4;
    if (reg >= DR_REG_H0 && reg <= DR_REG_H31)
        return OPSZ_2;
    if (reg >= DR_REG_B0 && reg <= DR_REG_B31)
        return OPSZ_1;
#    ifdef ARM
    if (reg >= DR_REG_CR0 && reg <= DR_REG_CR15)
        return OPSZ_PTR;
    if (reg >= DR_REG_CPSR && reg <= DR_REG_FPSCR)
        return OPSZ_4;
#    elif defined(AARCH64)
    if (reg == DR_REG_XZR)
        return OPSZ_8;
    if (reg == DR_REG_WZR)
        return OPSZ_4;
    if (reg >= DR_REG_MDCCSR_EL0 && reg <= DR_REG_SPSR_FIQ)
        return OPSZ_8;
    if (reg >= DR_REG_Z0 && reg <= DR_REG_Z31) {
#        if !defined(DR_HOST_NOT_TARGET) && !defined(STANDALONE_DECODER)
        return opnd_size_from_bytes(proc_get_vector_length_bytes());
#        else
        return OPSZ_SCALABLE;
#        endif
    }
    if ((reg >= DR_REG_P0 && reg <= DR_REG_P15) || reg == DR_REG_FFR)
        return OPSZ_SCALABLE_PRED;
    if (reg == DR_REG_CNTVCT_EL0)
        return OPSZ_8;
    if (reg >= DR_REG_NZCV && reg <= DR_REG_FPSR)
        return OPSZ_8;
#    endif
    if (reg == DR_REG_TPIDRURW || reg == DR_REG_TPIDRURO)
        return OPSZ_PTR;
#endif
    LOG(GLOBAL, LOG_ANNOTATIONS, 2, "reg=%d, %s, last reg=%d\n", reg,
        get_register_name(reg), DR_REG_LAST_ENUM);
    CLIENT_ASSERT(false, "reg_get_size: invalid register");
    return OPSZ_NA;
}

#ifndef STANDALONE_DECODER
/****************************************************************************/
/* dcontext convenience routines */
static opnd_t
dcontext_opnd_common(dcontext_t *dcontext, bool absolute, reg_id_t basereg, int offs,
                     opnd_size_t size)
{
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));
    /* offs is not raw offset, but includes upcontext size, so we
     * can tell unprotected from normal
     */
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask) &&
        offs < sizeof(unprotected_context_t)) {
        return opnd_create_base_disp(
            absolute ? REG_NULL : (basereg == REG_NULL ? REG_DCXT_PROT : basereg),
            REG_NULL, 0,
            ((int)(ptr_int_t)(absolute ? dcontext->upcontext.separate_upcontext : 0)) +
                offs,
            size);
    } else {
        if (offs >= sizeof(unprotected_context_t))
            offs -= sizeof(unprotected_context_t);
        return opnd_create_base_disp(
            absolute ? REG_NULL : (basereg == REG_NULL ? REG_DCXT : basereg), REG_NULL, 0,
            ((int)(ptr_int_t)(absolute ? dcontext : 0)) + offs, size);
    }
}

opnd_t
opnd_create_dcontext_field_sz(dcontext_t *dcontext, int offs, opnd_size_t sz)
{
    return dcontext_opnd_common(dcontext, true, REG_NULL, offs, sz);
}

opnd_t
opnd_create_dcontext_field(dcontext_t *dcontext, int offs)
{
    return dcontext_opnd_common(dcontext, true, REG_NULL, offs, OPSZ_PTR);
}

/* use basereg==REG_NULL to get default (xdi, or xsi for upcontext) */
opnd_t
opnd_create_dcontext_field_via_reg_sz(dcontext_t *dcontext, reg_id_t basereg, int offs,
                                      opnd_size_t sz)
{
    return dcontext_opnd_common(dcontext, false, basereg, offs, sz);
}

/* use basereg==REG_NULL to get default (xdi, or xsi for upcontext) */
opnd_t
opnd_create_dcontext_field_via_reg(dcontext_t *dcontext, reg_id_t basereg, int offs)
{
    return dcontext_opnd_common(dcontext, false, basereg, offs, OPSZ_PTR);
}

opnd_t
opnd_create_dcontext_field_byte(dcontext_t *dcontext, int offs)
{
    return dcontext_opnd_common(dcontext, true, REG_NULL, offs, OPSZ_1);
}

opnd_t
update_dcontext_address(opnd_t op, dcontext_t *old_dcontext, dcontext_t *new_dcontext)
{
    int offs;
    CLIENT_ASSERT(opnd_is_near_base_disp(op) && opnd_get_base(op) == REG_NULL &&
                      opnd_get_index(op) == REG_NULL,
                  "update_dcontext_address: invalid opnd");
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    offs = opnd_get_disp(op) - (uint)(ptr_uint_t)old_dcontext;
    if (offs >= 0 && offs < sizeof(dcontext_t)) {
        /* don't pass raw offset, add in upcontext size */
        offs += sizeof(unprotected_context_t);
        return opnd_create_dcontext_field(new_dcontext, offs);
    }
    /* some fields are in a separate memory region! */
    else {
        CLIENT_ASSERT(TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask),
                      "update_dcontext_address: inconsistent layout");
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        offs = opnd_get_disp(op) -
            (uint)(ptr_uint_t)(old_dcontext->upcontext.separate_upcontext);
        if (offs >= 0 && offs < sizeof(unprotected_context_t)) {
            /* raw offs is what we want for upcontext */
            return opnd_create_dcontext_field(new_dcontext, offs);
        }
    }
    /* not a dcontext offset: just return original value */
    return op;
}

opnd_t
opnd_create_tls_slot(int offs)
{
    return opnd_create_sized_tls_slot(offs, OPSZ_PTR);
}

#endif /* !STANDALONE_DECODER */
/****************************************************************************/
