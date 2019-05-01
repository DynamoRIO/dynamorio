
#undef FUNCNAME
#define FUNCNAME test_nop_1_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(90)
        RAW(90)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(90)
        RAW(8d) RAW(74) RAW(26) RAW(00)
        RAW(8d) RAW(74) RAW(26) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b6) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(eb) RAW(1c)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(eb) RAW(7f)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(90)
        RAW(31) RAW(c0)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
