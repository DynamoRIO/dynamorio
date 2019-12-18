/* **********************************************************
 * Copyright (c) 2010-2018 Google, Inc.   All rights reserved.
 * **********************************************************/

/* drutil: DynamoRIO Instrumentation Utilities
 * Derived from Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* DynamoRIO Instrumentation Utilities Extension */

#ifndef _DRUTIL_H_
#define _DRUTIL_H_ 1

/**
 * @file drutil.h
 * @brief Header for DynamoRIO Instrumentation Utilities Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drutil Instrumentation Utilities
 */
/*@{*/ /* begin doxygen group */

/***************************************************************************
 * INIT
 */

DR_EXPORT
/**
 * Initializes the drutil extension.  Must be called prior to any of the
 * other routines.  Can be called multiple times (by separate components,
 * normally) but each call must be paired with a corresponding call to
 * drutil_exit().
 *
 * \return whether successful.
 */
bool
drutil_init(void);

DR_EXPORT
/**
 * Cleans up the drutil extension.
 */
void
drutil_exit(void);

/***************************************************************************
 * MEMORY TRACING
 */

DR_EXPORT
/**
 * Inserts instructions prior to \p where in \p bb that determine and
 * store the memory address referred to by \p memref into the register
 * \p dst.  May clobber the register \p scratch.  Supports far memory
 * references. For far memory references via DS and ES, we assume that
 * the segment base is 0.
 *
 * All registers used in \p memref must hold their original
 * application values in order for the proper address to be computed
 * into \p dst.  The \p dst register may overlap with the registers
 * used in \p memref.  On ARM, \p scratch must be different from those used
 * in \p memref (as well as from \p dst).
 * On x86, \p scratch will not be used unless \p memref is a far reference
 * that either uses \p dst or is a base-disp with both a base and an index,
 * or \p memref is a reference in the #OP_xlat instruction.
 *
 * To obtain each memory address referenced in a single-instruction
 * string loop, use drutil_expand_rep_string() to transform such loops
 * into regular loops containing (non-loop) string instructions.
 *
 * \return whether successful.
 */
bool
drutil_insert_get_mem_addr(void *drcontext, instrlist_t *bb, instr_t *where,
                           opnd_t memref, reg_id_t dst, reg_id_t scratch);

DR_EXPORT
/**
 * Identical to drutil_insert_get_mem_addr() except it returns in the optional
 * OUT parameter \p scratch_used whether or not \p scratch was written to.
 */
bool
drutil_insert_get_mem_addr_ex(void *drcontext, instrlist_t *bb, instr_t *where,
                              opnd_t memref, reg_id_t dst, reg_id_t scratch,
                              OUT bool *scratch_used);

DR_EXPORT
/**
 * Returns the size of the memory reference \p memref in bytes.
 * To handle OP_enter, requires the containing instruction \p inst
 * to be passed in.
 * For single-instruction string loops, returns the size referenced
 * by each iteration.
 *
 * If the instruction is part of the xsave family of instructions, this
 * returns an incomplete computation of the xsave instruction's written
 * xsave area's size. Specifically, it
 *
 * - Ignores the user state mask components set in edx:eax, because they are
 *   dynamic values. The real output size of xsave depends on the instruction's
 *   user state mask AND the user state mask as supported by the CPU based on
 *   the XCR0 control register.
 * - Ignores supervisor state component PT
 *   (enabled/disabled by user state component mask bit 8).
 * - Ignores the user state component PKRU state
 *   (enabled/disabled by user state component mask bit 9).
 * - Ignores the xsaveopt flavor of xsave.
 * - Ignores the xsavec flavor of xsave (compacted format).
 *
 * It computes the expected size for the standard format of the x87 user state
 * component (enabled/disabled by user state component mask bit 0), the SSE user
 * state component (bit 1), the AVX user state component (bit 2), the MPX user
 * state components (bit 2 and 3) and the AVX-512 user state component (bit 7).
 */
uint
drutil_opnd_mem_size_in_bytes(opnd_t memref, instr_t *inst);

DR_EXPORT
/**
 * Expands single-instruction string loops (those using the \p rep or
 * \p repne prefixes) into regular loops to simplify memory usage
 * analysis.  This is accomplished by arranging for each
 * single-instruction string loop to occupy a basic block by itself
 * (by truncating the prior block before the loop, and truncating
 * instructions after the loop) and then exanding it into a
 * multi-instruction loop.
 *
 * WARNING: The added multi-instruction loop contains several
 * control-transfer instructions and is not straight-line code, which
 * can complicate subsequent analysis routines.
 *
 * WARNING: The added instructions have translations that are in the
 * middle of the original string loop instruction.  This is to prevent
 * passes that match exact addresses from having multiple hits and
 * doing something like inserting 6 clean calls.
 *
 * WARNING: The added instructions include a jecxz instruction which
 * will not be transformed into a 32-bit-reach instruction: thus,
 * excessive added instrumentation may result in a reachability problem.
 *
 * The client must use the \p drmgr Extension to order its
 * instrumentation in order to use this function.  This function must
 * be called from the application-to-application ("app2app") stage
 * (see drmgr_register_bb_app2app_event()).
 *
 * This transformation is deterministic, so the caller can return
 * DR_EMIT_DEFAULT from its event.
 *
 * \return whether successful.
 */
bool
drutil_expand_rep_string(void *drcontext, instrlist_t *bb);

DR_EXPORT
/**
 * Identical to drutil_expand_rep_string() but returns additional information.
 *
 * @param[in]  drcontext   The opaque context
 * @param[in]  bb          Instruction list passed to the app2app event
 * @param[out] expanded    Whether any expansion occurred
 * @param[out] stringop    The string instruction in the expanded loop
 *
 * \return whether successful.
 */
bool
drutil_expand_rep_string_ex(void *drcontext, instrlist_t *bb, OUT bool *expanded,
                            OUT instr_t **stringop);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRUTIL_H_ */
