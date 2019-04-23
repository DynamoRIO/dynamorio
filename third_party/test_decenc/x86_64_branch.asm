
#undef FUNCNAME
#define FUNCNAME test_x86_64_branch_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(ff) RAW(d0)
        RAW(ff) RAW(d0)
        RAW(66) RAW(ff) RAW(d0)
        RAW(66) RAW(ff) RAW(d0)
        RAW(66) RAW(ff) RAW(10)
        RAW(ff) RAW(e0)
        RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(20)
        RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(e8) RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(66) RAW(e9) RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(82) RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(ff) RAW(d0)
        RAW(ff) RAW(d0)
        RAW(66) RAW(ff) RAW(d0)
        RAW(66) RAW(ff) RAW(d0)
        RAW(66) RAW(ff) RAW(10)
        RAW(ff) RAW(e0)
        RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(20)
        RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
