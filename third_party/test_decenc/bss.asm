
#undef FUNCNAME
#define FUNCNAME test_bss_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
