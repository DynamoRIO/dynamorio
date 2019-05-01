
#undef FUNCNAME
#define FUNCNAME test_x86_64_movbe_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(66) RAW(45) RAW(0f) RAW(38) RAW(f0) RAW(29)
        RAW(45) RAW(0f) RAW(38) RAW(f0) RAW(29)
        RAW(4d) RAW(0f) RAW(38) RAW(f0) RAW(29)
        RAW(66) RAW(45) RAW(0f) RAW(38) RAW(f1) RAW(29)
        RAW(45) RAW(0f) RAW(38) RAW(f1) RAW(29)
        RAW(4d) RAW(0f) RAW(38) RAW(f1) RAW(29)
        RAW(66) RAW(45) RAW(0f) RAW(38) RAW(f0) RAW(29)
        RAW(45) RAW(0f) RAW(38) RAW(f0) RAW(29)
        RAW(4d) RAW(0f) RAW(38) RAW(f0) RAW(29)
        RAW(66) RAW(45) RAW(0f) RAW(38) RAW(f1) RAW(29)
        RAW(45) RAW(0f) RAW(38) RAW(f1) RAW(29)
        RAW(4d) RAW(0f) RAW(38) RAW(f1) RAW(29)
        RAW(66) RAW(0f) RAW(38) RAW(f0) RAW(19)
        RAW(0f) RAW(38) RAW(f0) RAW(19)
        RAW(48) RAW(0f) RAW(38) RAW(f0) RAW(19)
        RAW(66) RAW(0f) RAW(38) RAW(f1) RAW(19)
        RAW(0f) RAW(38) RAW(f1) RAW(19)
        RAW(48) RAW(0f) RAW(38) RAW(f1) RAW(19)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
