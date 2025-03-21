/* **********************************************************
 * Copyright (c) 2011-2018 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Need this defined and to the latest to get the latest defines and types */
#define _WIN32_WINNT 0x0601 /* == _WIN32_WINNT_WIN7 */
#define WINVER _WIN32_WINNT

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_os.h"
#include "drsyscall_windows.h"
#include "table_defines.h"

#include <wingdi.h> /* usually from windows.h; required by winddi.h + ENUMLOGFONTEXDVW */
#define NT_BUILD_ENVIRONMENT 1 /* for d3dnthal.h */
#include <d3dnthal.h>
#include <winddi.h> /* required by ntgdityp.h and prntfont.h */
#include <prntfont.h>
#include "../wininc/ntgdityp.h"
#include <ntgdi.h>
#include <winspool.h> /* for DRIVER_INFO_2W */
#include <dxgiformat.h> /* for DXGI_FORMAT */

/***************************************************************************/
/* System calls with wrappers in gdi32.dll.
 * Not all wrappers are exported: xref i#388.
 *
 * When adding new entries, use the NtGdi prefix.
 * When we try to find the wrapper via symbol lookup we try with
 * and without the prefix.
 *
 * Initially obtained via mksystable.pl on VS2008 ntgdi.h.
 * That version was checked in separately to track manual changes.
 *
 * FIXME i#485: issues with table that are not yet resolved:
 *
 * + OUT params with no size where size comes from prior syscall
 *   return value (see FIXMEs in table below): so have to watch pairs
 *   of calls (but what if app is able to compute max size some other
 *   way, maybe caching older call?), unless willing to only check for
 *   unaddr in post-syscall and thus after potential write to
 *   unaddressable memory by kernel (which is what we do today).
 *   Update: there are some of these in NtUser table as well.
 *
 * + missing ", return" annotations: NtGdiExtGetObjectW was missing one,
 *   and I'm afraid other ones that return int or UINT may also.
 *
 * + __out PVOID: for NtGdiGetUFIPathname and NtGdiDxgGenericThunk,
 *   is the PVOID that's written supposed to have a bcount (or ecount)
 *   annotation?  for now treated as PVOID*.
 *
 * + bcount in, ecount out for NtGdiSfmGetNotificationTokens (which is
 *   missing annotations)?  but what is size of token?
 *
 * + the REALIZATION_INFO struct is much larger on win7
 */

/* FIXME i#1089: fill in info on all the inlined args for all of
 * syscalls in this file.
 */

/* FIXME i#1093: figure out the failure codes for all the int and uint return values */

extern drsys_sysnum_t sysnum_GdiCreatePaletteInternal;
extern drsys_sysnum_t sysnum_GdiCheckBitmapBits;
extern drsys_sysnum_t sysnum_GdiHfontCreate;
extern drsys_sysnum_t sysnum_GdiDoPalette;
extern drsys_sysnum_t sysnum_GdiExtTextOutW;
extern drsys_sysnum_t sysnum_GdiDescribePixelFormat;
extern drsys_sysnum_t sysnum_GdiGetRasterizerCaps;
extern drsys_sysnum_t sysnum_GdiPolyPolyDraw;

syscall_info_t syscall_gdi32_info[] = {
    {{0,0},"NtGdiInit", OK, SYSARG_TYPE_BOOL32, 0, },
    {{0,0},"NtGdiSetDIBitsToDeviceInternal", OK, SYSARG_TYPE_SINT32, 16,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, -12, R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {10, sizeof(BITMAPINFO), R|CT, SYSARG_TYPE_BITMAPINFO},
         {11, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {12, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {13, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {14, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {15, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetFontResourceInfoInternalW", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, -1, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, -3, W|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetGlyphIndicesW", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(WORD)},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetGlyphIndicesWInternal", OK, SYSARG_TYPE_UINT32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(WORD), W|HT, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiCreatePaletteInternal", OK, DRSYS_TYPE_HANDLE, 2,
     {
         /*too complex: special-cased*/
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_GdiCreatePaletteInternal
    },
    {{0,0},"NtGdiArcInternal", OK, SYSARG_TYPE_BOOL32, 10,
     {
         {0, sizeof(ARCTYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {9, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetOutlineTextMetricsInternalW", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(TMDIFF), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetAndSetDCDword", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetDCObject", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetDCforBitmap", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetMonitorID", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_CWARRAY},
     }
    },
    {{0,0},"NtGdiGetLinkedUFIs", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(UNIVERSAL_FONT_ID)},
         {2, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetLinkedUFIs", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(UNIVERSAL_FONT_ID)},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetUFI", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNIVERSAL_FONT_ID), W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(DESIGNVECTOR), W|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(FLONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiForceUFIMapping", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNIVERSAL_FONT_ID), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetUFIPathname", OK, SYSARG_TYPE_BOOL32, 10,
     {
         {0, sizeof(UNIVERSAL_FONT_ID), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, MAX_PATH * 3, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {2, -1, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {3, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(BOOL), W|HT, DRSYS_TYPE_BOOL},
         {6, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(PVOID), W|HT, DRSYS_TYPE_POINTER},
         {8, sizeof(BOOL), W|HT, DRSYS_TYPE_BOOL},
         {9, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiAddRemoteFontToDC", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UNIVERSAL_FONT_ID), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiAddFontMemResourceEx", OK, DRSYS_TYPE_HANDLE, 5,
     {
         {0, -1, R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiRemoveFontMemResourceEx", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiUnmapMemFont", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtGdiRemoveMergeFont", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNIVERSAL_FONT_ID), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiAnyLinkedFonts", OK, SYSARG_TYPE_BOOL32, 0, },
    {{0,0},"NtGdiGetEmbUFI", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNIVERSAL_FONT_ID), W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(DESIGNVECTOR), W|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(FLONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(KERNEL_PVOID), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetEmbedFonts", OK, SYSARG_TYPE_UINT32, 0, },
    {{0,0},"NtGdiChangeGhostFont", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(KERNEL_PVOID), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiAddEmbFontToDC", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), R|HT, DRSYS_TYPE_POINTER},
     }
    },
    {{0,0},"NtGdiFontIsLinked", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    /* Return value is really either BOOL or HRGN: dynamic iterator gets it right,
     * and we document the limitations of the static iterators.
     */
    {{0,0},"NtGdiPolyPolyDraw", OK|SYSINFO_RET_ZERO_FAIL|SYSINFO_RET_TYPE_VARIES,
     DRSYS_TYPE_UNSIGNED_INT, 5,
     {
         /* Params 0 and 1 are special-cased as they vary */
         {2, -3, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }, &sysnum_GdiPolyPolyDraw
    },
    {{0,0},"NtGdiDoPalette", OK, SYSARG_TYPE_SINT32, 6,
     {
         {0, sizeof(HPALETTE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(WORD), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(WORD), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         /*special-cased: R or W depending*/
         {3, -2, SYSARG_NON_MEMARG|SYSARG_SIZE_IN_ELEMENTS, sizeof(PALETTEENTRY)},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     },&sysnum_GdiDoPalette
    },
    {{0,0},"NtGdiComputeXformCoefficients", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetWidthTable", OK|SYSINFO_RET_MINUS1_FAIL, SYSARG_TYPE_SINT32, 7,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(WCHAR)},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -3, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(USHORT)},
         {5, sizeof(WIDTHDATA), W|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(FLONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiDescribePixelFormat", OK, SYSARG_TYPE_SINT32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, W|HT, DRSYS_TYPE_STRUCT},
     }, &sysnum_GdiDescribePixelFormat
    },
    {{0,0},"NtGdiSetPixelFormat", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiSwapBuffers", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDxgGenericThunk", OK, SYSARG_TYPE_UINT32, 6,
     {
         {0, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(SIZE_T), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(PVOID), R|W|HT, DRSYS_TYPE_POINTER},
         {4, sizeof(SIZE_T), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(PVOID), R|W|HT, DRSYS_TYPE_POINTER},
     }
    },
    {{0,0},"NtGdiDdAddAttachedSurface", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DD_ADDATTACHEDSURFACEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdAttachSurface", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdBlt", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DD_BLTDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdCanCreateSurface", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_CANCREATESURFACEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdColorControl", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_COLORCONTROLDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdCreateDirectDrawObject", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdCreateSurface", OK, SYSARG_TYPE_UINT32, 8,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), R|HT, DRSYS_TYPE_HANDLE},
         {2, sizeof(DDSURFACEDESC), R|W|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(DD_SURFACE_GLOBAL), R|W|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(DD_SURFACE_LOCAL), R|W|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(DD_SURFACE_MORE), R|W|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(DD_CREATESURFACEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdChangeSurfacePointer", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtGdiDdCreateSurfaceObject", OK, DRSYS_TYPE_HANDLE, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DD_SURFACE_LOCAL), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(DD_SURFACE_MORE), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(DD_SURFACE_GLOBAL), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiDdDeleteSurfaceObject", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdDeleteDirectDrawObject", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdDestroySurface", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiDdFlip", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(DD_FLIPDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetAvailDriverMemory", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETAVAILDRIVERMEMORYDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetBltStatus", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETBLTSTATUSDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetDC", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PALETTEENTRY), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetDriverInfo", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETDRIVERINFODATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetFlipStatus", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETFLIPSTATUSDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetScanLine", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETSCANLINEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdSetExclusiveMode", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_SETEXCLUSIVEMODEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdFlipToGDISurface", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_FLIPTOGDISURFACEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdLock", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_LOCKDATA), R|W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdQueryDirectDrawObject", OK, SYSARG_TYPE_BOOL32, 11,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_HALINFO), W|HT, DRSYS_TYPE_STRUCT},
         {2, 3, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(DWORD)},
         {3, sizeof(D3DNTHAL_CALLBACKS), W|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(D3DNTHAL_GLOBALDRIVERDATA), W|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(DD_D3DBUFCALLBACKS), W|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(DDSURFACEDESC), W|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(VIDEOMEMORY), W|HT, DRSYS_TYPE_STRUCT},
         {9, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {10, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiDdReenableDirectDrawObject", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), R|W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiDdReleaseDC", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdResetVisrgn", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdSetColorKey", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_SETCOLORKEYDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdSetOverlayPosition", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DD_SETOVERLAYPOSITIONDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdUnattachSurface", OK, DRSYS_TYPE_VOID, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdUnlock", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_UNLOCKDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdUpdateOverlay", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DD_UPDATEOVERLAYDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdWaitForVerticalBlank", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_WAITFORVERTICALBLANKDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetDxHandle", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiDdSetGammaRamp", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DDGAMMARAMP), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdLockD3D", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_LOCKDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdUnlockD3D", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_UNLOCKDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdCreateD3DBuffer", OK, SYSARG_TYPE_UINT32, 8,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), R|W|HT, DRSYS_TYPE_HANDLE},
         {2, sizeof(DDSURFACEDESC), R|W|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(DD_SURFACE_GLOBAL), R|W|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(DD_SURFACE_LOCAL), R|W|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(DD_SURFACE_MORE), R|W|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(DD_CREATESURFACEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(HANDLE), R|W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdCanCreateD3DBuffer", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_CANCREATESURFACEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdDestroyD3DBuffer", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiD3dContextCreate", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(D3DNTHAL_CONTEXTCREATEI), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiD3dContextDestroy", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(D3DNTHAL_CONTEXTDESTROYDATA), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiD3dContextDestroyAll", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(D3DNTHAL_CONTEXTDESTROYALLDATA), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiD3dValidateTextureStageState", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(D3DNTHAL_VALIDATETEXTURESTAGESTATEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiD3dDrawPrimitives2", OK, SYSARG_TYPE_UINT32, 7,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(D3DNTHAL_DRAWPRIMITIVES2DATA), R|W|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(FLATPTR), R|W|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(DWORD), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(FLATPTR), R|W|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(DWORD), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiDdGetDriverState", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DD_GETDRIVERSTATEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdCreateSurfaceEx", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiDvpCanCreateVideoPort", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_CANCREATEVPORTDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpColorControl", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_VPORTCOLORDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpCreateVideoPort", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_CREATEVPORTDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpDestroyVideoPort", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_DESTROYVPORTDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpFlipVideoPort", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(DD_FLIPVPORTDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpGetVideoPortBandwidth", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETVPORTBANDWIDTHDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpGetVideoPortField", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETVPORTFIELDDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpGetVideoPortFlipStatus", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETVPORTFLIPSTATUSDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpGetVideoPortInputFormats", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETVPORTINPUTFORMATDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpGetVideoPortLine", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETVPORTLINEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpGetVideoPortOutputFormats", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETVPORTOUTPUTFORMATDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpGetVideoPortConnectInfo", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETVPORTCONNECTDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpGetVideoSignalStatus", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETVPORTSIGNALDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpUpdateVideoPort", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), R|HT, DRSYS_TYPE_HANDLE},
         {2, sizeof(HANDLE), R|HT, DRSYS_TYPE_HANDLE},
         {3, sizeof(DD_UPDATEVPORTDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpWaitForVideoPortSync", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_WAITFORVPORTSYNCDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpAcquireNotification", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), R|W|HT, DRSYS_TYPE_HANDLE},
         {2, sizeof(DDVIDEOPORTNOTIFY), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDvpReleaseNotification", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdGetMoCompGuids", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETMOCOMPGUIDSDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetMoCompFormats", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETMOCOMPFORMATSDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetMoCompBuffInfo", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETMOCOMPCOMPBUFFDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdGetInternalMoCompInfo", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_GETINTERNALMOCOMPDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdCreateMoComp", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_CREATEMOCOMPDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdDestroyMoComp", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_DESTROYMOCOMPDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdBeginMoCompFrame", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_BEGINMOCOMPFRAMEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdEndMoCompFrame", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_ENDMOCOMPFRAMEDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdRenderMoComp", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_RENDERMOCOMPDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdQueryMoCompStatus", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DD_QUERYMOCOMPSTATUSDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDdAlphaBlt", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DD_BLTDATA), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiAlphaBlend", OK, SYSARG_TYPE_BOOL32, 12,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {9, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {10, sizeof(BLENDFUNCTION), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
         {11, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGradientFill", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(TRIVERTEX)},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetIcmMode", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiCreateColorSpace", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(LOGCOLORSPACEEXW), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDeleteColorSpace", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSetColorSpace", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HCOLORSPACE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiCreateColorTransform", OK, DRSYS_TYPE_HANDLE, 8,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LOGCOLORSPACEW), R|HT, DRSYS_TYPE_STRUCT},
         {2, -3, R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -5, R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, -7, R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiDeleteColorTransform", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiCheckBitmapBits", OK, SYSARG_TYPE_BOOL32, 8,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         /*too complex: special-cased*/
     },&sysnum_GdiCheckBitmapBits
    },
    {{0,0},"NtGdiColorCorrectPalette", OK, SYSARG_TYPE_UINT32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HPALETTE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -3, R|W|SYSARG_SIZE_IN_ELEMENTS, sizeof(PALETTEENTRY)},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetColorSpaceforBitmap", OK, DRSYS_TYPE_UNSIGNED_INT, 1,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetDeviceGammaRamp", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DDGAMMARAMP), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiSetDeviceGammaRamp", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DDGAMMARAMP), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiIcmBrushInfo", OK, SYSARG_TYPE_BOOL32, 8,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BITMAPINFO) + ((/*MAX_COLORTABLE*/256 - 1) * sizeof(RGBQUAD)), R|W|HT, DRSYS_TYPE_BITMAPINFO},
         {3, -4, R|SYSARG_LENGTH_INOUT|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(BOOL), W|HT, DRSYS_TYPE_BOOL},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiFlush", OK, DRSYS_TYPE_VOID, 0, },
    {{0,0},"NtGdiCreateMetafileDC", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiMakeInfoDC", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiCreateClientObj", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiDeleteClientObj", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetBitmapBits", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiDeleteObjectApp", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetPath", OK, SYSARG_TYPE_SINT32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -3, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(POINT)},
         {2, -3, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(BYTE)},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiCreateCompatibleDC", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiCreateDIBitmapInternal", OK, DRSYS_TYPE_HANDLE, 11,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -8, R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, -7, R|HT, DRSYS_TYPE_BITMAPINFO},
         {6, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {10, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiCreateDIBSection", OK, DRSYS_TYPE_HANDLE, 9,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -5, R|HT, DRSYS_TYPE_BITMAPINFO},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(PVOID), W|HT, DRSYS_TYPE_POINTER},
     }
    },
    {{0,0},"NtGdiCreateSolidBrush", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiCreateDIBBrush", OK, DRSYS_TYPE_HANDLE, 6,
     {
         {0, -2, R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {4, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {5, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtGdiCreatePatternBrushInternal", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiCreateHatchBrushInternal", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiExtCreatePen", OK, DRSYS_TYPE_HANDLE, 11,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, -6, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {10, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiCreateEllipticRgn", OK, DRSYS_TYPE_HANDLE, 4,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiCreateRoundRectRgn", OK, DRSYS_TYPE_HANDLE, 6,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiCreateServerMetaFile", OK, DRSYS_TYPE_HANDLE, 6,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiExtCreateRegion", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(XFORM), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiMakeFontDir", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -2, W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(unsigned), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, R|HT, DRSYS_TYPE_CWARRAY},
         {4, sizeof(unsigned), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiPolyDraw", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -3, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(POINT)},
         {2, -3, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(BYTE)},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiPolyTextOutW", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(POLYTEXTW)},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetServerMetaFileBits", OK, SYSARG_TYPE_UINT32, 7,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEqualRgn", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetBitmapDimension", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(SIZE), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetNearestPaletteIndex", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HPALETTE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiPtVisible", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiRectVisible", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiRemoveFontResourceW", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, -1, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(WCHAR)},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DESIGNVECTOR), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiResizePalette", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HPALETTE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetBitmapDimension", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(SIZE), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiOffsetClipRgn", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetMetaRgn", OK, SYSARG_TYPE_SINT32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSetTextJustification", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetAppClipBox", OK, SYSARG_TYPE_SINT32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetTextExtentExW", OK, SYSARG_TYPE_BOOL32, 8,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
         {5, -4, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
         {6, sizeof(SIZE), W|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetCharABCWidthsW", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(WCHAR)},
         {4, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(ABC)},
     }
    },
    {{0,0},"NtGdiGetCharacterPlacementW", OK, SYSARG_TYPE_UINT32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(GCP_RESULTSW), R|W|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiAngleArc", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiBeginPath", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSelectClipPath", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiCloseFigure", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiEndPath", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiAbortPath", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiFillPath", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiStrokeAndFillPath", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiStrokePath", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiWidenPath", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiFlattenPath", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiPathToRegion", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSetMiterLimit", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetFontXform", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetMiterLimit", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEllipse", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiRectangle", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiRoundRect", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiPlgBlt", OK, SYSARG_TYPE_BOOL32, 11,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, 3, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(POINT)},
         {2, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {8, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {9, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {10, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiMaskBlt", OK, SYSARG_TYPE_BOOL32, 13,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {9, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {10, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {11, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {12, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiExtFloodFill", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiFillRgn", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiFrameRgn", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetPixel", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetPixel", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiStartPage", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiEndPage", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiStartDoc", OK, SYSARG_TYPE_SINT32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DOCINFOW), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(BOOL), W|HT, DRSYS_TYPE_BOOL},
         {3, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiEndDoc", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiAbortDoc", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiUpdateColors", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetCharWidthW", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(WCHAR)},
         {4, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
     }
    },
    {{0,0},"NtGdiGetCharWidthInfo", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(CHWIDTHINFO), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiDrawEscape", OK, SYSARG_TYPE_SINT32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, -2, R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiExtEscape", OK, SYSARG_TYPE_SINT32, 8,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(WCHAR)},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, -4, R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, -6, W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetFontData", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, W|HT, DRSYS_TYPE_STRUCT},
         {3, RET, W, },
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetFontFileData", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONGLONG), R|HT, DRSYS_TYPE_STRUCT},
         {3, -4, W|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(SIZE_T), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetFontFileInfo", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, W|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(SIZE_T), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(SIZE_T), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetGlyphOutline", OK, SYSARG_TYPE_UINT32, 8,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(WCHAR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(GLYPHMETRICS), W|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, -4, W|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(MAT2), R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiGetETM", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(EXTTEXTMETRIC), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetRasterizerCaps", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, -1, W|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_GdiGetRasterizerCaps
    },
    {{0,0},"NtGdiGetKerningPairs", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(KERNINGPAIR)},
         {2, RET, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(KERNINGPAIR)},
     }
    },
    {{0,0},"NtGdiMonoBitmap", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetObjectBitmapHandle", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEnumObjects", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiResetDC", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DEVMODEW)/*really var-len*/, R|CT, SYSARG_TYPE_DEVMODEW},
         {2, sizeof(BOOL), W|HT, DRSYS_TYPE_BOOL},
         {3, sizeof(DRIVER_INFO_2W), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(PUMDHPDEV *), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiSetBoundsRect", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetColorAdjustment", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(COLORADJUSTMENT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiSetColorAdjustment", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(COLORADJUSTMENT), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiCancelDC", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,WIN2K3},"NtGdiOpenDCW", OK, DRSYS_TYPE_HANDLE, 7,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(DEVMODEW)/*really var-len*/, R|CT, SYSARG_TYPE_DEVMODEW},
         {2, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {5, sizeof(DRIVER_INFO_2W), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(PUMDHPDEV *), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{WINVISTA,0},"NtGdiOpenDCW", OK, DRSYS_TYPE_HANDLE, 8,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(DEVMODEW)/*really var-len*/, R|CT, SYSARG_TYPE_DEVMODEW},
         {2, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {5, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(DRIVER_INFO_2W), R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(PUMDHPDEV *), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetDCDword", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetDCPoint", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(POINTL), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiScaleViewportExtEx", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(SIZE), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiScaleWindowExtEx", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(SIZE), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiSetVirtualResolution", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetSizeDevice", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetTransform", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(XFORM), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiModifyWorldTransform", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(XFORM), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiCombineTransform", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(XFORM), W|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(XFORM), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(XFORM), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiTransformPoints", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -3, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(POINT)},
         {2, -3, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(POINT)},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiConvertMetafileRect", OK, SYSARG_TYPE_SINT32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECTL), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetTextCharsetInfo", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(FONTSIGNATURE), W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiDoBanding", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(POINTL), W|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(SIZE), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetPerBandInfo", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PERBANDINFO), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetStats", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, -4, W|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetMagicColors", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PALETTEENTRY), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiSelectBrush", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSelectPen", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HPEN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSelectBitmap", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSelectFont", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HFONT), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiExtSelectClipRgn", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiCreatePen", OK, DRSYS_TYPE_HANDLE, 4,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiBitBlt", OK, SYSARG_TYPE_BOOL32, 11,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {10, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiTileBitBlt", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiTransparentBlt", OK, SYSARG_TYPE_BOOL32, 11,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {9, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {10, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetTextExtent", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(SIZE), W|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetTextMetricsW", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetTextFaceW", OK, SYSARG_TYPE_SINT32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -1, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {2, RET, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {3, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiGetRandomRgn", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiExtTextOutW", OK, SYSARG_TYPE_BOOL32, 9,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {5, -6, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, -6, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(INT)/*can be larger: special-cased*/},
         {8, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_GdiExtTextOutW
    },
    {{0,0},"NtGdiIntersectClipRect", OK, SYSARG_TYPE_SINT32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiCreateRectRgn", OK, DRSYS_TYPE_HANDLE, 4,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiPatBlt", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiPolyPatBlt", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(POLYPATBLT)},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiUnrealizeObject", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetStockObject", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiCreateCompatibleBitmap", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiCreateBitmapFromDxSurface", OK, DRSYS_TYPE_HANDLE, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiBeginGdiRendering", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiEndGdiRendering", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(BOOL), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiLineTo", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiMoveTo", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(POINT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiExtGetObjectW", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_STRUCT},
         {2, RET, W,},
     }
    },
    {{0,0},"NtGdiGetDeviceCaps", OK, SYSARG_TYPE_SINT32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetDeviceCapsAll", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DEVCAPS), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiStretchBlt", OK, SYSARG_TYPE_BOOL32, 12,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {9, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {10, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {11, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetBrushOrg", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(POINT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiCreateBitmap", OK, DRSYS_TYPE_HANDLE, 5,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(BYTE), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiCreateHalftonePalette", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiRestoreDC", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiExcludeClipRect", OK, SYSARG_TYPE_SINT32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiSaveDC", OK, SYSARG_TYPE_SINT32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiCombineRgn", OK, SYSARG_TYPE_SINT32, 4,
     {
         {0, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetRectRgn", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetBitmapBits", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetDIBitsInternal", OK, SYSARG_TYPE_SINT32, 9,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -7, W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(BITMAPINFO), R|W|CT, SYSARG_TYPE_BITMAPINFO},
         {6, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiOffsetRgn", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetRgnBox", OK, SYSARG_TYPE_SINT32, 2,
     {
         {0, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiRectInRegion", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetBoundsRect", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiPtInRegion", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetNearestColor", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetSystemPaletteUse", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSetSystemPaletteUse", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetRegionData", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_STRUCT},
         {2, RET, W,},
     }
    },
    {{0,0},"NtGdiInvertRgn", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,WINNT},"NtGdiHfontCreate", OK, DRSYS_TYPE_HANDLE, 5,
     {
         {0, sizeof(EXTLOGFONTW), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(LFTYPE), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{WIN2K,0},"NtGdiHfontCreate", OK, DRSYS_TYPE_HANDLE, 5,
     {
         /*special-cased*/
         {0, -1, SYSARG_NON_MEMARG|SYSARG_SIZE_IN_ELEMENTS, sizeof(ENUMLOGFONTEXDVW)},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(LFTYPE), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     },&sysnum_GdiHfontCreate
    },
    {{0,0},"NtGdiSetFontEnumeration", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEnumFonts", OK, SYSARG_TYPE_BOOL32, 8,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -3, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), R|W|SYSARG_IGNORE_IF_NEXT_NULL|HT, DRSYS_TYPE_UNSIGNED_INT},
         {7, -6, WI|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiQueryFonts", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, -1, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(UNIVERSAL_FONT_ID)},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(LARGE_INTEGER), W|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtGdiGetCharSet", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiEnableEudc", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiEudcLoadUnloadLink", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, -1, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiGetStringBitmapW", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(wchar_t), R,},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -3, W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiGetEudcTimeStampEx", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, -1, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiQueryFontAssocInfo", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetFontUnicodeRanges", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, RET, W,/*FIXME i#485: pre size from prior syscall ret*/},
     }
    },
    /* FIXME i#485: the REALIZATION_INFO struct is much larger on win7 */
    {{0,0},"NtGdiGetRealizationInfo", UNKNOWN, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(REALIZATION_INFO), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiAddRemoteMMInstanceToDC", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiUnloadPrinterDriver", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, -1, R,},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngAssociateSurface", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HSURF), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HDEV), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngEraseSurface", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngCreateBitmap", OK, DRSYS_TYPE_HANDLE, 5,
     {
         {0, sizeof(SIZEL), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtGdiEngDeleteSurface", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HSURF), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiEngLockSurface", OK, DRSYS_TYPE_POINTER, 1,
     {
         {0, sizeof(HSURF), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiEngUnlockSurface", OK, DRSYS_TYPE_VOID, 1,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiEngMarkBandingSurface", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HSURF), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiEngCreateDeviceSurface", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(DHSURF), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(SIZEL), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngCreateDeviceBitmap", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(DHSURF), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(SIZEL), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngCopyBits", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiEngStretchBlt", OK, SYSARG_TYPE_BOOL32, 11,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(COLORADJUSTMENT), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {9, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {10, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngBitBlt", OK, SYSARG_TYPE_BOOL32, 11,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {9, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {10, sizeof(ROP4), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngPlgBlt", OK, SYSARG_TYPE_BOOL32, 11,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(COLORADJUSTMENT), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(POINTFIX), R|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {9, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {10, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngCreatePalette", OK, DRSYS_TYPE_HANDLE, 6,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngDeletePalette", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HPALETTE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiEngStrokePath", OK, SYSARG_TYPE_BOOL32, 8,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(PATHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(XFORMOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(LINEATTRS), R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(MIX), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngFillPath", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(PATHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(MIX), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngStrokeAndFillPath", OK, SYSARG_TYPE_BOOL32, 10,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(PATHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(XFORMOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(LINEATTRS), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(MIX), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngPaint", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(MIX), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngLineTo", OK, SYSARG_TYPE_BOOL32, 9,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(MIX), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngAlphaBlend", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(BLENDOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiEngGradientFill", OK, SYSARG_TYPE_BOOL32, 10,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, -4, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(TRIVERTEX)},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, -6, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(PVOID)},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {9, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngTransparentBlt", OK, SYSARG_TYPE_BOOL32, 8,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngTextOut", OK, SYSARG_TYPE_BOOL32, 10,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(STROBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(FONTOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {9, sizeof(MIX), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiEngStretchBltROP", OK, SYSARG_TYPE_BOOL32, 13,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(COLORADJUSTMENT), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(RECTL), R|HT, DRSYS_TYPE_STRUCT},
         {9, sizeof(POINTL), R|HT, DRSYS_TYPE_STRUCT},
         {10, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {11, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {12, sizeof(ROP4), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiXLATEOBJ_cGetPalette", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
     }
    },
    {{0,0},"NtGdiCLIPOBJ_cEnumStart", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiCLIPOBJ_bEnum", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiCLIPOBJ_ppoGetPath", OK, DRSYS_TYPE_POINTER, 1,
     {
         {0, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiEngCreateClip", OK, DRSYS_TYPE_POINTER, 0, },
    {{0,0},"NtGdiEngDeleteClip", OK, DRSYS_TYPE_VOID, 1,
     {
         {0, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiBRUSHOBJ_pvAllocRbrush", OK, DRSYS_TYPE_POINTER, 2,
     {
         {0, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiBRUSHOBJ_pvGetRbrush", OK, DRSYS_TYPE_POINTER, 1,
     {
         {0, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiBRUSHOBJ_ulGetBrushColor", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiBRUSHOBJ_hGetColorTransform", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiXFORMOBJ_bApplyXform", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(XFORMOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(POINTL)},
         {4, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(POINTL)},
     }
    },
    {{0,0},"NtGdiXFORMOBJ_iGetXform", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(XFORMOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(XFORML), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiFONTOBJ_vGetInfo", OK, DRSYS_TYPE_VOID, 3,
     {
         {0, sizeof(FONTOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiFONTOBJ_cGetGlyphs", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(FONTOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(HGLYPH), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(GLYPHDATA **), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiFONTOBJ_pxoGetXform", OK, DRSYS_TYPE_POINTER, 1,
     {
         {0, sizeof(FONTOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiFONTOBJ_pifi", OK, DRSYS_TYPE_POINTER, 1,
     {
         {0, sizeof(FONTOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiFONTOBJ_pfdg", OK, DRSYS_TYPE_POINTER, 1,
     {
         {0, sizeof(FONTOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiFONTOBJ_cGetAllGlyphHandles", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(FONTOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, RET, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(HGLYPH)/*FIXME i#485: pre size from prior syscall ret*/},
     }
    },
    {{0,0},"NtGdiFONTOBJ_pvTrueTypeFontFile", OK, DRSYS_TYPE_POINTER, 2,
     {
         {0, sizeof(FONTOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiFONTOBJ_pQueryGlyphAttrs", OK, DRSYS_TYPE_POINTER, 2,
     {
         {0, sizeof(FONTOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiSTROBJ_bEnum", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(STROBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},/*XXX: I'm assuming R: else how know? prior syscall (i#485)?*/
         {2, -1, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(PGLYPHPOS)},
     }
    },
    {{0,0},"NtGdiSTROBJ_bEnumPositionsOnly", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(STROBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},/*XXX: I'm assuming R: else how know? prior syscall (i#485)?*/
         {2, -1, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(PGLYPHPOS)},
     }
    },
    {{0,0},"NtGdiSTROBJ_vEnumStart", OK, DRSYS_TYPE_VOID, 1,
     {
         {0, sizeof(STROBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiSTROBJ_dwGetCodePage", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(STROBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiSTROBJ_bGetAdvanceWidths", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(STROBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(POINTQF)},
     }
    },
    {{0,0},"NtGdiEngComputeGlyphSet", OK, DRSYS_TYPE_POINTER, 3,
     {
         {0, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiXLATEOBJ_iXlate", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiXLATEOBJ_hGetColorTransform", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(XLATEOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiPATHOBJ_vGetBounds", OK, DRSYS_TYPE_VOID, 2,
     {
         {0, sizeof(PATHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(RECTFX), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiPATHOBJ_bEnum", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(PATHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(PATHDATA), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiPATHOBJ_vEnumStart", OK, DRSYS_TYPE_VOID, 1,
     {
         {0, sizeof(PATHOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiEngDeletePath", OK, DRSYS_TYPE_VOID, 1,
     {
         {0, sizeof(PATHOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiPATHOBJ_vEnumStartClipLines", OK, DRSYS_TYPE_VOID, 4,
     {
         {0, sizeof(PATHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(CLIPOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(LINEATTRS), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiPATHOBJ_bEnumClipLines", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(PATHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiEngCheckAbort", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(SURFOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiGetDhpdev", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HDEV), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiHT_Get8BPPFormatPalette", OK, SYSARG_TYPE_SINT32, 4,
     {
         {0, RET, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(PALETTEENTRY)/*FIXME i#485: pre size from prior syscall ret*/},
         {1, sizeof(USHORT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(USHORT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(USHORT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiHT_Get8BPPMaskPalette", OK, SYSARG_TYPE_SINT32, 6,
     {
         {0, RET, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(PALETTEENTRY)/*FIXME i#485: pre size from prior syscall ret*/},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(BYTE), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(USHORT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(USHORT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(USHORT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiUpdateTransform", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSetLayout", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiMirrorWindowOrg", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiGetDeviceWidth", OK, SYSARG_TYPE_SINT32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSetPUMPDOBJ", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HUMPD), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(HUMPD), R|W|HT, DRSYS_TYPE_HANDLE},
         {3, sizeof(BOOL), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiBRUSHOBJ_DeleteRbrush", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(BRUSHOBJ), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiUMPDEngFreeUserMem", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(KERNEL_PVOID), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiSetBitmapAttributes", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiClearBitmapAttributes", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiSetBrushAttributes", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiClearBrushAttributes", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiDrawStream", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiMakeObjectXferable", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGdiMakeObjectUnXferable", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiSfmGetNotificationTokens", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UINT), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, -0, W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiSfmRegisterLogicalSurfaceForSignaling", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HLSURF), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtGdiDwmGetHighColorMode", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(DXGI_FORMAT), W|HT, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiDwmSetHighColorMode", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(DXGI_FORMAT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiDwmCaptureScreen", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(DXGI_FORMAT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtGdiDdCreateFullscreenSprite", OK, RNTST, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {3, sizeof(HDC), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdNotifyFullscreenSpriteUpdate", OK, RNTST, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdDestroyFullscreenSprite", OK, RNTST, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGdiDdQueryVisRgnUniqueness", OK, SYSARG_TYPE_UINT32, 0, },

    /***************************************************/
    /* FIXME i#1095: fill in the unknown info, esp Vista+ */
    {{0,0},"NtGdiAddFontResourceW", OK, SYSARG_TYPE_SINT32, 6,
     {
         {0, -1, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(WCHAR)},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DESIGNVECTOR), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGdiConsoleTextOut", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiEnumFontChunk", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiEnumFontClose", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiEnumFontOpen", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiFullscreenControl", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetSpoolMessage", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiInitSpool", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiSetupPublicCFONT", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiStretchDIBitsInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },

    /***************************************************/
    /* Added in Vista */
    /* XXX: add min OS version: but we have to distinguish the service packs! */
    {{0,0},"NtGdiConfigureOPMProtectedOutput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiCreateOPMProtectedOutputs", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDDCCIGetCapabilitiesString", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDDCCIGetCapabilitiesStringLength", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDDCCIGetTimingReport", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDDCCIGetVCPFeature", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDDCCISaveCurrentSettings", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDDCCISetVCPFeature", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICheckExclusiveOwnership", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICheckMonitorPowerState", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICheckOcclusion", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICloseAdapter", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICreateAllocation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICreateContext", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICreateDCFromMemory", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICreateDevice", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICreateOverlay", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDICreateSynchronizationObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIDestroyAllocation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIDestroyContext", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIDestroyDCFromMemory", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIDestroyDevice", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIDestroyOverlay", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIDestroySynchronizationObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIEscape", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIFlipOverlay", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIGetContextSchedulingPriority", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIGetDeviceState", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIGetDisplayModeList", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIGetMultisampleMethodList", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIGetPresentHistory", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIGetProcessSchedulingPriorityClass", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIGetRuntimeData", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIGetScanLine", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIGetSharedPrimaryHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIInvalidateActiveVidPn", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDILock", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIOpenAdapterFromDeviceName", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIOpenAdapterFromHdc", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIOpenResource", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIPollDisplayChildren", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIPresent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIQueryAdapterInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIQueryAllocationResidency", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIQueryResourceInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIQueryStatistics", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIReleaseProcessVidPnSourceOwners", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIRender", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISetAllocationPriority", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISetContextSchedulingPriority", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISetDisplayMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISetDisplayPrivateDriverFormat", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISetGammaRamp", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISetProcessSchedulingPriorityClass", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISetQueuedLimit", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISetVidPnSourceOwner", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISharedPrimaryLockNotification", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISharedPrimaryUnLockNotification", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDISignalSynchronizationObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIUnlock", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIUpdateOverlay", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIWaitForIdle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIWaitForSynchronizationObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDdDDIWaitForVerticalBlankEvent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDestroyOPMProtectedOutput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDestroyPhysicalMonitor", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDwmGetDirtyRgn", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiDwmGetSurfaceData", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetCOPPCompatibleOPMInformation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetCertificate", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetCertificateSize", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetNumberOfPhysicalMonitors", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetOPMInformation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetOPMRandomNumber", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetPhysicalMonitorDescription", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetPhysicalMonitors", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiGetSuggestedOPMProtectedOutputArraySize", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtGdiSetOPMSigningKeyAndSequenceNumbers", UNKNOWN, DRSYS_TYPE_UNKNOWN, },

    /***************************************************/
    /* Added in Win7 */
    {{WIN7,0},"NtGdiDdDDIAcquireKeyedMutex", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDICheckSharedResourceAccess", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDICheckVidPnExclusiveOwnership", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDIConfigureSharedResource", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDICreateKeyedMutex", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDIDestroyKeyedMutex", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDIGetOverlayState", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDIGetPresentQueueEvent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDIOpenKeyedMutex", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDIOpenSynchronizationObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiDdDDIReleaseKeyedMutex", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiGetCodePage", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtGdiHLSurfGetInformation", UNKNOWN, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HLSURF), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         /* FIXME: what's the info class?
          * {1, sizeof(HLSURF_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
          */
         {2, -3, R|SYSARG_LENGTH_INOUT|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{WIN7,0},"NtGdiHLSurfSetInformation", UNKNOWN, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HLSURF), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         /* FIXME: what's the info class?
          *{1, sizeof(HLSURF_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
          */
         {2, -3, R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },

    /***************************************************/
    /* Added in Win8 */
    /* FIXME i#1153: fill in details */
    {{WIN8,0},"NtGdiCreateBitmapFromDxSurface2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 7, },
    {{WIN8,0},"NtGdiCreateSessionMappedDIBSection", UNKNOWN, DRSYS_TYPE_UNKNOWN, 8, },
    {{WIN8,0},"NtGdiDdDDIAcquireKeyedMutex2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDICreateKeyedMutex2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDICreateOutputDupl", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIDestroyOutputDupl", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIEnumAdapters", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIGetContextInProcessSchedulingPriority", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIGetSharedResourceAdapterLuid", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOfferAllocations", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOpenAdapterFromLuid", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOpenKeyedMutex2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOpenNtHandleFromName", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOpenResourceFromNtHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOpenSyncObjectFromNtHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOutputDuplGetFrameInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOutputDuplGetMetaData", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOutputDuplGetPointerShapeData", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOutputDuplPresent", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIOutputDuplReleaseFrame", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIPinDirectFlipResources", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIQueryResourceInfoFromNtHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIReclaimAllocations", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIReleaseKeyedMutex2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDISetContextInProcessSchedulingPriority", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDISetStereoEnabled", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDISetVidPnSourceOwner1", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIShareObjects", UNKNOWN, DRSYS_TYPE_UNKNOWN, 5, },
    {{WIN8,0},"NtGdiDdDDIUnpinDirectFlipResources", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDdDDIWaitForVerticalBlankEvent2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtGdiDwmCreatedBitmapRemotingOutput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,0},"NtGdiSetUMPDSandboxState", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },

    /***************************************************/
    /* Added in Windows 8.1 */
    /* FIXME i#1360: fill in details */
    {{WIN81,0},"NtGdiDdDDICacheHybridQueryValue", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtGdiDdDDICheckMultiPlaneOverlaySupport", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtGdiDdDDIGetCachedHybridQueryValue", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtGdiDdDDINetDispGetNextChunkInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, 7, },
    {{WIN81,0},"NtGdiDdDDINetDispQueryMiracastDisplayDeviceStatus", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN81,0},"NtGdiDdDDINetDispQueryMiracastDisplayDeviceSupport", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtGdiDdDDINetDispStartMiracastDisplayDevice", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN81,0},"NtGdiDdDDINetDispStopMiracastDisplayDevice", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN81,0},"NtGdiDdDDIPresentMultiPlaneOverlay", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtGdiGetCurrentDpiInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },

    /***************************************************/
    /* Added in Windows 10 */
    /* FIXME i#1750: fill in details */
    {{WIN10,0},"NtGdiDdDDIAbandonSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIAcquireSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIAdjustFullscreenGamma", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIChangeVideoMemoryReservation", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDICheckMultiPlaneOverlaySupport2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDICreateContextVirtual", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDICreatePagingQueue", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDICreateSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIDestroyAllocation2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIDestroyPagingQueue", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIEnumAdapters2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIEvict", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIFreeGpuVirtualAddress", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIGetDWMVerticalBlankEvent", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIGetResourcePresentPrivateDriverData", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIGetSetSwapChainMetadata", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIInvalidateCache", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDILock2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIMakeResident", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIMapGpuVirtualAddress", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIMarkDeviceAsError", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDINetDispStartMiracastDisplayDeviceEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDINetDispStopSessions", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIOpenSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIOpenSyncObjectFromNtHandle2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIOpenSyncObjectNtHandleFromName", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIPresentMultiPlaneOverlay2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIQueryClockCalibration", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIQueryVidPnExclusiveOwnership", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIQueryVideoMemoryInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIReclaimAllocations2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIReleaseSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIReserveGpuVirtualAddress", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDISetDodIndirectSwapchain", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDISetStablePowerState", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDISetSyncRefreshCountWaitTarget", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDISetVidPnSourceHwProtection", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDISignalSynchronizationObjectFromCpu", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDISignalSynchronizationObjectFromGpu", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDISignalSynchronizationObjectFromGpu2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDISubmitCommand", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIUnlock2", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIUpdateGpuVirtualAddress", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIWaitForSynchronizationObjectFromCpu", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtGdiDdDDIWaitForSynchronizationObjectFromGpu", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    /* Added in Windows 10 1511 */
    /* FIXME i#1750: fill in details */
    {{WIN11,0},"NtGdiDdDDIFlushHeapTransitions", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN11,0},"NtGdiDdDDISetHwProtectionTeardownRecovery", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN11,0},"NtGdiGetCertificateByHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN11,0},"NtGdiGetCertificateSizeByHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    /* Added in Windows 10 1607 */
    /* FIXME i#1750: fill in details */
    {{WIN12,0},"NtGdiCreateOPMProtectedOutput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiDdDDICheckMultiPlaneOverlaySupport3", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiDdDDIPresentMultiPlaneOverlay3", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiDdDDIQueryFSEBlock", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiDdDDIQueryProcessOfferInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiDdDDISetFSEBlock", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiDdDDITrimProcessCommitment", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiDdDDIUpdateAllocationProperty", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiGetEntry", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiGetProcessSessionFonts", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtGdiGetPublicFontTableChangeCookie", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    /* Added in Windows 10 1703 */
    /* FIXME i#1750: fill in details */
    {{WIN13,0},"NtGdiAddInitialFonts", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDICreateHwContext", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDICreateHwQueue", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDIDestroyHwContext", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDIDestroyHwQueue", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDIGetAllocationPriority", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDIGetMemoryBudgetTarget", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDIGetMultiPlaneOverlayCaps", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDIGetPostCompositionCaps", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDIGetProcessSchedulingPriorityBand", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDIGetYieldPercentage", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDISetMemoryBudgetTarget", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDISetProcessSchedulingPriorityBand", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDISetYieldPercentage", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDISubmitCommandToHwQueue", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDISubmitSignalSyncObjectsToHwQueue", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiDdDDISubmitWaitForSyncObjectsToHwQueue", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiGetAppliedDeviceGammaRamp", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiGetBitmapDpiScaleValue", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiGetDCDpiScaleValue", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiGetGammaRampCapability", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiScaleRgn", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiScaleValues", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtGdiSetPrivateDeviceGammaRamp", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    /* Added in Windows 10 1709 */
    /* FIXME i#1750: fill in details */
    {{WIN14,0},"NtGdiDdDDIAddSurfaceToSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDICreateBundleObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDICreateProtectedSession", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIDDisplayEnum", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIDestroyProtectedSession", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIDispMgrCreate", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIDispMgrSourceOperation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIDispMgrTargetOperation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIExtractBundleObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIGetProcessDeviceLostSupport", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIOpenProtectedSessionFromNtHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIPresentRedirected", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIQueryProtectedSessionInfoFromNtHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIQueryProtectedSessionStatus", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIRemoveSurfaceFromSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDISetDeviceLostSupport", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDISetMonitorColorSpaceTransform", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiDdDDIUnOrderedPresentSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtGdiEnsureDpiDepDefaultGuiFontForPlateau", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    /* Added in Windows 10 1709 */
    /* FIXME i#1750: fill in details */
    {{WIN15,0},"NtGdiDdDDIGetProcessDeviceRemovalSupport", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtGdiDdDDIOpenBundleObjectNtHandleFromName", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtGdiDdDDIOpenKeyedMutexFromNtHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtGdiDdDDISetProcessDeviceRemovalSupport", UNKNOWN, DRSYS_TYPE_UNKNOWN, },

};
#define NUM_GDI32_SYSCALLS \
    (sizeof(syscall_gdi32_info)/sizeof(syscall_gdi32_info[0]))

size_t
num_gdi32_syscalls(void)
{
    return NUM_GDI32_SYSCALLS;
}
