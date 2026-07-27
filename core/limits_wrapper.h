#ifndef _LIMITS_WRAPPER_H_
#define _LIMITS_WRAPPER_H_

#include "configure.h"

#ifdef LINUX_KERNEL
/* The Linux kernel does not have a standard limits.h for basic C types,
 * but it defines some integer limits in <linux/kernel.h>. We include that
 * and manually define the standard C char/byte limits here.
 */
#    include <linux/kernel.h>

/* Number of bits in a `char'.  */
#    define CHAR_BIT 8

/* Minimum and maximum values a `signed char' can hold.  */
#    define SCHAR_MIN (-128)
#    define SCHAR_MAX 127

/* Maximum value an `unsigned char' can hold.  (Minimum is 0.)  */
#    define UCHAR_MAX 255

/* Minimum and maximum values a `char' can hold.  */
#    ifdef __CHAR_UNSIGNED__
#        define CHAR_MIN 0
#        define CHAR_MAX UCHAR_MAX
#    else
#        define CHAR_MIN SCHAR_MIN
#        define CHAR_MAX SCHAR_MAX
#    endif
#else
#    include <limits.h>
#endif

#endif /* _LIMITS_WRAPPER_H_ */
