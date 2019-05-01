
#undef FUNCNAME
#define FUNCNAME test_got_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(b8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8b) RAW(05) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8b) RAW(80) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(05) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(03) RAW(05) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(03) RAW(80) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(15) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(90) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(25) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(a0) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8b) RAW(05) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8b) RAW(80) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(05) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(03) RAW(05) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(03) RAW(80) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(90) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(15) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(a0) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(25) RAW(00) RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
