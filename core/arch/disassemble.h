/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2008 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001 Hewlett-Packard Company */

/* file "disassemble.h" */

#ifndef DISASSEMBLE_H
#define DISASSEMBLE_H

#include "../globals.h"

#if defined(INTERNAL) || defined(DEBUG) || defined(CLIENT_INTERFACE)

/* for printing to buffers */
#define MAX_OPND_DIS_SZ   64 /* gets long w/ ibl target names */
/* Long examples:
 * "<RAW>  <raw 0x00007f85922c0877-0x00007f85922c0882 == 48 63 f8 48 89 d6 b8 05 00 ...>"
 * "lock cmpxchg %rcx <rel> 0x000007fefd1a2728[8byte] %rax -> <rel> "
 * "0x000007fefd1a2728[8byte] %rax "
 */
#define MAX_INSTR_DIS_SZ 196
/* Here's a pretty long one,
 * "  0x00007f859277d63a  48 83 05 4e 63 21 00 add    $0x0000000000000001 <rel> "
 * "0x00007f8592993990 -> <rel> 0x00007f8592993990 \n                     01 "
 * For ARM:
 * " 8ca90aa1   vstm.hi %s0 %s1 %s2 %s3 %s4 %s5 %s6 %s7 %s8 %s9 %s10 %s11 %s12 %s13 %s14 "
 * "%s15 %s16 %s17 %s18 %s19 %s20 %s21 %s22 %s23 %s24 %s25 %s26 %s27 %s28 %s29 %s30 %s31 "
 * "%r9 -> (%r9)[124byte]"
 */
#define MAX_PC_DIS_SZ    228

/* DR_API EXPORT TOFILE dr_ir_utils.h */
/* DR_API EXPORT BEGIN */
/**
 * Flags controlling disassembly style
 */
typedef enum {
    /**
     * The default: displays all operands, including implicit operands.
     * Lists source operands first, then "->", and then destination
     * operands.
     */
    DR_DISASM_DR             =  0x0,
    /**
     * Requests Intel syntax for disassembly.  This sets the same option that is
     * controlled by the runtime option \p -syntax_intel.  Implicit operands
     * are not displayed.
     */
    DR_DISASM_INTEL          =  0x1,
    /**
     * Requests AT&T syntax for disassembly.  This sets the same option that is
     * controlled by the runtime option \p -syntax_att.  Implicit operands
     * are not displayed.
     */
    DR_DISASM_ATT            =  0x2,
    /**
     * Certain reserved or unspecified opcodes are in a gray area where they
     * could be decoded with their length and operands understood, but they are
     * not fully defined and in fact they may raise an illegal instruction fault
     * when executed.  By default, DR does not treat them as invalid.  If this
     * option is set, DR tightens up its decoding and does treat them as
     * invalid.
     */
    DR_DISASM_STRICT_INVALID =  0x4,
    /**
     * This flag only applies to the default DR style (i.e., it does not apply
     * when DR_DISASM_INTEL or DR_DISASM_ATT is selected).  That style by
     * default displays the size of memory or sub-register operands via a
     * suffix "[Nbytes]".  Setting this flag removes that suffix.
     */
    DR_DISASM_NO_OPND_SIZE   =  0x8,
    /**
     * Requests standard ARM assembler syntax for disassembly.  This
     * sets the same option that is controlled by the runtime option
     * \p -syntax_arm.  Implicit operands are not displayed.
     */
    DR_DISASM_ARM            =  0x10,
} dr_disasm_flags_t;
/* DR_API EXPORT END */

void
disassemble_options_init(void);

DR_API
/**
 * Sets the disassembly style and decoding options.
 * The default is to use DR's custom syntax, unless one of the \ref op_syntax_intel
 * "-syntax_intel", \ref op_syntax_att "-syntax_att", or \ref op_syntax_arm
 * "-syntax_arm" runtime options is specified.
 */
void
disassemble_set_syntax(dr_disasm_flags_t flags);

DR_API
/**
 * Decodes and then prints the instruction at address \p pc to file \p outfile.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 * Returns the address of the subsequent instruction, or NULL if the instruction
 * at \p pc is invalid.
 */
byte *
disassemble(dcontext_t *dcontext, byte *pc, file_t outfile);

DR_UNS_API /* deprecated from interface */
/**
 * Decodes and then prints the instruction at address \p pc to file \p outfile.
 * Prior to the instruction the address and raw bytes of the instruction
 * are printed.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 * Returns the address of the subsequent instruction, or a guess if the instruction
 * at \p pc is invalid.
 */
byte *
disassemble_with_bytes(dcontext_t *dcontext, byte *pc, file_t outfile);

DR_API
/**
 * Decodes and then prints the instruction at address \p pc to file \p outfile.
 * Prior to the instruction the address is printed if \p show_pc and the raw
 * bytes are printed if \p show_bytes.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 * Returns the address of the subsequent instruction, or NULL if the instruction
 * at \p pc is invalid.
 */
byte *
disassemble_with_info(dcontext_t *dcontext, byte *pc, file_t outfile,
                      bool show_pc, bool show_bytes);

DR_API
/**
 * Decodes the instruction at address \p copy_pc as though
 * it were located at address \p orig_pc, and then prints the
 * instruction to file \p outfile.
 * Prior to the instruction the address \p orig_pc is printed if \p show_pc and the raw
 * bytes are printed if \p show_bytes.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 * Returns the address of the subsequent instruction after the copy at
 * \p copy_pc, or NULL if the instruction at \p copy_pc is invalid.
 */
byte *
disassemble_from_copy(dcontext_t *dcontext, byte *copy_pc, byte *orig_pc,
                      file_t outfile, bool show_pc, bool show_bytes);

DR_API
/**
 * Decodes the instruction at address \p pc as though
 * it were located at address \p orig_pc, and then prints the
 * instruction to the buffer \p buf.
 * Always null-terminates, and will not print more than \p bufsz characters,
 * which includes the final null character.
 * Indicates the number of characters printed, not including the final null,
 * in \p printed, if \p printed is non-NULL.
 *
 * Prior to the instruction the address \p orig_pc is printed if \p show_pc and the raw
 * bytes are printed if \p show_bytes.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 * Returns the address of the subsequent instruction after the copy at
 * \p copy_pc, or NULL if the instruction at \p copy_pc is invalid.
 */
byte *
disassemble_to_buffer(dcontext_t *dcontext, byte *pc, byte *orig_pc,
                      bool show_pc, bool show_bytes, char *buf, size_t bufsz,
                      int *printed OUT);


/* DR_API EXPORT TOFILE dr_ir_instr.h */
DR_API
/**
 * Prints the instruction \p instr to file \p outfile.
 * Does not print address-size or data-size prefixes for other than
 * just-decoded instrs, and does not check that the instruction has a
 * valid encoding.  Prints each operand with leading zeros indicating
 * the size.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 */
void
instr_disassemble(dcontext_t *dcontext, instr_t *instr, file_t outfile);

DR_API
/**
 * Prints the instruction \p instr to the buffer \p buf.
 * Always null-terminates, and will not print more than \p bufsz characters,
 * which includes the final null character.
 * Returns the number of characters printed, not including the final null.
 *
 * Does not print address-size or data-size prefixes for other than
 * just-decoded instrs, and does not check that the instruction has a
 * valid encoding.  Prints each operand with leading zeros indicating
 * the size.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 */
size_t
instr_disassemble_to_buffer(dcontext_t *dcontext, instr_t *instr,
                            char *buf, size_t bufsz);

/* DR_API EXPORT TOFILE dr_ir_opnd.h */
DR_API
/**
 * Prints the operand \p opnd to file \p outfile.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 */
void
opnd_disassemble(dcontext_t *dcontext, opnd_t opnd, file_t outfile);

DR_API
/**
 * Prints the operand \p opnd to the buffer \p buf.
 * Always null-terminates, and will not print more than \p bufsz characters,
 * which includes the final null character.
 * Returns the number of characters printed, not including the final null.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 */
size_t
opnd_disassemble_to_buffer(dcontext_t *dcontext, opnd_t opnd, char *buf, size_t bufsz);

/* DR_API EXPORT TOFILE dr_ir_instrlist.h */
DR_API
/**
 * Prints each instruction in \p ilist in sequence to \p outfile.
 * The default is to use DR's custom syntax (see disassemble_set_syntax())
 * with additional information.  The first column contains the offset
 * in bytes from the start of the list.
 * Next, each instruction is labeled according to its type, which
 * will typically either be \p L3 for an unchanged application instruction
 * or \p m4 for a tool instruction (the names come from "Level 3" and
 * "meta Level 4", IR details which are no longer exposed to tools).
 * Tool instructions have their IR heap addresses included (indicated with a
 * leading @ character) to make instruction jump targets easier to
 * identify.  The final two columns contain the raw bytes and the
 * actual instruction disassembly.
 *
 * Below is an example where many tool instructions have been inserted around
 * 3 application instructions, which can be identified by the \p L3 in the 2nd
 * column.  The label instructions are referred to by branch and store
 * instructions, as can be seen by searching for the addresses of the labels.
 * \code
 * TAG  0xf77576e6
 *  +0    m4 @0xe7856eb4  64 89 0d 60 00 00 00 mov    %ecx -> %fs:0x00000060[4byte]
 *  +7    m4 @0xe78574a8  64 8a 0d 52 00 00 00 mov    %fs:0x00000052[1byte] -> %cl
 *  +14   m4 @0xe7855ad4  64 88 0d 54 00 00 00 mov    %cl -> %fs:0x00000054[1byte]
 *  +21   L3              83 ee 06             sub    $0x00000006 %esi -> %esi
 *  +24   m4 @0xe77c3acc  64 80 3d 52 00 00 00 cmp    %fs:0x00000052[1byte] $0x00
 *                        00
 *  +32   m4 @0xe7855c54  75 fe                jnz    @0xe7856054[4byte]
 *  +34   m4 @0xe7856e28  64 c6 05 54 00 00 00 mov    $0x00 -> %fs:0x00000054[1byte]
 *                        00
 *  +42   m4 @0xe7856754  eb fe                jmp    @0xe7857350[4byte]
 *  +44   m4 @0xe7856054                       <label>
 *  +44   m4 @0xe7857428  b9 e9 76 75 f7       mov    $0xf77576e9 -> %ecx
 *  +49   m4 @0xe7855b54  64 c7 05 64 00 00 00 mov    @0xe7856514[4byte] ->
 *                                                    %fs:0x00000064[4byte]
 *                        98 e6 7b e7
 *  +60   m4 @0xe7857a30  e9 0a 35 07 00       jmp    $0xe7831ba7
 *  +65   m4 @0xe7856514                       <label>
 *  +65   m4 @0xe7857350                       <label>
 *  +65   L3              83 fe 23             cmp    %esi $0x00000023
 *  +68   m4 @0xe7856da8  64 8b 0d 60 00 00 00 mov    %fs:0x00000060[4byte] -> %ecx
 *  +75   L3              0f 87 16 01 00 00    jnbe   $0xf7757808
 * END 0xf77576e6
 * \endcode
 */
void
instrlist_disassemble(dcontext_t *dcontext, app_pc tag,
                      instrlist_t *ilist, file_t outfile);

#endif /* INTERNAL || DEBUG || CLIENT_INTERFACE */

#endif /* DISASSEMBLE_H */
