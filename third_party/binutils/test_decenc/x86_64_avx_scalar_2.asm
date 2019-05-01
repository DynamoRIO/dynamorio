
#undef FUNCNAME
#define FUNCNAME test_x86_64_avx_scalar_2_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(66) RAW(0f) RAW(7e) RAW(21)
        RAW(66) RAW(0f) RAW(7e) RAW(e1)
        RAW(66) RAW(0f) RAW(6e) RAW(21)
        RAW(66) RAW(0f) RAW(6e) RAW(e1)
        RAW(66) RAW(48) RAW(0f) RAW(6e) RAW(e1)
        RAW(66) RAW(48) RAW(0f) RAW(7e) RAW(e1)
        RAW(66) RAW(0f) RAW(d6) RAW(21)
        RAW(66) RAW(48) RAW(0f) RAW(7e) RAW(e1)
        RAW(f3) RAW(0f) RAW(7e) RAW(21)
        RAW(66) RAW(48) RAW(0f) RAW(6e) RAW(e1)
        RAW(c5) RAW(f9) RAW(7e) RAW(21)
        RAW(c5) RAW(f9) RAW(7e) RAW(e1)
        RAW(c5) RAW(f9) RAW(6e) RAW(21)
        RAW(c5) RAW(f9) RAW(6e) RAW(e1)
        RAW(c4) RAW(e1) RAW(f9) RAW(7e) RAW(e1)
        RAW(c4) RAW(e1) RAW(f9) RAW(6e) RAW(e1)
        RAW(c5) RAW(f9) RAW(d6) RAW(21)
        RAW(c4) RAW(e1) RAW(f9) RAW(7e) RAW(e1)
        RAW(c5) RAW(fa) RAW(7e) RAW(21)
        RAW(c4) RAW(e1) RAW(f9) RAW(6e) RAW(e1)
        RAW(c5) RAW(fa) RAW(7e) RAW(f4)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
