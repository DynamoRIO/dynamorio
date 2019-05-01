
#undef FUNCNAME
#define FUNCNAME test_x86_64_movd_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(66) RAW(0f) RAW(6e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(66) RAW(48) RAW(0f) RAW(6e) RAW(c8)
        RAW(66) RAW(0f) RAW(7e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(66) RAW(48) RAW(0f) RAW(7e) RAW(c8)
        RAW(c5) RAW(f9) RAW(6e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(c4) RAW(e1) RAW(f9) RAW(6e) RAW(c8)
        RAW(c5) RAW(f9) RAW(7e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(c4) RAW(e1) RAW(f9) RAW(7e) RAW(c8)
        /* FIXME i#1312: Support AVX-512. */
        /* RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(6e) RAW(48) RAW(20) */
        /* RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(7e) RAW(48) RAW(20) */
        RAW(66) RAW(0f) RAW(6e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(66) RAW(0f) RAW(6e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(66) RAW(0f) RAW(6e) RAW(c8)
        RAW(66) RAW(0f) RAW(7e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(66) RAW(0f) RAW(7e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(66) RAW(0f) RAW(7e) RAW(c8)
        RAW(66) RAW(48) RAW(0f) RAW(6e) RAW(88) RAW(80) RAW(00)
        RAW(00) RAW(00)
        RAW(66) RAW(48) RAW(0f) RAW(6e) RAW(c8)
        RAW(66) RAW(48) RAW(0f) RAW(7e) RAW(88) RAW(80) RAW(00)
        RAW(00) RAW(00)
        RAW(66) RAW(48) RAW(0f) RAW(7e) RAW(c8)
        RAW(c5) RAW(f9) RAW(6e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(c5) RAW(f9) RAW(6e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(c5) RAW(f9) RAW(6e) RAW(c8)
        RAW(c5) RAW(f9) RAW(7e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(c5) RAW(f9) RAW(7e) RAW(88) RAW(80) RAW(00) RAW(00)
        RAW(00)
        RAW(c5) RAW(f9) RAW(7e) RAW(c8)
        /* FIXME i#1312: Support AVX-512. */
        /* RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(6e) RAW(48) RAW(20) */
        /* RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(6e) RAW(48) RAW(20) */
        /* RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(6e) RAW(c8) */
        /* RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(7e) RAW(48) RAW(20) */
        /* RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(7e) RAW(48) RAW(20) */
        /* RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(7e) RAW(c8) */
        RAW(c4) RAW(e1) RAW(f9) RAW(6e) RAW(c8)
        RAW(c4) RAW(e1) RAW(f9) RAW(7e) RAW(c8)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
