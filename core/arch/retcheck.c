/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2003 Massachusetts Institute of Technology */

/*
 * retcheck.c
 * Routines for the RETURN_AFTER_CALL and CHECK_RETURNS_SSE2 security features.
 * FIXME: Experimental.
 */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "instrlist.h"
#include "decode.h"

#include "../link.h" /* for frag tag */
#include "../fragment.h"
#include "../rct.h"
#include "instrument.h" /* for dr_insert_clean_call */

#ifdef CHECK_RETURNS_SSE2
/*
 * retcheck.c
 * Routines for the CHECK_RETURNS_SSE2 security feature.
 *
 * UNFINISHED:
 * There are two methods, one use a stack pointer the other a
 * constant top of stack.  Both can be optimized by using a
 * shared routine to reduce code bloat.  Need to evaluate an
 * optimized form of both and decide which is better!
 * Without shared code, the shift version is faster on gcc,
 * crafty, vortex, but table version is actually faster on the others!
 *
 * Crashes on release build on some programs
 * Stack ptr dies on eon & swim
 *
 * Need to provide asm code for win32 (currently #error)
 */

/* we have two ways of keeping our stack in the xmm registers:
 * use one of them as a stack pointer, or have a constant top of
 * stack and always shift the registers.
 */
#    define SSE2_USE_STACK_POINTER 0

/* keep mprotected stack in local or global heap? */
#    define USE_LOCAL_MPROT_STACK 0

#    if SSE2_USE_STACK_POINTER /* stack pointer and jump table method */
#        include "../fragment.h"
#        include "../link.h"
#    endif

/* make code more readable by shortening long lines */
#    define POST instrlist_postinsert
#    define PRE instrlist_preinsert

/* UNFINISHED:
 * start of code to have a shared routine for big table of sse2 instrs,
 * to reduce code bloat.
 * there is also code in arch.c and arch.h, under the same define
 * (CHECK_RETURNS_SSE2_EMIT)
 */
#    ifdef CHECK_RETURNS_SSE2_EMIT
/* in arch.c */
cache_pc
get_pextrw_entry(dcontext_t *dcontext);
cache_pc
get_pinsrw_entry(dcontext_t *dcontext);

byte *
emit_pextrw(dcontext_t *dcontext, byte *pc)
{
    instrlist_t ilist;

    /* initialize the ilist */
    instrlist_init(&ilist);

    for (i = 0; i < 62; i++) {
        instrlist_append(
            &ilist,
            INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_START_XMM + (i / 8)),
                                OPND_CREATE_MEM32(REG_ESP, 0), OPND_CREATE_INT8(i % 8)));
        instrlist_append(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(end)));
        instrlist_append(&ilist, INSTR_CREATE_nop(dcontext));
    }
    /* entry 62 */
    instrlist_append(
        &ilist,
        INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_START_XMM + (62 / 8)),
                            OPND_CREATE_MEM32(REG_ESP, 0), OPND_CREATE_INT8(62 % 8)));
    dr_insert_clean_call(dcontext, ilist, instr, (app_pc)check_return_too_deep,
                         false /*!fp*/, 1, OPND_CREATE_INTPTR(dcontext));

    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, false /* no instr targets */);
    ASSERT(pc != NULL);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}
#    endif /* CHECK_RETURNS_SSE2_EMIT */

#    if SSE2_USE_STACK_POINTER /* stack pointer and jump table method */
/* ################################################################################# */

/* instr should be the instr AFTER the call instr */
void
check_return_handle_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* ON CALL, STORE RETURN ADDRESS:
          push ra  // normal push of ra
          save ecx
          pextrw xmm7,7 -> ecx
          lea (,ecx,4) -> ecx      // ecx = ecx * 4, then ecx + ecx*2 = 12
          lea next_addr(ecx,ecx,2) -> ecx
          jmp ecx                  // pinsrw,jmp = 6+5 = 11 bytes, pad to 12
          0:  pinsrw (esp),0 -> xmm0; jmp end; nop
          1:  pinsrw (esp),1 -> xmm0; jmp end; nop
          8:  pinsrw (esp),0 -> xmm1; jmp end; nop
          62: pinsrw (esp),6 -> xmm7
          <clean call to check_return_too_deep>
                  // move 0..31 -> memory, mprotect the memory
                  // then slide 32..63 down
                  // set xmm7:7 to 30, let next instr inc it to get 31
        end:
          pextrw xmm7,7 -> ecx
          lea 1(ecx) -> ecx   // inc ecx
          pinsrw ecx,7 -> xmm7
          restore ecx
     */
    int i;
    instr_t *end = INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_ECX),
                                       opnd_create_reg(REG_XMM7), OPND_CREATE_INT8(7));
    PRE(ilist, instr, instr_create_save_to_dcontext(dcontext, REG_ECX, XCX_OFFSET));
    PRE(ilist, instr,
        INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_ECX), opnd_create_reg(REG_XMM7),
                            OPND_CREATE_INT8(7)));
    /* to get base+ecx*12, we do "ecx=ecx*4, ecx=base + ecx + ecx*2" */
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_NULL, REG_ECX, 4, 0, OPSZ_lea)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(
            dcontext, opnd_create_reg(REG_ECX),
            opnd_create_base_disp(REG_ECX, REG_ECX, 2, 0xaaaaaaaa, OPSZ_lea)));
#        if DISABLE_FOR_ANALYSIS
    PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(end)));
#        else
    PRE(ilist, instr, INSTR_CREATE_jmp_ind(dcontext, opnd_create_reg(REG_ECX)));
#        endif
    for (i = 0; i < 62; i++) {
        PRE(ilist, instr,
            INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_START_XMM + (i / 8)),
                                OPND_CREATE_MEM32(REG_ESP, 0), OPND_CREATE_INT8(i % 8)));
        PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(end)));
        PRE(ilist, instr, INSTR_CREATE_nop(dcontext));
    }
    /* entry 62 */
    PRE(ilist, instr,
        INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_START_XMM + (62 / 8)),
                            OPND_CREATE_MEM32(REG_ESP, 0), OPND_CREATE_INT8(62 % 8)));
    dr_insert_clean_call(dcontext, ilist, instr, (app_pc)check_return_too_deep,
                         false /*!fp*/, 1, OPND_CREATE_INTPTR(dcontext));
    PRE(ilist, instr, end);
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_ECX, REG_NULL, 0, 1, OPSZ_lea)));
#        if !DISABLE_FOR_ANALYSIS
    PRE(ilist, instr,
        INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_XMM7), opnd_create_reg(REG_ECX),
                            OPND_CREATE_INT8(7)));
#        endif
    PRE(ilist, instr, instr_create_restore_from_dcontext(dcontext, REG_ECX, XCX_OFFSET));
}

void
check_return_handle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* ON RETURN, CHECK RETURN ADDRESS:
          pop ra -> ecx  // normal pop
          save edx
          mov ecx, edx
          pextrw xmm7,7 -> ecx
          jecxz at_zero
          lea -1(ecx) -> ecx  // dec ecx
          jmp non_zero
        at_zero:
          mov 31, ecx
          <clean call to check_return_too_shallow> // restore 0..31 from memory
        non_zero:
          pinsrw ecx,7 -> xmm7     // store it back
          lea (,ecx,2) -> ecx      // ecx = ecx * 2
          lea next_addr(ecx,ecx,4) -> ecx   // ecx = ecx+ecx*4 = old_ecx*10
          jmp ecx                  // pextrw,jmp = 5+5 = 10 bytes
          0:  pextrw xmm0,0 -> ecx; jmp end
          1:  pextrw xmm0,1 -> ecx; jmp end
          8:  pextrw xmm1,0 -> ecx; jmp end
          62: pextrw xmm7,6 -> ecx;
        end:
          movzx dx,edx  // clear top 16 bits, for cmp w/ stored bottom 16 bits
          not %ecx
          lea 1(%ecx,%edx,1),%ecx  // "not ecx + 1" => -ecx, to cmp w/ edx
          jecxz ra_not_mangled
          call ra_mangled
        ra_not_mangled:
          restore edx
// FIXME: can't count on below esp not being clobbered!  (could get signal->handler!)
          mov -4(esp),ecx // restore return address
    */
    int i;
    instr_t *ra_not_mangled =
        instr_create_restore_from_dcontext(dcontext, REG_EDX, XDX_OFFSET);
    instr_t *end =
        INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_EDX), opnd_create_reg(REG_DX));
    instr_t *at_zero =
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_ECX), OPND_CREATE_INT32(31));
    instr_t *non_zero =
        INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_XMM7), opnd_create_reg(REG_ECX),
                            OPND_CREATE_INT8(7));
    PRE(ilist, instr, instr_create_save_to_dcontext(dcontext, REG_EDX, XDX_OFFSET));
    PRE(ilist, instr,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EDX),
                            opnd_create_reg(REG_ECX)));
    PRE(ilist, instr,
        INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_ECX), opnd_create_reg(REG_XMM7),
                            OPND_CREATE_INT8(7)));
#        if DISABLE_FOR_ANALYSIS
    PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(ra_not_mangled)));
#        endif
    PRE(ilist, instr, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(at_zero)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_ECX, REG_NULL, 0, -1, OPSZ_lea)));
    PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(non_zero)));
    PRE(ilist, instr, at_zero);
    dr_insert_clean_call(dcontext, ilist, instr, (app_pc)check_return_too_shallow,
                         false /*!fp*/, 1, OPND_CREATE_INTPTR(dcontext));
    PRE(ilist, instr, non_zero);
    /* to get base+ecx*10, we do "ecx=ecx*2, ecx=base + ecx + ecx*4" */
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_NULL, REG_ECX, 2, 0, OPSZ_lea)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(
            dcontext, opnd_create_reg(REG_ECX),
            opnd_create_base_disp(REG_ECX, REG_ECX, 4, 0xaaaaaaaa, OPSZ_lea)));
    PRE(ilist, instr, INSTR_CREATE_jmp_ind(dcontext, opnd_create_reg(REG_ECX)));
    for (i = 0; i < 63; i++) {
        PRE(ilist, instr,
            INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_ECX),
                                opnd_create_reg(REG_START_XMM + (i / 8)),
                                OPND_CREATE_INT8(i % 8)));
        PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(end)));
    }
    PRE(ilist, instr, end);
    PRE(ilist, instr, INSTR_CREATE_not(dcontext, opnd_create_reg(REG_ECX)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_ECX, REG_EDX, 1, 1, OPSZ_lea)));
    PRE(ilist, instr, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(ra_not_mangled)));
    dr_insert_clean_call(dcontext, ilist, instr, (app_pc)check_return_ra_mangled,
                         false /*!fp*/, 1, OPND_CREATE_INTPTR(dcontext));
    PRE(ilist, instr, ra_not_mangled);
    PRE(ilist, instr,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX),
                            OPND_CREATE_MEM32(REG_ESP, -4)));
}

/* touches up jmp* for table (needs address of start of table) */
void
finalize_return_check(dcontext_t *dcontext, fragment_t *f)
{
    byte *start_pc = (byte *)FCACHE_ENTRY_PC(f);
    byte *end_pc = fragment_body_end_pc(dcontext, f);
    byte *pc, *prev_pc;
    int leas_next = 0;
    instr_t instr;
    instr_init(dcontext, &instr);
    LOG(THREAD, LOG_ALL, 3, "finalize_return_check\n");

    SELF_PROTECT_CACHE(dcontext, f, WRITABLE);

    /* must fix up indirect jmp */
    pc = start_pc;
    do {
        prev_pc = pc;
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
        ASSERT(instr_valid(&instr)); /* our own code! */
        if (leas_next == 2) {
            d_r_loginst(dcontext, 3, &instr, "\tlea 2");
            if (instr_get_opcode(&instr) == OP_lea) {
                opnd_t op = instr_get_src(&instr, 0);
                int scale = opnd_get_scale(op);
                DEBUG_DECLARE(byte * nxt_pc;)
                /* put in pc of instr after jmp: jmp is 2 bytes long */
                instr_set_src(&instr, 0,
                              opnd_create_base_disp(REG_ECX, REG_ECX, scale,
                                                    (int)(pc + 2), OPSZ_lea));
                DEBUG_DECLARE(nxt_pc =) instr_encode(dcontext, &instr, prev_pc);
                ASSERT(nxt_pc != NULL);
            }
            leas_next = 0;
        }
        if (leas_next == 1) {
            d_r_loginst(dcontext, 3, &instr, "\tlea 1");
            if (instr_get_opcode(&instr) == OP_lea)
                leas_next = 2;
            else
                leas_next = 0;
        }
        /* we don't allow program to use sse, so pextrw/pinsrw are all ours */
        if (leas_next == 0 && instr_get_opcode(&instr) == OP_pextrw &&
            opnd_is_reg(instr_get_src(&instr, 0)) &&
            opnd_get_reg(instr_get_src(&instr, 0)) == REG_XMM7 &&
            opnd_is_immed_int(instr_get_src(&instr, 1)) &&
            opnd_get_immed_int(instr_get_src(&instr, 1)) == 7) {
            d_r_loginst(dcontext, 3, &instr, "\tfound pextrw");
            leas_next = 1;
        } else if (leas_next == 0 && instr_get_opcode(&instr) == OP_pinsrw &&
                   opnd_is_reg(instr_get_dst(&instr, 0)) &&
                   opnd_get_reg(instr_get_dst(&instr, 0)) == REG_XMM7 &&
                   opnd_is_immed_int(instr_get_src(&instr, 1)) &&
                   opnd_get_immed_int(instr_get_src(&instr, 1)) == 7) {
            d_r_loginst(dcontext, 3, &instr, "\tfound pinsrw");
            leas_next = 1;
        }
    } while (pc < end_pc);
    instr_free(dcontext, &instr);

    SELF_PROTECT_CACHE(dcontext, f, READONLY);
}

typedef struct _call_stack_32 {
    byte retaddr[32][2];
    struct _call_stack_32 *next;
} call_stack_32_t;

/* move 0..31 -> memory, mprotect the memory
 * then slide 32..63 down
 * set xmm7:7 to 30, let next instr inc it to get 31
 */
void
check_return_too_deep(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
                      volatile reg_t reg_edi, volatile reg_t reg_esi,
                      volatile reg_t reg_ebp, volatile reg_t reg_esp,
                      volatile reg_t reg_ebx, volatile reg_t reg_edx,
                      volatile reg_t reg_ecx, volatile reg_t reg_eax)
{
    byte xmm[8][16]; /* each sse2 is 128 bits = 16 bytes */
    call_stack_32_t *stack;

    ENTERING_DR();
#        if USE_LOCAL_MPROT_STACK
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);
#        endif

#        if USE_LOCAL_MPROT_STACK
    stack = heap_alloc(dcontext, sizeof(call_stack_32_t));
#        else
    stack = global_heap_alloc(sizeof(call_stack_32_t) HEAPACCT(ACCT_OTHER));
#        endif
    stack->next = dcontext->call_stack;
    dcontext->call_stack = stack;

    LOG(THREAD, LOG_ALL, 3, "check_return_too_deep\n");

    /* move from registers into memory where we can work with it */
    /* FIXME: align xmm so can use movdqa! */
#        ifdef UNIX
    asm("movdqu %%xmm0, %0" : "=m"(xmm[0]));
    asm("movdqu %%xmm1, %0" : "=m"(xmm[1]));
    asm("movdqu %%xmm2, %0" : "=m"(xmm[2]));
    asm("movdqu %%xmm3, %0" : "=m"(xmm[3]));
    asm("movdqu %%xmm4, %0" : "=m"(xmm[4]));
    asm("movdqu %%xmm5, %0" : "=m"(xmm[5]));
    asm("movdqu %%xmm6, %0" : "=m"(xmm[6]));
    asm("movdqu %%xmm7, %0" : "=m"(xmm[7]));
#        else
#            error NYI
#        endif

    LOG(THREAD, LOG_ALL, 3, "\tjust copied registers\n");

    /* we want 0..31 into our stack, that's the first 64 bytes */
    memcpy(stack->retaddr, xmm[0], 64);

#        ifdef DEBUG
    if (d_r_stats->loglevel >= 3) {
        int i, j;
        LOG(THREAD, LOG_ALL, 3, "Copied into stored stack:\n");
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 8; j++) {
                LOG(THREAD, LOG_ALL, 3, "\t%d %d 0x%02x%02x", i, j,
                    stack->retaddr[i * 8 + j][0], stack->retaddr[i * 8 + j][1]);
                if (j % 4 == 3)
                    LOG(THREAD, LOG_ALL, 3, "\n");
            }
        }
        LOG(THREAD, LOG_ALL, 3, "Before shifting:\n");
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                LOG(THREAD, LOG_ALL, 3, "\t%d %d 0x%02x%02x", i, j, xmm[i][j * 2],
                    xmm[i][j * 2 + 1]);
                if (j % 4 == 3)
                    LOG(THREAD, LOG_ALL, 3, "\n");
            }
        }
    }
#        endif

    /* now slide 32..63 down */
    memcpy(xmm[0], xmm[4], 64);

    /* move back into registers */
#        ifdef UNIX
    asm("movdqu %0, %%xmm0" : : "m"(xmm[0][0]));
    asm("movdqu %0, %%xmm1" : : "m"(xmm[1][0]));
    asm("movdqu %0, %%xmm2" : : "m"(xmm[2][0]));
    asm("movdqu %0, %%xmm3" : : "m"(xmm[3][0]));
    asm("movl $30, %eax");
    asm("pinsrw $7,%eax,%xmm7");
#        else
#            error NYI
#        endif

    dcontext->call_depth++;

    LOG(THREAD, LOG_ALL, 3, "\tdone, call depth is now %d\n", dcontext->call_depth);

#        if USE_LOCAL_MPROT_STACK
    SELF_PROTECT_LOCAL(dcontext, READONLY);
#        endif
    EXITING_DR();
}

void
check_return_too_shallow(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
                         volatile reg_t reg_edi, volatile reg_t reg_esi,
                         volatile reg_t reg_ebp, volatile reg_t reg_esp,
                         volatile reg_t reg_ebx, volatile reg_t reg_edx,
                         volatile reg_t reg_ecx, volatile reg_t reg_eax)
{
    ENTERING_DR();
#        if USE_LOCAL_MPROT_STACK
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);
#        endif

    LOG(THREAD, LOG_ALL, 3, "check_return_too_shallow\n");
    if (dcontext->call_depth == 0) {
        LOG(THREAD, LOG_ALL, 3, "\tbottomed out of dynamo, ignoring\n");
        reg_ecx = 0; /* undo the set to 31 prior to this call */
                     /* FIXME: would like to avoid rest of checks...but then have to put
                      * clean-call-cleanup at bottom...instead we have a hack where we put
                      * in a ret addr that will match, namely the real ret addr, sitting in edx
                      */
#        ifdef UNIX
        asm("movl %0, %%eax" : : "m"(reg_edx));
        asm("pinsrw $0,%eax,%xmm0");
#        else
#            error NYI
#        endif
        LOG(THREAD, LOG_ALL, 3, "\tset xmm0:0 to " PFX "\n", reg_edx);
    } else {
        /* restore 0..31 from memory */
        call_stack_32_t *stack = dcontext->call_stack;
        ASSERT(stack != NULL);
        /* move back into registers */
#        ifdef UNIX
        asm("movl %0, %%eax" : : "m"(stack->retaddr));
        asm("movdqu (%eax),     %xmm0");
        asm("movdqu 0x10(%eax), %xmm1");
        asm("movdqu 0x20(%eax), %xmm2");
        asm("movdqu 0x30(%eax), %xmm3");
#        else
#            error NYI
#        endif
#        ifdef DEBUG
        if (d_r_stats->loglevel >= 3) {
            int i, j;
            LOG(THREAD, LOG_ALL, 3, "Restored:\n");
            for (i = 0; i < 4; i++) {
                for (j = 0; j < 8; j++) {
                    LOG(THREAD, LOG_ALL, 3, "\t%d %d 0x%02x%02x", i, j,
                        stack->retaddr[i * 8 + j][0], stack->retaddr[i * 8 + j][1]);
                    if (j % 4 == 3)
                        LOG(THREAD, LOG_ALL, 3, "\n");
                }
            }
        }
#        endif
        stack = stack->next;
#        if USE_LOCAL_MPROT_STACK
        heap_free(dcontext, dcontext->call_stack, sizeof(call_stack_32_t));
#        else
        global_heap_free(dcontext->call_stack,
                         sizeof(call_stack_32_t) HEAPACCT(ACCT_OTHER));
#        endif
        dcontext->call_stack = stack;
        dcontext->call_depth--;
        LOG(THREAD, LOG_ALL, 3, "\tdone, call depth is now %d\n", dcontext->call_depth);
    }

#        if USE_LOCAL_MPROT_STACK
    SELF_PROTECT_LOCAL(dcontext, READONLY);
#        endif
    EXITING_DR();
}

void
check_return_ra_mangled(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
                        volatile reg_t reg_edi, volatile reg_t reg_esi,
                        volatile reg_t reg_ebp, volatile reg_t reg_esp,
                        volatile reg_t reg_ebx, volatile reg_t reg_edx,
                        volatile reg_t reg_ecx, volatile reg_t reg_eax)
{
    /* ecx had addr, then we did ecx' = edx-ecx, so old ecx = edx - ecx' */
    int stored_addr = reg_edx - reg_ecx;

    ENTERING_DR();
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);

#        ifdef DEBUG
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_ALL) != 0) {
        int idx;
#            ifdef UNIX
        asm("pextrw $7,%xmm7,%eax");
        asm("movl %%eax, %0" : "=m"(idx));
#            else
#                error NYI
#            endif
        LOG(THREAD, LOG_ALL, 3,
            "check_return_ra_mangled: stored=" PFX " vs real=" PFX ", idx=%d\n",
            stored_addr, reg_edx, idx);
    }
#        endif

    SYSLOG_INTERNAL_ERROR("ERROR: return address was mangled (bottom 16 bits: "
                          "0x%04x => 0x%04x)",
                          (reg_edx & 0x0000ffff), stored_addr);
    ASSERT_NOT_REACHED();

    SELF_PROTECT_LOCAL(dcontext, READONLY);
    EXITING_DR();
}

#    else /* !SSE2_USE_STACK_POINTER */
/* ################################################################################# */
/* instr should be the instr AFTER the call instr */
void
check_return_handle_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* ON CALL, STORE RETURN ADDRESS:
          push ra  // normal push of ra
          save ecx
          pextrw xmm7,7 -> ecx
          lea -63(ecx) -> ecx
          jecxz overflow
          jmp non_overflow
        overflow:
          <clean call to check_return_too_deep>
                  // move 31..62 -> memory, mprotect the memory
                  // set xmm7:7 to 32 by setting ecx = 32-64
        non_overflow:
          pslldq xmm7,2        # shift left one word
          lea 64(ecx) -> ecx   # restore, plus increment, the index
          pinsrw ecx,7 -> xmm7 # put index in its slot
          pextrw xmm6,7 -> ecx # move top of 6 to bottom of 7
          pinsrw ecx,0 -> xmm7 #
          pslldq xmm6,2        # now shift 6 left one word
          pextrw xmm5,7 -> ecx # move top of 5 to bottom of 6
          pinsrw ecx,0 -> xmm6 #
          pslldq xmm5,2        # now shift 5 left one word
          pextrw xmm4,7 -> ecx # move top of 4 to bottom of 5
          pinsrw ecx,0 -> xmm5 #
          pslldq xmm4,2        # now shift 4 left one word
          pextrw xmm3,7 -> ecx # move top of 3 to bottom of 4
          pinsrw ecx,0 -> xmm4 #
          pslldq xmm3,2        # now shift 3 left one word
          pextrw xmm2,7 -> ecx # move top of 2 to bottom of 3
          pinsrw ecx,0 -> xmm3 #
          pslldq xmm2,2        # now shift 2 left one word
          pextrw xmm1,7 -> ecx # move top of 1 to bottom of 2
          pinsrw ecx,0 -> xmm2 #
          pslldq xmm1,2        # now shift 1 left one word
          pextrw xmm0,7 -> ecx # move top of 0 to bottom of 1
          pinsrw ecx,0 -> xmm1 #
          pslldq xmm0,2        # now shift 0 left one word
          pinsrw (esp),0 -> xmm0   # now store new return address
        end:
          restore ecx
     */
    int i;
    instr_t *end = instr_create_restore_from_dcontext(dcontext, REG_ECX, XCX_OFFSET);
    instr_t *overflow = INSTR_CREATE_nop(dcontext);
    instr_t *non_overflow =
        INSTR_CREATE_pslldq(dcontext, opnd_create_reg(REG_XMM7), OPND_CREATE_INT8(2));
    PRE(ilist, instr, instr_create_save_to_dcontext(dcontext, REG_ECX, XCX_OFFSET));
    PRE(ilist, instr,
        INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_ECX), opnd_create_reg(REG_XMM7),
                            OPND_CREATE_INT8(7)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_ECX, REG_NULL, 0, -63, OPSZ_lea)));
    PRE(ilist, instr, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(overflow)));
    PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(non_overflow)));
    PRE(ilist, instr, overflow);
    dr_insert_clean_call(dcontext, ilist, instr, (app_pc)check_return_too_deep,
                         false /*!fp*/, 1, OPND_CREATE_INTPTR(dcontext));
    PRE(ilist, instr, non_overflow);
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_ECX, REG_NULL, 0, 64, OPSZ_lea)));
    PRE(ilist, instr,
        INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_XMM7), opnd_create_reg(REG_ECX),
                            OPND_CREATE_INT8(7)));
    for (i = 6; i >= 0; i--) {
        PRE(ilist, instr,
            INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_ECX),
                                opnd_create_reg(REG_START_XMM + i), OPND_CREATE_INT8(7)));
        PRE(ilist, instr,
            INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_START_XMM + i + 1),
                                opnd_create_reg(REG_ECX), OPND_CREATE_INT8(0)));
        PRE(ilist, instr,
            INSTR_CREATE_pslldq(dcontext, opnd_create_reg(REG_START_XMM + i),
                                OPND_CREATE_INT8(2)));
    }
    PRE(ilist, instr,
        INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_XMM0),
                            OPND_CREATE_MEM32(REG_ESP, 0), OPND_CREATE_INT8(0)));
    PRE(ilist, instr, end);
}

#        ifdef DEBUG
#            if 0 /* not used */
static void
check_debug_regs(dcontext_t *dcontext,
             volatile int errno, volatile reg_t eflags,
             volatile reg_t reg_edi, volatile reg_t reg_esi,
             volatile reg_t reg_ebp, volatile reg_t reg_esp,
             volatile reg_t reg_ebx, volatile reg_t reg_edx,
             volatile reg_t reg_ecx, volatile reg_t reg_eax)
{
    LOG(THREAD, LOG_ALL, 3, "check_debug2: eax="PFX" ecx="PFX" edx="PFX" ebx="PFX"\n"
        "esp="PFX" ebp="PFX" esi="PFX" edi="PFX"\n",
        reg_eax, reg_ecx, reg_edx, reg_ebx, reg_esp, reg_ebp, reg_esi, reg_edi);
}
#            endif

static void
check_debug(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
            volatile reg_t reg_edi, volatile reg_t reg_esi, volatile reg_t reg_ebp,
            volatile reg_t reg_esp, volatile reg_t reg_ebx, volatile reg_t reg_edx,
            volatile reg_t reg_ecx, volatile reg_t reg_eax)
{
    ENTERING_DR();
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);
    if (d_r_stats->loglevel >= 3) {
        int i, j;
        byte xmm[8][16]; /* each sse2 is 128 bits = 16 bytes */
        /* move from registers into memory where we can work with it */
#            ifdef UNIX
        asm("movdqu %%xmm0, %0" : "=m"(xmm[0]));
        asm("movdqu %%xmm1, %0" : "=m"(xmm[1]));
        asm("movdqu %%xmm2, %0" : "=m"(xmm[2]));
        asm("movdqu %%xmm3, %0" : "=m"(xmm[3]));
        asm("movdqu %%xmm4, %0" : "=m"(xmm[4]));
        asm("movdqu %%xmm5, %0" : "=m"(xmm[5]));
        asm("movdqu %%xmm6, %0" : "=m"(xmm[6]));
        asm("movdqu %%xmm7, %0" : "=m"(xmm[7]));
#            else
#                error NYI
#            endif
        LOG(THREAD, LOG_ALL, 3, "on our stack (in edx is " PFX "):\n", reg_edx);
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                LOG(THREAD, LOG_ALL, 3, "\t%d %d 0x%02x%02x", i, j, xmm[i][j * 2 + 1],
                    xmm[i][j * 2]);
                if (j % 4 == 3)
                    LOG(THREAD, LOG_ALL, 3, "\n");
            }
        }
    }
    SELF_PROTECT_LOCAL(dcontext, READONLY);
    EXITING_DR();
}
#        endif /* DEBUG */

void
check_return_handle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* ON RETURN, CHECK RETURN ADDRESS:
          pop ra -> ecx  // normal pop
          save edx
          mov ecx, edx
          save ebx
          pextrw xmm7,7 -> ecx
          jecxz at_zero
          lea -1(ecx) -> ecx    # dec ecx
          pinsrw ecx,7 -> xmm7  # store index
          jmp non_zero
        at_zero:
          <clean call to check_return_too_shallow>
                  // restore from memory to 0..31
                  // copy xmm0:0 into ebx
                  // shift 1..31 down into 0..30
                  // set xmm7:7 to 31
          jmp end
        non_zero:
          pextrw xmm0,0 -> ebx
          psrldq xmm0,2         # shift 0 right one word
          pextrw xmm1,0 -> ecx  # move bottom of 1 to top of 0
          pinsrw ecx,7 -> xmm0
          psrldq xmm1,2
          pextrw xmm2,0 -> ecx  # move bottom of 2 to top of 1
          pinsrw ecx,7 -> xmm1
          psrldq xmm2,2
          pextrw xmm3,0 -> ecx  # move bottom of 3 to top of 2
          pinsrw ecx,7 -> xmm2
          psrldq xmm3,2
          pextrw xmm4,0 -> ecx  # move bottom of 4 to top of 3
          pinsrw ecx,7 -> xmm3
          psrldq xmm4,2
          pextrw xmm5,0 -> ecx  # move bottom of 5 to top of 4
          pinsrw ecx,7 -> xmm4
          psrldq xmm5,2
          pextrw xmm6,0 -> ecx  # move bottom of 6 to top of 5
          pinsrw ecx,7 -> xmm5
          psrldq xmm6,2
          pextrw xmm7,0 -> ecx  # move bottom of 7 to top of 6
          pinsrw ecx,7 -> xmm6
          psrldq xmm7,2
          pextrw xmm7,6 -> ecx  # shift index back to top slot
          pinsrw ecx,7 -> xmm7
        end:
          mov edx,ecx
          movzx cx,ecx  // clear top 16 bits, for cmp w/ stored bottom 16 bits
          not %ebx
          lea 1(%ebx,%ecx,1),%ecx  // "not ebx + 1" => -ecx, to cmp w/ ecx
          jecxz ra_not_mangled
          call ra_mangled
        ra_not_mangled:
          restore ebx
          mov edx, ecx // restore ret addr
          restore edx
    */
    int i;
    instr_t *ra_not_mangled =
        instr_create_restore_from_dcontext(dcontext, REG_EBX, XBX_OFFSET);
    instr_t *end =
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX), opnd_create_reg(REG_EDX));
    instr_t *at_zero = INSTR_CREATE_nop(dcontext);
    instr_t *non_zero =
        INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_EBX), opnd_create_reg(REG_XMM0),
                            OPND_CREATE_INT8(0));
    PRE(ilist, instr, instr_create_save_to_dcontext(dcontext, REG_EDX, XDX_OFFSET));
    PRE(ilist, instr,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EDX),
                            opnd_create_reg(REG_ECX)));
    PRE(ilist, instr, instr_create_save_to_dcontext(dcontext, REG_EBX, XBX_OFFSET));

#        ifdef DEBUG
    if (d_r_stats->loglevel >= 4) {
        dr_insert_clean_call(dcontext, ilist, instr, (app_pc)check_debug, false /*!fp*/,
                             1, OPND_CREATE_INTPTR(dcontext));
    }
#        endif

    PRE(ilist, instr,
        INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_ECX), opnd_create_reg(REG_XMM7),
                            OPND_CREATE_INT8(7)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EBX),
                            opnd_create_reg(REG_ECX)));
    PRE(ilist, instr, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(at_zero)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_ECX, REG_NULL, 0, -1, OPSZ_lea)));
    PRE(ilist, instr,
        INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_XMM7), opnd_create_reg(REG_ECX),
                            OPND_CREATE_INT8(7)));
    PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(non_zero)));
    PRE(ilist, instr, at_zero);
    dr_insert_clean_call(dcontext, ilist, instr, (app_pc)check_return_too_shallow,
                         false /*!fp*/, 1, OPND_CREATE_INTPTR(dcontext));
    PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(end)));
    PRE(ilist, instr, non_zero);
    PRE(ilist, instr,
        INSTR_CREATE_psrldq(dcontext, opnd_create_reg(REG_XMM0), OPND_CREATE_INT8(2)));
    for (i = 1; i <= 7; i++) {
        PRE(ilist, instr,
            INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_ECX),
                                opnd_create_reg(REG_START_XMM + i), OPND_CREATE_INT8(0)));
        PRE(ilist, instr,
            INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_START_XMM + i - 1),
                                opnd_create_reg(REG_ECX), OPND_CREATE_INT8(7)));
        PRE(ilist, instr,
            INSTR_CREATE_psrldq(dcontext, opnd_create_reg(REG_START_XMM + i),
                                OPND_CREATE_INT8(2)));
    }
    PRE(ilist, instr,
        INSTR_CREATE_pextrw(dcontext, opnd_create_reg(REG_ECX), opnd_create_reg(REG_XMM7),
                            OPND_CREATE_INT8(6)));
    PRE(ilist, instr,
        INSTR_CREATE_pinsrw(dcontext, opnd_create_reg(REG_XMM7), opnd_create_reg(REG_ECX),
                            OPND_CREATE_INT8(7)));
    PRE(ilist, instr, end);
    PRE(ilist, instr,
        INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_ECX), opnd_create_reg(REG_CX)));
    PRE(ilist, instr, INSTR_CREATE_not(dcontext, opnd_create_reg(REG_EBX)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_EBX, REG_ECX, 1, 1, OPSZ_lea)));
    PRE(ilist, instr, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(ra_not_mangled)));
    dr_insert_clean_call(dcontext, ilist, instr, (app_pc)check_return_ra_mangled,
                         false /*!fp*/, 1, OPND_CREATE_INTPTR(dcontext));
    PRE(ilist, instr, ra_not_mangled);
    PRE(ilist, instr,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX),
                            opnd_create_reg(REG_EDX)));
    PRE(ilist, instr, instr_create_restore_from_dcontext(dcontext, REG_EDX, XDX_OFFSET));
}

/* touches up jmp* for table (needs address of start of table) */
void
finalize_return_check(dcontext_t *dcontext, fragment_t *f)
{
}

typedef struct _call_stack_32 {
    byte retaddr[32][2];
    struct _call_stack_32 *next;
} call_stack_32_t;

/* move 0..31 -> memory, mprotect the memory
 * then slide 32..63 down
 * set xmm7:7 to 30, let next instr inc it to get 31
 */
void
check_return_too_deep(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
                      volatile reg_t reg_edi, volatile reg_t reg_esi,
                      volatile reg_t reg_ebp, volatile reg_t reg_esp,
                      volatile reg_t reg_ebx, volatile reg_t reg_edx,
                      volatile reg_t reg_ecx, volatile reg_t reg_eax)
{
    byte xmm[8][16]; /* each sse2 is 128 bits = 16 bytes */
    call_stack_32_t *stack;

    ENTERING_DR();
#        if USE_LOCAL_MPROT_STACK
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);
#        endif

#        if USE_LOCAL_MPROT_STACK
    stack = heap_alloc(dcontext, sizeof(call_stack_32_t));
#        else
    stack = global_heap_alloc(sizeof(call_stack_32_t) HEAPACCT(ACCT_OTHER));
#        endif
    stack->next = dcontext->call_stack;
    dcontext->call_stack = stack;

    LOG(THREAD, LOG_ALL, 3, "check_return_too_deep\n");

    /* move from registers into memory where we can work with it */
    /* FIXME: align xmm so can use movdqa! */
#        ifdef UNIX
    asm("movdqu %%xmm0, %0" : "=m"(xmm[0]));
    asm("movdqu %%xmm1, %0" : "=m"(xmm[1]));
    asm("movdqu %%xmm2, %0" : "=m"(xmm[2]));
    asm("movdqu %%xmm3, %0" : "=m"(xmm[3]));
    asm("movdqu %%xmm4, %0" : "=m"(xmm[4]));
    asm("movdqu %%xmm5, %0" : "=m"(xmm[5]));
    asm("movdqu %%xmm6, %0" : "=m"(xmm[6]));
    asm("movdqu %%xmm7, %0" : "=m"(xmm[7]));
#        else
#            error NYI
#        endif

    LOG(THREAD, LOG_ALL, 3, "\tjust copied registers\n");

    /* we want 31..62 into our stack, that's the last 64 bytes before index */
    memcpy(stack->retaddr, &xmm[3][14], 64);

#        ifdef DEBUG
    if (d_r_stats->loglevel >= 3) {
        int i, j;
        LOG(THREAD, LOG_ALL, 3, "Copied into stored stack:\n");
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 8; j++) {
                LOG(THREAD, LOG_ALL, 3, "\t%d %d 0x%02x%02x", i, j,
                    stack->retaddr[i * 8 + j][1], stack->retaddr[i * 8 + j][0]);
                if (j % 4 == 3)
                    LOG(THREAD, LOG_ALL, 3, "\n");
            }
        }
        LOG(THREAD, LOG_ALL, 3, "Before shifting:\n");
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                LOG(THREAD, LOG_ALL, 3, "\t%d %d 0x%02x%02x", i, j, xmm[i][j * 2 + 1],
                    xmm[i][j * 2]);
                if (j % 4 == 3)
                    LOG(THREAD, LOG_ALL, 3, "\n");
            }
        }
    }
#        endif

#        if !DISABLE_FOR_ANALYSIS
    /* move back into registers */
#            ifdef UNIX
    asm("movdqu %0, %%xmm0" : : "m"(xmm[0][0]));
    asm("movdqu %0, %%xmm1" : : "m"(xmm[1][0]));
    asm("movdqu %0, %%xmm2" : : "m"(xmm[2][0]));
    asm("movdqu %0, %%xmm3" : : "m"(xmm[3][0]));
    asm("movl $30, %eax");
    asm("pinsrw $7,%eax,%xmm7");
#            else
#                error NYI
#            endif
#        endif

    /* set to 32...but will have 64 added to it, so sub that now */
    reg_ecx = 32 - 64;

    dcontext->call_depth++;

    LOG(THREAD, LOG_ALL, 3, "\tdone, call depth is now %d\n", dcontext->call_depth);

#        if USE_LOCAL_MPROT_STACK
    SELF_PROTECT_LOCAL(dcontext, READONLY);
#        endif
    EXITING_DR();
}

void
check_return_too_shallow(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
                         volatile reg_t reg_edi, volatile reg_t reg_esi,
                         volatile reg_t reg_ebp, volatile reg_t reg_esp,
                         volatile reg_t reg_ebx, volatile reg_t reg_edx,
                         volatile reg_t reg_ecx, volatile reg_t reg_eax)
{
    ENTERING_DR();
#        if USE_LOCAL_MPROT_STACK
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);
#        endif

    LOG(THREAD, LOG_ALL, 3, "check_return_too_shallow\n");
    if (dcontext->call_depth == 0) {
        LOG(THREAD, LOG_ALL, 3, "\tbottomed out of dynamo, ignoring\n");
#        ifdef UNIX
        asm("movl $0, %eax");
        asm("pinsrw $7,%eax,%xmm7");
#        else
#            error NYI
#        endif
        /* we set ebx so that check will succeed */
        reg_ebx = (reg_edx & 0x0000ffff);
    } else {
        /* restore 0..31 from memory */
        call_stack_32_t *stack = dcontext->call_stack;
        ASSERT(stack != NULL);

        reg_ebx = (stack->retaddr[0][1] << 8) | (stack->retaddr[0][0]);
        LOG(THREAD, LOG_ALL, 3, "\tsetting reg_ebx to stored retaddr " PFX "\n", reg_ebx);

        /* move back into registers */
#        ifdef UNIX
        /* gcc 4.0 doesn't like:  "m"(stack->retaddr) */
        void *retaddr = stack->retaddr;
        asm("movl %0, %%eax" : : "m"(retaddr));
        /* off by one to get 1..31 into slots 0..30 */
        asm("movdqu 0x02(%eax), %xmm0");
        asm("movdqu 0x12(%eax), %xmm1");
        asm("movdqu 0x22(%eax), %xmm2");
        asm("movdqu 0x32(%eax), %xmm3");
        asm("movl $31, %eax");
        asm("pinsrw $7,%eax,%xmm7");
#        else
#            error NYI
#        endif
#        ifdef DEBUG
        if (d_r_stats->loglevel >= 3) {
            int i, j;
            LOG(THREAD, LOG_ALL, 3, "Restored:\n");
            for (i = 0; i < 4; i++) {
                for (j = 0; j < 8; j++) {
                    LOG(THREAD, LOG_ALL, 3, "\t%d %d 0x%02x%02x", i, j,
                        stack->retaddr[i * 8 + j][1], stack->retaddr[i * 8 + j][0]);
                    if (j % 4 == 3)
                        LOG(THREAD, LOG_ALL, 3, "\n");
                }
            }
        }
#        endif
        stack = stack->next;
#        if USE_LOCAL_MPROT_STACK
        heap_free(dcontext, dcontext->call_stack, sizeof(call_stack_32_t));
#        else
        global_heap_free(dcontext->call_stack,
                         sizeof(call_stack_32_t) HEAPACCT(ACCT_OTHER));
#        endif
        dcontext->call_stack = stack;
        dcontext->call_depth--;
        LOG(THREAD, LOG_ALL, 3, "\tdone, call depth is now %d\n", dcontext->call_depth);
    }

#        if USE_LOCAL_MPROT_STACK
    SELF_PROTECT_LOCAL(dcontext, READONLY);
#        endif
    EXITING_DR();
}

void
check_return_ra_mangled(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
                        volatile reg_t reg_edi, volatile reg_t reg_esi,
                        volatile reg_t reg_ebp, volatile reg_t reg_esp,
                        volatile reg_t reg_ebx, volatile reg_t reg_edx,
                        volatile reg_t reg_ecx, volatile reg_t reg_eax)
{
    /* ebx had addr, then we did ebx = ~ebx */
    int stored_addr = ~reg_ebx;

    ENTERING_DR();
    SELF_PROTECT_LOCAL(dcontext, WRITABLE);

#        ifdef DEBUG
    if (d_r_stats->loglevel >= 3) {
        int idx, i, j;
        byte xmm[8][16]; /* each sse2 is 128 bits = 16 bytes */
        /* move from registers into memory where we can work with it */
#            ifdef UNIX
        asm("movdqu %%xmm0, %0" : "=m"(xmm[0]));
        asm("movdqu %%xmm1, %0" : "=m"(xmm[1]));
        asm("movdqu %%xmm2, %0" : "=m"(xmm[2]));
        asm("movdqu %%xmm3, %0" : "=m"(xmm[3]));
        asm("movdqu %%xmm4, %0" : "=m"(xmm[4]));
        asm("movdqu %%xmm5, %0" : "=m"(xmm[5]));
        asm("movdqu %%xmm6, %0" : "=m"(xmm[6]));
        asm("movdqu %%xmm7, %0" : "=m"(xmm[7]));
#            else
#                error NYI
#            endif
        LOG(THREAD, LOG_ALL, 3, "on our stack:\n");
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                LOG(THREAD, LOG_ALL, 3, "\t%d %d 0x%02x%02x", i, j, xmm[i][j * 2 + 1],
                    xmm[i][j * 2]);
                if (j % 4 == 3)
                    LOG(THREAD, LOG_ALL, 3, "\n");
            }
        }

#            ifdef UNIX
        asm("pextrw $7,%xmm7,%eax");
        asm("movl %%eax, %0" : "=m"(idx));
#            else
#                error NYI
#            endif
        LOG(THREAD, LOG_ALL, 3,
            "check_return_ra_mangled: stored=" PFX " vs real=" PFX ", idx=%d\n",
            stored_addr, reg_edx, idx);
    }
#        endif
    SYSLOG_INTERNAL_ERROR(
        "ERROR: return address was mangled (bottom 16 bits: 0x%04x => 0x%04x)",
        (reg_edx & 0x0000ffff), stored_addr);
    ASSERT_NOT_REACHED();

    SELF_PROTECT_LOCAL(dcontext, READONLY);
    EXITING_DR();
}

#    endif /* !SSE2_USE_STACK_POINTER */
/*################################################################################*/

#endif /* CHECK_RETURNS_SSE2 */

#ifdef RETURN_AFTER_CALL
/* Return instructions are allowed to target only instructions immediately following  */
/* a call instruction that has already been executed. */

static void
add_call_site(dcontext_t *dcontext, app_pc target_pc, bool direct)
{
    /* TODO: should be part of vm_area_t to allow flushing  */
    fragment_add_after_call(dcontext, target_pc);
}

/* return 0 if not found */
static int
find_call_site(dcontext_t *dcontext, app_pc target_pc)
{
    if (fragment_after_call_lookup(dcontext, target_pc) != NULL)
        return 1;
    else
        return 0; /* not found */
}

/* check only the table */
bool
is_observed_call_site(dcontext_t *dcontext, app_pc retaddr)
{
    return (find_call_site(dcontext, retaddr) != 0);
}

static int INLINE_ONCE
start_enforcing(dcontext_t *dcontext, app_pc target_pc)
{
    static int start_enforcing =
        0; /* FIXME: should be thread local. this will handle vfork */
    int at_bottom;

    LOG(THREAD, LOG_INTERP, 3, "RCT: start_enforcing = %d\n", start_enforcing);

    if (start_enforcing)
        return 1;

    at_bottom = at_initial_stack_bottom(dcontext, target_pc);
    if (!at_bottom) {
        LOG(THREAD, LOG_INTERP, 1, "RCT: no bottom - start enforcing now\n");
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        start_enforcing = 1;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        return 1;
    }

    /* FIXME: we reach the stack bottom on Windows quite late at
       fragment_t 2768, tag 0x77f9fb67 <ntdll.dll~KiUserApcDispatcher+0x7>
       can we do better?
       All other threads running at that time will ignore attacks.
       FIXME: therefore start_enforcing should be thread local
    */

    if (at_bottom == 1) {
        LOG(THREAD, LOG_INTERP, 1, "RCT: seen bottom - start enforcing after this \n");
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        start_enforcing = 1;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        return 0; /* let this last one through */
    }

    return 0; /* do not enforce yet */
}

void
add_return_target(dcontext_t *dcontext, app_pc instr_pc, instr_t *instr)
{
    bool direct = instr_is_call_direct(instr);
    app_pc after_call_pc = instr_pc + instr_length(dcontext, instr);
    /* CHECK: is this always faster than decode_next_pc(dcontext, instr_pc) */
    add_call_site(dcontext, after_call_pc, direct);
    STATS_INC(ret_after_call_added);

    DOLOG(1, LOG_INTERP, {
        if (direct) {
            LOG(THREAD, LOG_INTERP, 3,
                "RCT: call at " PFX "\tafter_call=" PFX "\ttarget=" PFX "\n", instr_pc,
                after_call_pc, opnd_get_pc(instr_get_target(instr)));
        } else {
            /* of course, while building a basic block we can't tell the indirect call
             * target */
            LOG(THREAD, LOG_INTERP, 3, "RCT: ind call at " PFX "\tafter_call=" PFX "\n",
                instr_pc, after_call_pc);
        }
    });
}

#    ifdef DIRECT_CALL_CHECK
#        warning not yet implemented
/* Further restrict return to existing code, to only target indirect after call sites,
   since direct calls have known return targets.  Usually compilers generate only a single
   RET instruction, but if we cannot count on that (i.e. assembly hacks),
   then this check will also have false positives
*/
/* This reverse check of (call 1->1 return) can be implemented relatively efficiently:
   we have to have _all_ return lookups actually check if the stored tag is of a
   direct call (which should be the common case so check can be made on miss path).
   If target is indeed a direct call then they compare themselves with the stored value,
   [unless first call in which case the valid value is yet unknown]

   Note that we have a many-to-one relationship of (calls *->1 return)
   and also a 1-to-many for (ind call 1->* returns).
*/

unsigned first_ret_from[MAX_CALL_CNT]; /* the first registered return */

enum {
    RETURN_FROM_EXPECTED_CALLEE = 1, /* all good */
    RETURN_FOR_FIRST_TIME = 2,       /* probably good,
                                        as long as no one corrupted it before first use.
                                        unfortunately, for attacks on uncommon paths
                                        this protection doesn't add much
                                     */
    RETURN_UNKNOWN_CALLEE = -1
};

/* return >0 if ok */
int
reverse_check_ret_source(app_pc target_pc, app_pc source_pc)
{
    uint call_site_ndx = find_call_site(dcontext, target_pc);
    ASSERT_NOT_TESTED();
    ASSERT(call_site_ndx < MAX_CALL_CNT);
    if (first_ret_from[call_site_ndx] == source_pc)
        return RETURN_FROM_EXPECTED_CALLEE; /* all good */
    if (!first_ret_from[call_site_ndx]) {   /* never returned to */
        /* assigning first callee */
        first_ret_from[call_site_ndx] = source_pc;
        return RETURN_FOR_FIRST_TIME;
    } else {
        /* direct call returned to from a different address than last time */
        return 0; /*  mismatch - possible RA corruption */
    }
}
#    endif /* DIRECT_CALL_CHECK */

static bool
at_iret_exception(dcontext_t *dcontext, app_pc target_pc, app_pc source_pc)
{
#    if defined(X64) && defined(WINDOWS)
    /* Check if this ntdll64!RtlRestoreContext's iret.  While my instance
     * of ntdll64 has RtlRestoreContext as straight-line code, it could
     * easily be split up in the future, so we only check for an iret
     * being in ntdll itself.
     */
    bool res = false;
    instrlist_t *ilist = build_app_bb_ilist(dcontext, source_pc, INVALID_FILE);
    instr_t *iret = instrlist_last(ilist);
    instr_t *ipush = (iret != NULL) ? instr_get_prev(iret) : NULL;

    if (get_module_base(source_pc) == get_ntdll_base()) {
        /* We could check that this bb starts w/ fxrstor but rather than be
         * too fragile I'm allowing any iret inside ntdll */
        if (instr_get_opcode(instrlist_last(ilist)) == OP_iret) {
            SYSLOG_INTERNAL_WARNING_ONCE("RCT: iret matched @" PFX, source_pc);
            res = true;
        }
    }
    instrlist_clear_and_destroy(dcontext, ilist);
    /* case 9398: build_app_bb_ilist modified last decode page, so restore here */
    set_thread_decode_page_start(dcontext, (app_pc)PAGE_START(target_pc));
    return res;
#    else
    return false;
#    endif
}

/* similar to vbjmp, though here we have a push of a register */
static bool
at_pushregret_exception(dcontext_t *dcontext, app_pc target_pc, app_pc source_pc)
{
    /* Check if this a "push reg; ret" seen in mscoree (case 7317):
     *   push reg
     *   ret
     */
    bool res = false;

    instrlist_t *ilist = build_app_bb_ilist(dcontext, source_pc, INVALID_FILE);
    instr_t *iret = instrlist_last(ilist);
    instr_t *ipush = (iret != NULL) ? instr_get_prev(iret) : NULL;

    if (ipush != NULL && instr_get_opcode(ipush) == OP_push &&
        opnd_is_reg(instr_get_src(ipush, 0)) && instr_is_return(iret) &&
        instr_num_srcs(iret) == 2 /* no ret immed */) {
        /* sanity check: is reg value the ret target? */
        reg_id_t reg = opnd_get_reg(instr_get_src(ipush, 0));
        reg_t val = reg_get_value_priv(reg, get_mcontext(dcontext));
        LOG(GLOBAL, LOG_INTERP, 3,
            "RCT: at_pushregret_exception: push %d reg == " PFX "; ret\n", reg, val);
        if ((app_pc)val == target_pc) {
            SYSLOG_INTERNAL_WARNING_ONCE("RCT: push reg/ret matched @" PFX, target_pc);
            res = true;
        }
    }
    instrlist_clear_and_destroy(dcontext, ilist);
    /* case 9398: build_app_bb_ilist modified last decode page, so restore here */
    set_thread_decode_page_start(dcontext, (app_pc)PAGE_START(target_pc));
    return res;
}

static bool
at_vbjmp_exception(dcontext_t *dcontext, app_pc target_pc, app_pc source_pc)
{
    /* Verify if this a VB generated push/ret, where the immediate in the push has
       the target_pc as an immediate
     * (this is seen in winword (case 670)):
     *   push target-address
     *   ret
     */
    bool res = false;

    instrlist_t *ilist = build_app_bb_ilist(dcontext, source_pc, INVALID_FILE);
    instr_t *iret = instrlist_last(ilist);
    instr_t *ipush = iret ? instr_get_prev(iret) : NULL;

    /* FIXME: need to restrict to only two instructions */
    if (ipush && instr_get_opcode(ipush) == OP_push_imm && instr_is_return(iret) &&
        opnd_get_size(instr_get_src(ipush, 0)) == OPSZ_4) {
        ptr_uint_t immed = (ptr_uint_t)opnd_get_immed_int(instr_get_src(ipush, 0));
        IF_X64(ASSERT_TRUNCATE(immed, uint, opnd_get_immed_int(instr_get_src(ipush, 0))));
        LOG(GLOBAL, LOG_INTERP, 3,
            "RCT: at_vbjmp_exception: testing target " PFX " for push $" PFX
            "; ret pattern\n",
            target_pc, immed);
        if ((app_pc)immed == target_pc) {
            SYSLOG_INTERNAL_WARNING_ONCE("RCT: push/ret matched @" PFX, target_pc);
            res = true;
        }
    }
    instrlist_clear_and_destroy(dcontext, ilist);
    /* case 9398: build_app_bb_ilist modified last decode page, so restore here */
    set_thread_decode_page_start(dcontext, (app_pc)PAGE_START(target_pc));
    return res;
}

static bool
at_vbpop_exception(dcontext_t *dcontext, app_pc target_pc, app_pc source_pc)
{
    /* Verify if this a VB generated sequence where RETurn just goes to the next
     instruction.
     * (this is seen in FMStocks_Bus.dll and FMStocks_Bus.dll (case 1718))
     * The functions called seem generic enough, to allow for another pattern on this.
     * All we're checking for now is (source_pc + 1) == target_pc.
     110045E0                 call    ebx ; __vbaStrMove
     110045E2                 push    offset loc_1100462A
     110045E7                 jmp     short loc_11004620

     11004620 loc_11004620:  ; CODE XREF: sub_11004510+D7
     11004620                 lea     ecx, [ebp+var_20]
     11004623                 call    ds:__vbaFreeStr
     11004629                 retn
     1100462A loc_1100462A:  ; DATA XREF: sub_11004510+D2
     1100462A                 mov     ecx, [ebp-14h]
     */
    /* FIXME: make this part of at_vbjmp_exception() */
    /* FIXME: also see security-common/vbjmp-rac-test.c and why
       we may end up having to treat specially a "push $code; jmp "
       for a slightly more general handling of this.
    */

    /* We assume that the RET instruction is a single one and is in
       its own basic block, so we expect it to be at source_pc.
       FIXME: If it doesn't works this way, we'll have
       to build a basic block like at_vbjmp_exception() does. */
    /* FIXME: What if the source_pc is a trace, then we'd need to find
       the exiting branch and make sure it matches? */
    if ((source_pc + 1) == target_pc) {
        LOG(THREAD, LOG_INTERP, 2,
            "RCT: at_vbpop_exception; matched ret " PFX " to next " PFX " pattern\n",
            source_pc, target_pc);
        SYSLOG_INTERNAL_WARNING_ONCE("RCT: ret/next matched @" PFX, source_pc, target_pc);
        return true;
    }
    return false;
}

static bool
at_mso_rct_exception(dcontext_t *dcontext, app_pc target_pc)
{
    /* winlogon.exe (case 1214) and mso.dll (case 1158) in Office 10
       (from Winstone 2002) appear to have a very weird code that for
       many function calls modifies the return address on the stack so
       that it skips several bytes to reach the real instruction.

       The purpose of that code is not yet grokked.  I have not
       identified if that is supposed to be an exception handling
       mechanism or another language construct, in any case, it breaks the ABI.

       There are ~500 places in winlogon.exe that have this pattern:
       jmp *(fptr)
       pushfd
       pushad
       push args dwords
       call <some func>    ; there are only 10 responsible for the 500 call sites in
winlogon ac:                    ; after call instruction yet return targets realac sub
esp, 0x400+ function of args popad popfd <a few instrs usually pop's or push'es, or add
esp> realac: ; actual return target add esp, 4*args lea ebx, &after mov [fptr], ebx popad
       popfd
    after:

       37 places in mso.dll dll also match this pattern:

       30bf58f9 e87f80f1ff       call    MSO+0xd97d (30b0d97d)  ; two locations here
       -> this is the pushed real after call address
I am not sure if this is supposed to be data or instructions for something
       30bf58fe 81ec14040000     sub     esp,?0x414? ; immediate varies
      [30bf5904 83c408           add     esp,0x8 ]; this "instruction" not always here...
       30bf5907 61               popad
       30bf5908 9d               popfd
       30bf5909 5d               pop     ebp
       -> this is the address where the above call returns to
       30bf590a 83c428           add     esp,?0x28?  ; varies

       The way the callees are passed the offset to the realac is
       unclear - after several unexplainable hoops of adding known
       constants to the return address on the stack they end up there.

       What we do when a RAC fails:

        I prefer not building a basic block for suspect attacker
        controlled data - keep in mind we do this check before we check
        code origins, therefore we'll match raw bytes.

        is_readable_without_exception(target_pc, 17)

        For now I'll go with this pattern match on the target_pc:
        ; this cleans up the arguments to the call so it shouldn't be huge (I've seen 52)
        83 c4   at the target_pc,   add esp, 0xbyte
        ; the address they are loading in here is at the end of the code block!
        8d 1d [target_pc+1]  at target_pc+3      lea ebx, target_pc+17
        ; In case we start doing something about indirect jumps - we can keep
        ; the information about the indirect jump targeting target_pc+17
        ; We don't match the next line's pattern
        89 1d   at target_pc+9      mov [?addr32], ebx
        61      at target_pc+15     popad
        9d      at target_pc+16     popfd
        target_pc+17:

        We should then check that there is a valid after call site in
        the 32 bytes preceding the target_pc (I've seen 16 but giving some margin).
        This will make it a little stricter in order to keep it
        independent of code origins - so that attackers can't point us
        to random code with the above prefix.

    */
    enum {
        MSO_PATTERN_SIZE = 17,
        MSO_PATTERN_ADD_ESP = 0xc483,
        MSO_PATTERN_LEA_EBX_OFFSET = 3,
        MSO_PATTERN_LEA_EBX = 0x1d8d,
        MSO_PATTERN_LEA_EBX_DISP_OFFSET = 2 + MSO_PATTERN_LEA_EBX_OFFSET,
        MSO_PATTERN_POPAD_POPFD_OFFSET = 15,
        MSO_PATTERN_POPAD_POPFD = 0x9d61,
        MSO_PATTERN_MAX_AC_OFFSET = 32
    };

    if (!is_readable_without_exception(target_pc, MSO_PATTERN_SIZE))
        return false;

    LOG(GLOBAL, LOG_INTERP, 3, "RCT: at_mso_rct_exception(" PFX ")\n", target_pc);

#    ifdef X64
    /* let's wait until we hit this so we know what the new pattern
     * looks like */
    return false;
#    endif

    if ((*(uint *)(target_pc + MSO_PATTERN_LEA_EBX_DISP_OFFSET) ==
         (uint)(ptr_uint_t)(target_pc + MSO_PATTERN_SIZE)) &&
        (*(ushort *)(target_pc + MSO_PATTERN_LEA_EBX_OFFSET) == MSO_PATTERN_LEA_EBX) &&
        (*(ushort *)(target_pc + MSO_PATTERN_POPAD_POPFD_OFFSET) ==
         MSO_PATTERN_POPAD_POPFD) &&
        *(ushort *)(target_pc) == MSO_PATTERN_ADD_ESP) {
        uint fromac;

        LOG(GLOBAL, LOG_INTERP, 2,
            "RCT: at_mso_rct_exception(" PFX "): pattern matched, "
            "testing if after call\n",
            target_pc);

        for (fromac = 0; fromac < MSO_PATTERN_MAX_AC_OFFSET; fromac++) {
            if (find_call_site(dcontext, target_pc - fromac)) {
                SYSLOG_INTERNAL_WARNING_ONCE("RCT: mso rct matched @" PFX, target_pc);

                LOG(GLOBAL, LOG_INTERP, 2,
                    "RCT: at_mso_rct_exception(" PFX "): "
                    "pattern matched %d after real after call site",
                    target_pc, fromac);

                /* CHECK: in case we see many of these exceptions at the same target
                   then we should add this target_pc as a valid after_call_site
                   so we don't have to match it in the future
                */
                return true;
            }
        }
    }

    return false;
}

/* licdll.dll (case 1690) Licensing agent
   that is used by Automatic updates has several RCT violations.

   I have no idea why are they breaking the API again - only
   possible explanation is for some sort of obfuscation.  I
   wouldn't be surprised if debugging this changes its behaviour.


   licdll.dll from XP SP1
   55a6877d 8d1de9813b60     lea     ebx,[603b81e9]         ; EBX=603b81e9
   55a68783 83ec1c           sub     esp,0x1c
   55a68786 891c24           mov     [esp],ebx              ; [ESP] = 0x603b81e9
   ...
   55a687a5 812c2428fa940a   sub     dword ptr [esp],0xa94fa28
   ; [ESP] = 0x603b81e9 - 0xa94fa28 = 0x55a687c1!
   ...
   55a687bc e90a230000       jmp     licdll!Ordinal221+0xaacb (55a6aacb)

   [1] BAD TARGET
   55a687c1 8b542424         mov     edx,[esp+0x24]
   55a687c5 8b0c24           mov     ecx,[esp]
   55a687c8 891424           mov     [esp],edx
   55a687cb 894c2424         mov     [esp+0x24],ecx
   55a687cf 9d               popfd
   55a687d0 61               popad
   55a687d1 c3               ret
   [2] BAD SOURCE - the ret of this same fragment is then targeting a piece of DGC
   on two freshly created pages.

   Exactly the same code appears at 0x55a65446 in the same dll on xp.
   in the win2003 version of it 0x62FB5478 and 0x62FB87AE have the same fragment.

   FIXME: Of the three different SUB [esp] offsets 0A94FA28h, 0C98F744h, and 0EEF3E64h,
   the latter two exhibit different potential target patterns.  Need to test those.

   What we do when a RAC fails, see comments in
   at_mso_rct_exception() on why we match raw bytes:

   Note that this same fragment is both a source and a target, so
   it may be worthwhile matching it as close as possible.

   1) FIXME: check if source fragment is in module licdll.dll

   2) In case the target is a future executable then we don't look at
   the target but instead look at the source in the next step.

   3) is_readable_without_exception(target_pc, 17)

   4) Pattern match:
   8b 54 24 24 pc        mov edx,[esp+0x24]
   24          pc+13     mov [esp+0x24], ecx ; 89 4c 24 24]
   9d          pc+14     popfd
   61          pc+15     popad
   c3          pc+16     ret
   pc+17:
*/

static bool
licdll_pattern_match(dcontext_t *dcontext, app_pc pattern_pc)
{
    enum {
        LICDLL_PATTERN_SIZE = 17,
        LICDLL_PATTERN_MOV_EDX_ESP_24 = 0x2424548b,
        LICDLL_PATTERN_24_POPFD_OFFSET = 13,
        LICDLL_PATTERN_24_POPFD_POPAD_RET = 0xc3619d24
    };

    if (!is_readable_without_exception(pattern_pc, LICDLL_PATTERN_SIZE))
        return false;

    LOG(THREAD, LOG_INTERP, 2, "RCT: at_licdll_rct_exception(" PFX ")\n", pattern_pc);

#    ifdef X64
    /* let's wait until we hit this so we know what the new pattern
     * looks like */
    return false;
#    endif

    if ((*(uint *)(pattern_pc + LICDLL_PATTERN_24_POPFD_OFFSET) ==
         LICDLL_PATTERN_24_POPFD_POPAD_RET) &&
        (*(uint *)(pattern_pc) == LICDLL_PATTERN_MOV_EDX_ESP_24)) {

        LOG(THREAD, LOG_INTERP, 1,
            "RCT: at_licdll_rct_exception(" PFX "): pattern matched\n", pattern_pc);

        return true;
    }
    return false;
}

static bool
at_licdll_rct_exception(dcontext_t *dcontext, app_pc target_pc, app_pc source_pc)
{

    /* 1) FIXME: check if source fragment is in module licdll.dll
       we could do that with get_module_short_name(source_pc),
       but it looks like both licdll and dpcdll need this */

    /* 2) FIXME: In case the target is a future executable then we
       don't look at the target but instead look at the source in the next step. */

    /* CHECK: in case we see many of these exceptions at the same target
       then we should add this target_pc as a valid after_call_site
       so we don't have to match it in the future
    */

    if (licdll_pattern_match(dcontext, target_pc)) {
        SYSLOG_INTERNAL_WARNING_ONCE("RCT: licdll rct matched target @" PFX, target_pc);
        return true;
    }
    /* case 9398: set to source for our check
     * FIXME: we could read off the end of the page!  Should use TRY or safe_read.
     */
    ASSERT(check_in_last_thread_vm_area(dcontext, (app_pc)PAGE_START(target_pc)));
    set_thread_decode_page_start(dcontext, (app_pc)PAGE_START(source_pc));
    /* the same piece of code then is then RETurning into some DGC */
    if (licdll_pattern_match(dcontext, source_pc)) {
        SYSLOG_INTERNAL_WARNING_ONCE("RCT: licdll rct matched source @" PFX, source_pc);
        /* We assume any match will abort future app derefs so we don't need
         * to restore the last decode page */
        return true;
    }
    /* case 9398: now restore (if return true we assume no more derefs) */
    set_thread_decode_page_start(dcontext, (app_pc)PAGE_START(target_pc));

    return false;
}

/* return after call check
   called by d_r_dispatch after inlined return lookup routine has failed */
/* FIXME: return value is ignored */
int
ret_after_call_check(dcontext_t *dcontext, app_pc target_addr, app_pc src_addr)
{
    /* FIXME If we change shared_syscalls to use the ret table (instead of the jmp
     * table), we need to fix up the use of instr_addr further down, since it could
     * store a nonsensical value and cause reverse_check_ret_source() to return a
     * failure code.
     */
#    if defined(DEBUG) || defined(DIRECT_CALL_CHECK)
    cache_pc instr_addr = EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit);
#    endif

    LOG(THREAD, LOG_INTERP, 3, "RCT: return \taddr = " PFX "\ttarget = " PFX "\n",
        instr_addr, target_addr);

    STATS_INC(ret_after_call_validations);

    /* FIXME: currently this is only a partial check,
       a trace lookup will not exit the fcache for a check like this
       to fully provide the return-after-call guarantee.

       [Note that there is an ibl even in basic blocks and currently
       those simply look for any trace, the next step is to restrict
       the return hashtable only to valid "after call" targets]

       Yet false positives with this simpler check
       would be something to get worried about already.
    */

    /* TODO: write a unit test that forms a trace and then modifies
       the return address to show this needs to be done from within */

    /* Case 9398: handle unreadable races from derefs in checks below.
     * Any checks that read src must set back to target.
     * FIXME: better to use TRY, or safe_read for each?  if use TRY
     * then have to make sure to call bb_build_abort() if necessary,
     * since TRY fault takes precedence over decode fault.
     * FIXME: we could read off the end of the page!  This is just a quick fix,
     * not foolproof.
     */
    set_thread_decode_page_start(dcontext, (app_pc)PAGE_START(target_addr));

    if (!find_call_site(dcontext, target_addr)) {
        LOG(THREAD, LOG_INTERP, 1, "RCT: bad return target: " PFX "\n", target_addr);
        if (!start_enforcing(dcontext, target_addr)) {
            /* FIXME to be fixed whenever we figure out how to start first */
            LOG(THREAD, LOG_INTERP, 1, "RCT: haven't started yet --ok\n");
            STATS_INC(ret_after_call_before_start);
            /* do not add exemption */
            return 2;
        }

        /* Now come the known cases of unjustified ugliness from Microsoft apps.
           For regression testing purposes we test for them on all platforms */

        /* FIXME: see case 285 for a better method of obtaining source_pc,
           which for all uses here is assumed to be a bb tag,
           and will likely break if a trace containing these bb's is build.
           Also see case 1858 about storing into RAC table validated targets.
        */
        if (DYNAMO_OPTION(vbpop_rct) &&
            at_vbpop_exception(dcontext, target_addr, src_addr)) {
            LOG(THREAD, LOG_INTERP, 1, "RCT: known exception on VB pop --ok\n");
            STATS_INC(ret_after_call_known_exceptions);
            goto exempted;
        }

        if (DYNAMO_OPTION(vbjmp_allowed) &&
            at_vbjmp_exception(dcontext, target_addr, src_addr)) {
            LOG(THREAD, LOG_INTERP, 1, "RCT: known exception on VB jmp --ok\n");
            STATS_INC(ret_after_call_known_exceptions);
            goto exempted;
        }

        if (DYNAMO_OPTION(mso_rct) && at_mso_rct_exception(dcontext, target_addr)) {
            LOG(THREAD, LOG_INTERP, 1, "RCT: known exception on mso ret --ok\n");
            STATS_INC(ret_after_call_known_exceptions);
            goto exempted;
        }

        if (DYNAMO_OPTION(licdll_rct) &&
            at_licdll_rct_exception(dcontext, target_addr, src_addr)) {
            LOG(THREAD, LOG_INTERP, 1, "RCT: known exception on licdll ret --ok\n");
            STATS_INC(ret_after_call_known_exceptions);
            goto exempted;
        }

        if (DYNAMO_OPTION(pushregret_rct) &&
            at_pushregret_exception(dcontext, target_addr, src_addr)) {
            LOG(THREAD, LOG_INTERP, 1, "RCT: known exception on push reg; ret --ok\n");
            STATS_INC(ret_after_call_known_exceptions);
            STATS_INC(ret_after_call_pushregret);
            /* FIXME: we don't want to cache the target of this pattern
             * as the usage we've seen is once-only.  But, it has also been
             * to DGC-only, which is currently not cached anyway.
             */
            goto exempted;
        }

        if (DYNAMO_OPTION(iret_rct) &&
            at_iret_exception(dcontext, target_addr, src_addr)) {
            LOG(THREAD, LOG_INTERP, 1, "RCT: known exception on iret --ok\n");
            goto exempted;
        }

        /* additional handling for known OS specific exceptions is in
           unix/signal.c (for ld) and
           win32/callback.c (for exempt modules, Win2003 fibers, and SEH)
        */
        if (at_known_exception(dcontext, target_addr, src_addr)) {
            LOG(THREAD, LOG_INTERP, 1, "RCT: known exception --ok\n");
            STATS_INC(ret_after_call_known_exceptions);
            goto exempted;
        }

        LOG(THREAD, LOG_INTERP, 1,
            "RCT: BAD[%d] real problem target=" PFX " src fragment=" PFX "\n",
            GLOBAL_STAT(ret_after_call_violations), target_addr, src_addr);
        STATS_INC(ret_after_call_violations);

        if (DYNAMO_OPTION(unloaded_target_exception) &&
            is_unreadable_or_currently_unloaded_region(target_addr)) {
            /* we know we either had unload in progress, or we're
             * beyond unload, but unlike other violations we want to
             * know the difference between unreadable due to unload,
             * vs other unreadable ranges
             */
            /* if it is currently tracked as unloaded we'll just ignore */
            /* case 9364 - we may prefer to kill a thread when
             * unreadable memory that hasn't been unloaded
             * Alternatively, if throwing an exception is always OK,
             * we could exempt in all cases when we reach this.
             */
            /* We assume that we'll throw an unreadable exception for
             * both unloaded and unreadable memory later.  (Note that
             * we flush the fragments after we flush the RAC during
             * process_mmap(), so there is a small chance that we'll
             * in fact completely allow execution - which is OK since
             * still a possible APP race.)  FIXME: it may be
             * preferable to throw our own exception here, if DLLs
             * are in inconsistent state a lot longer while unloaded
             * under us compared to native, then any execution during
             * unload would be bad.
             */
            /* if we are unreadable, we could be _after_ unload */
            if (is_in_last_unloaded_region(target_addr)) {
                DODEBUG({
                    if (!is_readable_without_exception(target_addr, 4)) {
                        /* if currently unreadable and in last unloaded module
                         * we'd let this through and assume that we'll throw
                         * an exception to the app
                         */
                        LOG(THREAD, LOG_RCT, 1,
                            "RCT: DLL unload in progress, " PFX " --ok\n", target_addr);
                        STATS_INC(num_unloaded_race_during);
                    } else {
                        LOG(THREAD, LOG_RCT, 1,
                            "RCT: target in already unloaded DLL, " PFX " --ok\n",
                            target_addr);
                        STATS_INC(num_unloaded_race_after);
                    }
                });
                /* case 6008 should apply this exemption to unreadable all
                 * unloaded DLLs not only the last one memory execution
                 */

                /* do not add exemption */
                return 3; /* allow, don't throw .C */
            } else {
                /* we probably were just unreadable, bad app or possibly attack,
                 * leaving to rct_ret_unreadable further down
                 */
                /* FIXME: case 6008 there is also a possibility of a
                 * race (that we were during unload at the time we
                 * checked, but since we only keep the last unmap,
                 * another one could have taken place, so we would get
                 * here even if we wanted to exempt.
                 */
                /* fall through */
                ASSERT_NOT_TESTED();
            }
        }

        /* ASLR: check if is in wouldbe region, if so report as failure */
        if (aslr_is_possible_attack(target_addr)) {
            LOG(THREAD, LOG_RCT, 1, "RCT: ASLR: wouldbe a preferred DLL, " PFX " --BAD\n",
                target_addr);
            /* fall through and report */
            ASSERT_NOT_TESTED();
            /* FIXME: case 7017 ASLR_NORMALIZE_ID handling */
            STATS_INC(aslr_rct_ret_wouldbe);
        }

        /* special handling of unreadable memory targets - most likely
         * corrupted app, but could also be an unsuccessful attack
         */
        if (TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ret_unreadable))) {
            if (!is_readable_without_exception(target_addr, 4)) {
                SYSLOG_INTERNAL_WARNING_ONCE("return target " PFX " unreadable",
                                             target_addr);

                /* We will eventually throw an exception unless
                 * security violation handles this differently.
                 * e.g. if OPTION_NO_REPORT|OPTION_BLOCK we may kill a thread
                 */
                /* the current defaults will let all of this through */
                /* FIXME: for now only OPTION_NO_REPORT is supported
                 * by security_violation() and that's all we currently need */
                if (security_violation(dcontext, target_addr, RETURN_TARGET_VIOLATION,
                                       DYNAMO_OPTION(rct_ret_unreadable)) ==
                    RETURN_TARGET_VIOLATION) {
                    /* do not cache unreadable memory target */
                    return -1;
                } else {
                    /* do not cache unreadable memory target */
                    return 1;
                }
            }
        }

        SYSLOG_INTERNAL_WARNING_ONCE("return target " PFX " with no known caller",
                                     target_addr);
        /* does not return in protect mode */
        if (security_violation(dcontext, target_addr, RETURN_TARGET_VIOLATION,
                               OPTION_BLOCK | OPTION_REPORT) == RETURN_TARGET_VIOLATION) {
            /* running in detect mode */
            ASSERT(DYNAMO_OPTION(detect_mode)
                   /* case 9712: client security callback can modify the action.
                    * FIXME: if a client changes the action to ACTION_CONTINUE,
                    * this address will be exempted and we won't complain again.
                    * In the future we may need to add another action type. */
                   || CLIENTS_EXIST());
            /* we'll cache violation target */
            goto exempted;
        } else { /* decided not to throw the violation */
            /* exempted Threat ID */
            /* we'll cache violation target */
            goto exempted;
        }
    exempted:
        /* add target if in a module (code or data section), but not in DGC */
        if (DYNAMO_OPTION(rct_cache_exempt) == RCT_CACHE_EXEMPT_ALL ||
            (DYNAMO_OPTION(rct_cache_exempt) == RCT_CACHE_EXEMPT_MODULES &&
             (get_module_base(target_addr) != NULL))) {
            /* FIXME: extra system calls may be become more expensive
             * than extra exits for simple pattern matches, should
             * have a cheap way of determining whether an address is
             * in a module code section  */

            fragment_add_after_call(dcontext, target_addr);
            ASSERT_CURIOSITY(is_executable_address(target_addr));
            STATS_INC(ret_after_call_exempt_added);
        } else {
        }
        return 1;
    }
#    ifdef DIRECT_CALL_CHECK
    else {
        /* extra check on direct calls */
        /* TODO: verify if target is direct call */
        /* FIXME: make sure that instr_addr gets shifted properly on unit resize
           i.e. considered as a normal fragment address, then this check is ok to use a
           cache_pc */
        if (reverse_check_ret_source(target_addr, instr_addr) < 0) {
            LOG(1, "RCT: bad return source:" PFX " for after call target: " PFX "\n",
                instr_addr, target_addr);
            return -1;
        }
    }
#    endif /* DIRECT_CALL_CHECK */
    LOG(THREAD, LOG_INTERP, 3, "RCT: good return to " PFX "\n", target_addr);
    STATS_INC(ret_after_call_good);

    return 1;
}

#endif /* RETURN_AFTER_CALL */
