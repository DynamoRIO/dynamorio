
#undef FUNCNAME
#define FUNCNAME test_amdfam10_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(f3) RAW(0f) RAW(bd) RAW(19)
        RAW(66) RAW(f3) RAW(0f) RAW(bd) RAW(19)
        RAW(f3) RAW(0f) RAW(bd) RAW(d9)
        RAW(66) RAW(f3) RAW(0f) RAW(bd) RAW(d9)
        RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(66) RAW(0f) RAW(79) RAW(ca)
        RAW(66) RAW(0f) RAW(78) RAW(c1) RAW(02) RAW(04)
        RAW(f2) RAW(0f) RAW(79) RAW(ca)
        RAW(f2) RAW(0f) RAW(78) RAW(ca) RAW(02) RAW(04)
        RAW(f2) RAW(0f) RAW(2b) RAW(09)
        RAW(f3) RAW(0f) RAW(2b) RAW(09)
        RAW(f3) RAW(0f) RAW(bd) RAW(19)
        RAW(66) RAW(f3) RAW(0f) RAW(bd) RAW(19)
        RAW(f3) RAW(0f) RAW(bd) RAW(d9)
        RAW(66) RAW(f3) RAW(0f) RAW(bd) RAW(d9)
        RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(66) RAW(0f) RAW(79) RAW(ca)
        RAW(66) RAW(0f) RAW(78) RAW(c1) RAW(02) RAW(04)
        RAW(f2) RAW(0f) RAW(79) RAW(ca)
        RAW(f2) RAW(0f) RAW(78) RAW(ca) RAW(02) RAW(04)
        RAW(f2) RAW(0f) RAW(2b) RAW(09)
        RAW(f3) RAW(0f) RAW(2b) RAW(09)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
