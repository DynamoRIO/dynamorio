
#undef FUNCNAME
#define FUNCNAME test_clwb_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(66) RAW(0f) RAW(ae) RAW(31)
        RAW(66) RAW(0f) RAW(ae) RAW(b4) RAW(f4) RAW(c0) RAW(1d)
        RAW(fe) RAW(ff)
        RAW(66) RAW(0f) RAW(ae) RAW(31)
        RAW(66) RAW(0f) RAW(ae) RAW(b4) RAW(f4) RAW(c0) RAW(1d)
        RAW(fe) RAW(ff)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
