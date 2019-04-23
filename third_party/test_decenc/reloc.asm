
#undef FUNCNAME
#define FUNCNAME test_reloc_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(b3) RAW(00)
        RAW(68) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(05) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(81) RAW(c3) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(69) RAW(d2) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(9a)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(66) RAW(68) RAW(00) RAW(00)
        RAW(90)
        RAW(90)
        RAW(90)
        RAW(90)
        RAW(90)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
