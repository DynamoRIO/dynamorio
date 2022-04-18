/* More subroutines needed by GCC output code on some machines.  */
/* Compile this one with gcc.  */
/* Copyright (C) 1989-2014 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

/* This is extracted from gcc's libgcc/libgcc2.c with these typedefs added: */
typedef short Wtype;
typedef int DWtype;
typedef unsigned int UWtype;
typedef unsigned long long UDWtype;
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
struct DWstruct {Wtype high, low;};
#else
struct DWstruct {Wtype low, high;};
#endif
typedef union {
  struct DWstruct s;
  DWtype ll;
} DWunion;

UDWtype
__udivmoddi4 (UDWtype n, UDWtype d, UDWtype *rp)
{
  UDWtype q = 0, r = n, y = d;
  UWtype lz1, lz2, i, k;

  /* Implements align divisor shift dividend method. This algorithm
     aligns the divisor under the dividend and then perform number of
     test-subtract iterations which shift the dividend left. Number of
     iterations is k + 1 where k is the number of bit positions the
     divisor must be shifted left  to align it under the dividend.
     quotient bits can be saved in the rightmost positions of the dividend
     as it shifts left on each test-subtract iteration. */

  if (y <= r)
    {
      lz1 = __builtin_clzll (d);
      lz2 = __builtin_clzll (n);

      k = lz1 - lz2;
      y = (y << k);

      /* Dividend can exceed 2 ^ (width − 1) − 1 but still be less than the
         aligned divisor. Normal iteration can drops the high order bit
         of the dividend. Therefore, first test-subtract iteration is a
         special case, saving its quotient bit in a separate location and
         not shifting the dividend. */
      if (r >= y)
        {
          r = r - y;
          q =  (1ULL << k);
        }

      if (k > 0)
        {
          y = y >> 1;

          /* k additional iterations where k regular test subtract shift
            dividend iterations are done.  */
          i = k;
          do
            {
              if (r >= y)
                r = ((r - y) << 1) + 1;
              else
                r =  (r << 1);
              i = i - 1;
            } while (i != 0);

          /* First quotient bit is combined with the quotient bits resulting
             from the k regular iterations.  */
          q = q + r;
          r = r >> k;
          q = q - (r << k);
        }
    }

  if (rp)
    *rp = r;
  return q;
}

DWtype
__moddi3 (DWtype u, DWtype v)
{
  Wtype c = 0;
  DWunion uu = {.ll = u};
  DWunion vv = {.ll = v};
  DWtype w;

  if (uu.s.high < 0)
    c = ~c,
    uu.ll = -uu.ll;
  if (vv.s.high < 0)
    vv.ll = -vv.ll;

  (void) __udivmoddi4 (uu.ll, vv.ll, (UDWtype*)&w);
  if (c)
    w = -w;

  return w;
}
