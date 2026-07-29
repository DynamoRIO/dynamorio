#ifndef _STDDEF_WRAPPER_H_
#define _STDDEF_WRAPPER_H_

#include "configure.h"

#ifdef LINUX_KERNEL
#    include <linux/stddef.h>
#    ifndef _WCHAR_T_DEFINED
#        define _WCHAR_T_DEFINED
typedef __WCHAR_TYPE__ wchar_t;
#    endif
#else
#    include <stddef.h>
#endif

#endif
