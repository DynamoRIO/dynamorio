
#undef FUNCNAME
#define FUNCNAME test_x86_64_evex_wig2_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(f1) RAW(36) RAW(30) RAW(2a) RAW(f0)
        RAW(62) RAW(f1) RAW(36) RAW(00) RAW(2a) RAW(f0)
        RAW(62) RAW(f1) RAW(37) RAW(00) RAW(2a) RAW(f0)
        RAW(62) RAW(f1) RAW(36) RAW(30) RAW(7b) RAW(f0)
        RAW(62) RAW(f1) RAW(36) RAW(00) RAW(7b) RAW(f0)
        RAW(62) RAW(f1) RAW(07) RAW(08) RAW(7b) RAW(f0)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
