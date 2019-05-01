
#undef FUNCNAME
#define FUNCNAME test_clzero_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(0f) RAW(01) RAW(fc)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
