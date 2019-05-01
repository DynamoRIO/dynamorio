
#undef FUNCNAME
#define FUNCNAME test_equ_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(b8) RAW(ff) RAW(ff) RAW(ff) RAW(ff)
        RAW(a1) RAW(ff) RAW(ff) RAW(ff) RAW(ff) RAW(b8) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(85) RAW(c9)
        RAW(64) RAW(8b) RAW(0c) RAW(89)
        RAW(d8) RAW(c1)
        RAW(b8) RAW(fe) RAW(ff) RAW(ff) RAW(ff)
        RAW(a1) RAW(fe) RAW(ff) RAW(ff) RAW(ff) RAW(b8) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(85) RAW(d2)
        RAW(65) RAW(8b) RAW(14) RAW(d2)
        RAW(65) RAW(8b) RAW(14) RAW(d2)
        RAW(d8) RAW(c1)
        RAW(d8) RAW(c7)
        RAW(8b) RAW(42) RAW(04)
        RAW(8b) RAW(42) RAW(04)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
