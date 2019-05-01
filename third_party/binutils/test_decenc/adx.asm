
#undef FUNCNAME
#define FUNCNAME test_adx_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(81) RAW(90) RAW(01)
        RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(ca)
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(94) RAW(f4) RAW(0f)
        RAW(04) RAW(f6) RAW(ff)
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(00)
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(ca)
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(00)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(81) RAW(90) RAW(01)
        RAW(00) RAW(00)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(ca)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(94) RAW(f4) RAW(0f)
        RAW(04) RAW(f6) RAW(ff)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(00)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(ca)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(00)
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(82) RAW(8f) RAW(01)
        RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(d1)
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(94) RAW(f4) RAW(c0)
        RAW(1d) RAW(fe) RAW(ff)
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(00)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(82) RAW(8f) RAW(01)
        RAW(00) RAW(00)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(d1)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(94) RAW(f4) RAW(c0)
        RAW(1d) RAW(fe) RAW(ff)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
