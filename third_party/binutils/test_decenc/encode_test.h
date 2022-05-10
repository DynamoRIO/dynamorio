#ifdef X64

enum {
    ENCODE_FLAG_SET_DST_SIZE_HALF = 1,
    ENCODE_FLAG_Z = 2,
};

#        define ASSERT(x)                                                       \
            ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s\n", \
                                         __FILE__, __LINE__, #x),               \
                              dr_abort(), 0)                                    \
                           : 0))

#        define HANDLE_FLAGS(instr, flags)                       \
            do {                                                 \
                if (flags & ENCODE_FLAG_Z) {                     \
                    instr_set_prefix_flag(instr, PREFIX_EVEX_z); \
                }                                                \
                if (flags & ENCODE_FLAG_SET_DST_SIZE_HALF) {     \
                    opnd_t dst = instr_get_dst(instr, 0);        \
                    if (opnd_get_size(dst) == OPSZ_64)           \
                        opnd_set_size(&dst, OPSZ_32);            \
                    else if (opnd_get_size(dst) == OPSZ_32)      \
                        opnd_set_size(&dst, OPSZ_16);            \
                    else if (opnd_get_size(dst) == OPSZ_16)      \
                        opnd_set_size(&dst, OPSZ_8);             \
                    instr_set_dst(instr, 0, dst);                \
                }                                                \
            } while (0)

//
#        if VERBOSE
#            define PRINT_TEST_NAME(name) // add fprintf
#        else
#            define PRINT_TEST_NAME(x)
#        endif

#    define ENCODE_TEST_3args(name, opc, flags, arg1, arg2, arg3)      \
        do {                                                           \
            instr_t *instr = INSTR_CREATE_##opc(dc, arg1, arg2, arg3); \
            PRINT_TEST_NAME(name);                                     \
            HANDLE_FLAGS(instr, flags);                                \
            test_instr_encode(dc, instr, sizeof(name));                \
            ASSERT(!memcmp(buf, name, sizeof(name)));                  \
        } while (0)

#    define ENCODE_TEST_4args(name, opc, flags, arg1, arg2, arg3, arg4)      \
        do {                                                                 \
            instr_t *instr = INSTR_CREATE_##opc(dc, arg1, arg2, arg3, arg4); \
            PRINT_TEST_NAME(name);                                           \
            HANDLE_FLAGS(instr, flags);                                      \
            test_instr_encode(dc, instr, sizeof(name));                      \
            ASSERT(!memcmp(buf, name, sizeof(name)));                        \
        } while (0)

#endif
