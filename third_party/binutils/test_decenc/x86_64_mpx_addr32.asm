
#undef FUNCNAME
#define FUNCNAME test_x86_64_mpx_addr32_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(67) RAW(f3) RAW(0f) RAW(1b) RAW(08)
        RAW(67) RAW(f3) RAW(0f) RAW(1b) RAW(4c) RAW(19) RAW(03)
        RAW(67) RAW(66) RAW(41) RAW(0f) RAW(1a) RAW(08)
        RAW(67) RAW(66) RAW(41) RAW(0f) RAW(1a) RAW(4c) RAW(11)
        RAW(03)
        RAW(67) RAW(66) RAW(0f) RAW(1b) RAW(08)
        RAW(67) RAW(66) RAW(0f) RAW(1b) RAW(4c) RAW(01) RAW(03)
        RAW(67) RAW(f3) RAW(0f) RAW(1a) RAW(09)
        RAW(67) RAW(f3) RAW(0f) RAW(1a) RAW(4c) RAW(01) RAW(03)
        RAW(67) RAW(f2) RAW(0f) RAW(1a) RAW(09)
        RAW(67) RAW(f2) RAW(0f) RAW(1a) RAW(4c) RAW(01) RAW(03)
        RAW(67) RAW(f2) RAW(0f) RAW(1b) RAW(09)
        RAW(67) RAW(f2) RAW(0f) RAW(1b) RAW(4c) RAW(01) RAW(03)
        RAW(67) RAW(0f) RAW(1b) RAW(44) RAW(18) RAW(03)
        RAW(67) RAW(0f) RAW(1b) RAW(53) RAW(03)
        RAW(67) RAW(0f) RAW(1a) RAW(44) RAW(18) RAW(03)
        RAW(67) RAW(0f) RAW(1a) RAW(53) RAW(03)
        RAW(67) RAW(f3) RAW(0f) RAW(1b) RAW(08)
        RAW(67) RAW(f3) RAW(0f) RAW(1b) RAW(4c) RAW(02) RAW(03)
        RAW(67) RAW(66) RAW(0f) RAW(1a) RAW(08)
        RAW(67) RAW(66) RAW(0f) RAW(1a) RAW(4c) RAW(02) RAW(03)
        RAW(67) RAW(66) RAW(0f) RAW(1b) RAW(08)
        RAW(67) RAW(66) RAW(0f) RAW(1b) RAW(4c) RAW(02) RAW(03)
        RAW(67) RAW(f3) RAW(0f) RAW(1a) RAW(08)
        RAW(67) RAW(f3) RAW(0f) RAW(1a) RAW(4c) RAW(02) RAW(03)
        RAW(67) RAW(f2) RAW(0f) RAW(1a) RAW(08)
        RAW(67) RAW(f2) RAW(0f) RAW(1a) RAW(4c) RAW(02) RAW(03)
        RAW(67) RAW(f2) RAW(0f) RAW(1b) RAW(08)
        RAW(67) RAW(f2) RAW(0f) RAW(1b) RAW(4c) RAW(02) RAW(03)
        RAW(67) RAW(0f) RAW(1b) RAW(44) RAW(18) RAW(03)
        RAW(67) RAW(0f) RAW(1b) RAW(14) RAW(1d) RAW(03) RAW(00)
        RAW(00) RAW(00)
        RAW(67) RAW(0f) RAW(1a) RAW(44) RAW(18) RAW(03)
        RAW(67) RAW(0f) RAW(1a) RAW(14) RAW(1d) RAW(03) RAW(00)
        RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
