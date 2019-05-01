
#undef FUNCNAME
#define FUNCNAME test_segment_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(8c) RAW(18)
        RAW(8c) RAW(18)
        RAW(8e) RAW(18)
        RAW(8e) RAW(18)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
