/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "decode_private.h"
#include "instr_create_shared.h"

#ifdef X64
/*
 * Each instruction stores whether it should be interpreted in 32-bit
 * (x86) or 64-bit (x64) mode.  This routine sets the mode for \p instr.
 */
void
instr_set_x86_mode(instr_t *instr, bool x86)
{
    if (x86)
        instr->flags |= INSTR_X86_MODE;
    else
        instr->flags &= ~INSTR_X86_MODE;
}

/*
 * Each instruction stores whether it should be interpreted in 32-bit
 * (x86) or 64-bit (x64) mode.  This routine returns the mode for \p instr.
 */
bool
instr_get_x86_mode(instr_t *instr)
{
    return TEST(INSTR_X86_MODE, instr->flags);
}
#endif

bool
instr_set_isa_mode(instr_t *instr, dr_isa_mode_t mode)
{
#ifdef X64
    if (mode == DR_ISA_IA32)
        instr_set_x86_mode(instr, true);
    else if (mode == DR_ISA_AMD64)
        instr_set_x86_mode(instr, false);
    else
        return false;
#else
    if (mode != DR_ISA_IA32)
        return false;
#endif
    return true;
}

dr_isa_mode_t
instr_get_isa_mode(instr_t *instr)
{
#ifdef X64
    return TEST(INSTR_X86_MODE, instr->flags) ? DR_ISA_IA32 : DR_ISA_AMD64;
#else
    return DR_ISA_IA32;
#endif
}

int
instr_length_arch(dcontext_t *dcontext, instr_t *instr)
{
    /* hardcode length for cti */
    switch (instr_get_opcode(instr)) {
    case OP_jmp:
    case OP_call:
        /* XXX i#1315: we should support 2-byte immeds => length 3 */
        return 5;
    case OP_jb:
    case OP_jnb:
    case OP_jbe:
    case OP_jnbe:
    case OP_jl:
    case OP_jnl:
    case OP_jle:
    case OP_jnle:
    case OP_jo:
    case OP_jno:
    case OP_jp:
    case OP_jnp:
    case OP_js:
    case OP_jns:
    case OP_jz:
    case OP_jnz:
        /* XXX i#1315: we should support 2-byte immeds => length 4+ */
        return 6 +
            ((TEST(PREFIX_JCC_TAKEN, instr_get_prefixes(instr)) ||
              TEST(PREFIX_JCC_NOT_TAKEN, instr_get_prefixes(instr)))
                 ? 1
                 : 0);
    case OP_jb_short:
    case OP_jnb_short:
    case OP_jbe_short:
    case OP_jnbe_short:
    case OP_jl_short:
    case OP_jnl_short:
    case OP_jle_short:
    case OP_jnle_short:
    case OP_jo_short:
    case OP_jno_short:
    case OP_jp_short:
    case OP_jnp_short:
    case OP_js_short:
    case OP_jns_short:
    case OP_jz_short:
    case OP_jnz_short:
        return 2 +
            ((TEST(PREFIX_JCC_TAKEN, instr_get_prefixes(instr)) ||
              TEST(PREFIX_JCC_NOT_TAKEN, instr_get_prefixes(instr)))
                 ? 1
                 : 0);
        /* alternative names (e.g., OP_jae_short) are equivalent,
         * so don't need to list them */
    case OP_jmp_short: return 2;
    case OP_jecxz:
    case OP_loop:
    case OP_loope:
    case OP_loopne:
        if (opnd_get_reg(instr_get_src(instr, 1)) !=
            REG_XCX IF_X64(&&!instr_get_x86_mode(instr)))
            return 3; /* need addr prefix */
        else
            return 2;
    case OP_LABEL: return 0;
    case OP_xbegin:
        /* XXX i#1315: we should support 2-byte immeds => length 4 */
        return 6;
    default: return -1;
    }
}

bool
opc_is_not_a_real_memory_load(int opc)
{
    /* lea has a mem_ref source operand, but doesn't actually read */
    if (opc == OP_lea)
        return true;
    /* The multi-byte nop has a mem/reg source operand, but it does not read. */
    if (opc == OP_nop_modrm)
        return true;
    return false;
}

/* Returns whether ordinal is within the count of memory references
 * (i.e., the caller should iterate, incrementing ordinal by one,
 * until it returns false).
 * If it returns true, sets *selected to whether this memory
 * reference actually goes through (i.e., whether it is enabled in
 * the mask).
 * If *selected is true, returns the scaled index in *result.
 *
 * On a fault, any completed memory loads have their corresponding
 * mask bits cleared, so we shouldn't have to do anything special
 * to support faults of VSIB accesses.
 */
static bool
instr_compute_VSIB_index(bool *selected OUT, app_pc *result OUT, bool *is_write OUT,
                         instr_t *instr, int ordinal, priv_mcontext_t *mc, size_t mc_size,
                         dr_mcontext_flags_t mc_flags)
{
    CLIENT_ASSERT(selected != NULL && result != NULL && mc != NULL,
                  "vsib address computation: invalid args");
    CLIENT_ASSERT(TEST(DR_MC_MULTIMEDIA, mc_flags),
                  "dr_mcontext_t.flags must include DR_MC_MULTIMEDIA");
    opnd_t src0 = instr_get_src(instr, 0);
    /* We detect whether the instruction is EVEX by looking at its potential mask operand.
     */
    bool is_evex = opnd_is_reg(src0) && reg_is_opmask(opnd_get_reg(src0));
    int opc = instr_get_opcode(instr);
    opnd_size_t index_size = OPSZ_NA;
    opnd_size_t mem_size = OPSZ_NA;
    switch (opc) {
    case OP_vgatherdpd:
        index_size = OPSZ_4;
        mem_size = OPSZ_8;
        *is_write = false;
        break;
    case OP_vgatherqpd:
        index_size = OPSZ_8;
        mem_size = OPSZ_8;
        *is_write = false;
        break;
    case OP_vgatherdps:
        index_size = OPSZ_4;
        mem_size = OPSZ_4;
        *is_write = false;
        break;
    case OP_vgatherqps:
        index_size = OPSZ_8;
        mem_size = OPSZ_4;
        *is_write = false;
        break;
    case OP_vpgatherdd:
        index_size = OPSZ_4;
        mem_size = OPSZ_4;
        *is_write = false;
        break;
    case OP_vpgatherqd:
        index_size = OPSZ_8;
        mem_size = OPSZ_4;
        *is_write = false;
        break;
    case OP_vpgatherdq:
        index_size = OPSZ_4;
        mem_size = OPSZ_8;
        *is_write = false;
        break;
    case OP_vpgatherqq:
        index_size = OPSZ_8;
        mem_size = OPSZ_8;
        *is_write = false;
        break;
    case OP_vscatterdpd:
        index_size = OPSZ_4;
        mem_size = OPSZ_8;
        *is_write = true;
        break;
    case OP_vscatterqpd:
        index_size = OPSZ_8;
        mem_size = OPSZ_8;
        *is_write = true;
        break;
    case OP_vscatterdps:
        index_size = OPSZ_4;
        mem_size = OPSZ_4;
        *is_write = true;
        break;
    case OP_vscatterqps:
        index_size = OPSZ_8;
        mem_size = OPSZ_4;
        *is_write = true;
        break;
    case OP_vpscatterdd:
        index_size = OPSZ_4;
        mem_size = OPSZ_4;
        *is_write = true;
        break;
    case OP_vpscatterqd:
        index_size = OPSZ_8;
        mem_size = OPSZ_4;
        *is_write = true;
        break;
    case OP_vpscatterdq:
        index_size = OPSZ_4;
        mem_size = OPSZ_8;
        *is_write = true;
        break;
    case OP_vpscatterqq:
        index_size = OPSZ_8;
        mem_size = OPSZ_8;
        *is_write = true;
        break;
    default: CLIENT_ASSERT(false, "non-VSIB opcode passed in"); return false;
    }
    opnd_t memop;
    reg_id_t mask_reg;
    if (is_evex) {
        /* We assume that all EVEX VSIB-using instructions have the VSIB memop as the 2nd
         * source and the (EVEX-)mask register as the 1st source for gather reads, and the
         * VSIB memop as the first destination for scatter writes.
         */
        if (*is_write)
            memop = instr_get_dst(instr, 0);
        else
            memop = instr_get_src(instr, 1);
        mask_reg = opnd_get_reg(instr_get_src(instr, 0));
    } else {
        /* We assume that all VEX VSIB-using instructions have the VSIB memop as the 1st
         * source and the mask register as the 2nd source. There are no VEX encoded AVX
         * scatter instructions.
         */
        memop = instr_get_src(instr, 0);
        mask_reg = opnd_get_reg(instr_get_src(instr, 1));
    }
    int scale = opnd_get_scale(memop);
    int index_reg_start;
    int mask_reg_start;
    uint64 index_addr;
    reg_id_t index_reg = opnd_get_index(memop);
    if (reg_get_size(index_reg) == OPSZ_64) {
        CLIENT_ASSERT(mc_size >= offsetof(dr_mcontext_t, simd) +
                              MCXT_NUM_SIMD_SSE_AVX_SLOTS * ZMM_REG_SIZE,
                      "Incompatible client, invalid dr_mcontext_t.size.");
        index_reg_start = DR_REG_START_ZMM;
    } else if (reg_get_size(index_reg) == OPSZ_32) {
        CLIENT_ASSERT(
            mc_size >= offsetof(dr_mcontext_t, simd) +
                    /* With regards to backward compatibility, ymm size slots were already
                     * there, and this is what we need to make the version check for.
                     */
                    MCXT_NUM_SIMD_SSE_AVX_SLOTS * YMM_REG_SIZE,
            "Incompatible client, invalid dr_mcontext_t.size.");
        index_reg_start = DR_REG_START_YMM;
    } else {
        CLIENT_ASSERT(mc_size >= offsetof(dr_mcontext_t, simd) +
                              MCXT_NUM_SIMD_SSE_AVX_SLOTS * YMM_REG_SIZE,
                      "Incompatible client, invalid dr_mcontext_t.size.");
        index_reg_start = DR_REG_START_XMM;
    }
    /* Size check for upper 16 AVX-512 registers, requiring updated dr_mcontext_t simd
     * size.
     */
    CLIENT_ASSERT(index_reg - index_reg_start < MCXT_NUM_SIMD_SSE_AVX_SLOTS ||
                      mc_size >= offsetof(dr_mcontext_t, simd) +
                              MCXT_NUM_SIMD_SSE_AVX_SLOTS * ZMM_REG_SIZE,
                  "Incompatible client, invalid dr_mcontext_t.size.");
    if (is_evex)
        mask_reg_start = DR_REG_START_OPMASK;
    else
        mask_reg_start = index_reg_start;

    LOG(THREAD_GET, LOG_ALL, 4,
        "%s: ordinal=%d: index size=%s, mem size=%s, index reg=%s\n", __FUNCTION__,
        ordinal, size_names[index_size], size_names[mem_size], reg_names[index_reg]);

    if (index_size == OPSZ_4) {
        int mask;
        if (ordinal >= (int)opnd_size_in_bytes(reg_get_size(index_reg)) /
                (int)opnd_size_in_bytes(mem_size))
            return false;
        if (is_evex) {
            mask = (mc->opmask[mask_reg - mask_reg_start] >> ordinal) & 0x1;
            if (mask == 0) { /* mask bit not set */
                *selected = false;
                return true;
            }
        } else {
            mask = (int)mc->simd[mask_reg - mask_reg_start].u32[ordinal];
            if (mask >= 0) { /* top bit not set */
                *selected = false;
                return true;
            }
        }
        *selected = true;
        index_addr = mc->simd[index_reg - index_reg_start].u32[ordinal];
    } else if (index_size == OPSZ_8) {
        int mask;
        /* For qword indices, the number of ordinals is not dependent on the mem_size,
         * therefore we can divide by opnd_size_in_bytes(index_size).
         */
        if (ordinal >= (int)opnd_size_in_bytes(reg_get_size(index_reg)) /
                (int)opnd_size_in_bytes(index_size))
            return false;
        if (is_evex) {
            mask = (mc->opmask[mask_reg - mask_reg_start] >> ordinal) & 0x1;
            if (mask == 0) { /* mask bit not set */
                *selected = false;
                return true;
            }
        } else {
            /* just top half */
            mask = (int)mc->simd[mask_reg - mask_reg_start].u32[ordinal * 2 + 1];
            if (mask >= 0) { /* top bit not set */
                *selected = false;
                return true;
            }
        }
        *selected = true;
#ifdef X64
        index_addr = mc->simd[index_reg - index_reg_start].reg[ordinal];
#else
        index_addr =
            (((uint64)mc->simd[index_reg - index_reg_start].u32[ordinal * 2 + 1]) << 32) |
            mc->simd[index_reg - index_reg_start].u32[ordinal * 2];
#endif
    } else
        return false;

    LOG(THREAD_GET, LOG_ALL, 4, "%s: ordinal=%d: " PFX "*%d=" PFX "\n", __FUNCTION__,
        ordinal, index_addr, scale, index_addr * scale);

    index_addr *= scale;
#ifdef X64
    *result = (app_pc)index_addr;
#else
    *result = (app_pc)(uint)index_addr; /* truncated */
#endif
    return true;
}

bool
instr_compute_address_VSIB(instr_t *instr, priv_mcontext_t *mc, size_t mc_size,
                           dr_mcontext_flags_t mc_flags, opnd_t curop, uint index,
                           OUT bool *have_addr, OUT app_pc *addr, OUT bool *write)
{
    /* We assume that any instr w/ a VSIB opnd has no other
     * memory reference (and the VSIB is a source)!  Else we'll
     * have to be more careful w/ memcount, as we have multiple
     * iters in the VSIB.
     */
    bool selected = false;
    /* XXX: b/c we have no iterator state we have to repeat the
     * full iteration on each call
     */
    uint vsib_idx = 0;
    bool is_write = false;
    *have_addr = true;
    while (instr_compute_VSIB_index(&selected, addr, &is_write, instr, vsib_idx, mc,
                                    mc_size, mc_flags) &&
           (!selected || vsib_idx < index)) {
        vsib_idx++;
        selected = false;
    }
    if (selected && vsib_idx == index) {
        *write = is_write;
        if (addr != NULL) {
            /* Add in seg, base, and disp */
            *addr = opnd_compute_address_helper(curop, mc, (ptr_int_t)*addr);
        }
        return true;
    } else
        return false;
}

/* return the branch type of the (branch) inst */
uint
instr_branch_type(instr_t *cti_instr)
{
    switch (instr_get_opcode(cti_instr)) {
    case OP_call: return LINK_DIRECT | LINK_CALL; /* unconditional */
    case OP_jmp_short:
    case OP_jmp: return LINK_DIRECT | LINK_JMP; /* unconditional */
    case OP_ret: return LINK_INDIRECT | LINK_RETURN;
    case OP_jmp_ind: return LINK_INDIRECT | LINK_JMP;
    case OP_call_ind: return LINK_INDIRECT | LINK_CALL;
    case OP_jb_short:
    case OP_jnb_short:
    case OP_jbe_short:
    case OP_jnbe_short:
    case OP_jl_short:
    case OP_jnl_short:
    case OP_jle_short:
    case OP_jnle_short:
    case OP_jo_short:
    case OP_jno_short:
    case OP_jp_short:
    case OP_jnp_short:
    case OP_js_short:
    case OP_jns_short:
    case OP_jz_short:
    case OP_jnz_short:
        /* alternative names (e.g., OP_jae_short) are equivalent,
         * so don't need to list them */
    case OP_jecxz:
    case OP_loop:
    case OP_loope:
    case OP_loopne:
    case OP_jb:
    case OP_jnb:
    case OP_jbe:
    case OP_jnbe:
    case OP_jl:
    case OP_jnl:
    case OP_jle:
    case OP_jnle:
    case OP_jo:
    case OP_jno:
    case OP_jp:
    case OP_jnp:
    case OP_js:
    case OP_jns:
    case OP_jz:
    case OP_jnz: return LINK_DIRECT | LINK_JMP; /* conditional */
    case OP_jmp_far:
        /* far direct is treated as indirect (i#823) */
        return LINK_INDIRECT | LINK_JMP | LINK_FAR;
    case OP_jmp_far_ind: return LINK_INDIRECT | LINK_JMP | LINK_FAR;
    case OP_call_far:
        /* far direct is treated as indirect (i#823) */
        return LINK_INDIRECT | LINK_CALL | LINK_FAR;
    case OP_call_far_ind: return LINK_INDIRECT | LINK_CALL | LINK_FAR;
    case OP_ret_far:
    case OP_iret: return LINK_INDIRECT | LINK_RETURN | LINK_FAR;
    default:
        LOG(THREAD_GET, LOG_ALL, 0, "branch_type: unknown opcode: %d\n",
            instr_get_opcode(cti_instr));
        CLIENT_ASSERT(false, "instr_branch_type: unknown opcode");
    }

    return LINK_INDIRECT;
}

bool
instr_is_mov(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_mov_st || opc == OP_mov_ld || opc == OP_mov_imm ||
            opc == OP_mov_seg || opc == OP_mov_priv);
}

bool
instr_is_call_arch(instr_t *instr)
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    return (opc == OP_call || opc == OP_call_far || opc == OP_call_ind ||
            opc == OP_call_far_ind);
}

bool
instr_is_call_direct(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_call || opc == OP_call_far);
}

bool
instr_is_near_call_direct(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_call);
}

bool
instr_is_call_indirect(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_call_ind || opc == OP_call_far_ind);
}

bool
instr_is_return(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_ret || opc == OP_ret_far || opc == OP_iret);
}

/*** WARNING!  The following rely on ordering of opcodes! ***/

bool
opc_is_cbr_arch(int opc)
{
    return ((opc >= OP_jo && opc <= OP_jnle) ||
            (opc >= OP_jo_short && opc <= OP_jnle_short) ||
            (opc >= OP_loopne && opc <= OP_jecxz));
}

bool
instr_is_cbr_arch(instr_t *instr) /* conditional branch */
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    return opc_is_cbr_arch(opc);
}

bool
instr_is_mbr_arch(instr_t *instr) /* multi-way branch */
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    return (opc == OP_jmp_ind || opc == OP_call_ind || opc == OP_ret ||
            opc == OP_jmp_far_ind || opc == OP_call_far_ind || opc == OP_ret_far ||
            opc == OP_iret);
}

bool
instr_is_jump_mem(instr_t *instr)
{
    return instr_get_opcode(instr) == OP_jmp_ind &&
        opnd_is_memory_reference(instr_get_target(instr));
}

bool
instr_is_far_cti(instr_t *instr) /* target address has a segment and offset */
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_jmp_far || opc == OP_call_far || opc == OP_jmp_far_ind ||
            opc == OP_call_far_ind || opc == OP_ret_far || opc == OP_iret);
}

bool
instr_is_far_abs_cti(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_jmp_far || opc == OP_call_far);
}

bool
instr_is_ubr_arch(instr_t *instr) /* unconditional branch */
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    return (opc == OP_jmp || opc == OP_jmp_short || opc == OP_jmp_far);
}

bool
instr_is_near_ubr(instr_t *instr) /* unconditional branch */
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_jmp || opc == OP_jmp_short);
}

/* This routine does NOT decode the cti of instr if the raw bits are valid,
 * since all short ctis have single-byte opcodes and so just grabbing the first
 * byte can tell if instr is a cti short
 */
bool
instr_is_cti_short(instr_t *instr)
{
    int opc;
    if (instr_opcode_valid(instr)) /* 1st choice: set opcode */
        opc = instr_get_opcode(instr);
    else if (instr_raw_bits_valid(instr)) { /* 2nd choice: 1st byte */
        /* get raw opcode
         * FIXME: figure out which callers really rely on us not
         * up-decoding here -- if nobody then just do the
         * instr_get_opcode() and get rid of all this
         */
        opc = (int)*(instr_get_raw_bits(instr));
        return (opc == RAW_OPCODE_jmp_short ||
                (opc >= RAW_OPCODE_jcc_short_start && opc <= RAW_OPCODE_jcc_short_end) ||
                (opc >= RAW_OPCODE_loop_start && opc <= RAW_OPCODE_loop_end));
    } else /* ok, fine, decode opcode */
        opc = instr_get_opcode(instr);
    return (opc == OP_jmp_short || (opc >= OP_jo_short && opc <= OP_jnle_short) ||
            (opc >= OP_loopne && opc <= OP_jecxz));
}

bool
instr_is_cti_loop(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    /* only looking for loop* and jecxz */
    return (opc >= OP_loopne && opc <= OP_jecxz);
}

/* Checks whether instr is a jecxz/loop* that was originally an app instruction.
 * All such app instructions are mangled into a jecxz/loop*,jmp_short,jmp sequence.
 * If pc != NULL, pc is expected to point the the beginning of the encoding of
 * instr, and the following instructions are assumed to be encoded in sequence
 * after instr.
 * Otherwise, the encoding is expected to be found in instr's allocated bits.
 * This routine does NOT decode instr to the opcode level.
 * The caller should remangle any short-rewrite cti before calling this routine.
 */
bool
instr_is_cti_short_rewrite(instr_t *instr, byte *pc)
{
    /* ASSUMPTION: all app jecxz/loop* are converted to the pattern
     * (jecxz/loop*,jmp_short,jmp), and all jecxz/loop* generated by DynamoRIO
     * DO NOT MATCH THAT PATTERN.
     *
     * For clients, I belive we're robust in the presence of a client adding a
     * pattern that matches ours exactly: decode_fragment() won't think it's an
     * exit cti if it's in a fine-grained fragment where we have Linkstubs.  Since
     * bb building marks as non-coarse if a client adds any cti at all (meta or
     * not), we're protected there.  The other uses of remangle are in perscache,
     * which is only for coarse once again (coarse in general has a hard time
     * finding exit ctis: case 8711/PR 213146), and instr_expand(), which shouldn't
     * be used in the presence of clients w/ bb hooks.
     * Note that we now help clients make jecxz/loop transformations that look
     * just like ours: instr_convert_short_meta_jmp_to_long() (PR 266292).
     */
    if (pc == NULL) {
        if (!instr_has_allocated_bits(instr))
            return false;
        pc = instr_get_raw_bits(instr);
        if (*pc == ADDR_PREFIX_OPCODE) {
            pc++;
            if (instr->length != CTI_SHORT_REWRITE_LENGTH + 1)
                return false;
        } else if (instr->length != CTI_SHORT_REWRITE_LENGTH)
            return false;
    } else {
        if (*pc == ADDR_PREFIX_OPCODE)
            pc++;
    }
    if (instr_opcode_valid(instr)) {
        int opc = instr_get_opcode(instr);
        if (opc < OP_loopne || opc > OP_jecxz)
            return false;
    } else {
        /* don't require decoding to opcode level */
        int raw_opc = (int)*(pc);
        if (raw_opc < RAW_OPCODE_loop_start || raw_opc > RAW_OPCODE_loop_end)
            return false;
    }
    /* now check remaining undecoded bytes */
    if (*(pc + 2) != decode_first_opcode_byte(OP_jmp_short))
        return false;
    if (*(pc + 4) != decode_first_opcode_byte(OP_jmp))
        return false;
    return true;
}

bool
instr_is_interrupt(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_int || opc == OP_int3 || opc == OP_into);
}

bool
instr_is_syscall(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    /* FIXME: Intel processors treat "syscall" as invalid in 32-bit mode;
     * do we need to treat it specially? */
    if (opc == OP_sysenter || opc == OP_syscall)
        return true;
    if (opc == OP_int) {
        int num = instr_get_interrupt_number(instr);
#ifdef WINDOWS
        return ((byte)num == 0x2e);
#else
#    ifdef VMX86_SERVER
        return ((byte)num == 0x80 || (byte)num == VMKUW_SYSCALL_GATEWAY);
#    elif defined(MACOS)
        return ((byte)num == 0x80 || /* BSD syscall */
                (byte)num == 0x81 || /* Mach syscall */
                (byte)num == 0x82);  /* Mach machine-dependent syscall */
#    else
        return ((byte)num == 0x80);
#    endif
#endif
    }
#ifdef WINDOWS
    /* PR 240258 (WOW64): consider this a syscall */
    if (instr_is_wow64_syscall(instr))
        return true;
#endif
    return false;
}

#ifdef WINDOWS
DR_API
bool
instr_is_wow64_syscall(instr_t *instr)
{
    /* TODO i#5949: add support for standalone decoding of a single instr ignoring
     * the host platform.  It's not clear how best to do this for matching things
     * like "call %edx": should we instead provide instr_is_maybe_syscall(), and
     * additionally have it take in the prior and subsequent instructions or
     * PC-to-decode for prior and subsequent?
     */
#    ifdef STANDALONE_DECODER
    /* We don't have get_os_version(), etc., and we assume this routine is not needed */
    return false;
#    else
    /* For x64 DR we assume we're controlling the wow64 code too and thus
     * a wow64 "syscall" is just an indirect call (xref i#821, i#49)
     */
    if (IF_X64_ELSE(true, !is_wow64_process(NT_CURRENT_PROCESS)))
        return false;
    CLIENT_ASSERT(get_syscall_method() == SYSCALL_METHOD_WOW64,
                  "wow64 system call inconsistency");
    if (get_os_version() < WINDOWS_VERSION_10) {
        opnd_t tgt;
        if (instr_get_opcode(instr) != OP_call_ind)
            return false;
        tgt = instr_get_target(instr);
        return (opnd_is_far_base_disp(tgt) && opnd_get_segment(tgt) == SEG_FS &&
                opnd_get_base(tgt) == REG_NULL && opnd_get_index(tgt) == REG_NULL &&
                opnd_get_disp(tgt) == WOW64_TIB_OFFSET);
    } else {
        /* It's much simpler to have a syscall gateway instruction where
         * does_syscall_ret_to_callsite() is true: so we require that the
         * instr passed here has its translation set.  This also gets the
         * syscall # into the same bb to help static analysis.
         */
        opnd_t tgt;
        app_pc xl8;
        uint imm;
        byte opbyte;
        /* We can't just compare to wow64_syscall_call_tgt b/c there are copies
         * in {ntdll,kernelbase,kernel32,user32,gdi32}!Wow64SystemServiceCall.
         * They are all identical and we could perform a hardcoded pattern match,
         * but that is fragile across updates (it broke in 1511 and again in 1607).
         * Instead we just look for "mov edx,imm; call edx; ret" and we assume
         * that will never happen in regular code.
         * XXX: should we instead consider treating the far jmp as the syscall, and
         * putting in hooks on the return paths in wow64cpu!RunSimulatedCode()
         * (might be tricky b/c we'd have to decode 64-bit code), or changing
         * the return addr?
         */
#        ifdef DEBUG
        /* We still pattern match in debug to provide a sanity check */
        static const byte WOW64_SYSSVC[] = {
            0x64, 0x8b, 0x15, 0x30, 0x00, 0x00, 0x00, /* mov edx,dword ptr fs:[30h] */
            /* The offset here varies across updates so we do do not check it */
            0x8b, 0x92, /* mov edx,dword ptr [edx+254h] */
        };
        static const byte WOW64_SYSSVC_1609[] = {
            0xff, 0x25, /* + offs for "jmp dword ptr [ntdll!Wow64Transition]" */
        };
        byte tgt_code[sizeof(WOW64_SYSSVC)];
#        endif
        if (instr_get_opcode(instr) != OP_call_ind)
            return false;
        tgt = instr_get_target(instr);
        if (!opnd_is_reg(tgt) || opnd_get_reg(tgt) != DR_REG_EDX)
            return false;
        xl8 = get_app_instr_xl8(instr);
        if (xl8 == NULL)
            return false;
        if (/* Is the "call edx" followed by a "ret"? */
            d_r_safe_read(xl8 + CTI_IND1_LENGTH, sizeof(opbyte), &opbyte) &&
            (opbyte == RET_NOIMM_OPCODE || opbyte == RET_IMM_OPCODE) &&
            /* Is the "call edx" preceded by a "mov imm into edx"? */
            d_r_safe_read(xl8 - sizeof(imm) - 1, sizeof(opbyte), &opbyte) &&
            opbyte == MOV_IMM_EDX_OPCODE) {
            /* Slightly worried: let's at least have some kind of marker a user
             * could see to make it easier to diagnose problems.
             * It's a tradeoff: less likely to break in a future update, but
             * more likely to mess up an app with unusual code.
             * We could also check whether in a system dll but we'd need to
             * cache the bounds of multiple libs.
             */
            ASSERT_CURIOSITY(
                d_r_safe_read(xl8 - sizeof(imm), sizeof(imm), &imm) &&
                    (d_r_safe_read((app_pc)(ptr_uint_t)imm, sizeof(tgt_code), tgt_code) &&
                     memcmp(tgt_code, WOW64_SYSSVC, sizeof(tgt_code)) == 0) ||
                (d_r_safe_read((app_pc)(ptr_uint_t)imm, sizeof(WOW64_SYSSVC_1609),
                               tgt_code) &&
                 memcmp(tgt_code, WOW64_SYSSVC_1609, sizeof(WOW64_SYSSVC_1609)) == 0));
            return true;
        } else
            return false;
    }
#    endif /* STANDALONE_DECODER */
}
#endif

/* looks for mov_imm and mov_st and xor w/ src==dst,
 * returns the constant they set their dst to
 */
bool
instr_is_mov_constant(instr_t *instr, ptr_int_t *value)
{
    int opc = instr_get_opcode(instr);
    if (opc == OP_xor) {
        if (opnd_same(instr_get_src(instr, 0), instr_get_dst(instr, 0))) {
            *value = 0;
            return true;
        } else
            return false;
    } else if (opc == OP_mov_imm || opc == OP_mov_st) {
        opnd_t op = instr_get_src(instr, 0);
        if (opnd_is_immed_int(op)) {
            *value = opnd_get_immed_int(op);
            return true;
        } else
            return false;
    }
    return false;
}

bool
instr_is_prefetch(instr_t *instr)
{
    int opcode = instr_get_opcode(instr);

    if (opcode == OP_prefetchnta || opcode == OP_prefetcht0 || opcode == OP_prefetcht1 ||
        opcode == OP_prefetcht2 || opcode == OP_prefetch || opcode == OP_prefetchw)
        return true;

    return false;
}

bool
instr_is_string_op(instr_t *instr)
{
    uint opc = instr_get_opcode(instr);
    return (opc == OP_ins || opc == OP_outs || opc == OP_movs || opc == OP_stos ||
            opc == OP_lods || opc == OP_cmps || opc == OP_scas);
}

bool
instr_is_rep_string_op(instr_t *instr)
{
    uint opc = instr_get_opcode(instr);
    return (opc == OP_rep_ins || opc == OP_rep_outs || opc == OP_rep_movs ||
            opc == OP_rep_stos || opc == OP_rep_lods || opc == OP_rep_cmps ||
            opc == OP_repne_cmps || opc == OP_rep_scas || opc == OP_repne_scas);
}

bool
instr_is_floating_ex(instr_t *instr, dr_fp_type_t *type OUT)
{
    int opc = instr_get_opcode(instr);

    switch (opc) {
    case OP_fnclex:
    case OP_fninit:
    case OP_fxsave32:
    case OP_fxrstor32:
    case OP_fxsave64:
    case OP_fxrstor64:
    case OP_ldmxcsr:
    case OP_stmxcsr:
    case OP_fldenv:
    case OP_fldcw:
    case OP_fnstenv:
    case OP_fnstcw:
    case OP_frstor:
    case OP_fnsave:
    case OP_fnstsw:
    case OP_xsave32:
    case OP_xrstor32:
    case OP_xsaveopt32:
    case OP_xsavec32:
    case OP_xsave64:
    case OP_xrstor64:
    case OP_xsaveopt64:
    case OP_xsavec64:
    case OP_vldmxcsr:
    case OP_vstmxcsr:
    case OP_fwait: {
        if (type != NULL)
            *type = DR_FP_STATE;
        return true;
    }

    case OP_fld:
    case OP_fst:
    case OP_fstp:
    case OP_fild:
    case OP_movntps:
    case OP_movntpd:
    case OP_movups:
    case OP_movss:
    case OP_movupd:
    case OP_movsd:
    case OP_movlps:
    case OP_movlpd:
    case OP_movhps:
    case OP_movhpd:
    case OP_movaps:
    case OP_movapd:
    case OP_movsldup:
    case OP_movshdup:
    case OP_movddup:
    case OP_vmovss:
    case OP_vmovsd:
    case OP_vmovups:
    case OP_vmovupd:
    case OP_vmovlps:
    case OP_vmovsldup:
    case OP_vmovlpd:
    case OP_vmovddup:
    case OP_vmovhps:
    case OP_vmovshdup:
    case OP_vmovhpd:
    case OP_vmovaps:
    case OP_vmovapd:
    case OP_vmovntps:
    case OP_vmovntpd:
    case OP_unpcklps:
    case OP_unpcklpd:
    case OP_unpckhps:
    case OP_unpckhpd:
    case OP_vunpcklps:
    case OP_vunpcklpd:
    case OP_vunpckhps:
    case OP_vunpckhpd:
    case OP_extractps:
    case OP_insertps:
    case OP_vextractps:
    case OP_vinsertps:
    case OP_vinsertf128:
    case OP_vextractf128:
    case OP_vbroadcastss:
    case OP_vbroadcastsd:
    case OP_vbroadcastf128:
    case OP_vperm2f128:
    case OP_vpermilpd:
    case OP_vpermilps:
    case OP_vmaskmovps:
    case OP_vmaskmovpd:
    case OP_shufps:
    case OP_shufpd:
    case OP_vshufps:
    case OP_vshufpd: {
        if (type != NULL)
            *type = DR_FP_MOVE;
        return true;
    }

    case OP_fist:
    case OP_fistp:
    case OP_fbld:
    case OP_fbstp:
    case OP_fisttp:
    case OP_cvtpi2ps:
    case OP_cvtsi2ss:
    case OP_cvtpi2pd:
    case OP_cvtsi2sd:
    case OP_cvttps2pi:
    case OP_cvttss2si:
    case OP_cvttpd2pi:
    case OP_cvttsd2si:
    case OP_cvtps2pi:
    case OP_cvtss2si:
    case OP_cvtpd2pi:
    case OP_cvtsd2si:
    case OP_cvtps2pd:
    case OP_cvtss2sd:
    case OP_cvtpd2ps:
    case OP_cvtsd2ss:
    case OP_cvtdq2ps:
    case OP_cvttps2dq:
    case OP_cvtps2dq:
    case OP_cvtdq2pd:
    case OP_cvttpd2dq:
    case OP_cvtpd2dq:
    case OP_vcvtsi2ss:
    case OP_vcvtsi2sd:
    case OP_vcvttss2si:
    case OP_vcvttsd2si:
    case OP_vcvtss2si:
    case OP_vcvtsd2si:
    case OP_vcvtps2pd:
    case OP_vcvtss2sd:
    case OP_vcvtpd2ps:
    case OP_vcvtsd2ss:
    case OP_vcvtdq2ps:
    case OP_vcvttps2dq:
    case OP_vcvtps2dq:
    case OP_vcvtdq2pd:
    case OP_vcvttpd2dq:
    case OP_vcvtpd2dq:
    case OP_vcvtph2ps:
    case OP_vcvtps2ph: {
        if (type != NULL)
            *type = DR_FP_CONVERT;
        return true;
    }

    case OP_ucomiss:
    case OP_ucomisd:
    case OP_comiss:
    case OP_comisd:
    case OP_movmskps:
    case OP_movmskpd:
    case OP_sqrtps:
    case OP_sqrtss:
    case OP_sqrtpd:
    case OP_sqrtsd:
    case OP_rsqrtps:
    case OP_rsqrtss:
    case OP_rcpps:
    case OP_rcpss:
    case OP_andps:
    case OP_andpd:
    case OP_andnps:
    case OP_andnpd:
    case OP_orps:
    case OP_orpd:
    case OP_xorps:
    case OP_xorpd:
    case OP_addps:
    case OP_addss:
    case OP_addpd:
    case OP_addsd:
    case OP_mulps:
    case OP_mulss:
    case OP_mulpd:
    case OP_mulsd:
    case OP_subps:
    case OP_subss:
    case OP_subpd:
    case OP_subsd:
    case OP_minps:
    case OP_minss:
    case OP_minpd:
    case OP_minsd:
    case OP_divps:
    case OP_divss:
    case OP_divpd:
    case OP_divsd:
    case OP_maxps:
    case OP_maxss:
    case OP_maxpd:
    case OP_maxsd:
    case OP_cmpps:
    case OP_cmpss:
    case OP_cmppd:
    case OP_cmpsd:

    case OP_fadd:
    case OP_fmul:
    case OP_fcom:
    case OP_fcomp:
    case OP_fsub:
    case OP_fsubr:
    case OP_fdiv:
    case OP_fdivr:
    case OP_fiadd:
    case OP_fimul:
    case OP_ficom:
    case OP_ficomp:
    case OP_fisub:
    case OP_fisubr:
    case OP_fidiv:
    case OP_fidivr:
    case OP_fxch:
    case OP_fnop:
    case OP_fchs:
    case OP_fabs:
    case OP_ftst:
    case OP_fxam:
    case OP_fld1:
    case OP_fldl2t:
    case OP_fldl2e:
    case OP_fldpi:
    case OP_fldlg2:
    case OP_fldln2:
    case OP_fldz:
    case OP_f2xm1:
    case OP_fyl2x:
    case OP_fptan:
    case OP_fpatan:
    case OP_fxtract:
    case OP_fprem1:
    case OP_fdecstp:
    case OP_fincstp:
    case OP_fprem:
    case OP_fyl2xp1:
    case OP_fsqrt:
    case OP_fsincos:
    case OP_frndint:
    case OP_fscale:
    case OP_fsin:
    case OP_fcos:
    case OP_fcmovb:
    case OP_fcmove:
    case OP_fcmovbe:
    case OP_fcmovu:
    case OP_fucompp:
    case OP_fcmovnb:
    case OP_fcmovne:
    case OP_fcmovnbe:
    case OP_fcmovnu:
    case OP_fucomi:
    case OP_fcomi:
    case OP_ffree:
    case OP_fucom:
    case OP_fucomp:
    case OP_faddp:
    case OP_fmulp:
    case OP_fcompp:
    case OP_fsubrp:
    case OP_fsubp:
    case OP_fdivrp:
    case OP_fdivp:
    case OP_fucomip:
    case OP_fcomip:
    case OP_ffreep:

    /* SSE3/3D-Now!/SSE4 */
    case OP_haddpd:
    case OP_haddps:
    case OP_hsubpd:
    case OP_hsubps:
    case OP_addsubpd:
    case OP_addsubps:
    case OP_femms:
    case OP_movntss:
    case OP_movntsd:
    case OP_blendvps:
    case OP_blendvpd:
    case OP_roundps:
    case OP_roundpd:
    case OP_roundss:
    case OP_roundsd:
    case OP_blendps:
    case OP_blendpd:
    case OP_dpps:
    case OP_dppd:

    /* AVX */
    case OP_vucomiss:
    case OP_vucomisd:
    case OP_vcomiss:
    case OP_vcomisd:
    case OP_vmovmskps:
    case OP_vmovmskpd:
    case OP_vsqrtps:
    case OP_vsqrtss:
    case OP_vsqrtpd:
    case OP_vsqrtsd:
    case OP_vrsqrtps:
    case OP_vrsqrtss:
    case OP_vrcpps:
    case OP_vrcpss:
    case OP_vandps:
    case OP_vandpd:
    case OP_vandnps:
    case OP_vandnpd:
    case OP_vorps:
    case OP_vorpd:
    case OP_vxorps:
    case OP_vxorpd:
    case OP_vaddps:
    case OP_vaddss:
    case OP_vaddpd:
    case OP_vaddsd:
    case OP_vmulps:
    case OP_vmulss:
    case OP_vmulpd:
    case OP_vmulsd:
    case OP_vsubps:
    case OP_vsubss:
    case OP_vsubpd:
    case OP_vsubsd:
    case OP_vminps:
    case OP_vminss:
    case OP_vminpd:
    case OP_vminsd:
    case OP_vdivps:
    case OP_vdivss:
    case OP_vdivpd:
    case OP_vdivsd:
    case OP_vmaxps:
    case OP_vmaxss:
    case OP_vmaxpd:
    case OP_vmaxsd:
    case OP_vcmpps:
    case OP_vcmpss:
    case OP_vcmppd:
    case OP_vcmpsd:
    case OP_vhaddpd:
    case OP_vhaddps:
    case OP_vhsubpd:
    case OP_vhsubps:
    case OP_vaddsubpd:
    case OP_vaddsubps:
    case OP_vblendvps:
    case OP_vblendvpd:
    case OP_vroundps:
    case OP_vroundpd:
    case OP_vroundss:
    case OP_vroundsd:
    case OP_vblendps:
    case OP_vblendpd:
    case OP_vdpps:
    case OP_vdppd:
    case OP_vtestps:
    case OP_vtestpd:

    /* FMA */
    case OP_vfmadd132ps:
    case OP_vfmadd132pd:
    case OP_vfmadd213ps:
    case OP_vfmadd213pd:
    case OP_vfmadd231ps:
    case OP_vfmadd231pd:
    case OP_vfmadd132ss:
    case OP_vfmadd132sd:
    case OP_vfmadd213ss:
    case OP_vfmadd213sd:
    case OP_vfmadd231ss:
    case OP_vfmadd231sd:
    case OP_vfmaddsub132ps:
    case OP_vfmaddsub132pd:
    case OP_vfmaddsub213ps:
    case OP_vfmaddsub213pd:
    case OP_vfmaddsub231ps:
    case OP_vfmaddsub231pd:
    case OP_vfmsubadd132ps:
    case OP_vfmsubadd132pd:
    case OP_vfmsubadd213ps:
    case OP_vfmsubadd213pd:
    case OP_vfmsubadd231ps:
    case OP_vfmsubadd231pd:
    case OP_vfmsub132ps:
    case OP_vfmsub132pd:
    case OP_vfmsub213ps:
    case OP_vfmsub213pd:
    case OP_vfmsub231ps:
    case OP_vfmsub231pd:
    case OP_vfmsub132ss:
    case OP_vfmsub132sd:
    case OP_vfmsub213ss:
    case OP_vfmsub213sd:
    case OP_vfmsub231ss:
    case OP_vfmsub231sd:
    case OP_vfnmadd132ps:
    case OP_vfnmadd132pd:
    case OP_vfnmadd213ps:
    case OP_vfnmadd213pd:
    case OP_vfnmadd231ps:
    case OP_vfnmadd231pd:
    case OP_vfnmadd132ss:
    case OP_vfnmadd132sd:
    case OP_vfnmadd213ss:
    case OP_vfnmadd213sd:
    case OP_vfnmadd231ss:
    case OP_vfnmadd231sd:
    case OP_vfnmsub132ps:
    case OP_vfnmsub132pd:
    case OP_vfnmsub213ps:
    case OP_vfnmsub213pd:
    case OP_vfnmsub231ps:
    case OP_vfnmsub231pd:
    case OP_vfnmsub132ss:
    case OP_vfnmsub132sd:
    case OP_vfnmsub213ss:
    case OP_vfnmsub213sd:
    case OP_vfnmsub231ss:
    case OP_vfnmsub231sd: {
        if (type != NULL)
            *type = DR_FP_MATH;
        return true;
    }

    default: return false;
    }
}

bool
instr_can_set_single_step(instr_t *instr)
{
    return (instr_get_opcode(instr) == OP_popf || instr_get_opcode(instr) == OP_iret);
}

bool
instr_may_write_zmm_or_opmask_register(instr_t *instr)
{
    if (instr_get_prefix_flag(instr, PREFIX_EVEX))
        return true;

    for (int i = 0; i < instr_num_dsts(instr); ++i) {
        opnd_t dst = instr_get_dst(instr, i);
        if (opnd_is_reg(dst)) {
            if (reg_is_strictly_zmm(opnd_get_reg(dst)) ||
                reg_is_opmask(opnd_get_reg(dst)))
                return true;
        }
    }
    return false;
}

bool
instr_is_floating(instr_t *instr)
{
    return instr_is_floating_ex(instr, NULL);
}

bool
instr_saves_float_pc(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    return (op == OP_fnsave || op == OP_fnstenv || op == OP_fxsave32 ||
            op == OP_xsave32 || op == OP_xsaveopt32 || op == OP_xsavec32 ||
            op == OP_xsavec64 || op == OP_fxsave64 || op == OP_xsave64 ||
            op == OP_xsaveopt64);
}

static bool
opcode_is_mmx(int op)
{
    switch (op) {
    case OP_emms:
    case OP_movd:
    case OP_movq:
    case OP_packssdw:
    case OP_packsswb:
    case OP_packuswb:
    case OP_paddb:
    case OP_paddw:
    case OP_paddd:
    case OP_paddsb:
    case OP_paddsw:
    case OP_paddusb:
    case OP_paddusw:
    case OP_pand:
    case OP_pandn:
    case OP_por:
    case OP_pxor:
    case OP_pcmpeqb:
    case OP_pcmpeqw:
    case OP_pcmpeqd:
    case OP_pcmpgtb:
    case OP_pcmpgtw:
    case OP_pcmpgtd:
    case OP_pmaddwd:
    case OP_pmulhw:
    case OP_pmullw:
    case OP_psllw:
    case OP_pslld:
    case OP_psllq:
    case OP_psrad:
    case OP_psraw:
    case OP_psrlw:
    case OP_psrld:
    case OP_psrlq:
    case OP_psubb:
    case OP_psubw:
    case OP_psubd:
    case OP_psubsb:
    case OP_psubsw:
    case OP_psubusb:
    case OP_psubusw:
    case OP_punpckhbw:
    case OP_punpckhwd:
    case OP_punpckhdq:
    case OP_punpcklbw:
    case OP_punpckldq:
    case OP_punpcklwd: return true;
    default: return false;
    }
}

static bool
opcode_is_opmask(int op)
{
    switch (op) {
    case OP_kmovw:
    case OP_kmovb:
    case OP_kmovq:
    case OP_kmovd:
    case OP_kandw:
    case OP_kandb:
    case OP_kandq:
    case OP_kandd:
    case OP_kandnw:
    case OP_kandnb:
    case OP_kandnq:
    case OP_kandnd:
    case OP_kunpckbw:
    case OP_kunpckwd:
    case OP_kunpckdq:
    case OP_knotw:
    case OP_knotb:
    case OP_knotq:
    case OP_knotd:
    case OP_korw:
    case OP_korb:
    case OP_korq:
    case OP_kord:
    case OP_kxnorw:
    case OP_kxnorb:
    case OP_kxnorq:
    case OP_kxnord:
    case OP_kxorw:
    case OP_kxorb:
    case OP_kxorq:
    case OP_kxord:
    case OP_kaddw:
    case OP_kaddb:
    case OP_kaddq:
    case OP_kaddd:
    case OP_kortestw:
    case OP_kortestb:
    case OP_kortestq:
    case OP_kortestd:
    case OP_kshiftlw:
    case OP_kshiftlb:
    case OP_kshiftlq:
    case OP_kshiftld:
    case OP_kshiftrw:
    case OP_kshiftrb:
    case OP_kshiftrq:
    case OP_kshiftrd:
    case OP_ktestw:
    case OP_ktestb:
    case OP_ktestq:
    case OP_ktestd: return true;
    default: return false;
    }
}

static bool
opcode_is_sse(int op)
{
    switch (op) {
    case OP_addps:
    case OP_addss:
    case OP_andnps:
    case OP_andps:
    case OP_cmpps:
    case OP_cmpss:
    case OP_comiss:
    case OP_cvtpi2ps:
    case OP_cvtps2pi:
    case OP_cvtsi2ss:
    case OP_cvtss2si:
    case OP_cvttps2pi:
    case OP_cvttss2si:
    case OP_divps:
    case OP_divss:
    case OP_ldmxcsr:
    case OP_maskmovq:
    case OP_maxps:
    case OP_maxss:
    case OP_minps:
    case OP_minss:
    case OP_movaps:
    case OP_movhps: /* == OP_movlhps */
    case OP_movlps: /* == OP_movhlps */
    case OP_movmskps:
    case OP_movntps:
    case OP_movntq:
    case OP_movss:
    case OP_movups:
    case OP_mulps:
    case OP_mulss:
    case OP_nop_modrm:
    case OP_orps:
    case OP_pavgb:
    case OP_pavgw:
    case OP_pextrw:
    case OP_pinsrw:
    case OP_pmaxsw:
    case OP_pmaxub:
    case OP_pminsw:
    case OP_pminub:
    case OP_pmovmskb:
    case OP_pmulhuw:
    case OP_prefetchnta:
    case OP_prefetcht0:
    case OP_prefetcht1:
    case OP_prefetcht2:
    case OP_psadbw:
    case OP_pshufw:
    case OP_rcpps:
    case OP_rcpss:
    case OP_rsqrtps:
    case OP_rsqrtss:
    case OP_sfence:
    case OP_shufps:
    case OP_sqrtps:
    case OP_sqrtss:
    case OP_stmxcsr:
    case OP_subps:
    case OP_subss:
    case OP_ucomiss:
    case OP_unpckhps:
    case OP_unpcklps:
    case OP_xorps: return true;
    default: return false;
    }
}

static bool
opcode_is_new_in_sse2(int op)
{
    switch (op) {
    case OP_addpd:
    case OP_addsd:
    case OP_andnpd:
    case OP_andpd:
    case OP_clflush: /* has own cpuid bit */
    case OP_cmppd:
    case OP_cmpsd:
    case OP_comisd:
    case OP_cvtdq2pd:
    case OP_cvtdq2ps:
    case OP_cvtpd2dq:
    case OP_cvtpd2pi:
    case OP_cvtpd2ps:
    case OP_cvtpi2pd:
    case OP_cvtps2dq:
    case OP_cvtps2pd:
    case OP_cvtsd2si:
    case OP_cvtsd2ss:
    case OP_cvtsi2sd:
    case OP_cvtss2sd:
    case OP_cvttpd2dq:
    case OP_cvttpd2pi:
    case OP_cvttps2dq:
    case OP_cvttsd2si:
    case OP_divpd:
    case OP_divsd:
    case OP_maskmovdqu:
    case OP_maxpd:
    case OP_maxsd:
    case OP_minpd:
    case OP_minsd:
    case OP_movapd:
    case OP_movdq2q:
    case OP_movdqa:
    case OP_movdqu:
    case OP_movhpd:
    case OP_movlpd:
    case OP_movmskpd:
    case OP_movntdq:
    case OP_movntpd:
    case OP_movnti:
    case OP_movq2dq:
    case OP_movsd:
    case OP_movupd:
    case OP_mulpd:
    case OP_mulsd:
    case OP_orpd:
    case OP_paddq:
    case OP_pmuludq:
    case OP_pshufd:
    case OP_pshufhw:
    case OP_pshuflw:
    case OP_pslldq:
    case OP_psrldq:
    case OP_psubq:
    case OP_punpckhqdq:
    case OP_punpcklqdq:
    case OP_shufpd:
    case OP_sqrtpd:
    case OP_sqrtsd:
    case OP_subpd:
    case OP_subsd:
    case OP_ucomisd:
    case OP_unpckhpd:
    case OP_unpcklpd:
    case OP_xorpd: return true;
    default: return false;
    }
}

static bool
opcode_is_widened_in_sse2(int op)
{
    switch (op) {
    case OP_pavgb:
    case OP_pavgw:
    case OP_pextrw:
    case OP_pinsrw:
    case OP_pmaxsw:
    case OP_pmaxub:
    case OP_pminsw:
    case OP_pminub:
    case OP_pmovmskb:
    case OP_pmulhuw:
    case OP_psadbw: return true;
    default: return opcode_is_mmx(op) && op != OP_emms;
    }
}

static bool
instr_has_xmm_opnd(instr_t *instr)
{
    int i;
    opnd_t opnd;
    CLIENT_ASSERT(instr_operands_valid(instr), "instr_shrink_to_16_bits: invalid opnds");
    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd = instr_get_dst(instr, i);
        if (opnd_is_reg(opnd) && reg_is_xmm(opnd_get_reg(opnd)))
            return true;
    }
    for (i = 0; i < instr_num_srcs(instr); i++) {
        opnd = instr_get_src(instr, i);
        if (opnd_is_reg(opnd) && reg_is_xmm(opnd_get_reg(opnd)))
            return true;
    }
    return false;
}

bool
instr_is_mmx(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    if (opcode_is_mmx(op)) {
        /* SSE2 extends SSE and MMX integer opcodes */
        if (opcode_is_widened_in_sse2(op))
            return !instr_has_xmm_opnd(instr);
        return true;
    }
    return false;
}

bool
instr_is_opmask(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    return opcode_is_opmask(op);
}

bool
instr_is_sse(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    if (opcode_is_sse(op)) {
        /* SSE2 extends SSE and MMX integer opcodes */
        if (opcode_is_widened_in_sse2(op))
            return !instr_has_xmm_opnd(instr);
        return true;
    }
    return false;
}

bool
instr_is_sse2(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    if (opcode_is_new_in_sse2(op))
        return true;
    /* SSE2 extends SSE and MMX integer opcodes */
    if (opcode_is_widened_in_sse2(op))
        return instr_has_xmm_opnd(instr);
    return false;
}

bool
instr_is_sse_or_sse2(instr_t *instr)
{
    return instr_is_sse(instr) || instr_is_sse2(instr);
}

bool
instr_is_sse3(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    /* We rely on the enum order here.  We include OP_monitor and OP_mwait. */
    return (op >= OP_fisttp && op <= OP_movddup);
}

bool
instr_is_3DNow(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    /* We rely on the enum order here. */
    return (op >= OP_femms && op <= OP_pswapd) || op == OP_prefetch || op == OP_prefetchw;
}

bool
instr_is_ssse3(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    /* We rely on the enum order here. */
    return (op >= OP_pshufb && op <= OP_palignr);
}

bool
instr_is_sse41(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    /* We rely on the enum order here. */
    return (op >= OP_pblendvb && op <= OP_mpsadbw && op != OP_pcmpgtq && op != OP_crc32);
}

bool
instr_is_sse42(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    /* We rely on the enum order here. */
    return (op >= OP_pcmpestrm && op <= OP_pcmpistri) || op == OP_pcmpgtq ||
        op == OP_crc32 || op == OP_popcnt;
}

bool
instr_is_sse4A(instr_t *instr)
{
    int op = instr_get_opcode(instr);
    /* We rely on the enum order here. */
    return (op >= OP_popcnt && op <= OP_lzcnt);
}

bool
instr_is_mov_imm_to_tos(instr_t *instr)
{
    return instr_opcode_valid(instr) && instr_get_opcode(instr) == OP_mov_st &&
        (opnd_is_immed(instr_get_src(instr, 0)) ||
         opnd_is_near_instr(instr_get_src(instr, 0))) &&
        opnd_is_near_base_disp(instr_get_dst(instr, 0)) &&
        opnd_get_base(instr_get_dst(instr, 0)) == REG_ESP &&
        opnd_get_index(instr_get_dst(instr, 0)) == REG_NULL &&
        opnd_get_disp(instr_get_dst(instr, 0)) == 0;
}

/* Returns true iff instr is an "undefined" instruction */
bool
instr_is_undefined(instr_t *instr)
{
    return (instr_opcode_valid(instr) &&
            (instr_get_opcode(instr) == OP_ud2 || instr_get_opcode(instr) == OP_ud1));
}

DR_API
/* Given a cbr, change the opcode (and potentially branch hint
 * prefixes) to that of the inverted branch condition.
 */
void
instr_invert_cbr(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    CLIENT_ASSERT(instr_is_cbr(instr), "instr_invert_cbr: instr not a cbr");
    if (instr_is_cti_short_rewrite(instr, NULL)) {
        /* these all look like this:
                     jcxz cx_zero
                     jmp-short cx_nonzero
            cx_zero: jmp foo
            cx_nonzero:
         */
        uint disp1_pos = 1, disp2_pos = 3;
        if (instr_get_raw_byte(instr, 0) == ADDR_PREFIX_OPCODE) {
            disp1_pos++;
            disp2_pos++;
        }
        if (instr_get_raw_byte(instr, disp1_pos) == 2) {
            CLIENT_ASSERT(instr_get_raw_byte(instr, disp2_pos) == 5,
                          "instr_invert_cbr: cti_short_rewrite is corrupted");
            /* swap targets of the short jumps: */
            instr_set_raw_byte(instr, disp1_pos, (byte)7); /* target cx_nonzero */
            instr_set_raw_byte(instr, disp2_pos, (byte)0); /* target next inst, cx_zero */
            /* with inverted logic we don't need jmp-short but we keep it in
             * case we get inverted again */
        } else {
            /* re-invert */
            CLIENT_ASSERT(instr_get_raw_byte(instr, disp1_pos) == 7 &&
                              instr_get_raw_byte(instr, disp2_pos) == 0,
                          "instr_invert_cbr: cti_short_rewrite is corrupted");
            instr_set_raw_byte(instr, disp1_pos, (byte)2);
            instr_set_raw_byte(instr, disp2_pos, (byte)5);
        }
    } else if ((opc >= OP_jo && opc <= OP_jnle) ||
               (opc >= OP_jo_short && opc <= OP_jnle_short)) {
        switch (opc) {
        case OP_jb: opc = OP_jnb; break;
        case OP_jnb: opc = OP_jb; break;
        case OP_jbe: opc = OP_jnbe; break;
        case OP_jnbe: opc = OP_jbe; break;
        case OP_jl: opc = OP_jnl; break;
        case OP_jnl: opc = OP_jl; break;
        case OP_jle: opc = OP_jnle; break;
        case OP_jnle: opc = OP_jle; break;
        case OP_jo: opc = OP_jno; break;
        case OP_jno: opc = OP_jo; break;
        case OP_jp: opc = OP_jnp; break;
        case OP_jnp: opc = OP_jp; break;
        case OP_js: opc = OP_jns; break;
        case OP_jns: opc = OP_js; break;
        case OP_jz: opc = OP_jnz; break;
        case OP_jnz: opc = OP_jz; break;
        case OP_jb_short: opc = OP_jnb_short; break;
        case OP_jnb_short: opc = OP_jb_short; break;
        case OP_jbe_short: opc = OP_jnbe_short; break;
        case OP_jnbe_short: opc = OP_jbe_short; break;
        case OP_jl_short: opc = OP_jnl_short; break;
        case OP_jnl_short: opc = OP_jl_short; break;
        case OP_jle_short: opc = OP_jnle_short; break;
        case OP_jnle_short: opc = OP_jle_short; break;
        case OP_jo_short: opc = OP_jno_short; break;
        case OP_jno_short: opc = OP_jo_short; break;
        case OP_jp_short: opc = OP_jnp_short; break;
        case OP_jnp_short: opc = OP_jp_short; break;
        case OP_js_short: opc = OP_jns_short; break;
        case OP_jns_short: opc = OP_js_short; break;
        case OP_jz_short: opc = OP_jnz_short; break;
        case OP_jnz_short: opc = OP_jz_short; break;
        default: CLIENT_ASSERT(false, "instr_invert_cbr: unknown opcode"); break;
        }
        instr_set_opcode(instr, opc);
        /* reverse any branch hint */
        if (TEST(PREFIX_JCC_TAKEN, instr_get_prefixes(instr))) {
            instr->prefixes &= ~PREFIX_JCC_TAKEN;
            instr->prefixes |= PREFIX_JCC_NOT_TAKEN;
        } else if (TEST(PREFIX_JCC_NOT_TAKEN, instr_get_prefixes(instr))) {
            instr->prefixes &= ~PREFIX_JCC_NOT_TAKEN;
            instr->prefixes |= PREFIX_JCC_TAKEN;
        }
    } else
        CLIENT_ASSERT(false, "instr_invert_cbr: unknown opcode");
}

/* Given a machine state, returns whether or not the cbr instr would be taken
 * if the state is before execution (pre == true) or after (pre == false).
 */
bool
instr_cbr_taken(instr_t *instr, priv_mcontext_t *mcontext, bool pre)
{
    CLIENT_ASSERT(instr_is_cbr(instr), "instr_cbr_taken: instr not a cbr");
    if (instr_is_cti_loop(instr)) {
        uint opc = instr_get_opcode(instr);
        switch (opc) {
        case OP_loop: return (mcontext->xcx != (pre ? 1U : 0U));
        case OP_loope:
            return (TEST(EFLAGS_ZF, mcontext->xflags) &&
                    mcontext->xcx != (pre ? 1U : 0U));
        case OP_loopne:
            return (!TEST(EFLAGS_ZF, mcontext->xflags) &&
                    mcontext->xcx != (pre ? 1U : 0U));
        case OP_jecxz: return (mcontext->xcx == 0U);
        default: CLIENT_ASSERT(false, "instr_cbr_taken: unknown opcode"); return false;
        }
    }
    return instr_jcc_taken(instr, mcontext->xflags);
}

/* Given eflags, returns whether or not the conditional branch opc would be taken */
static bool
opc_jcc_taken(int opc, reg_t eflags)
{
    switch (opc) {
    case OP_jo:
    case OP_jo_short: return TEST(EFLAGS_OF, eflags);
    case OP_jno:
    case OP_jno_short: return !TEST(EFLAGS_OF, eflags);
    case OP_jb:
    case OP_jb_short: return TEST(EFLAGS_CF, eflags);
    case OP_jnb:
    case OP_jnb_short: return !TEST(EFLAGS_CF, eflags);
    case OP_jz:
    case OP_jz_short: return TEST(EFLAGS_ZF, eflags);
    case OP_jnz:
    case OP_jnz_short: return !TEST(EFLAGS_ZF, eflags);
    case OP_jbe:
    case OP_jbe_short: return TESTANY(EFLAGS_CF | EFLAGS_ZF, eflags);
    case OP_jnbe:
    case OP_jnbe_short: return !TESTANY(EFLAGS_CF | EFLAGS_ZF, eflags);
    case OP_js:
    case OP_js_short: return TEST(EFLAGS_SF, eflags);
    case OP_jns:
    case OP_jns_short: return !TEST(EFLAGS_SF, eflags);
    case OP_jp:
    case OP_jp_short: return TEST(EFLAGS_PF, eflags);
    case OP_jnp:
    case OP_jnp_short: return !TEST(EFLAGS_PF, eflags);
    case OP_jl:
    case OP_jl_short: return (TEST(EFLAGS_SF, eflags) != TEST(EFLAGS_OF, eflags));
    case OP_jnl:
    case OP_jnl_short: return (TEST(EFLAGS_SF, eflags) == TEST(EFLAGS_OF, eflags));
    case OP_jle:
    case OP_jle_short:
        return (TEST(EFLAGS_ZF, eflags) ||
                TEST(EFLAGS_SF, eflags) != TEST(EFLAGS_OF, eflags));
    case OP_jnle:
    case OP_jnle_short:
        return (!TEST(EFLAGS_ZF, eflags) &&
                TEST(EFLAGS_SF, eflags) == TEST(EFLAGS_OF, eflags));
    default: CLIENT_ASSERT(false, "instr_jcc_taken: unknown opcode"); return false;
    }
}

/* Given eflags, returns whether or not the conditional branch instr would be taken */
bool
instr_jcc_taken(instr_t *instr, reg_t eflags)
{
    int opc = instr_get_opcode(instr);
    CLIENT_ASSERT(instr_is_cbr(instr) && !instr_is_cti_loop(instr),
                  "instr_jcc_taken: instr not a non-jecxz/loop-cbr");
    return opc_jcc_taken(opc, eflags);
}

DR_API
/* Converts a cmovcc opcode \p cmovcc_opcode to the OP_jcc opcode that
 * tests the same bits in eflags.
 */
int
instr_cmovcc_to_jcc(int cmovcc_opcode)
{
    int jcc_opc = OP_INVALID;
    if (cmovcc_opcode >= OP_cmovo && cmovcc_opcode <= OP_cmovnle) {
        jcc_opc = cmovcc_opcode - OP_cmovo + OP_jo;
    } else {
        switch (cmovcc_opcode) {
        case OP_fcmovb: jcc_opc = OP_jb; break;
        case OP_fcmove: jcc_opc = OP_jz; break;
        case OP_fcmovbe: jcc_opc = OP_jbe; break;
        case OP_fcmovu: jcc_opc = OP_jp; break;
        case OP_fcmovnb: jcc_opc = OP_jnb; break;
        case OP_fcmovne: jcc_opc = OP_jnz; break;
        case OP_fcmovnbe: jcc_opc = OP_jnbe; break;
        case OP_fcmovnu: jcc_opc = OP_jnp; break;
        default: CLIENT_ASSERT(false, "invalid cmovcc opcode"); return OP_INVALID;
        }
    }
    return jcc_opc;
}

DR_API
/* Given \p eflags, returns whether or not the conditional move
 * instruction \p instr would execute the move.  The conditional move
 * can be an OP_cmovcc or an OP_fcmovcc instruction.
 */
bool
instr_cmovcc_triggered(instr_t *instr, reg_t eflags)
{
    int opc = instr_get_opcode(instr);
    int jcc_opc = instr_cmovcc_to_jcc(opc);
    return opc_jcc_taken(jcc_opc, eflags);
}

DR_API
dr_pred_trigger_t
instr_predicate_triggered(instr_t *instr, dr_mcontext_t *mc)
{
    dr_pred_type_t pred = instr_get_predicate(instr);
    if (pred == DR_PRED_NONE)
        return DR_PRED_TRIGGER_NOPRED;
    else if (pred == DR_PRED_COMPLEX) {
#ifndef STANDALONE_DECODER /* no safe_read there */
        int opc = instr_get_opcode(instr);
        if (opc == OP_bsf || opc == OP_bsr) {
            /* The src can't involve a multimedia reg or VSIB */
            opnd_t src = instr_get_src(instr, 0);
            CLIENT_ASSERT(instr_num_srcs(instr) == 1, "invalid predicate/instr combo");
            if (opnd_is_immed_int(src)) {
                return (opnd_get_immed_int(src) != 0) ? DR_PRED_TRIGGER_MATCH
                                                      : DR_PRED_TRIGGER_MISMATCH;
            } else if (opnd_is_reg(src)) {
                return (reg_get_value(opnd_get_reg(src), mc) != 0)
                    ? DR_PRED_TRIGGER_MATCH
                    : DR_PRED_TRIGGER_MISMATCH;
            } else if (opnd_is_memory_reference(src)) {
                ptr_int_t val;
                if (!d_r_safe_read(opnd_compute_address(src, mc),
                                   MIN(opnd_get_size(src), sizeof(val)), &val))
                    return false;
                return (val != 0) ? DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
            } else
                CLIENT_ASSERT(false, "invalid predicate/instr combo");
        }
        /* XXX: add other opcodes: OP_getsec, OP_xend, OP_*maskmov* */
#endif
        return DR_PRED_TRIGGER_UNKNOWN;
    } else if (pred >= DR_PRED_O && pred <= DR_PRED_NLE) {
        /* We rely on DR_PRED_ having the same ordering as the OP_jcc opcodes */
        int jcc_opc = pred - DR_PRED_O + OP_jo;
        return opc_jcc_taken(jcc_opc, mc->xflags) ? DR_PRED_TRIGGER_MATCH
                                                  : DR_PRED_TRIGGER_MISMATCH;
    }
    return DR_PRED_TRIGGER_INVALID;
}

bool
instr_predicate_reads_srcs(dr_pred_type_t pred)
{
    /* All complex instances so far read srcs. */
    return pred == DR_PRED_COMPLEX;
}

bool
instr_predicate_writes_eflags(dr_pred_type_t pred)
{
    /* Only OP_bsf and OP_bsr are conditional and write eflags, and they do
     * the eflags write unconditionally.
     */
    return pred == DR_PRED_COMPLEX;
}

bool
instr_predicate_is_cond(dr_pred_type_t pred)
{
    return pred != DR_PRED_NONE;
}

bool
reg_is_gpr(reg_id_t reg)
{
    return (reg >= REG_RAX && reg <= REG_DIL);
}

bool
reg_is_segment(reg_id_t reg)
{
    return (reg >= SEG_ES && reg <= SEG_GS);
}

bool
reg_is_simd(reg_id_t reg)
{
    return reg_is_strictly_xmm(reg) || reg_is_strictly_ymm(reg) ||
        reg_is_strictly_zmm(reg) || reg_is_mmx(reg);
}

bool
reg_is_vector_simd(reg_id_t reg)
{
    return reg_is_strictly_xmm(reg) || reg_is_strictly_ymm(reg) ||
        reg_is_strictly_zmm(reg);
}

bool
reg_is_opmask(reg_id_t reg)
{
    return (reg >= DR_REG_START_OPMASK && reg <= DR_REG_STOP_OPMASK);
}

bool
reg_is_bnd(reg_id_t reg)
{
    return (reg >= DR_REG_START_BND && reg <= DR_REG_STOP_BND);
}

bool
reg_is_strictly_zmm(reg_id_t reg)
{
    return (reg >= DR_REG_START_ZMM && reg <= DR_REG_STOP_ZMM);
}

bool
reg_is_ymm(reg_id_t reg)
{
    return reg_is_strictly_ymm(reg);
}

bool
reg_is_strictly_ymm(reg_id_t reg)
{
    return (reg >= DR_REG_START_YMM && reg <= DR_REG_STOP_YMM);
}

bool
reg_is_xmm(reg_id_t reg)
{
    /* This function is deprecated and the only one out of the x86
     * reg_is_ set of functions that calls its wider sibling.
     */
    return (reg_is_strictly_xmm(reg) || reg_is_strictly_ymm(reg));
}

bool
reg_is_strictly_xmm(reg_id_t reg)
{
    return (reg >= DR_REG_START_XMM && reg <= DR_REG_STOP_XMM);
}

bool
reg_is_mmx(reg_id_t reg)
{
    return (reg >= DR_REG_START_MMX && reg <= DR_REG_STOP_MMX);
}

bool
reg_is_fp(reg_id_t reg)
{
    return (reg >= DR_REG_START_FLOAT && reg <= DR_REG_STOP_FLOAT);
}

bool
opnd_same_sizes_ok(opnd_size_t s1, opnd_size_t s2, bool is_reg)
{
    opnd_size_t s1_default, s2_default;
    decode_info_t di;
    if (s1 == s2)
        return true;
    /* This routine is used for variable sizes in INSTR_CREATE macros so we
     * check whether the default size matches.  If we need to do more
     * then we'll have to hook into encode's size resolution to resolve all
     * operands with each other's constraints at the instr level before coming here.
     */
    IF_X86_64(di.x86_mode = false);
    di.prefixes = 0;
    s1_default = resolve_variable_size(&di, s1, is_reg);
    s2_default = resolve_variable_size(&di, s2, is_reg);
    return (s1_default == s2_default);
}

instr_t *
instr_create_popa(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, OP_popa, 8, 2);
    instr_set_dst(in, 0, opnd_create_reg(REG_ESP));
    instr_set_dst(in, 1, opnd_create_reg(REG_EAX));
    instr_set_dst(in, 2, opnd_create_reg(REG_EBX));
    instr_set_dst(in, 3, opnd_create_reg(REG_ECX));
    instr_set_dst(in, 4, opnd_create_reg(REG_EDX));
    instr_set_dst(in, 5, opnd_create_reg(REG_EBP));
    instr_set_dst(in, 6, opnd_create_reg(REG_ESI));
    instr_set_dst(in, 7, opnd_create_reg(REG_EDI));
    instr_set_src(in, 0, opnd_create_reg(REG_ESP));
    instr_set_src(in, 1, opnd_create_base_disp(REG_ESP, REG_NULL, 0, 0, OPSZ_32_short16));
    return in;
}

instr_t *
instr_create_pusha(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, OP_pusha, 2, 8);
    instr_set_dst(in, 0, opnd_create_reg(REG_ESP));
    instr_set_dst(in, 1,
                  opnd_create_base_disp(REG_ESP, REG_NULL, 0, -32, OPSZ_32_short16));
    instr_set_src(in, 0, opnd_create_reg(REG_ESP));
    instr_set_src(in, 1, opnd_create_reg(REG_EAX));
    instr_set_src(in, 2, opnd_create_reg(REG_EBX));
    instr_set_src(in, 3, opnd_create_reg(REG_ECX));
    instr_set_src(in, 4, opnd_create_reg(REG_EDX));
    instr_set_src(in, 5, opnd_create_reg(REG_EBP));
    instr_set_src(in, 6, opnd_create_reg(REG_ESI));
    instr_set_src(in, 7, opnd_create_reg(REG_EDI));
    return in;
}

instr_t *
instr_create_nbyte_nop(dcontext_t *dcontext, uint num_bytes, bool raw)
{
    CLIENT_ASSERT(num_bytes != 0, "instr_create_nbyte_nop: 0 bytes passed");
    CLIENT_ASSERT(num_bytes <= 3, "instr_create_nbyte_nop: > 3 bytes not supported");
    /* INSTR_CREATE_nop*byte creates nop according to dcontext->x86_mode.
     * In x86_to_x64, we want to create x64 nop, but dcontext may be in x86 mode.
     * As a workaround, we call INSTR_CREATE_RAW_nop*byte here if in x86_to_x64.
     */
    if (raw IF_X64(|| DYNAMO_OPTION(x86_to_x64))) {
        switch (num_bytes) {
        case 1: return INSTR_CREATE_RAW_nop1byte(dcontext);
        case 2: return INSTR_CREATE_RAW_nop2byte(dcontext);
        case 3: return INSTR_CREATE_RAW_nop3byte(dcontext);
        }
    } else {
        switch (num_bytes) {
        case 1: return INSTR_CREATE_nop1byte(dcontext);
        case 2: return INSTR_CREATE_nop2byte(dcontext);
        case 3: return INSTR_CREATE_nop3byte(dcontext);
        }
    }
    CLIENT_ASSERT(false, "instr_create_nbyte_nop: invalid parameters");
    return NULL;
}

/* Borrowed from optimize.c, prob. belongs here anyways, could make it more
 * specific to the ones we create above, but know it works as is FIXME */
/* return true if this instr is a nop, does not check for all types of nops
 * since there are many, these seem to be the most common */
bool
instr_is_nop(instr_t *inst)
{
    /* XXX: could check raw bits for 0x90 to avoid the decoding if raw */
    int opcode = instr_get_opcode(inst);
    if (opcode == OP_nop || opcode == OP_nop_modrm)
        return true;
    if ((opcode == OP_mov_ld || opcode == OP_mov_st) &&
        opnd_same(instr_get_src(inst, 0), instr_get_dst(inst, 0))
        /* for 64-bit, targeting a 32-bit register zeroes the top bits => not a nop! */
        IF_X64(&&(instr_get_x86_mode(inst) || !opnd_is_reg(instr_get_dst(inst, 0)) ||
                  reg_get_size(opnd_get_reg(instr_get_dst(inst, 0))) != OPSZ_4)))
        return true;
    if (opcode == OP_xchg &&
        opnd_same(instr_get_dst(inst, 0), instr_get_dst(inst, 1))
        /* for 64-bit, targeting a 32-bit register zeroes the top bits => not a nop! */
        IF_X64(&&(instr_get_x86_mode(inst) ||
                  opnd_get_size(instr_get_dst(inst, 0)) != OPSZ_4)))
        return true;
    if (opcode == OP_lea &&
        opnd_is_base_disp(instr_get_src(inst, 0)) /* x64: rel, abs aren't base-disp */ &&
        opnd_get_disp(instr_get_src(inst, 0)) == 0 &&
        ((opnd_get_base(instr_get_src(inst, 0)) == opnd_get_reg(instr_get_dst(inst, 0)) &&
          opnd_get_index(instr_get_src(inst, 0)) == REG_NULL) ||
         (opnd_get_index(instr_get_src(inst, 0)) ==
              opnd_get_reg(instr_get_dst(inst, 0)) &&
          opnd_get_base(instr_get_src(inst, 0)) == REG_NULL &&
          opnd_get_scale(instr_get_src(inst, 0)) == 1)))
        return true;
    return false;
}

DR_API
bool
instr_is_exclusive_load(instr_t *instr)
{
    return false;
}

DR_API
bool
instr_is_exclusive_store(instr_t *instr)
{
    return false;
}

DR_API
bool
instr_is_scatter(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    case OP_vpscatterdd:
    case OP_vscatterdpd:
    case OP_vscatterdps:
    case OP_vpscatterdq:
    case OP_vpscatterqd:
    case OP_vscatterqpd:
    case OP_vscatterqps:
    case OP_vpscatterqq: return true;
    default: return false;
    }
}

DR_API
bool
instr_is_gather(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    case OP_vpgatherdd:
    case OP_vgatherdpd:
    case OP_vgatherdps:
    case OP_vpgatherdq:
    case OP_vpgatherqd:
    case OP_vgatherqpd:
    case OP_vgatherqps:
    case OP_vpgatherqq: return true;
    default: return false;
    }
}
