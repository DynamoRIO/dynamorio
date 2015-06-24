/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _DREXT_H_
#define _DREXT_H_ 1

/**
 * @file drext.h
 * @brief Common header for DynamoRIO extensions
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Indicates the status of an extension API call. For more details, see the documentation
 * for the specific API call.
 */
typedef enum _drext_status_t {
    /**
     * The API call completed successfully.
     */
    DREXT_SUCCESS,
    /**
     * A general error occurred.
     */
    DREXT_ERROR,
    /**
     * The internal state of the feature is incompatible with the called API function.
     * Consult the documentation for the API function to determine the conditions under
     * which it may be called.
     */
    DREXT_ERROR_INCOMPATIBLE_STATE,
    /**
     * The operation is not currently implemented, though may be implemented at some
     * time in the future.
     */
    DREXT_ERROR_NOT_IMPLEMENTED
} drext_status_t;

#ifdef __cplusplus
}
#endif

#endif /* _DREXT_H_ */
