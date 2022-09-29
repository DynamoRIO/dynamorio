/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
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

#ifndef _DR_IR_UTILS_H_
#define _DR_IR_UTILS_H_ 1

/**************************************************
 * CODE TRANSFORMATION UTILITIES
 */
/**
 * @file dr_ir_utils.h
 * @brief Code transformation utilities.
 */

/**
 * An enum of spill slots to use with dr_save_reg(), dr_restore_reg(),
 * dr_save_arith_flags(), dr_restore_arith_flags() and
 * dr_insert_mbr_instrumentation().  Values stored in spill slots remain
 * valid only until the next non-meta (i.e. application) instruction.  Spill slots
 * can be accessed/modifed during clean calls and restore_state_events (see
 * dr_register_restore_state_event()) with dr_read_saved_reg() and
 * dr_write_saved_reg().
 *
 * Spill slots <= dr_max_opnd_accessible_spill_slot() can be directly accessed
 * from client inserted instructions with dr_reg_spill_slot_opnd().
 *
 * \note Some spill slots may be faster to access than others.  Currently spill
 * slots 1-3 are significantly faster to access than the others when running
 * without -thread_private.  When running with -thread_private all spill slots
 * are expected to have similar performance.  This is subject to change in future
 * releases, but clients may assume that smaller numbered spill slots are faster
 * or the same cost to access as larger numbered spill slots.
 *
 * \note The number of spill slots may change in future releases.
 */
typedef enum {
    SPILL_SLOT_1 = 0, /** spill slot for register save/restore routines */
    SPILL_SLOT_2 = 1, /** spill slot for register save/restore routines */
    SPILL_SLOT_3 = 2, /** spill slot for register save/restore routines */
    SPILL_SLOT_4 = 3, /** spill slot for register save/restore routines */
    SPILL_SLOT_5 = 4, /** spill slot for register save/restore routines */
    SPILL_SLOT_6 = 5, /** spill slot for register save/restore routines */
    SPILL_SLOT_7 = 6, /** spill slot for register save/restore routines */
    SPILL_SLOT_8 = 7, /** spill slot for register save/restore routines */
    SPILL_SLOT_9 = 8, /** spill slot for register save/restore routines */
#ifdef X64
    SPILL_SLOT_10 = 9,             /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_11 = 10,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_12 = 11,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_13 = 12,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_14 = 13,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_15 = 14,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_16 = 15,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_17 = 16,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_MAX = SPILL_SLOT_17 /** Enum value of the last register save/restore
                                    *  spill slot. */
#else
    SPILL_SLOT_MAX = SPILL_SLOT_9 /** Enum value of the last register save/restore
                                   *  spill slot. */
#endif
} dr_spill_slot_t;

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the
 * register \p reg in the spill slot \p slot.  See dr_restore_reg(). Use
 * dr_read_saved_reg() and dr_write_saved_reg() to access spill slots from clean
 * calls and restore_state_events (see dr_register_restore_state_event()).
 * \note The stored value remains available only until the next non-meta (i.e.
 * application) instruction. Use dr_insert_write_tls_field() and
 * dr_insert_read_tls_field() for a persistent (but more costly to access)
 * thread-local-storage location.  See also dr_raw_tls_calloc().
 *
 * Generally, using the \ref page_drreg Extension Library instead is recommended.
 * When using custom spills and restores, be sure to look for the labels
 * #DR_NOTE_ANNOTATION and #DR_NOTE_REG_BARRIER at which all application values
 * should be restored to registers.
 */
void
dr_save_reg(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
            dr_spill_slot_t slot);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore the
 * register \p reg from the spill slot \p slot.  See dr_save_reg() for notes on
 * lifetime and alternative access to spill slots.
 */
void
dr_restore_reg(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
               dr_spill_slot_t slot);

DR_API
/**
 * Returns the largest dr_spill_slot_t that can be accessed with an opnd_t from
 * dr_reg_spill_slot_opnd().
 */
dr_spill_slot_t
dr_max_opnd_accessible_spill_slot(void);

DR_API
/**
 * Returns an opnd_t that directly accesses the spill slot \p slot. Only slots
 * <= dr_max_opnd_accessible_spill_slot() can be used with this routine.
 * \note \p slot must be <= dr_max_opnd_accessible_spill_slot()
 */
opnd_t
dr_reg_spill_slot_opnd(void *drcontext, dr_spill_slot_t slot);

/* internal version */
opnd_t
reg_spill_slot_opnd(void *drcontext, dr_spill_slot_t slot);

DR_API
/**
 * Can be used from a clean call or a restore_state_event (see
 * dr_register_restore_state_event()) to see the value saved in spill slot
 * \p slot by dr_save_reg().
 */
reg_t
dr_read_saved_reg(void *drcontext, dr_spill_slot_t slot);

DR_API
/**
 * Can be used from a clean call to modify the value saved in the spill slot
 * \p slot by dr_save_reg() such that a later dr_restore_reg() will see the
 * new value.
 *
 * \note This routine should only be used during a clean call out of the
 * cache.  Use at any other time could corrupt application or DynamoRIO
 * state.
 */
void
dr_write_saved_reg(void *drcontext, dr_spill_slot_t slot, reg_t value);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the 6
 * arithmetic flags into xax after first saving xax to the spill slot \p slot.
 * This is equivalent to dr_save_reg() of xax to \p slot followed by lahf and
 * seto al instructions.  See dr_restore_arith_flags().
 *
 * \warning At completion of the inserted instructions the saved flags are in the
 * xax register.  The xax register should not be modified after using this
 * routine unless it is first saved (and later restored prior to
 * using dr_restore_arith_flags()).
 *
 * \note X86-only
 *
 * \deprecated This routine is equivalent to dr_save_reg() followed by
 * dr_save_arith_flags_to_xax().
 */
void
dr_save_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                    dr_spill_slot_t slot);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore the 6
 * arithmetic flags, assuming they were saved using dr_save_arith_flags() with
 * slot \p slot and that xax holds the same value it did after the save.
 *
 * \note X86-only
 *
 * \deprecated This routine is equivalent to dr_restore_arith_flags_from_xax()
 * followed by dr_restore_reg().
 */
void
dr_restore_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                       dr_spill_slot_t slot);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the 6
 * arithmetic flags into xax.  This currently uses DynamoRIO's "lahf ; seto al"
 * code sequence, which is faster and easier than pushf.  If the caller wishes
 * to use xax between saving and restoring these flags, they must save and
 * restore xax, potentially using dr_save_reg()/dr_restore_reg().  If the caller
 * needs to save both the current value of xax and the flags stored to xax by
 * this routine, they must use separate spill slots, or they will overwrite the
 * original xax value in memory.
 * \note X86-only
 *
 * \warning Clobbers xax; the caller must ensure xax is dead or saved at
 * \p where.
 */
void
dr_save_arith_flags_to_xax(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore the 6
 * arithmetic flags from xax.  This currently uses DynamoRIO's "add $0x7f %al ;
 * sahf" code sequence, which is faster and easier than popf.  The caller must
 * ensure that xax contains the arithmetic flags, most likely from
 * dr_save_arith_flags_to_xax().
 * \note X86-only
 */
void
dr_restore_arith_flags_from_xax(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the
 * arithmetic flags (6 arithmetic flags on X86 or APSR on ARM) into \p reg.
 * If the caller wishes to use \p reg between saving and restoring these flags,
 * they must save and restore \p reg, potentially using dr_save_reg()/dr_restore_reg().
 * If the caller needs to save both the current value of \p reg and the flags stored
 * to \p reg by this routine, they must use separate spill slots, or they will
 * overwrite the original \p reg value in memory.
 *
 * \note On X86, only DR_REG_XAX should be passed in.
 *
 * \warning Clobbers \p reg; the caller must ensure \p reg is dead or saved at
 * \p where.
 */
void
dr_save_arith_flags_to_reg(void *drcontext, instrlist_t *ilist, instr_t *where,
                           reg_id_t reg);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore
 * the arithmetic flags (6 arithmetic flags on X86 or APSR on ARM) from \p reg.
 * The caller must ensure that \p reg contains the program status flags,
 * most likely from dr_save_arith_flags_to_reg().
 *
 * \note On X86, only DR_REG_XAX should be passed in.
 */
void
dr_restore_arith_flags_from_reg(void *drcontext, instrlist_t *ilist, instr_t *where,
                                reg_id_t reg);

DR_API
/**
 * A convenience routine to aid restoring the arith flags done by outlined code,
 * such as when handling restore state events. The routine takes the current value of
 * the flags register \p cur_xflags, as well as the saved value \p saved_xflag, in order
 * to return the original app value.
 */
reg_t
dr_merge_arith_flags(reg_t cur_xflags, reg_t saved_xflag);

/* FIXME PR 315327: add routines to save, restore and access from C code xmm registers
 * from our dcontext slots.  Not clear we really need to since we can't do it all
 * that much faster than the client can already with read/write tls field (only one
 * extra load) or (if -thread_private) absolute addresses.
 */

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to read into the
 * general-purpose full-size register \p reg from the user-controlled drcontext
 * field for this thread.  Reads from the same field as dr_get_tls_field().
 */
void
dr_insert_read_tls_field(void *drcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to write the
 * general-purpose full-size register \p reg to the user-controlled drcontext field
 * for this thread.  Writes to the same field as dr_set_tls_field().
 */
void
dr_insert_write_tls_field(void *drcontext, instrlist_t *ilist, instr_t *where,
                          reg_id_t reg);

DR_API
/** Inserts \p instr as a non-application instruction into \p ilist prior to \p where. */
void
instrlist_meta_preinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/** Inserts \p instr as a non-application instruction into \p ilist after \p where. */
void
instrlist_meta_postinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/** Inserts \p instr as a non-application instruction onto the end of \p ilist */
void
instrlist_meta_append(instrlist_t *ilist, instr_t *instr);

DR_API
/**
 * Inserts \p instr as a non-application instruction that can fault (see
 * instr_set_meta_may_fault()) into \p ilist prior to \p where.
 *
 * \deprecated Essentially equivalent to instrlist_meta_preinsert()
 */
void
instrlist_meta_fault_preinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/**
 * Inserts \p instr as a non-application instruction that can fault (see
 * instr_set_meta_may_fault()) into \p ilist after \p where.
 *
 * \deprecated Essentially equivalent to instrlist_meta_postinsert()
 */
void
instrlist_meta_fault_postinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/**
 * Inserts \p instr as a non-application instruction that can fault (see
 * instr_set_meta_may_fault()) onto the end of \p ilist.
 *
 * \deprecated Essentially equivalent to instrlist_meta_append()
 */
void
instrlist_meta_fault_append(instrlist_t *ilist, instr_t *instr);

/* dr_insert_* are used by general DR */

/* FIXME PR 213600: for clean call args that reference memory the
 * client may prefer to receive the fault itself rather than it being treated
 * as an app exception (xref PR 302951).
 */
DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save state
 * for a call, switch to this thread's DR stack, set up the passed-in
 * parameters, make a call to \p callee, clean up the parameters, and
 * then restore the saved state.
 *
 * The callee must use the standard C calling convention that matches the
 * underlying 32-bit or 64-bit binary interface convention ("cdecl"). Other
 * calling conventions, such as "fastcall" and "stdcall", are not supported.
 *
 * This routine expects to be passed a number of arguments beyond \p
 * num_args equal to the value of \p num_args.  Each of those
 * arguments is a parameter to pass to the clean call, in the order
 * passed to this routine.  Each argument should be of type #opnd_t
 * and will be copied into the proper location for that argument
 * slot as specified by the calling convention.
 *
 * Stores the application state information on the DR stack, where it can be
 * accessed from \c callee using dr_get_mcontext() and modified using
 * dr_set_mcontext().  However, if register reservation code is in use (e.g.,
 * via the drreg extension library: \ref page_drreg), dr_insert_clean_call_ex()
 * must be called with its flags argument including
 * #DR_CLEANCALL_READS_APP_CONTEXT (for dr_get_mcontext() use) and/or
 * #DR_CLEANCALL_WRITES_APP_CONTEXT (for dr_set_mcontext() use) (and
 * possibly #DR_CLEANCALL_MULTIPATH) to ensure proper interaction with
 * register reservations.
 *
 * On x86, if \p save_fpstate is true, preserves the x87 floating-point and
 * MMX state on the
 * DR stack. Note that it is relatively expensive to save this state (on the
 * order of 200 cycles) and that it typically takes 512 bytes to store
 * it (see proc_fpstate_save_size()).
 * The last floating-point instruction address in the saved state is left in an
 * untranslated state (i.e., it may point into the code cache).  This optional
 * floating-point state preservation is specific to x87; floating-point values in
 * XMM, YMM, or ZMM registers, or any SIMD register on any non-x86 architecture, are
 * always preserved.  Thus, on ARM/AArch64, \p save_fpstate is ignored.
 *
 * DR does support translating a fault in an argument (e.g., an
 * argument that references application memory); such a fault will be
 * treated as an application exception.
 *
 * The clean call sequence will be optimized based on the runtime option
 * \ref op_cleancall "-opt_cleancall".
 *
 * For 64-bit, for purposes of reachability, this call is assumed to
 * be destined for encoding into DR's code cache-reachable memory region.
 * This includes the code cache as well as memory allocated with
 * dr_thread_alloc(), dr_global_alloc(), dr_nonheap_alloc(), or
 * dr_custom_alloc() with #DR_ALLOC_CACHE_REACHABLE.  The call used
 * here will be direct if it is reachable from those locations; if it
 * is not reachable, an indirect call through r11 will be used (with
 * r11's contents being clobbered).  Use dr_insert_clean_call_ex()
 * with #DR_CLEANCALL_INDIRECT to ensure reachability when encoding to
 * a location other than DR's regular code region.  See also
 * dr_insert_call_ex().
 *
 * \note The stack used to save state and call \p callee is limited to
 * 20KB by default; this can be changed with the -stack_size DR runtime
 * parameter.  This stack cannot be used to store state that persists
 * beyond \c callee's return point.
 *
 * \note This routine only supports passing arguments that are
 * integers or pointers of a size equal to the register size: i.e., no
 * floating-point, multimedia, or aggregate data types.
 * The routine also supports immediate integers that are smaller than
 * the register size, and for 64-bit mode registers or memory references that
 * are OPSZ_4.
 *
 * \note For 64-bit mode, passing arguments that use calling
 * convention registers (for Windows, RCX, RDX, R8, R9; for Linux,
 * RDI, RSI, RDX, RCX, R8 and R9) are supported but may incur
 * additional stack usage.
 *
 * \note For 64-bit mode, if a 32-bit immediate integer is specified as an
 * argument and it has its top bit set, we assume it is intended to be
 * sign-extended to 64-bits; otherwise we zero-extend it.
 *
 * \note For 64-bit mode, variable-sized argument operands may not work
 * properly.
 *
 * \note Arguments that reference sub-register portions of DR_REG_XSP are
 * not supported (full DR_REG_XSP is supported).
 */
void
dr_insert_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
                     bool save_fpstate, uint num_args, ...);

DR_API
/**
 * Identical to dr_insert_clean_call() except it takes in \p
 * save_flags which allows requests to not save certain state.  This
 * is intended for use at application call entry points or other
 * contexts where a client is comfortable making assumptions.  Keep in
 * mind that any register that is not saved will not be present in a
 * context obtained from dr_get_mcontext().
 */
void
dr_insert_clean_call_ex(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
                        dr_cleancall_save_t save_flags, uint num_args, ...);

/* Inserts a complete call to callee with the passed-in arguments, wrapped
 * by an app save and restore.
 * On x86, if \p save_fpstate is true, saves the x87 fp/mmx state.
 * On ARM/AArch64, \p save_fpstate is ignored.
 *
 * NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot via
 * dr_prepare_for_call(). We guarantee to clients that all other slots
 * (except the XAX mcontext slot) will remain untouched.
 *
 * NOTE : dr_insert_cbr_instrumentation has assumption about the clean call
 * instrumentation layout, changes to the clean call instrumentation may break
 * dr_insert_cbr_instrumentation.
 */
void
dr_insert_clean_call_ex_varg(void *drcontext, instrlist_t *ilist, instr_t *where,
                             void *callee, dr_cleancall_save_t save_flags, uint num_args,
                             opnd_t *args);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to set
 * up the passed-in parameters, make a call to \p callee, and clean up
 * the parameters.
 *
 * The callee must use the standard C calling convention that matches the
 * underlying 32-bit or 64-bit binary interface convention ("cdecl"). Other
 * calling conventions, such as "fastcall" and "stdcall", are not supported.
 *
 * This routine uses the existing stack.  In 64-bit mode, this routine assumes
 * that the stack pointer is currently 16-byte aligned.
 *
 * The application state is NOT saved or restored (use dr_prepare_for_call()
 * and dr_cleanup_after_call(), or replace this routine with dr_insert_clean_call()).
 * The parameter set-up may write to registers if the calling convention so
 * dictates.  The registers are NOT saved beforehand (to do so, use
 * dr_insert_clean_call()).
 *
 * It is up to the caller of this routine to preserve any caller-saved registers
 * that the callee might modify.
 *
 * DR does not support translating a fault in an argument.  For fault
 * transparency, the client must perform the translation (see
 * #dr_register_restore_state_event()), or use
 * #dr_insert_clean_call().
 *
 * For 64-bit, for purposes of reachability, this call is assumed to
 * be destined for encoding into DR's code cache-reachable memory region.
 * This includes the code cache as well as memory allocated with
 * dr_thread_alloc(), dr_global_alloc(), dr_nonheap_alloc(), or
 * dr_custom_alloc() with #DR_ALLOC_CACHE_REACHABLE.  The call used
 * here will be direct if it is reachable from those locations; if it
 * is not reachable, an indirect call through r11 will be used (with
 * r11's contents being clobbered).  Use dr_insert_call_ex() when
 * encoding to a location other than DR's regular code region.
 *
 * \note This routine only supports passing arguments that are
 * integers or pointers of a size equal to the register size: i.e., no
 * floating-point, multimedia, or aggregate data types.
 * The routine also supports immediate integers that are smaller than
 * the register size, and for 64-bit mode registers or memory references that
 * are OPSZ_4.
 *
 * \note For 64-bit mode, passing arguments that use calling
 * convention registers (for Windows, RCX, RDX, R8, R9; for Linux,
 * RDI, RSI, RDX, RCX, R8 and R9) are supported but may incur
 * additional stack usage.
 *
 * \note For 64-bit mode, if a 32-bit immediate integer is specified as an
 * argument and it has its top bit set, we assume it is intended to be
 * sign-extended to 64-bits; otherwise we zero-extend it.
 *
 * \note For 64-bit mode, variable-sized argument operands may not work
 * properly.
 *
 * \note Arguments that reference DR_REG_XSP are not supported in 64-bit mode.
 */
void
dr_insert_call(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
               uint num_args, ...);

DR_API
/**
 * Identical to dr_insert_call() except it takes in \p encode_pc
 * indicating roughly where the call sequence will be encoded.  If \p
 * callee is not reachable from \p encode_pc plus or minus one page,
 * an indirect call will be used instead of the direct call used by
 * dr_insert_call().  The indirect call overwrites the r11 register.
 *
 * \return true if the inserted call is direct and false if indirect.
 */
bool
dr_insert_call_ex(void *drcontext, instrlist_t *ilist, instr_t *where, byte *encode_pc,
                  void *callee, uint num_args, ...);

/* Not exported.  Currently used for ARM to avoid storing to %lr. */
void
dr_insert_call_noreturn(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
                        uint num_args, ...);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save state for a call.
 * Stores the application state information on the DR stack.
 * Returns the size of the data stored on the DR stack (in case the caller
 * needs to align the stack pointer).
 *
 * \warning On x86, this routine does NOT save the x87 floating-point or MMX state: to do
 * that the instrumentation routine should call proc_save_fpstate() to save and then
 * proc_restore_fpstate() to restore (or use dr_insert_clean_call()).
 *
 * \note The preparation modifies the DR_REG_XSP and DR_REG_XAX registers
 * (after saving them).  Use dr_insert_clean_call() instead if an
 * argument to the subsequent call that references DR_REG_XAX is
 * desired.
 *
 * \note The stack used to save the state is limited to
 * 20KB by default; this can be changed with the -stack_size DR runtime
 * parameter.  This stack cannot be used to store state that persists
 * beyond a single clean call, code cache execution, or probe callback
 * function execution.
 */
uint
dr_prepare_for_call(void *drcontext, instrlist_t *ilist, instr_t *instr);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore state
 * after a call.
 */
void
dr_cleanup_after_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                      uint sizeof_param_area);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the current
 * esp and switch to this thread's DR stack.
 * \note The DR stack is limited to 20KB by default; this can be changed with
 * the -stack_size DR runtime parameter.  This stack cannot be used to store
 * state that persists beyond a single clean call, code cache execution,
 * or probe callback function execution.
 */
void
dr_swap_to_clean_stack(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore into
 * esp the value saved by dr_swap_to_clean_stack().
 */
void
dr_restore_app_stack(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_API
/**
 * Calls the specified function \p func after switching to the DR stack
 * for the thread corresponding to \p drcontext.
 * Passes in 8 arguments.  Uses the C calling convention, so \p func will work
 * just fine even if if takes fewer than 8 args.
 * Swaps the stack back upon return and returns the value returned by \p func.
 *
 * On Windows, this routine does swap the TEB stack fields, avoiding
 * issues with fault handling on Windows 8.1.  This means there is no need
 * for the callee to use dr_switch_to_dr_state_ex() with DR_STATE_STACK_BOUNDS.
 */
void *
dr_call_on_clean_stack(void *drcontext, void *(*func)(void), void *arg1, void *arg2,
                       void *arg3, void *arg4, void *arg5, void *arg6, void *arg7,
                       void *arg8);

/* providing functionality of old -instr_calls and -instr_branches flags */
DR_API
/**
 * Assumes that \p instr is a near call.
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * two arguments:
 * -# address of call instruction (caller)
 * -# target address of call (callee)
 * \note Sets #DR_CLEANCALL_READS_APP_CONTEXT and #DR_CLEANCALL_WRITES_APP_CONTEXT.
 * Conditionally skipping the instrumentation inserted by this routine is not
 * supported (i.e., #DR_CLEANCALL_MULTIPATH is not supported here).
 */
void
dr_insert_call_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                               void *callee);

DR_API
/**
 * Assumes that \p instr is an indirect branch.
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * two arguments:
 * -# address of branch instruction
 * -# target address of branch
 * \note Only the address portion of a far indirect branch is considered.
 * \note \p scratch_slot must be <= dr_max_opnd_accessible_spill_slot(). \p
 * scratch_slot is used internally to this routine and will be clobbered.
 * \note Sets #DR_CLEANCALL_READS_APP_CONTEXT and #DR_CLEANCALL_WRITES_APP_CONTEXT.
 * Conditionally skipping the instrumentation inserted by this routine is not
 * supported (i.e., #DR_CLEANCALL_MULTIPATH is not supported here).
 */
/* If we re-enable -opt_speed (or -indcall2direct directly) we should add back:
 * \note This routine is not supported when the -opt_speed option is specified.
 */
void
dr_insert_mbr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee, dr_spill_slot_t scratch_slot);

DR_API
/**
 * Assumes that \p instr is a conditional branch
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * three arguments:
 * -# address of branch instruction
 * -# target address of branch
 * -# 0 if the branch is not taken, 1 if it is taken
 * \note Sets #DR_CLEANCALL_READS_APP_CONTEXT and #DR_CLEANCALL_WRITES_APP_CONTEXT.
 * Conditionally skipping the instrumentation inserted by this routine is not
 * supported (i.e., #DR_CLEANCALL_MULTIPATH is not supported here).
 */
void
dr_insert_cbr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee);

DR_API
/**
 * Assumes that \p instr is a conditional branch
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * four arguments:
 * -# address of branch instruction
 * -# target address of branch
 * -# fall-through address of branch
 * -# 0 if the branch is not taken, 1 if it is taken
 * -# user defined operand (e.g., TLS slot, immed value, register, etc.)
 * \note The user defined operand cannot use register ebx!
 * \note Sets #DR_CLEANCALL_READS_APP_CONTEXT and #DR_CLEANCALL_WRITES_APP_CONTEXT.
 * Conditionally skipping the instrumentation inserted by this routine is not
 * supported (i.e., #DR_CLEANCALL_MULTIPATH is not supported here).
 */
void
dr_insert_cbr_instrumentation_ex(void *drcontext, instrlist_t *ilist, instr_t *instr,
                                 void *callee, opnd_t user_data);

/* FIXME: will never see any ubrs! */
DR_API
/**
 * Assumes that \p instr is a direct, near, unconditional branch.
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * two arguments:
 * -# address of branch instruction
 * -# target address of branch
 *
 * \warning Basic block eliding is controlled by -max_elide_jmp.  If that
 * option is set to non-zero, ubrs may never be seen.
 * \note Sets #DR_CLEANCALL_READS_APP_CONTEXT and #DR_CLEANCALL_WRITES_APP_CONTEXT.
 * Conditionally skipping the instrumentation inserted by this routine is not
 * supported (i.e., #DR_CLEANCALL_MULTIPATH is not supported here).
 */
void
dr_insert_ubr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee);

DR_API
/**
 * Causes DynamoRIO to insert code that stores \p value into the
 * return address slot on the stack immediately after the original
 * value is read by the return instruction \p instr.
 * \p instr must be a return instruction or this routine will fail.
 *
 * On ARM, \p value is ignored and instead a value that is guaranteed
 * to not look like a return address is used.  This is for efficiency
 * reasons, as on ARM it would require an extra register spill in
 * order to write an arbitrary value.
 *
 * \note This is meant to make it easier to obtain efficient
 * callstacks by eliminating stale return addresses from prior stack
 * frames.  However, it is possible that writing to the application
 * stack could result in incorrect application behavior, so use this
 * at your own risk.
 *
 * \return whether successful.
 */
bool
dr_clobber_retaddr_after_read(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              ptr_uint_t value);

DR_API
/**
 * Returns true if the simd fields in dr_mcontext_t are valid xmm, ymm, or zmm values
 * (i.e., whether the underlying processor supports SSE).
 * \note If #DR_MC_MULTIMEDIA is not specified when calling dr_get_mcontext(),
 * the simd fields will not be filled in regardless of the return value
 * of this routine.
 */
bool
dr_mcontext_xmm_fields_valid(void);

DR_API
/**
 * Returns true if the simd fields in dr_mcontext_t are valid zmm values
 * (i.e., whether the underlying processor and OS support AVX-512 and AVX-512 code
 * is present).
 * \note If #DR_MC_MULTIMEDIA is not specified when calling dr_get_mcontext(),
 * the simd fields will not be filled in regardless of the return value
 * of this routine.
 */
bool
dr_mcontext_zmm_fields_valid(void);

/* dr_get_mcontext() needed for translating clean call arg errors */

DR_API
/**
 * Copies the fields of the current application machine context selected
 * by the \p flags field of \p context into \p context.
 *
 * This routine may only be called from:

 * - A clean call invoked by dr_insert_clean_call() or dr_prepare_for_call().  If
 *   register reservation code is in use (e.g., via the drreg extension library \ref
 *   page_drreg), dr_insert_clean_call_ex() must be used with its flags argument
 *   including #DR_CLEANCALL_READS_APP_CONTEXT (and possibly
 *   #DR_CLEANCALL_MULTIPATH)to ensure proper interaction with register reservations.
 * - A pre- or post-syscall event (dr_register_pre_syscall_event(),
 *   dr_register_post_syscall_event())
 * - Basic block or trace creation events (dr_register_bb_event(),
 *   dr_register_trace_event()), but for basic block creation only when the
 *   basic block callback parameters \p for_trace and \p translating are
 *   false, and for trace creation only when \p translating is false.
 * - A nudge callback (dr_register_nudge_event()) on Linux.
 *   (On Windows nudges happen in separate dedicated threads.)
 * - A thread or process exit event (dr_register_thread_exit_event(),
 *   dr_register_exit_event())
 * - A thread init event (dr_register_thread_init_event()) for all but
 *   the initial thread.
 * - A kernel transfer event (dr_register_kernel_xfer_event()).  Here the obtained
 *   context is the target context of the transfer, not the source (about to be
 *   changed) context.  For Windows system call event types #DR_XFER_CONTINUE and
 *   #DR_XFER_SET_CONTEXT_THREAD, only the portions of the context selected by the
 *   application are available.  The \p flags field of \p context is adjusted to
 *   reflect which fields were returned.  Given the disparity in how Ebp/Rbp is
 *   handled (in #DR_MC_INTEGER but in CONTEXT_CONTROL), clients that care about that
 *   register are better off using system call events instead of kernel transfer events
 *   to take actions on these two system calls.
 *
 * Even when #DR_MC_CONTROL is specified, does NOT copy the pc field,
 * except for system call events, when it will point at the
 * post-syscall address, and kernel transfer events, when it will point to the
 * target pc.
 *
 * Returns false if called from the init event or the initial thread's
 * init event; returns true otherwise (cannot distinguish whether the
 * caller is in a clean call so it is up to the caller to ensure it is
 * used properly).
 *
 * The size field of \p context must be set to the size of the
 * structure as known at compile time.  If the size field is invalid,
 * this routine will return false.
 *
 * The flags field of \p context must be set to the desired amount of
 * information using the dr_mcontext_flags_t values.  Asking for
 * multimedia registers incurs a higher performance cost.  An invalid
 * flags value will return false.
 *
 * \note NUM_SIMD_SLOTS in the dr_mcontext_t.xmm array are filled in,
 * but only if dr_mcontext_xmm_fields_valid() returns true and
 * #DR_MC_MULTIMEDIA is set in the flags field.
 *
 * \note The context is the context saved at the dr_insert_clean_call() or
 * dr_prepare_for_call() points.  It does not correct for any registers saved
 * with dr_save_reg().  To access registers saved with dr_save_reg() from a
 * clean call use dr_read_saved_reg().
 *
 * \note System data structures are swapped to private versions prior to
 * invoking clean calls or client events.  Use dr_switch_to_app_state()
 * to examine the application version of system state.
 */
bool
dr_get_mcontext(void *drcontext, dr_mcontext_t *context);

DR_API
/**
 * Sets the fields of the application machine context selected by the
 * flags field of \p context to the values in \p context.
 *
 * This routine may only be called from:
 * - A clean call invoked by dr_insert_clean_call() or dr_prepare_for_call().  If
 *   register reservation code is in use (e.g., via the drreg extension library \ref
 *   page_drreg), dr_insert_clean_call_ex() must be used with its flags argument
 *   including #DR_CLEANCALL_WRITES_APP_CONTEXT (and possibly
 *   #DR_CLEANCALL_MULTIPATH) to ensure proper interaction with register
 *   reservations.
 * - A pre- or post-syscall event (dr_register_pre_syscall_event(),
 *   dr_register_post_syscall_event())
 *   dr_register_thread_exit_event())
 * - A kernel transfer event (dr_register_kernel_xfer_event()) other than
 *   #DR_XFER_CALLBACK_RETURN.  Here the modified context is the target context of
 *   the transfer, not the source (about to be changed) context.  For Windows system
 *   call event types #DR_XFER_CONTINUE and #DR_XFER_SET_CONTEXT_THREAD, only the
 *   portions of the context selected by the application can be changed.  The \p
 *   flags field of \p context is adjusted to reflect which fields these are.  Given
 *   the disparity in how Ebp/Rbp is handled (in #DR_MC_INTEGER but in
 *   CONTEXT_CONTROL), clients that care about that register are better off using
 *   system call events instead of kernel transfer events to take actions on these
 *   two system calls.  - Basic block or trace creation events
 *   (dr_register_bb_event(), dr_register_trace_event()), but for basic block
 *   creation only when the basic block callback parameters \p for_trace and \p
 *   translating are false, and for trace creation only when \p translating is false.
 *
 * Ignores the pc field, except for kernel transfer events.
 *
 * If the size field of \p context is invalid, this routine will
 * return false.  A dr_mcontext_t obtained from DR will have the size field set.
 *
 * The flags field of \p context must be set to select the desired
 * fields for copying, using the dr_mcontext_flags_t values.  Asking
 * to copy multimedia registers incurs a higher performance cost.  An
 * invalid flags value will return false.
 *
 * \return whether successful.
 *
 * \note The xmm fields are only set for processes where the underlying
 * processor supports them (and when DR_MC_MULTIMEDIA is set in the flags field).
 * For dr_insert_clean_call() that requested \p
 * save_fpstate, the xmm values set here override that saved state.  Use
 * dr_mcontext_xmm_fields_valid() to determine whether the xmm fields are valid.
 */
bool
dr_set_mcontext(void *drcontext, dr_mcontext_t *context);

/* FIXME - combine with dr_set_mcontext()?  Implementation wise it's nice to split the
 * two since handling the pc with dr_set_mcontext() would complicate the clean call
 * handling. But perhaps would be nicer from an interface perspective to combine them. */
DR_API
/**
 * Immediately resumes application execution from a clean call out of the cache (see
 * dr_insert_clean_call() or dr_prepare_for_call()) or an exception event with the
 * state specified in \p mcontext (including pc, and including the xmm fields
 * that are valid according to dr_mcontext_xmm_fields_valid()).
 * The flags field of \p context must contain DR_MC_ALL; using a partial set
 * of fields is not suported.
 *
 * For 32-bit ARM, be sure to use dr_app_pc_as_jump_target() to properly set the ISA
 * mode for the continuation pc if it was obtained from instr_get_app_pc() or a
 * similar source rather than from dr_get_proc_address().  This will set the least
 * significant bit of the mcontext pc field to 1 when in Thumb mode
 * (#DR_ISA_ARM_THUMB), \ref sec_thumb for more information.
 *
 * \note dr_get_mcontext() can be used to get the register state (except pc)
 * saved in dr_insert_clean_call() or dr_prepare_for_call().
 *
 * \note If x87 floating point state was saved by dr_prepare_for_call() or
 * dr_insert_clean_call() it is not restored (other than the valid xmm
 * fields according to dr_mcontext_xmm_fields_valid(), if
 * #DR_MC_MULTIMEDIA is specified in the flags field).  The caller
 * should instead manually save and restore the floating point state
 * with proc_save_fpstate() and proc_restore_fpstate() if necessary.
 *
 * \note If the caller wishes to set any other state (such as xmm
 * registers that are not part of the mcontext) they may do so by just
 * setting that state in the current thread before making this call.
 * To set system data structures, use dr_switch_to_app_state(), make
 * the changes, and then switch back with dr_switch_to_dr_state()
 * before calling this routine.
 *
 * \note This routine may only be called from a clean call from the cache. It can not be
 * called from any registered event callback except the exception event
 * (dr_register_exception_event()).  From a signal event callback, use the
 * #DR_SIGNAL_REDIRECT return value rather than calling this routine.
 *
 * \note If control is being redirected to a new pc (determined using drsyms,
 * dr_get_proc_address, or any other way) ensure that the app state (such as
 * the register values) are set as expected by the transfer point.
 *
 * \return false if unsuccessful; if successful, does not return.
 */
bool
dr_redirect_execution(dr_mcontext_t *context);

/** Flags to request non-default preservation of state in a clean call */
#define SPILL_SLOT_REDIRECT_NATIVE_TGT SPILL_SLOT_1

DR_API
/**
 * Returns the target to use for a native context transfer to a target
 * application address.
 *
 * Normally, redirection is performed from a client context in a clean
 * call or event callback by invoking dr_redirect_execution().  In
 * some circumstances, redirection from an application (or "native")
 * context is desirable without creating an application control
 * transfer in a basic block.
 *
 * To accomplish such a redirection, store the target application
 * address in #SPILL_SLOT_REDIRECT_NATIVE_TGT by calling
 * dr_write_saved_reg().  Set up any other application state as
 * desired directly in the current machine context.  Then jump to the
 * target returned by this routine.  By default, the target is global
 * and can be cached globally.  However, if traces are thread-private,
 * or if traces are disabled and basic blocks are thread-private,
 * there will be a separate target per \p drcontext.
 *
 * If a basic block is exited via such a redirection, the block should
 * be emitted with the flag DR_EMIT_MUST_END_TRACE in order to avoid
 * trace building errors.
 *
 * For ARM, the address returned by this routine has its least significant
 * bit set to 1 if the target is Thumb.
 *
 * Returns null on error.
 */
byte *
dr_redirect_native_target(void *drcontext);

#ifdef WINDOWS
DR_API
/**
 * Copies the machine state in \p src into \p dst.  Sets the \p
 * ContextFlags field of \p dst to reflect the \p flags field of \p
 * src.  However, CONTEXT_CONTROL includes Ebp/Rbp, while that's under
 * #DR_MC_INTEGER, so we recommend always setting both #DR_MC_INTEGER
 * and #DR_MC_CONTROL when calling this routine.
 *
 * It is up to the caller to ensure that \p dst is allocated and
 * initialized properly in order to contain multimedia processor
 * state, if #DR_MC_MULTIMEDIA is set in the \p flags field of \p src.
 *
 * The current segment register values are filled in under the assumption
 * that this context is for the calling thread.
 *
 * \note floating-point values are not filled in for \p dst.
 * \note Windows only.
 *
 * \return false if unsuccessful; if successful, does not return.
 */
bool
dr_mcontext_to_context(CONTEXT *dst, dr_mcontext_t *src);
#endif

DR_API
/**
 * Create meta instructions for storing pointer-size integer \p val to \p dst,
 * and then insert them into \p ilist prior to \p where.
 * Pointers to the first and last created meta instructions are returned
 * in \p first and \p last, unless only one meta instruction is created,
 * in which case NULL is returned in last.
 * If the instruction is a no-op (when dst is the zero register on AArch64)
 * then no instructions are created and NULL is returned in first and last.
 */
void
instrlist_insert_mov_immed_ptrsz(void *drcontext, ptr_int_t val, opnd_t dst,
                                 instrlist_t *ilist, instr_t *where, instr_t **first OUT,
                                 instr_t **last OUT);

DR_API
/**
 * Create meta instructions for pushing pointer-size integer \p val on the stack,
 * and then insert them into \p ilist prior to \p where.
 * Pointers to the first and last created meta instructions are returned
 * in \p first and \p last, unless only one meta instruction is created,
 * in which case NULL is returned in last.
 */
void
instrlist_insert_push_immed_ptrsz(void *drcontext, ptr_int_t val, instrlist_t *ilist,
                                  instr_t *where, instr_t **first OUT,
                                  instr_t **last OUT);

DR_API
/**
 * Create meta instructions for storing the address of \p src_inst to \p dst,
 * and then insert them into \p ilist prior to \p where.
 * The \p encode_estimate parameter, used only for 64-bit mode,
 * indicates whether the final address of \p src_inst, when it is
 * encoded later, will fit in 32 bits or needs 64 bits.
 * If the encoding will be in DynamoRIO's code cache, pass NULL.
 * If the final encoding location is unknown, pass a high address to be on
 * the safe side.
 * Pointers to the first and last created meta instructions are returned
 * in \p first and \p last, unless only one meta instruction is created,
 * in which case NULL is returned in last.
 * If the instruction is a no-op (when dst is the zero register on AArch64)
 * then no instructions are created and NULL is returned in first and last.
 */
void
instrlist_insert_mov_instr_addr(void *drcontext, instr_t *src_inst, byte *encode_estimate,
                                opnd_t dst, instrlist_t *ilist, instr_t *where,
                                instr_t **first OUT, instr_t **last OUT);

DR_API
/**
 * Create meta instructions for pushing the address of \p src_inst on the stack,
 * and then insert them into \p ilist prior to \p where.
 * The \p encode_estimate parameter, used only for 64-bit mode,
 * indicates whether the final address of \p src_inst, when it is
 * encoded later, will fit in 32 bits or needs 64 bits.
 * If the encoding will be in DynamoRIO's code cache, pass NULL.
 * If the final encoding location is unknown, pass a high address to be on
 * the safe side.
 * Pointers to the first and last created meta instructions are returned
 * in \p first and \p last, unless only one meta instruction is created,
 * in which case NULL is returned in last.
 */
void
instrlist_insert_push_instr_addr(void *drcontext, instr_t *src_inst,
                                 byte *encode_estimate, instrlist_t *ilist,
                                 instr_t *where, instr_t **first OUT, instr_t **last OUT);

DR_API
/**
 * Returns the register that is stolen and used by DynamoRIO.
 * Reference \ref sec_reg_stolen for more information.
 */
reg_id_t
dr_get_stolen_reg(void);

DR_API
/**
 * Insert code to get the application value of the register stolen by DynamoRIO
 * into register \p reg.
 * Reference \ref sec_reg_stolen for more information.
 *
 * \return whether successful.
 *
 * \note ARM-only
 */
bool
dr_insert_get_stolen_reg_value(void *drcontext, instrlist_t *ilist, instr_t *instr,
                               reg_id_t reg);

DR_API
/**
 * Insert code to set the value of register \p reg as the application value of
 * the register stolen by DynamoRIO
 * Reference \ref sec_reg_stolen for more information.
 *
 * \return whether successful.
 *
 * \note ARM-only
 */
bool
dr_insert_set_stolen_reg_value(void *drcontext, instrlist_t *ilist, instr_t *instr,
                               reg_id_t reg);

DR_API
/**
 * Removes all OP_it instructions from \p ilist without changing the
 * instructions that were inside each IT block.  This is intended to
 * be paired with dr_insert_it_instrs(), where a client's examination
 * of the application instruction list and insertion of
 * instrumentation occurs in between the two calls and thus does not
 * have to worry about groups of instructions that cannot be separated
 * or changed.  The resulting predicated instructions are not
 * encodable in Thumb mode (#DR_ISA_ARM_THUMB): dr_insert_it_instrs()
 * must be called before encoding.
 *
 * \return the number of OP_it instructions removed; -1 on error.
 *
 * \note ARM-only
 */
int
dr_remove_it_instrs(void *drcontext, instrlist_t *ilist);

DR_API
/**
 * Inserts enough OP_it instructions with proper parameters into \p
 * ilist to make all predicated instructions in \p ilist legal in
 * Thumb mode (#DR_ISA_ARM_THUMB).  Treats predicated app and tool
 * instructions identically, but marks inserted OP_it instructions as
 * app instructions (see instr_set_app()).
 *
 * \return the number of OP_it instructions inserted; -1 on error.
 *
 * \note ARM-only
 */
int
dr_insert_it_instrs(void *drcontext, instrlist_t *ilist);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the
 * x87 floating point state into the 16-byte-aligned buffer referred to by
 * \p buf, which must be 512 bytes for processors with the FXSR
 * feature, and 108 bytes for those without (where this routine does
 * not support 16-bit operand sizing).  \p buf should have size of
 * OPSZ_512; this routine will automatically adjust it to OPSZ_108 if
 * necessary.  \note proc_fpstate_save_size() can be used to determine
 * the particular size needed.
 *
 * When the FXSR feature is present, the fxsave format matches the bitwidth
 * of the ISA mode of the current thread (see dr_get_isa_mode()).
 *
 * The last floating-point instruction address is left in an
 * untranslated state (i.e., it may point into the code cache).
 */
void
dr_insert_save_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore the
 * x87 floating point state from the 16-byte-aligned buffer referred to by
 * buf, which must be 512 bytes for processors with the FXSR feature,
 * and 108 bytes for those without (where this routine does not
 * support 16-bit operand sizing).  \p buf should have size of
 * OPSZ_512; this routine will automatically adjust it to OPSZ_108 if
 * necessary.  \note proc_fpstate_save_size() can be used to determine
 * the particular size needed.
 *
 * When the FXSR feature is present, the fxsave format matches the bitwidth
 * of the ISA mode of the current thread (see dr_get_isa_mode()).
 */
void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where,
                          opnd_t buf);

DR_API
/**
 * Insert code to get the segment base address pointed to by seg into
 * register reg. In Linux, it is only supported with -mangle_app_seg option.
 * In Windows, it only supports getting base address of the TLS segment.
 *
 * \return whether successful.
 */
bool
dr_insert_get_seg_base(void *drcontext, instrlist_t *ilist, instr_t *instr, reg_id_t seg,
                       reg_id_t reg);

#endif /* _DR_IR_UTILS_H_ */
