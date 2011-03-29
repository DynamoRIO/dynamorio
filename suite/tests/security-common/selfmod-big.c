/* **********************************************************
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

#include "tools.h"

#ifdef USE_DYNAMO
# include "dynamorio.h"
#endif

static uint big[1024];

void
foo(int iters)
{
    /* executes iters iters, by modifying the iters bound using
     * self-modifying code
     */
    int total;
    /* first we make the foo code writable.  we can't trust "foo" as a func
     * ptr b/c it may point to the ILT.  we go ahead and re-enable protection
     * every time, just guessing at the # pages needed.
     */
#ifdef LINUX
    asm("  call next_inst_prot");
    asm("next_inst_prot:");
    asm("  popl  %%edx" : : : "edx");
    asm("  pushl %0" : : "i" (ALLOW_READ|ALLOW_WRITE|ALLOW_EXEC));
    /* protect_mem will expand size to page.  If go too far (even
     * PAGE_SIZE here) will evict later code from executable_areas
     * that won't come back and will get double violations (case
     * 7940).
     */
    asm("  pushl %0" : : "i" (1));
    asm("  pushl %edx");
    asm("  call protect_mem");
    asm("  addl  $12, %%esp" : : : "esp");

    asm("  movl %0, %%ecx" : : "r" (iters) : "ecx");
    asm("  call next_inst");
    asm("next_inst:");
    asm("  pop %edx");
    /* add to retaddr: 1 == pop
     *                 3 == mov ecx into target
     *                 1 == opcode of target movl
     */
    asm("  movl %ecx, 0x5(%edx)"); /* the modifying store */
    asm("  movl $0x12345678,%eax"); /* this instr's immed gets overwritten */

        /* now we have as many write instrs as necessary to cause a too-big
         * selfmod fragment.  xref case 7893.
         */
    asm("  movl %0, %%ecx" : : "r" (iters) : "ecx");
    asm("  movl %0, %%edx" : : "g" (big));
    asm("  movl %ecx, 0(%edx)");
    asm("  movl %ecx, 1(%edx)");
    asm("  movl %ecx, 2(%edx)");
    asm("  movl %ecx, 3(%edx)");
    asm("  movl %ecx, 4(%edx)");
    asm("  movl %ecx, 5(%edx)");
    asm("  movl %ecx, 6(%edx)");
    asm("  movl %ecx, 7(%edx)");
    asm("  movl %ecx, 8(%edx)");
    asm("  movl %ecx, 9(%edx)");
    asm("  movl %ecx, 10(%edx)");
    asm("  movl %ecx, 11(%edx)");
    asm("  movl %ecx, 12(%edx)");
    asm("  movl %ecx, 13(%edx)");
    asm("  movl %ecx, 14(%edx)");
    asm("  movl %ecx, 15(%edx)");
    asm("  movl %ecx, 16(%edx)");
    asm("  movl %ecx, 17(%edx)");
    asm("  movl %ecx, 18(%edx)");
    asm("  movl %ecx, 19(%edx)");
    asm("  movl %ecx, 20(%edx)");
    asm("  movl %ecx, 21(%edx)");
    asm("  movl %ecx, 22(%edx)");
    asm("  movl %ecx, 23(%edx)");
    asm("  movl %ecx, 24(%edx)");
    asm("  movl %ecx, 25(%edx)");
    asm("  movl %ecx, 26(%edx)");
    asm("  movl %ecx, 27(%edx)");
    asm("  movl %ecx, 28(%edx)");
    asm("  movl %ecx, 29(%edx)");
    asm("  movl %ecx, 30(%edx)");
    asm("  movl %ecx, 31(%edx)");
    asm("  movl %ecx, 32(%edx)");
    asm("  movl %ecx, 33(%edx)");
    asm("  movl %ecx, 34(%edx)");
    asm("  movl %ecx, 35(%edx)");
    asm("  movl %ecx, 36(%edx)");
    asm("  movl %ecx, 37(%edx)");
    asm("  movl %ecx, 38(%edx)");
    asm("  movl %ecx, 39(%edx)");
    asm("  movl %ecx, 40(%edx)");
    asm("  movl %ecx, 41(%edx)");
    asm("  movl %ecx, 42(%edx)");
    asm("  movl %ecx, 43(%edx)");
    asm("  movl %ecx, 44(%edx)");
    asm("  movl %ecx, 45(%edx)");
    asm("  movl %ecx, 46(%edx)");
    asm("  movl %ecx, 47(%edx)");
    asm("  movl %ecx, 48(%edx)");
    asm("  movl %ecx, 49(%edx)");
    asm("  movl %ecx, 50(%edx)");
    asm("  movl %ecx, 51(%edx)");
    asm("  movl %ecx, 52(%edx)");
    asm("  movl %ecx, 53(%edx)");
    asm("  movl %ecx, 54(%edx)");
    asm("  movl %ecx, 55(%edx)");
    asm("  movl %ecx, 56(%edx)");
    asm("  movl %ecx, 57(%edx)");
    asm("  movl %ecx, 58(%edx)");
    asm("  movl %ecx, 59(%edx)");
    asm("  movl %ecx, 60(%edx)");
    asm("  movl %ecx, 61(%edx)");
    asm("  movl %ecx, 62(%edx)");
    asm("  movl %ecx, 63(%edx)");
    asm("  movl %ecx, 64(%edx)");
    asm("  movl %ecx, 65(%edx)");
    asm("  movl %ecx, 66(%edx)");
    asm("  movl %ecx, 67(%edx)");
    asm("  movl %ecx, 68(%edx)");
    asm("  movl %ecx, 69(%edx)");
    asm("  movl %ecx, 70(%edx)");
    asm("  movl %ecx, 71(%edx)");
    asm("  movl %ecx, 72(%edx)");
    asm("  movl %ecx, 73(%edx)");
    asm("  movl %ecx, 74(%edx)");
    asm("  movl %ecx, 75(%edx)");
    asm("  movl %ecx, 76(%edx)");
    asm("  movl %ecx, 77(%edx)");
    asm("  movl %ecx, 78(%edx)");
    asm("  movl %ecx, 79(%edx)");
    asm("  movl %ecx, 80(%edx)");
    asm("  movl %ecx, 81(%edx)");
    asm("  movl %ecx, 82(%edx)");
    asm("  movl %ecx, 83(%edx)");
    asm("  movl %ecx, 84(%edx)");
    asm("  movl %ecx, 85(%edx)");
    asm("  movl %ecx, 86(%edx)");
    asm("  movl %ecx, 87(%edx)");
    asm("  movl %ecx, 88(%edx)");
    asm("  movl %ecx, 89(%edx)");
    asm("  movl %ecx, 90(%edx)");
    asm("  movl %ecx, 91(%edx)");
    asm("  movl %ecx, 92(%edx)");
    asm("  movl %ecx, 93(%edx)");
    asm("  movl %ecx, 94(%edx)");
    asm("  movl %ecx, 95(%edx)");
    asm("  movl %ecx, 96(%edx)");
    asm("  movl %ecx, 97(%edx)");
    asm("  movl %ecx, 98(%edx)");
    asm("  movl %ecx, 99(%edx)");
    asm("  movl %ecx, 100(%edx)");
    asm("  movl %ecx, 101(%edx)");
    asm("  movl %ecx, 102(%edx)");
    asm("  movl %ecx, 103(%edx)");
    asm("  movl %ecx, 104(%edx)");
    asm("  movl %ecx, 105(%edx)");
    asm("  movl %ecx, 106(%edx)");
    asm("  movl %ecx, 107(%edx)");
    asm("  movl %ecx, 108(%edx)");
    asm("  movl %ecx, 109(%edx)");
    asm("  movl %ecx, 110(%edx)");
    asm("  movl %ecx, 111(%edx)");
    asm("  movl %ecx, 112(%edx)");
    asm("  movl %ecx, 113(%edx)");
    asm("  movl %ecx, 114(%edx)");
    asm("  movl %ecx, 115(%edx)");
    asm("  movl %ecx, 116(%edx)");
    asm("  movl %ecx, 117(%edx)");
    asm("  movl %ecx, 118(%edx)");
    asm("  movl %ecx, 119(%edx)");
    asm("  movl %ecx, 120(%edx)");
    asm("  movl %ecx, 121(%edx)");
    asm("  movl %ecx, 122(%edx)");
    asm("  movl %ecx, 123(%edx)");
    asm("  movl %ecx, 124(%edx)");
    asm("  movl %ecx, 125(%edx)");
    asm("  movl %ecx, 126(%edx)");
    asm("  movl %ecx, 127(%edx)");
    asm("  movl %ecx, 128(%edx)");
    asm("  movl %ecx, 129(%edx)");
    asm("  movl %ecx, 130(%edx)");
    asm("  movl %ecx, 131(%edx)");
    asm("  movl %ecx, 132(%edx)");
    asm("  movl %ecx, 133(%edx)");
    asm("  movl %ecx, 134(%edx)");
    asm("  movl %ecx, 135(%edx)");
    asm("  movl %ecx, 136(%edx)");
    asm("  movl %ecx, 137(%edx)");
    asm("  movl %ecx, 138(%edx)");
    asm("  movl %ecx, 139(%edx)");
    asm("  movl %ecx, 140(%edx)");
    asm("  movl %ecx, 141(%edx)");
    asm("  movl %ecx, 142(%edx)");
    asm("  movl %ecx, 143(%edx)");
    asm("  movl %ecx, 144(%edx)");
    asm("  movl %ecx, 145(%edx)");
    asm("  movl %ecx, 146(%edx)");
    asm("  movl %ecx, 147(%edx)");
    asm("  movl %ecx, 148(%edx)");
    asm("  movl %ecx, 149(%edx)");
    asm("  movl %ecx, 150(%edx)");
    asm("  movl %ecx, 151(%edx)");
    asm("  movl %ecx, 152(%edx)");
    asm("  movl %ecx, 153(%edx)");
    asm("  movl %ecx, 154(%edx)");
    asm("  movl %ecx, 155(%edx)");
    asm("  movl %ecx, 156(%edx)");
    asm("  movl %ecx, 157(%edx)");
    asm("  movl %ecx, 158(%edx)");
    asm("  movl %ecx, 159(%edx)");
    asm("  movl %ecx, 160(%edx)");
    asm("  movl %ecx, 161(%edx)");
    asm("  movl %ecx, 162(%edx)");
    asm("  movl %ecx, 163(%edx)");
    asm("  movl %ecx, 164(%edx)");
    asm("  movl %ecx, 165(%edx)");
    asm("  movl %ecx, 166(%edx)");
    asm("  movl %ecx, 167(%edx)");
    asm("  movl %ecx, 168(%edx)");
    asm("  movl %ecx, 169(%edx)");
    asm("  movl %ecx, 170(%edx)");
    asm("  movl %ecx, 171(%edx)");
    asm("  movl %ecx, 172(%edx)");
    asm("  movl %ecx, 173(%edx)");
    asm("  movl %ecx, 174(%edx)");
    asm("  movl %ecx, 175(%edx)");
    asm("  movl %ecx, 176(%edx)");
    asm("  movl %ecx, 177(%edx)");
    asm("  movl %ecx, 178(%edx)");
    asm("  movl %ecx, 179(%edx)");
    asm("  movl %ecx, 180(%edx)");
    asm("  movl %ecx, 181(%edx)");
    asm("  movl %ecx, 182(%edx)");
    asm("  movl %ecx, 183(%edx)");
    asm("  movl %ecx, 184(%edx)");
    asm("  movl %ecx, 185(%edx)");
    asm("  movl %ecx, 186(%edx)");
    asm("  movl %ecx, 187(%edx)");
    asm("  movl %ecx, 188(%edx)");
    asm("  movl %ecx, 189(%edx)");
    asm("  movl %ecx, 190(%edx)");
    asm("  movl %ecx, 191(%edx)");
    asm("  movl %ecx, 192(%edx)");
    asm("  movl %ecx, 193(%edx)");
    asm("  movl %ecx, 194(%edx)");
    asm("  movl %ecx, 195(%edx)");
    asm("  movl %ecx, 196(%edx)");
    asm("  movl %ecx, 197(%edx)");
    asm("  movl %ecx, 198(%edx)");
    asm("  movl %ecx, 199(%edx)");
    asm("  movl %ecx, 200(%edx)");
    asm("  movl %ecx, 201(%edx)");
    asm("  movl %ecx, 202(%edx)");
    asm("  movl %ecx, 203(%edx)");
    asm("  movl %ecx, 204(%edx)");
    asm("  movl %ecx, 205(%edx)");
    asm("  movl %ecx, 206(%edx)");
    asm("  movl %ecx, 207(%edx)");
    asm("  movl %ecx, 208(%edx)");
    asm("  movl %ecx, 209(%edx)");
    asm("  movl %ecx, 210(%edx)");
    asm("  movl %ecx, 211(%edx)");
    asm("  movl %ecx, 212(%edx)");
    asm("  movl %ecx, 213(%edx)");
    asm("  movl %ecx, 214(%edx)");
    asm("  movl %ecx, 215(%edx)");
    asm("  movl %ecx, 216(%edx)");
    asm("  movl %ecx, 217(%edx)");
    asm("  movl %ecx, 218(%edx)");
    asm("  movl %ecx, 219(%edx)");
    asm("  movl %ecx, 220(%edx)");
    asm("  movl %ecx, 221(%edx)");
    asm("  movl %ecx, 222(%edx)");
    asm("  movl %ecx, 223(%edx)");
    asm("  movl %ecx, 224(%edx)");
    asm("  movl %ecx, 225(%edx)");
    asm("  movl %ecx, 226(%edx)");
    asm("  movl %ecx, 227(%edx)");
    asm("  movl %ecx, 228(%edx)");
    asm("  movl %ecx, 229(%edx)");
    asm("  movl %ecx, 230(%edx)");
    asm("  movl %ecx, 231(%edx)");
    asm("  movl %ecx, 232(%edx)");
    asm("  movl %ecx, 233(%edx)");
    asm("  movl %ecx, 234(%edx)");
    asm("  movl %ecx, 235(%edx)");
    asm("  movl %ecx, 236(%edx)");
    asm("  movl %ecx, 237(%edx)");
    asm("  movl %ecx, 238(%edx)");
    asm("  movl %ecx, 239(%edx)");
    asm("  movl %ecx, 240(%edx)");
    asm("  movl %ecx, 241(%edx)");
    asm("  movl %ecx, 242(%edx)");
    asm("  movl %ecx, 243(%edx)");
    asm("  movl %ecx, 244(%edx)");
    asm("  movl %ecx, 245(%edx)");
    asm("  movl %ecx, 246(%edx)");
    asm("  movl %ecx, 247(%edx)");
    asm("  movl %ecx, 248(%edx)");
    asm("  movl %ecx, 249(%edx)");
    asm("  movl %ecx, 250(%edx)");
    asm("  movl %ecx, 251(%edx)");
    asm("  movl %ecx, 252(%edx)");
    asm("  movl %ecx, 253(%edx)");
    asm("  movl %ecx, 254(%edx)");
    asm("  movl %ecx, 255(%edx)");
    asm("  movl %ecx, 256(%edx)");
    asm("  movl %ecx, 257(%edx)");
    asm("  movl %ecx, 258(%edx)");
    asm("  movl %ecx, 259(%edx)");
    asm("  movl %ecx, 260(%edx)");
    asm("  movl %ecx, 261(%edx)");
    asm("  movl %ecx, 262(%edx)");
    asm("  movl %ecx, 263(%edx)");
    asm("  movl %ecx, 264(%edx)");
    asm("  movl %ecx, 265(%edx)");
    asm("  movl %ecx, 266(%edx)");
    asm("  movl %ecx, 267(%edx)");
    asm("  movl %ecx, 268(%edx)");
    asm("  movl %ecx, 269(%edx)");
    asm("  movl %ecx, 270(%edx)");
    asm("  movl %ecx, 271(%edx)");
    asm("  movl %ecx, 272(%edx)");
    asm("  movl %ecx, 273(%edx)");
    asm("  movl %ecx, 274(%edx)");
    asm("  movl %ecx, 275(%edx)");
    asm("  movl %ecx, 276(%edx)");
    asm("  movl %ecx, 277(%edx)");
    asm("  movl %ecx, 278(%edx)");
    asm("  movl %ecx, 279(%edx)");
    asm("  movl %ecx, 280(%edx)");
    asm("  movl %ecx, 281(%edx)");
    asm("  movl %ecx, 282(%edx)");
    asm("  movl %ecx, 283(%edx)");
    asm("  movl %ecx, 284(%edx)");
    asm("  movl %ecx, 285(%edx)");
    asm("  movl %ecx, 286(%edx)");
    asm("  movl %ecx, 287(%edx)");
    asm("  movl %ecx, 288(%edx)");
    asm("  movl %ecx, 289(%edx)");
    asm("  movl %ecx, 290(%edx)");
    asm("  movl %ecx, 291(%edx)");
    asm("  movl %ecx, 292(%edx)");
    asm("  movl %ecx, 293(%edx)");
    asm("  movl %ecx, 294(%edx)");
    asm("  movl %ecx, 295(%edx)");
    asm("  movl %ecx, 296(%edx)");
    asm("  movl %ecx, 297(%edx)");
    asm("  movl %ecx, 298(%edx)");
    asm("  movl %ecx, 299(%edx)");
    asm("  movl %ecx, 300(%edx)");
    asm("  movl %ecx, 301(%edx)");
    asm("  movl %ecx, 302(%edx)");
    asm("  movl %ecx, 303(%edx)");
    asm("  movl %ecx, 304(%edx)");
    asm("  movl %ecx, 305(%edx)");
    asm("  movl %ecx, 306(%edx)");
    asm("  movl %ecx, 307(%edx)");
    asm("  movl %ecx, 308(%edx)");
    asm("  movl %ecx, 309(%edx)");
    asm("  movl %ecx, 310(%edx)");
    asm("  movl %ecx, 311(%edx)");
    asm("  movl %ecx, 312(%edx)");
    asm("  movl %ecx, 313(%edx)");
    asm("  movl %ecx, 314(%edx)");
    asm("  movl %ecx, 315(%edx)");
    asm("  movl %ecx, 316(%edx)");
    asm("  movl %ecx, 317(%edx)");
    asm("  movl %ecx, 318(%edx)");
    asm("  movl %ecx, 319(%edx)");
    asm("  movl %ecx, 320(%edx)");
    asm("  movl %ecx, 321(%edx)");
    asm("  movl %ecx, 322(%edx)");
    asm("  movl %ecx, 323(%edx)");
    asm("  movl %ecx, 324(%edx)");
    asm("  movl %ecx, 325(%edx)");
    asm("  movl %ecx, 326(%edx)");
    asm("  movl %ecx, 327(%edx)");
    asm("  movl %ecx, 328(%edx)");
    asm("  movl %ecx, 329(%edx)");
    asm("  movl %ecx, 330(%edx)");
    asm("  movl %ecx, 331(%edx)");
    asm("  movl %ecx, 332(%edx)");
    asm("  movl %ecx, 333(%edx)");
    asm("  movl %ecx, 334(%edx)");
    asm("  movl %ecx, 335(%edx)");
    asm("  movl %ecx, 336(%edx)");
    asm("  movl %ecx, 337(%edx)");
    asm("  movl %ecx, 338(%edx)");
    asm("  movl %ecx, 339(%edx)");
    asm("  movl %ecx, 340(%edx)");
    asm("  movl %ecx, 341(%edx)");
    asm("  movl %ecx, 342(%edx)");
    asm("  movl %ecx, 343(%edx)");
    asm("  movl %ecx, 344(%edx)");
    asm("  movl %ecx, 345(%edx)");
    asm("  movl %ecx, 346(%edx)");
    asm("  movl %ecx, 347(%edx)");
    asm("  movl %ecx, 348(%edx)");
    asm("  movl %ecx, 349(%edx)");
    asm("  movl %ecx, 350(%edx)");
    asm("  movl %ecx, 351(%edx)");
    asm("  movl %ecx, 352(%edx)");
    asm("  movl %ecx, 353(%edx)");
    asm("  movl %ecx, 354(%edx)");
    asm("  movl %ecx, 355(%edx)");
    asm("  movl %ecx, 356(%edx)");
    asm("  movl %ecx, 357(%edx)");
    asm("  movl %ecx, 358(%edx)");
    asm("  movl %ecx, 359(%edx)");
    asm("  movl %ecx, 360(%edx)");
    asm("  movl %ecx, 361(%edx)");
    asm("  movl %ecx, 362(%edx)");
    asm("  movl %ecx, 363(%edx)");
    asm("  movl %ecx, 364(%edx)");
    asm("  movl %ecx, 365(%edx)");
    asm("  movl %ecx, 366(%edx)");
    asm("  movl %ecx, 367(%edx)");
    asm("  movl %ecx, 368(%edx)");
    asm("  movl %ecx, 369(%edx)");
    asm("  movl %ecx, 370(%edx)");
    asm("  movl %ecx, 371(%edx)");
    asm("  movl %ecx, 372(%edx)");
    asm("  movl %ecx, 373(%edx)");
    asm("  movl %ecx, 374(%edx)");
    asm("  movl %ecx, 375(%edx)");
    asm("  movl %ecx, 376(%edx)");
    asm("  movl %ecx, 377(%edx)");
    asm("  movl %ecx, 378(%edx)");
    asm("  movl %ecx, 379(%edx)");
    asm("  movl %ecx, 380(%edx)");
    asm("  movl %ecx, 381(%edx)");
    asm("  movl %ecx, 382(%edx)");
    asm("  movl %ecx, 383(%edx)");
    asm("  movl %ecx, 384(%edx)");
    asm("  movl %ecx, 385(%edx)");
    asm("  movl %ecx, 386(%edx)");
    asm("  movl %ecx, 387(%edx)");
    asm("  movl %ecx, 388(%edx)");
    asm("  movl %ecx, 389(%edx)");
    asm("  movl %ecx, 390(%edx)");
    asm("  movl %ecx, 391(%edx)");
    asm("  movl %ecx, 392(%edx)");
    asm("  movl %ecx, 393(%edx)");
    asm("  movl %ecx, 394(%edx)");
    asm("  movl %ecx, 395(%edx)");
    asm("  movl %ecx, 396(%edx)");
    asm("  movl %ecx, 397(%edx)");
    asm("  movl %ecx, 398(%edx)");
    asm("  movl %ecx, 399(%edx)");
    asm("  movl %ecx, 400(%edx)");
    asm("  movl %ecx, 401(%edx)");
    asm("  movl %ecx, 402(%edx)");
    asm("  movl %ecx, 403(%edx)");
    asm("  movl %ecx, 404(%edx)");
    asm("  movl %ecx, 405(%edx)");
    asm("  movl %ecx, 406(%edx)");
    asm("  movl %ecx, 407(%edx)");
    asm("  movl %ecx, 408(%edx)");
    asm("  movl %ecx, 409(%edx)");
    asm("  movl %ecx, 410(%edx)");
    asm("  movl %ecx, 411(%edx)");
    asm("  movl %ecx, 412(%edx)");
    asm("  movl %ecx, 413(%edx)");
    asm("  movl %ecx, 414(%edx)");
    asm("  movl %ecx, 415(%edx)");
    asm("  movl %ecx, 416(%edx)");
    asm("  movl %ecx, 417(%edx)");
    asm("  movl %ecx, 418(%edx)");
    asm("  movl %ecx, 419(%edx)");
    asm("  movl %ecx, 420(%edx)");
    asm("  movl %ecx, 421(%edx)");
    asm("  movl %ecx, 422(%edx)");
    asm("  movl %ecx, 423(%edx)");
    asm("  movl %ecx, 424(%edx)");
    asm("  movl %ecx, 425(%edx)");
    asm("  movl %ecx, 426(%edx)");
    asm("  movl %ecx, 427(%edx)");
    asm("  movl %ecx, 428(%edx)");
    asm("  movl %ecx, 429(%edx)");
    asm("  movl %ecx, 430(%edx)");
    asm("  movl %ecx, 431(%edx)");
    asm("  movl %ecx, 432(%edx)");
    asm("  movl %ecx, 433(%edx)");
    asm("  movl %ecx, 434(%edx)");
    asm("  movl %ecx, 435(%edx)");
    asm("  movl %ecx, 436(%edx)");
    asm("  movl %ecx, 437(%edx)");
    asm("  movl %ecx, 438(%edx)");
    asm("  movl %ecx, 439(%edx)");
    asm("  movl %ecx, 440(%edx)");
    asm("  movl %ecx, 441(%edx)");
    asm("  movl %ecx, 442(%edx)");
    asm("  movl %ecx, 443(%edx)");
    asm("  movl %ecx, 444(%edx)");
    asm("  movl %ecx, 445(%edx)");
    asm("  movl %ecx, 446(%edx)");
    asm("  movl %ecx, 447(%edx)");
    asm("  movl %ecx, 448(%edx)");
    asm("  movl %ecx, 449(%edx)");
    asm("  movl %ecx, 450(%edx)");
    asm("  movl %ecx, 451(%edx)");
    asm("  movl %ecx, 452(%edx)");
    asm("  movl %ecx, 453(%edx)");
    asm("  movl %ecx, 454(%edx)");
    asm("  movl %ecx, 455(%edx)");
    asm("  movl %ecx, 456(%edx)");
    asm("  movl %ecx, 457(%edx)");
    asm("  movl %ecx, 458(%edx)");
    asm("  movl %ecx, 459(%edx)");
    asm("  movl %ecx, 460(%edx)");
    asm("  movl %ecx, 461(%edx)");
    asm("  movl %ecx, 462(%edx)");
    asm("  movl %ecx, 463(%edx)");
    asm("  movl %ecx, 464(%edx)");
    asm("  movl %ecx, 465(%edx)");
    asm("  movl %ecx, 466(%edx)");
    asm("  movl %ecx, 467(%edx)");
    asm("  movl %ecx, 468(%edx)");
    asm("  movl %ecx, 469(%edx)");
    asm("  movl %ecx, 470(%edx)");
    asm("  movl %ecx, 471(%edx)");
    asm("  movl %ecx, 472(%edx)");
    asm("  movl %ecx, 473(%edx)");
    asm("  movl %ecx, 474(%edx)");
    asm("  movl %ecx, 475(%edx)");
    asm("  movl %ecx, 476(%edx)");
    asm("  movl %ecx, 477(%edx)");
    asm("  movl %ecx, 478(%edx)");
    asm("  movl %ecx, 479(%edx)");
    asm("  movl %ecx, 480(%edx)");
    asm("  movl %ecx, 481(%edx)");
    asm("  movl %ecx, 482(%edx)");
    asm("  movl %ecx, 483(%edx)");
    asm("  movl %ecx, 484(%edx)");
    asm("  movl %ecx, 485(%edx)");
    asm("  movl %ecx, 486(%edx)");
    asm("  movl %ecx, 487(%edx)");
    asm("  movl %ecx, 488(%edx)");
    asm("  movl %ecx, 489(%edx)");
    asm("  movl %ecx, 490(%edx)");
    asm("  movl %ecx, 491(%edx)");
    asm("  movl %ecx, 492(%edx)");
    asm("  movl %ecx, 493(%edx)");
    asm("  movl %ecx, 494(%edx)");
    asm("  movl %ecx, 495(%edx)");
    asm("  movl %ecx, 496(%edx)");
    asm("  movl %ecx, 497(%edx)");
    asm("  movl %ecx, 498(%edx)");
    asm("  movl %ecx, 499(%edx)");
    asm("  movl %ecx, 500(%edx)");
    asm("  movl %ecx, 501(%edx)");
    asm("  movl %ecx, 502(%edx)");
    asm("  movl %ecx, 503(%edx)");
    asm("  movl %ecx, 504(%edx)");
    asm("  movl %ecx, 505(%edx)");
    asm("  movl %ecx, 506(%edx)");
    asm("  movl %ecx, 507(%edx)");
    asm("  movl %ecx, 508(%edx)");
    asm("  movl %ecx, 509(%edx)");
    asm("  movl %ecx, 510(%edx)");
    asm("  movl %ecx, 511(%edx)");
    asm("  movl %ecx, 512(%edx)");
    asm("  movl %ecx, 513(%edx)");
    asm("  movl %ecx, 514(%edx)");
    asm("  movl %ecx, 515(%edx)");
    asm("  movl %ecx, 516(%edx)");
    asm("  movl %ecx, 517(%edx)");
    asm("  movl %ecx, 518(%edx)");
    asm("  movl %ecx, 519(%edx)");
    asm("  movl %ecx, 520(%edx)");
    asm("  movl %ecx, 521(%edx)");
    asm("  movl %ecx, 522(%edx)");
    asm("  movl %ecx, 523(%edx)");
    asm("  movl %ecx, 524(%edx)");
    asm("  movl %ecx, 525(%edx)");
    asm("  movl %ecx, 526(%edx)");
    asm("  movl %ecx, 527(%edx)");
    asm("  movl %ecx, 528(%edx)");
    asm("  movl %ecx, 529(%edx)");
    asm("  movl %ecx, 530(%edx)");
    asm("  movl %ecx, 531(%edx)");
    asm("  movl %ecx, 532(%edx)");
    asm("  movl %ecx, 533(%edx)");
    asm("  movl %ecx, 534(%edx)");
    asm("  movl %ecx, 535(%edx)");
    asm("  movl %ecx, 536(%edx)");
    asm("  movl %ecx, 537(%edx)");
    asm("  movl %ecx, 538(%edx)");
    asm("  movl %ecx, 539(%edx)");
    asm("  movl %ecx, 540(%edx)");
    asm("  movl %ecx, 541(%edx)");
    asm("  movl %ecx, 542(%edx)");
    asm("  movl %ecx, 543(%edx)");
    asm("  movl %ecx, 544(%edx)");
    asm("  movl %ecx, 545(%edx)");
    asm("  movl %ecx, 546(%edx)");
    asm("  movl %ecx, 547(%edx)");
    asm("  movl %ecx, 548(%edx)");
    asm("  movl %ecx, 549(%edx)");
    asm("  movl %ecx, 550(%edx)");
    asm("  movl %ecx, 551(%edx)");
    asm("  movl %ecx, 552(%edx)");
    asm("  movl %ecx, 553(%edx)");
    asm("  movl %ecx, 554(%edx)");
    asm("  movl %ecx, 555(%edx)");
    asm("  movl %ecx, 556(%edx)");
    asm("  movl %ecx, 557(%edx)");
    asm("  movl %ecx, 558(%edx)");
    asm("  movl %ecx, 559(%edx)");
    asm("  movl %ecx, 560(%edx)");
    asm("  movl %ecx, 561(%edx)");
    asm("  movl %ecx, 562(%edx)");
    asm("  movl %ecx, 563(%edx)");
    asm("  movl %ecx, 564(%edx)");
    asm("  movl %ecx, 565(%edx)");
    asm("  movl %ecx, 566(%edx)");
    asm("  movl %ecx, 567(%edx)");
    asm("  movl %ecx, 568(%edx)");
    asm("  movl %ecx, 569(%edx)");
    asm("  movl %ecx, 570(%edx)");
    asm("  movl %ecx, 571(%edx)");
    asm("  movl %ecx, 572(%edx)");
    asm("  movl %ecx, 573(%edx)");
    asm("  movl %ecx, 574(%edx)");
    asm("  movl %ecx, 575(%edx)");
    asm("  movl %ecx, 576(%edx)");
    asm("  movl %ecx, 577(%edx)");
    asm("  movl %ecx, 578(%edx)");
    asm("  movl %ecx, 579(%edx)");
    asm("  movl %ecx, 580(%edx)");
    asm("  movl %ecx, 581(%edx)");
    asm("  movl %ecx, 582(%edx)");
    asm("  movl %ecx, 583(%edx)");
    asm("  movl %ecx, 584(%edx)");
    asm("  movl %ecx, 585(%edx)");
    asm("  movl %ecx, 586(%edx)");
    asm("  movl %ecx, 587(%edx)");
    asm("  movl %ecx, 588(%edx)");
    asm("  movl %ecx, 589(%edx)");
    asm("  movl %ecx, 590(%edx)");
    asm("  movl %ecx, 591(%edx)");
    asm("  movl %ecx, 592(%edx)");
    asm("  movl %ecx, 593(%edx)");
    asm("  movl %ecx, 594(%edx)");
    asm("  movl %ecx, 595(%edx)");
    asm("  movl %ecx, 596(%edx)");
    asm("  movl %ecx, 597(%edx)");
    asm("  movl %ecx, 598(%edx)");
    asm("  movl %ecx, 599(%edx)");
    asm("  movl %ecx, 600(%edx)");
    asm("  movl %ecx, 601(%edx)");
    asm("  movl %ecx, 602(%edx)");
    asm("  movl %ecx, 603(%edx)");
    asm("  movl %ecx, 604(%edx)");
    asm("  movl %ecx, 605(%edx)");
    asm("  movl %ecx, 606(%edx)");
    asm("  movl %ecx, 607(%edx)");
    asm("  movl %ecx, 608(%edx)");
    asm("  movl %ecx, 609(%edx)");
    asm("  movl %ecx, 610(%edx)");
    asm("  movl %ecx, 611(%edx)");
    asm("  movl %ecx, 612(%edx)");
    asm("  movl %ecx, 613(%edx)");
    asm("  movl %ecx, 614(%edx)");
    asm("  movl %ecx, 615(%edx)");
    asm("  movl %ecx, 616(%edx)");
    asm("  movl %ecx, 617(%edx)");
    asm("  movl %ecx, 618(%edx)");
    asm("  movl %ecx, 619(%edx)");
    asm("  movl %ecx, 620(%edx)");
    asm("  movl %ecx, 621(%edx)");
    asm("  movl %ecx, 622(%edx)");
    asm("  movl %ecx, 623(%edx)");
    asm("  movl %ecx, 624(%edx)");
    asm("  movl %ecx, 625(%edx)");
    asm("  movl %ecx, 626(%edx)");
    asm("  movl %ecx, 627(%edx)");
    asm("  movl %ecx, 628(%edx)");
    asm("  movl %ecx, 629(%edx)");
    asm("  movl %ecx, 630(%edx)");
    asm("  movl %ecx, 631(%edx)");
    asm("  movl %ecx, 632(%edx)");
    asm("  movl %ecx, 633(%edx)");
    asm("  movl %ecx, 634(%edx)");
    asm("  movl %ecx, 635(%edx)");
    asm("  movl %ecx, 636(%edx)");
    asm("  movl %ecx, 637(%edx)");
    asm("  movl %ecx, 638(%edx)");
    asm("  movl %ecx, 639(%edx)");
    asm("  movl %ecx, 640(%edx)");
    asm("  movl %ecx, 641(%edx)");
    asm("  movl %ecx, 642(%edx)");
    asm("  movl %ecx, 643(%edx)");
    asm("  movl %ecx, 644(%edx)");
    asm("  movl %ecx, 645(%edx)");
    asm("  movl %ecx, 646(%edx)");
    asm("  movl %ecx, 647(%edx)");
    asm("  movl %ecx, 648(%edx)");
    asm("  movl %ecx, 649(%edx)");
    asm("  movl %ecx, 650(%edx)");
    asm("  movl %ecx, 651(%edx)");
    asm("  movl %ecx, 652(%edx)");
    asm("  movl %ecx, 653(%edx)");
    asm("  movl %ecx, 654(%edx)");
    asm("  movl %ecx, 655(%edx)");
    asm("  movl %ecx, 656(%edx)");
    asm("  movl %ecx, 657(%edx)");
    asm("  movl %ecx, 658(%edx)");
    asm("  movl %ecx, 659(%edx)");
    asm("  movl %ecx, 660(%edx)");
    asm("  movl %ecx, 661(%edx)");
    asm("  movl %ecx, 662(%edx)");
    asm("  movl %ecx, 663(%edx)");
    asm("  movl %ecx, 664(%edx)");
    asm("  movl %ecx, 665(%edx)");
    asm("  movl %ecx, 666(%edx)");
    asm("  movl %ecx, 667(%edx)");
    asm("  movl %ecx, 668(%edx)");
    asm("  movl %ecx, 669(%edx)");
    asm("  movl %ecx, 670(%edx)");
    asm("  movl %ecx, 671(%edx)");
    asm("  movl %ecx, 672(%edx)");
    asm("  movl %ecx, 673(%edx)");
    asm("  movl %ecx, 674(%edx)");
    asm("  movl %ecx, 675(%edx)");
    asm("  movl %ecx, 676(%edx)");
    asm("  movl %ecx, 677(%edx)");
    asm("  movl %ecx, 678(%edx)");
    asm("  movl %ecx, 679(%edx)");
    asm("  movl %ecx, 680(%edx)");
    asm("  movl %ecx, 681(%edx)");
    asm("  movl %ecx, 682(%edx)");
    asm("  movl %ecx, 683(%edx)");
    asm("  movl %ecx, 684(%edx)");
    asm("  movl %ecx, 685(%edx)");
    asm("  movl %ecx, 686(%edx)");
    asm("  movl %ecx, 687(%edx)");
    asm("  movl %ecx, 688(%edx)");
    asm("  movl %ecx, 689(%edx)");
    asm("  movl %ecx, 690(%edx)");
    asm("  movl %ecx, 691(%edx)");
    asm("  movl %ecx, 692(%edx)");
    asm("  movl %ecx, 693(%edx)");
    asm("  movl %ecx, 694(%edx)");
    asm("  movl %ecx, 695(%edx)");
    asm("  movl %ecx, 696(%edx)");
    asm("  movl %ecx, 697(%edx)");
    asm("  movl %ecx, 698(%edx)");
    asm("  movl %ecx, 699(%edx)");
    asm("  movl %ecx, 700(%edx)");
    asm("  movl %ecx, 701(%edx)");
    asm("  movl %ecx, 702(%edx)");
    asm("  movl %ecx, 703(%edx)");
    asm("  movl %ecx, 704(%edx)");
    asm("  movl %ecx, 705(%edx)");
    asm("  movl %ecx, 706(%edx)");
    asm("  movl %ecx, 707(%edx)");
    asm("  movl %ecx, 708(%edx)");
    asm("  movl %ecx, 709(%edx)");
    asm("  movl %ecx, 710(%edx)");
    asm("  movl %ecx, 711(%edx)");
    asm("  movl %ecx, 712(%edx)");
    asm("  movl %ecx, 713(%edx)");
    asm("  movl %ecx, 714(%edx)");
    asm("  movl %ecx, 715(%edx)");
    asm("  movl %ecx, 716(%edx)");
    asm("  movl %ecx, 717(%edx)");
    asm("  movl %ecx, 718(%edx)");
    asm("  movl %ecx, 719(%edx)");
    asm("  movl %ecx, 720(%edx)");
    asm("  movl %ecx, 721(%edx)");
    asm("  movl %ecx, 722(%edx)");
    asm("  movl %ecx, 723(%edx)");
    asm("  movl %ecx, 724(%edx)");
    asm("  movl %ecx, 725(%edx)");
    asm("  movl %ecx, 726(%edx)");
    asm("  movl %ecx, 727(%edx)");
    asm("  movl %ecx, 728(%edx)");
    asm("  movl %ecx, 729(%edx)");
    asm("  movl %ecx, 730(%edx)");
    asm("  movl %ecx, 731(%edx)");
    asm("  movl %ecx, 732(%edx)");
    asm("  movl %ecx, 733(%edx)");
    asm("  movl %ecx, 734(%edx)");
    asm("  movl %ecx, 735(%edx)");
    asm("  movl %ecx, 736(%edx)");
    asm("  movl %ecx, 737(%edx)");
    asm("  movl %ecx, 738(%edx)");
    asm("  movl %ecx, 739(%edx)");
    asm("  movl %ecx, 740(%edx)");
    asm("  movl %ecx, 741(%edx)");
    asm("  movl %ecx, 742(%edx)");
    asm("  movl %ecx, 743(%edx)");
    asm("  movl %ecx, 744(%edx)");
    asm("  movl %ecx, 745(%edx)");
    asm("  movl %ecx, 746(%edx)");
    asm("  movl %ecx, 747(%edx)");
    asm("  movl %ecx, 748(%edx)");
    asm("  movl %ecx, 749(%edx)");
    asm("  movl %ecx, 750(%edx)");
    asm("  movl %ecx, 751(%edx)");
    asm("  movl %ecx, 752(%edx)");
    asm("  movl %ecx, 753(%edx)");
    asm("  movl %ecx, 754(%edx)");
    asm("  movl %ecx, 755(%edx)");
    asm("  movl %ecx, 756(%edx)");
    asm("  movl %ecx, 757(%edx)");
    asm("  movl %ecx, 758(%edx)");
    asm("  movl %ecx, 759(%edx)");
    asm("  movl %ecx, 760(%edx)");
    asm("  movl %ecx, 761(%edx)");
    asm("  movl %ecx, 762(%edx)");
    asm("  movl %ecx, 763(%edx)");
    asm("  movl %ecx, 764(%edx)");
    asm("  movl %ecx, 765(%edx)");
    asm("  movl %ecx, 766(%edx)");
    asm("  movl %ecx, 767(%edx)");
    asm("  movl %ecx, 768(%edx)");
    asm("  movl %ecx, 769(%edx)");
    asm("  movl %ecx, 770(%edx)");
    asm("  movl %ecx, 771(%edx)");
    asm("  movl %ecx, 772(%edx)");
    asm("  movl %ecx, 773(%edx)");
    asm("  movl %ecx, 774(%edx)");
    asm("  movl %ecx, 775(%edx)");
    asm("  movl %ecx, 776(%edx)");
    asm("  movl %ecx, 777(%edx)");
    asm("  movl %ecx, 778(%edx)");
    asm("  movl %ecx, 779(%edx)");
    asm("  movl %ecx, 780(%edx)");
    asm("  movl %ecx, 781(%edx)");
    asm("  movl %ecx, 782(%edx)");
    asm("  movl %ecx, 783(%edx)");
    asm("  movl %ecx, 784(%edx)");
    asm("  movl %ecx, 785(%edx)");
    asm("  movl %ecx, 786(%edx)");
    asm("  movl %ecx, 787(%edx)");
    asm("  movl %ecx, 788(%edx)");
    asm("  movl %ecx, 789(%edx)");
    asm("  movl %ecx, 790(%edx)");
    asm("  movl %ecx, 791(%edx)");
    asm("  movl %ecx, 792(%edx)");
    asm("  movl %ecx, 793(%edx)");
    asm("  movl %ecx, 794(%edx)");
    asm("  movl %ecx, 795(%edx)");
    asm("  movl %ecx, 796(%edx)");
    asm("  movl %ecx, 797(%edx)");
    asm("  movl %ecx, 798(%edx)");
    asm("  movl %ecx, 799(%edx)");
    asm("  movl %ecx, 800(%edx)");
    asm("  movl %ecx, 801(%edx)");
    asm("  movl %ecx, 802(%edx)");
    asm("  movl %ecx, 803(%edx)");
    asm("  movl %ecx, 804(%edx)");
    asm("  movl %ecx, 805(%edx)");
    asm("  movl %ecx, 806(%edx)");
    asm("  movl %ecx, 807(%edx)");
    asm("  movl %ecx, 808(%edx)");
    asm("  movl %ecx, 809(%edx)");
    asm("  movl %ecx, 810(%edx)");
    asm("  movl %ecx, 811(%edx)");
    asm("  movl %ecx, 812(%edx)");
    asm("  movl %ecx, 813(%edx)");
    asm("  movl %ecx, 814(%edx)");
    asm("  movl %ecx, 815(%edx)");
    asm("  movl %ecx, 816(%edx)");
    asm("  movl %ecx, 817(%edx)");
    asm("  movl %ecx, 818(%edx)");
    asm("  movl %ecx, 819(%edx)");
    asm("  movl %ecx, 820(%edx)");
    asm("  movl %ecx, 821(%edx)");
    asm("  movl %ecx, 822(%edx)");
    asm("  movl %ecx, 823(%edx)");
    asm("  movl %ecx, 824(%edx)");
    asm("  movl %ecx, 825(%edx)");
    asm("  movl %ecx, 826(%edx)");
    asm("  movl %ecx, 827(%edx)");
    asm("  movl %ecx, 828(%edx)");
    asm("  movl %ecx, 829(%edx)");
    asm("  movl %ecx, 830(%edx)");
    asm("  movl %ecx, 831(%edx)");
    asm("  movl %ecx, 832(%edx)");
    asm("  movl %ecx, 833(%edx)");
    asm("  movl %ecx, 834(%edx)");
    asm("  movl %ecx, 835(%edx)");
    asm("  movl %ecx, 836(%edx)");
    asm("  movl %ecx, 837(%edx)");
    asm("  movl %ecx, 838(%edx)");
    asm("  movl %ecx, 839(%edx)");
    asm("  movl %ecx, 840(%edx)");
    asm("  movl %ecx, 841(%edx)");
    asm("  movl %ecx, 842(%edx)");
    asm("  movl %ecx, 843(%edx)");
    asm("  movl %ecx, 844(%edx)");
    asm("  movl %ecx, 845(%edx)");
    asm("  movl %ecx, 846(%edx)");
    asm("  movl %ecx, 847(%edx)");
    asm("  movl %ecx, 848(%edx)");
    asm("  movl %ecx, 849(%edx)");
    asm("  movl %ecx, 850(%edx)");
    asm("  movl %ecx, 851(%edx)");
    asm("  movl %ecx, 852(%edx)");
    asm("  movl %ecx, 853(%edx)");
    asm("  movl %ecx, 854(%edx)");
    asm("  movl %ecx, 855(%edx)");
    asm("  movl %ecx, 856(%edx)");
    asm("  movl %ecx, 857(%edx)");
    asm("  movl %ecx, 858(%edx)");
    asm("  movl %ecx, 859(%edx)");
    asm("  movl %ecx, 860(%edx)");
    asm("  movl %ecx, 861(%edx)");
    asm("  movl %ecx, 862(%edx)");
    asm("  movl %ecx, 863(%edx)");
    asm("  movl %ecx, 864(%edx)");
    asm("  movl %ecx, 865(%edx)");
    asm("  movl %ecx, 866(%edx)");
    asm("  movl %ecx, 867(%edx)");
    asm("  movl %ecx, 868(%edx)");
    asm("  movl %ecx, 869(%edx)");
    asm("  movl %ecx, 870(%edx)");
    asm("  movl %ecx, 871(%edx)");
    asm("  movl %ecx, 872(%edx)");
    asm("  movl %ecx, 873(%edx)");
    asm("  movl %ecx, 874(%edx)");
    asm("  movl %ecx, 875(%edx)");
    asm("  movl %ecx, 876(%edx)");
    asm("  movl %ecx, 877(%edx)");
    asm("  movl %ecx, 878(%edx)");
    asm("  movl %ecx, 879(%edx)");
    asm("  movl %ecx, 880(%edx)");
    asm("  movl %ecx, 881(%edx)");
    asm("  movl %ecx, 882(%edx)");
    asm("  movl %ecx, 883(%edx)");
    asm("  movl %ecx, 884(%edx)");
    asm("  movl %ecx, 885(%edx)");
    asm("  movl %ecx, 886(%edx)");
    asm("  movl %ecx, 887(%edx)");
    asm("  movl %ecx, 888(%edx)");
    asm("  movl %ecx, 889(%edx)");
    asm("  movl %ecx, 890(%edx)");
    asm("  movl %ecx, 891(%edx)");
    asm("  movl %ecx, 892(%edx)");
    asm("  movl %ecx, 893(%edx)");
    asm("  movl %ecx, 894(%edx)");
    asm("  movl %ecx, 895(%edx)");
    asm("  movl %ecx, 896(%edx)");
    asm("  movl %ecx, 897(%edx)");
    asm("  movl %ecx, 898(%edx)");
    asm("  movl %ecx, 899(%edx)");

    asm("  movl $0x0,%ecx"); /* counter for diagnostics */
    asm("repeata:");
    asm("  decl %eax");
    asm("  inc  %ecx");
    asm("  cmpl $0x0,%eax");
    asm("  jnz repeata");
    asm("  movl %%ecx, %0" : "=r" (total));
#else
    __asm {
        call next_inst_prot
      next_inst_prot:
        pop  edx
        push ALLOW_READ|ALLOW_WRITE|ALLOW_EXEC
        push PAGE_SIZE*3
        push edx
        call protect_mem
        add  esp, 12

        mov  ecx, iters
        call next_inst
      next_inst:
        pop  edx
    /* add to retaddr: 1 == pop
     *                 3 == mov ecx into target
     *                 1 == opcode of target movl
     */
        mov  dword ptr [edx + 0x5], ecx /* the modifying store */
        mov  eax,0x12345678 /* this instr's immed gets overwritten */

        /* now we have as many write instrs as necessary to cause a too-big
         * selfmod fragment.  xref case 7893.
         */
        mov  ecx, iters
        mov  edx, offset big
        mov  dword ptr [edx + 0], ecx
        mov  dword ptr [edx + 1], ecx
        mov  dword ptr [edx + 2], ecx
        mov  dword ptr [edx + 3], ecx
        mov  dword ptr [edx + 4], ecx
        mov  dword ptr [edx + 5], ecx
        mov  dword ptr [edx + 6], ecx
        mov  dword ptr [edx + 7], ecx
        mov  dword ptr [edx + 8], ecx
        mov  dword ptr [edx + 9], ecx
        mov  dword ptr [edx + 10], ecx
        mov  dword ptr [edx + 11], ecx
        mov  dword ptr [edx + 12], ecx
        mov  dword ptr [edx + 13], ecx
        mov  dword ptr [edx + 14], ecx
        mov  dword ptr [edx + 15], ecx
        mov  dword ptr [edx + 16], ecx
        mov  dword ptr [edx + 17], ecx
        mov  dword ptr [edx + 18], ecx
        mov  dword ptr [edx + 19], ecx
        mov  dword ptr [edx + 20], ecx
        mov  dword ptr [edx + 21], ecx
        mov  dword ptr [edx + 22], ecx
        mov  dword ptr [edx + 23], ecx
        mov  dword ptr [edx + 24], ecx
        mov  dword ptr [edx + 25], ecx
        mov  dword ptr [edx + 26], ecx
        mov  dword ptr [edx + 27], ecx
        mov  dword ptr [edx + 28], ecx
        mov  dword ptr [edx + 29], ecx
        mov  dword ptr [edx + 30], ecx
        mov  dword ptr [edx + 31], ecx
        mov  dword ptr [edx + 32], ecx
        mov  dword ptr [edx + 33], ecx
        mov  dword ptr [edx + 34], ecx
        mov  dword ptr [edx + 35], ecx
        mov  dword ptr [edx + 36], ecx
        mov  dword ptr [edx + 37], ecx
        mov  dword ptr [edx + 38], ecx
        mov  dword ptr [edx + 39], ecx
        mov  dword ptr [edx + 40], ecx
        mov  dword ptr [edx + 41], ecx
        mov  dword ptr [edx + 42], ecx
        mov  dword ptr [edx + 43], ecx
        mov  dword ptr [edx + 44], ecx
        mov  dword ptr [edx + 45], ecx
        mov  dword ptr [edx + 46], ecx
        mov  dword ptr [edx + 47], ecx
        mov  dword ptr [edx + 48], ecx
        mov  dword ptr [edx + 49], ecx
        mov  dword ptr [edx + 50], ecx
        mov  dword ptr [edx + 51], ecx
        mov  dword ptr [edx + 52], ecx
        mov  dword ptr [edx + 53], ecx
        mov  dword ptr [edx + 54], ecx
        mov  dword ptr [edx + 55], ecx
        mov  dword ptr [edx + 56], ecx
        mov  dword ptr [edx + 57], ecx
        mov  dword ptr [edx + 58], ecx
        mov  dword ptr [edx + 59], ecx
        mov  dword ptr [edx + 60], ecx
        mov  dword ptr [edx + 61], ecx
        mov  dword ptr [edx + 62], ecx
        mov  dword ptr [edx + 63], ecx
        mov  dword ptr [edx + 64], ecx
        mov  dword ptr [edx + 65], ecx
        mov  dword ptr [edx + 66], ecx
        mov  dword ptr [edx + 67], ecx
        mov  dword ptr [edx + 68], ecx
        mov  dword ptr [edx + 69], ecx
        mov  dword ptr [edx + 70], ecx
        mov  dword ptr [edx + 71], ecx
        mov  dword ptr [edx + 72], ecx
        mov  dword ptr [edx + 73], ecx
        mov  dword ptr [edx + 74], ecx
        mov  dword ptr [edx + 75], ecx
        mov  dword ptr [edx + 76], ecx
        mov  dword ptr [edx + 77], ecx
        mov  dword ptr [edx + 78], ecx
        mov  dword ptr [edx + 79], ecx
        mov  dword ptr [edx + 80], ecx
        mov  dword ptr [edx + 81], ecx
        mov  dword ptr [edx + 82], ecx
        mov  dword ptr [edx + 83], ecx
        mov  dword ptr [edx + 84], ecx
        mov  dword ptr [edx + 85], ecx
        mov  dword ptr [edx + 86], ecx
        mov  dword ptr [edx + 87], ecx
        mov  dword ptr [edx + 88], ecx
        mov  dword ptr [edx + 89], ecx
        mov  dword ptr [edx + 90], ecx
        mov  dword ptr [edx + 91], ecx
        mov  dword ptr [edx + 92], ecx
        mov  dword ptr [edx + 93], ecx
        mov  dword ptr [edx + 94], ecx
        mov  dword ptr [edx + 95], ecx
        mov  dword ptr [edx + 96], ecx
        mov  dword ptr [edx + 97], ecx
        mov  dword ptr [edx + 98], ecx
        mov  dword ptr [edx + 99], ecx
        mov  dword ptr [edx + 100], ecx
        mov  dword ptr [edx + 101], ecx
        mov  dword ptr [edx + 102], ecx
        mov  dword ptr [edx + 103], ecx
        mov  dword ptr [edx + 104], ecx
        mov  dword ptr [edx + 105], ecx
        mov  dword ptr [edx + 106], ecx
        mov  dword ptr [edx + 107], ecx
        mov  dword ptr [edx + 108], ecx
        mov  dword ptr [edx + 109], ecx
        mov  dword ptr [edx + 110], ecx
        mov  dword ptr [edx + 111], ecx
        mov  dword ptr [edx + 112], ecx
        mov  dword ptr [edx + 113], ecx
        mov  dword ptr [edx + 114], ecx
        mov  dword ptr [edx + 115], ecx
        mov  dword ptr [edx + 116], ecx
        mov  dword ptr [edx + 117], ecx
        mov  dword ptr [edx + 118], ecx
        mov  dword ptr [edx + 119], ecx
        mov  dword ptr [edx + 120], ecx
        mov  dword ptr [edx + 121], ecx
        mov  dword ptr [edx + 122], ecx
        mov  dword ptr [edx + 123], ecx
        mov  dword ptr [edx + 124], ecx
        mov  dword ptr [edx + 125], ecx
        mov  dword ptr [edx + 126], ecx
        mov  dword ptr [edx + 127], ecx
        mov  dword ptr [edx + 128], ecx
        mov  dword ptr [edx + 129], ecx
        mov  dword ptr [edx + 130], ecx
        mov  dword ptr [edx + 131], ecx
        mov  dword ptr [edx + 132], ecx
        mov  dword ptr [edx + 133], ecx
        mov  dword ptr [edx + 134], ecx
        mov  dword ptr [edx + 135], ecx
        mov  dword ptr [edx + 136], ecx
        mov  dword ptr [edx + 137], ecx
        mov  dword ptr [edx + 138], ecx
        mov  dword ptr [edx + 139], ecx
        mov  dword ptr [edx + 140], ecx
        mov  dword ptr [edx + 141], ecx
        mov  dword ptr [edx + 142], ecx
        mov  dword ptr [edx + 143], ecx
        mov  dword ptr [edx + 144], ecx
        mov  dword ptr [edx + 145], ecx
        mov  dword ptr [edx + 146], ecx
        mov  dword ptr [edx + 147], ecx
        mov  dword ptr [edx + 148], ecx
        mov  dword ptr [edx + 149], ecx
        mov  dword ptr [edx + 150], ecx
        mov  dword ptr [edx + 151], ecx
        mov  dword ptr [edx + 152], ecx
        mov  dword ptr [edx + 153], ecx
        mov  dword ptr [edx + 154], ecx
        mov  dword ptr [edx + 155], ecx
        mov  dword ptr [edx + 156], ecx
        mov  dword ptr [edx + 157], ecx
        mov  dword ptr [edx + 158], ecx
        mov  dword ptr [edx + 159], ecx
        mov  dword ptr [edx + 160], ecx
        mov  dword ptr [edx + 161], ecx
        mov  dword ptr [edx + 162], ecx
        mov  dword ptr [edx + 163], ecx
        mov  dword ptr [edx + 164], ecx
        mov  dword ptr [edx + 165], ecx
        mov  dword ptr [edx + 166], ecx
        mov  dword ptr [edx + 167], ecx
        mov  dword ptr [edx + 168], ecx
        mov  dword ptr [edx + 169], ecx
        mov  dword ptr [edx + 170], ecx
        mov  dword ptr [edx + 171], ecx
        mov  dword ptr [edx + 172], ecx
        mov  dword ptr [edx + 173], ecx
        mov  dword ptr [edx + 174], ecx
        mov  dword ptr [edx + 175], ecx
        mov  dword ptr [edx + 176], ecx
        mov  dword ptr [edx + 177], ecx
        mov  dword ptr [edx + 178], ecx
        mov  dword ptr [edx + 179], ecx
        mov  dword ptr [edx + 180], ecx
        mov  dword ptr [edx + 181], ecx
        mov  dword ptr [edx + 182], ecx
        mov  dword ptr [edx + 183], ecx
        mov  dword ptr [edx + 184], ecx
        mov  dword ptr [edx + 185], ecx
        mov  dword ptr [edx + 186], ecx
        mov  dword ptr [edx + 187], ecx
        mov  dword ptr [edx + 188], ecx
        mov  dword ptr [edx + 189], ecx
        mov  dword ptr [edx + 190], ecx
        mov  dword ptr [edx + 191], ecx
        mov  dword ptr [edx + 192], ecx
        mov  dword ptr [edx + 193], ecx
        mov  dword ptr [edx + 194], ecx
        mov  dword ptr [edx + 195], ecx
        mov  dword ptr [edx + 196], ecx
        mov  dword ptr [edx + 197], ecx
        mov  dword ptr [edx + 198], ecx
        mov  dword ptr [edx + 199], ecx
        mov  dword ptr [edx + 200], ecx
        mov  dword ptr [edx + 201], ecx
        mov  dword ptr [edx + 202], ecx
        mov  dword ptr [edx + 203], ecx
        mov  dword ptr [edx + 204], ecx
        mov  dword ptr [edx + 205], ecx
        mov  dword ptr [edx + 206], ecx
        mov  dword ptr [edx + 207], ecx
        mov  dword ptr [edx + 208], ecx
        mov  dword ptr [edx + 209], ecx
        mov  dword ptr [edx + 210], ecx
        mov  dword ptr [edx + 211], ecx
        mov  dword ptr [edx + 212], ecx
        mov  dword ptr [edx + 213], ecx
        mov  dword ptr [edx + 214], ecx
        mov  dword ptr [edx + 215], ecx
        mov  dword ptr [edx + 216], ecx
        mov  dword ptr [edx + 217], ecx
        mov  dword ptr [edx + 218], ecx
        mov  dword ptr [edx + 219], ecx
        mov  dword ptr [edx + 220], ecx
        mov  dword ptr [edx + 221], ecx
        mov  dword ptr [edx + 222], ecx
        mov  dword ptr [edx + 223], ecx
        mov  dword ptr [edx + 224], ecx
        mov  dword ptr [edx + 225], ecx
        mov  dword ptr [edx + 226], ecx
        mov  dword ptr [edx + 227], ecx
        mov  dword ptr [edx + 228], ecx
        mov  dword ptr [edx + 229], ecx
        mov  dword ptr [edx + 230], ecx
        mov  dword ptr [edx + 231], ecx
        mov  dword ptr [edx + 232], ecx
        mov  dword ptr [edx + 233], ecx
        mov  dword ptr [edx + 234], ecx
        mov  dword ptr [edx + 235], ecx
        mov  dword ptr [edx + 236], ecx
        mov  dword ptr [edx + 237], ecx
        mov  dword ptr [edx + 238], ecx
        mov  dword ptr [edx + 239], ecx
        mov  dword ptr [edx + 240], ecx
        mov  dword ptr [edx + 241], ecx
        mov  dword ptr [edx + 242], ecx
        mov  dword ptr [edx + 243], ecx
        mov  dword ptr [edx + 244], ecx
        mov  dword ptr [edx + 245], ecx
        mov  dword ptr [edx + 246], ecx
        mov  dword ptr [edx + 247], ecx
        mov  dword ptr [edx + 248], ecx
        mov  dword ptr [edx + 249], ecx
        mov  dword ptr [edx + 250], ecx
        mov  dword ptr [edx + 251], ecx
        mov  dword ptr [edx + 252], ecx
        mov  dword ptr [edx + 253], ecx
        mov  dword ptr [edx + 254], ecx
        mov  dword ptr [edx + 255], ecx
        mov  dword ptr [edx + 256], ecx
        mov  dword ptr [edx + 257], ecx
        mov  dword ptr [edx + 258], ecx
        mov  dword ptr [edx + 259], ecx
        mov  dword ptr [edx + 260], ecx
        mov  dword ptr [edx + 261], ecx
        mov  dword ptr [edx + 262], ecx
        mov  dword ptr [edx + 263], ecx
        mov  dword ptr [edx + 264], ecx
        mov  dword ptr [edx + 265], ecx
        mov  dword ptr [edx + 266], ecx
        mov  dword ptr [edx + 267], ecx
        mov  dword ptr [edx + 268], ecx
        mov  dword ptr [edx + 269], ecx
        mov  dword ptr [edx + 270], ecx
        mov  dword ptr [edx + 271], ecx
        mov  dword ptr [edx + 272], ecx
        mov  dword ptr [edx + 273], ecx
        mov  dword ptr [edx + 274], ecx
        mov  dword ptr [edx + 275], ecx
        mov  dword ptr [edx + 276], ecx
        mov  dword ptr [edx + 277], ecx
        mov  dword ptr [edx + 278], ecx
        mov  dword ptr [edx + 279], ecx
        mov  dword ptr [edx + 280], ecx
        mov  dword ptr [edx + 281], ecx
        mov  dword ptr [edx + 282], ecx
        mov  dword ptr [edx + 283], ecx
        mov  dword ptr [edx + 284], ecx
        mov  dword ptr [edx + 285], ecx
        mov  dword ptr [edx + 286], ecx
        mov  dword ptr [edx + 287], ecx
        mov  dword ptr [edx + 288], ecx
        mov  dword ptr [edx + 289], ecx
        mov  dword ptr [edx + 290], ecx
        mov  dword ptr [edx + 291], ecx
        mov  dword ptr [edx + 292], ecx
        mov  dword ptr [edx + 293], ecx
        mov  dword ptr [edx + 294], ecx
        mov  dword ptr [edx + 295], ecx
        mov  dword ptr [edx + 296], ecx
        mov  dword ptr [edx + 297], ecx
        mov  dword ptr [edx + 298], ecx
        mov  dword ptr [edx + 299], ecx
        mov  dword ptr [edx + 300], ecx
        mov  dword ptr [edx + 301], ecx
        mov  dword ptr [edx + 302], ecx
        mov  dword ptr [edx + 303], ecx
        mov  dword ptr [edx + 304], ecx
        mov  dword ptr [edx + 305], ecx
        mov  dword ptr [edx + 306], ecx
        mov  dword ptr [edx + 307], ecx
        mov  dword ptr [edx + 308], ecx
        mov  dword ptr [edx + 309], ecx
        mov  dword ptr [edx + 310], ecx
        mov  dword ptr [edx + 311], ecx
        mov  dword ptr [edx + 312], ecx
        mov  dword ptr [edx + 313], ecx
        mov  dword ptr [edx + 314], ecx
        mov  dword ptr [edx + 315], ecx
        mov  dword ptr [edx + 316], ecx
        mov  dword ptr [edx + 317], ecx
        mov  dword ptr [edx + 318], ecx
        mov  dword ptr [edx + 319], ecx
        mov  dword ptr [edx + 320], ecx
        mov  dword ptr [edx + 321], ecx
        mov  dword ptr [edx + 322], ecx
        mov  dword ptr [edx + 323], ecx
        mov  dword ptr [edx + 324], ecx
        mov  dword ptr [edx + 325], ecx
        mov  dword ptr [edx + 326], ecx
        mov  dword ptr [edx + 327], ecx
        mov  dword ptr [edx + 328], ecx
        mov  dword ptr [edx + 329], ecx
        mov  dword ptr [edx + 330], ecx
        mov  dword ptr [edx + 331], ecx
        mov  dword ptr [edx + 332], ecx
        mov  dword ptr [edx + 333], ecx
        mov  dword ptr [edx + 334], ecx
        mov  dword ptr [edx + 335], ecx
        mov  dword ptr [edx + 336], ecx
        mov  dword ptr [edx + 337], ecx
        mov  dword ptr [edx + 338], ecx
        mov  dword ptr [edx + 339], ecx
        mov  dword ptr [edx + 340], ecx
        mov  dword ptr [edx + 341], ecx
        mov  dword ptr [edx + 342], ecx
        mov  dword ptr [edx + 343], ecx
        mov  dword ptr [edx + 344], ecx
        mov  dword ptr [edx + 345], ecx
        mov  dword ptr [edx + 346], ecx
        mov  dword ptr [edx + 347], ecx
        mov  dword ptr [edx + 348], ecx
        mov  dword ptr [edx + 349], ecx
        mov  dword ptr [edx + 350], ecx
        mov  dword ptr [edx + 351], ecx
        mov  dword ptr [edx + 352], ecx
        mov  dword ptr [edx + 353], ecx
        mov  dword ptr [edx + 354], ecx
        mov  dword ptr [edx + 355], ecx
        mov  dword ptr [edx + 356], ecx
        mov  dword ptr [edx + 357], ecx
        mov  dword ptr [edx + 358], ecx
        mov  dword ptr [edx + 359], ecx
        mov  dword ptr [edx + 360], ecx
        mov  dword ptr [edx + 361], ecx
        mov  dword ptr [edx + 362], ecx
        mov  dword ptr [edx + 363], ecx
        mov  dword ptr [edx + 364], ecx
        mov  dword ptr [edx + 365], ecx
        mov  dword ptr [edx + 366], ecx
        mov  dword ptr [edx + 367], ecx
        mov  dword ptr [edx + 368], ecx
        mov  dword ptr [edx + 369], ecx
        mov  dword ptr [edx + 370], ecx
        mov  dword ptr [edx + 371], ecx
        mov  dword ptr [edx + 372], ecx
        mov  dword ptr [edx + 373], ecx
        mov  dword ptr [edx + 374], ecx
        mov  dword ptr [edx + 375], ecx
        mov  dword ptr [edx + 376], ecx
        mov  dword ptr [edx + 377], ecx
        mov  dword ptr [edx + 378], ecx
        mov  dword ptr [edx + 379], ecx
        mov  dword ptr [edx + 380], ecx
        mov  dword ptr [edx + 381], ecx
        mov  dword ptr [edx + 382], ecx
        mov  dword ptr [edx + 383], ecx
        mov  dword ptr [edx + 384], ecx
        mov  dword ptr [edx + 385], ecx
        mov  dword ptr [edx + 386], ecx
        mov  dword ptr [edx + 387], ecx
        mov  dword ptr [edx + 388], ecx
        mov  dword ptr [edx + 389], ecx
        mov  dword ptr [edx + 390], ecx
        mov  dword ptr [edx + 391], ecx
        mov  dword ptr [edx + 392], ecx
        mov  dword ptr [edx + 393], ecx
        mov  dword ptr [edx + 394], ecx
        mov  dword ptr [edx + 395], ecx
        mov  dword ptr [edx + 396], ecx
        mov  dword ptr [edx + 397], ecx
        mov  dword ptr [edx + 398], ecx
        mov  dword ptr [edx + 399], ecx
        mov  dword ptr [edx + 400], ecx
        mov  dword ptr [edx + 401], ecx
        mov  dword ptr [edx + 402], ecx
        mov  dword ptr [edx + 403], ecx
        mov  dword ptr [edx + 404], ecx
        mov  dword ptr [edx + 405], ecx
        mov  dword ptr [edx + 406], ecx
        mov  dword ptr [edx + 407], ecx
        mov  dword ptr [edx + 408], ecx
        mov  dword ptr [edx + 409], ecx
        mov  dword ptr [edx + 410], ecx
        mov  dword ptr [edx + 411], ecx
        mov  dword ptr [edx + 412], ecx
        mov  dword ptr [edx + 413], ecx
        mov  dword ptr [edx + 414], ecx
        mov  dword ptr [edx + 415], ecx
        mov  dword ptr [edx + 416], ecx
        mov  dword ptr [edx + 417], ecx
        mov  dword ptr [edx + 418], ecx
        mov  dword ptr [edx + 419], ecx
        mov  dword ptr [edx + 420], ecx
        mov  dword ptr [edx + 421], ecx
        mov  dword ptr [edx + 422], ecx
        mov  dword ptr [edx + 423], ecx
        mov  dword ptr [edx + 424], ecx
        mov  dword ptr [edx + 425], ecx
        mov  dword ptr [edx + 426], ecx
        mov  dword ptr [edx + 427], ecx
        mov  dword ptr [edx + 428], ecx
        mov  dword ptr [edx + 429], ecx
        mov  dword ptr [edx + 430], ecx
        mov  dword ptr [edx + 431], ecx
        mov  dword ptr [edx + 432], ecx
        mov  dword ptr [edx + 433], ecx
        mov  dword ptr [edx + 434], ecx
        mov  dword ptr [edx + 435], ecx
        mov  dword ptr [edx + 436], ecx
        mov  dword ptr [edx + 437], ecx
        mov  dword ptr [edx + 438], ecx
        mov  dword ptr [edx + 439], ecx
        mov  dword ptr [edx + 440], ecx
        mov  dword ptr [edx + 441], ecx
        mov  dword ptr [edx + 442], ecx
        mov  dword ptr [edx + 443], ecx
        mov  dword ptr [edx + 444], ecx
        mov  dword ptr [edx + 445], ecx
        mov  dword ptr [edx + 446], ecx
        mov  dword ptr [edx + 447], ecx
        mov  dword ptr [edx + 448], ecx
        mov  dword ptr [edx + 449], ecx
        mov  dword ptr [edx + 450], ecx
        mov  dword ptr [edx + 451], ecx
        mov  dword ptr [edx + 452], ecx
        mov  dword ptr [edx + 453], ecx
        mov  dword ptr [edx + 454], ecx
        mov  dword ptr [edx + 455], ecx
        mov  dword ptr [edx + 456], ecx
        mov  dword ptr [edx + 457], ecx
        mov  dword ptr [edx + 458], ecx
        mov  dword ptr [edx + 459], ecx
        mov  dword ptr [edx + 460], ecx
        mov  dword ptr [edx + 461], ecx
        mov  dword ptr [edx + 462], ecx
        mov  dword ptr [edx + 463], ecx
        mov  dword ptr [edx + 464], ecx
        mov  dword ptr [edx + 465], ecx
        mov  dword ptr [edx + 466], ecx
        mov  dword ptr [edx + 467], ecx
        mov  dword ptr [edx + 468], ecx
        mov  dword ptr [edx + 469], ecx
        mov  dword ptr [edx + 470], ecx
        mov  dword ptr [edx + 471], ecx
        mov  dword ptr [edx + 472], ecx
        mov  dword ptr [edx + 473], ecx
        mov  dword ptr [edx + 474], ecx
        mov  dword ptr [edx + 475], ecx
        mov  dword ptr [edx + 476], ecx
        mov  dword ptr [edx + 477], ecx
        mov  dword ptr [edx + 478], ecx
        mov  dword ptr [edx + 479], ecx
        mov  dword ptr [edx + 480], ecx
        mov  dword ptr [edx + 481], ecx
        mov  dword ptr [edx + 482], ecx
        mov  dword ptr [edx + 483], ecx
        mov  dword ptr [edx + 484], ecx
        mov  dword ptr [edx + 485], ecx
        mov  dword ptr [edx + 486], ecx
        mov  dword ptr [edx + 487], ecx
        mov  dword ptr [edx + 488], ecx
        mov  dword ptr [edx + 489], ecx
        mov  dword ptr [edx + 490], ecx
        mov  dword ptr [edx + 491], ecx
        mov  dword ptr [edx + 492], ecx
        mov  dword ptr [edx + 493], ecx
        mov  dword ptr [edx + 494], ecx
        mov  dword ptr [edx + 495], ecx
        mov  dword ptr [edx + 496], ecx
        mov  dword ptr [edx + 497], ecx
        mov  dword ptr [edx + 498], ecx
        mov  dword ptr [edx + 499], ecx
        mov  dword ptr [edx + 500], ecx
        mov  dword ptr [edx + 501], ecx
        mov  dword ptr [edx + 502], ecx
        mov  dword ptr [edx + 503], ecx
        mov  dword ptr [edx + 504], ecx
        mov  dword ptr [edx + 505], ecx
        mov  dword ptr [edx + 506], ecx
        mov  dword ptr [edx + 507], ecx
        mov  dword ptr [edx + 508], ecx
        mov  dword ptr [edx + 509], ecx
        mov  dword ptr [edx + 510], ecx
        mov  dword ptr [edx + 511], ecx
        mov  dword ptr [edx + 512], ecx
        mov  dword ptr [edx + 513], ecx
        mov  dword ptr [edx + 514], ecx
        mov  dword ptr [edx + 515], ecx
        mov  dword ptr [edx + 516], ecx
        mov  dword ptr [edx + 517], ecx
        mov  dword ptr [edx + 518], ecx
        mov  dword ptr [edx + 519], ecx
        mov  dword ptr [edx + 520], ecx
        mov  dword ptr [edx + 521], ecx
        mov  dword ptr [edx + 522], ecx
        mov  dword ptr [edx + 523], ecx
        mov  dword ptr [edx + 524], ecx
        mov  dword ptr [edx + 525], ecx
        mov  dword ptr [edx + 526], ecx
        mov  dword ptr [edx + 527], ecx
        mov  dword ptr [edx + 528], ecx
        mov  dword ptr [edx + 529], ecx
        mov  dword ptr [edx + 530], ecx
        mov  dword ptr [edx + 531], ecx
        mov  dword ptr [edx + 532], ecx
        mov  dword ptr [edx + 533], ecx
        mov  dword ptr [edx + 534], ecx
        mov  dword ptr [edx + 535], ecx
        mov  dword ptr [edx + 536], ecx
        mov  dword ptr [edx + 537], ecx
        mov  dword ptr [edx + 538], ecx
        mov  dword ptr [edx + 539], ecx
        mov  dword ptr [edx + 540], ecx
        mov  dword ptr [edx + 541], ecx
        mov  dword ptr [edx + 542], ecx
        mov  dword ptr [edx + 543], ecx
        mov  dword ptr [edx + 544], ecx
        mov  dword ptr [edx + 545], ecx
        mov  dword ptr [edx + 546], ecx
        mov  dword ptr [edx + 547], ecx
        mov  dword ptr [edx + 548], ecx
        mov  dword ptr [edx + 549], ecx
        mov  dword ptr [edx + 550], ecx
        mov  dword ptr [edx + 551], ecx
        mov  dword ptr [edx + 552], ecx
        mov  dword ptr [edx + 553], ecx
        mov  dword ptr [edx + 554], ecx
        mov  dword ptr [edx + 555], ecx
        mov  dword ptr [edx + 556], ecx
        mov  dword ptr [edx + 557], ecx
        mov  dword ptr [edx + 558], ecx
        mov  dword ptr [edx + 559], ecx
        mov  dword ptr [edx + 560], ecx
        mov  dword ptr [edx + 561], ecx
        mov  dword ptr [edx + 562], ecx
        mov  dword ptr [edx + 563], ecx
        mov  dword ptr [edx + 564], ecx
        mov  dword ptr [edx + 565], ecx
        mov  dword ptr [edx + 566], ecx
        mov  dword ptr [edx + 567], ecx
        mov  dword ptr [edx + 568], ecx
        mov  dword ptr [edx + 569], ecx
        mov  dword ptr [edx + 570], ecx
        mov  dword ptr [edx + 571], ecx
        mov  dword ptr [edx + 572], ecx
        mov  dword ptr [edx + 573], ecx
        mov  dword ptr [edx + 574], ecx
        mov  dword ptr [edx + 575], ecx
        mov  dword ptr [edx + 576], ecx
        mov  dword ptr [edx + 577], ecx
        mov  dword ptr [edx + 578], ecx
        mov  dword ptr [edx + 579], ecx
        mov  dword ptr [edx + 580], ecx
        mov  dword ptr [edx + 581], ecx
        mov  dword ptr [edx + 582], ecx
        mov  dword ptr [edx + 583], ecx
        mov  dword ptr [edx + 584], ecx
        mov  dword ptr [edx + 585], ecx
        mov  dword ptr [edx + 586], ecx
        mov  dword ptr [edx + 587], ecx
        mov  dword ptr [edx + 588], ecx
        mov  dword ptr [edx + 589], ecx
        mov  dword ptr [edx + 590], ecx
        mov  dword ptr [edx + 591], ecx
        mov  dword ptr [edx + 592], ecx
        mov  dword ptr [edx + 593], ecx
        mov  dword ptr [edx + 594], ecx
        mov  dword ptr [edx + 595], ecx
        mov  dword ptr [edx + 596], ecx
        mov  dword ptr [edx + 597], ecx
        mov  dword ptr [edx + 598], ecx
        mov  dword ptr [edx + 599], ecx
        mov  dword ptr [edx + 600], ecx
        mov  dword ptr [edx + 601], ecx
        mov  dword ptr [edx + 602], ecx
        mov  dword ptr [edx + 603], ecx
        mov  dword ptr [edx + 604], ecx
        mov  dword ptr [edx + 605], ecx
        mov  dword ptr [edx + 606], ecx
        mov  dword ptr [edx + 607], ecx
        mov  dword ptr [edx + 608], ecx
        mov  dword ptr [edx + 609], ecx
        mov  dword ptr [edx + 610], ecx
        mov  dword ptr [edx + 611], ecx
        mov  dword ptr [edx + 612], ecx
        mov  dword ptr [edx + 613], ecx
        mov  dword ptr [edx + 614], ecx
        mov  dword ptr [edx + 615], ecx
        mov  dword ptr [edx + 616], ecx
        mov  dword ptr [edx + 617], ecx
        mov  dword ptr [edx + 618], ecx
        mov  dword ptr [edx + 619], ecx
        mov  dword ptr [edx + 620], ecx
        mov  dword ptr [edx + 621], ecx
        mov  dword ptr [edx + 622], ecx
        mov  dword ptr [edx + 623], ecx
        mov  dword ptr [edx + 624], ecx
        mov  dword ptr [edx + 625], ecx
        mov  dword ptr [edx + 626], ecx
        mov  dword ptr [edx + 627], ecx
        mov  dword ptr [edx + 628], ecx
        mov  dword ptr [edx + 629], ecx
        mov  dword ptr [edx + 630], ecx
        mov  dword ptr [edx + 631], ecx
        mov  dword ptr [edx + 632], ecx
        mov  dword ptr [edx + 633], ecx
        mov  dword ptr [edx + 634], ecx
        mov  dword ptr [edx + 635], ecx
        mov  dword ptr [edx + 636], ecx
        mov  dword ptr [edx + 637], ecx
        mov  dword ptr [edx + 638], ecx
        mov  dword ptr [edx + 639], ecx
        mov  dword ptr [edx + 640], ecx
        mov  dword ptr [edx + 641], ecx
        mov  dword ptr [edx + 642], ecx
        mov  dword ptr [edx + 643], ecx
        mov  dword ptr [edx + 644], ecx
        mov  dword ptr [edx + 645], ecx
        mov  dword ptr [edx + 646], ecx
        mov  dword ptr [edx + 647], ecx
        mov  dword ptr [edx + 648], ecx
        mov  dword ptr [edx + 649], ecx
        mov  dword ptr [edx + 650], ecx
        mov  dword ptr [edx + 651], ecx
        mov  dword ptr [edx + 652], ecx
        mov  dword ptr [edx + 653], ecx
        mov  dword ptr [edx + 654], ecx
        mov  dword ptr [edx + 655], ecx
        mov  dword ptr [edx + 656], ecx
        mov  dword ptr [edx + 657], ecx
        mov  dword ptr [edx + 658], ecx
        mov  dword ptr [edx + 659], ecx
        mov  dword ptr [edx + 660], ecx
        mov  dword ptr [edx + 661], ecx
        mov  dword ptr [edx + 662], ecx
        mov  dword ptr [edx + 663], ecx
        mov  dword ptr [edx + 664], ecx
        mov  dword ptr [edx + 665], ecx
        mov  dword ptr [edx + 666], ecx
        mov  dword ptr [edx + 667], ecx
        mov  dword ptr [edx + 668], ecx
        mov  dword ptr [edx + 669], ecx
        mov  dword ptr [edx + 670], ecx
        mov  dword ptr [edx + 671], ecx
        mov  dword ptr [edx + 672], ecx
        mov  dword ptr [edx + 673], ecx
        mov  dword ptr [edx + 674], ecx
        mov  dword ptr [edx + 675], ecx
        mov  dword ptr [edx + 676], ecx
        mov  dword ptr [edx + 677], ecx
        mov  dword ptr [edx + 678], ecx
        mov  dword ptr [edx + 679], ecx
        mov  dword ptr [edx + 680], ecx
        mov  dword ptr [edx + 681], ecx
        mov  dword ptr [edx + 682], ecx
        mov  dword ptr [edx + 683], ecx
        mov  dword ptr [edx + 684], ecx
        mov  dword ptr [edx + 685], ecx
        mov  dword ptr [edx + 686], ecx
        mov  dword ptr [edx + 687], ecx
        mov  dword ptr [edx + 688], ecx
        mov  dword ptr [edx + 689], ecx
        mov  dword ptr [edx + 690], ecx
        mov  dword ptr [edx + 691], ecx
        mov  dword ptr [edx + 692], ecx
        mov  dword ptr [edx + 693], ecx
        mov  dword ptr [edx + 694], ecx
        mov  dword ptr [edx + 695], ecx
        mov  dword ptr [edx + 696], ecx
        mov  dword ptr [edx + 697], ecx
        mov  dword ptr [edx + 698], ecx
        mov  dword ptr [edx + 699], ecx
        mov  dword ptr [edx + 700], ecx
        mov  dword ptr [edx + 701], ecx
        mov  dword ptr [edx + 702], ecx
        mov  dword ptr [edx + 703], ecx
        mov  dword ptr [edx + 704], ecx
        mov  dword ptr [edx + 705], ecx
        mov  dword ptr [edx + 706], ecx
        mov  dword ptr [edx + 707], ecx
        mov  dword ptr [edx + 708], ecx
        mov  dword ptr [edx + 709], ecx
        mov  dword ptr [edx + 710], ecx
        mov  dword ptr [edx + 711], ecx
        mov  dword ptr [edx + 712], ecx
        mov  dword ptr [edx + 713], ecx
        mov  dword ptr [edx + 714], ecx
        mov  dword ptr [edx + 715], ecx
        mov  dword ptr [edx + 716], ecx
        mov  dword ptr [edx + 717], ecx
        mov  dword ptr [edx + 718], ecx
        mov  dword ptr [edx + 719], ecx
        mov  dword ptr [edx + 720], ecx
        mov  dword ptr [edx + 721], ecx
        mov  dword ptr [edx + 722], ecx
        mov  dword ptr [edx + 723], ecx
        mov  dword ptr [edx + 724], ecx
        mov  dword ptr [edx + 725], ecx
        mov  dword ptr [edx + 726], ecx
        mov  dword ptr [edx + 727], ecx
        mov  dword ptr [edx + 728], ecx
        mov  dword ptr [edx + 729], ecx
        mov  dword ptr [edx + 730], ecx
        mov  dword ptr [edx + 731], ecx
        mov  dword ptr [edx + 732], ecx
        mov  dword ptr [edx + 733], ecx
        mov  dword ptr [edx + 734], ecx
        mov  dword ptr [edx + 735], ecx
        mov  dword ptr [edx + 736], ecx
        mov  dword ptr [edx + 737], ecx
        mov  dword ptr [edx + 738], ecx
        mov  dword ptr [edx + 739], ecx
        mov  dword ptr [edx + 740], ecx
        mov  dword ptr [edx + 741], ecx
        mov  dword ptr [edx + 742], ecx
        mov  dword ptr [edx + 743], ecx
        mov  dword ptr [edx + 744], ecx
        mov  dword ptr [edx + 745], ecx
        mov  dword ptr [edx + 746], ecx
        mov  dword ptr [edx + 747], ecx
        mov  dword ptr [edx + 748], ecx
        mov  dword ptr [edx + 749], ecx
        mov  dword ptr [edx + 750], ecx
        mov  dword ptr [edx + 751], ecx
        mov  dword ptr [edx + 752], ecx
        mov  dword ptr [edx + 753], ecx
        mov  dword ptr [edx + 754], ecx
        mov  dword ptr [edx + 755], ecx
        mov  dword ptr [edx + 756], ecx
        mov  dword ptr [edx + 757], ecx
        mov  dword ptr [edx + 758], ecx
        mov  dword ptr [edx + 759], ecx
        mov  dword ptr [edx + 760], ecx
        mov  dword ptr [edx + 761], ecx
        mov  dword ptr [edx + 762], ecx
        mov  dword ptr [edx + 763], ecx
        mov  dword ptr [edx + 764], ecx
        mov  dword ptr [edx + 765], ecx
        mov  dword ptr [edx + 766], ecx
        mov  dword ptr [edx + 767], ecx
        mov  dword ptr [edx + 768], ecx
        mov  dword ptr [edx + 769], ecx
        mov  dword ptr [edx + 770], ecx
        mov  dword ptr [edx + 771], ecx
        mov  dword ptr [edx + 772], ecx
        mov  dword ptr [edx + 773], ecx
        mov  dword ptr [edx + 774], ecx
        mov  dword ptr [edx + 775], ecx
        mov  dword ptr [edx + 776], ecx
        mov  dword ptr [edx + 777], ecx
        mov  dword ptr [edx + 778], ecx
        mov  dword ptr [edx + 779], ecx
        mov  dword ptr [edx + 780], ecx
        mov  dword ptr [edx + 781], ecx
        mov  dword ptr [edx + 782], ecx
        mov  dword ptr [edx + 783], ecx
        mov  dword ptr [edx + 784], ecx
        mov  dword ptr [edx + 785], ecx
        mov  dword ptr [edx + 786], ecx
        mov  dword ptr [edx + 787], ecx
        mov  dword ptr [edx + 788], ecx
        mov  dword ptr [edx + 789], ecx
        mov  dword ptr [edx + 790], ecx
        mov  dword ptr [edx + 791], ecx
        mov  dword ptr [edx + 792], ecx
        mov  dword ptr [edx + 793], ecx
        mov  dword ptr [edx + 794], ecx
        mov  dword ptr [edx + 795], ecx
        mov  dword ptr [edx + 796], ecx
        mov  dword ptr [edx + 797], ecx
        mov  dword ptr [edx + 798], ecx
        mov  dword ptr [edx + 799], ecx
        mov  dword ptr [edx + 800], ecx
        mov  dword ptr [edx + 801], ecx
        mov  dword ptr [edx + 802], ecx
        mov  dword ptr [edx + 803], ecx
        mov  dword ptr [edx + 804], ecx
        mov  dword ptr [edx + 805], ecx
        mov  dword ptr [edx + 806], ecx
        mov  dword ptr [edx + 807], ecx
        mov  dword ptr [edx + 808], ecx
        mov  dword ptr [edx + 809], ecx
        mov  dword ptr [edx + 810], ecx
        mov  dword ptr [edx + 811], ecx
        mov  dword ptr [edx + 812], ecx
        mov  dword ptr [edx + 813], ecx
        mov  dword ptr [edx + 814], ecx
        mov  dword ptr [edx + 815], ecx
        mov  dword ptr [edx + 816], ecx
        mov  dword ptr [edx + 817], ecx
        mov  dword ptr [edx + 818], ecx
        mov  dword ptr [edx + 819], ecx
        mov  dword ptr [edx + 820], ecx
        mov  dword ptr [edx + 821], ecx
        mov  dword ptr [edx + 822], ecx
        mov  dword ptr [edx + 823], ecx
        mov  dword ptr [edx + 824], ecx
        mov  dword ptr [edx + 825], ecx
        mov  dword ptr [edx + 826], ecx
        mov  dword ptr [edx + 827], ecx
        mov  dword ptr [edx + 828], ecx
        mov  dword ptr [edx + 829], ecx
        mov  dword ptr [edx + 830], ecx
        mov  dword ptr [edx + 831], ecx
        mov  dword ptr [edx + 832], ecx
        mov  dword ptr [edx + 833], ecx
        mov  dword ptr [edx + 834], ecx
        mov  dword ptr [edx + 835], ecx
        mov  dword ptr [edx + 836], ecx
        mov  dword ptr [edx + 837], ecx
        mov  dword ptr [edx + 838], ecx
        mov  dword ptr [edx + 839], ecx
        mov  dword ptr [edx + 840], ecx
        mov  dword ptr [edx + 841], ecx
        mov  dword ptr [edx + 842], ecx
        mov  dword ptr [edx + 843], ecx
        mov  dword ptr [edx + 844], ecx
        mov  dword ptr [edx + 845], ecx
        mov  dword ptr [edx + 846], ecx
        mov  dword ptr [edx + 847], ecx
        mov  dword ptr [edx + 848], ecx
        mov  dword ptr [edx + 849], ecx
        mov  dword ptr [edx + 850], ecx
        mov  dword ptr [edx + 851], ecx
        mov  dword ptr [edx + 852], ecx
        mov  dword ptr [edx + 853], ecx
        mov  dword ptr [edx + 854], ecx
        mov  dword ptr [edx + 855], ecx
        mov  dword ptr [edx + 856], ecx
        mov  dword ptr [edx + 857], ecx
        mov  dword ptr [edx + 858], ecx
        mov  dword ptr [edx + 859], ecx
        mov  dword ptr [edx + 860], ecx
        mov  dword ptr [edx + 861], ecx
        mov  dword ptr [edx + 862], ecx
        mov  dword ptr [edx + 863], ecx
        mov  dword ptr [edx + 864], ecx
        mov  dword ptr [edx + 865], ecx
        mov  dword ptr [edx + 866], ecx
        mov  dword ptr [edx + 867], ecx
        mov  dword ptr [edx + 868], ecx
        mov  dword ptr [edx + 869], ecx
        mov  dword ptr [edx + 870], ecx
        mov  dword ptr [edx + 871], ecx
        mov  dword ptr [edx + 872], ecx
        mov  dword ptr [edx + 873], ecx
        mov  dword ptr [edx + 874], ecx
        mov  dword ptr [edx + 875], ecx
        mov  dword ptr [edx + 876], ecx
        mov  dword ptr [edx + 877], ecx
        mov  dword ptr [edx + 878], ecx
        mov  dword ptr [edx + 879], ecx
        mov  dword ptr [edx + 880], ecx
        mov  dword ptr [edx + 881], ecx
        mov  dword ptr [edx + 882], ecx
        mov  dword ptr [edx + 883], ecx
        mov  dword ptr [edx + 884], ecx
        mov  dword ptr [edx + 885], ecx
        mov  dword ptr [edx + 886], ecx
        mov  dword ptr [edx + 887], ecx
        mov  dword ptr [edx + 888], ecx
        mov  dword ptr [edx + 889], ecx
        mov  dword ptr [edx + 890], ecx
        mov  dword ptr [edx + 891], ecx
        mov  dword ptr [edx + 892], ecx
        mov  dword ptr [edx + 893], ecx
        mov  dword ptr [edx + 894], ecx
        mov  dword ptr [edx + 895], ecx
        mov  dword ptr [edx + 896], ecx
        mov  dword ptr [edx + 897], ecx
        mov  dword ptr [edx + 898], ecx
        mov  dword ptr [edx + 899], ecx
        mov  ecx,0x0 /* counter for diagnostics */
      repeata:
        dec  eax
        inc  ecx
        cmp  eax,0x0
        jnz  repeata
        mov  total,ecx
    }
#endif
    print("Executed 0x%x iters\n", total);
}

int
main()
{
#ifndef LINUX
    int old;
#endif
    INIT();

#ifdef USE_DYNAMO
    dynamorio_app_init();
    dynamorio_app_start();
#endif

    foo(0xabcd);
    foo(0x1234);
    foo(0xef01);

#ifdef USE_DYNAMO
    dynamorio_app_stop();
    dynamorio_app_exit();
#endif

    return 0;
}
