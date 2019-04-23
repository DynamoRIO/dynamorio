
#undef FUNCNAME
#define FUNCNAME test_avx512f_vpclmulqdq_wig_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(f3) RAW(75) RAW(48) RAW(44) RAW(f2) RAW(ab)
        RAW(62) RAW(f3) RAW(75) RAW(48) RAW(44) RAW(b4) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(62) RAW(f3) RAW(75) RAW(48) RAW(44) RAW(72) RAW(7f)
        RAW(7b)
        RAW(62) RAW(f3) RAW(75) RAW(48) RAW(44) RAW(ea) RAW(ab)
        RAW(62) RAW(f3) RAW(75) RAW(48) RAW(44) RAW(ac) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(62) RAW(f3) RAW(75) RAW(48) RAW(44) RAW(6a) RAW(7f)
        RAW(7b)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
