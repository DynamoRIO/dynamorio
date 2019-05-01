
#undef FUNCNAME
#define FUNCNAME test_clflushopt_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(66) RAW(0f) RAW(ae) RAW(39)
        RAW(66) RAW(0f) RAW(ae) RAW(bc) RAW(f4) RAW(c0) RAW(1d)
        RAW(fe) RAW(ff)
        RAW(66) RAW(0f) RAW(ae) RAW(39)
        RAW(66) RAW(0f) RAW(ae) RAW(bc) RAW(f4) RAW(c0) RAW(1d)
        RAW(fe) RAW(ff)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
