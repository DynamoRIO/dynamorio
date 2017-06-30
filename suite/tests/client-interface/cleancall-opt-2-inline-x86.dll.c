/* *******************************************************************************
 * Copyright (c) 2017 ARM Limited. All rights reserved.
 * *******************************************************************************/

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
 * * Neither the name of MIT nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL MIT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test the clean call inliner on X86. */
#include "cleancall-opt-2-inline.dll.h"

/*
callpic_pop:
    push REG_XBP
    mov REG_XBP, REG_XSP
    call Lnext_label
    Lnext_label:
    pop REG_XBX
    leave
    ret
*/
static instrlist_t *
codegen_callpic_pop(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    instr_t *next_label = INSTR_CREATE_label(dc);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(next_label)));
    APP(ilist, next_label);
    APP(ilist, INSTR_CREATE_pop(dc, opnd_create_reg(DR_REG_XBX)));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/*
callpic_mov:
    push REG_XBP
    mov REG_XBP, REG_XSP
    call Lnext_instr_mov
    Lnext_instr_mov:
    mov REG_XBX, [REG_XSP]
    leave
    ret
*/
static instrlist_t *
codegen_callpic_mov(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    instr_t *next_label = INSTR_CREATE_label(dc);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(next_label)));
    APP(ilist, next_label);
    APP(ilist, INSTR_CREATE_mov_ld
        (dc, opnd_create_reg(DR_REG_XBX), OPND_CREATE_MEMPTR(DR_REG_XSP, 0)));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/* Non-leaf functions cannot be inlined.
nonleaf:
    push REG_XBP
    mov REG_XBP, REG_XSP
    call other_func
    leave
    ret
other_func:
    ret
*/
static instrlist_t *
codegen_nonleaf(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    instr_t *other_func = INSTR_CREATE_label(dc);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(other_func)));
    codegen_epilogue(dc, ilist);
    APP(ilist, other_func);
    APP(ilist, INSTR_CREATE_ret(dc));
    return ilist;
}

/* Conditional branches cannot be inlined.  Avoid flags usage to make test case
 * more specific.
cond_br:
    push REG_XBP
    mov REG_XBP, REG_XSP
    mov REG_XCX, ARG1
    jecxz Larg_zero
        mov REG_XAX, HEX(DEADBEEF)
        mov SYMREF(global_count), REG_XAX
    Larg_zero:
    leave
    ret
*/
static instrlist_t *
codegen_cond_br(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    instr_t *arg_zero = INSTR_CREATE_label(dc);
    opnd_t xcx = opnd_create_reg(DR_REG_XCX);
    codegen_prologue(dc, ilist);
    /* If arg1 is non-zero, write 0xDEADBEEF to global_count. */
    APP(ilist, INSTR_CREATE_mov_ld(dc, xcx, codegen_opnd_arg1()));
    APP(ilist, INSTR_CREATE_jecxz(dc, opnd_create_instr(arg_zero)));
    APP(ilist, INSTR_CREATE_mov_imm(dc, xcx, OPND_CREATE_INTPTR(&global_count)));
    APP(ilist, INSTR_CREATE_mov_st(dc, OPND_CREATE_MEMPTR(DR_REG_XCX, 0),
                                   OPND_CREATE_INT32((int)0xDEADBEEF)));
    APP(ilist, arg_zero);
    codegen_epilogue(dc, ilist);
    return ilist;
}

/* A function that uses 2 registers and 1 local variable, which should fill all
 * of the scratch slots that the inliner uses.  This used to clobber the scratch
 * slots exposed to the client.
tls_clobber:
    push REG_XBP
    mov REG_XBP, REG_XSP
    sub REG_XSP, ARG_SZ
    mov REG_XAX, HEX(DEAD)
    mov REG_XDX, HEX(BEEF)
    mov [REG_XSP], REG_XAX
    leave
    ret
*/
static instrlist_t *
codegen_tls_clobber(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t xax = opnd_create_reg(DR_REG_XAX);
    opnd_t xdx = opnd_create_reg(DR_REG_XDX);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_sub
        (dc, opnd_create_reg(DR_REG_XSP), OPND_CREATE_INT8(sizeof(reg_t))));
    APP(ilist, INSTR_CREATE_mov_imm(dc, xax, OPND_CREATE_INT32(0xDEAD)));
    APP(ilist, INSTR_CREATE_mov_imm(dc, xdx, OPND_CREATE_INT32(0xBEEF)));
    APP(ilist, INSTR_CREATE_mov_st(dc, OPND_CREATE_MEMPTR(DR_REG_XSP, 0), xax));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/* Zero the aflags.  Inliner must ensure they are restored.
aflags_clobber:
    push REG_XBP
    mov REG_XBP, REG_XSP
    mov REG_XAX, 0
    add al, HEX(7F)
    sahf
    leave
    ret
*/
static instrlist_t *
codegen_aflags_clobber(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_mov_imm
        (dc, opnd_create_reg(DR_REG_XAX), OPND_CREATE_INTPTR(0)));
    APP(ilist, INSTR_CREATE_add
        (dc, opnd_create_reg(DR_REG_AL), OPND_CREATE_INT8(0x7F)));
    APP(ilist, INSTR_CREATE_sahf(dc));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/* Reduced code from inscount generated by gcc47 -O0.
gcc47_inscount:
#ifdef X64
    push   %rbp
    mov    %rsp,%rbp
    mov    %rdi,-0x8(%rbp)
    mov    global_count(%rip),%rdx
    mov    -0x8(%rbp),%rax
    add    %rdx,%rax
    mov    %rax,global_count(%rip)
    pop    %rbp
    retq
#else
    push   %ebp
    mov    %esp,%ebp
    call   pic_thunk
    add    $0x1c86,%ecx
    mov    global_count(%ecx),%edx
    mov    0x8(%ebp),%eax
    add    %edx,%eax
    mov    %eax,global_count(%ecx)
    pop    %ebp
    ret
pic_thunk:
    mov    (%esp),%ecx
    ret
#endif
*/
static instrlist_t *
codegen_gcc47_inscount(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t global;
    opnd_t xax = opnd_create_reg(DR_REG_XAX);
    opnd_t xdx = opnd_create_reg(DR_REG_XDX);
#ifdef X64
    /* This local is past TOS.  That's OK by the sysv x64 ABI. */
    opnd_t local = OPND_CREATE_MEMPTR(DR_REG_XBP, -(int)sizeof(reg_t));
    codegen_prologue(dc, ilist);
    global = opnd_create_rel_addr(&global_count, OPSZ_PTR);
    APP(ilist, INSTR_CREATE_mov_st(dc, local, codegen_opnd_arg1()));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xdx, global));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xax, local));
    APP(ilist, INSTR_CREATE_add(dc, xax, xdx));
    APP(ilist, INSTR_CREATE_mov_st(dc, global, xax));
    codegen_epilogue(dc, ilist);
#else
    instr_t *pic_thunk = INSTR_CREATE_mov_ld
        (dc, opnd_create_reg(DR_REG_XCX), OPND_CREATE_MEMPTR(DR_REG_XSP, 0));
    codegen_prologue(dc, ilist);
    /* XXX: Do a real 32-bit PIC-style access.  For now we just use an absolute
     * reference since we're 32-bit and everything is reachable.
     */
    global = opnd_create_abs_addr(&global_count, OPSZ_PTR);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(pic_thunk)));
    APP(ilist, INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_XCX),
                                OPND_CREATE_INT32(0x0)));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xdx, global));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xax, codegen_opnd_arg1()));
    APP(ilist, INSTR_CREATE_add(dc, xax, xdx));
    APP(ilist, INSTR_CREATE_mov_st(dc, global, xax));
    codegen_epilogue(dc, ilist);

    APP(ilist, pic_thunk);
    APP(ilist, INSTR_CREATE_ret(dc));
#endif
    return ilist;
}
