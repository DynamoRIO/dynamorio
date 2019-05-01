
#undef FUNCNAME
#define FUNCNAME test_pr12589_1_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(eb) RAW(02)
        RAW(89) RAW(c0)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
