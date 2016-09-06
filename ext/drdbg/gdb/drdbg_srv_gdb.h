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

/* DynamoRIO Debugger Transparency Extension: GDB Server */

#ifndef _drdbg_srv_gdb_H_
#define _drdbg_srv_gdb_H_ 1

/**
 * @file drdbg_srv_gdb.h
 * @brief Header for DynamoRIO Debugger Transparency Extension
 */

#include "drdbg.h"
#include "../drdbg_server_int.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef drdbg_status_t (*drdbg_gdb_cmd_func_t)(int cmd_index, char *buf, int len,
                                               drdbg_srv_int_cmd_t *cmd,
                                               void **cmd_args);
typedef drdbg_status_t (*drdbg_gdb_put_cmd_func_t)(drdbg_srv_int_cmd_t *cmd,
                                                   void **cmd_args);

typedef struct _gdb_cmd_t {
    const drdbg_srv_int_cmd_t cmd_id;
    const char *cmd_str;
    const drdbg_gdb_cmd_func_t get;
} gdb_cmd_t;

extern const gdb_cmd_t SUPPORTED_CMDS[];

/* GDB Remote Protocol packet prefixes */
typedef enum {
    DRDBG_GDB_CMD_PREFIX_MULTI = 'v',
    DRDBG_GDB_CMD_PREFIX_QUERY = 'q',
    DRDBG_GDB_CMD_PREFIX_QUERY_SET = 'Q'
} DRDBG_GDB_CMD_PREFIX;

drdbg_status_t
drdbg_srv_gdb_init(drdbg_srv_int_t *dbg_server);

#ifdef __cplusplus
}
#endif

#endif /* _drdbg_srv_gdb_H_ */
