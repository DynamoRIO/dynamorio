
#undef FUNCNAME
#define FUNCNAME test_x86_64_vpclmulqdq_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        /* FIXME i#1312: Support AVX-512. */
        /* RAW(62) RAW(03) RAW(15) RAW(20) RAW(44) RAW(f4) RAW(ab) */
        /* RAW(62) RAW(23) RAW(15) RAW(20) RAW(44) RAW(b4) RAW(f0) */
        RAW(24) RAW(01) RAW(00) RAW(00) RAW(7b)
        RAW(62) RAW(63) RAW(15) RAW(20) RAW(44) RAW(72) RAW(7f)
        RAW(7b)
        /* RAW(62) RAW(03) RAW(15) RAW(20) RAW(44) RAW(f4) RAW(ab) */
        /* RAW(62) RAW(23) RAW(15) RAW(20) RAW(44) RAW(b4) RAW(f0) */
        RAW(34) RAW(12) RAW(00) RAW(00) RAW(7b)
        /* RAW(62) RAW(63) RAW(15) RAW(20) RAW(44) RAW(72) RAW(7f) */
        /* RAW(7b) */
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
