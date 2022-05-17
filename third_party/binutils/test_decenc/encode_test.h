#ifdef X64

#    define PREFIX_EVEX_z 0x000800000 // see i#5488
#    define R(reg) opnd_create_reg(DR_REG_##reg)
#    define Xh(reg) opnd_create_reg_partial(DR_REG_##reg, OPSZ_8)
#    define Yh(reg) opnd_create_reg_partial(DR_REG_##reg, OPSZ_16)
#    define Zh(reg) opnd_create_reg_partial(DR_REG_##reg, OPSZ_32)
#    define M(b, i, s, d, sz) opnd_create_base_disp(DR_REG_##b, DR_REG_##i, s, d, sz)

enum {
    Z = 1,
};

#    define HANDLE_FLAGS(instr, flags)                       \
        do {                                                 \
            if (flags & Z) {                                 \
                instr_set_prefix_flag(instr, PREFIX_EVEX_z); \
            }                                                \
        } while (0)

//
#    if VERBOSE
#        define PRINT_TEST_NAME(name) // add fprintf
#    else
#        define PRINT_TEST_NAME(x)
#    endif

#    define ENC3(name, opc, flags, arg1, arg2, arg3)                   \
        do {                                                           \
            instr_t *instr = INSTR_CREATE_##opc(dc, arg1, arg2, arg3); \
            PRINT_TEST_NAME(name);                                     \
            HANDLE_FLAGS(instr, flags);                                \
            test_instr_encode(dc, instr, sizeof(name));                \
            if (memcmp(buf, name, sizeof(name))) {                     \
                dr_printf("memcmp mismatch\n");                        \
            }                                                          \
        } while (0)

#    define ENC4(name, opc, flags, arg1, arg2, arg3, arg4)                   \
        do {                                                                 \
            instr_t *instr = INSTR_CREATE_##opc(dc, arg1, arg2, arg3, arg4); \
            PRINT_TEST_NAME(name);                                           \
            HANDLE_FLAGS(instr, flags);                                      \
            test_instr_encode(dc, instr, sizeof(name));                      \
            if (memcmp(buf, name, sizeof(name))) {                           \
                dr_printf("memcmp mismatch\n");                              \
            }                                                                \
        } while (0)

#endif
