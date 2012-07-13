/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

#include "tools.h"

#ifdef USE_DYNAMO
# include "dynamorio.h"
#endif

#ifdef X64
/* Reduced from V8, which uses x64 absolute addresses in code which ends up
 * being sandboxed.  The original code is not self-modifying, but is flushed
 * enough to trigger sandboxing.
 *
 * 0x000034b7f8b11366:  48 a1 48 33 51 36 ff 7e 00 00   movabs 0x7eff36513348,%rax
 * ...
 * 0x000034b7f8b11311:  48 a3 20 13 51 36 ff 7e 00 00   movabs %rax,0x7eff36511320
 */
void
test_mov_abs(void)
{
    char *rwx_mem = allocate_mem(4096, ALLOW_READ|ALLOW_WRITE|ALLOW_EXEC);
    char *pc = rwx_mem;
    void *(*do_selfmod_abs)(void);
    void *out_val = 0;
    uint64 *global_addr = (uint64 *) pc;

    /* Put a 64-bit 0xdeadbeefdeadbeef into mapped memory.  Typically most
     * memory from mmap is outside the low 4 GB, so this makes sure that any
     * mangling we do avoids address truncation.
     */
    *(uint64 *)pc = 0xdeadbeefdeadbeefULL;
    pc += 8;

    /* Encode an absolute load and store.  If we write it in assembly, gas picks
     * the wrong encoding, so we manually encode it here.  It has to be on the
     * same page as the data to trigger sandboxing.
     */
    do_selfmod_abs = (void *(*)(void))pc;
    *pc++ = 0x48;  /* REX.W */
    *pc++ = 0xa1;  /* movabs load -> rax */
    *(uint64 **)pc = global_addr;
    pc += 8;
    *pc++ = 0x48;  /* REX.W */
    *pc++ = 0xa3;  /* movabs store <- rax */
    *(uint64 **)pc = global_addr;
    pc += 8;
    *pc++ = 0xc3;  /* ret */

    print("before do_selfmod_abs\n");
    out_val = do_selfmod_abs();
    print("%p\n", out_val);
    /* rwx_mem is leaked, tools.h doesn't give us a way to free it. */
}

/* FIXME: Test reladdr. */
#endif /* X64 */

void
foo(int iters)
{
    int total = code_self_mod(iters);
    print("Executed 0x%x iters\n", total);
}

int
main()
{
    INIT();

#ifdef USE_DYNAMO
    dynamorio_app_init();
    dynamorio_app_start();
#endif

    /* make foo code writable */
    protect_mem(code_self_mod, 1024, ALLOW_READ|ALLOW_WRITE|ALLOW_EXEC);
    // Note that main and the exception handler __except_handler3 are on this page too

    foo(0xabcd);
    foo(0x1234);
    foo(0xef01);

#ifdef X64
    test_mov_abs();
#endif

#ifdef USE_DYNAMO
    dynamorio_app_stop();
    dynamorio_app_exit();
#endif

    return 0;
}
