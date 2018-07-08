/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2010 VMware, Inc.  All rights reserved.
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

/* Test having a selfmod bb with a ton of write instrs that creates a too-big
 * selfmod fragment.  Xref case 7893.
 */

#ifndef ASM_CODE_ONLY

#    include "tools.h"

#    ifdef USE_DYNAMO
#        include "dynamorio.h"
#    endif

uint big[1024];

/* executes iters iters, by modifying the iters bound using self-modifying code
 */
int
foo(int iters);

int
main(void)
{
    INIT();

    print("Executed 0x%x iters\n", foo(0xabcd));
    print("Executed 0x%x iters\n", foo(0x1234));
    print("Executed 0x%x iters\n", foo(0xef01));

    return 0;
}

#else /* ASM_CODE_ONLY */

#    include "asm_defines.asm"

/* clang-format off */
START_FILE

DECL_EXTERN(big)
DECL_EXTERN(protect_mem)

    /* Returns the PC of the next instruction in XAX.  Clobbers no registers.
     */
#define FUNCNAME get_next_pc
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, [REG_XSP]
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME foo
        DECLARE_FUNC(FUNCNAME)
DECLARE_GLOBAL(foo_start)
DECLARE_GLOBAL(foo_end)
GLOBAL_LABEL(FUNCNAME:)
ADDRTAKEN_LABEL(foo_start:)
        mov      REG_XCX, ARG1  /* iters */
        push     REG_XBP
        push     REG_XBX
        push     REG_XDX
        /* 16-aligned on x64 */

        /* Make this memory RWX. */
        mov      REG_XBX, REG_XCX               /* save iters across call */
        lea      REG_XAX, SYMREF(foo_start)
        lea      REG_XDX, SYMREF(foo_end)
        sub      REG_XDX, REG_XAX               /* compute precise length */
        CALLC3(GLOBAL_REF(protect_mem), REG_XAX, REG_XDX, HEX(7)/*rwx*/)
        mov      REG_XCX, REG_XBX               /* restore iters after call */

        lea      REG_XDX, SYMREF(immed_plus_four)
        mov      DWORD [REG_XDX - 4], ecx /* the modifying store */
        mov      eax, HEX(12345678) /* this instr's immed gets overwritten */
    immed_plus_four:

        /* now we have as many write instrs as necessary to cause a too-big
         * selfmod fragment.  xref case 7893.
         */
#if defined(LINUX) && defined(X64)
        lea      REG_XDX, SYMREF(big) /* rip-relative on x64 */
#else
        mov      REG_XDX, offset GLOBAL_REF(big)
#endif
        mov      DWORD [REG_XDX + 0], ecx
        mov      DWORD [REG_XDX + 1], ecx
        mov      DWORD [REG_XDX + 2], ecx
        mov      DWORD [REG_XDX + 3], ecx
        mov      DWORD [REG_XDX + 4], ecx
        mov      DWORD [REG_XDX + 5], ecx
        mov      DWORD [REG_XDX + 6], ecx
        mov      DWORD [REG_XDX + 7], ecx
        mov      DWORD [REG_XDX + 8], ecx
        mov      DWORD [REG_XDX + 9], ecx
        mov      DWORD [REG_XDX + 10], ecx
        mov      DWORD [REG_XDX + 11], ecx
        mov      DWORD [REG_XDX + 12], ecx
        mov      DWORD [REG_XDX + 13], ecx
        mov      DWORD [REG_XDX + 14], ecx
        mov      DWORD [REG_XDX + 15], ecx
        mov      DWORD [REG_XDX + 16], ecx
        mov      DWORD [REG_XDX + 17], ecx
        mov      DWORD [REG_XDX + 18], ecx
        mov      DWORD [REG_XDX + 19], ecx
        mov      DWORD [REG_XDX + 20], ecx
        mov      DWORD [REG_XDX + 21], ecx
        mov      DWORD [REG_XDX + 22], ecx
        mov      DWORD [REG_XDX + 23], ecx
        mov      DWORD [REG_XDX + 24], ecx
        mov      DWORD [REG_XDX + 25], ecx
        mov      DWORD [REG_XDX + 26], ecx
        mov      DWORD [REG_XDX + 27], ecx
        mov      DWORD [REG_XDX + 28], ecx
        mov      DWORD [REG_XDX + 29], ecx
        mov      DWORD [REG_XDX + 30], ecx
        mov      DWORD [REG_XDX + 31], ecx
        mov      DWORD [REG_XDX + 32], ecx
        mov      DWORD [REG_XDX + 33], ecx
        mov      DWORD [REG_XDX + 34], ecx
        mov      DWORD [REG_XDX + 35], ecx
        mov      DWORD [REG_XDX + 36], ecx
        mov      DWORD [REG_XDX + 37], ecx
        mov      DWORD [REG_XDX + 38], ecx
        mov      DWORD [REG_XDX + 39], ecx
        mov      DWORD [REG_XDX + 40], ecx
        mov      DWORD [REG_XDX + 41], ecx
        mov      DWORD [REG_XDX + 42], ecx
        mov      DWORD [REG_XDX + 43], ecx
        mov      DWORD [REG_XDX + 44], ecx
        mov      DWORD [REG_XDX + 45], ecx
        mov      DWORD [REG_XDX + 46], ecx
        mov      DWORD [REG_XDX + 47], ecx
        mov      DWORD [REG_XDX + 48], ecx
        mov      DWORD [REG_XDX + 49], ecx
        mov      DWORD [REG_XDX + 50], ecx
        mov      DWORD [REG_XDX + 51], ecx
        mov      DWORD [REG_XDX + 52], ecx
        mov      DWORD [REG_XDX + 53], ecx
        mov      DWORD [REG_XDX + 54], ecx
        mov      DWORD [REG_XDX + 55], ecx
        mov      DWORD [REG_XDX + 56], ecx
        mov      DWORD [REG_XDX + 57], ecx
        mov      DWORD [REG_XDX + 58], ecx
        mov      DWORD [REG_XDX + 59], ecx
        mov      DWORD [REG_XDX + 60], ecx
        mov      DWORD [REG_XDX + 61], ecx
        mov      DWORD [REG_XDX + 62], ecx
        mov      DWORD [REG_XDX + 63], ecx
        mov      DWORD [REG_XDX + 64], ecx
        mov      DWORD [REG_XDX + 65], ecx
        mov      DWORD [REG_XDX + 66], ecx
        mov      DWORD [REG_XDX + 67], ecx
        mov      DWORD [REG_XDX + 68], ecx
        mov      DWORD [REG_XDX + 69], ecx
        mov      DWORD [REG_XDX + 70], ecx
        mov      DWORD [REG_XDX + 71], ecx
        mov      DWORD [REG_XDX + 72], ecx
        mov      DWORD [REG_XDX + 73], ecx
        mov      DWORD [REG_XDX + 74], ecx
        mov      DWORD [REG_XDX + 75], ecx
        mov      DWORD [REG_XDX + 76], ecx
        mov      DWORD [REG_XDX + 77], ecx
        mov      DWORD [REG_XDX + 78], ecx
        mov      DWORD [REG_XDX + 79], ecx
        mov      DWORD [REG_XDX + 80], ecx
        mov      DWORD [REG_XDX + 81], ecx
        mov      DWORD [REG_XDX + 82], ecx
        mov      DWORD [REG_XDX + 83], ecx
        mov      DWORD [REG_XDX + 84], ecx
        mov      DWORD [REG_XDX + 85], ecx
        mov      DWORD [REG_XDX + 86], ecx
        mov      DWORD [REG_XDX + 87], ecx
        mov      DWORD [REG_XDX + 88], ecx
        mov      DWORD [REG_XDX + 89], ecx
        mov      DWORD [REG_XDX + 90], ecx
        mov      DWORD [REG_XDX + 91], ecx
        mov      DWORD [REG_XDX + 92], ecx
        mov      DWORD [REG_XDX + 93], ecx
        mov      DWORD [REG_XDX + 94], ecx
        mov      DWORD [REG_XDX + 95], ecx
        mov      DWORD [REG_XDX + 96], ecx
        mov      DWORD [REG_XDX + 97], ecx
        mov      DWORD [REG_XDX + 98], ecx
        mov      DWORD [REG_XDX + 99], ecx
        mov      DWORD [REG_XDX + 100], ecx
        mov      DWORD [REG_XDX + 101], ecx
        mov      DWORD [REG_XDX + 102], ecx
        mov      DWORD [REG_XDX + 103], ecx
        mov      DWORD [REG_XDX + 104], ecx
        mov      DWORD [REG_XDX + 105], ecx
        mov      DWORD [REG_XDX + 106], ecx
        mov      DWORD [REG_XDX + 107], ecx
        mov      DWORD [REG_XDX + 108], ecx
        mov      DWORD [REG_XDX + 109], ecx
        mov      DWORD [REG_XDX + 110], ecx
        mov      DWORD [REG_XDX + 111], ecx
        mov      DWORD [REG_XDX + 112], ecx
        mov      DWORD [REG_XDX + 113], ecx
        mov      DWORD [REG_XDX + 114], ecx
        mov      DWORD [REG_XDX + 115], ecx
        mov      DWORD [REG_XDX + 116], ecx
        mov      DWORD [REG_XDX + 117], ecx
        mov      DWORD [REG_XDX + 118], ecx
        mov      DWORD [REG_XDX + 119], ecx
        mov      DWORD [REG_XDX + 120], ecx
        mov      DWORD [REG_XDX + 121], ecx
        mov      DWORD [REG_XDX + 122], ecx
        mov      DWORD [REG_XDX + 123], ecx
        mov      DWORD [REG_XDX + 124], ecx
        mov      DWORD [REG_XDX + 125], ecx
        mov      DWORD [REG_XDX + 126], ecx
        mov      DWORD [REG_XDX + 127], ecx
        mov      DWORD [REG_XDX + 128], ecx
        mov      DWORD [REG_XDX + 129], ecx
        mov      DWORD [REG_XDX + 130], ecx
        mov      DWORD [REG_XDX + 131], ecx
        mov      DWORD [REG_XDX + 132], ecx
        mov      DWORD [REG_XDX + 133], ecx
        mov      DWORD [REG_XDX + 134], ecx
        mov      DWORD [REG_XDX + 135], ecx
        mov      DWORD [REG_XDX + 136], ecx
        mov      DWORD [REG_XDX + 137], ecx
        mov      DWORD [REG_XDX + 138], ecx
        mov      DWORD [REG_XDX + 139], ecx
        mov      DWORD [REG_XDX + 140], ecx
        mov      DWORD [REG_XDX + 141], ecx
        mov      DWORD [REG_XDX + 142], ecx
        mov      DWORD [REG_XDX + 143], ecx
        mov      DWORD [REG_XDX + 144], ecx
        mov      DWORD [REG_XDX + 145], ecx
        mov      DWORD [REG_XDX + 146], ecx
        mov      DWORD [REG_XDX + 147], ecx
        mov      DWORD [REG_XDX + 148], ecx
        mov      DWORD [REG_XDX + 149], ecx
        mov      DWORD [REG_XDX + 150], ecx
        mov      DWORD [REG_XDX + 151], ecx
        mov      DWORD [REG_XDX + 152], ecx
        mov      DWORD [REG_XDX + 153], ecx
        mov      DWORD [REG_XDX + 154], ecx
        mov      DWORD [REG_XDX + 155], ecx
        mov      DWORD [REG_XDX + 156], ecx
        mov      DWORD [REG_XDX + 157], ecx
        mov      DWORD [REG_XDX + 158], ecx
        mov      DWORD [REG_XDX + 159], ecx
        mov      DWORD [REG_XDX + 160], ecx
        mov      DWORD [REG_XDX + 161], ecx
        mov      DWORD [REG_XDX + 162], ecx
        mov      DWORD [REG_XDX + 163], ecx
        mov      DWORD [REG_XDX + 164], ecx
        mov      DWORD [REG_XDX + 165], ecx
        mov      DWORD [REG_XDX + 166], ecx
        mov      DWORD [REG_XDX + 167], ecx
        mov      DWORD [REG_XDX + 168], ecx
        mov      DWORD [REG_XDX + 169], ecx
        mov      DWORD [REG_XDX + 170], ecx
        mov      DWORD [REG_XDX + 171], ecx
        mov      DWORD [REG_XDX + 172], ecx
        mov      DWORD [REG_XDX + 173], ecx
        mov      DWORD [REG_XDX + 174], ecx
        mov      DWORD [REG_XDX + 175], ecx
        mov      DWORD [REG_XDX + 176], ecx
        mov      DWORD [REG_XDX + 177], ecx
        mov      DWORD [REG_XDX + 178], ecx
        mov      DWORD [REG_XDX + 179], ecx
        mov      DWORD [REG_XDX + 180], ecx
        mov      DWORD [REG_XDX + 181], ecx
        mov      DWORD [REG_XDX + 182], ecx
        mov      DWORD [REG_XDX + 183], ecx
        mov      DWORD [REG_XDX + 184], ecx
        mov      DWORD [REG_XDX + 185], ecx
        mov      DWORD [REG_XDX + 186], ecx
        mov      DWORD [REG_XDX + 187], ecx
        mov      DWORD [REG_XDX + 188], ecx
        mov      DWORD [REG_XDX + 189], ecx
        mov      DWORD [REG_XDX + 190], ecx
        mov      DWORD [REG_XDX + 191], ecx
        mov      DWORD [REG_XDX + 192], ecx
        mov      DWORD [REG_XDX + 193], ecx
        mov      DWORD [REG_XDX + 194], ecx
        mov      DWORD [REG_XDX + 195], ecx
        mov      DWORD [REG_XDX + 196], ecx
        mov      DWORD [REG_XDX + 197], ecx
        mov      DWORD [REG_XDX + 198], ecx
        mov      DWORD [REG_XDX + 199], ecx
        mov      DWORD [REG_XDX + 200], ecx
        mov      DWORD [REG_XDX + 201], ecx
        mov      DWORD [REG_XDX + 202], ecx
        mov      DWORD [REG_XDX + 203], ecx
        mov      DWORD [REG_XDX + 204], ecx
        mov      DWORD [REG_XDX + 205], ecx
        mov      DWORD [REG_XDX + 206], ecx
        mov      DWORD [REG_XDX + 207], ecx
        mov      DWORD [REG_XDX + 208], ecx
        mov      DWORD [REG_XDX + 209], ecx
        mov      DWORD [REG_XDX + 210], ecx
        mov      DWORD [REG_XDX + 211], ecx
        mov      DWORD [REG_XDX + 212], ecx
        mov      DWORD [REG_XDX + 213], ecx
        mov      DWORD [REG_XDX + 214], ecx
        mov      DWORD [REG_XDX + 215], ecx
        mov      DWORD [REG_XDX + 216], ecx
        mov      DWORD [REG_XDX + 217], ecx
        mov      DWORD [REG_XDX + 218], ecx
        mov      DWORD [REG_XDX + 219], ecx
        mov      DWORD [REG_XDX + 220], ecx
        mov      DWORD [REG_XDX + 221], ecx
        mov      DWORD [REG_XDX + 222], ecx
        mov      DWORD [REG_XDX + 223], ecx
        mov      DWORD [REG_XDX + 224], ecx
        mov      DWORD [REG_XDX + 225], ecx
        mov      DWORD [REG_XDX + 226], ecx
        mov      DWORD [REG_XDX + 227], ecx
        mov      DWORD [REG_XDX + 228], ecx
        mov      DWORD [REG_XDX + 229], ecx
        mov      DWORD [REG_XDX + 230], ecx
        mov      DWORD [REG_XDX + 231], ecx
        mov      DWORD [REG_XDX + 232], ecx
        mov      DWORD [REG_XDX + 233], ecx
        mov      DWORD [REG_XDX + 234], ecx
        mov      DWORD [REG_XDX + 235], ecx
        mov      DWORD [REG_XDX + 236], ecx
        mov      DWORD [REG_XDX + 237], ecx
        mov      DWORD [REG_XDX + 238], ecx
        mov      DWORD [REG_XDX + 239], ecx
        mov      DWORD [REG_XDX + 240], ecx
        mov      DWORD [REG_XDX + 241], ecx
        mov      DWORD [REG_XDX + 242], ecx
        mov      DWORD [REG_XDX + 243], ecx
        mov      DWORD [REG_XDX + 244], ecx
        mov      DWORD [REG_XDX + 245], ecx
        mov      DWORD [REG_XDX + 246], ecx
        mov      DWORD [REG_XDX + 247], ecx
        mov      DWORD [REG_XDX + 248], ecx
        mov      DWORD [REG_XDX + 249], ecx
        mov      DWORD [REG_XDX + 250], ecx
        mov      DWORD [REG_XDX + 251], ecx
        mov      DWORD [REG_XDX + 252], ecx
        mov      DWORD [REG_XDX + 253], ecx
        mov      DWORD [REG_XDX + 254], ecx
        mov      DWORD [REG_XDX + 255], ecx
        mov      DWORD [REG_XDX + 256], ecx
        mov      DWORD [REG_XDX + 257], ecx
        mov      DWORD [REG_XDX + 258], ecx
        mov      DWORD [REG_XDX + 259], ecx
        mov      DWORD [REG_XDX + 260], ecx
        mov      DWORD [REG_XDX + 261], ecx
        mov      DWORD [REG_XDX + 262], ecx
        mov      DWORD [REG_XDX + 263], ecx
        mov      DWORD [REG_XDX + 264], ecx
        mov      DWORD [REG_XDX + 265], ecx
        mov      DWORD [REG_XDX + 266], ecx
        mov      DWORD [REG_XDX + 267], ecx
        mov      DWORD [REG_XDX + 268], ecx
        mov      DWORD [REG_XDX + 269], ecx
        mov      DWORD [REG_XDX + 270], ecx
        mov      DWORD [REG_XDX + 271], ecx
        mov      DWORD [REG_XDX + 272], ecx
        mov      DWORD [REG_XDX + 273], ecx
        mov      DWORD [REG_XDX + 274], ecx
        mov      DWORD [REG_XDX + 275], ecx
        mov      DWORD [REG_XDX + 276], ecx
        mov      DWORD [REG_XDX + 277], ecx
        mov      DWORD [REG_XDX + 278], ecx
        mov      DWORD [REG_XDX + 279], ecx
        mov      DWORD [REG_XDX + 280], ecx
        mov      DWORD [REG_XDX + 281], ecx
        mov      DWORD [REG_XDX + 282], ecx
        mov      DWORD [REG_XDX + 283], ecx
        mov      DWORD [REG_XDX + 284], ecx
        mov      DWORD [REG_XDX + 285], ecx
        mov      DWORD [REG_XDX + 286], ecx
        mov      DWORD [REG_XDX + 287], ecx
        mov      DWORD [REG_XDX + 288], ecx
        mov      DWORD [REG_XDX + 289], ecx
        mov      DWORD [REG_XDX + 290], ecx
        mov      DWORD [REG_XDX + 291], ecx
        mov      DWORD [REG_XDX + 292], ecx
        mov      DWORD [REG_XDX + 293], ecx
        mov      DWORD [REG_XDX + 294], ecx
        mov      DWORD [REG_XDX + 295], ecx
        mov      DWORD [REG_XDX + 296], ecx
        mov      DWORD [REG_XDX + 297], ecx
        mov      DWORD [REG_XDX + 298], ecx
        mov      DWORD [REG_XDX + 299], ecx
        mov      DWORD [REG_XDX + 300], ecx
        mov      DWORD [REG_XDX + 301], ecx
        mov      DWORD [REG_XDX + 302], ecx
        mov      DWORD [REG_XDX + 303], ecx
        mov      DWORD [REG_XDX + 304], ecx
        mov      DWORD [REG_XDX + 305], ecx
        mov      DWORD [REG_XDX + 306], ecx
        mov      DWORD [REG_XDX + 307], ecx
        mov      DWORD [REG_XDX + 308], ecx
        mov      DWORD [REG_XDX + 309], ecx
        mov      DWORD [REG_XDX + 310], ecx
        mov      DWORD [REG_XDX + 311], ecx
        mov      DWORD [REG_XDX + 312], ecx
        mov      DWORD [REG_XDX + 313], ecx
        mov      DWORD [REG_XDX + 314], ecx
        mov      DWORD [REG_XDX + 315], ecx
        mov      DWORD [REG_XDX + 316], ecx
        mov      DWORD [REG_XDX + 317], ecx
        mov      DWORD [REG_XDX + 318], ecx
        mov      DWORD [REG_XDX + 319], ecx
        mov      DWORD [REG_XDX + 320], ecx
        mov      DWORD [REG_XDX + 321], ecx
        mov      DWORD [REG_XDX + 322], ecx
        mov      DWORD [REG_XDX + 323], ecx
        mov      DWORD [REG_XDX + 324], ecx
        mov      DWORD [REG_XDX + 325], ecx
        mov      DWORD [REG_XDX + 326], ecx
        mov      DWORD [REG_XDX + 327], ecx
        mov      DWORD [REG_XDX + 328], ecx
        mov      DWORD [REG_XDX + 329], ecx
        mov      DWORD [REG_XDX + 330], ecx
        mov      DWORD [REG_XDX + 331], ecx
        mov      DWORD [REG_XDX + 332], ecx
        mov      DWORD [REG_XDX + 333], ecx
        mov      DWORD [REG_XDX + 334], ecx
        mov      DWORD [REG_XDX + 335], ecx
        mov      DWORD [REG_XDX + 336], ecx
        mov      DWORD [REG_XDX + 337], ecx
        mov      DWORD [REG_XDX + 338], ecx
        mov      DWORD [REG_XDX + 339], ecx
        mov      DWORD [REG_XDX + 340], ecx
        mov      DWORD [REG_XDX + 341], ecx
        mov      DWORD [REG_XDX + 342], ecx
        mov      DWORD [REG_XDX + 343], ecx
        mov      DWORD [REG_XDX + 344], ecx
        mov      DWORD [REG_XDX + 345], ecx
        mov      DWORD [REG_XDX + 346], ecx
        mov      DWORD [REG_XDX + 347], ecx
        mov      DWORD [REG_XDX + 348], ecx
        mov      DWORD [REG_XDX + 349], ecx
        mov      DWORD [REG_XDX + 350], ecx
        mov      DWORD [REG_XDX + 351], ecx
        mov      DWORD [REG_XDX + 352], ecx
        mov      DWORD [REG_XDX + 353], ecx
        mov      DWORD [REG_XDX + 354], ecx
        mov      DWORD [REG_XDX + 355], ecx
        mov      DWORD [REG_XDX + 356], ecx
        mov      DWORD [REG_XDX + 357], ecx
        mov      DWORD [REG_XDX + 358], ecx
        mov      DWORD [REG_XDX + 359], ecx
        mov      DWORD [REG_XDX + 360], ecx
        mov      DWORD [REG_XDX + 361], ecx
        mov      DWORD [REG_XDX + 362], ecx
        mov      DWORD [REG_XDX + 363], ecx
        mov      DWORD [REG_XDX + 364], ecx
        mov      DWORD [REG_XDX + 365], ecx
        mov      DWORD [REG_XDX + 366], ecx
        mov      DWORD [REG_XDX + 367], ecx
        mov      DWORD [REG_XDX + 368], ecx
        mov      DWORD [REG_XDX + 369], ecx
        mov      DWORD [REG_XDX + 370], ecx
        mov      DWORD [REG_XDX + 371], ecx
        mov      DWORD [REG_XDX + 372], ecx
        mov      DWORD [REG_XDX + 373], ecx
        mov      DWORD [REG_XDX + 374], ecx
        mov      DWORD [REG_XDX + 375], ecx
        mov      DWORD [REG_XDX + 376], ecx
        mov      DWORD [REG_XDX + 377], ecx
        mov      DWORD [REG_XDX + 378], ecx
        mov      DWORD [REG_XDX + 379], ecx
        mov      DWORD [REG_XDX + 380], ecx
        mov      DWORD [REG_XDX + 381], ecx
        mov      DWORD [REG_XDX + 382], ecx
        mov      DWORD [REG_XDX + 383], ecx
        mov      DWORD [REG_XDX + 384], ecx
        mov      DWORD [REG_XDX + 385], ecx
        mov      DWORD [REG_XDX + 386], ecx
        mov      DWORD [REG_XDX + 387], ecx
        mov      DWORD [REG_XDX + 388], ecx
        mov      DWORD [REG_XDX + 389], ecx
        mov      DWORD [REG_XDX + 390], ecx
        mov      DWORD [REG_XDX + 391], ecx
        mov      DWORD [REG_XDX + 392], ecx
        mov      DWORD [REG_XDX + 393], ecx
        mov      DWORD [REG_XDX + 394], ecx
        mov      DWORD [REG_XDX + 395], ecx
        mov      DWORD [REG_XDX + 396], ecx
        mov      DWORD [REG_XDX + 397], ecx
        mov      DWORD [REG_XDX + 398], ecx
        mov      DWORD [REG_XDX + 399], ecx
        mov      DWORD [REG_XDX + 400], ecx
        mov      DWORD [REG_XDX + 401], ecx
        mov      DWORD [REG_XDX + 402], ecx
        mov      DWORD [REG_XDX + 403], ecx
        mov      DWORD [REG_XDX + 404], ecx
        mov      DWORD [REG_XDX + 405], ecx
        mov      DWORD [REG_XDX + 406], ecx
        mov      DWORD [REG_XDX + 407], ecx
        mov      DWORD [REG_XDX + 408], ecx
        mov      DWORD [REG_XDX + 409], ecx
        mov      DWORD [REG_XDX + 410], ecx
        mov      DWORD [REG_XDX + 411], ecx
        mov      DWORD [REG_XDX + 412], ecx
        mov      DWORD [REG_XDX + 413], ecx
        mov      DWORD [REG_XDX + 414], ecx
        mov      DWORD [REG_XDX + 415], ecx
        mov      DWORD [REG_XDX + 416], ecx
        mov      DWORD [REG_XDX + 417], ecx
        mov      DWORD [REG_XDX + 418], ecx
        mov      DWORD [REG_XDX + 419], ecx
        mov      DWORD [REG_XDX + 420], ecx
        mov      DWORD [REG_XDX + 421], ecx
        mov      DWORD [REG_XDX + 422], ecx
        mov      DWORD [REG_XDX + 423], ecx
        mov      DWORD [REG_XDX + 424], ecx
        mov      DWORD [REG_XDX + 425], ecx
        mov      DWORD [REG_XDX + 426], ecx
        mov      DWORD [REG_XDX + 427], ecx
        mov      DWORD [REG_XDX + 428], ecx
        mov      DWORD [REG_XDX + 429], ecx
        mov      DWORD [REG_XDX + 430], ecx
        mov      DWORD [REG_XDX + 431], ecx
        mov      DWORD [REG_XDX + 432], ecx
        mov      DWORD [REG_XDX + 433], ecx
        mov      DWORD [REG_XDX + 434], ecx
        mov      DWORD [REG_XDX + 435], ecx
        mov      DWORD [REG_XDX + 436], ecx
        mov      DWORD [REG_XDX + 437], ecx
        mov      DWORD [REG_XDX + 438], ecx
        mov      DWORD [REG_XDX + 439], ecx
        mov      DWORD [REG_XDX + 440], ecx
        mov      DWORD [REG_XDX + 441], ecx
        mov      DWORD [REG_XDX + 442], ecx
        mov      DWORD [REG_XDX + 443], ecx
        mov      DWORD [REG_XDX + 444], ecx
        mov      DWORD [REG_XDX + 445], ecx
        mov      DWORD [REG_XDX + 446], ecx
        mov      DWORD [REG_XDX + 447], ecx
        mov      DWORD [REG_XDX + 448], ecx
        mov      DWORD [REG_XDX + 449], ecx
        mov      DWORD [REG_XDX + 450], ecx
        mov      DWORD [REG_XDX + 451], ecx
        mov      DWORD [REG_XDX + 452], ecx
        mov      DWORD [REG_XDX + 453], ecx
        mov      DWORD [REG_XDX + 454], ecx
        mov      DWORD [REG_XDX + 455], ecx
        mov      DWORD [REG_XDX + 456], ecx
        mov      DWORD [REG_XDX + 457], ecx
        mov      DWORD [REG_XDX + 458], ecx
        mov      DWORD [REG_XDX + 459], ecx
        mov      DWORD [REG_XDX + 460], ecx
        mov      DWORD [REG_XDX + 461], ecx
        mov      DWORD [REG_XDX + 462], ecx
        mov      DWORD [REG_XDX + 463], ecx
        mov      DWORD [REG_XDX + 464], ecx
        mov      DWORD [REG_XDX + 465], ecx
        mov      DWORD [REG_XDX + 466], ecx
        mov      DWORD [REG_XDX + 467], ecx
        mov      DWORD [REG_XDX + 468], ecx
        mov      DWORD [REG_XDX + 469], ecx
        mov      DWORD [REG_XDX + 470], ecx
        mov      DWORD [REG_XDX + 471], ecx
        mov      DWORD [REG_XDX + 472], ecx
        mov      DWORD [REG_XDX + 473], ecx
        mov      DWORD [REG_XDX + 474], ecx
        mov      DWORD [REG_XDX + 475], ecx
        mov      DWORD [REG_XDX + 476], ecx
        mov      DWORD [REG_XDX + 477], ecx
        mov      DWORD [REG_XDX + 478], ecx
        mov      DWORD [REG_XDX + 479], ecx
        mov      DWORD [REG_XDX + 480], ecx
        mov      DWORD [REG_XDX + 481], ecx
        mov      DWORD [REG_XDX + 482], ecx
        mov      DWORD [REG_XDX + 483], ecx
        mov      DWORD [REG_XDX + 484], ecx
        mov      DWORD [REG_XDX + 485], ecx
        mov      DWORD [REG_XDX + 486], ecx
        mov      DWORD [REG_XDX + 487], ecx
        mov      DWORD [REG_XDX + 488], ecx
        mov      DWORD [REG_XDX + 489], ecx
        mov      DWORD [REG_XDX + 490], ecx
        mov      DWORD [REG_XDX + 491], ecx
        mov      DWORD [REG_XDX + 492], ecx
        mov      DWORD [REG_XDX + 493], ecx
        mov      DWORD [REG_XDX + 494], ecx
        mov      DWORD [REG_XDX + 495], ecx
        mov      DWORD [REG_XDX + 496], ecx
        mov      DWORD [REG_XDX + 497], ecx
        mov      DWORD [REG_XDX + 498], ecx
        mov      DWORD [REG_XDX + 499], ecx
        mov      DWORD [REG_XDX + 500], ecx
        mov      DWORD [REG_XDX + 501], ecx
        mov      DWORD [REG_XDX + 502], ecx
        mov      DWORD [REG_XDX + 503], ecx
        mov      DWORD [REG_XDX + 504], ecx
        mov      DWORD [REG_XDX + 505], ecx
        mov      DWORD [REG_XDX + 506], ecx
        mov      DWORD [REG_XDX + 507], ecx
        mov      DWORD [REG_XDX + 508], ecx
        mov      DWORD [REG_XDX + 509], ecx
        mov      DWORD [REG_XDX + 510], ecx
        mov      DWORD [REG_XDX + 511], ecx
        mov      DWORD [REG_XDX + 512], ecx
        mov      DWORD [REG_XDX + 513], ecx
        mov      DWORD [REG_XDX + 514], ecx
        mov      DWORD [REG_XDX + 515], ecx
        mov      DWORD [REG_XDX + 516], ecx
        mov      DWORD [REG_XDX + 517], ecx
        mov      DWORD [REG_XDX + 518], ecx
        mov      DWORD [REG_XDX + 519], ecx
        mov      DWORD [REG_XDX + 520], ecx
        mov      DWORD [REG_XDX + 521], ecx
        mov      DWORD [REG_XDX + 522], ecx
        mov      DWORD [REG_XDX + 523], ecx
        mov      DWORD [REG_XDX + 524], ecx
        mov      DWORD [REG_XDX + 525], ecx
        mov      DWORD [REG_XDX + 526], ecx
        mov      DWORD [REG_XDX + 527], ecx
        mov      DWORD [REG_XDX + 528], ecx
        mov      DWORD [REG_XDX + 529], ecx
        mov      DWORD [REG_XDX + 530], ecx
        mov      DWORD [REG_XDX + 531], ecx
        mov      DWORD [REG_XDX + 532], ecx
        mov      DWORD [REG_XDX + 533], ecx
        mov      DWORD [REG_XDX + 534], ecx
        mov      DWORD [REG_XDX + 535], ecx
        mov      DWORD [REG_XDX + 536], ecx
        mov      DWORD [REG_XDX + 537], ecx
        mov      DWORD [REG_XDX + 538], ecx
        mov      DWORD [REG_XDX + 539], ecx
        mov      DWORD [REG_XDX + 540], ecx
        mov      DWORD [REG_XDX + 541], ecx
        mov      DWORD [REG_XDX + 542], ecx
        mov      DWORD [REG_XDX + 543], ecx
        mov      DWORD [REG_XDX + 544], ecx
        mov      DWORD [REG_XDX + 545], ecx
        mov      DWORD [REG_XDX + 546], ecx
        mov      DWORD [REG_XDX + 547], ecx
        mov      DWORD [REG_XDX + 548], ecx
        mov      DWORD [REG_XDX + 549], ecx
        mov      DWORD [REG_XDX + 550], ecx
        mov      DWORD [REG_XDX + 551], ecx
        mov      DWORD [REG_XDX + 552], ecx
        mov      DWORD [REG_XDX + 553], ecx
        mov      DWORD [REG_XDX + 554], ecx
        mov      DWORD [REG_XDX + 555], ecx
        mov      DWORD [REG_XDX + 556], ecx
        mov      DWORD [REG_XDX + 557], ecx
        mov      DWORD [REG_XDX + 558], ecx
        mov      DWORD [REG_XDX + 559], ecx
        mov      DWORD [REG_XDX + 560], ecx
        mov      DWORD [REG_XDX + 561], ecx
        mov      DWORD [REG_XDX + 562], ecx
        mov      DWORD [REG_XDX + 563], ecx
        mov      DWORD [REG_XDX + 564], ecx
        mov      DWORD [REG_XDX + 565], ecx
        mov      DWORD [REG_XDX + 566], ecx
        mov      DWORD [REG_XDX + 567], ecx
        mov      DWORD [REG_XDX + 568], ecx
        mov      DWORD [REG_XDX + 569], ecx
        mov      DWORD [REG_XDX + 570], ecx
        mov      DWORD [REG_XDX + 571], ecx
        mov      DWORD [REG_XDX + 572], ecx
        mov      DWORD [REG_XDX + 573], ecx
        mov      DWORD [REG_XDX + 574], ecx
        mov      DWORD [REG_XDX + 575], ecx
        mov      DWORD [REG_XDX + 576], ecx
        mov      DWORD [REG_XDX + 577], ecx
        mov      DWORD [REG_XDX + 578], ecx
        mov      DWORD [REG_XDX + 579], ecx
        mov      DWORD [REG_XDX + 580], ecx
        mov      DWORD [REG_XDX + 581], ecx
        mov      DWORD [REG_XDX + 582], ecx
        mov      DWORD [REG_XDX + 583], ecx
        mov      DWORD [REG_XDX + 584], ecx
        mov      DWORD [REG_XDX + 585], ecx
        mov      DWORD [REG_XDX + 586], ecx
        mov      DWORD [REG_XDX + 587], ecx
        mov      DWORD [REG_XDX + 588], ecx
        mov      DWORD [REG_XDX + 589], ecx
        mov      DWORD [REG_XDX + 590], ecx
        mov      DWORD [REG_XDX + 591], ecx
        mov      DWORD [REG_XDX + 592], ecx
        mov      DWORD [REG_XDX + 593], ecx
        mov      DWORD [REG_XDX + 594], ecx
        mov      DWORD [REG_XDX + 595], ecx
        mov      DWORD [REG_XDX + 596], ecx
        mov      DWORD [REG_XDX + 597], ecx
        mov      DWORD [REG_XDX + 598], ecx
        mov      DWORD [REG_XDX + 599], ecx
        mov      DWORD [REG_XDX + 600], ecx
        mov      DWORD [REG_XDX + 601], ecx
        mov      DWORD [REG_XDX + 602], ecx
        mov      DWORD [REG_XDX + 603], ecx
        mov      DWORD [REG_XDX + 604], ecx
        mov      DWORD [REG_XDX + 605], ecx
        mov      DWORD [REG_XDX + 606], ecx
        mov      DWORD [REG_XDX + 607], ecx
        mov      DWORD [REG_XDX + 608], ecx
        mov      DWORD [REG_XDX + 609], ecx
        mov      DWORD [REG_XDX + 610], ecx
        mov      DWORD [REG_XDX + 611], ecx
        mov      DWORD [REG_XDX + 612], ecx
        mov      DWORD [REG_XDX + 613], ecx
        mov      DWORD [REG_XDX + 614], ecx
        mov      DWORD [REG_XDX + 615], ecx
        mov      DWORD [REG_XDX + 616], ecx
        mov      DWORD [REG_XDX + 617], ecx
        mov      DWORD [REG_XDX + 618], ecx
        mov      DWORD [REG_XDX + 619], ecx
        mov      DWORD [REG_XDX + 620], ecx
        mov      DWORD [REG_XDX + 621], ecx
        mov      DWORD [REG_XDX + 622], ecx
        mov      DWORD [REG_XDX + 623], ecx
        mov      DWORD [REG_XDX + 624], ecx
        mov      DWORD [REG_XDX + 625], ecx
        mov      DWORD [REG_XDX + 626], ecx
        mov      DWORD [REG_XDX + 627], ecx
        mov      DWORD [REG_XDX + 628], ecx
        mov      DWORD [REG_XDX + 629], ecx
        mov      DWORD [REG_XDX + 630], ecx
        mov      DWORD [REG_XDX + 631], ecx
        mov      DWORD [REG_XDX + 632], ecx
        mov      DWORD [REG_XDX + 633], ecx
        mov      DWORD [REG_XDX + 634], ecx
        mov      DWORD [REG_XDX + 635], ecx
        mov      DWORD [REG_XDX + 636], ecx
        mov      DWORD [REG_XDX + 637], ecx
        mov      DWORD [REG_XDX + 638], ecx
        mov      DWORD [REG_XDX + 639], ecx
        mov      DWORD [REG_XDX + 640], ecx
        mov      DWORD [REG_XDX + 641], ecx
        mov      DWORD [REG_XDX + 642], ecx
        mov      DWORD [REG_XDX + 643], ecx
        mov      DWORD [REG_XDX + 644], ecx
        mov      DWORD [REG_XDX + 645], ecx
        mov      DWORD [REG_XDX + 646], ecx
        mov      DWORD [REG_XDX + 647], ecx
        mov      DWORD [REG_XDX + 648], ecx
        mov      DWORD [REG_XDX + 649], ecx
        mov      DWORD [REG_XDX + 650], ecx
        mov      DWORD [REG_XDX + 651], ecx
        mov      DWORD [REG_XDX + 652], ecx
        mov      DWORD [REG_XDX + 653], ecx
        mov      DWORD [REG_XDX + 654], ecx
        mov      DWORD [REG_XDX + 655], ecx
        mov      DWORD [REG_XDX + 656], ecx
        mov      DWORD [REG_XDX + 657], ecx
        mov      DWORD [REG_XDX + 658], ecx
        mov      DWORD [REG_XDX + 659], ecx
        mov      DWORD [REG_XDX + 660], ecx
        mov      DWORD [REG_XDX + 661], ecx
        mov      DWORD [REG_XDX + 662], ecx
        mov      DWORD [REG_XDX + 663], ecx
        mov      DWORD [REG_XDX + 664], ecx
        mov      DWORD [REG_XDX + 665], ecx
        mov      DWORD [REG_XDX + 666], ecx
        mov      DWORD [REG_XDX + 667], ecx
        mov      DWORD [REG_XDX + 668], ecx
        mov      DWORD [REG_XDX + 669], ecx
        mov      DWORD [REG_XDX + 670], ecx
        mov      DWORD [REG_XDX + 671], ecx
        mov      DWORD [REG_XDX + 672], ecx
        mov      DWORD [REG_XDX + 673], ecx
        mov      DWORD [REG_XDX + 674], ecx
        mov      DWORD [REG_XDX + 675], ecx
        mov      DWORD [REG_XDX + 676], ecx
        mov      DWORD [REG_XDX + 677], ecx
        mov      DWORD [REG_XDX + 678], ecx
        mov      DWORD [REG_XDX + 679], ecx
        mov      DWORD [REG_XDX + 680], ecx
        mov      DWORD [REG_XDX + 681], ecx
        mov      DWORD [REG_XDX + 682], ecx
        mov      DWORD [REG_XDX + 683], ecx
        mov      DWORD [REG_XDX + 684], ecx
        mov      DWORD [REG_XDX + 685], ecx
        mov      DWORD [REG_XDX + 686], ecx
        mov      DWORD [REG_XDX + 687], ecx
        mov      DWORD [REG_XDX + 688], ecx
        mov      DWORD [REG_XDX + 689], ecx
        mov      DWORD [REG_XDX + 690], ecx
        mov      DWORD [REG_XDX + 691], ecx
        mov      DWORD [REG_XDX + 692], ecx
        mov      DWORD [REG_XDX + 693], ecx
        mov      DWORD [REG_XDX + 694], ecx
        mov      DWORD [REG_XDX + 695], ecx
        mov      DWORD [REG_XDX + 696], ecx
        mov      DWORD [REG_XDX + 697], ecx
        mov      DWORD [REG_XDX + 698], ecx
        mov      DWORD [REG_XDX + 699], ecx
        mov      DWORD [REG_XDX + 700], ecx
        mov      DWORD [REG_XDX + 701], ecx
        mov      DWORD [REG_XDX + 702], ecx
        mov      DWORD [REG_XDX + 703], ecx
        mov      DWORD [REG_XDX + 704], ecx
        mov      DWORD [REG_XDX + 705], ecx
        mov      DWORD [REG_XDX + 706], ecx
        mov      DWORD [REG_XDX + 707], ecx
        mov      DWORD [REG_XDX + 708], ecx
        mov      DWORD [REG_XDX + 709], ecx
        mov      DWORD [REG_XDX + 710], ecx
        mov      DWORD [REG_XDX + 711], ecx
        mov      DWORD [REG_XDX + 712], ecx
        mov      DWORD [REG_XDX + 713], ecx
        mov      DWORD [REG_XDX + 714], ecx
        mov      DWORD [REG_XDX + 715], ecx
        mov      DWORD [REG_XDX + 716], ecx
        mov      DWORD [REG_XDX + 717], ecx
        mov      DWORD [REG_XDX + 718], ecx
        mov      DWORD [REG_XDX + 719], ecx
        mov      DWORD [REG_XDX + 720], ecx
        mov      DWORD [REG_XDX + 721], ecx
        mov      DWORD [REG_XDX + 722], ecx
        mov      DWORD [REG_XDX + 723], ecx
        mov      DWORD [REG_XDX + 724], ecx
        mov      DWORD [REG_XDX + 725], ecx
        mov      DWORD [REG_XDX + 726], ecx
        mov      DWORD [REG_XDX + 727], ecx
        mov      DWORD [REG_XDX + 728], ecx
        mov      DWORD [REG_XDX + 729], ecx
        mov      DWORD [REG_XDX + 730], ecx
        mov      DWORD [REG_XDX + 731], ecx
        mov      DWORD [REG_XDX + 732], ecx
        mov      DWORD [REG_XDX + 733], ecx
        mov      DWORD [REG_XDX + 734], ecx
        mov      DWORD [REG_XDX + 735], ecx
        mov      DWORD [REG_XDX + 736], ecx
        mov      DWORD [REG_XDX + 737], ecx
        mov      DWORD [REG_XDX + 738], ecx
        mov      DWORD [REG_XDX + 739], ecx
        mov      DWORD [REG_XDX + 740], ecx
        mov      DWORD [REG_XDX + 741], ecx
        mov      DWORD [REG_XDX + 742], ecx
        mov      DWORD [REG_XDX + 743], ecx
        mov      DWORD [REG_XDX + 744], ecx
        mov      DWORD [REG_XDX + 745], ecx
        mov      DWORD [REG_XDX + 746], ecx
        mov      DWORD [REG_XDX + 747], ecx
        mov      DWORD [REG_XDX + 748], ecx
        mov      DWORD [REG_XDX + 749], ecx
        mov      DWORD [REG_XDX + 750], ecx
        mov      DWORD [REG_XDX + 751], ecx
        mov      DWORD [REG_XDX + 752], ecx
        mov      DWORD [REG_XDX + 753], ecx
        mov      DWORD [REG_XDX + 754], ecx
        mov      DWORD [REG_XDX + 755], ecx
        mov      DWORD [REG_XDX + 756], ecx
        mov      DWORD [REG_XDX + 757], ecx
        mov      DWORD [REG_XDX + 758], ecx
        mov      DWORD [REG_XDX + 759], ecx
        mov      DWORD [REG_XDX + 760], ecx
        mov      DWORD [REG_XDX + 761], ecx
        mov      DWORD [REG_XDX + 762], ecx
        mov      DWORD [REG_XDX + 763], ecx
        mov      DWORD [REG_XDX + 764], ecx
        mov      DWORD [REG_XDX + 765], ecx
        mov      DWORD [REG_XDX + 766], ecx
        mov      DWORD [REG_XDX + 767], ecx
        mov      DWORD [REG_XDX + 768], ecx
        mov      DWORD [REG_XDX + 769], ecx
        mov      DWORD [REG_XDX + 770], ecx
        mov      DWORD [REG_XDX + 771], ecx
        mov      DWORD [REG_XDX + 772], ecx
        mov      DWORD [REG_XDX + 773], ecx
        mov      DWORD [REG_XDX + 774], ecx
        mov      DWORD [REG_XDX + 775], ecx
        mov      DWORD [REG_XDX + 776], ecx
        mov      DWORD [REG_XDX + 777], ecx
        mov      DWORD [REG_XDX + 778], ecx
        mov      DWORD [REG_XDX + 779], ecx
        mov      DWORD [REG_XDX + 780], ecx
        mov      DWORD [REG_XDX + 781], ecx
        mov      DWORD [REG_XDX + 782], ecx
        mov      DWORD [REG_XDX + 783], ecx
        mov      DWORD [REG_XDX + 784], ecx
        mov      DWORD [REG_XDX + 785], ecx
        mov      DWORD [REG_XDX + 786], ecx
        mov      DWORD [REG_XDX + 787], ecx
        mov      DWORD [REG_XDX + 788], ecx
        mov      DWORD [REG_XDX + 789], ecx
        mov      DWORD [REG_XDX + 790], ecx
        mov      DWORD [REG_XDX + 791], ecx
        mov      DWORD [REG_XDX + 792], ecx
        mov      DWORD [REG_XDX + 793], ecx
        mov      DWORD [REG_XDX + 794], ecx
        mov      DWORD [REG_XDX + 795], ecx
        mov      DWORD [REG_XDX + 796], ecx
        mov      DWORD [REG_XDX + 797], ecx
        mov      DWORD [REG_XDX + 798], ecx
        mov      DWORD [REG_XDX + 799], ecx
        mov      DWORD [REG_XDX + 800], ecx
        mov      DWORD [REG_XDX + 801], ecx
        mov      DWORD [REG_XDX + 802], ecx
        mov      DWORD [REG_XDX + 803], ecx
        mov      DWORD [REG_XDX + 804], ecx
        mov      DWORD [REG_XDX + 805], ecx
        mov      DWORD [REG_XDX + 806], ecx
        mov      DWORD [REG_XDX + 807], ecx
        mov      DWORD [REG_XDX + 808], ecx
        mov      DWORD [REG_XDX + 809], ecx
        mov      DWORD [REG_XDX + 810], ecx
        mov      DWORD [REG_XDX + 811], ecx
        mov      DWORD [REG_XDX + 812], ecx
        mov      DWORD [REG_XDX + 813], ecx
        mov      DWORD [REG_XDX + 814], ecx
        mov      DWORD [REG_XDX + 815], ecx
        mov      DWORD [REG_XDX + 816], ecx
        mov      DWORD [REG_XDX + 817], ecx
        mov      DWORD [REG_XDX + 818], ecx
        mov      DWORD [REG_XDX + 819], ecx
        mov      DWORD [REG_XDX + 820], ecx
        mov      DWORD [REG_XDX + 821], ecx
        mov      DWORD [REG_XDX + 822], ecx
        mov      DWORD [REG_XDX + 823], ecx
        mov      DWORD [REG_XDX + 824], ecx
        mov      DWORD [REG_XDX + 825], ecx
        mov      DWORD [REG_XDX + 826], ecx
        mov      DWORD [REG_XDX + 827], ecx
        mov      DWORD [REG_XDX + 828], ecx
        mov      DWORD [REG_XDX + 829], ecx
        mov      DWORD [REG_XDX + 830], ecx
        mov      DWORD [REG_XDX + 831], ecx
        mov      DWORD [REG_XDX + 832], ecx
        mov      DWORD [REG_XDX + 833], ecx
        mov      DWORD [REG_XDX + 834], ecx
        mov      DWORD [REG_XDX + 835], ecx
        mov      DWORD [REG_XDX + 836], ecx
        mov      DWORD [REG_XDX + 837], ecx
        mov      DWORD [REG_XDX + 838], ecx
        mov      DWORD [REG_XDX + 839], ecx
        mov      DWORD [REG_XDX + 840], ecx
        mov      DWORD [REG_XDX + 841], ecx
        mov      DWORD [REG_XDX + 842], ecx
        mov      DWORD [REG_XDX + 843], ecx
        mov      DWORD [REG_XDX + 844], ecx
        mov      DWORD [REG_XDX + 845], ecx
        mov      DWORD [REG_XDX + 846], ecx
        mov      DWORD [REG_XDX + 847], ecx
        mov      DWORD [REG_XDX + 848], ecx
        mov      DWORD [REG_XDX + 849], ecx
        mov      DWORD [REG_XDX + 850], ecx
        mov      DWORD [REG_XDX + 851], ecx
        mov      DWORD [REG_XDX + 852], ecx
        mov      DWORD [REG_XDX + 853], ecx
        mov      DWORD [REG_XDX + 854], ecx
        mov      DWORD [REG_XDX + 855], ecx
        mov      DWORD [REG_XDX + 856], ecx
        mov      DWORD [REG_XDX + 857], ecx
        mov      DWORD [REG_XDX + 858], ecx
        mov      DWORD [REG_XDX + 859], ecx
        mov      DWORD [REG_XDX + 860], ecx
        mov      DWORD [REG_XDX + 861], ecx
        mov      DWORD [REG_XDX + 862], ecx
        mov      DWORD [REG_XDX + 863], ecx
        mov      DWORD [REG_XDX + 864], ecx
        mov      DWORD [REG_XDX + 865], ecx
        mov      DWORD [REG_XDX + 866], ecx
        mov      DWORD [REG_XDX + 867], ecx
        mov      DWORD [REG_XDX + 868], ecx
        mov      DWORD [REG_XDX + 869], ecx
        mov      DWORD [REG_XDX + 870], ecx
        mov      DWORD [REG_XDX + 871], ecx
        mov      DWORD [REG_XDX + 872], ecx
        mov      DWORD [REG_XDX + 873], ecx
        mov      DWORD [REG_XDX + 874], ecx
        mov      DWORD [REG_XDX + 875], ecx
        mov      DWORD [REG_XDX + 876], ecx
        mov      DWORD [REG_XDX + 877], ecx
        mov      DWORD [REG_XDX + 878], ecx
        mov      DWORD [REG_XDX + 879], ecx
        mov      DWORD [REG_XDX + 880], ecx
        mov      DWORD [REG_XDX + 881], ecx
        mov      DWORD [REG_XDX + 882], ecx
        mov      DWORD [REG_XDX + 883], ecx
        mov      DWORD [REG_XDX + 884], ecx
        mov      DWORD [REG_XDX + 885], ecx
        mov      DWORD [REG_XDX + 886], ecx
        mov      DWORD [REG_XDX + 887], ecx
        mov      DWORD [REG_XDX + 888], ecx
        mov      DWORD [REG_XDX + 889], ecx
        mov      DWORD [REG_XDX + 890], ecx
        mov      DWORD [REG_XDX + 891], ecx
        mov      DWORD [REG_XDX + 892], ecx
        mov      DWORD [REG_XDX + 893], ecx
        mov      DWORD [REG_XDX + 894], ecx
        mov      DWORD [REG_XDX + 895], ecx
        mov      DWORD [REG_XDX + 896], ecx
        mov      DWORD [REG_XDX + 897], ecx
        mov      DWORD [REG_XDX + 898], ecx
        mov      DWORD [REG_XDX + 899], ecx
        mov      REG_XCX, 0 /* counter for diagnostics */
      repeata:
        dec      eax
        inc      ecx
        cmp      eax, 0
        jnz      repeata

        mov      eax,ecx

        pop      REG_XDX
        pop      REG_XBX
        pop      REG_XBP
        ret
ADDRTAKEN_LABEL(foo_end:)
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */

#endif /* ASM_CODE_ONLY */
