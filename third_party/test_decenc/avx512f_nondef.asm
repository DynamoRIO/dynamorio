
#undef FUNCNAME
#define FUNCNAME test_avx512f_nondef_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(f3) RAW(d5) RAW(1f) RAW(0b) RAW(f4) RAW(7b)
        RAW(62) RAW(f3) RAW(d5) RAW(5f) RAW(0b) RAW(f4) RAW(7b)
        RAW(62) RAW(f2) RAW(55) RAW(1f) RAW(3b) RAW(f4)
        RAW(62) RAW(c2) RAW(55) RAW(1f) RAW(3b) RAW(f4)
        RAW(62) RAW(f2) RAW(7e) RAW(48) RAW(31) RAW(72) RAW(7f)
        RAW(62)
        RAW(f2) RAW(7e) RAW(58)
        RAW(31) RAW(72) RAW(7f)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
