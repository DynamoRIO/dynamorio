
#undef FUNCNAME
#define FUNCNAME test_size_3_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(b8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(f8) RAW(ff) RAW(ff) RAW(ff)
        RAW(b8) RAW(08) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(f0) RAW(ff) RAW(ff) RAW(ff)
        RAW(b8) RAW(10) RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
