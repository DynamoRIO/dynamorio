/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2003 VMware, Inc.  All rights reserved.
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

/* basic block builder may build infinite loop here */

int
main()
{
#if defined(__i386__) || defined(__x86_64__)
    __asm__("jmp    aroundexit");
    __asm__("doexit:          ");
    __asm__("       movl   $1,%eax       # exit");
    __asm__("       movl   $0,%ebx       # exit code");
    __asm__("       int    $0x80         # kernel");
    __asm__("aroundexit: call doexit");
#elif defined(__aarch64__)
    __asm__("b      aroundexit");
    __asm__("doexit:          ");
    __asm__("       mov    w8, #94       // exit_group");
    __asm__("       mov    w0, #0        // exit code");
    __asm__("       svc    #0            // kernel");
    __asm__("aroundexit: bl doexit");
#elif defined(__arm__)
    __asm__("b      aroundexit");
    __asm__("doexit:          ");
    __asm__("       mov    r7, #248      @ exit_group");
    __asm__("       mov    r0, #0        @ exit code");
    __asm__("       svc    0             @ kernel");
    __asm__("aroundexit: bl doexit");
#elif defined(__riscv) && __riscv_xlen == 64
    __asm__("j      aroundexit");
    __asm__("doexit:          ");
    __asm__("       li      a7, 93       # exit");
    __asm__("       li      a0, 0        # exit code");
    __asm__("       ecall                # kernel");
    __asm__("aroundexit: jal doexit");
#else
#    error NYI
#endif
    return 0;
}
