/* **********************************************************
 * Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
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

/*
 *
 * internal common header file for share interface
 *
 */

#ifndef _DETERMINA_SHARE_H_
#define _DETERMINA_SHARE_H_

#include "configure.h"

#ifdef WINDOWS
/*
C:\PROGRAM FILES\MICROSOFT VISUAL STUDIO\VC98\INCLUDE\rpcasync.h(45) :
warning C4115: '_RPC_ASYNC_STATE' : named type definition in parentheses
*/
#    pragma warning(disable : 4115)

#    include "mfapi.h"
#endif /* WINDOWS */

#ifdef __cplusplus
extern "C" {
#endif

/* make sure; for example, mmcgui shouldn't have to know about this */
#ifndef NOT_DYNAMORIO_CORE
#    define NOT_DYNAMORIO_CORE
#endif

/* FIXME: this should be stripped out of the core at some point...*/
#ifndef HOT_PATCHING_INTERFACE
#    define HOT_PATCHING_INTERFACE
#endif

#include "globals_shared.h"

#ifdef __cplusplus
}
#endif

#include "utils.h"

#endif
