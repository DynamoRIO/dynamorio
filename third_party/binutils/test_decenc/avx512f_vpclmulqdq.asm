
#undef FUNCNAME
#define FUNCNAME test_avx512f_vpclmulqdq_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(62) RAW(f3) RAW(65) RAW(48) RAW(44) RAW(c9) RAW(ab)
        RAW(62) RAW(f3) RAW(65) RAW(48) RAW(44) RAW(8c) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(62) RAW(f3) RAW(65) RAW(48) RAW(44) RAW(4a) RAW(7f)
        RAW(7b)
        RAW(62) RAW(f3) RAW(6d) RAW(48) RAW(44) RAW(d2) RAW(ab)
        RAW(62) RAW(f3) RAW(6d) RAW(48) RAW(44) RAW(94) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(62) RAW(f3) RAW(6d) RAW(48) RAW(44) RAW(52) RAW(7f)
        RAW(7b)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
