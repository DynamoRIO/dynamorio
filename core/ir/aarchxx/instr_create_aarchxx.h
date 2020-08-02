/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc. All rights reserved.
 * **********************************************************/

#ifndef INSTR_CREATE_AARCHXX_H
#define INSTR_CREATE_AARCHXX_H 1

#include "../instr_create_shared.h"
#include "instr.h"

/* DR_API EXPORT TOFILE dr_ir_macros_aarchxx.h */
/* DR_API EXPORT BEGIN */

/** Immediate values for INSTR_CREATE_dmb(). */
enum {
    DR_DMB_OSHLD = 1, /**< DMB Outer Shareable - Loads. */
    DR_DMB_OSHST = 2, /**< DMB Outer Shareable - Stores. */
    DR_DMB_OSH = 3,   /**< DMB Outer Shareable - Loads and Stores. */
    DR_DMB_NSHLD = 5, /**< DMB Non Shareable - Loads. */
    DR_DMB_NSHST = 6, /**< DMB Non Shareable - Stores. */
    DR_DMB_NSH = 7,   /**< DMB Non Shareable - Loads and Stores. */
    DR_DMB_ISHLD = 9, /**< DMB Inner Shareable - Loads. */
    DR_DMB_ISHST = 10 /**< DMB Inner Shareable - Stores. */,
    DR_DMB_ISH = 11, /**< DMB Inner Shareable - Loads and Stores. */
    DR_DMB_LD = 13,  /**< DMB Full System - Loads. */
    DR_DMB_ST = 14,  /**< DMB Full System - Stores. */
    DR_DMB_SY = 15,  /**< DMB Full System - Loads and Stores. */
};

/** @name Signature: (imm) */
/* @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param imm barrier operation #DR_DMB_OSHLD.
 */
#define INSTR_CREATE_dmb(dc, imm) instr_create_0dst_1src((dc), OP_dmb, (imm))
/* @} */ /* end doxygen group */

/* DR_API EXPORT END */

#endif /* INSTR_CREATE_H */

