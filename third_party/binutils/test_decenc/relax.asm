
#undef FUNCNAME
#define FUNCNAME test_relax_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(90)
        RAW(90)
        RAW(eb) RAW(14)
        RAW(eb) RAW(12)
        RAW(41)
        RAW(42)
        RAW(43)
        RAW(44)
        RAW(45)
        RAW(46)
        RAW(47)
        RAW(48)
        RAW(49) RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(eb) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
