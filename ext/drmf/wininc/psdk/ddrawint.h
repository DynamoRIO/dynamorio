/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from a ReactOS header to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

/* from reactos/include/psdk/ddrawint.h */

/*
 * ddrawint.h
 *
 * DirectDraw NT driver interface
 *
 * Contributors:
 *   Created by Ge van Geldorp
 *
 * THIS SOFTWARE IS NOT COPYRIGHTED
 *
 * This source code is offered for use in the public domain. You may
 * use, modify or distribute it freely.
 *
 * This code is distributed in the hope that it will be useful but
 * WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 * DISCLAIMED. This includes but is not limited to warranties of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef __DD_INCLUDED__
#define __DD_INCLUDED__

DEFINE_GUID( GUID_MiscellaneousCallbacks,  0xEFD60CC0, 0x49e7, 0x11d0, 0x88, 0x9d, 0x0, 0xaa, 0x0, 0xbb, 0xb7, 0x6a);
DEFINE_GUID( GUID_Miscellaneous2Callbacks, 0x406B2F00, 0x3E5A, 0x11D1, 0xB6, 0x40, 0x00, 0xAA, 0x00, 0xA1, 0xF9, 0x6A);
DEFINE_GUID( GUID_VideoPortCallbacks,      0xefd60cc1, 0x49e7, 0x11d0, 0x88, 0x9d, 0x0, 0xaa, 0x0, 0xbb, 0xb7, 0x6a);
DEFINE_GUID( GUID_ColorControlCallbacks,   0xefd60cc2, 0x49e7, 0x11d0, 0x88, 0x9d, 0x0, 0xaa, 0x0, 0xbb, 0xb7, 0x6a);
DEFINE_GUID( GUID_MotionCompCallbacks,     0xb1122b40, 0x5dA5, 0x11d1, 0x8f, 0xcF, 0x00, 0xc0, 0x4f, 0xc2, 0x9b, 0x4e);
DEFINE_GUID( GUID_VideoPortCaps,           0xefd60cc3, 0x49e7, 0x11d0, 0x88, 0x9d, 0x0, 0xaa, 0x0, 0xbb, 0xb7, 0x6a);
DEFINE_GUID( GUID_D3DCaps,                 0x7bf06991, 0x8794, 0x11d0, 0x91, 0x39, 0x08, 0x00, 0x36, 0xd2, 0xef, 0x02);
DEFINE_GUID( GUID_D3DExtendedCaps,         0x7de41f80, 0x9d93, 0x11d0, 0x89, 0xab, 0x00, 0xa0, 0xc9, 0x05, 0x41, 0x29);
DEFINE_GUID( GUID_D3DCallbacks,            0x7bf06990, 0x8794, 0x11d0, 0x91, 0x39, 0x08, 0x00, 0x36, 0xd2, 0xef, 0x02);
DEFINE_GUID( GUID_D3DCallbacks2,           0xba584e1, 0x70b6, 0x11d0, 0x88, 0x9d, 0x0, 0xaa, 0x0, 0xbb, 0xb7, 0x6a);
DEFINE_GUID( GUID_D3DCallbacks3,           0xddf41230, 0xec0a, 0x11d0, 0xa9, 0xb6, 0x00, 0xaa, 0x00, 0xc0, 0x99, 0x3e);
DEFINE_GUID( GUID_NonLocalVidMemCaps,      0x86c4fa80, 0x8d84, 0x11d0, 0x94, 0xe8, 0x00, 0xc0, 0x4f, 0xc3, 0x41, 0x37);
DEFINE_GUID( GUID_KernelCallbacks,         0x80863800, 0x6B06, 0x11D0, 0x9B, 0x06, 0x0, 0xA0, 0xC9, 0x03, 0xA3, 0xB8);
DEFINE_GUID( GUID_KernelCaps,              0xFFAA7540, 0x7AA8, 0x11D0, 0x9B, 0x06, 0x00, 0xA0, 0xC9, 0x03, 0xA3, 0xB8);
DEFINE_GUID( GUID_ZPixelFormats,           0x93869880, 0x36cf, 0x11d1, 0x9b, 0x1b, 0x0, 0xaa, 0x0, 0xbb, 0xb8, 0xae);
DEFINE_GUID( GUID_DDMoreCaps,              0x880baf30, 0xb030, 0x11d0, 0x8e, 0xa7, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b);
DEFINE_GUID( GUID_D3DParseUnknownCommandCallback, 0x2e04ffa0, 0x98e4, 0x11d1, 0x8c, 0xe1, 0x0, 0xa0, 0xc9, 0x6, 0x29, 0xa8);
DEFINE_GUID( GUID_NTCallbacks,             0x6fe9ecde, 0xdf89, 0x11d1, 0x9d, 0xb0, 0x00, 0x60, 0x08, 0x27, 0x71, 0xba);
DEFINE_GUID( GUID_DDMoreSurfaceCaps,       0x3b8a0466, 0xf269, 0x11d1, 0x88, 0x0b, 0x0, 0xc0, 0x4f, 0xd9, 0x30, 0xc5);
DEFINE_GUID( GUID_GetHeapAlignment,        0x42e02f16, 0x7b41, 0x11d2, 0x8b, 0xff, 0x0, 0xa0, 0xc9, 0x83, 0xea, 0xf6);
DEFINE_GUID( GUID_UpdateNonLocalHeap,      0x42e02f17, 0x7b41, 0x11d2, 0x8b, 0xff, 0x0, 0xa0, 0xc9, 0x83, 0xea, 0xf6);
DEFINE_GUID( GUID_NTPrivateDriverCaps,     0xfad16a23, 0x7b66, 0x11d2, 0x83, 0xd7, 0x0, 0xc0, 0x4f, 0x7c, 0xe5, 0x8c);
DEFINE_GUID( GUID_DDStereoMode,            0xf828169c, 0xa8e8, 0x11d2, 0xa1, 0xf2, 0x0, 0xa0, 0xc9, 0x83, 0xea, 0xf6);
DEFINE_GUID( GUID_VPE2Callbacks,           0x52882147, 0x2d47, 0x469a, 0xa0, 0xd1, 0x3, 0x45, 0x58, 0x90, 0xf6, 0xc8);


#ifndef GUID_DEFS_ONLY

#ifndef _NO_DDRAWINT_NO_COM
#ifndef _NO_COM
#define _NO_COM
#include <ddraw.h>
#include <dvp.h>
#undef _NO_COM
#else
#include <ddraw.h>
#include <dvp.h>
#endif
#else
#include <ddraw.h>
#include <dvp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAKE_HRESULT // fixme this if statment should not be here, but MAKE_HRESULT should be here
#define MAKE_HRESULT(sev,fac,code) ((HRESULT) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))) )
#endif

#ifndef FLATPTR_DEFINED
typedef ULONG_PTR FLATPTR;
#define FLATPTR_DEFINED
#endif

typedef struct _DD_VIDEOPORT_LOCAL   *PDD_VIDEOPORT_LOCAL;

/************************************************************************/
/* _DD_GETHEAPALIGNMENTDATA is defined in dmemmgr.h                     */
 /************************************************************************/
struct _DD_GETHEAPALIGNMENTDATA;
#ifndef DD_GETHEAPALIGNMENTDATA_DECLARED
typedef struct _DD_GETHEAPALIGNMENTDATA *PDD_GETHEAPALIGNMENTDATA;
#define DD_GETHEAPALIGNMENTDATA_DECLARED
#endif

/************************************************************************/
/* Video memory info structures                                         */
/************************************************************************/

typedef struct _VIDEOMEMORY {
  DWORD dwFlags;
  FLATPTR fpStart;
  __GNU_EXTENSION union {
    FLATPTR fpEnd;
    DWORD dwWidth;
  };
  DDSCAPS ddsCaps;
  DDSCAPS ddsCapsAlt;
  __GNU_EXTENSION union {
    struct _VMEMHEAP *lpHeap;
    DWORD dwHeight;
  };
} VIDEOMEMORY, *PVIDEOMEMORY;

typedef struct _VIDEOMEMORYINFO {
  FLATPTR fpPrimary;
  DWORD dwFlags;
  DWORD dwDisplayWidth;
  DWORD dwDisplayHeight;
  LONG lDisplayPitch;
  DDPIXELFORMAT ddpfDisplay;
  DWORD dwOffscreenAlign;
  DWORD dwOverlayAlign;
  DWORD dwTextureAlign;
  DWORD dwZBufferAlign;
  DWORD dwAlphaAlign;
  PVOID pvPrimary;
} VIDEOMEMORYINFO, *LPVIDEOMEMORYINFO;

typedef struct _DD_DIRECTDRAW_GLOBAL {
  PVOID dhpdev;
  ULONG_PTR dwReserved1;
  ULONG_PTR dwReserved2;
  LPDDVIDEOPORTCAPS lpDDVideoPortCaps;
} DD_DIRECTDRAW_GLOBAL, *PDD_DIRECTDRAW_GLOBAL;

typedef struct _DD_DIRECTDRAW_LOCAL {
  PDD_DIRECTDRAW_GLOBAL lpGbl;
} DD_DIRECTDRAW_LOCAL, *PDD_DIRECTDRAW_LOCAL;

typedef struct _DD_SURFACE_GLOBAL {
  __GNU_EXTENSION union {
    DWORD dwBlockSizeY;
    LONG lSlicePitch;
  };
  __GNU_EXTENSION union {
    PVIDEOMEMORY lpVidMemHeap;
    DWORD dwBlockSizeX;
    DWORD dwUserMemSize;
  };
  FLATPTR fpVidMem;
  __GNU_EXTENSION union {
    LONG lPitch;
    DWORD dwLinearSize;
  };
  LONG yHint;
  LONG xHint;
  DWORD wHeight;
  DWORD wWidth;
  ULONG_PTR dwReserved1;
  DDPIXELFORMAT ddpfSurface;
  FLATPTR fpHeapOffset;
  HANDLE hCreatorProcess;
} DD_SURFACE_GLOBAL, *PDD_SURFACE_GLOBAL;

typedef struct _DD_SURFACE_MORE {
  DWORD dwMipMapCount;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  DWORD dwOverlayFlags;
  DDSCAPSEX ddsCapsEx;
  DWORD dwSurfaceHandle;
} DD_SURFACE_MORE, *PDD_SURFACE_MORE;

typedef struct _DD_ATTACHLIST *PDD_ATTACHLIST;

typedef struct _DD_SURFACE_LOCAL {
  PDD_SURFACE_GLOBAL lpGbl;
  DWORD dwFlags;
  DDSCAPS ddsCaps;
  ULONG_PTR dwReserved1;
  __GNU_EXTENSION union {
    DDCOLORKEY ddckCKSrcOverlay;
    DDCOLORKEY ddckCKSrcBlt;
  };
  __GNU_EXTENSION union {
    DDCOLORKEY ddckCKDestOverlay;
    DDCOLORKEY ddckCKDestBlt;
  };
  PDD_SURFACE_MORE lpSurfMore;
  PDD_ATTACHLIST lpAttachList;
  PDD_ATTACHLIST lpAttachListFrom;
  RECT rcOverlaySrc;
} DD_SURFACE_LOCAL, *PDD_SURFACE_LOCAL;

typedef struct _DD_ATTACHLIST {
  PDD_ATTACHLIST lpLink;
  PDD_SURFACE_LOCAL lpAttached;
} DD_ATTACHLIST;

typedef struct _DD_SURFACE_INT {
  PDD_SURFACE_LOCAL lpLcl;
} DD_SURFACE_INT, *PDD_SURFACE_INT;

/************************************************************************/
/* DDI representation of the DirectDrawPalette object                   */
/************************************************************************/

typedef struct _DD_PALETTE_GLOBAL {
  ULONG_PTR Reserved1;
} DD_PALETTE_GLOBAL, *PDD_PALETTE_GLOBAL;

/************************************************************************/
/* DDI representation of the DirectDrawVideo object                     */
/************************************************************************/

typedef struct {
  PDD_DIRECTDRAW_LOCAL lpDD;
  GUID guid;
  DWORD dwUncompWidth;
  DWORD dwUncompHeight;
  DDPIXELFORMAT ddUncompPixelFormat;
  DWORD dwDriverReserved1;
  DWORD dwDriverReserved2;
  DWORD dwDriverReserved3;
  LPVOID lpDriverReserved1;
  LPVOID lpDriverReserved2;
  LPVOID lpDriverReserved3;
} DD_MOTIONCOMP_LOCAL, *PDD_MOTIONCOMP_LOCAL;

typedef struct _DD_VIDEOPORT_LOCAL {
  PDD_DIRECTDRAW_LOCAL lpDD;
  DDVIDEOPORTDESC ddvpDesc;
  DDVIDEOPORTINFO ddvpInfo;
  PDD_SURFACE_INT lpSurface;
  PDD_SURFACE_INT lpVBISurface;
  DWORD dwNumAutoflip;
  DWORD dwNumVBIAutoflip;
  ULONG_PTR dwReserved1;
  ULONG_PTR dwReserved2;
  ULONG_PTR dwReserved3;
} DD_VIDEOPORT_LOCAL;

/************************************************************************/
/* IDirectDrawSurface callbacks                                         */
/************************************************************************/

typedef struct _DD_LOCKDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  DWORD bHasRect;
  RECTL rArea;
  LPVOID lpSurfData;
  HRESULT ddRVal;
  PVOID Lock;
  DWORD dwFlags;
  FLATPTR fpProcess;
} DD_LOCKDATA, *PDD_LOCKDATA;
typedef DWORD (WINAPI *PDD_SURFCB_LOCK)(PDD_LOCKDATA);

typedef struct _DD_UNLOCKDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  HRESULT ddRVal;
  PVOID Unlock;
} DD_UNLOCKDATA, *PDD_UNLOCKDATA;
typedef DWORD (WINAPI *PDD_SURFCB_UNLOCK)(PDD_UNLOCKDATA);

#define DDABLT_SRCOVERDEST        0x00000001
#define DDBLT_AFLAGS              0x80000000

typedef struct _DD_BLTDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDDestSurface;
  RECTL rDest;
  PDD_SURFACE_LOCAL lpDDSrcSurface;
  RECTL rSrc;
  DWORD dwFlags;
  DWORD dwROPFlags;
  DDBLTFX bltFX;
  HRESULT ddRVal;
  PVOID Blt;
  BOOL IsClipped;
  RECTL rOrigDest;
  RECTL rOrigSrc;
  DWORD dwRectCnt;
  LPRECT prDestRects;
  DWORD dwAFlags;
  DDARGB ddargbScaleFactors;
} DD_BLTDATA, *PDD_BLTDATA;
typedef DWORD (WINAPI *PDD_SURFCB_BLT)(PDD_BLTDATA);

typedef struct _DD_UPDATEOVERLAYDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDDestSurface;
  RECTL rDest;
  PDD_SURFACE_LOCAL lpDDSrcSurface;
  RECTL rSrc;
  DWORD dwFlags;
  DDOVERLAYFX overlayFX;
  HRESULT ddRVal;
  PVOID UpdateOverlay;
} DD_UPDATEOVERLAYDATA, *PDD_UPDATEOVERLAYDATA;
typedef DWORD (WINAPI *PDD_SURFCB_UPDATEOVERLAY)(PDD_UPDATEOVERLAYDATA);

typedef struct _DD_SETOVERLAYPOSITIONDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSrcSurface;
  PDD_SURFACE_LOCAL lpDDDestSurface;
  LONG lXPos;
  LONG lYPos;
  HRESULT ddRVal;
  PVOID SetOverlayPosition;
} DD_SETOVERLAYPOSITIONDATA, *PDD_SETOVERLAYPOSITIONDATA;
typedef DWORD (WINAPI *PDD_SURFCB_SETOVERLAYPOSITION)(PDD_SETOVERLAYPOSITIONDATA);

typedef struct _DD_SETPALETTEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  PDD_PALETTE_GLOBAL lpDDPalette;
  HRESULT ddRVal;
  PVOID SetPalette;
  BOOL Attach;
} DD_SETPALETTEDATA, *PDD_SETPALETTEDATA;
typedef DWORD (WINAPI *PDD_SURFCB_SETPALETTE)(PDD_SETPALETTEDATA);

typedef struct _DD_FLIPDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpSurfCurr;
  PDD_SURFACE_LOCAL lpSurfTarg;
  DWORD dwFlags;
  HRESULT ddRVal;
  PVOID Flip;
  PDD_SURFACE_LOCAL lpSurfCurrLeft;
  PDD_SURFACE_LOCAL lpSurfTargLeft;
} DD_FLIPDATA, *PDD_FLIPDATA;
typedef DWORD (WINAPI *PDD_SURFCB_FLIP)(PDD_FLIPDATA);

typedef struct _DD_DESTROYSURFACEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  HRESULT ddRVal;
  PVOID DestroySurface;
} DD_DESTROYSURFACEDATA, *PDD_DESTROYSURFACEDATA;
typedef DWORD (WINAPI *PDD_SURFCB_DESTROYSURFACE)(PDD_DESTROYSURFACEDATA);

typedef struct _DD_SETCLIPLISTDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  HRESULT ddRVal;
  PVOID SetClipList;
} DD_SETCLIPLISTDATA, *PDD_SETCLIPLISTDATA;
typedef DWORD (WINAPI *PDD_SURFCB_SETCLIPLIST)(PDD_SETCLIPLISTDATA);

typedef struct _DD_ADDATTACHEDSURFACEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  PDD_SURFACE_LOCAL lpSurfAttached;
  HRESULT ddRVal;
  PVOID AddAttachedSurface;
} DD_ADDATTACHEDSURFACEDATA, *PDD_ADDATTACHEDSURFACEDATA;
typedef DWORD (WINAPI *PDD_SURFCB_ADDATTACHEDSURFACE)(PDD_ADDATTACHEDSURFACEDATA);

typedef struct _DD_SETCOLORKEYDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  DWORD dwFlags;
  DDCOLORKEY ckNew;
  HRESULT ddRVal;
  PVOID SetColorKey;
} DD_SETCOLORKEYDATA, *PDD_SETCOLORKEYDATA;
typedef DWORD (WINAPI *PDD_SURFCB_SETCOLORKEY)(PDD_SETCOLORKEYDATA);

typedef struct _DD_GETBLTSTATUSDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  DWORD dwFlags;
  HRESULT ddRVal;
  PVOID GetBltStatus;
} DD_GETBLTSTATUSDATA, *PDD_GETBLTSTATUSDATA;
typedef DWORD (WINAPI *PDD_SURFCB_GETBLTSTATUS)(PDD_GETBLTSTATUSDATA);

typedef struct _DD_GETFLIPSTATUSDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  DWORD dwFlags;
  HRESULT ddRVal;
  PVOID GetFlipStatus;
} DD_GETFLIPSTATUSDATA, *PDD_GETFLIPSTATUSDATA;
typedef DWORD (WINAPI *PDD_SURFCB_GETFLIPSTATUS)(PDD_GETFLIPSTATUSDATA);

typedef struct DD_SURFACECALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_SURFCB_DESTROYSURFACE DestroySurface;
  PDD_SURFCB_FLIP Flip;
  PDD_SURFCB_SETCLIPLIST SetClipList;
  PDD_SURFCB_LOCK Lock;
  PDD_SURFCB_UNLOCK Unlock;
  PDD_SURFCB_BLT Blt;
  PDD_SURFCB_SETCOLORKEY SetColorKey;
  PDD_SURFCB_ADDATTACHEDSURFACE AddAttachedSurface;
  PDD_SURFCB_GETBLTSTATUS GetBltStatus;
  PDD_SURFCB_GETFLIPSTATUS GetFlipStatus;
  PDD_SURFCB_UPDATEOVERLAY UpdateOverlay;
  PDD_SURFCB_SETOVERLAYPOSITION SetOverlayPosition;
  PVOID reserved4;
  PDD_SURFCB_SETPALETTE SetPalette;
} DD_SURFACECALLBACKS, *PDD_SURFACECALLBACKS;

#define DDHAL_SURFCB32_DESTROYSURFACE       0x00000001
#define DDHAL_SURFCB32_FLIP                 0x00000002
#define DDHAL_SURFCB32_SETCLIPLIST          0x00000004
#define DDHAL_SURFCB32_LOCK                 0x00000008
#define DDHAL_SURFCB32_UNLOCK               0x00000010
#define DDHAL_SURFCB32_BLT                  0x00000020
#define DDHAL_SURFCB32_SETCOLORKEY          0x00000040
#define DDHAL_SURFCB32_ADDATTACHEDSURFACE   0x00000080
#define DDHAL_SURFCB32_GETBLTSTATUS         0x00000100
#define DDHAL_SURFCB32_GETFLIPSTATUS        0x00000200
#define DDHAL_SURFCB32_UPDATEOVERLAY        0x00000400
#define DDHAL_SURFCB32_SETOVERLAYPOSITION   0x00000800
#define DDHAL_SURFCB32_RESERVED4            0x00001000
#define DDHAL_SURFCB32_SETPALETTE           0x00002000

/************************************************************************/
/* IDirectDraw callbacks                                                */
/************************************************************************/

typedef struct _DD_CREATESURFACEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  DDSURFACEDESC *lpDDSurfaceDesc;
  PDD_SURFACE_LOCAL *lplpSList;
  DWORD dwSCnt;
  HRESULT ddRVal;
  PVOID CreateSurface;
} DD_CREATESURFACEDATA, *PDD_CREATESURFACEDATA;
typedef DWORD (WINAPI *PDD_CREATESURFACE)(PDD_CREATESURFACEDATA);

typedef struct _DD_DRVSETCOLORKEYDATA {
  PDD_SURFACE_LOCAL lpDDSurface;
  DWORD dwFlags;
  DDCOLORKEY ckNew;
  HRESULT ddRVal;
  PVOID SetColorKey;
} DD_DRVSETCOLORKEYDATA, *PDD_DRVSETCOLORKEYDATA;
typedef DWORD (WINAPI *PDD_SETCOLORKEY)(PDD_DRVSETCOLORKEYDATA);

#define DDWAITVB_I_TESTVB    0x80000006

typedef struct _DD_WAITFORVERTICALBLANKDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  DWORD dwFlags;
  DWORD bIsInVB;
  DWORD hEvent;
  HRESULT ddRVal;
  PVOID WaitForVerticalBlank;
} DD_WAITFORVERTICALBLANKDATA, *PDD_WAITFORVERTICALBLANKDATA;
typedef DWORD (WINAPI *PDD_WAITFORVERTICALBLANK)(PDD_WAITFORVERTICALBLANKDATA);

typedef struct _DD_CANCREATESURFACEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  DDSURFACEDESC *lpDDSurfaceDesc;
  DWORD bIsDifferentPixelFormat;
  HRESULT ddRVal;
  PVOID CanCreateSurface;
} DD_CANCREATESURFACEDATA, *PDD_CANCREATESURFACEDATA;
typedef DWORD (WINAPI *PDD_CANCREATESURFACE)(PDD_CANCREATESURFACEDATA);

typedef struct _DD_CREATEPALETTEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_PALETTE_GLOBAL lpDDPalette;
  LPPALETTEENTRY lpColorTable;
  HRESULT ddRVal;
  PVOID CreatePalette;
  BOOL is_excl;
} DD_CREATEPALETTEDATA, *PDD_CREATEPALETTEDATA;
typedef DWORD (WINAPI *PDD_CREATEPALETTE)(PDD_CREATEPALETTEDATA);

typedef struct _DD_GETSCANLINEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  DWORD dwScanLine;
  HRESULT ddRVal;
  PVOID GetScanLine;
} DD_GETSCANLINEDATA, *PDD_GETSCANLINEDATA;
typedef DWORD (WINAPI *PDD_GETSCANLINE)(PDD_GETSCANLINEDATA);

typedef struct _DD_MAPMEMORYDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  BOOL bMap;
  HANDLE hProcess;
  FLATPTR fpProcess;
  HRESULT ddRVal;
} DD_MAPMEMORYDATA, *PDD_MAPMEMORYDATA;
typedef DWORD (WINAPI *PDD_MAPMEMORY)(PDD_MAPMEMORYDATA);

typedef struct _DD_DESTROYDRIVERDATA *PDD_DESTROYDRIVERDATA;
typedef struct _DD_SETMODEDATA *PDD_SETMODEDATA;

typedef DWORD (APIENTRY *PDD_DESTROYDRIVER)(PDD_DESTROYDRIVERDATA);
typedef DWORD (APIENTRY *PDD_SETMODE)(PDD_SETMODEDATA);

typedef struct DD_CALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_DESTROYDRIVER DestroyDriver;
  PDD_CREATESURFACE CreateSurface;
  PDD_SETCOLORKEY SetColorKey;
  PDD_SETMODE SetMode;
  PDD_WAITFORVERTICALBLANK WaitForVerticalBlank;
  PDD_CANCREATESURFACE CanCreateSurface;
  PDD_CREATEPALETTE CreatePalette;
  PDD_GETSCANLINE GetScanLine;
  PDD_MAPMEMORY MapMemory;
} DD_CALLBACKS, *PDD_CALLBACKS;

#define DDHAL_CB32_DESTROYDRIVER        0x00000001l
#define DDHAL_CB32_CREATESURFACE        0x00000002l
#define DDHAL_CB32_SETCOLORKEY          0x00000004l
#define DDHAL_CB32_SETMODE              0x00000008l
#define DDHAL_CB32_WAITFORVERTICALBLANK 0x00000010l
#define DDHAL_CB32_CANCREATESURFACE     0x00000020l
#define DDHAL_CB32_CREATEPALETTE        0x00000040l
#define DDHAL_CB32_GETSCANLINE          0x00000080l
#define DDHAL_CB32_MAPMEMORY            0x80000000l

typedef struct _DD_GETAVAILDRIVERMEMORYDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  DDSCAPS DDSCaps;
  DWORD dwTotal;
  DWORD dwFree;
  HRESULT ddRVal;
  PVOID GetAvailDriverMemory;
} DD_GETAVAILDRIVERMEMORYDATA, *PDD_GETAVAILDRIVERMEMORYDATA;
typedef DWORD (WINAPI *PDD_GETAVAILDRIVERMEMORY)(PDD_GETAVAILDRIVERMEMORYDATA);

typedef struct _DD_MISCELLANEOUSCALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_GETAVAILDRIVERMEMORY GetAvailDriverMemory;
} DD_MISCELLANEOUSCALLBACKS, *PDD_MISCELLANEOUSCALLBACKS;

#define DDHAL_MISCCB32_GETAVAILDRIVERMEMORY 0x00000001

typedef DWORD (WINAPI *PDD_ALPHABLT)(PDD_BLTDATA);

typedef struct _DD_CREATESURFACEEXDATA {
  DWORD dwFlags;
  PDD_DIRECTDRAW_LOCAL lpDDLcl;
  PDD_SURFACE_LOCAL lpDDSLcl;
  HRESULT ddRVal;
} DD_CREATESURFACEEXDATA, *PDD_CREATESURFACEEXDATA;
typedef DWORD (WINAPI *PDD_CREATESURFACEEX)(PDD_CREATESURFACEEXDATA);

typedef struct _DD_GETDRIVERSTATEDATA {
  DWORD dwFlags;
  __GNU_EXTENSION union {
    PDD_DIRECTDRAW_GLOBAL lpDD;
    DWORD_PTR dwhContext;
  };
  LPDWORD lpdwStates;
  DWORD dwLength;
  HRESULT ddRVal;
} DD_GETDRIVERSTATEDATA, *PDD_GETDRIVERSTATEDATA;
typedef DWORD (WINAPI *PDD_GETDRIVERSTATE)(PDD_GETDRIVERSTATEDATA);

typedef struct _DD_DESTROYDDLOCALDATA {
  DWORD dwFlags;
  PDD_DIRECTDRAW_LOCAL pDDLcl;
  HRESULT ddRVal;
} DD_DESTROYDDLOCALDATA, *PDD_DESTROYDDLOCALDATA;
typedef DWORD (WINAPI *PDD_DESTROYDDLOCAL)(PDD_DESTROYDDLOCALDATA);

typedef struct _DD_MISCELLANEOUS2CALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_ALPHABLT AlphaBlt;
  PDD_CREATESURFACEEX CreateSurfaceEx;
  PDD_GETDRIVERSTATE GetDriverState;
  PDD_DESTROYDDLOCAL DestroyDDLocal;
} DD_MISCELLANEOUS2CALLBACKS, *PDD_MISCELLANEOUS2CALLBACKS;

#define DDHAL_MISC2CB32_ALPHABLT            0x00000001
#define DDHAL_MISC2CB32_CREATESURFACEEX     0x00000002
#define DDHAL_MISC2CB32_GETDRIVERSTATE      0x00000004
#define DDHAL_MISC2CB32_DESTROYDDLOCAL      0x00000008

typedef struct _DD_FREEDRIVERMEMORYDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  HRESULT ddRVal;
  PVOID FreeDriverMemory;
} DD_FREEDRIVERMEMORYDATA, *PDD_FREEDRIVERMEMORYDATA;
typedef DWORD (WINAPI *PDD_FREEDRIVERMEMORY)(PDD_FREEDRIVERMEMORYDATA);

typedef struct _DD_SETEXCLUSIVEMODEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  DWORD dwEnterExcl;
  DWORD dwReserved;
  HRESULT ddRVal;
  PVOID SetExclusiveMode;
} DD_SETEXCLUSIVEMODEDATA, *PDD_SETEXCLUSIVEMODEDATA;
typedef DWORD (WINAPI *PDD_SETEXCLUSIVEMODE)(PDD_SETEXCLUSIVEMODEDATA);

typedef struct _DD_FLIPTOGDISURFACEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  DWORD dwToGDI;
  DWORD dwReserved;
  HRESULT ddRVal;
  PVOID FlipToGDISurface;
} DD_FLIPTOGDISURFACEDATA, *PDD_FLIPTOGDISURFACEDATA;
typedef DWORD (WINAPI *PDD_FLIPTOGDISURFACE)(PDD_FLIPTOGDISURFACEDATA);

typedef struct _DD_NTCALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_FREEDRIVERMEMORY FreeDriverMemory;
  PDD_SETEXCLUSIVEMODE SetExclusiveMode;
  PDD_FLIPTOGDISURFACE FlipToGDISurface;
} DD_NTCALLBACKS, *PDD_NTCALLBACKS;

#define DDHAL_NTCB32_FREEDRIVERMEMORY 0x00000001
#define DDHAL_NTCB32_SETEXCLUSIVEMODE 0x00000002
#define DDHAL_NTCB32_FLIPTOGDISURFACE 0x00000004

/************************************************************************/
/* IDirectDrawPalette callbacks                                         */
/************************************************************************/

typedef struct _DD_DESTROYPALETTEDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_PALETTE_GLOBAL lpDDPalette;
  HRESULT ddRVal;
  PVOID DestroyPalette;
} DD_DESTROYPALETTEDATA, *PDD_DESTROYPALETTEDATA;
typedef DWORD (WINAPI *PDD_PALCB_DESTROYPALETTE)(PDD_DESTROYPALETTEDATA);

typedef struct _DD_SETENTRIESDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_PALETTE_GLOBAL lpDDPalette;
  DWORD dwBase;
  DWORD dwNumEntries;
  LPPALETTEENTRY lpEntries;
  HRESULT ddRVal;
  PVOID SetEntries;
} DD_SETENTRIESDATA, *PDD_SETENTRIESDATA;
typedef DWORD (WINAPI *PDD_PALCB_SETENTRIES)(PDD_SETENTRIESDATA);

typedef struct DD_PALETTECALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_PALCB_DESTROYPALETTE DestroyPalette;
  PDD_PALCB_SETENTRIES SetEntries;
} DD_PALETTECALLBACKS, *PDD_PALETTECALLBACKS;

#define DDHAL_PALCB32_DESTROYPALETTE  0x00000001l
#define DDHAL_PALCB32_SETENTRIES      0x00000002l

/************************************************************************/
/* IDirectDrawVideoport callbacks                                       */
/************************************************************************/

typedef struct _DD_CANCREATEVPORTDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  LPDDVIDEOPORTDESC lpDDVideoPortDesc;
  HRESULT ddRVal;
  PVOID CanCreateVideoPort;
} DD_CANCREATEVPORTDATA, *PDD_CANCREATEVPORTDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_CANCREATEVIDEOPORT)(PDD_CANCREATEVPORTDATA);

typedef struct _DD_CREATEVPORTDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  LPDDVIDEOPORTDESC lpDDVideoPortDesc;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  HRESULT ddRVal;
  PVOID CreateVideoPort;
} DD_CREATEVPORTDATA, *PDD_CREATEVPORTDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_CREATEVIDEOPORT)(PDD_CREATEVPORTDATA);

typedef struct _DD_FLIPVPORTDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  PDD_SURFACE_LOCAL lpSurfCurr;
  PDD_SURFACE_LOCAL lpSurfTarg;
  HRESULT ddRVal;
  PVOID FlipVideoPort;
} DD_FLIPVPORTDATA, *PDD_FLIPVPORTDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_FLIP)(PDD_FLIPVPORTDATA);

typedef struct _DD_GETVPORTBANDWIDTHDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  LPDDPIXELFORMAT lpddpfFormat;
  DWORD dwWidth;
  DWORD dwHeight;
  DWORD dwFlags;
  LPDDVIDEOPORTBANDWIDTH lpBandwidth;
  HRESULT ddRVal;
  PVOID GetVideoPortBandwidth;
} DD_GETVPORTBANDWIDTHDATA, *PDD_GETVPORTBANDWIDTHDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_GETBANDWIDTH)(PDD_GETVPORTBANDWIDTHDATA);

typedef struct _DD_GETVPORTINPUTFORMATDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  DWORD dwFlags;
  LPDDPIXELFORMAT lpddpfFormat;
  DWORD dwNumFormats;
  HRESULT ddRVal;
  PVOID GetVideoPortInputFormats;
} DD_GETVPORTINPUTFORMATDATA, *PDD_GETVPORTINPUTFORMATDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_GETINPUTFORMATS)(PDD_GETVPORTINPUTFORMATDATA);

typedef struct _DD_GETVPORTOUTPUTFORMATDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  DWORD dwFlags;
  LPDDPIXELFORMAT lpddpfInputFormat;
  LPDDPIXELFORMAT lpddpfOutputFormats;
  DWORD dwNumFormats;
  HRESULT ddRVal;
  PVOID GetVideoPortInputFormats;
} DD_GETVPORTOUTPUTFORMATDATA, *PDD_GETVPORTOUTPUTFORMATDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_GETOUTPUTFORMATS)(PDD_GETVPORTOUTPUTFORMATDATA);

typedef struct _DD_GETVPORTFIELDDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  BOOL bField;
  HRESULT ddRVal;
  PVOID GetVideoPortField;
} DD_GETVPORTFIELDDATA, *PDD_GETVPORTFIELDDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_GETFIELD)(PDD_GETVPORTFIELDDATA);

typedef struct _DD_GETVPORTLINEDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  DWORD dwLine;
  HRESULT ddRVal;
  PVOID GetVideoPortLine;
} DD_GETVPORTLINEDATA, *PDD_GETVPORTLINEDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_GETLINE)(PDD_GETVPORTLINEDATA);

typedef struct _DD_GETVPORTCONNECTDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  DWORD dwPortId;
  LPDDVIDEOPORTCONNECT lpConnect;
  DWORD dwNumEntries;
  HRESULT ddRVal;
  PVOID GetVideoPortConnectInfo;
} DD_GETVPORTCONNECTDATA, *PDD_GETVPORTCONNECTDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_GETVPORTCONNECT)(PDD_GETVPORTCONNECTDATA);

typedef struct _DD_DESTROYVPORTDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  HRESULT ddRVal;
  PVOID DestroyVideoPort;
} DD_DESTROYVPORTDATA, *PDD_DESTROYVPORTDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_DESTROYVPORT)(PDD_DESTROYVPORTDATA);

typedef struct _DD_GETVPORTFLIPSTATUSDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  FLATPTR fpSurface;
  HRESULT ddRVal;
  PVOID GetVideoPortFlipStatus;
} DD_GETVPORTFLIPSTATUSDATA, *PDD_GETVPORTFLIPSTATUSDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_GETFLIPSTATUS)(PDD_GETVPORTFLIPSTATUSDATA);

typedef struct _DD_UPDATEVPORTDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  PDD_SURFACE_INT *lplpDDSurface;
  PDD_SURFACE_INT *lplpDDVBISurface;
  LPDDVIDEOPORTINFO lpVideoInfo;
  DWORD dwFlags;
  DWORD dwNumAutoflip;
  DWORD dwNumVBIAutoflip;
  HRESULT ddRVal;
  PVOID UpdateVideoPort;
} DD_UPDATEVPORTDATA, *PDD_UPDATEVPORTDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_UPDATE)(PDD_UPDATEVPORTDATA);

typedef struct _DD_WAITFORVPORTSYNCDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  DWORD dwFlags;
  DWORD dwLine;
  DWORD dwTimeOut;
  HRESULT ddRVal;
  PVOID UpdateVideoPort;
} DD_WAITFORVPORTSYNCDATA, *PDD_WAITFORVPORTSYNCDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_WAITFORSYNC)(PDD_WAITFORVPORTSYNCDATA);

typedef struct _DD_GETVPORTSIGNALDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  DWORD dwStatus;
  HRESULT ddRVal;
  PVOID GetVideoSignalStatus;
} DD_GETVPORTSIGNALDATA, *PDD_GETVPORTSIGNALDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_GETSIGNALSTATUS)(PDD_GETVPORTSIGNALDATA);

typedef struct _DD_VPORTCOLORDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  DWORD dwFlags;
  LPDDCOLORCONTROL lpColorData;
  HRESULT ddRVal;
  PVOID ColorControl;
} DD_VPORTCOLORDATA, *PDD_VPORTCOLORDATA;
typedef DWORD (WINAPI *PDD_VPORTCB_COLORCONTROL)(PDD_VPORTCOLORDATA);

typedef struct DD_VIDEOPORTCALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_VPORTCB_CANCREATEVIDEOPORT CanCreateVideoPort;
  PDD_VPORTCB_CREATEVIDEOPORT CreateVideoPort;
  PDD_VPORTCB_FLIP FlipVideoPort;
  PDD_VPORTCB_GETBANDWIDTH GetVideoPortBandwidth;
  PDD_VPORTCB_GETINPUTFORMATS GetVideoPortInputFormats;
  PDD_VPORTCB_GETOUTPUTFORMATS GetVideoPortOutputFormats;
  PVOID lpReserved1;
  PDD_VPORTCB_GETFIELD GetVideoPortField;
  PDD_VPORTCB_GETLINE GetVideoPortLine;
  PDD_VPORTCB_GETVPORTCONNECT GetVideoPortConnectInfo;
  PDD_VPORTCB_DESTROYVPORT DestroyVideoPort;
  PDD_VPORTCB_GETFLIPSTATUS GetVideoPortFlipStatus;
  PDD_VPORTCB_UPDATE UpdateVideoPort;
  PDD_VPORTCB_WAITFORSYNC WaitForVideoPortSync;
  PDD_VPORTCB_GETSIGNALSTATUS GetVideoSignalStatus;
  PDD_VPORTCB_COLORCONTROL ColorControl;
} DD_VIDEOPORTCALLBACKS, *PDD_VIDEOPORTCALLBACKS;

#define DDHAL_VPORT32_CANCREATEVIDEOPORT    0x00000001
#define DDHAL_VPORT32_CREATEVIDEOPORT       0x00000002
#define DDHAL_VPORT32_FLIP                  0x00000004
#define DDHAL_VPORT32_GETBANDWIDTH          0x00000008
#define DDHAL_VPORT32_GETINPUTFORMATS       0x00000010
#define DDHAL_VPORT32_GETOUTPUTFORMATS      0x00000020
#define DDHAL_VPORT32_GETFIELD              0x00000080
#define DDHAL_VPORT32_GETLINE               0x00000100
#define DDHAL_VPORT32_GETCONNECT            0x00000200
#define DDHAL_VPORT32_DESTROY               0x00000400
#define DDHAL_VPORT32_GETFLIPSTATUS         0x00000800
#define DDHAL_VPORT32_UPDATE                0x00001000
#define DDHAL_VPORT32_WAITFORSYNC           0x00002000
#define DDHAL_VPORT32_GETSIGNALSTATUS       0x00004000
#define DDHAL_VPORT32_COLORCONTROL          0x00008000


/************************************************************************/
/* IDirectDrawColorControl callbacks                                    */
/************************************************************************/

#define DDRAWI_GETCOLOR      0x0001
#define DDRAWI_SETCOLOR      0x0002

typedef struct _DD_COLORCONTROLDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  LPDDCOLORCONTROL lpColorData;
  DWORD dwFlags;
  HRESULT ddRVal;
  PVOID ColorControl;
} DD_COLORCONTROLDATA, *PDD_COLORCONTROLDATA;
typedef DWORD (WINAPI *PDD_COLORCB_COLORCONTROL)(PDD_COLORCONTROLDATA);

typedef struct _DD_COLORCONTROLCALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_COLORCB_COLORCONTROL ColorControl;
} DD_COLORCONTROLCALLBACKS, *PDD_COLORCONTROLCALLBACKS;

#define DDHAL_COLOR_COLORCONTROL            0x00000001

/************************************************************************/
/* IDirectDrawVideo callbacks                                           */
/************************************************************************/

typedef struct _DD_GETMOCOMPGUIDSDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  DWORD dwNumGuids;
  GUID *lpGuids;
  HRESULT ddRVal;
} DD_GETMOCOMPGUIDSDATA, *PDD_GETMOCOMPGUIDSDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_GETGUIDS)(PDD_GETMOCOMPGUIDSDATA);

typedef struct _DD_GETMOCOMPFORMATSDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  GUID *lpGuid;
  DWORD dwNumFormats;
  LPDDPIXELFORMAT lpFormats;
  HRESULT ddRVal;
} DD_GETMOCOMPFORMATSDATA, *PDD_GETMOCOMPFORMATSDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_GETFORMATS)(PDD_GETMOCOMPFORMATSDATA);

typedef struct _DD_CREATEMOCOMPDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_MOTIONCOMP_LOCAL lpMoComp;
  GUID *lpGuid;
  DWORD dwUncompWidth;
  DWORD dwUncompHeight;
  DDPIXELFORMAT ddUncompPixelFormat;
  LPVOID lpData;
  DWORD dwDataSize;
  HRESULT ddRVal;
} DD_CREATEMOCOMPDATA, *PDD_CREATEMOCOMPDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_CREATE)(PDD_CREATEMOCOMPDATA);

typedef struct _DDCOMPBUFFERINFO {
  DWORD dwSize;
  DWORD dwNumCompBuffers;
  DWORD dwWidthToCreate;
  DWORD dwHeightToCreate;
  DWORD dwBytesToAllocate;
  DDSCAPS2 ddCompCaps;
  DDPIXELFORMAT ddPixelFormat;
} DDCOMPBUFFERINFO, *LPDDCOMPBUFFERINFO;

typedef struct _DD_GETMOCOMPCOMPBUFFDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  GUID *lpGuid;
  DWORD dwWidth;
  DWORD dwHeight;
  DDPIXELFORMAT ddPixelFormat;
  DWORD dwNumTypesCompBuffs;
  LPDDCOMPBUFFERINFO lpCompBuffInfo;
  HRESULT ddRVal;
} DD_GETMOCOMPCOMPBUFFDATA, *PDD_GETMOCOMPCOMPBUFFDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_GETCOMPBUFFINFO)(PDD_GETMOCOMPCOMPBUFFDATA);

typedef struct _DD_GETINTERNALMOCOMPDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  GUID *lpGuid;
  DWORD dwWidth;
  DWORD dwHeight;
  DDPIXELFORMAT ddPixelFormat;
  DWORD dwScratchMemAlloc;
  HRESULT ddRVal;
} DD_GETINTERNALMOCOMPDATA, *PDD_GETINTERNALMOCOMPDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_GETINTERNALINFO)(PDD_GETINTERNALMOCOMPDATA);

typedef struct _DD_BEGINMOCOMPFRAMEDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_MOTIONCOMP_LOCAL lpMoComp;
  PDD_SURFACE_LOCAL lpDestSurface;
  DWORD dwInputDataSize;
  LPVOID lpInputData;
  DWORD dwOutputDataSize;
  LPVOID lpOutputData;
  HRESULT ddRVal;
} DD_BEGINMOCOMPFRAMEDATA, *PDD_BEGINMOCOMPFRAMEDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_BEGINFRAME)(PDD_BEGINMOCOMPFRAMEDATA);

typedef struct _DD_ENDMOCOMPFRAMEDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_MOTIONCOMP_LOCAL lpMoComp;
  LPVOID lpInputData;
  DWORD dwInputDataSize;
  HRESULT ddRVal;
} DD_ENDMOCOMPFRAMEDATA, *PDD_ENDMOCOMPFRAMEDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_ENDFRAME)(PDD_ENDMOCOMPFRAMEDATA);

typedef struct _DDMOCOMPBUFFERINFO {
  DWORD dwSize;
  PDD_SURFACE_LOCAL lpCompSurface;
  DWORD dwDataOffset;
  DWORD dwDataSize;
  LPVOID lpPrivate;
} DDMOCOMPBUFFERINFO, *LPDDMOCOMPBUFFERINFO;

typedef struct _DD_RENDERMOCOMPDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_MOTIONCOMP_LOCAL lpMoComp;
  DWORD dwNumBuffers;
  LPDDMOCOMPBUFFERINFO lpBufferInfo;
  DWORD dwFunction;
  LPVOID lpInputData;
  DWORD dwInputDataSize;
  LPVOID lpOutputData;
  DWORD dwOutputDataSize;
  HRESULT ddRVal;
} DD_RENDERMOCOMPDATA, *PDD_RENDERMOCOMPDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_RENDER)(PDD_RENDERMOCOMPDATA);

#define DDMCQUERY_READ 0x00000001

typedef struct _DD_QUERYMOCOMPSTATUSDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_MOTIONCOMP_LOCAL lpMoComp;
  PDD_SURFACE_LOCAL lpSurface;
  DWORD dwFlags;
  HRESULT ddRVal;
} DD_QUERYMOCOMPSTATUSDATA, *PDD_QUERYMOCOMPSTATUSDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_QUERYSTATUS)(PDD_QUERYMOCOMPSTATUSDATA);

typedef struct _DD_DESTROYMOCOMPDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_MOTIONCOMP_LOCAL lpMoComp;
  HRESULT ddRVal;
} DD_DESTROYMOCOMPDATA, *PDD_DESTROYMOCOMPDATA;
typedef DWORD (WINAPI *PDD_MOCOMPCB_DESTROY)(PDD_DESTROYMOCOMPDATA);

typedef struct DD_MOTIONCOMPCALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_MOCOMPCB_GETGUIDS GetMoCompGuids;
  PDD_MOCOMPCB_GETFORMATS GetMoCompFormats;
  PDD_MOCOMPCB_CREATE CreateMoComp;
  PDD_MOCOMPCB_GETCOMPBUFFINFO GetMoCompBuffInfo;
  PDD_MOCOMPCB_GETINTERNALINFO GetInternalMoCompInfo;
  PDD_MOCOMPCB_BEGINFRAME BeginMoCompFrame;
  PDD_MOCOMPCB_ENDFRAME EndMoCompFrame;
  PDD_MOCOMPCB_RENDER RenderMoComp;
  PDD_MOCOMPCB_QUERYSTATUS QueryMoCompStatus;
  PDD_MOCOMPCB_DESTROY DestroyMoComp;
} DD_MOTIONCOMPCALLBACKS, *PDD_MOTIONCOMPCALLBACKS;

#define DDHAL_MOCOMP32_GETGUIDS             0x00000001
#define DDHAL_MOCOMP32_GETFORMATS           0x00000002
#define DDHAL_MOCOMP32_CREATE               0x00000004
#define DDHAL_MOCOMP32_GETCOMPBUFFINFO      0x00000008
#define DDHAL_MOCOMP32_GETINTERNALINFO      0x00000010
#define DDHAL_MOCOMP32_BEGINFRAME           0x00000020
#define DDHAL_MOCOMP32_ENDFRAME             0x00000040
#define DDHAL_MOCOMP32_RENDER               0x00000080
#define DDHAL_MOCOMP32_QUERYSTATUS          0x00000100
#define DDHAL_MOCOMP32_DESTROY              0x00000200

/************************************************************************/
/* D3D buffer callbacks                                                 */
/************************************************************************/

typedef struct _DD_D3DBUFCALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_CANCREATESURFACE CanCreateD3DBuffer;
  PDD_CREATESURFACE CreateD3DBuffer;
  PDD_SURFCB_DESTROYSURFACE DestroyD3DBuffer;
  PDD_SURFCB_LOCK LockD3DBuffer;
  PDD_SURFCB_UNLOCK UnlockD3DBuffer;
} DD_D3DBUFCALLBACKS, *PDD_D3DBUFCALLBACKS;

/************************************************************************/
/* DdGetDriverInfo callback                                             */
/************************************************************************/

typedef struct _DD_GETDRIVERINFODATA {
// Input:
  PVOID dhpdev;
  DWORD dwSize;
  DWORD dwFlags;
  GUID guidInfo;
  DWORD dwExpectedSize;
  PVOID lpvData;
// Output:
  DWORD dwActualSize;
  HRESULT ddRVal;
} DD_GETDRIVERINFODATA, *PDD_GETDRIVERINFODATA;
typedef DWORD (WINAPI *PDD_GETDRIVERINFO)(PDD_GETDRIVERINFODATA);

/************************************************************************/
/* Driver info structures                                               */
/************************************************************************/

typedef struct _DDNTCORECAPS {
  DWORD dwSize;
  DWORD dwCaps;
  DWORD dwCaps2;
  DWORD dwCKeyCaps;
  DWORD dwFXCaps;
  DWORD dwFXAlphaCaps;
  DWORD dwPalCaps;
  DWORD dwSVCaps;
  DWORD dwAlphaBltConstBitDepths;
  DWORD dwAlphaBltPixelBitDepths;
  DWORD dwAlphaBltSurfaceBitDepths;
  DWORD dwAlphaOverlayConstBitDepths;
  DWORD dwAlphaOverlayPixelBitDepths;
  DWORD dwAlphaOverlaySurfaceBitDepths;
  DWORD dwZBufferBitDepths;
  DWORD dwVidMemTotal;
  DWORD dwVidMemFree;
  DWORD dwMaxVisibleOverlays;
  DWORD dwCurrVisibleOverlays;
  DWORD dwNumFourCCCodes;
  DWORD dwAlignBoundarySrc;
  DWORD dwAlignSizeSrc;
  DWORD dwAlignBoundaryDest;
  DWORD dwAlignSizeDest;
  DWORD dwAlignStrideAlign;
  DWORD dwRops[DD_ROP_SPACE];
  DDSCAPS ddsCaps;
  DWORD dwMinOverlayStretch;
  DWORD dwMaxOverlayStretch;
  DWORD dwMinLiveVideoStretch;
  DWORD dwMaxLiveVideoStretch;
  DWORD dwMinHwCodecStretch;
  DWORD dwMaxHwCodecStretch;
  DWORD dwReserved1;
  DWORD dwReserved2;
  DWORD dwReserved3;
  DWORD dwSVBCaps;
  DWORD dwSVBCKeyCaps;
  DWORD dwSVBFXCaps;
  DWORD dwSVBRops[DD_ROP_SPACE];
  DWORD dwVSBCaps;
  DWORD dwVSBCKeyCaps;
  DWORD dwVSBFXCaps;
  DWORD dwVSBRops[DD_ROP_SPACE];
  DWORD dwSSBCaps;
  DWORD dwSSBCKeyCaps;
  DWORD dwSSBFXCaps;
  DWORD dwSSBRops[DD_ROP_SPACE];
  DWORD dwMaxVideoPorts;
  DWORD dwCurrVideoPorts;
  DWORD dwSVBCaps2;
} DDNTCORECAPS, *PDDNTCORECAPS;

typedef struct _DD_HALINFO_V4 {
  DWORD dwSize;
  VIDEOMEMORYINFO vmiData;
  DDNTCORECAPS ddCaps;
  PDD_GETDRIVERINFO GetDriverInfo;
  DWORD dwFlags;
} DD_HALINFO_V4, *PDD_HALINFO_V4;

typedef struct _DD_HALINFO {
  DWORD dwSize;
  VIDEOMEMORYINFO vmiData;
  DDNTCORECAPS ddCaps;
  PDD_GETDRIVERINFO GetDriverInfo;
  DWORD dwFlags;
  PVOID lpD3DGlobalDriverData;
  PVOID lpD3DHALCallbacks;
  PDD_D3DBUFCALLBACKS lpD3DBufCallbacks;
} DD_HALINFO, *PDD_HALINFO;

typedef struct _DD_NONLOCALVIDMEMCAPS {
  DWORD dwSize;
  DWORD dwNLVBCaps;
  DWORD dwNLVBCaps2;
  DWORD dwNLVBCKeyCaps;
  DWORD dwNLVBFXCaps;
  DWORD dwNLVBRops[DD_ROP_SPACE];
} DD_NONLOCALVIDMEMCAPS, *PDD_NONLOCALVIDMEMCAPS;

typedef struct _DD_MORESURFACECAPS {
  DWORD dwSize;
  DDSCAPSEX ddsCapsMore;
  struct tagNTExtendedHeapRestrictions {
    DDSCAPSEX ddsCapsEx;
    DDSCAPSEX ddsCapsExAlt;
  } ddsExtendedHeapRestrictions[1];
} DD_MORESURFACECAPS, *PDD_MORESURFACECAPS;


/*********************************************************/
/* Kernel Callbacks                                      */
/*********************************************************/
typedef struct _DD_SYNCSURFACEDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_SURFACE_LOCAL lpDDSurface;
  DWORD dwSurfaceOffset;
  ULONG_PTR fpLockPtr;
  LONG lPitch;
  DWORD dwOverlayOffset;
  ULONG dwDriverReserved1;
  ULONG dwDriverReserved2;
  ULONG dwDriverReserved3;
  ULONG dwDriverReserved4;
  HRESULT ddRVal;
} DD_SYNCSURFACEDATA, *PDD_SYNCSURFACEDATA;
typedef DWORD (WINAPI *PDD_KERNELCB_SYNCSURFACE)(PDD_SYNCSURFACEDATA);

typedef struct _DD_SYNCVIDEOPORTDATA {
  PDD_DIRECTDRAW_LOCAL lpDD;
  PDD_VIDEOPORT_LOCAL lpVideoPort;
  DWORD dwOriginOffset;
  DWORD dwHeight;
  DWORD dwVBIHeight;
  ULONG dwDriverReserved1;
  ULONG dwDriverReserved2;
  ULONG dwDriverReserved3;
  HRESULT ddRVal;
} DD_SYNCVIDEOPORTDATA, *PDD_SYNCVIDEOPORTDATA;
typedef DWORD (WINAPI *PDD_KERNELCB_SYNCVIDEOPORT)(PDD_SYNCVIDEOPORTDATA);

typedef struct DD_NTPRIVATEDRIVERCAPS {
  DWORD dwSize;
  DWORD dwPrivateCaps;
} DD_NTPRIVATEDRIVERCAPS;

typedef struct _DD_UPDATENONLOCALHEAPDATA {
  PDD_DIRECTDRAW_GLOBAL lpDD;
  DWORD dwHeap;
  FLATPTR fpGARTLin;
  FLATPTR fpGARTDev;
  ULONG_PTR ulPolicyMaxBytes;
  HRESULT ddRVal;
  VOID* UpdateNonLocalHeap;
} DD_UPDATENONLOCALHEAPDATA, *PDD_UPDATENONLOCALHEAPDATA;

typedef struct _DD_STEREOMODE {
  DWORD dwSize;
  DWORD dwHeight;
  DWORD dwWidth;
  DWORD dwBpp;
  DWORD dwRefreshRate;
  BOOL bSupported;
} DD_STEREOMODE, *PDD_STEREOMODE;

typedef struct _DD_MORECAPS {
  DWORD dwSize;
  DWORD dwAlphaCaps;
  DWORD dwSVBAlphaCaps;
  DWORD dwVSBAlphaCaps;
  DWORD dwSSBAlphaCaps;
  DWORD dwFilterCaps;
  DWORD dwSVBFilterCaps;
  DWORD dwVSBFilterCaps;
  DWORD dwSSBFilterCaps;
} DD_MORECAPS, *PDD_MORECAPS;

typedef struct _DD_CLIPPER_GLOBAL {
  ULONG_PTR dwReserved1;
} DD_CLIPPER_GLOBAL;

typedef struct _DD_CLIPPER_LOCAL {
  ULONG_PTR dwReserved1;
} DD_CLIPPER_LOCAL;

typedef struct _DD_PALETTE_LOCAL {
  ULONG dwReserved0;
  ULONG_PTR dwReserved1;
} DD_PALETTE_LOCAL;

typedef struct DD_KERNELCALLBACKS {
  DWORD dwSize;
  DWORD dwFlags;
  PDD_KERNELCB_SYNCSURFACE SyncSurfaceData;
  PDD_KERNELCB_SYNCVIDEOPORT SyncVideoPortData;
} DD_KERNELCALLBACKS, *PDD_KERNELCALLBACKS;

#define MAX_AUTOFLIP_BUFFERS                  10
#define DDSCAPS_EXECUTEBUFFER                 DDSCAPS_RESERVED2
#define DDSCAPS_COMMANDBUFFER                 DDSCAPS_RESERVED3
#define DDSCAPS_VERTEXBUFFER                  DDSCAPS_RESERVED4
#define DDPF_D3DFORMAT                        0x00200000l
#define D3DFORMAT_OP_TEXTURE                  0x00000001L
#define D3DFORMAT_OP_VOLUMETEXTURE            0x00000002L
#define D3DFORMAT_OP_CUBETEXTURE              0x00000004L
#define D3DFORMAT_OP_OFFSCREEN_RENDERTARGET   0x00000008L
#define D3DFORMAT_OP_SAME_FORMAT_RENDERTARGET 0x00000010L
#define D3DFORMAT_OP_ZSTENCIL                 0x00000040L
#define D3DFORMAT_OP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH  0x00000080L
#define D3DFORMAT_OP_SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET 0x00000100L
#define D3DFORMAT_OP_DISPLAYMODE              0x00000400L
#define D3DFORMAT_OP_3DACCELERATION           0x00000800L
#define D3DFORMAT_OP_PIXELSIZE                0x00001000L
#define D3DFORMAT_OP_CONVERT_TO_ARGB          0x00002000L
#define D3DFORMAT_OP_OFFSCREENPLAIN           0x00004000L
#define D3DFORMAT_OP_SRGBREAD                 0x00008000L
#define D3DFORMAT_OP_BUMPMAP                  0x00010000L
#define D3DFORMAT_OP_DMAP                     0x00020000L
#define D3DFORMAT_OP_NOFILTER                 0x00040000L
#define D3DFORMAT_MEMBEROFGROUP_ARGB          0x00080000L
#define D3DFORMAT_OP_SRGBWRITE                0x00100000L
#define D3DFORMAT_OP_NOALPHABLEND             0x00200000L
#define D3DFORMAT_OP_AUTOGENMIPMAP            0x00400000L
#define D3DFORMAT_OP_VERTEXTEXTURE            0x00800000L
#define D3DFORMAT_OP_NOTEXCOORDWRAPNORMIP     0x01000000L
#define DDHAL_PLEASEALLOC_BLOCKSIZE           0x00000002l
#define DDHAL_PLEASEALLOC_USERMEM             0x00000004l

#define VIDMEM_ISLINEAR                       0x00000001l
#define VIDMEM_ISRECTANGULAR                  0x00000002l
#define VIDMEM_ISHEAP                         0x00000004l
#define VIDMEM_ISNONLOCAL                     0x00000008l
#define VIDMEM_ISWC                           0x00000010l
#define VIDMEM_HEAPDISABLED                   0x00000020l

#define DDHAL_CREATESURFACEEX_SWAPHANDLES     0x00000001l

#define DDHAL_KERNEL_SYNCSURFACEDATA          0x00000001l
#define DDHAL_KERNEL_SYNCVIDEOPORTDATA        0x00000002l

#define DDHAL_DRIVER_NOTHANDLED               0x00000000l
#define DDHAL_DRIVER_HANDLED                  0x00000001l
#define DDHAL_DRIVER_NOCKEYHW                 0x00000002l

#define DDRAWISURF_HASCKEYSRCBLT              0x00000800L
#define DDRAWISURF_HASPIXELFORMAT             0x00002000L
#define DDRAWISURF_HASOVERLAYDATA             0x00004000L
#define DDRAWISURF_FRONTBUFFER                0x04000000L
#define DDRAWISURF_BACKBUFFER                 0x08000000L
#define DDRAWISURF_INVALID                    0x10000000L
#define DDRAWISURF_DRIVERMANAGED              0x40000000L

#define ROP_HAS_SOURCE                        0x00000001l
#define ROP_HAS_PATTERN                       0x00000002l
#define ROP_HAS_SOURCEPATTERN                 ROP_HAS_SOURCE | ROP_HAS_PATTERN

#define DDHAL_EXEBUFCB32_CANCREATEEXEBUF      0x00000001l
#define DDHAL_EXEBUFCB32_CREATEEXEBUF         0x00000002l
#define DDHAL_EXEBUFCB32_DESTROYEXEBUF        0x00000004l
#define DDHAL_EXEBUFCB32_LOCKEXEBUF           0x00000008l
#define DDHAL_EXEBUFCB32_UNLOCKEXEBUF         0x00000010l

#define DDHAL_D3DBUFCB32_CANCREATED3DBUF      DDHAL_EXEBUFCB32_CANCREATEEXEBUF
#define DDHAL_D3DBUFCB32_CREATED3DBUF         DDHAL_EXEBUFCB32_CREATEEXEBUF
#define DDHAL_D3DBUFCB32_DESTROYD3DBUF        DDHAL_EXEBUFCB32_DESTROYEXEBUF
#define DDHAL_D3DBUFCB32_LOCKD3DBUF           DDHAL_EXEBUFCB32_LOCKEXEBUF
#define DDHAL_D3DBUFCB32_UNLOCKD3DBUF         DDHAL_EXEBUFCB32_UNLOCKEXEBUF

#define DDHALINFO_ISPRIMARYDISPLAY            0x00000001
#define DDHALINFO_MODEXILLEGAL                0x00000002
#define DDHALINFO_GETDRIVERINFOSET            0x00000004
#define DDHALINFO_GETDRIVERINFO2              0x00000008

#define DDRAWIVPORT_ON                        0x00000001
#define DDRAWIVPORT_SOFTWARE_AUTOFLIP         0x00000002
#define DDRAWIVPORT_COLORKEYANDINTERP         0x00000004

#define DDHAL_PRIVATECAP_ATOMICSURFACECREATION   0x00000001l
#define DDHAL_PRIVATECAP_NOTIFYPRIMARYCREATION   0x00000002l
#define DDHAL_PRIVATECAP_RESERVED1               0x00000004l

#define DDRAWI_VPORTSTART                     0x0001
#define DDRAWI_VPORTSTOP                      0x0002
#define DDRAWI_VPORTUPDATE                    0x0003
#define DDRAWI_VPORTGETCOLOR                  0x0001
#define DDRAWI_VPORTSETCOLOR                  0x0002

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GUID_DEFS_ONLY */

#endif /* __DD_INCLUDED__ */
