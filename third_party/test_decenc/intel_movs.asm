
#undef FUNCNAME
#define FUNCNAME test_intel_movs_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(a4)
        RAW(a4)
        RAW(64) RAW(a4)
        RAW(66) RAW(a5)
        RAW(66) RAW(a5)
        RAW(64) RAW(66) RAW(a5)
        RAW(a5)
        RAW(a5)
        RAW(64) RAW(a5)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
