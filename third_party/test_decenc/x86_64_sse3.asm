
#undef FUNCNAME
#define FUNCNAME test_x86_64_sse3_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(66) RAW(0f) RAW(d0) RAW(01)
        RAW(66) RAW(0f) RAW(d0) RAW(ca)
        RAW(f2) RAW(0f) RAW(d0) RAW(13)
        RAW(f2) RAW(0f) RAW(d0) RAW(dc)
        RAW(df) RAW(88) RAW(90) RAW(90) RAW(90) RAW(00)
        RAW(db) RAW(88) RAW(90) RAW(90) RAW(90) RAW(00)
        RAW(dd) RAW(88) RAW(90) RAW(90) RAW(90) RAW(00)
        RAW(66) RAW(0f) RAW(7c) RAW(65) RAW(00)
        RAW(66) RAW(0f) RAW(7c) RAW(ee)
        RAW(f2) RAW(0f) RAW(7c) RAW(37)
        RAW(f2) RAW(0f) RAW(7c) RAW(f8)
        RAW(66) RAW(0f) RAW(7d) RAW(c1)
        RAW(66) RAW(0f) RAW(7d) RAW(0a)
        RAW(f2) RAW(0f) RAW(7d) RAW(d2)
        RAW(f2) RAW(0f) RAW(7d) RAW(1c) RAW(24)
        RAW(f2) RAW(0f) RAW(f0) RAW(2e)
        RAW(0f) RAW(01) RAW(c8)
        RAW(0f) RAW(01) RAW(c8)
        RAW(f2) RAW(0f) RAW(12) RAW(f7)
        RAW(f2) RAW(0f) RAW(12) RAW(38)
        RAW(f3) RAW(0f) RAW(16) RAW(01)
        RAW(f3) RAW(0f) RAW(16) RAW(ca)
        RAW(f3) RAW(0f) RAW(12) RAW(13)
        RAW(f3) RAW(0f) RAW(12) RAW(dc)
        RAW(0f) RAW(01) RAW(c9)
        RAW(0f) RAW(01) RAW(c9)
        RAW(67) RAW(0f) RAW(01) RAW(c8)
        RAW(67) RAW(0f) RAW(01) RAW(c8)
        RAW(f2) RAW(0f) RAW(12) RAW(38)
        RAW(f2) RAW(0f) RAW(12) RAW(38)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
