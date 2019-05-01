
#undef FUNCNAME
#define FUNCNAME test_x86_64_avx512_4fmaps_warn_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(62) RAW(72) RAW(7f) RAW(48) RAW(9a) RAW(10)
        RAW(62) RAW(72) RAW(77) RAW(48) RAW(9a) RAW(10)
        RAW(62) RAW(72) RAW(6f) RAW(48) RAW(9a) RAW(10)
        RAW(62) RAW(72) RAW(67) RAW(48) RAW(9a) RAW(10)
        RAW(62) RAW(72) RAW(5f) RAW(48) RAW(9a) RAW(10)
        RAW(62) RAW(72) RAW(7f) RAW(48) RAW(aa) RAW(10)
        RAW(62) RAW(72) RAW(77) RAW(48) RAW(aa) RAW(10)
        RAW(62) RAW(72) RAW(6f) RAW(48) RAW(aa) RAW(10)
        RAW(62) RAW(72) RAW(67) RAW(48) RAW(aa) RAW(10)
        RAW(62) RAW(72) RAW(5f) RAW(48) RAW(aa) RAW(10)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
