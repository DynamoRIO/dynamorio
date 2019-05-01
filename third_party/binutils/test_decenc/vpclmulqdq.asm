
#undef FUNCNAME
#define FUNCNAME test_vpclmulqdq_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(c4) RAW(e3) RAW(55) RAW(44) RAW(f4) RAW(ab)
        RAW(c4) RAW(e3) RAW(55) RAW(44) RAW(b4) RAW(f4) RAW(c0)
        RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(c4) RAW(e3) RAW(55) RAW(44) RAW(b2) RAW(e0) RAW(0f)
        RAW(00) RAW(00) RAW(7b)
        RAW(c4) RAW(e3) RAW(55) RAW(44) RAW(f4) RAW(ab)
        RAW(c4) RAW(e3) RAW(55) RAW(44) RAW(b4) RAW(f4) RAW(c0)
        RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(c4) RAW(e3) RAW(55) RAW(44) RAW(b2) RAW(e0) RAW(0f)
        RAW(00) RAW(00) RAW(7b)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
