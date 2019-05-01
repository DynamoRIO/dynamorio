
#undef FUNCNAME
#define FUNCNAME test_suffix_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(0f) RAW(01) RAW(c8)
        RAW(0f) RAW(01) RAW(c9)
        RAW(66) RAW(cf)
        RAW(cf)
        RAW(cf)
        RAW(0f) RAW(07)
        RAW(0f) RAW(07)
        RAW(66) RAW(cf)
        RAW(cf)
        RAW(cf)
        RAW(0f) RAW(07)
        RAW(0f) RAW(07)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
