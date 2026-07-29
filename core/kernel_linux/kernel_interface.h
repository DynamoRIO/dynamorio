#ifndef _KERNEL_INTERFACE_H_
#define _KERNEL_INTERFACE_H_

int
kernel_get_cpu_id(void);

unsigned int
kernel_query_time_seconds(void);

#define KERNEL_ENV_NAME_MAX 50
#define KERNEL_ENV_VALUE_MAX 512

const char *
kernel_getenv(const char *name);

#endif /* _KERNEL_INTERFACE_H_ */
