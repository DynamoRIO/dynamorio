/* **********************************************************
 * Copyright (c) 2001-2009 VMware, Inc.  All rights reserved.
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

/* file "decode_fast.h" */

#ifndef DECODE_FAST_H
#define DECODE_FAST_H

/* DR_API EXPORT TOFILE dr_ir_utils.h */
DR_API
/** 
 * Decodes only enough of the instruction at address \p pc to determine its size.
 * Returns that size.
 * If \p num_prefixes is non-NULL, returns the number of prefix bytes.
 * If \p rip_rel_pos is non-NULL, returns the offset into the instruction
 * of a rip-relative addressing displacement (for data only: ignores
 * control-transfer relative addressing), or 0 if none.
 * May return 0 size for certain invalid instructions.
 */
int 
decode_sizeof(dcontext_t *dcontext, byte *pc, int *num_prefixes
              _IF_X64(uint *rip_rel_pos));

DR_API
/** 
 * Decodes only enough of the instruction at address \p pc to determine its size.
 * Returns the address of the byte following the instruction.
 * Returns NULL on decoding an invalid instruction.
 */
#ifdef UNSUPPORTED_API
/**
 * This corresponds to a Level 1 decoding.
 */
#endif
byte *
decode_next_pc(dcontext_t *dcontext, byte *pc);

DR_UNS_API
/** 
 * Decodes the size of the instruction at address \p pc.
 * The instruction's raw bits are set to valid and pointed at \p pc
 * (xref instr_get_raw_bits()).
 * Assumes that \p instr is already initialized, but uses the x86/x64 mode
 * for the thread \p dcontext rather than that set in instr.
 * If caller is re-using same instr struct over multiple decodings,
 * caller should call instr_reset() or instr_reuse().
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instr and sets opcode to OP_INVALID.
 */
#ifdef UNSUPPORTED_API
/* This corresponds to a Level 1 decoding. */
#endif
byte *
decode_raw(dcontext_t *dcontext, byte *pc, instr_t *instr);

DR_UNS_API
/** 
 * Decodes only enough of the instruction at address \p pc to determine
 * its size, its effects on the 6 arithmetic eflags, and whether it is
 * a control-transfer instruction.  If it is, the operands fields of
 * \p instr are filled in.  If not, only the raw bits fields of \p instr are
 * filled in. 
 *
 * Fills in the PREFIX_SEG_GS and PREFIX_SEG_FS prefix flags for all instrs.
 * Does NOT fill in any other prefix flags unless this is a cti instr
 * and the flags affect the instr.
 *
 * For x64, calls instr_set_rip_rel_pos().  Thus, if the raw bytes are
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
#ifdef UNSUPPORTED_API
/* This corresponds to a Level 3 decoding for control transfer
 * instructions but a Level 1 decoding plus arithmetic eflags
 * information for all other instructions.
 */
#endif
byte *
decode_cti(dcontext_t *dcontext, byte *pc, instr_t *instr);


#endif /* DECODE_FAST_H */
