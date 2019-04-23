
#undef FUNCNAME
#define FUNCNAME test_xsavec_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(0f) RAW(c7) RAW(21)
        RAW(0f) RAW(c7) RAW(a4) RAW(f4) RAW(c0) RAW(1d) RAW(fe)
        RAW(ff)
        RAW(0f) RAW(c7) RAW(21)
        RAW(0f) RAW(c7) RAW(a4) RAW(f4) RAW(c0) RAW(1d) RAW(fe)
        RAW(ff)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
