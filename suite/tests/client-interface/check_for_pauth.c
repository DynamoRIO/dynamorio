/* ****************************************************
 * Copyright (c) 2026 Arm Limited  All rights reserved.
 * ***************************************************/

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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <stdbool.h>

/* This function has been extracted to a file as it is used by a CMake
 * script and by a unit test. Only standard C types are used so it can
 * easily be used by the CMake script as a code fragment.
 */
static bool
pauth_indicated_by_isa_registers()
{
    long long id_aa64isar1_el1 = 0;
    long long id_aa64isar2_el1 = 0;

    /* Read the two relevant registers and check the feature nibbles. */
    asm("mrs %0, ID_AA64ISAR1_EL1" : "=r"(id_aa64isar1_el1));

    /* The ID_AA64ISAR2_EL1 register is not recognized by gcc 11.5. */
    asm(".inst 0xd5380640\n" /* mrs x0, ID_AA64ISAR2_EL1 */
        "mov %0, x0"
        : "=r"(id_aa64isar2_el1)
        :
        : "x0");

    /* IMPLEMENTATION DEFINED algorithm for generic code authentication */
#define GPI (0x1 << 28)

    /* QARMA5 algorithm for generic code authentication */
#define GPA (0x1 << 24)

    /* IMPLEMENTATION DEFINED algorithm for address authentication */
#define API (0xF << 8)

    /* QARMA5 algorithm for address authentication */
#define APA (0xF << 4)

    /* QARMA3 algorithm for address authentication */
#define APA3 (0xF << 12)

    /* QARMA3 algorithm for generic code authentication */
#define GPA3 (0x1 << 8)

    /* If one of these conditions is met then FEATURE_PAUTH is implemented. */
    if ((id_aa64isar1_el1 & GPI) || (id_aa64isar1_el1 & GPA) ||
        (id_aa64isar1_el1 & API) || (id_aa64isar1_el1 & APA) ||
        (id_aa64isar2_el1 & APA3) || (id_aa64isar2_el1 & GPA3))
        return true;

    return false;
}
