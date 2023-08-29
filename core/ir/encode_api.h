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

#ifndef _DR_IR_ENCODE_H_
#define _DR_IR_ENCODE_H_ 1

/**************************************************
 * ENCODING ROUTINES
 */
/**
 * @file dr_ir_encode.h
 * @brief Encoding routines.
 */

/** Specifies which processor mode to use when decoding or encoding. */
typedef enum _dr_isa_mode_t {
    DR_ISA_IA32,              /**< IA-32 (Intel/AMD 32-bit mode). */
    DR_ISA_X86 = DR_ISA_IA32, /**< Alias for DR_ISA_IA32. */
    DR_ISA_AMD64,             /**< AMD64 (Intel/AMD 64-bit mode). */
    DR_ISA_ARM_A32,           /**< ARM A32 (AArch32 ARM). */
    DR_ISA_ARM_THUMB,         /**< Thumb (ARM T32). */
    DR_ISA_ARM_A64,           /**< ARM A64 (AArch64). */
    DR_ISA_RV64IMAFDC,        /**< RISC-V (rv64imafdc). */
} dr_isa_mode_t;

DR_API
/**
 * The decode and encode routines use a per-thread persistent flag that
 * indicates which processor mode to use.  This routine sets that flag to the
 * indicated value and optionally returns the old value.  Be sure to restore the
 * old value prior to any further application execution to avoid problems in
 * mis-interpreting application code.
 */
bool
dr_set_isa_mode(void *drcontext, dr_isa_mode_t new_mode, dr_isa_mode_t *old_mode OUT);

DR_API
/**
 * The decode and encode routines use a per-thread persistent flag that
 * indicates which processor mode to use.  This routine returns the value of
 * that flag.
 */
dr_isa_mode_t
dr_get_isa_mode(void *drcontext);

/**
 * AArch64 Scalable Vector Extension's vector length in bits is one of:
 * 128 256 384 512 640 768 896 1024 1152 1280 1408 1536 1664 1792 1920 2048
 * TODO i#3044: This function will only allow setting vector length if not
 * running on SVE.
 */
void
dr_set_sve_vector_length(int vl);

/**
 * Read AArch64 Scalable Vector Extension's vector length, in bits.
 */
int
dr_get_sve_vector_length(void);

enum {
#ifdef X86
    MAX_INSTR_LENGTH = 17,
    MAX_SRC_OPNDS = 8, /* pusha */
    MAX_DST_OPNDS = 8, /* popa */
#elif defined(AARCH64)
    /* The maximum instruction length is 64 to allow for an OP_ldstex containing
     * up to 16 real instructions. The longest such block seen so far in real
     * code had 7 instructions so this is likely to be enough. With the current
     * implementation, a larger value would significantly slow down the search
     * for such blocks in the decoder: see decode_ldstex().
     */
    MAX_INSTR_LENGTH = 64,
    MAX_SRC_OPNDS = 8,
    MAX_DST_OPNDS = 8,
#elif defined(ARM)
    MAX_INSTR_LENGTH = 4,
    /* With register lists we can see quite long operand lists. */
    MAX_SRC_OPNDS = 33, /* vstm s0-s31 */
    MAX_DST_OPNDS = MAX_SRC_OPNDS,
#elif defined(RISCV64)
    MAX_INSTR_LENGTH = 4,
    MAX_SRC_OPNDS = 3,
    MAX_DST_OPNDS = 1,
#endif
};

DR_API
/**
 * Returns true iff \p instr can be encoded as
 *    - a valid IA-32 instruction on X86
 *    - a valid Armv8-a instruction on AArch64 (Note: The AArch64 encoder/decoder is
 *      not complete yet, so DynamoRIO may fail to encode some valid Armv8-a
 *      instructions)
 *    - a valid Armv7 instruction on ARM
 */
bool
instr_is_encoding_possible(instr_t *instr);

DR_API
/**
 * Encodes \p instr into the memory at \p pc.
 * Uses the x86/x64 mode stored in instr, not the mode of the current thread.
 * Returns the pc after the encoded instr, or NULL if the encoding failed.
 * If instr is a cti with an instr_t target, the offset fields of instr and
 * of the target must be set with the respective offsets of each instr_t!
 * (instrlist_encode does this automatically, if the target is in the list).
 * x86 instructions can occupy up to 17 bytes, so the caller should ensure
 * the target location has enough room to avoid overflow.
 * \note: In Thumb mode, some instructions have different behavior depending
 * on whether they are in an IT block. To correctly encode such instructions,
 * they should be encoded within an instruction list with the corresponding
 * IT instruction using instrlist_encode().
 */
byte *
instr_encode(void *drcontext, instr_t *instr, byte *pc);

DR_API
/**
 * Encodes \p instr into the memory at \p copy_pc in preparation for copying
 * to \p final_pc.  Any pc-relative component is encoded as though the
 * instruction were located at \p final_pc.  This allows for direct copying
 * of the encoded bytes to \p final_pc without re-relativization.
 *
 * Uses the x86/x64 mode stored in instr, not the mode of the current thread.
 * Returns the pc after the encoded instr, or NULL if the encoding failed.
 * If instr is a cti with an instr_t target, the offset fields of instr and
 * of the target must be set with the respective offsets of each instr_t!
 * (instrlist_encode does this automatically, if the target is in the list).
 * x86 instructions can occupy up to 17 bytes, so the caller should ensure
 * the target location has enough room to avoid overflow.
 * \note: In Thumb mode, some instructions have different behavior depending
 * on whether they are in an IT block. To correctly encode such instructions,
 * they should be encoded within an instruction list with the corresponding
 * IT instruction using instrlist_encode().
 */
byte *
instr_encode_to_copy(void *drcontext, instr_t *instr, byte *copy_pc, byte *final_pc);

DR_API
/**
 * Encodes each instruction in \p ilist in turn in contiguous memory starting
 * at \p pc.  Returns the pc after all of the encodings, or NULL if any one
 * of the encodings failed.
 * Uses the x86/x64 mode stored in each instr, not the mode of the current thread.
 * In order for instr_t operands to be encoded properly,
 * \p has_instr_jmp_targets must be true.  If \p has_instr_jmp_targets is true,
 * the offset field of each instr_t in ilist will be overwritten, and if any
 * instr_t targets are not in \p ilist, they must have their offset fields set with
 * their offsets relative to pc.
 * x86 instructions can occupy up to 17 bytes each, so the caller should ensure
 * the target location has enough room to avoid overflow.
 */
byte *
instrlist_encode(void *drcontext, instrlist_t *ilist, byte *pc,
                 bool has_instr_jmp_targets);

DR_API
/**
 * Encodes each instruction in \p ilist in turn in contiguous memory
 * starting \p copy_pc in preparation for copying to \p final_pc.  Any
 * pc-relative instruction is encoded as though the instruction list were
 * located at \p final_pc.  This allows for direct copying of the
 * encoded bytes to \p final_pc without re-relativization.
 *
 * Returns the pc after all of the encodings, or NULL if any one
 * of the encodings failed.
 *
 * Uses the x86/x64 mode stored in each instr, not the mode of the current thread.
 *
 * In order for instr_t operands to be encoded properly,
 * \p has_instr_jmp_targets must be true.  If \p has_instr_jmp_targets is true,
 * the offset field of each instr_t in ilist will be overwritten, and if any
 * instr_t targets are not in \p ilist, they must have their offset fields set with
 * their offsets relative to pc.
 *
 * If \p max_pc is non-NULL, computes the total size required to encode the
 * instruction list before performing any encoding.  If the whole list will not
 * fit starting at \p copy_pc without exceeding \p max_pc, returns NULL without
 * encoding anything.  Otherwise encodes as normal.  Note that x86 instructions
 * can occupy up to 17 bytes each, so if \p max_pc is NULL, the caller should
 * ensure the target location has enough room to avoid overflow.
 */
byte *
instrlist_encode_to_copy(void *drcontext, instrlist_t *ilist, byte *copy_pc,
                         byte *final_pc, byte *max_pc, bool has_instr_jmp_targets);

#endif /* _DR_IR_ENCODE_H_ */
