
#undef FUNCNAME
#define FUNCNAME test_rdrnd_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(66) RAW(0f) RAW(c7) RAW(f3)
        RAW(0f) RAW(c7) RAW(f3)
        RAW(66) RAW(0f) RAW(c7) RAW(f3)
        RAW(0f) RAW(c7) RAW(f3)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
