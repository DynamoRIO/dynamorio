/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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

#ifndef _DR_API_H_
#define _DR_API_H_ 1

/**
 * @file dr_api.h
 * @brief Top-level include file for DynamoRIO API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Defines and type definitions */
#include "dr_defines.h"

/* Event registration routines */
#include "dr_events.h"

/* Application start/stop interface */
#include "dr_app.h"

/* opnd_t (instruction operand) routines */
#include "dr_ir_opnd.h"

/* High-level routines: memory allocation, mutex support, file
 * support, printing, thread support, adaptive optimization,
 * custom traces, processor-specific utilities, trace dumping,
 * and module information.
 */
#include "dr_tools.h"

/* Utility routines for identifying features of the processor. */
#include "dr_proc.h"

/* Instruction convenience & decode/disassemble routines */
#include "dr_ir_utils.h"

/* instrlist_t routines */
#include "dr_ir_instrlist.h"

/* instr_t routines */
#include "dr_ir_instr.h"

/* opcode OP_ constants and opcode-only routines */
#include "dr_ir_opcodes.h"

/* CREATE_INSTR_ macros */
#include "dr_ir_macros.h"

/* Annotation handler registration routines */
#include "dr_annotation.h"

#ifndef DYNAMORIO_STANDALONE
/**
 * When registering a process, users must provide a list of paths to
 * client libraries and their associated client-specific options.  DR
 * looks up "dr_init" in each client library and calls that function
 * when the process starts.  Clients can register to receive callbacks
 * for the various events within dr_init.  Note that client paths and
 * options cannot include semicolons. \p client_id is the ID supplied
 * at registration and is used to identify the client for
 * dr_get_options(), dr_get_client_path(), and external nudges.
 */
DR_EXPORT void dr_init(client_id_t client_id);

/* Version checking */
/* This equals major*100 + minor */
DR_EXPORT LINK_ONCE int _USES_DR_VERSION_ = ${VERSION_NUMBER_INTEGER};
#else
LINK_ONCE int _USES_DR_VERSION_ = ${VERSION_NUMBER_INTEGER};
#endif

#ifdef __cplusplus
}
#endif

#endif /* _DR_API_H_ */
