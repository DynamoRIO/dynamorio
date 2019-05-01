
#undef FUNCNAME
#define FUNCNAME test_arch_10_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(0f) RAW(44) RAW(d8)
        RAW(0f) RAW(ae) RAW(38)
        RAW(0f) RAW(05)
        RAW(0f) RAW(fc) RAW(dc)
        RAW(f3) RAW(0f) RAW(58) RAW(dc)
        RAW(f2) RAW(0f) RAW(58) RAW(dc)
        RAW(66) RAW(0f) RAW(d0) RAW(dc)
        RAW(66) RAW(0f) RAW(38) RAW(01) RAW(dc)
        RAW(66) RAW(0f) RAW(38) RAW(41) RAW(d9)
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        RAW(c5) RAW(fc) RAW(77)
        /* vmxoff, CPL 0 instruction. */
        /* RAW(0f) RAW(01) RAW(c4) */
        RAW(0f) RAW(37)
        RAW(0f) RAW(01) RAW(d0)
        RAW(0f) RAW(ae) RAW(31)
        RAW(66) RAW(0f) RAW(38) RAW(dc) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(c1) RAW(08)
        RAW(c4) RAW(e2) RAW(79) RAW(dc) RAW(11)
        RAW(c4) RAW(e3) RAW(49) RAW(44) RAW(d4) RAW(08)
        RAW(c4) RAW(e2) RAW(c9) RAW(98) RAW(d4)
        RAW(0f) RAW(38) RAW(f0) RAW(19)
        RAW(66) RAW(0f) RAW(38) RAW(80) RAW(19)
        RAW(0f) RAW(01) RAW(f9)
        RAW(0f) RAW(0d) RAW(0c) RAW(75) RAW(00) RAW(10) RAW(00)
        RAW(00)
        RAW(f2) RAW(0f) RAW(79) RAW(ca)
        RAW(0f) RAW(01) RAW(da)
        RAW(f3) RAW(0f) RAW(bd) RAW(d9)
        /* xstore-rng, unsupported. */
        /* RAW(0f) RAW(a7) RAW(c0) */
        RAW(0f) RAW(1f) RAW(00)
        RAW(c4) RAW(e2) RAW(60) RAW(f3) RAW(c9)
        RAW(8f) RAW(e9) RAW(60) RAW(01) RAW(c9)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
