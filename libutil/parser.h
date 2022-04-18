/* **********************************************************
 * Copyright (c) 2005-2006 VMware, Inc.  All rights reserved.
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

#ifndef _DETERMINA_PARSER_H_
#define _DETERMINA_PARSER_H_

#define L_NEWLINE L"\r\n"
#define L_EQUALS L"="

char *
next_token(char *start, SIZE_T *len);

char *
parse_line(char *start, BOOL *done, WCHAR *param, WCHAR *value, SIZE_T maxchars);

char *
parse_line_sep(char *start, char sep, BOOL *done, WCHAR *param, WCHAR *value,
               SIZE_T maxchars);

char *
get_message_block_size(char *start, WCHAR *end_delimiter_w, SIZE_T *size);

void
msg_append(char *msg_buffer, SIZE_T maxchars, WCHAR *data, SIZE_T *accumlen);

void
msg_append_nvp(char *msg_buffer, SIZE_T maxchars, SIZE_T *accumlen, WCHAR *name,
               WCHAR *value);

#endif
