/* **********************************************************
 * Copyright (c) 2010-2024 Google, Inc.  All rights reserved.
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
    /**
     * IA-32 (Intel/AMD 32-bit mode).
     */
    DR_ISA_IA32,
    /**
     * Alias for DR_ISA_IA32.
     */
    DR_ISA_X86 = DR_ISA_IA32,
    /**
     * AMD64 (Intel/AMD 64-bit mode).
     */
    DR_ISA_AMD64,
    /**
     * ARM A32 (AArch32 ARM).
     */
    DR_ISA_ARM_A32,
    /**
     * Thumb (ARM T32).
     */
    DR_ISA_ARM_THUMB,
    /**
     * ARM A64 (AArch64).
     */
    DR_ISA_ARM_A64,
    /**
     * RISC-V (RV64).
     */
    DR_ISA_RV64,
    /**
     * A synthetic ISA that has the purpose of preserving register dependencies and giving
     * hints on the type of operation an instruction performs.
     *
     * Being a synthetic ISA, some routines that work on instructions coming from an
     * actual ISA (such as #DR_ISA_AMD64) are not supported (e.g., decode_sizeof()).
     *
     * Currently we support:
     * - instr_convert_to_isa_regdeps(): to convert an #instr_t of an actual ISA to a
     *   #DR_ISA_REGDEPS #instr_t.
     * - instr_encode() and instr_encode_to_copy(): to encode a #DR_ISA_REGDEPS #instr_t
     *   into a sequence of contiguous bytes.
     * - decode() and decode_from_copy(): to decode an encoded #DR_ISA_REGDEPS instruction
     *   into an #instr_t.
     *
     * A #DR_ISA_REGDEPS #instr_t contains the following information:
     * - categories: composed by #dr_instr_category_t values, they indicate the type of
     *   operation performed (e.g., a load, a store, a floating point math operation, a
     *   branch, etc.).  Note that categories are composable, hence more than one category
     *   can be set.  This information can be obtained using instr_get_category().
     * - arithmetic flags: we don't distinguish between different flags, we only report if
     *   at least one arithmetic flag was read (all arithmetic flags will be set to read)
     *   and/or written (all arithmetic flags will be set to written).  This information
     *   can be obtained using instr_get_arith_flags().
     * - number of source and destination operands: we only consider register operands.
     *   This information can be obtained using instr_num_srcs() and instr_num_dsts().
     * - source operation size: is the largest source operand the instruction operates on.
     *   This information can be obtained by accessing the #instr_t operation_size field.
     * - list of register operand identifiers: they are contained in #opnd_t lists,
     *   separated in source and destination. Note that these #reg_id_t identifiers are
     *   virtual and it should not be assumed that they belong to any DR_REG_ enum value
     *   of any specific architecture. These identifiers are meant for tracking register
     *   dependencies with respect to other #DR_ISA_REGDEPS instructions only.  These
     *   lists can be obtained by walking the #instr_t operands with instr_get_dst() and
     *   instr_get_src().
     * - ISA mode: is always #DR_ISA_REGDEPS.  This information can be obtained using
     *   instr_get_isa_mode().
     * - encoding bytes: an array of bytes containing the #DR_ISA_REGDEPS #instr_t
     *   encoding.  Note that this information is present only for decoded instructions
     *   (i.e., #instr_t generated by decode() or decode_from_copy()).  This information
     *   can be obtained using instr_get_raw_bits().
     * - length: the length of the encoded instruction in bytes.  Note that this
     *   information is present only for decoded instructions (i.e., #instr_t generated by
     *   decode() or decode_from_copy()).  This information can be obtained by accessing
     *   the #instr_t length field.
     *
     * Note that all routines that operate on #instr_t and #opnd_t are also supported for
     * #DR_ISA_REGDEPS instructions.  However, querying information outside of those
     * described above (e.g., the instruction opcode with instr_get_opcode()) will return
     * the zeroed value set by instr_create() or instr_init() when the #instr_t was
     * created (e.g., instr_get_opcode() would return OP_INVALID).
     */
    DR_ISA_REGDEPS,
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
dr_set_isa_mode(void *drcontext, dr_isa_mode_t new_mode,
                dr_isa_mode_t *old_mode DR_PARAM_OUT);

DR_API
/**
 * The decode and encode routines use a per-thread persistent flag that
 * indicates which processor mode to use.  This routine returns the value of
 * that flag.
 */
dr_isa_mode_t
dr_get_isa_mode(void *drcontext);

DR_API
/**
 * - AArch64 Scalable Vector Extension's vector length in bits is one of:
 *   128 256 384 512 640 768 896 1024 1152 1280 1408 1536 1664 1792 1920 2048
 * - RISC-V Vector Extension's vector length in bit is from 64 to 65536 in the
 *   power of 2.
 * Returns whether successful.
 * TODO i#3044: This function will only allow setting vector length if not
 * running on SVE or RVV.
 */
bool
dr_set_vector_length(int vl);

DR_API
/**
 * Read AArch64 SVE or RISC-V Vector's vector length, in bits.
 */
int
dr_get_vector_length(void);

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
