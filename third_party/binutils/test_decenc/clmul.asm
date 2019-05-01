
#undef FUNCNAME
#define FUNCNAME test_clmul_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(08)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(08)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(10)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(10)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(11)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(11)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(08)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(08)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(10)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(10)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(01) RAW(11)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(11)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
