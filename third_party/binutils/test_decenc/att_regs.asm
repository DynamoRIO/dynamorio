
#undef FUNCNAME
#define FUNCNAME test_att_regs_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(a1) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(a1) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(a1) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(a1) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(a1) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(a1) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(a1) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(0f) RAW(20)
        RAW(c0) RAW(0f)
        RAW(21) RAW(c0)
        /* DynamoRIO does not support test registers. */
        /* RAW(0f) RAW(24) RAW(c0) */
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(a1) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(dd) RAW(c0)
        RAW(0f) RAW(ef) RAW(c0)
        RAW(0f) RAW(57) RAW(c0)
        RAW(c5) RAW(fc) RAW(57) RAW(c0)
        RAW(44) RAW(88) RAW(c0)
        RAW(66) RAW(44) RAW(89) RAW(c0)
        RAW(44) RAW(89) RAW(c0)
        RAW(4c) RAW(89) RAW(c0)
        RAW(eb) RAW(fe)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
