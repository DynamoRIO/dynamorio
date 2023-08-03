#ifndef _DR_IR_CATEGORIES_H_
#define _DR_IR_CATEGORIES_H_ 1

typedef enum _dr_instr_category_type_t {
    DR_INSTR_CATEGORY_UNCATEGORIZED = 0x0, /* Uncategorised. */
    DR_INSTR_CATEGORY_MATH_INT = 0x1,      /* Integer arithmetic operations */
    DR_INSTR_CATEGORY_MATH_FLOAT = 0x2,    /* Floating-Point arithmetic operations */
    DR_INSTR_CATEGORY_LOAD = 0x4,          /* Loads */
    DR_INSTR_CATEGORY_STORE = 0x8,         /* Stores */
    DR_INSTR_CATEGORY_BRANCH = 0x10,       /* Branches */
    DR_INSTR_CATEGORY_SIMD = 0x20,         /* Operations with vector registers (SIMD) */
    DR_INSTR_CATEGORY_OTHER = 0x40         /* Other types of instructions */
} dr_instr_category_type_t;

#endif /* _DR_IR_CATEGORIES_H_ */
