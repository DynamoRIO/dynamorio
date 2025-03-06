/* **********************************************************
 * Copyright (c) 2012-2016 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _DRMEMORY_FRAMEWORK_H_
#define _DRMEMORY_FRAMEWORK_H_ 1

/* Dr. Memory Framework shared header */

#include "dr_api.h"

/**
 * @file drmemory_framework.h
 * @brief Shared header for Dr. Memory Framework
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drmf Dr. Memory Framework
 */
/*@{*/ /* begin doxygen group */

#define EXPANDSTR(x) #x
#define STRINGIFY(x) EXPANDSTR(x)

/* Version checking.
 * We provide an oldest-compatible version and a current version.
 * When we make additions to the API, we increment just the current version.
 * When we modify any part of the existing API, we increment the
 * current version, but we also increase the oldest-compatible
 * version to match the (just-incremented) current version.
 */
#define DRMF_VERSION_COMPAT ${DRMF_VERSION_COMPAT}
#define DRMF_VERSION_CUR ${DRMF_VERSION_CUR}
#define DRMF_VERSION_USED_VAR    _DRMF_VERSION_USED_
DR_EXPORT LINK_ONCE int DRMF_VERSION_USED_VAR = ${DRMF_VERSION_CUR};
#define DRMF_VERSION_USED_NAME    STRINGIFY(DRMF_VERSION_USED_VAR)


/** Status codes for the Dr. Memory Framework */
typedef enum {
    DRMF_SUCCESS,                  /**< Operation succeeded. */
    DRMF_ERROR,                    /**< Operation failed. */
    DRMF_ERROR_INCOMPATIBLE_VERSION,  /**< Operation failed: incompatible version */
    DRMF_ERROR_INVALID_PARAMETER,  /**< Operation failed: invalid parameter */
    DRMF_ERROR_INVALID_SIZE,       /**< Operation failed: invalid size */
    DRMF_ERROR_NOT_IMPLEMENTED,    /**< Operation failed: not yet implemented */
    DRMF_ERROR_FEATURE_NOT_AVAILABLE, /**< Operation failed: not available */
    DRMF_ERROR_NOMEM,              /**< Operation failed: not enough memory */
    DRMF_ERROR_DETAILS_UNKNOWN,    /**< Operation failed: answer not yet known */
    DRMF_ERROR_NOT_FOUND,          /**< Operation failed: query not found */
    DRMF_ERROR_INVALID_CALL,       /**< Operation failed: pre-req for call not met */
    DRMF_ERROR_NOT_ENOUGH_REGS,    /**< Operation failed: not enough registers for use */
    DRMF_ERROR_ACCESS_DENIED,      /**< Operation failed: access denied */
    DRMF_WARNING_ALREADY_INITIALIZED, /**< Operation aborted: already initialized */
    DRMF_ERROR_NOT_INITIALIZED,     /**< Operation failed: not initialized */
    DRMF_ERROR_INVALID_ADDRESS,     /**< Operation failed: invalid address */
    DRMF_WARNING_UNSUPPORTED_KERNEL,  /**< Continuing not advised: unsupported kernel */
} drmf_status_t;


/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRMEMORY_FRAMEWORK_H_ */
