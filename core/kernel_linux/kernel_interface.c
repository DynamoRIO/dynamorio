#include "kernel_interface.h"

int
kernel_get_cpu_id(void)
{
    return smp_processor_id();
}

#define KERNEL_ENV_MAX 20

typedef struct {
    char name[KERNEL_ENV_NAME_MAX];
    char value[KERNEL_ENV_VALUE_MAX];
} kernel_env_t;

static kernel_env_t env_vars[KERNEL_ENV_MAX];
static int env_count = 0;

const char *
kernel_getenv(const char *name)
{
    for (int i = 0; i < env_count; i++) {
        if (strcmp(name, env_vars[i].name) == 0) {
            return (const char *)env_vars[i].value;
        }
    }
    return NULL;
}
