
#undef FUNCNAME
#define FUNCNAME test_x86_64_mpx_branch_1_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(f2) RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(f2) RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(f2) RAW(eb) RAW(fd)
        RAW(f2) RAW(72) RAW(fa)
        RAW(f2) RAW(e8) RAW(f4) RAW(ff) RAW(ff) RAW(ff)
        RAW(f2) RAW(eb) RAW(09)
        RAW(f2) RAW(72) RAW(06)
        RAW(f2) RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(f2) RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(f2) RAW(0f) RAW(82) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(f2) RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(f2) RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(f2) RAW(0f) RAW(82) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(f2) RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
