
#undef FUNCNAME
#define FUNCNAME test_reg_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(0f) RAW(71) RAW(d6) RAW(02)
        RAW(66) RAW(0f) RAW(71) RAW(d6) RAW(02)
        RAW(0f) RAW(71) RAW(e6) RAW(02)
        RAW(66) RAW(0f) RAW(71) RAW(e6) RAW(02)
        RAW(0f) RAW(71) RAW(f6) RAW(02)
        RAW(66) RAW(0f) RAW(71) RAW(f6) RAW(02)
        RAW(0f) RAW(72) RAW(d6) RAW(02)
        RAW(66) RAW(0f) RAW(72) RAW(d6) RAW(02)
        RAW(0f) RAW(72) RAW(e6) RAW(02)
        RAW(66) RAW(0f) RAW(72) RAW(e6) RAW(02)
        RAW(0f) RAW(72) RAW(f6) RAW(02)
        RAW(66) RAW(0f) RAW(72) RAW(f6) RAW(02)
        RAW(0f) RAW(73) RAW(d6) RAW(02)
        RAW(66) RAW(0f) RAW(73) RAW(d6) RAW(02)
        RAW(66) RAW(0f) RAW(73) RAW(de) RAW(02)
        RAW(0f) RAW(73) RAW(f6) RAW(02)
        RAW(66) RAW(0f) RAW(73) RAW(f6) RAW(02)
        RAW(66) RAW(0f) RAW(73) RAW(fe) RAW(02)
        RAW(0f) RAW(71) RAW(d6) RAW(02)
        RAW(66) RAW(0f) RAW(71) RAW(d6) RAW(02)
        RAW(0f) RAW(71) RAW(e6) RAW(02)
        RAW(66) RAW(0f) RAW(71) RAW(e6) RAW(02)
        RAW(0f) RAW(71) RAW(f6) RAW(02)
        RAW(66) RAW(0f) RAW(71) RAW(f6) RAW(02)
        RAW(0f) RAW(72) RAW(d6) RAW(02)
        RAW(66) RAW(0f) RAW(72) RAW(d6) RAW(02)
        RAW(0f) RAW(72) RAW(e6) RAW(02)
        RAW(66) RAW(0f) RAW(72) RAW(e6) RAW(02)
        RAW(0f) RAW(72) RAW(f6) RAW(02)
        RAW(66) RAW(0f) RAW(72) RAW(f6) RAW(02)
        RAW(0f) RAW(73) RAW(d6) RAW(02)
        RAW(66) RAW(0f) RAW(73) RAW(d6) RAW(02)
        RAW(66) RAW(0f) RAW(73) RAW(de) RAW(02)
        RAW(0f) RAW(73) RAW(f6) RAW(02)
        RAW(66) RAW(0f) RAW(73) RAW(f6) RAW(02)
        RAW(66) RAW(0f) RAW(73) RAW(fe) RAW(02)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
