
#undef FUNCNAME
#define FUNCNAME test_x86_64_gotpcrel_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(48) RAW(c7) RAW(c0) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(48) RAW(8b) RAW(04) RAW(25) RAW(00) RAW(00) RAW(00)
        RAW(00)
        RAW(48) RAW(8b) RAW(05) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(48) RAW(8b) RAW(81) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(15) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(90) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(25) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(48) RAW(c7) RAW(c0) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(48) RAW(8b) RAW(04) RAW(25) RAW(00) RAW(00) RAW(00)
        RAW(00)
        RAW(48) RAW(8b) RAW(05) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(48) RAW(8b) RAW(81) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(15) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(90) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(25) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
