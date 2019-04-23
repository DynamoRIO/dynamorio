
#undef FUNCNAME
#define FUNCNAME test_mixed_mode_reloc_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(67) RAW(66) RAW(8b) RAW(83) RAW(00) RAW(00) RAW(00)
        RAW(00)
        RAW(66) RAW(e8) RAW(fc) RAW(ff)
        RAW(8b) RAW(83) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e8) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        RAW(48) RAW(8b) RAW(83) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e8) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
