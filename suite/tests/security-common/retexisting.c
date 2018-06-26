/* **********************************************************
 * Copyright (c) 2012-2015 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2004 VMware, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY

#    include "tools.h"

/* In ASM_CODE_ONLY. */
void
precious_push_fake_retaddr(void);

void
precious(void)
{
#    ifdef USER32 /* map user32.dll for a RunAll test */
    MessageBeep(0);
#    endif
    print("M-m-my PRECIOUS is stolen! ATTACK SUCCESSFUL!\n");
    exit(1);
}

void
ring(void **retaddr_p)
{
    print("looking at ring\n");
    *retaddr_p = (void *)&precious_push_fake_retaddr;
}

ptr_int_t
foo(void)
{
    print("in foo\n");
    return 1;
}

ptr_int_t
bar(void)
{
    print("in bar\n");
    return 3;
}

ptr_int_t
twofoo(void)
{
    ptr_int_t a = foo();
    print("first foo a=" SZFMT "\n", a);

    a += foo();
    print("second foo a=" SZFMT "\n", a);
    return a;
}

int
main(void)
{
    INIT();

    print("starting good function\n");
    twofoo();
    print("starting bad function\n");
    call_with_retaddr(ring);
    print("all done [not seen]\n");
}

#else

#    include "asm_defines.asm"

/* clang-format off */
START_FILE

DECL_EXTERN(precious)

/* ring() returns to this code, at which point we have exactly 16-byte
 * alignment.  However, the ABI expects there to be a retaddr on the stack and
 * the stack to be 16-byte alinged minus 8.  We use this trampoline to push a
 * fake retaddr to match the ABI.
 */
    DECLARE_FUNC(precious_push_fake_retaddr)
GLOBAL_LABEL(precious_push_fake_retaddr:)
        push     0          /* Fake retaddr, will crash if it returns. */
        jmp      GLOBAL_REF(precious)    /* no return */
        END_FUNC(precious_push_fake_retaddr)

END_FILE
/* clang-format on */

#endif
