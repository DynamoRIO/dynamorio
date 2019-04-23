
#undef FUNCNAME
#define FUNCNAME test_rep_suffix_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(f3) RAW(ac)
        RAW(f3) RAW(aa)
        RAW(66) RAW(f3) RAW(ad)
        RAW(66) RAW(f3) RAW(ab)
        RAW(f3) RAW(ad)
        RAW(f3) RAW(ab)
        RAW(f3) RAW(0f) RAW(bc) RAW(c1)
        RAW(f3) RAW(0f) RAW(bd) RAW(c1)
        RAW(f3) RAW(c3)
        RAW(f3) RAW(90)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
