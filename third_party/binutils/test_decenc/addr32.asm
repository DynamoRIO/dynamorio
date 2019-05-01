
#undef FUNCNAME
#define FUNCNAME test_addr32_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(67) RAW(a0) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(a1) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(66) RAW(a1) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(a2) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(a3) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(66) RAW(a3) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(66) RAW(c7) RAW(04) RAW(24) RAW(01) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
