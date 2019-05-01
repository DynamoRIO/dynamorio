
#undef FUNCNAME
#define FUNCNAME test_x86_64_gfni_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(66) RAW(0f) RAW(38) RAW(cf) RAW(ec)
        RAW(66) RAW(42) RAW(0f) RAW(38) RAW(cf) RAW(ac) RAW(f0)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(66) RAW(0f) RAW(38) RAW(cf) RAW(aa) RAW(f0) RAW(07)
        RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(ce) RAW(ec) RAW(ab)
        RAW(66) RAW(42) RAW(0f) RAW(3a) RAW(ce) RAW(ac) RAW(f0)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(66) RAW(0f) RAW(3a) RAW(ce) RAW(aa) RAW(f0) RAW(07)
        RAW(00) RAW(00) RAW(7b)
        RAW(66) RAW(0f) RAW(3a) RAW(cf) RAW(ec) RAW(ab)
        RAW(66) RAW(42) RAW(0f) RAW(3a) RAW(cf) RAW(ac) RAW(f0)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(66) RAW(0f) RAW(3a) RAW(cf) RAW(aa) RAW(f0) RAW(07)
        RAW(00) RAW(00) RAW(7b)
        RAW(66) RAW(0f) RAW(38) RAW(cf) RAW(ec)
        RAW(66) RAW(42) RAW(0f) RAW(38) RAW(cf) RAW(ac) RAW(f0)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(66) RAW(0f) RAW(38) RAW(cf) RAW(aa) RAW(f0) RAW(07)
        RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(ce) RAW(ec) RAW(ab)
        RAW(66) RAW(42) RAW(0f) RAW(3a) RAW(ce) RAW(ac) RAW(f0)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(66) RAW(0f) RAW(3a) RAW(ce) RAW(aa) RAW(f0) RAW(07)
        RAW(00) RAW(00) RAW(7b)
        RAW(66) RAW(0f) RAW(3a) RAW(cf) RAW(ec) RAW(ab)
        RAW(66) RAW(42) RAW(0f) RAW(3a) RAW(cf) RAW(ac) RAW(f0)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff) RAW(7b)
        RAW(66) RAW(0f) RAW(3a) RAW(cf) RAW(aa) RAW(f0) RAW(07)
        RAW(00) RAW(00) RAW(7b)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
