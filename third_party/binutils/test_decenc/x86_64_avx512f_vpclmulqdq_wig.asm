
#undef FUNCNAME
#define FUNCNAME test_x86_64_avx512f_vpclmulqdq_wig_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(a3) RAW(5d) RAW(40) RAW(44) RAW(f3) RAW(ab)
        RAW(62) RAW(a3) RAW(5d) RAW(40) RAW(44) RAW(b4) RAW(f0)
        RAW(23) RAW(01) RAW(00) RAW(00) RAW(7b)
        RAW(62) RAW(e3) RAW(5d) RAW(40) RAW(44) RAW(72) RAW(7f)
        RAW(7b)
        RAW(62) RAW(23) RAW(1d) RAW(40) RAW(44) RAW(ef) RAW(ab)
        RAW(62) RAW(23) RAW(1d) RAW(40) RAW(44) RAW(ac) RAW(f0)
        RAW(34) RAW(12) RAW(00) RAW(00) RAW(7b)
        RAW(62) RAW(63) RAW(1d) RAW(40) RAW(44) RAW(6a) RAW(7f)
        RAW(7b)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
