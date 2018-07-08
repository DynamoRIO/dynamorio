/* **********************************************************
 * Copyright (c) 2005 VMware, Inc.  All rights reserved.
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

/* case 4175 had this sequence of Flush */
#include "tools.h"

int
main()
{
    void *p;
    BOOL res;
    int size = 0x04000;
    int commit = 0;
    int version;

    INIT();
    version = get_windows_version();
    assert(version != 0);

    p = VirtualAlloc(0, size, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    assert(p);
    print("alloced %d\n", size);

    commit += 0x1000;
    p = VirtualAlloc(p, commit, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    assert(p);
    print("committed %d\n", commit);

    commit += 0x1000;
    p = VirtualAlloc(p, commit, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    assert(p);
    print("committed %d\n", commit);

    /* in fact nothing prevents a flush on non-reserved memory */
    if (1)
        commit += 0x1000;
    res = FlushInstructionCache(GetCurrentProcess(), p, commit);
    assert(res);
    print("flushed %d\n", commit);

    commit += 0x1000;
    p = VirtualAlloc(p, commit, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    assert(p);
    print("committed %d\n", commit);

    /* FIXME: should add VirtualQuery calls here to verify it all */
    /* case 4494 - VirtualFree special cases */
    res = VirtualFree((char *)p + 0x2000 - 1, 3, MEM_DECOMMIT);
    print("attempting to decommit 3 byte cross-page 0 - should decommit two pages\n",
          commit);
    assert(res);

    /* LastErrorValue: (Win32) 0x1e7 (487) - Attempt to access invalid
     * address.  LastStatusValue: (NTSTATUS) 0xc000009f - Virtual
     * memory cannot be freed as base address is not the base of the
     * region and a region size of zero was specified.
     */

    res = VirtualFree((char *)p + 0x3040, 0, MEM_DECOMMIT);
    print("attempting to decommitted 3 byte cross-page 0 - should fail\n");
    assert(res == 0 && GetLastError() == 487);

    res = VirtualFree((char *)p + 0x10, commit, MEM_DECOMMIT);
    print("decommitting (p+0x10, %d) (gets backwards aligned) - should hopefully fail\n",
          commit);
    /*  LastErrorValue: (Win32) 0x57 (87) - The parameter is incorrect. */
    /* LastStatusValue: (NTSTATUS) 0xc000001a - Virtual memory cannot be freed. */
    assert(res == 0 && GetLastError() == 87);

    res = VirtualFree((char *)p + 0x10, 0, MEM_DECOMMIT);
    print("decommitted size 0 and p (gets backwards aligned) - should decommit whole "
          "region\n");
    if (version == WINDOWS_VERSION_NT) {
        /* on NT NtFreeVirtualMemory does NOT back-align the base and fails instead */
        /* FIXME: change message above -- but then have to change template */
        assert(res == 0 && GetLastError() == 487);
    } else {
        assert(res);
    }

    res = VirtualFree((char *)p + 0, commit, MEM_DECOMMIT);
    print("decommitting (p+0x0, %d) - should be ok\n", commit);
    assert(res);

    /* MEM_RELEASE tests */
    res = VirtualFree((char *)p + 0x3010, 0, MEM_RELEASE);
    print("releasing p+0x3010 - should fail\n");
    assert(res == 0 && GetLastError() == 487);

    res = VirtualFree((char *)p + 0x10, 0, MEM_RELEASE);
    print("releasing p+0x10 - will actually free\n");
    if (version == WINDOWS_VERSION_NT) {
        /* on NT NtFreeVirtualMemory does NOT back-align the base and fails instead */
        /* FIXME: change message above -- but then have to change template */
        assert(res == 0 && GetLastError() == 487);
    } else {
        assert(res);
    }

    p = VirtualAlloc(0, size, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    assert(p);
    print("alloced again %d\n", size);

    res = VirtualFree(p, 0, MEM_RELEASE);
    print("released p\n");
    assert(res);

    print("Successful\n");
}
