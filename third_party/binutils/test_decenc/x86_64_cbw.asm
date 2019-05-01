
#undef FUNCNAME
#define FUNCNAME test_x86_64_cbw_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(66) RAW(98)
        RAW(98)
        RAW(48) RAW(98)
        RAW(66) RAW(40) RAW(98)
        RAW(40) RAW(98)
        RAW(66) RAW(48) RAW(98)
        RAW(66) RAW(99)
        RAW(99)
        RAW(48) RAW(99)
        RAW(66) RAW(40) RAW(99)
        RAW(40) RAW(99)
        RAW(66) RAW(48) RAW(99)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
