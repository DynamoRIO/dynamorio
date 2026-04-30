#ifndef _DR_IR_ISA_FEATURES_X86_H_
#define _DR_IR_ISA_FEATURES_X86_H_ 1

/****************************************************************************
 * ISA_FEATURE
 */
/**
 * @file dr_ir_isa_features_x86.h
 * @brief ISA features constants for X86.
 */
/** ISA feature constants returned by instr_get_isa_feature(). */
enum {
    ISA_FEAT_INVALID = 0, /**< Invalid ISA feature. Reserved value. */
    ISA_FEAT_UNKNOWN = 1, /**< Unknown ISA feature. Reserved value. */

    /* TODO i#7842: Add the actual ISA_FEAT_ constants once instr_get_isa_feature() is
     * implemented.
     */
};

/****************************************************************************/

#endif /* _DR_IR_ISA_FEATURES_X86_H_ */
