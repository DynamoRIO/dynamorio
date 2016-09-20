/* **********************************************************
 * Copyright (c) 2011-2016 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
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

/* Usage: define INCLUDE_NAME to be the ir_*args*.h file full of OPCODE()
 * macros, and define OPCODE_FOR_CREATE() appropriately for that file.
 * Then #include this file.
 * We do this so we can split up the macro expansions into multiple functions,
 * avoiding a problem with VS2010 where it takes up to 25 minutes to compile
 * ir.c (i#827).
 */

    byte *pc, *next_pc;
    byte *end;
    instrlist_t *ilist = instrlist_create(dc);
    instr_t *instr, *orig;

#   define XOPCODE OPCODE
#   define OPCODE(name, opc, icnm, ...) \
    int len_##name;
#   include INCLUDE_NAME
#   undef OPCODE
#   undef XOPCODE

#   define OPCODE OPCODE_FOR_CREATE
#   define XOPCODE XOPCODE_FOR_CREATE
#   include INCLUDE_NAME
#   undef OPCODE
#   undef XOPCODE

    end = instrlist_encode(dc, ilist, buf, false);

    instr = instr_create(dc);
    pc = buf;
    orig = instrlist_first(ilist);

#   define XOPCODE OPCODE
#   define OPCODE(name, opc, icnm, flags, ...) do { \
    if ((flags & IF_X64_ELSE(X86_ONLY, X64_ONLY)) == 0 && len_##name != 0) { \
        instr_reset(dc, instr); \
        next_pc = decode(dc, pc, instr); \
        ASSERT((next_pc - pc) == decode_sizeof(dc, pc, NULL _IF_X64(NULL))); \
        ASSERT((next_pc - pc) == len_##name); \
        ASSERT(instr_get_opcode(instr) == OP_##opc); \
        /* ensure operands all came out the same (xref i#1232) */ \
        ASSERT(instr_same(orig, instr) || \
               instr_num_srcs(orig) > 0 && opnd_is_instr(instr_get_target(orig))); \
        pc = next_pc; \
        orig = instr_get_next(orig); \
        if (orig != NULL && instr_is_label(orig)) \
            orig = instr_get_next(orig); \
    } } while (0);
#   include INCLUDE_NAME
#   undef OPCODE
#   undef XOPCODE

#if VERBOSE
    for (pc = buf; pc < end; )
        pc = disassemble_with_info(dc, pc, STDOUT, true, true);
#endif

    instr_destroy(dc, instr);
    instrlist_clear_and_destroy(dc, ilist);
