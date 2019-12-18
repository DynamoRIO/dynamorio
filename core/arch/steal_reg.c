/* **********************************************************
 * Copyright (c) 2000-2008 VMware, Inc.  All rights reserved.
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

/* x86/steal_reg.c */

#include "../globals.h"

#include "arch.h"
#include "instr.h"
#include "instrlist.h"
#include "steal_reg.h"
#include "../fragment.h"

/* N.B.:
 * For debugging reg stealing, it's nice to be able to only do reg stealing
 * on selected fragments.  To support this we allow STEAL_REGISTER to be defined
 * even when DCONTEXT_IN_EDI is not.
 */

/* around whole file: */
#ifdef STEAL_REGISTER

/* In the following functions, edi is assumed to be a pointer to the
 * dynamo context object.  The application's version of edi is kept
 * in memory, and we use ebx as a scratch register.  We use esi as
 * an alternative scratch register.  */

/* x86 notes:
 * cti instruction reg usage:
 *   jmp,jcc direct: 'J' = immed only = no regs used
 *   jmp indirect:   'E' = either single reg or base or base + index
 *                       = max 2 regs
 *   ret/lret: 'I' = immed only = no regs used
 *   jcxz and loop*: 'J' + ecx = uses ecx only
 *   calls are just like jmps
 * other instructions:
 *   string instrs use esi and edi
 *   rep uses ecx
 *   xlat uses al, ebx
 *   wrmsr uses eax, ecx, and edx -- but requires kernel mode
 * eax and edx are used for call return values
 *   (edx for 64-bit values)
 *
 * Must treat 8-bit registers separately!
 * 16-bit versions (e.g., w/ data size prefix) can be treated just
 * like 32-bit since in terms of modrm bits DI==EDI and we'll change
 * it to use BX/SI/DX just like in 32-bit mode we'd use EBX/ESI/EDX.
 */

/* you must pass in the NEXT instruction!
 * restore_state restores state prior to the instruction passed in!
 * you can pass in null, in which case restore_state will append
 * to ilist.
 */
void
restore_state(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist)
{
    if (ilist->flags == EDI_VAL_IN_MEM)
        return;

    if (instr != NULL) {
        /* insert store if edi's value in ebx doesn't match dcontext->xdi */
        if (ilist->flags != EDI_VAL_IN_EBX_AND_MEM) {
            instrlist_preinsert(
                ilist, instr,
                instr_create_save_to_dcontext(dcontext, REG_EBX, XDI_OFFSET));
        }

        /* restore application's value for ebx in ebx */
        instrlist_preinsert(
            ilist, instr,
            instr_create_restore_from_dcontext(dcontext, REG_EBX, XBX_OFFSET));

#    ifndef DCONTEXT_IN_EDI
        /* restore app's edi */
        instrlist_preinsert(
            ilist, instr,
            instr_create_restore_from_dcontext(dcontext, REG_EDI, XDI_OFFSET));
#    endif

    } else {
        if (ilist->flags != EDI_VAL_IN_EBX_AND_MEM) {
            instrlist_append(
                ilist, instr_create_save_to_dcontext(dcontext, REG_EBX, XDI_OFFSET));
        }

        instrlist_append(
            ilist, instr_create_restore_from_dcontext(dcontext, REG_EBX, XBX_OFFSET));

#    ifndef DCONTEXT_IN_EDI
        /* restore app's edi */
        instrlist_append(
            ilist, instr_create_restore_from_dcontext(dcontext, REG_EDI, XDI_OFFSET));
#    endif
    }

    ilist->flags = EDI_VAL_IN_MEM;
}

static void
expand_pusha(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist)
{
    /* Convert into the following sequence:
         pusha
         addl  $4, %esp         # %esp = %esp + 4
         push  edi_offset(%edi) # push real edi value onto stack
    */

    if (ilist->flags != EDI_VAL_IN_MEM)
        restore_state(dcontext, instr, ilist);

#    ifndef DCONTEXT_IN_EDI
    return;
#    endif

    /* reverse order! */

    /* build and insert push instruction as binary */
    instrlist_postinsert(ilist, instr,
                         instr_create_raw_3bytes(dcontext, 0xff,
                                                 0x77, /* %edi + 8-bit offset + /6 */
                                                 XDI_OFFSET));

    /* build and insert addl instruction as binary */
    instrlist_postinsert(ilist, instr,
                         instr_create_raw_3bytes(dcontext, 0x83,
                                                 0xc4, /* %esp + 8-bit immed + /0 */
                                                 4));
}

static void
expand_popa(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist)
{
#    ifndef DCONTEXT_IN_EDI
    return;
#    endif

    /* Convert into the following sequence:
         movl  (%esp), %ebx     # get edi value from stack
         movl  %ebx, edi_offset(%edi)   # save it in context
         movl  %edi, 12(%esp)   # squirrel away context ptr in esp location
         popa                   # NOTE: doesn't restore esp from stack
         movl  -20(%esp), %edi  # restore context ptr
    */
    instrlist_preinsert(ilist, instr,
                        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EBX),
                                            OPND_CREATE_MEM32(REG_ESP, 0)));
    instrlist_preinsert(ilist, instr,
                        instr_create_save_to_dcontext(dcontext, REG_EBX, XDI_OFFSET));
    instrlist_preinsert(ilist, instr,
                        INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_ESP, 12),
                                            opnd_create_reg(REG_EDI)));
    instrlist_postinsert(ilist, instr,
                         INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EDI),
                                             OPND_CREATE_MEM32(REG_ESP, -20)));
    ilist->flags = EDI_VAL_IN_MEM;
}

/* Handle implicit references to edi.  This routine assumes that
 * edi is both read and written, that no explicit operands exist
 * in the instruction, and that ebx is not used. */
/*
example:
  0x00405178   f3 a5                repz movs  %ds:%esi,%es:%edi
becomes
  0x0142cb94   89 5f 04             mov   %ebx,0x4(%edi)
  0x0142cb97   89 fb                mov   %edi,%ebx
  0x0142cb99   8b 7b 14             mov   0x14(%ebx),%edi
  0x0142cb9c   f3 a5                repz movs  %ds:%esi,%es:%edi
  0x0142cb9e   87 fb                xchg  %edi,%ebx
  0x0142cba0   89 5f 14             mov   %ebx,0x14(%edi)
  0x0142cba3   8b 5f 04             mov   0x4(%edi),%ebx
*/
static void
use_edi(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist)
{
#    ifndef DCONTEXT_IN_EDI
    /* save cur edi, then bring dcontext into edi */
    instrlist_preinsert(ilist, instr,
                        instr_create_save_to_dcontext(dcontext, REG_EDI, XDI_OFFSET));
    instrlist_preinsert(
        ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EDI),
                             opnd_create_immed_int((int)dcontext, OPSZ_PTR)));
#    endif

    if (ilist->flags == EDI_VAL_IN_MEM) {
        instrlist_preinsert(ilist, instr,
                            instr_create_save_to_dcontext(dcontext, REG_EBX, XBX_OFFSET));
        instrlist_preinsert(ilist, instr,
                            XINST_CREATE_move(dcontext, opnd_create_reg(REG_EBX),
                                             opnd_create_reg(REG_EDI));
        instrlist_preinsert(ilist, instr,
                            load_instr(dcontext, REG_EDI, REG_EBX, XDI_OFFSET));
    } else {
        /* create 'xchg %edi, %ebx' */
        instrlist_preinsert(ilist, instr,
                            INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_EDI),
                                              opnd_create_reg(REG_EBX)));
    }

    /* instruction can now use edi */

    /* create 'xchg %edi, %ebx' */
    instrlist_postinsert(
        ilist, instr,
        INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_EDI), opnd_create_reg(REG_EBX)));

    ilist->flags = EDI_VAL_IN_EBX;
}

/* An alternative rewriter: Rewrite the modrm byte using esi or edx, since
 *  ebx is unavailable (i.e., simultaneously used).
 *
example, with following instr that uses edi too (but not ebx):
  0x0040a492   8a 1f                mov   (%edi),%bl
  0x0040a494   47                   inc   %edi
becomes:
  0x0142faba   89 77 10             mov   %esi,0x10(%edi)
  0x0142fabd   8b 77 14             mov   0x14(%edi),%esi
  0x0142fac0   8a 1e                mov   (%esi),%bl
  0x0142fac2   8b 77 10             mov   0x10(%edi),%esi
  0x0142fac5   89 5f 04             mov   %ebx,0x4(%edi)
  0x0142fac8   8b 5f 14             mov   0x14(%edi),%ebx
  0x0142facb   43                   inc   %ebx
  0x0142facc   89 5f 14             mov   %ebx,0x14(%edi)
  0x0142facf   8b 5f 04             mov   0x4(%edi),%ebx
*/
static void
use_different_reg(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist, int reg,
                  int offs, bool read, bool write)
{
    if (ilist->flags != EDI_VAL_IN_MEM)
        restore_state(dcontext, instr, ilist); /* cannot use ebx */

#    ifndef DCONTEXT_IN_EDI
    /* save cur edi, then bring shared_dcontext into edi */
    instrlist_preinsert(ilist, instr,
                        instr_create_save_to_dcontext(dcontext, REG_EDI, XDI_OFFSET));
    instrlist_preinsert(
        ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EDI),
                             opnd_create_immed_int((int)dcontext, OPSZ_PTR)));
#    endif

    /* save current reg */
    instrlist_preinsert(ilist, instr, instr_create_save_to_dcontext(dcontext, reg, offs));
    if (read) {
        /* bring in current value of edi */
        instrlist_preinsert(
            ilist, instr, instr_create_restore_from_dcontext(dcontext, reg, XDI_OFFSET));
    }

    /* instr goes here */

    /* add to ilist in reverse order */
#    ifndef DCONTEXT_IN_EDI
    /* restore app's edi */
    instrlist_postinsert(
        ilist, instr,
        load_abs_instr(dcontext, REG_EDI, ((int)(&shared_dcontext)) + XDI_OFFSET));
#    endif
    /* restore previous value of reg */
    instrlist_postinsert(ilist, instr,
                         instr_create_restore_from_dcontext(dcontext, reg, offs));
    if (write) {
        /* copy value of edi back into memory location */
        instrlist_postinsert(ilist, instr,
                             instr_create_save_to_dcontext(dcontext, reg, XDI_OFFSET));
    }
}

/*###########################################################################
 *
 * Here are a bunch of (old) examples
 * Note that the current code will put in a direct memory reference when
 * it can, so push %edi => push 0x14(%edi)
 */
/* Instruction directly reads and writes edi, e.g. INC %edi.
   Register specifier is encoded directly in opcode byte. */
/*
example:
  0x00405d2a   47                   inc   %edi
becomes:
  0x0142b8c9   89 5f 04             mov   %ebx,0x4(%edi)
  0x0142b8cc   8b 5f 14             mov   0x14(%edi),%ebx
  0x0142b8cf   43                   inc   %ebx
  0x0142b8d0   89 5f 14             mov   %ebx,0x14(%edi)
  0x0142b8d3   8b 5f 04             mov   0x4(%edi),%ebx
example, with prev instr that uses edi:
  0x00409e49   0f b6 37             movzx (%edi),%esi
  0x00409e4c   47                   inc   %edi
becomes:
  0x0142fe80   89 5f 04             mov   %ebx,0x4(%edi)
  0x0142fe83   8b 5f 14             mov   0x14(%edi),%ebx
  0x0142fe86   0f b6 33             movzx (%ebx),%esi
  0x0142fe89   43                   inc   %ebx
  0x0142fe8a   89 5f 14             mov   %ebx,0x14(%edi)
  0x0142fe8d   8b 5f 04             mov   0x4(%edi),%ebx
*/
/* Instruction directly reads edi, e.g. PUSH %edi.
   Register specifier is encoded directly in opcode byte. */
/*
example:
  0x00409d60   57                   push  %edi
becomes:
  0x0142fbd1   89 5f 04             mov   %ebx,0x4(%edi)
  0x0142fbd4   8b 5f 14             mov   0x14(%edi),%ebx
  0x0142fbd7   53                   push  %ebx
  0x0142fbd8   8b 5f 04             mov   0x4(%edi),%ebx
example, with following instr that uses edi as well:
  0x00409e16   57                   push  %edi
  0x00409e17   8b 7c 24 14          mov   0x14(%esp),%edi
becomes:
  0x0142ff15   89 5f 04             mov   %ebx,0x4(%edi)
  0x0142ff18   8b 5f 14             mov   0x14(%edi),%ebx
  0x0142ff1b   53                   push  %ebx
  0x0142ff1c   8b 5c 24 14          mov   0x14(%esp),%ebx
  0x0142ff20   89 5f 14             mov   %ebx,0x14(%edi)
  0x0142ff23   8b 5f 04             mov   0x4(%edi),%ebx
*/
/* Instruction directly writes edi, e.g. POP %edi.
   Register specifier is encoded directly in opcode byte. */
/*
example:
  0x00409e99   5f                   pop   %edi
becomes:
  0x0142fce0   89 5f 04             mov   %ebx,0x4(%edi)
  0x0142fce3   5b                   pop   %ebx
  0x0142fce4   89 5f 14             mov   %ebx,0x14(%edi)
  0x0142fce7   8b 5f 04             mov   0x4(%edi),%ebx
*/
/*
example:
  0x00409e33   0f b6 07             movzx (%edi),%eax
becomes:
  0x0142fec0   89 5f 04             mov   %ebx,0x4(%edi)
  0x0142fec3   8b 5f 14             mov   0x14(%edi),%ebx
  0x0142fec6   0f b6 03             movzx (%ebx),%eax
  0x0142fec9   8b 5f 04             mov   0x4(%edi),%ebx
new example:
  0x00403de3   8b 3e                mov   (%esi) -> %edi
  0x00403de5   2b f8                sub   %eax %edi -> %edi
becomes:
  0x0536e663   89 5f 04             mov   %ebx -> 0x4(%edi)
  0x0536e666   8b 1e                mov   (%esi) -> %ebx
  0x0536e668   2b d8                sub   %eax %ebx -> %ebx
  0x0536e66a   89 5f 14             mov   %ebx -> 0x14(%edi)
  0x0536e66d   8b 5f 04             mov   0x4(%edi) -> %ebx
here's a good example of reg steal + indirect jmp mangling:
  0x77f831ff   ff 24 bd c0 32 f8 77 jmp   0x77f832c0(,%edi,4)
becomes:
  0x052de580   89 57 0c             mov   %edx -> 0xc(%edi)
  0x052de583   89 5f 04             mov   %ebx -> 0x4(%edi)
  0x052de586   8b 5f 14             mov   0x14(%edi) -> %ebx
  0x052de589   8b 14 bd c0 32 f8 77 mov   0x77f832c0(,%edi,4) -> %edx
  0x052de590   8b 5f 04             mov   0x4(%edi) -> %ebx
  0x052de593   e9 00 00 00 00       jmp   0x52de598 <exit stub 0>
*/
/*
 * end of examples
 *
 *###########################################################################*/

static void
use_ebx(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist, bool read, bool write)
{
    if (ilist->flags == EDI_VAL_IN_MEM) {

#    ifndef DCONTEXT_IN_EDI
        /* save cur edi, then bring shared_dcontext into edi */
        instrlist_preinsert(
            ilist, instr,
            store_abs_instr(dcontext, REG_EDI, ((int)(&shared_dcontext)) + XDI_OFFSET));
        instrlist_preinsert(ilist, instr,
                            move_immed_instr(dcontext, (int)&shared_dcontext, REG_EDI));
#    endif

        /* save current ebx */
        instrlist_preinsert(ilist, instr,
                            instr_create_save_to_dcontext(dcontext, REG_EBX, XBX_OFFSET));
        if (read) {
            /* bring value of edi into ebx */
            instrlist_preinsert(
                ilist, instr,
                instr_create_restore_from_dcontext(dcontext, REG_EBX, XDI_OFFSET));
            ilist->flags = EDI_VAL_IN_EBX_AND_MEM;
        }
    }

    /* instr goes here */

    if (write) {
        /* record that memory location of edi is stale */
        ilist->flags = EDI_VAL_IN_EBX;
    }
}

void
steal_reg(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist)
{
    opnd_t *uses[5];
#    ifdef DCONTEXT_IN_EDI
    opnd_t save[5];
#    endif
    int i, num_uses;
    bool writes, reads;

#    ifndef DCONTEXT_IN_EDI
#        if 0
    i = GLOBAL_STAT(num_fragments);
    /* use this to narrow location of reg steal bug */
    if (i < 0 || i > 1000000)
        return;
#        endif
#    endif

    /* special cases */
    switch (instr_get_opcode(instr)) {
    case OP_pusha: expand_pusha(dcontext, instr, ilist); return;
    case OP_popa: expand_popa(dcontext, instr, ilist); return;
    case OP_ins:
    case OP_rep_ins:
    case OP_outs:
    case OP_rep_outs:
    case OP_movs:
    case OP_rep_movs:
    case OP_stos:
    case OP_rep_stos:
    case OP_lods:
    case OP_rep_lods:
    case OP_cmps:
    case OP_rep_cmps:
    case OP_repne_cmps:
    case OP_scas:
    case OP_rep_scas:
    case OP_repne_scas:
        /* these all use edi implicitly, there's no way around it */
        use_edi(dcontext, instr, ilist);
        return;
    }

    if (!instr_uses_reg(instr, REG_EDI) && !uses_reg(instr, REG_DI)) {
        if (ilist->flags != EDI_VAL_IN_MEM)
            restore_state(dcontext, instr, ilist);
        return;
    }

    /* if we get to here we know we're going to change the operands,
     * possibly by directly writing to operand bytes, so we have to explicitly
     * set original bits invalid:
     */
    instr_set_raw_bits_valid(instr, false);

    /* gather edi usage info */
    num_uses = 0;
    writes = reads = false;
    for (i = 0; i < instr_num_dsts(instr); i++) {
        if (opnd_uses_reg(instr_get_dst(instr, i), REG_EDI) ||
            opnd_uses_reg(instr_get_dst(instr, i), REG_DI)) {
            /* FIXME: this code was written back when accessed instr fields
             * directly...need to fix this up
             */
#    error FIXME -- OLD CODE
            uses[num_uses++] = &instr->dsts[i];
            if (opnd_is_memory_reference(instr_get_dst(instr, i)))
                reads = true;
            else if (opnd_uses_reg(instr_get_dst(instr, i), REG_DI)) {
                reads = true; /* need to copy in upper 16 bits of edi */
                writes = true;
            } else
                writes = true;
        }
    }
    for (i = 0; i < instr_num_src(instr); i++) {
        if (opnd_uses_reg(instr_get_src(instr, i), REG_EDI) ||
            opnd_uses_reg(instr_get_src(instr, i), REG_DI)) {
            /* FIXME: this code was written back when accessed instr fields
             * directly...need to fix this up
             */
#    error FIXME -- OLD CODE
            if (i == 0)
                uses[num_uses++] = &instr->src0;
            else
                uses[num_uses++] = &instr->srcs[i];
            reads = true;
        }
    }
    ASSERT(num_uses > 0);

    /* Try replacing register edi with direct memory access
     * FIXME: try to use memory access to replace di?
     * need to select 16-bit data size, + 16 bit offset from 0x14(%edi)?
     * no 16-bit offset b/c of little-endian-ness!
     */
#    ifdef DCONTEXT_IN_EDI
    if (ilist->flags == EDI_VAL_IN_MEM) {
        for (i = 0; i < num_uses; i++) {
            save[i] = *uses[i];
            if (opnd_is_reg(save[i]) && opnd_get_reg(save[i]) == REG_EDI) {
                *uses[i] = opnd_create_base_disp(REG_EDI, REG_NULL, 0, XDI_OFFSET,
                                                 reg_get_size(REG_EDI));
            } else {
                /* give up */
                int j;
                for (j = 0; j < i; j++)
                    *uses[j] = save[j];
                i = 0;
                break;
            }
        }
        if (i > 0) { /* we didn't give up */
            /* switch to store if currently reg-reg "load" */
            if (instr_get_opcode(instr) == OP_mov_ld && num_uses == 1 && writes) {
                instr_set_opcode(instr, OP_mov_st);
                i = 0;
            }
            if (instr_is_encoding_possible(instr)) {
                /* It worked, we're done */
                LOG(THREAD, LOG_INTERP, 3,
                    "*** Successfully used memory to replace edi!\n");
                return;
            }
            /* Else, restore */
            if (i == 0) /* flag saying "changed OP_mov_ld to OP_mov_st" */
                instr_set_opcode(instr, OP_mov_ld);
            for (i = 0; i < num_uses; i++) {
                *uses[i] = save[i];
            }
        }
    }
#    endif

    if (!instr_uses_reg(instr, REG_EBX) && !instr_uses_reg(instr, REG_BX) &&
        !instr_uses_reg(instr, REG_BL) && !instr_uses_reg(instr, REG_BH)) {
        /* use ebx */
        use_ebx(dcontext, instr, ilist, reads, writes);
        for (i = 0; i < num_uses; i++) {
            opnd_replace_reg(uses[i], REG_EDI, REG_EBX);
            opnd_replace_reg(uses[i], REG_DI, REG_BX);
        }
    } else if (!instr_uses_reg(instr, REG_ESI) && !instr_uses_reg(instr, REG_SI)) {
        /* use esi */
        use_different_reg(dcontext, instr, ilist, REG_ESI, XSI_OFFSET, reads, writes);
        for (i = 0; i < num_uses; i++) {
            opnd_replace_reg(uses[i], REG_EDI, REG_ESI);
            opnd_replace_reg(uses[i], REG_DI, REG_SI);
        }
    } else {
        ASSERT(!instr_uses_reg(instr, REG_EDX) && !instr_uses_reg(instr, REG_DX) &&
               !instr_uses_reg(instr, REG_DL) && !instr_uses_reg(instr, REG_DH));
        /* use edx */
        use_different_reg(dcontext, instr, ilist, REG_EDX, XDX_OFFSET, reads, writes);
        for (i = 0; i < num_uses; i++) {
            opnd_replace_reg(uses[i], REG_EDI, REG_EDX);
            opnd_replace_reg(uses[i], REG_DI, REG_DX);
        }
    }
}

/*
sequence to study for better stealing:
  0x77fca2da   0f b7 38             movzx  (%eax) -> %edi
  0x77fca2dd   2b fb                sub    %ebx %edi -> %edi
  0x77fca2df   89 7d a8             mov    %edi -> 0xffffffa8(%ebp)
becomes:
  0x0265e0cd   89 5f 04             mov    %ebx -> 0x4(%edi)
  0x0265e0d0   0f b7 18             movzx  (%eax) -> %ebx
  0x0265e0d3   89 5f 14             mov    %ebx -> 0x14(%edi)
  0x0265e0d6   8b 5f 04             mov    0x4(%edi) -> %ebx
  0x0265e0d9   89 77 10             mov    %esi -> 0x10(%edi)
  0x0265e0dc   8b 77 14             mov    0x14(%edi) -> %esi
  0x0265e0df   2b f3                sub    %ebx %esi -> %esi
  0x0265e0e1   89 77 14             mov    %esi -> 0x14(%edi)
  0x0265e0e4   8b 77 10             mov    0x10(%edi) -> %esi
  0x0265e0e7   89 5f 04             mov    %ebx -> 0x4(%edi)
  0x0265e0ea   8b 5f 14             mov    0x14(%edi) -> %ebx
  0x0265e0ed   89 5d a8             mov    %ebx -> 0xffffffa8(%ebp)
  0x0265e0f0   8b 5f 04             mov    0x4(%edi) -> %ebx
*/

#endif /* STEAL_REGISTER (around whole file) */
