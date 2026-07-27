#ifndef _TYPES_WRAPPER_H_
#define _TYPES_WRAPPER_H_

#include "configure.h"

#ifdef LINUX_KERNEL
#    include <linux/types.h>
#else
#    include <sys/types.h>
#endif

#endif
