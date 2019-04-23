
#undef FUNCNAME
#define FUNCNAME test_x86_64_evex_lig_2_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(7e) RAW(21)
        RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(7e) RAW(e1)
        RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(6e) RAW(21)
        RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(6e) RAW(e1)
        RAW(62) RAW(f1) RAW(fd) RAW(08) RAW(7e) RAW(21)
        RAW(62) RAW(f1) RAW(fd) RAW(08) RAW(7e) RAW(e1)
        RAW(62) RAW(f1) RAW(fd) RAW(08) RAW(6e) RAW(21)
        RAW(62) RAW(f1) RAW(fd) RAW(08) RAW(6e) RAW(e1)
        RAW(62) RAW(f1) RAW(fe) RAW(08) RAW(7e) RAW(f4)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(17) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(17) RAW(00) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(14) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(14) RAW(00) RAW(00)
        RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(c5) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(15) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(15) RAW(00) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(16) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(16) RAW(00) RAW(00)
        RAW(62) RAW(f3) RAW(fd) RAW(08) RAW(16) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(fd) RAW(08) RAW(16) RAW(00) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(21) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(21) RAW(00) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(20) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(20) RAW(00) RAW(00)
        RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(c4) RAW(c0) RAW(00)
        RAW(62) RAW(f1) RAW(7d) RAW(08) RAW(c4) RAW(00) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(22) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(7d) RAW(08) RAW(22) RAW(00) RAW(00)
        RAW(62) RAW(f3) RAW(fd) RAW(08) RAW(22) RAW(c0) RAW(00)
        RAW(62) RAW(f3) RAW(fd) RAW(08) RAW(22) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
