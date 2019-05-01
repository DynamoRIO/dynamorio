
#undef FUNCNAME
#define FUNCNAME test_x86_64_arch_3_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        /* clac, CPL 0 instruction. */
        /* RAW(0f) RAW(01) RAW(ca) */
        /* stac, CPL 0 instruction. */
        /* RAW(0f) RAW(01) RAW(cb) */
        RAW(66) RAW(0f) RAW(38) RAW(f6) RAW(ca)
        RAW(f3) RAW(0f) RAW(38) RAW(f6) RAW(ca)
        RAW(0f) RAW(c7) RAW(f8)
        /* FIXME i3577: clzero, AMD only. */
        /* RAW(0f) RAW(01) RAW(fc) */
        /* FIXME i#3581: Support SHA opcodes. */
        /* RAW(44) */
        /* RAW(0f) RAW(38) RAW(c8) RAW(00) */
        RAW(48)
        RAW(0f) RAW(c7) RAW(21)
        RAW(48)
        /* xsaves, CPL 0 instruction. */
        /* RAW(0f) RAW(c7) RAW(29) */
        RAW(66) RAW(0f) RAW(ae) RAW(39)
        /* FIXME i3577: monitorx, AMD only. */
        /* RAW(0f) RAW(01) RAW(fa) */
        /* RAW(67) RAW(0f) RAW(01) RAW(fa) */
        /* RAW(0f) RAW(01) RAW(fa) */
        /* FIXME i3577: mwaitx, AMD only. */
        /* RAW(0f) RAW(01) RAW(fb) */
        /* RAW(0f) RAW(01) RAW(fb) */
        RAW(66) RAW(0f) RAW(ae) RAW(31)
        RAW(66) RAW(42)
        RAW(0f) RAW(ae) RAW(b4) RAW(f0) RAW(23) RAW(01) RAW(00)
        RAW(00)
        RAW(f3) RAW(0f) RAW(c7) RAW(f8)
        RAW(f3) RAW(41)
        RAW(0f) RAW(c7) RAW(fa)
        RAW(f3) RAW(0f) RAW(09)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
