
#undef FUNCNAME
#define FUNCNAME test_x86_64_segovr_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(8b) RAW(00)
        RAW(8b) RAW(01)
        RAW(8b) RAW(02)
        RAW(8b) RAW(03)
        RAW(3e) RAW(8b) RAW(04) RAW(24)
        RAW(3e) RAW(8b) RAW(45) RAW(00)
        RAW(8b) RAW(06)
        RAW(8b) RAW(07)
        RAW(41) RAW(8b) RAW(00)
        RAW(41) RAW(8b) RAW(01)
        RAW(41) RAW(8b) RAW(02)
        RAW(41) RAW(8b) RAW(03)
        RAW(41) RAW(8b) RAW(04) RAW(24)
        RAW(41) RAW(8b) RAW(45) RAW(00)
        RAW(41) RAW(8b) RAW(06)
        RAW(41) RAW(8b) RAW(07)
        RAW(36) RAW(8b) RAW(00)
        RAW(36) RAW(8b) RAW(01)
        RAW(36) RAW(8b) RAW(02)
        RAW(36) RAW(8b) RAW(03)
        RAW(8b) RAW(04) RAW(24)
        RAW(8b) RAW(45) RAW(00)
        RAW(36) RAW(8b) RAW(06)
        RAW(36) RAW(8b) RAW(07)
        RAW(36) RAW(41) RAW(8b) RAW(00)
        RAW(36) RAW(41) RAW(8b) RAW(01)
        RAW(36) RAW(41) RAW(8b) RAW(02)
        RAW(36) RAW(41) RAW(8b) RAW(03)
        RAW(36) RAW(41) RAW(8b) RAW(04) RAW(24)
        RAW(36) RAW(41) RAW(8b) RAW(45) RAW(00)
        RAW(36) RAW(41) RAW(8b) RAW(06)
        RAW(36) RAW(41) RAW(8b) RAW(07)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
