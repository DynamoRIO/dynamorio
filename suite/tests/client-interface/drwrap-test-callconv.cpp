/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test the drwrap extension with non-default calling conventions where available. */

#include "tools.h"
#include "stdio.h"

#if defined(X64) || defined(ARM)
#    define FASTCALL /* there is no fastcall on these platforms */
#else
#    ifdef WINDOWS
#        define FASTCALL __fastcall
#    elif defined(UNIX)
#        define FASTCALL __attribute__((fastcall))
#    endif
#endif

#define DEFAULT_LENGTH 8

class Rectangular {
public:
    Rectangular();

    EXPORT void /* implies thiscall on Windows x86 */
    setLength(int length);

    EXPORT void FASTCALL
    computeWeight(int width, int height, int density);

    EXPORT void /* implies thiscall on Windows x86 */
    computeDisplacement(int xContact, int yContact, int zContact, int contactVelocity,
                        int contactWeight, int surfaceViscosity, int xSurfaceAngle,
                        int ySurfaceAngle, int zSurfaceAngle);

private:
    int length;
};

Rectangular::Rectangular()
{
    this->length = DEFAULT_LENGTH;
}

EXPORT void
Rectangular::setLength(int length)
{
    print("Changing length from %d", this->length);
    this->length = length;
    print(" to %d\n", this->length);
}

EXPORT void FASTCALL
Rectangular::computeWeight(int width, int height, int density)
{
    int volume = this->length * width * height;
    int weight = volume * density;

    print("Computed weight %d for volume %d\n", weight, volume);
}

EXPORT void
Rectangular::computeDisplacement(int xContact, int yContact, int zContact,
                                 int contactVelocity, int contactWeight,
                                 int surfaceViscosity, int xSurfaceAngle,
                                 int ySurfaceAngle, int zSurfaceAngle)
{
    print("Calculate displacement for contact at [%d, %d, %d] with velocity %d "
          "and weight %d on a surface having viscosity %d and angle [%d, %d, %d]\n",
          xContact, yContact, zContact, contactVelocity, contactWeight, surfaceViscosity,
          xSurfaceAngle, ySurfaceAngle, zSurfaceAngle);
}

int
main()
{
    Rectangular r;
    r.computeWeight(3, 2, 10);
    r.setLength(7);
    r.computeWeight(3, 2, 10);

    r.computeDisplacement(1, 2, 3, 4, 5, 6, 7, 8, 9);
    r.computeDisplacement(1, 2, 3, 4, 5, 6, 7, 8, 9);

    return 0;
}
