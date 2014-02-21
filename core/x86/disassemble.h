/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
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

#if defined(INTERNAL) || defined(DEBUG) || defined(CLIENT_INTERFACE)

/* for printing to buffers */
#define MAX_OPND_DIS_SZ   32
/* Long example:
 * "<RAW>  <raw 0x00007f85922c0877-0x00007f85922c0882 == 48 63 f8 48 89 d6 b8 05 00 ...>"
 */
#define MAX_INSTR_DIS_SZ  96
/* Here's a pretty long one,
 * "  0x00007f859277d63a  48 83 05 4e 63 21 00 add    $0x0000000000000001 <rel> 0x00007f8592993990 -> <rel> 0x00007f8592993990 \n                     01 "
 */
#define MAX_PC_DIS_SZ    192

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
} dr_disasm_flags_t;
/* DR_API EXPORT END */

DR_API
/**
 * Sets the disassembly style and decoding options.
 */
void
disassemble_set_syntax(dr_disasm_flags_t flags);

DR_API
/**
 * Decodes and then prints the instruction at address \p pc to file \p outfile.
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
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
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
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
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
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
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
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
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
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
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
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
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
 */
size_t
instr_disassemble_to_buffer(dcontext_t *dcontext, instr_t *instr,
                            char *buf, size_t bufsz);

/* DR_API EXPORT TOFILE dr_ir_opnd.h */
DR_API
/**
 * Prints the operand \p opnd to file \p outfile.
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
 */
void
opnd_disassemble(dcontext_t *dcontext, opnd_t opnd, file_t outfile);

DR_API
/**
 * Prints the operand \p opnd to the buffer \p buf.
 * Always null-terminates, and will not print more than \p bufsz characters,
 * which includes the final null character.
 * Returns the number of characters printed, not including the final null.
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
 */
size_t
opnd_disassemble_to_buffer(dcontext_t *dcontext, opnd_t opnd, char *buf, size_t bufsz);

/* DR_API EXPORT TOFILE dr_ir_instrlist.h */
DR_API
/**
 * Prints each instruction in \p ilist in sequence to \p outfile.
 * The default is to use AT&T-style syntax, unless the \ref op_syntax_intel
 * "-syntax_intel" runtime option is specified.
 */
void
instrlist_disassemble(dcontext_t *dcontext, app_pc tag,
                      instrlist_t *ilist, file_t outfile);

#endif /* INTERNAL || DEBUG || CLIENT_INTERFACE */

#endif /* DISASSEMBLE_H */
