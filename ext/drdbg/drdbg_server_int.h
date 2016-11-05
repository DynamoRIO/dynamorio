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

/* DynamoRIO Debugger Transparency Extension: Debug Server Interface */

#ifndef _DRDBG_SERVER_INT_H_
#define _DRDBG_SERVER_INT_H_ 1

/**
 * @file drdbg_server_int.h
 * @brief Header for DynamoRIO Debugger Transparency Extension
 */

#include "drdbg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRDBG_CMD_NOT_IMPLEMENTED, /* Command not implemented */
    DRDBG_CMD_SERVER_INTERNAL, /* Reserved for server specific packets */
    DRDBG_CMD_QUERY_STOP_RSN,  /* Ask why target stopped */
    DRDBG_CMD_QUERY_CMD,       /* Custom monitor command */
    DRDBG_CMD_REG_READ,        /* Read register(s) */
    DRDBG_CMD_REG_WRITE,       /* Write register(s) */
    DRDBG_CMD_MEM_READ,        /* Read memory */
    DRDBG_CMD_MEM_WRITE,       /* Write memory */
    DRDBG_CMD_CONTINUE,        /* Continue execution */
    DRDBG_CMD_STEP,            /* Single Step */
    DRDBG_CMD_SWBREAK,         /* Software breakpoint */
    DRDBG_CMD_KILL,            /* Kill process */

    DRDBG_CMD_NUM_CMDS         /* Must be last entry */
} drdbg_srv_int_cmd_t;

typedef
drdbg_status_t
(*drdbg_srv_int_start_t)(uint port);

typedef
drdbg_status_t
(*drdbg_srv_int_accept_t)(void);

typedef
drdbg_status_t
(*drdbg_srv_int_stop_t)(void);

typedef struct _drdbg_srv_int_cmd_data_t {
    drdbg_srv_int_cmd_t cmd_id;
    void *cmd_data;
    drdbg_status_t status;
} drdbg_srv_int_cmd_data_t;

typedef
drdbg_status_t
(*drdbg_srv_int_comm_t)(drdbg_srv_int_cmd_data_t *data, bool blocking);

typedef struct _drdbg_srv_int_t {
    drdbg_srv_int_start_t start;
    drdbg_srv_int_accept_t accept;
    drdbg_srv_int_stop_t stop;
    drdbg_srv_int_comm_t get_cmd;
    drdbg_srv_int_comm_t put_cmd;
} drdbg_srv_int_t;

typedef enum {
    DRDBG_STOP_RSN_RECV_SIG
} drdbg_stop_rsn_t;

typedef struct _drdbg_cmd_data_query_stop_rsn_t {
    drdbg_stop_rsn_t stop_rsn;
    int signum;
} drdbg_cmd_data_query_stop_rsn_t;

typedef struct _drdbg_cmd_data_mem_op_t {
    void *addr;
    char *data;
    ssize_t len;
} drdbg_cmd_data_mem_op_t;

typedef struct _drdbg_cmd_data_swbreak_t {
    void *addr;
    int kind;
    bool insert; /* True to add BP, false to remove */
} drdbg_cmd_data_swbreak_t;

typedef struct _drdbg_cmd_data_kill_t {
    unsigned int pid;
} drdbg_cmd_data_kill_t;

typedef struct _drdbg_cmd_data_query_cmd_t {
    char *data;
    ssize_t len;
} drdbg_cmd_data_query_cmd_t;

#ifdef __cplusplus
}
#endif

#endif /* _drdbg_srv_gdb_H_ */
