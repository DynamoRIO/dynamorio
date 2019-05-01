
#undef FUNCNAME
#define FUNCNAME test_amd_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(0f) RAW(0d) RAW(03)
        RAW(0f) RAW(0d) RAW(0c) RAW(75) RAW(00) RAW(10) RAW(00)
        RAW(00)
        RAW(0f) RAW(0e)
        RAW(0f) RAW(0f) RAW(00) RAW(bf)
        RAW(0f) RAW(0f) RAW(48) RAW(02) RAW(1d)
        RAW(0f) RAW(0f) RAW(90) RAW(00) RAW(01) RAW(00) RAW(00)
        RAW(ae)
        RAW(0f) RAW(0f) RAW(1e) RAW(9e)
        RAW(0f) RAW(0f) RAW(66) RAW(02) RAW(b0)
        RAW(0f) RAW(0f) RAW(ae) RAW(90) RAW(90) RAW(00) RAW(00)
        RAW(90)
        RAW(0f) RAW(0f) RAW(74) RAW(75) RAW(00) RAW(a0)
        RAW(0f) RAW(0f) RAW(7c) RAW(75) RAW(02) RAW(a4)
        RAW(0f) RAW(0f) RAW(84) RAW(75) RAW(90) RAW(90) RAW(90)
        RAW(90) RAW(94)
        RAW(0f) RAW(0f) RAW(0d) RAW(04) RAW(00) RAW(00) RAW(00)
        RAW(b4)
        RAW(2e) RAW(0f) RAW(0f) RAW(54) RAW(c3) RAW(07) RAW(96)
        RAW(0f) RAW(0f) RAW(d8) RAW(a6)
        RAW(0f) RAW(0f) RAW(e1) RAW(b6)
        RAW(0f) RAW(0f) RAW(ea) RAW(a7)
        RAW(0f) RAW(0f) RAW(f3) RAW(97)
        RAW(0f) RAW(0f) RAW(fc) RAW(9a)
        RAW(0f) RAW(0f) RAW(c5) RAW(aa)
        RAW(0f) RAW(0f) RAW(ce) RAW(0d)
        RAW(0f) RAW(0f) RAW(d7) RAW(b7)
        RAW(0f) RAW(05)
        RAW(0f) RAW(07)
        RAW(0f) RAW(01) RAW(f9)
        RAW(2e) RAW(0f)
        RAW(0f) RAW(54) RAW(c3)
        RAW(07)
        RAW(c3)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
