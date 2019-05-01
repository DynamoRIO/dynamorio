
#undef FUNCNAME
#define FUNCNAME test_x86_64_rtm_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(c6) RAW(f8) RAW(08)
        RAW(c7) RAW(f8) RAW(fa) RAW(ff) RAW(ff) RAW(ff)
        RAW(c7) RAW(f8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(0f) RAW(01) RAW(d5)
        RAW(c6) RAW(f8) RAW(08)
        RAW(c7) RAW(f8) RAW(fa) RAW(ff) RAW(ff) RAW(ff)
        RAW(c7) RAW(f8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(0f) RAW(01) RAW(d5)
        RAW(0f) RAW(01) RAW(d6)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
