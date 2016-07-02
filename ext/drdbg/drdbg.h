/* **********************************************************
 * Copyright (c) 2016 Google, Inc.   All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* DynamoRIO Debugger Transparency Extension: an extension for
 * mantaining transparent debugging of targets running
 * under DynamoRIO.
 */

#ifndef _DRDBG_H_
#define _DRDBG_H_ 1

/**
 * @file drreg.h
 * @brief Header for DynamoRIO Debugger Transparency Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

#define END_SWAP_UINT32(_num) \
    ((((_num)>>24)&0xff)     |  /* move byte 3 to byte 0 */ \
     (((_num)<<8) &0xff0000) |  /* move byte 1 to byte 2 */ \
     (((_num)>>8) &0xff00)   |  /* move byte 2 to byte 1 */ \
     (((_num)<<24)&0xff000000)) // byte 0 to byte 3

typedef enum {
    DRDBG_EVENT_BP
} DRDBG_EVENT;

typedef struct _drdbg_event_t {
    DRDBG_EVENT event;
    void *data;
} drdbg_event_t;

typedef enum {
    DRDBG_BP_DISABLED,
    DRDBG_BP_ENABLED,
    DRDBG_BP_PENDING,
    DRDBG_BP_REMOVED
} DRDBG_BP_STATUS;

typedef struct _drdbg_bp_t {
    app_pc pc;
    DRDBG_BP_STATUS status;
    void *tag;
} drdbg_bp_t;

typedef struct _drdbg_event_data_bp_t {
    drdbg_bp_t *bp;
    //dr_mcontext_t mcontext;
    bool keep_waiting;
} drdbg_event_data_bp_t;

typedef enum {
    DRDBG_SUCCESS,  /* Operation succeeded. */
    DRDBG_ERROR,    /* Operation failed. */
} drdbg_status_t;

typedef struct _drdbg_options_t {
    /* Port to listen on for debugger */
    uint port;
} drdbg_options_t;

typedef drdbg_status_t (*drdbg_handler_t)(void **arg);

DR_EXPORT
drdbg_status_t
drdbg_init(drdbg_options_t *ops);

DR_EXPORT
__attribute__((destructor))
drdbg_status_t
drdbg_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* _DRDBG_H_ */
