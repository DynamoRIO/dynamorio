
#undef FUNCNAME
#define FUNCNAME test_x86_64_branch_4_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(66) RAW(ff) RAW(d0)
        RAW(66) RAW(ff) RAW(10)
        RAW(66) RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(20)
        RAW(66) RAW(ff) RAW(d0)
        RAW(66) RAW(ff) RAW(d0)
        RAW(66) RAW(ff) RAW(10)
        RAW(66) RAW(ff) RAW(10)
        RAW(66) RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(e0)
        RAW(66) RAW(ff) RAW(20)
        RAW(66) RAW(ff) RAW(20)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
