
#undef FUNCNAME
#define FUNCNAME test_x86_64_mem_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(0f) RAW(01) RAW(06)
        RAW(0f) RAW(01) RAW(0e)
        RAW(0f) RAW(01) RAW(16)
        RAW(0f) RAW(01) RAW(1e)
        RAW(0f) RAW(01) RAW(3e)
        RAW(0f) RAW(c7) RAW(0e)
        RAW(48) RAW(0f) RAW(c7) RAW(0e)
        RAW(0f) RAW(c7) RAW(36)
        RAW(66) RAW(0f) RAW(c7) RAW(36)
        RAW(f3) RAW(0f) RAW(c7) RAW(36)
        RAW(0f) RAW(c7) RAW(3e)
        RAW(0f) RAW(ae) RAW(06)
        RAW(0f) RAW(ae) RAW(0e)
        RAW(0f) RAW(ae) RAW(16)
        RAW(0f) RAW(ae) RAW(1e)
        RAW(0f) RAW(ae) RAW(3e)
        RAW(0f) RAW(01) RAW(06)
        RAW(0f) RAW(01) RAW(0e)
        RAW(0f) RAW(01) RAW(16)
        RAW(0f) RAW(01) RAW(1e)
        RAW(0f) RAW(01) RAW(3e)
        RAW(0f) RAW(c7) RAW(0e)
        RAW(48) RAW(0f) RAW(c7) RAW(0e)
        RAW(0f) RAW(c7) RAW(36)
        RAW(66) RAW(0f) RAW(c7) RAW(36)
        RAW(f3) RAW(0f) RAW(c7) RAW(36)
        RAW(0f) RAW(c7) RAW(3e)
        RAW(0f) RAW(ae) RAW(06)
        RAW(0f) RAW(ae) RAW(0e)
        RAW(0f) RAW(ae) RAW(16)
        RAW(0f) RAW(ae) RAW(1e)
        RAW(0f) RAW(ae) RAW(3e)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
