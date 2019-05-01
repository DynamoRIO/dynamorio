
#undef FUNCNAME
#define FUNCNAME test_x86_64_io_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(48) RAW(ed)
        RAW(66) RAW(48) RAW(ed)
        RAW(48) RAW(ef)
        RAW(66) RAW(48) RAW(ef)
        RAW(48) RAW(6d)
        RAW(66) RAW(48) RAW(6d)
        RAW(48) RAW(6f)
        RAW(66) RAW(48) RAW(6f)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
