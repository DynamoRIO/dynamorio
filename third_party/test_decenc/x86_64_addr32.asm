
#undef FUNCNAME
#define FUNCNAME test_x86_64_addr32_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(67) RAW(48) RAW(8d) RAW(80) RAW(00) RAW(00) RAW(00)
        RAW(00)
        RAW(67) RAW(49) RAW(8d) RAW(80) RAW(00) RAW(00) RAW(00)
        RAW(00)
       /* An address size prefix leads to an encoding assertion in 64-bit mode,
        * because the 64-bit address will not be reachable.
        */
        /* RAW(67) RAW(48) RAW(8d) RAW(05) RAW(00) RAW(00) RAW(00) */
        /* RAW(00) */
        RAW(67) RAW(48) RAW(8d) RAW(04) RAW(25) RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(67) RAW(a0) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(66) RAW(a1) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(a1) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(48) RAW(a1) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(48) RAW(a1) RAW(98) RAW(08) RAW(80) RAW(00)
        RAW(67) RAW(48) RAW(8b) RAW(1c) RAW(25) RAW(98) RAW(08)
        RAW(80) RAW(00)
        RAW(67) RAW(a2) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(66) RAW(a3) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(a3) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(48) RAW(a3) RAW(98) RAW(08) RAW(60) RAW(00)
        RAW(67) RAW(48) RAW(a3) RAW(98) RAW(08) RAW(80) RAW(00)
        RAW(67) RAW(48) RAW(89) RAW(1c) RAW(25) RAW(98) RAW(08)
        RAW(80) RAW(00)
        RAW(67) RAW(89) RAW(04) RAW(25) RAW(11) RAW(22) RAW(33)
        RAW(ff)
        RAW(67) RAW(89) RAW(04) RAW(65) RAW(11) RAW(22) RAW(33)
        RAW(ff)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
