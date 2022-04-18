#include "dr_api.h"
#include "client_tools.h"
#include "string.h"

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'test_reg_size'",
                       "http://dynamorio.org/issues");

    size_t n;
    opnd_size_t res;
    const char *reg;
    for (int i = DR_REG_NULL + 1; i <= DR_REG_LAST_VALID_ENUM; ++i) {
        if (i == DR_REG_INVALID)
            continue;
#ifdef X86
#    ifndef X64
        if (i >= REG_START_64 && i <= REG_STOP_64)
            continue;
        if (i >= REG_START_x64_8 && i <= REG_STOP_x64_8)
            continue;
#    endif
        // Filter out reserved register enum
        if (i > DR_REG_STOP_XMM && i <= RESERVED_XMM)
            continue;
        if (i > DR_REG_STOP_YMM && i <= RESERVED_YMM)
            continue;
        if (i > DR_REG_STOP_ZMM && i <= RESERVED_ZMM)
            continue;
        if (i > DR_REG_STOP_OPMASK && i <= RESERVED_OPMASK)
            continue;
#endif
        // checking register name by its length
        reg = get_register_name(i);
        n = strlen(reg);
        CHECK(n != 0, "register name should not be empty!");
        for (size_t j = 0; j < n; ++j) {
            if (!((reg[j] >= 'a' && reg[j] <= 'z') || (reg[j] >= '0' && reg[j] <= '9') ||
                  reg[j] == '_')) {
                dr_fprintf(STDERR, "register name is invalid: %s\n", reg);
                CHECK(false, "register should be named with a-z/0-9/_");
            }
        }
        res = reg_get_size(i);
        CHECK(res != OPSZ_NA, "reg_get_size return OPSZ_NA!");
    }
}
