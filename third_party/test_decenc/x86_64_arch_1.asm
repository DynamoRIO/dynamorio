
#undef FUNCNAME
#define FUNCNAME test_x86_64_arch_1_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(66) RAW(0f) RAW(38) RAW(17) RAW(c1)
        RAW(66) RAW(0f) RAW(3a) RAW(09) RAW(c1) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(08) RAW(c1) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(0b) RAW(c1) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(0a) RAW(c1) RAW(00)
        RAW(66) RAW(0f) RAW(38) RAW(41) RAW(d9)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
