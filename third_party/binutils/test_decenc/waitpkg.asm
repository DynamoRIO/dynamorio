
#undef FUNCNAME
#define FUNCNAME test_waitpkg_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(f3) RAW(0f) RAW(ae) RAW(f0)
        RAW(67) RAW(f3) RAW(0f) RAW(ae)
        RAW(f1)
        RAW(f2) RAW(0f) RAW(ae)
        RAW(f1)
        RAW(66) RAW(0f) RAW(ae)
        RAW(f1)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
