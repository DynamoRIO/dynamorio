
#undef FUNCNAME
#define FUNCNAME test_x86_64_mpx_branch_2_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(eb) RAW(fe)
        RAW(72) RAW(fc)
        RAW(e8) RAW(f7) RAW(ff) RAW(ff) RAW(ff)
        RAW(eb) RAW(07)
        RAW(72) RAW(05)
        RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(0f) RAW(82) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(0f) RAW(82) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
