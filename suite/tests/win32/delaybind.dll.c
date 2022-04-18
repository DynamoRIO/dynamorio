/* **********************************************************
 * Copyright (c) 2006 VMware, Inc.  All rights reserved.
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

/* case 8507 delay load testing */

#include "tools.h"
#include <windows.h>

/* note that we want _this_ DLL bound normally to advapi32.dll  */

/* too bad that most of the useful functions in advapi32
 * or aren't present on NT [ConvertSidToStringSid()]  or 2000 [CreateWellKnownSid()]
 */

/* Windows NT 4.0 and earlier: ConvertSidToStringSid and
 *  ConvertStringSidToSid are not supported. Use the following code to
 *   convert a SID to string format.
 */

BOOL
GetTextualSid(PSID pSid,            // binary SID
              LPTSTR TextualSid,    // buffer for Textual representation of SID
              LPDWORD lpdwBufferLen // required/provided TextualSid buffersize
)
{
    PSID_IDENTIFIER_AUTHORITY psia;
    DWORD dwSubAuthorities;
    DWORD dwSidRev = SID_REVISION;
    DWORD dwCounter;
    DWORD dwSidSize;

    // Validate the binary SID.

    if (!IsValidSid(pSid))
        return FALSE;

    // Get the identifier authority value from the SID.

    psia = GetSidIdentifierAuthority(pSid);

    // Get the number of subauthorities in the SID.

    dwSubAuthorities = *GetSidSubAuthorityCount(pSid);

    // Compute the buffer length.
    // S-SID_REVISION- + IdentifierAuthority- + subauthorities- + NULL

    dwSidSize = (15 + 12 + (12 * dwSubAuthorities) + 1) * sizeof(TCHAR);

    // Check input buffer length.
    // If too small, indicate the proper size and set the last error.

    if (*lpdwBufferLen < dwSidSize) {
        *lpdwBufferLen = dwSidSize;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    // Add 'S' prefix and revision number to the string.

    dwSidSize = wsprintf(TextualSid, TEXT("S-%lu-"), dwSidRev);

    // Add a SID identifier authority to the string.

    if ((psia->Value[0] != 0) || (psia->Value[1] != 0)) {
        dwSidSize += wsprintf(
            TextualSid + lstrlen(TextualSid), TEXT("0x%02hx%02hx%02hx%02hx%02hx%02hx"),
            (USHORT)psia->Value[0], (USHORT)psia->Value[1], (USHORT)psia->Value[2],
            (USHORT)psia->Value[3], (USHORT)psia->Value[4], (USHORT)psia->Value[5]);
    } else {
        dwSidSize +=
            wsprintf(TextualSid + lstrlen(TextualSid), TEXT("%lu"),
                     (ULONG)(psia->Value[5]) + (ULONG)(psia->Value[4] << 8) +
                         (ULONG)(psia->Value[3] << 16) + (ULONG)(psia->Value[2] << 24));
    }

    // Add SID subauthorities to the string.
    //
    for (dwCounter = 0; dwCounter < dwSubAuthorities; dwCounter++) {
        dwSidSize += wsprintf(TextualSid + dwSidSize, TEXT("-%lu"),
                              *GetSidSubAuthority(pSid, dwCounter));
    }

    return TRUE;
}

void
test_sid(void)
{
    TCHAR buf[100];
    DWORD size = sizeof(buf);

    PSID psid;

    /*  from
     * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/secauthz/security/searching_for_a_sid_in_an_access_token_in_c__.asp
     */

    SID_IDENTIFIER_AUTHORITY SIDAuth = SECURITY_NT_AUTHORITY;
    BOOL ok;

    // Create a SID for the BUILTIN\Administrators group.

    if (!AllocateAndInitializeSid(&SIDAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &psid)) {
        print("AllocateAndInitializeSid Error %u\n", GetLastError());
        return;
    }

    ok = GetTextualSid(psid, buf, &size);

    if (ok) {
        print("BUILTIN\\Administrators: %s\n", buf);
    } else {
        print("FAILED!\n", buf);

        if (!IsValidSid(psid))
            print("invalid SID!\n", buf);
    }
}

int __declspec(dllexport) make_a_lib(int arg)
{
    test_sid();
    return 0;
}

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH: print("in delay bind dll\n"); break;
    }
    return TRUE;
}
