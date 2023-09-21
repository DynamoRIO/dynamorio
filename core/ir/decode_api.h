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

#ifndef _DR_IR_DECODE_H_
#define _DR_IR_DECODE_H_ 1

/**************************************************
 * DECODING ROUTINES
 */
/**
 * @file dr_ir_decode.h
 * @brief Decoding routines.
 */

DR_API
/**
 * Decodes only enough of the instruction at address \p pc to determine
 * its eflags usage, which is returned in \p usage as EFLAGS_ constants
 * or'ed together.
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instruction.
 */
/* This corresponds to halfway between Level 1 and Level 2: a Level 1 decoding
 * plus eflags information (usually only at Level 2).  Levels are not exposed
 * in the API anymore, however.
 */
byte *
decode_eflags_usage(void *drcontext, byte *pc, uint *usage, dr_opnd_query_flags_t flags);

DR_API
/**
 * Decodes the instruction at address \p pc into \p instr, filling in the
 * instruction's opcode, eflags usage, prefixes, and operands.
 * The instruction's raw bits are set to valid and pointed at \p pc
 * (xref instr_get_raw_bits()).
 * Assumes that \p instr is already initialized, but uses the x86/x64 mode
 * for the thread \p dcontext rather than that set in instr.
 * If caller is re-using same instr_t struct over multiple decodings,
 * caller should call instr_reset() or instr_reuse().
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instr and sets opcode to OP_INVALID.
 */
/* This corresponds to a Level 3 decoding.  Levels are not exposed
 * in the API anymore, however.
 */
byte *
decode(void *drcontext, byte *pc, instr_t *instr);

DR_API
/**
 * Decodes the instruction at address \p copy_pc into \p instr as though
 * it were located at address \p orig_pc.  Any pc-relative operands have
 * their values calculated as though the instruction were actually at
 * \p orig_pc, though that address is never de-referenced.
 * The instruction's raw bits are not valid, but its application address field
 * (see instr_get_app_pc()) is set to \p orig_pc.
 * The instruction's opcode, eflags usage, prefixes, and operands are
 * all filled in.
 * Assumes that \p instr is already initialized, but uses the x86/x64 mode
 * for the thread \p dcontext rather than that set in instr.
 * If caller is re-using same instr_t struct over multiple decodings,
 * caller should call instr_reset() or instr_reuse().
 * Returns the address of the next byte after the decoded instruction
 * copy at \p copy_pc.
 * Returns NULL on decoding an invalid instr and sets opcode to OP_INVALID.
 */
/* This corresponds to a Level 3 decoding.  Levels are not exposed
 * in the API anymore, however.
 */
byte *
decode_from_copy(void *drcontext, byte *copy_pc, byte *orig_pc, instr_t *instr);

/* decode_as_bb() is defined in interp.c, but declared here so it will
 * be listed next to the other decode routines in the API headers.
 */
DR_API
/**
 * Client routine to decode instructions at an arbitrary app address,
 * following all the rules that DynamoRIO follows internally for
 * terminating basic blocks.  Note that DynamoRIO does not validate
 * that \p start_pc is actually the first instruction of a basic block.
 * \note Caller is reponsible for freeing the list and its instrs!
 */
instrlist_t *
decode_as_bb(void *drcontext, byte *start_pc);

DR_API
/**
 * Decodes the trace with tag \p tag, and returns an instrlist_t of
 * the instructions comprising that fragment.  If \p tag is not a
 * valid tag for an existing trace, the routine returns NULL.  Clients
 * can use dr_trace_exists_at() to determine whether the trace exists.
 * \note Unlike the instruction list presented by the trace event, the
 * list here does not include any existing client modifications.  If
 * client-modified instructions are needed, it is the responsibility
 * of the client to record or recreate that list itself.
 * \note This routine does not support decoding thread-private traces
 * created by other than the calling thread.
 */
instrlist_t *
decode_trace(void *drcontext, void *tag);

DR_API
/**
 * Given an OP_ constant, returns the first byte of its opcode when
 * encoded as an IA-32 instruction.
 */
byte
decode_first_opcode_byte(int opcode);

DR_API
/** Given an OP_ constant, returns the string name of its opcode. */
const char *
decode_opcode_name(int opcode);

#ifdef X64
DR_API
/**
 * The decode and encode routines use a per-thread persistent flag that
 * indicates whether to treat code as 32-bit (x86) or 64-bit (x64).  This
 * routine sets that flag to the indicated value and returns the old value.  Be
 * sure to restore the old value prior to any further application execution to
 * avoid problems in mis-interpreting application code.
 *
 * \note For 64-bit DR builds only.
 *
 * \deprecated Replaced by dr_set_isa_mode().
 */
bool
set_x86_mode(void *drcontext, bool x86);

DR_API
/**
 * The decode and encode routines use a per-thread persistent flag that
 * indicates whether to treat code as 32-bit (x86) or 64-bit (x64).  This
 * routine returns the value of that flag.
 *
 * \note For 64-bit DR builds only.
 *
 * \deprecated Replaced by dr_get_isa_mode().
 */
bool
get_x86_mode(void *drcontext);
#endif /* X64 */

DR_API
/**
 * Given an application program counter value, returns the
 * corresponding value to use as an indirect branch target for the
 * given \p isa_mode.  For ARM's Thumb mode (#DR_ISA_ARM_THUMB), this
 * involves setting the least significant bit of the address.
 */
app_pc
dr_app_pc_as_jump_target(dr_isa_mode_t isa_mode, app_pc pc);

DR_API
/**
 * Given an application program counter value, returns the
 * corresponding value to use as a memory load target for the given \p
 * isa_mode, or for comparing to the application address inside a
 * basic block or trace.  For ARM's Thumb mode (#DR_ISA_ARM_THUMB),
 * this involves clearing the least significant bit of the address.
 */
app_pc
dr_app_pc_as_load_target(dr_isa_mode_t isa_mode, app_pc pc);

DR_API
/**
 * Calculates the size, in bytes, of the memory read or write of
 * the instr at \p pc.  If the instruction is a repeating string instruction,
 * considers only one iteration.
 * Returns the pc of the following instruction.
 * If the instruction at \p pc does not reference memory, or is invalid,
 * returns NULL.
 */
app_pc
decode_memory_reference_size(void *drcontext, app_pc pc, uint *size_in_bytes);

DR_API
/**
 * Decodes only enough of the instruction at address \p pc to determine its size.
 * Returns that size.
 * If \p num_prefixes is non-NULL, returns the number of prefix bytes.
 *
 * On x86, if \p rip_rel_pos is non-NULL, returns the offset into the instruction of a
 * rip-relative addressing displacement (for data only: ignores control-transfer
 * relative addressing; use decode_sizeof_ex() for that), or 0 if none.
 * The \p rip_rel_pos parameter is only implemented for x86, where the displacement
 * is always 4 bytes in size.
 *
 * May return 0 size for certain invalid instructions.
 */
int
decode_sizeof(void *drcontext, byte *pc, int *num_prefixes _IF_X86_64(uint *rip_rel_pos));

#ifdef X86
DR_API
/**
 * Decodes only enough of the instruction at address \p pc to determine its size.
 * Returns that size.
 * If \p num_prefixes is non-NULL, returns the number of prefix bytes.
 *
 * On x86, if \p rip_rel_pos is non-NULL, returns the offset into the instruction of a
 * rip-relative addressing displacement for data or control-transfer relative
 * addressing, or 0 if none.  This is only implemented for x86, where the displacement
 * is always 4 bytes for data but can be 1 byte for control.
 *
 * May return 0 size for certain invalid instructions.
 */
int
decode_sizeof_ex(void *drcontext, byte *pc, int *num_prefixes, uint *rip_rel_pos);
#endif /* X86 */

DR_API
/**
 * Decodes only enough of the instruction at address \p pc to determine its size.
 * Returns the address of the byte following the instruction.
 * May return NULL on decoding certain invalid instructions.
 */
/**
 * This corresponds to a Level 1 decoding.  Levels are not exposed
 * in the API anymore, however.
 */
byte *
decode_next_pc(void *drcontext, byte *pc);

DR_UNS_EXCEPT_TESTS_API
/* Decodes only enough of the instruction at address \p pc to determine
 * its size, its effects on the 6 arithmetic eflags, and whether it is
 * a control-transfer instruction.  If it is, the operands fields of
 * \p instr are filled in.  If not, only the raw bits fields of \p instr are
 * filled in.
 *
 * Fills in the PREFIX_SEG_GS and PREFIX_SEG_FS prefix flags for all instrs.
 * Does NOT fill in any other prefix flags unless this is a cti instr
 * and the flags affect the instr.
 *
 * For x86, calls instr_set_rip_rel_pos().  Thus, if the raw bytes are
 * not modified prior to encode time, any rip-relative offset will be
 * automatically re-relativized (though encoding will fail if the new
 * encode location cannot reach the original target).
 *
 * Assumes that \p instr is already initialized, but uses the x86/x64 mode
 * for the thread \p dcontext rather than that set in instr.
 * If caller is re-using same instr struct over multiple decodings,
 * caller should call instr_reset() or instr_reuse().
 * Returns the address of the byte following the instruction.
 * Returns NULL on decoding an invalid instr and sets opcode to OP_INVALID.
 */
/* This corresponds to a Level 3 decoding for control transfer
 * instructions but a Level 1 decoding plus arithmetic eflags
 * information for all other instructions.  Levels are not exposed
 * in the API anymore, however.
 */
byte *
decode_cti(void *drcontext, byte *pc, instr_t *instr);

#endif /* _DR_IR_DECODE_H_ */
