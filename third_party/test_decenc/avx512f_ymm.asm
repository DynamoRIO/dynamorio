
#undef FUNCNAME
#define FUNCNAME test_avx512f_ymm_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(f2) RAW(7d) RAW(48) RAW(31) RAW(c0)
        RAW(62) RAW(f2) RAW(7d) RAW(48) RAW(33) RAW(c0)
        RAW(62) RAW(f1) RAW(7c) RAW(48) RAW(5a) RAW(c0)
        RAW(62) RAW(f1) RAW(fd) RAW(48) RAW(5a) RAW(c0)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
