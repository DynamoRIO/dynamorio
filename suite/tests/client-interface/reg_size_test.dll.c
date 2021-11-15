#include "dr_api.h"

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'test_reg_size'",
                       "http://dynamorio.org/issues");

    opnd_size_t res;
    for (int i = DR_REG_NULL + 1; i < DR_REG_LAST_VALID_ENUM; ++i) {
        if (i == DR_REG_INVALID)
            continue;
        if (get_register_name(i)[0] == '\0') {
            continue;
        }
#if defined(X86) && !defined(X64)
        if (i >= REG_START_64 && i <= REG_STOP_64)
            continue;
        if (i >= REG_START_x64_8 && i <= REG_STOP_x64_8)
            continue;
#endif
        res = reg_get_size(i);
    }
}

#ifdef __cplusplus
}
#endif
