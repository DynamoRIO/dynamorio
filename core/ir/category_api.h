#ifndef _DR_IR_CATEGORIES_H_
#define _DR_IR_CATEGORIES_H_ 1

typedef enum _dr_instr_category_type_t {
    DR_INSTR_CATEGORY_NONE        = 0x0,
    DR_INSTR_CATEGORY_MATH_INT    = 0x1,
    DR_INSTR_CATEGORY_MATH_FLOAT  = 0x2,
    DR_INSTR_CATEGORY_LOAD        = 0x4,
    DR_INSTR_CATEGORY_STORE       = 0x8,
    DR_INSTR_CATEGORY_BRANCH      = 0x10,
    DR_INSTR_CATEGORY_SIMD        = 0x20,
    DR_INSTR_CATEGORY_OTHER       = 0x40
} dr_instr_category_type_t;

#endif /* _DR_IR_CATEGORIES_H_ */