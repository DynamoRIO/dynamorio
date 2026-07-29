#ifndef _STDARG_WRAPPER_H_
#define _STDARG_WRAPPER_H_

#include "configure.h"

#ifdef LINUX_KERNEL
#    include <linux/stdarg.h>
#else
#    include <stdarg.h>
#endif

#endif
