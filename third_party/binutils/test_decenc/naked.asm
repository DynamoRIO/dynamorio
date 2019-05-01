
#undef FUNCNAME
#define FUNCNAME test_naked_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(26) RAW(66) RAW(ff) RAW(23)
        RAW(8a) RAW(25) RAW(50) RAW(00) RAW(00) RAW(00)
        RAW(b2) RAW(20)
        RAW(bb) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(d9) RAW(c9)
        RAW(36) RAW(8c) RAW(a4) RAW(81) RAW(d2) RAW(04) RAW(00)
        RAW(00)
        RAW(8c) RAW(2c) RAW(ed) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(26) RAW(88) RAW(25) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(2e) RAW(8b) RAW(74) RAW(14) RAW(80)
        RAW(65) RAW(f3) RAW(a5)
        RAW(ec)
        RAW(66) RAW(ef)
        RAW(67) RAW(d2) RAW(14) RAW(0f)
        RAW(20) RAW(d0)
        RAW(0f) RAW(72) RAW(d0) RAW(04)
        RAW(66) RAW(47)
        RAW(66) RAW(51)
        RAW(66) RAW(58)
        RAW(66) RAW(87) RAW(dd)
        RAW(6a) RAW(02)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
