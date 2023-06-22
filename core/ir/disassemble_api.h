/* **********************************************************
 * Copyright (c) 2010-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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

#ifndef _DR_IR_DISASSEMBLE_H_
#define _DR_IR_DISASSEMBLE_H_ 1

/**************************************************
 * DISASSEMBLY ROUTINES
 */
/**
 * @file dr_ir_disassemble.h
 * @brief Disassembly routines.
 */

/**
 * Flags controlling disassembly style
 */
typedef enum {
    /**
     * The default: displays all operands, including implicit operands.
     * Lists source operands first, then "->", and then destination
     * operands.
     */
    DR_DISASM_DR = 0x0,
    /**
     * Requests Intel syntax for disassembly.  This sets the same option that is
     * controlled by the runtime option \p -syntax_intel.  Implicit operands
     * are not displayed.
     */
    DR_DISASM_INTEL = 0x1,
    /**
     * Requests AT&T syntax for disassembly.  This sets the same option that is
     * controlled by the runtime option \p -syntax_att.  Implicit operands
     * are not displayed.
     */
    DR_DISASM_ATT = 0x2,
    /**
     * Certain reserved or unspecified opcodes are in a gray area where they
     * could be decoded with their length and operands understood, but they are
     * not fully defined and in fact they may raise an illegal instruction fault
     * when executed.  By default, DR does not treat them as invalid.  If this
     * option is set, DR tightens up its decoding and does treat them as
     * invalid.
     */
    DR_DISASM_STRICT_INVALID = 0x4,
    /**
     * This flag only applies to the default DR style (i.e., it does not apply
     * when DR_DISASM_INTEL or DR_DISASM_ATT is selected).  That style by
     * default displays the size of memory or sub-register operands via a
     * suffix "[Nbytes]".  Setting this flag removes that suffix.
     */
    DR_DISASM_NO_OPND_SIZE = 0x8,
    /**
     * Requests standard ARM (32-bit) assembler syntax for disassembly.  This
     * sets the same option that is controlled by the runtime option
     * \p -syntax_arm.  Implicit operands are not displayed.
     */
    DR_DISASM_ARM = 0x10,
    /**
     * Requests RISC-V assembler syntax for disassembly. This set the same option that
     * is controlled by the runtime option \p -syntax_riscv. Implicit oprands are not
     * displayed.
     */
    DR_DISASM_RISCV = 0x20,
} dr_disasm_flags_t;
/* TODO i#4382: Add DR_DISASM_AARCH64. */

DR_API
/**
 * Sets the disassembly style and decoding options.
 * The default is to use DR's custom syntax, unless one of the \ref op_syntax_intel
 * "-syntax_intel", \ref op_syntax_att "-syntax_att", \ref op_syntax_arm
 * "-syntax_arm", or \ref op_syntax_riscv "-syntax_riscv" runtime options is specified.
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
disassemble(void *drcontext, byte *pc, file_t outfile);

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
disassemble_with_info(void *drcontext, byte *pc, file_t outfile, bool show_pc,
                      bool show_bytes);

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
disassemble_from_copy(void *drcontext, byte *copy_pc, byte *orig_pc, file_t outfile,
                      bool show_pc, bool show_bytes);

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
disassemble_to_buffer(void *drcontext, byte *pc, byte *orig_pc, bool show_pc,
                      bool show_bytes, char *buf, size_t bufsz, int *printed OUT);

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
instr_disassemble(void *drcontext, instr_t *instr, file_t outfile);

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
instr_disassemble_to_buffer(void *drcontext, instr_t *instr, char *buf, size_t bufsz);

DR_API
/**
 * Prints the operand \p opnd to file \p outfile.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 */
void
opnd_disassemble(void *drcontext, opnd_t opnd, file_t outfile);

DR_API
/**
 * Prints the operand \p opnd to the buffer \p buf.
 * Always null-terminates, and will not print more than \p bufsz characters,
 * which includes the final null character.
 * Returns the number of characters printed, not including the final null.
 * The default is to use DR's custom syntax (see disassemble_set_syntax()).
 */
size_t
opnd_disassemble_to_buffer(void *drcontext, opnd_t opnd, char *buf, size_t bufsz);

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
instrlist_disassemble(void *drcontext, app_pc tag, instrlist_t *ilist, file_t outfile);

#endif /* _DR_IR_DISASSEMBLE_H_ */
