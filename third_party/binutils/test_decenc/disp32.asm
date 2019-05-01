
#undef FUNCNAME
#define FUNCNAME test_disp32_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(8b) RAW(18)
        RAW(8b) RAW(58) RAW(03)
        RAW(8b) RAW(58) RAW(00)
        RAW(8b) RAW(58) RAW(03)
        RAW(8b) RAW(98) RAW(ff) RAW(0f) RAW(00) RAW(00)
        RAW(8b) RAW(98) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8b) RAW(98) RAW(03) RAW(00) RAW(00) RAW(00)
        /* FIXME i#1312: Support AVX-512. */
        /* RAW(62) RAW(f1) RAW(fe) RAW(08) RAW(6f) RAW(98) RAW(c0) */
        /* RAW(ff) RAW(ff) RAW(ff) */
        RAW(eb) RAW(07)
        RAW(eb) RAW(05)
        RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(89) RAW(18)
        RAW(89) RAW(58) RAW(03)
        RAW(89) RAW(98) RAW(ff) RAW(0f) RAW(00) RAW(00)
        RAW(89) RAW(58) RAW(00)
        RAW(89) RAW(58) RAW(03)
        RAW(89) RAW(98) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(89) RAW(98) RAW(03) RAW(00) RAW(00) RAW(00)
        /* FIXME i#1312: Support AVX-512. */
        /* RAW(62) RAW(f1) RAW(fe) RAW(08) RAW(6f) RAW(98) RAW(c0) */
        /* RAW(ff) RAW(ff) RAW(ff) */
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
