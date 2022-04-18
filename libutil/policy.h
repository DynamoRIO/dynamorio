/* **********************************************************
 * Copyright (c) 2021-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2007 VMware, Inc.  All rights reserved.
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

 policy definition format specification
-----------------------------------------


the syntax of the policy definition message is as follows:

<policy_message> ::==
 POLICY_VERSION=<string>
 APPINITFLAGS=<string>
 APPINITALLOWLIST=<string>
 APPINITBLOCKLIST=<string>
 GLOBAL_PROTECT=<boolean>
 <application_block>*

<application_block> ::==
 BEGIN_BLOCK
 <app_specifier>
 <dynamorio_option_line>*
 END_BLOCK

<app_specifier> ::== [ GLOBAL | APP_NAME=<string> ]

<dynamorio_option_line> ::== <dynamorio_option>=<string>

<dynamorio_option> ::==
 [ DYNAMORIO_OPTIONS |
   DYNAMORIO_AUTOINJECT |
   DYNAMORIO_RUNUNDER ]

<boolean> ::== [ 0 | 1 ]



details:

(1) the APPINITFLAGS, together with the APPINITBLOCKLIST and
    APPINITALLOWLIST, controls how our bootstrap dll is added to the
    AppInit_DLLs registry key. the value of the flags should be a sum
    of the APPINIT_* flags as defined in share/config.h

    the APPINITBLOCKLIST and APPINITALLOWLIST values are only used if
    specified by the flags. we'll provide the allowlist/blocklist.


(2) GLOBAL_PROTECT: OPTIONAL: if this is 0, then protection is
    disabled (and all application blocks are optional). in normal
    situations this will be 1. if this is not specified, then the
    current setting is kept (see also enable_protection)


(3) there must be a GLOBAL block, which should come first, and it must
have DYNAMORIO_RUNUNDER=1 set as an option. for consistency it should
set DYNAMORIO_OPTIONS to blank (DYNAMORIO_OPTIONS=).


(4) the APP_NAME must be a valid "application id". this is usually the
.exe filename, but can also be "qualified". we will provide a complete
list of supported applications.


(5) some applications will also require special values for
DYNAMORIO_RUNUNDER; we will provide this information as well.


(6) DYNAMORIO_AUTOINJECT must be specified for every non-global
application block, and it should be of the format
"\lib\NNNNN\dynamorio.dll", where NNNNN is the version number of the
core dll being used. in general, any value that starts with a "\" will
have the local installation path prepended when using the actual
value (eg, it indicates a relative installation path).


(7) DYNAMORIO_OPTIONS is a string consisting of protection
options to our core. the variability in this will depend on the level
of configurability desired in your UI; but the majority of the options
will not need to change. probably the only configuration necessary
will be whether an app should be on or off; and this should be
controlled by including or excluding the corresponding app block.


see sample.mfp for an example policy string:


*/

#ifndef _DETERMINA_POLICY_H_
#define _DETERMINA_POLICY_H_

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POLICY_DEF_KEYS       \
    MSG_FIELD(APP_NAME)       \
    MSG_FIELD(GLOBAL)         \
    MSG_FIELD(BEGIN_BLOCK)    \
    MSG_FIELD(END_BLOCK)      \
    MSG_FIELD(GLOBAL_PROTECT) \
    MSG_FIELD(BEGIN_MP_MODES) \
    MSG_FIELD(END_MP_MODES)

typedef enum {
#define MSG_FIELD(x) MSGKEY_##x,
    POLICY_DEF_KEYS
#undef MSG_FIELD
        MSGKEY_BAD_FIELD
} msg_id;

char *
parse_policy_line(char *start, BOOL *done, msg_id *mfield, WCHAR *param, WCHAR *value,
                  SIZE_T maxchars);

DWORD
parse_policy(char *policy_definition,
             /* OUT */ ConfigGroup **config,
             /* OUT */ ConfigGroup **options, BOOL validating);

#ifdef __cplusplus
}
#endif

#endif
