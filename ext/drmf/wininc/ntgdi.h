/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from a Windows SDK header to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

/* from SDK 7.1 ntgdi.h */

/******************************Module*Header*******************************\
* Module Name: ntgdi.h
*
* Structures defining kernel-mode entry points for GDI.
*
* Copyright (c) 1994-1999 Microsoft Corporation
\**************************************************************************/

#ifndef W32KAPI
#    define W32KAPI DECLSPEC_ADDRSAFE
#endif

typedef struct _HLSURF_INFORMATION_PROBE {
    union {
        HLSURF_INFORMATION_SURFACE Surface;
        HLSURF_INFORMATION_PRESENTFLAGS PresentFlags;
        HLSURF_INFORMATION_TOKENUPDATEID UpdateId;
        HLSURF_INFORMATION_SET_SIGNALING SetSignaling;
        DWMSURFACEDATA SurfaceData;
        HLSURF_INFORMATION_DIRTYREGIONS DirtyRegions;
        HLSURF_INFORMATION_REDIRSTYLE RedirStyle;
    } u;
} HLSURF_INFORMATION_PROBE, *PHLSURF_INFORMATION_PROBE;

// A few GDI API's fill buffers but have no input parameter saying how big
// the buffers are.  The idiom is that you call it once with a NULL buffer
// pointer to find out how big the buffer should be, allocate that much,
// then call again.  This sounds like a job for __out_bcount_opt(size), but
// this annotation requires (size) to be known beforehand (i.e. it is a
// precondition).  The best we can do is to specify the size as a
// postcondition.

#define __out_post_bcount_opt(size)                          \
    __post __valid __refparam __post __byte_writableTo(size) \
    __exceptthat __maybenull
#define __out_post_ecount_opt(size)                          \
    __post __valid __refparam __post __elem_writableTo(size) \
    __exceptthat __maybenull

// Trace creation of all GDI SURFACE objects
#define TRACE_SURFACE_ALLOCS (DBG || 0)

// enable directX API to change surface pointer
#ifdef COMBOX_SANDBOX
#    define DX_LONGHORN_PRESERVEDC
#endif

// PRIVATE

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiInit();

__kernel_entry W32KAPI int APIENTRY
NtGdiSetDIBitsToDeviceInternal(__in HDC hdcDest, __in int xDst, __in int yDst,
                               __in DWORD cx, __in DWORD cy, __in int xSrc, __in int ySrc,
                               __in DWORD iStartScan, __in DWORD cNumScan,
                               __in_bcount(cjMaxBits) LPBYTE pInitBits,
                               __in LPBITMAPINFO pbmi, __in DWORD iUsage,
                               __in UINT cjMaxBits, __in UINT cjMaxInfo,
                               __in BOOL bTransformCoordinates, __in_opt HANDLE hcmXform);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetFontResourceInfoInternalW(__in_ecount(cwc) LPWSTR pwszFiles, __in ULONG cwc,
                                  __in ULONG cFiles, __in UINT cjIn,
                                  __out LPDWORD pdwBytes, __out_bcount(cjIn) LPVOID pvBuf,
                                  __in DWORD iType);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiGetGlyphIndicesW(__in HDC hdc, __in_ecount_opt(cwc) LPWSTR pwc, __in int cwc,
                      __out_opt LPWORD
                          pgi, // XXX(bruening): should be __out_ecount_opt(cwc)?
                      __in DWORD iMode);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiGetGlyphIndicesWInternal(__in HDC hdc, __in_ecount_opt(cwc) LPWSTR pwc, __in int cwc,
                              __out_opt LPWORD pgi, __in DWORD iMode, __in BOOL bSubset);

//
// pLogPal is annotated as __in_bcount(cEntries * 4  + 4) because the
// current SAL doesn't support sizeof operator. The size of related buffer is
// sizeof(LOGPALETTE) - sizeof(PALETTEENTRY) + sizeof(PALETTEENTRY) * cEntries.
//

__kernel_entry W32KAPI HPALETTE APIENTRY
NtGdiCreatePaletteInternal(__in_bcount(cEntries * 4 + 4) LPLOGPALETTE pLogPal,
                           __in UINT cEntries);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiArcInternal(__in ARCTYPE arctype, __in HDC hdc, __in int x1, __in int y1,
                 __in int x2, __in int y2, __in int x3, __in int y3, __in int x4,
                 __in int y4);

__kernel_entry W32KAPI int APIENTRY
NtGdiStretchDIBitsInternal(__in HDC hdc, __in int xDst, __in int yDst, __in int cxDst,
                           __in int cyDst, __in int xSrc, __in int ySrc, __in int cxSrc,
                           __in int cySrc, __in_bcount_opt(cjMaxBits) LPBYTE pjInit,
                           __in LPBITMAPINFO pbmi, __in DWORD dwUsage, __in DWORD dwRop4,
                           __in UINT cjMaxInfo, __in UINT cjMaxBits,
                           __in HANDLE hcmXform);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiGetOutlineTextMetricsInternalW(__in HDC hdc, __in ULONG cjotm,
                                    __out_bcount_opt(cjotm) OUTLINETEXTMETRICW *potmw,
                                    __out TMDIFF *ptmd);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetAndSetDCDword(__in HDC hdc, __in UINT u, __in DWORD dwIn, __out DWORD *pdwResult);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiGetDCObject(__in HDC hdc, __in int itype);

__kernel_entry W32KAPI HDC APIENTRY
NtGdiGetDCforBitmap(__in HBITMAP hsurf);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetMonitorID(__in HDC hdc, __in DWORD dwSize,
                  __out_bcount(dwSize) LPWSTR pszMonitorID);

// flags returned from GetUFI and passed to GetUFIBits
#define FL_UFI_PRIVATEFONT 1
#define FL_UFI_DESIGNVECTOR_PFF 2
#define FL_UFI_MEMORYFONT 4

__kernel_entry W32KAPI INT APIENTRY
NtGdiGetLinkedUFIs(__in HDC hdc,
                   __out_ecount_opt(BufferSize) PUNIVERSAL_FONT_ID pufiLinkedUFIs,
                   __in INT BufferSize);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetLinkedUFIs(__in HDC hdc, __in_ecount(uNumUFIs) PUNIVERSAL_FONT_ID pufiLinks,
                   __in ULONG uNumUFIs);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetUFI(__in HDC hdc, __out PUNIVERSAL_FONT_ID pufi, __out_opt DESIGNVECTOR *pdv,
            __out ULONG *pcjDV, __out ULONG *pulBaseCheckSum, __out FLONG *pfl);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiForceUFIMapping(__in HDC hdc, __in PUNIVERSAL_FONT_ID pufi);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetUFIPathname(
    __in PUNIVERSAL_FONT_ID pufi, __deref_out_range(0, MAX_PATH * 3) ULONG *pcwc,
    __out_ecount_part_opt(MAX_PATH * 3, *pcwc) LPWSTR pwszPathname,
    __out_opt ULONG *pcNumFiles, __in FLONG fl, __out_opt BOOL *pbMemFont,
    __out_opt ULONG *pcjView,
    __out_opt PVOID *pvView, // XXX(bruening): was PVOID; should be bcount(pcjView)?
    __out_opt BOOL *pbTTC, __out_opt ULONG *piTTC);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiAddRemoteFontToDC(__in HDC hdc, __in_bcount(cjBuffer) PVOID pvBuffer,
                       __in ULONG cjBuffer, __in_opt PUNIVERSAL_FONT_ID pufi);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiAddFontMemResourceEx(__in_bcount(cjBuffer) PVOID pvBuffer, __in DWORD cjBuffer,
                          __in_bcount_opt(cjDV) DESIGNVECTOR *pdv, __in ULONG cjDV,
                          __out DWORD *pNumFonts);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiRemoveFontMemResourceEx(__in HANDLE hMMFont);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiUnmapMemFont(__in PVOID pvView);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiRemoveMergeFont(__in HDC hdc, __in UNIVERSAL_FONT_ID *pufi);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiAnyLinkedFonts();

// local printing with embedded fonts

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetEmbUFI(__in HDC hdc, __out PUNIVERSAL_FONT_ID pufi, __out_opt DESIGNVECTOR *pdv,
               __out ULONG *pcjDV, __out ULONG *pulBaseCheckSum, __out FLONG *pfl,
               __out KERNEL_PVOID *embFontID);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiGetEmbedFonts();

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiChangeGhostFont(__in KERNEL_PVOID *pfontID, __in BOOL bLoad);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiAddEmbFontToDC(__in HDC hdc, __in VOID **pFontID);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiFontIsLinked(__in HDC hdc);

__kernel_entry W32KAPI ULONG_PTR APIENTRY
NtGdiPolyPolyDraw(__in HDC hdc, __in PPOINT ppt, __in_ecount(ccpt) PULONG pcpt,
                  __in ULONG ccpt, __in int iFunc);

__kernel_entry W32KAPI LONG APIENTRY
NtGdiDoPalette(__in HPALETTE hpal, __in WORD iStart, __in WORD cEntries,
               __in_ecount_opt(cEntries) PALETTEENTRY *pPalEntries, __in DWORD iFunc,
               __in BOOL bInbound);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiComputeXformCoefficients(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetWidthTable(__in HDC hdc, __in ULONG cSpecial, __in_ecount(cwc) WCHAR *pwc,
                   __in ULONG cwc, __out_ecount(cwc) USHORT *psWidth,
                   __out_opt WIDTHDATA *pwd, __out FLONG *pflInfo);

__kernel_entry W32KAPI __success(return > 0) int APIENTRY
    NtGdiDescribePixelFormat(__in HDC hdc, __in int ipfd, __in UINT cjpfd,
                             __out_bcount(cjpfd) PPIXELFORMATDESCRIPTOR ppfd);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetPixelFormat(__in HDC hdc, __in int ipfd);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSwapBuffers(__in HDC hdc);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDxgGenericThunk(
    __in ULONG_PTR ulIndex, __in ULONG_PTR ulHandle, __inout SIZE_T *pdwSizeOfPtr1,
    __inout PVOID *pvPtr1, // XXX(bruening): was PVOID: should be bcount(pdwSizeOfPtr1)?
    __inout SIZE_T *pdwSizeOfPtr2,
    __inout PVOID *pvPtr2 // XXX(bruening): was PVOID: should be bcount(pdwSizeOfPtr2)?
);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdAddAttachedSurface(__in HANDLE hSurface, __in HANDLE hSurfaceAttached,
                          __inout PDD_ADDATTACHEDSURFACEDATA puAddAttachedSurfaceData);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDdAttachSurface(__in HANDLE hSurfaceFrom, __in HANDLE hSurfaceTo);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdBlt(__in HANDLE hSurfaceDest, __in HANDLE hSurfaceSrc,
           __inout PDD_BLTDATA puBltData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdCanCreateSurface(__in HANDLE hDirectDraw,
                        __inout PDD_CANCREATESURFACEDATA puCanCreateSurfaceData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdColorControl(__in HANDLE hSurface,
                    __inout PDD_COLORCONTROLDATA puColorControlData);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiDdCreateDirectDrawObject(__in HDC hdc);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdCreateSurface(__in HANDLE hDirectDraw, __in HANDLE *hSurface,
                     __inout DDSURFACEDESC *puSurfaceDescription,
                     __inout DD_SURFACE_GLOBAL *puSurfaceGlobalData,
                     __inout DD_SURFACE_LOCAL *puSurfaceLocalData,
                     __inout DD_SURFACE_MORE *puSurfaceMoreData,
                     __inout DD_CREATESURFACEDATA *puCreateSurfaceData,
                     __out HANDLE *puhSurface);

#ifdef DX_LONGHORN_PRESERVEDC

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdChangeSurfacePointer(__in HANDLE hSurface, __in PVOID pSurfacePointer);

#endif // DX_LONGHORN_PRESERVEDC

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiDdCreateSurfaceObject(__in HANDLE hDirectDrawLocal, __in HANDLE hSurface,
                           __in PDD_SURFACE_LOCAL puSurfaceLocal,
                           __in PDD_SURFACE_MORE puSurfaceMore,
                           __in PDD_SURFACE_GLOBAL puSurfaceGlobal, __in BOOL bComplete);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDdDeleteSurfaceObject(__in HANDLE hSurface);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDdDeleteDirectDrawObject(__in HANDLE hDirectDrawLocal);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdDestroySurface(__in HANDLE hSurface, __in BOOL bRealDestroy);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdFlip(__in HANDLE hSurfaceCurrent, __in HANDLE hSurfaceTarget,
            __in HANDLE hSurfaceCurrentLeft, __in HANDLE hSurfaceTargetLeft,
            __inout PDD_FLIPDATA puFlipData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetAvailDriverMemory(__in HANDLE hDirectDraw,
                            __inout PDD_GETAVAILDRIVERMEMORYDATA
                                puGetAvailDriverMemoryData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetBltStatus(__in HANDLE hSurface,
                    __inout PDD_GETBLTSTATUSDATA puGetBltStatusData);

__kernel_entry W32KAPI HDC APIENTRY
NtGdiDdGetDC(__in HANDLE hSurface, __in PALETTEENTRY *puColorTable);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetDriverInfo(__in HANDLE hDirectDraw,
                     __inout PDD_GETDRIVERINFODATA puGetDriverInfoData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetFlipStatus(__in HANDLE hSurface,
                     __inout PDD_GETFLIPSTATUSDATA puGetFlipStatusData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetScanLine(__in HANDLE hDirectDraw,
                   __inout PDD_GETSCANLINEDATA puGetScanLineData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdSetExclusiveMode(__in HANDLE hDirectDraw,
                        __inout PDD_SETEXCLUSIVEMODEDATA puSetExclusiveModeData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdFlipToGDISurface(__in HANDLE hDirectDraw,
                        __inout PDD_FLIPTOGDISURFACEDATA puFlipToGDISurfaceData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdLock(__in HANDLE hSurface, __inout PDD_LOCKDATA puLockData, __in HDC hdcClip);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDdQueryDirectDrawObject(__in HANDLE hDirectDrawLocal, __out PDD_HALINFO pHalInfo,
                             __out_ecount(3) DWORD *pCallBackFlags,
                             __out_opt LPD3DNTHAL_CALLBACKS puD3dCallbacks,
                             __out_opt LPD3DNTHAL_GLOBALDRIVERDATA puD3dDriverData,
                             __out_opt PDD_D3DBUFCALLBACKS puD3dBufferCallbacks,
                             __out_opt LPDDSURFACEDESC puD3dTextureFormats,
                             __out DWORD *puNumHeaps, __out_opt VIDEOMEMORY *puvmList,
                             __out DWORD *puNumFourCC, __out_opt DWORD *puFourCC);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDdReenableDirectDrawObject(__in HANDLE hDirectDrawLocal, __inout BOOL *pubNewMode);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDdReleaseDC(__in HANDLE hSurface);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDdResetVisrgn(__in HANDLE hSurface, __in HWND hwnd);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdSetColorKey(__in HANDLE hSurface, __inout PDD_SETCOLORKEYDATA puSetColorKeyData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdSetOverlayPosition(__in HANDLE hSurfaceSource, __in HANDLE hSurfaceDestination,
                          __inout PDD_SETOVERLAYPOSITIONDATA puSetOverlayPositionData);

__kernel_entry W32KAPI VOID APIENTRY
NtGdiDdUnattachSurface(__in HANDLE hSurface, __in HANDLE hSurfaceAttached);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdUnlock(__in HANDLE hSurface, __inout PDD_UNLOCKDATA puUnlockData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdUpdateOverlay(__in HANDLE hSurfaceDestination, __in HANDLE hSurfaceSource,
                     __inout PDD_UPDATEOVERLAYDATA puUpdateOverlayData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdWaitForVerticalBlank(__in HANDLE hDirectDraw,
                            __inout PDD_WAITFORVERTICALBLANKDATA
                                puWaitForVerticalBlankData);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiDdGetDxHandle(__in_opt HANDLE hDirectDraw, __in_opt HANDLE hSurface,
                   __in BOOL bRelease);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDdSetGammaRamp(__in HANDLE hDirectDraw, __in HDC hdc,
                    __in_bcount(256 * 3 * 2) LPVOID lpGammaRamp);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdLockD3D(__in HANDLE hSurface, __inout PDD_LOCKDATA puLockData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdUnlockD3D(__in HANDLE hSurface, __inout PDD_UNLOCKDATA puUnlockData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdCreateD3DBuffer(__in HANDLE hDirectDraw, __inout HANDLE *hSurface,
                       __inout DDSURFACEDESC *puSurfaceDescription,
                       __inout DD_SURFACE_GLOBAL *puSurfaceGlobalData,
                       __inout DD_SURFACE_LOCAL *puSurfaceLocalData,
                       __inout DD_SURFACE_MORE *puSurfaceMoreData,
                       __inout DD_CREATESURFACEDATA *puCreateSurfaceData,
                       __inout HANDLE *puhSurface);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdCanCreateD3DBuffer(__in HANDLE hDirectDraw,
                          __inout PDD_CANCREATESURFACEDATA puCanCreateSurfaceData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdDestroyD3DBuffer(__in HANDLE hSurface);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiD3dContextCreate(__in HANDLE hDirectDrawLocal, __in HANDLE hSurfColor,
                      __in HANDLE hSurfZ, __inout D3DNTHAL_CONTEXTCREATEI *pdcci);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiD3dContextDestroy(__in LPD3DNTHAL_CONTEXTDESTROYDATA varname);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiD3dContextDestroyAll(__out LPD3DNTHAL_CONTEXTDESTROYALLDATA pdcdad);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiD3dValidateTextureStageState(__inout LPD3DNTHAL_VALIDATETEXTURESTAGESTATEDATA pData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiD3dDrawPrimitives2(__in HANDLE hCmdBuf, __in HANDLE hVBuf,
                        __inout LPD3DNTHAL_DRAWPRIMITIVES2DATA pded,
                        __inout FLATPTR *pfpVidMemCmd, __inout DWORD *pdwSizeCmd,
                        __inout FLATPTR *pfpVidMemVtx, __inout DWORD *pdwSizeVtx);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetDriverState(__inout PDD_GETDRIVERSTATEDATA pdata);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdCreateSurfaceEx(__in HANDLE hDirectDraw, __in HANDLE hSurface,
                       __in DWORD dwSurfaceHandle);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpCanCreateVideoPort(__in HANDLE hDirectDraw,
                           __inout PDD_CANCREATEVPORTDATA puCanCreateVPortData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpColorControl(__in HANDLE hVideoPort, __inout PDD_VPORTCOLORDATA puVPortColorData);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiDvpCreateVideoPort(__in HANDLE hDirectDraw,
                        __inout PDD_CREATEVPORTDATA puCreateVPortData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpDestroyVideoPort(__in HANDLE hVideoPort,
                         __inout PDD_DESTROYVPORTDATA puDestroyVPortData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpFlipVideoPort(__in HANDLE hVideoPort, __in HANDLE hDDSurfaceCurrent,
                      __in HANDLE hDDSurfaceTarget,
                      __inout PDD_FLIPVPORTDATA puFlipVPortData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpGetVideoPortBandwidth(__in HANDLE hVideoPort,
                              __inout PDD_GETVPORTBANDWIDTHDATA puGetVPortBandwidthData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpGetVideoPortField(__in HANDLE hVideoPort,
                          __inout PDD_GETVPORTFIELDDATA puGetVPortFieldData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpGetVideoPortFlipStatus(
    __in HANDLE hDirectDraw, __inout PDD_GETVPORTFLIPSTATUSDATA puGetVPortFlipStatusData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpGetVideoPortInputFormats(__in HANDLE hVideoPort,
                                 __inout PDD_GETVPORTINPUTFORMATDATA
                                     puGetVPortInputFormatData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpGetVideoPortLine(__in HANDLE hVideoPort,
                         __inout PDD_GETVPORTLINEDATA puGetVPortLineData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpGetVideoPortOutputFormats(__in HANDLE hVideoPort,
                                  __inout PDD_GETVPORTOUTPUTFORMATDATA
                                      puGetVPortOutputFormatData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpGetVideoPortConnectInfo(__in HANDLE hDirectDraw,
                                __inout PDD_GETVPORTCONNECTDATA puGetVPortConnectData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpGetVideoSignalStatus(__in HANDLE hVideoPort,
                             __inout PDD_GETVPORTSIGNALDATA puGetVPortSignalData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpUpdateVideoPort(__in HANDLE hVideoPort, __in HANDLE *phSurfaceVideo,
                        __in HANDLE *phSurfaceVbi,
                        __inout PDD_UPDATEVPORTDATA puUpdateVPortData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpWaitForVideoPortSync(__in HANDLE hVideoPort,
                             __inout PDD_WAITFORVPORTSYNCDATA puWaitForVPortSyncData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpAcquireNotification(__in HANDLE hVideoPort, __inout HANDLE *hEvent,
                            __in LPDDVIDEOPORTNOTIFY pNotify);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDvpReleaseNotification(__in HANDLE hVideoPort, __in HANDLE hEvent);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetMoCompGuids(__in HANDLE hDirectDraw,
                      __inout PDD_GETMOCOMPGUIDSDATA puGetMoCompGuidsData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetMoCompFormats(__in HANDLE hDirectDraw,
                        __inout PDD_GETMOCOMPFORMATSDATA puGetMoCompFormatsData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetMoCompBuffInfo(__in HANDLE hDirectDraw,
                         __inout PDD_GETMOCOMPCOMPBUFFDATA puGetBuffData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdGetInternalMoCompInfo(__in HANDLE hDirectDraw,
                             __inout PDD_GETINTERNALMOCOMPDATA puGetInternalData);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiDdCreateMoComp(__in HANDLE hDirectDraw,
                    __inout PDD_CREATEMOCOMPDATA puCreateMoCompData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdDestroyMoComp(__in HANDLE hMoComp,
                     __inout PDD_DESTROYMOCOMPDATA puDestroyMoCompData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdBeginMoCompFrame(__in HANDLE hMoComp,
                        __inout PDD_BEGINMOCOMPFRAMEDATA puBeginFrameData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdEndMoCompFrame(__in HANDLE hMoComp, __inout PDD_ENDMOCOMPFRAMEDATA puEndFrameData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdRenderMoComp(__in HANDLE hMoComp, __inout PDD_RENDERMOCOMPDATA puRenderMoCompData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdQueryMoCompStatus(__in HANDLE hMoComp,
                         __inout PDD_QUERYMOCOMPSTATUSDATA puQueryMoCompStatusData);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiDdAlphaBlt(__in HANDLE hSurfaceDest, __in_opt HANDLE hSurfaceSrc,
                __inout PDD_BLTDATA puBltData);

// Image32

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiAlphaBlend(__in HDC hdcDst, __in LONG DstX, __in LONG DstY, __in LONG DstCx,
                __in LONG DstCy, __in HDC hdcSrc, __in LONG SrcX, __in LONG SrcY,
                __in LONG SrcCx, __in LONG SrcCy, __in BLENDFUNCTION BlendFunction,
                __in HANDLE hcmXform);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGradientFill(__in HDC hdc, __in_ecount(nVertex) PTRIVERTEX pVertex,
                  __in ULONG nVertex, __in PVOID pMesh, __in ULONG nMesh,
                  __in ULONG ulMode);

// icm (Image Color Matching)
__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetIcmMode(__in HDC hdc, __in ULONG nCommand, __in ULONG ulMode);

#define ICM_SET_MODE 1
#define ICM_SET_CALIBRATE_MODE 2
#define ICM_SET_COLOR_MODE 3
#define ICM_CHECK_COLOR_MODE 4

typedef struct _LOGCOLORSPACEEXW {
    LOGCOLORSPACEW lcsColorSpace;
    DWORD dwFlags;
} LOGCOLORSPACEEXW, *PLOGCOLORSPACEEXW;

#define LCSEX_ANSICREATED 0x0001 // Created by CreateColorSpaceA()
#define LCSEX_TEMPPROFILE 0x0002 // Color profile is temporary file

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiCreateColorSpace(__in PLOGCOLORSPACEEXW pLogColorSpace);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDeleteColorSpace(__in HANDLE hColorSpace);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetColorSpace(__in HDC hdc, __in HCOLORSPACE hColorSpace);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiCreateColorTransform(__in HDC hdc, __in LPLOGCOLORSPACEW pLogColorSpaceW,
                          __in_bcount_opt(cjSrcProfile) PVOID pvSrcProfile,
                          __in ULONG cjSrcProfile,
                          __in_bcount_opt(cjDestProfile) PVOID pvDestProfile,
                          __in ULONG cjDestProfile,
                          __in_bcount_opt(cjTargetProfile) PVOID pvTargetProfile,
                          __in ULONG cjTargetProfile);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDeleteColorTransform(__in HDC hdc, __in HANDLE hColorTransform);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiCheckBitmapBits(__in HDC hdc, __in HANDLE hColorTransform, __in PVOID pvBits,
                     __in ULONG bmFormat, __in DWORD dwWidth, __in DWORD dwHeight,
                     __in DWORD dwStride,
                     __out_bcount(dwWidth *dwHeight) PBYTE paResults);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiColorCorrectPalette(__in HDC hdc, __in HPALETTE hpal, __in ULONG FirstEntry,
                         __in ULONG NumberOfEntries,
                         __inout_ecount(NumberOfEntries) PALETTEENTRY *ppalEntry,
                         __in ULONG Command);

__kernel_entry W32KAPI ULONG_PTR APIENTRY
NtGdiGetColorSpaceforBitmap(__in HBITMAP hsurf);

typedef enum _COLORPALETTEINFO {
    ColorPaletteQuery,
    ColorPaletteSet
} COLORPALETTEINFO,
    *PCOLORPALETTEINFO;

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetDeviceGammaRamp(__in HDC hdc, __out_bcount(256 * 2 * 3) LPVOID lpGammaRamp);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetDeviceGammaRamp(__in HDC hdc, __in_bcount(256 * 2 * 3) LPVOID lpGammaRamp);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiIcmBrushInfo(__in HDC hdc, __in HBRUSH hbrush,
                  __inout_bcount(sizeof(BITMAPINFO) +
                                 ((/*MAX_COLORTABLE*/ 256 - 1) * sizeof(RGBQUAD)))
                      PBITMAPINFO pbmiDIB,
                  __inout_bcount(*pulBits) PVOID pvBits, __inout ULONG *pulBits,
                  __out_opt DWORD *piUsage, __out_opt BOOL *pbAlreadyTran,
                  __in ULONG Command);

typedef enum _ICM_DIB_INFO_CMD {
    IcmQueryBrush,
    IcmSetBrush
} ICM_DIB_INFO,
    *PICM_DIB_INFO;

// PUBLIC

__kernel_entry W32KAPI VOID APIENTRY
NtGdiFlush();

__kernel_entry W32KAPI HDC APIENTRY
NtGdiCreateMetafileDC(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiMakeInfoDC(__in HDC hdc, __in BOOL bSet);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiCreateClientObj(__in ULONG ulType);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDeleteClientObj(__in HANDLE h);

__kernel_entry W32KAPI LONG APIENTRY
NtGdiGetBitmapBits(__in HBITMAP hbm, __in ULONG cjMax,
                   __out_bcount_opt(cjMax) PBYTE pjOut);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDeleteObjectApp(__in HANDLE hobj);

__kernel_entry W32KAPI int APIENTRY
NtGdiGetPath(__in HDC hdc, __out_ecount_opt(cptBuf) LPPOINT pptlBuf,
             __out_ecount_opt(cptBuf) LPBYTE pjTypes, __in int cptBuf);

__kernel_entry W32KAPI HDC APIENTRY
NtGdiCreateCompatibleDC(__in_opt HDC hdc);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiCreateDIBitmapInternal(__in HDC hdc, __in INT cx, __in INT cy, __in DWORD fInit,
                            __in_bcount_opt(cjMaxBits) LPBYTE pjInit,
                            __in_bcount_opt(cjMaxInitInfo) LPBITMAPINFO pbmi,
                            __in DWORD iUsage, __in UINT cjMaxInitInfo,
                            __in UINT cjMaxBits, __in FLONG f, __in HANDLE hcmXform);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiCreateDIBSection(__in_opt HDC hdc, __in_opt HANDLE hSectionApp, __in DWORD dwOffset,
                      __in_bcount_opt(cjHeader) LPBITMAPINFO pbmi, __in DWORD iUsage,
                      __in UINT cjHeader, __in FLONG fl, __in ULONG_PTR dwColorSpace,
                      __deref_out PVOID *ppvBits);

__kernel_entry W32KAPI HBRUSH APIENTRY
NtGdiCreateSolidBrush(__in COLORREF cr, __in_opt HBRUSH hbr);

__kernel_entry W32KAPI HBRUSH APIENTRY
NtGdiCreateDIBBrush(__in_bcount(cj) PVOID pv, __in FLONG fl, __in UINT cj, __in BOOL b8X8,
                    __in BOOL bPen, __in PVOID pClient);

__kernel_entry W32KAPI HBRUSH APIENTRY
NtGdiCreatePatternBrushInternal(__in HBITMAP hbm, __in BOOL bPen, __in BOOL b8X8);

__kernel_entry W32KAPI HBRUSH APIENTRY
NtGdiCreateHatchBrushInternal(__in ULONG ulStyle, __in COLORREF clrr, __in BOOL bPen);

__kernel_entry W32KAPI HPEN APIENTRY
NtGdiExtCreatePen(__in ULONG flPenStyle, __in ULONG ulWidth, __in ULONG iBrushStyle,
                  __in ULONG ulColor, __in ULONG_PTR lClientHatch, __in ULONG_PTR lHatch,
                  __in ULONG cstyle, __in_ecount_opt(cstyle) PULONG pulStyle,
                  __in ULONG cjDIB, __in BOOL bOldStylePen, __in_opt HBRUSH hbrush);

__kernel_entry W32KAPI HRGN APIENTRY
NtGdiCreateEllipticRgn(__in int xLeft, __in int yTop, __in int xRight, __in int yBottom);

__kernel_entry W32KAPI HRGN APIENTRY
NtGdiCreateRoundRectRgn(__in int xLeft, __in int yTop, __in int xRight, __in int yBottom,
                        __in int xWidth, __in int yHeight);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiCreateServerMetaFile(__in DWORD iType, __in ULONG cjData,
                          __in_bcount(cjData) LPBYTE pjData, __in DWORD mm,
                          __in DWORD xExt, __in DWORD yExt);

__kernel_entry W32KAPI HRGN APIENTRY
NtGdiExtCreateRegion(__in_opt LPXFORM px, __in DWORD cj, __in_bcount(cj) LPRGNDATA prgn);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiMakeFontDir(__in FLONG flEmbed, __out_bcount(cjFontDir) PBYTE pjFontDir,
                 __in unsigned cjFontDir, __in_bcount(cjPathname) LPWSTR pwszPathname,
                 __in unsigned cjPathname);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiPolyDraw(__in HDC hdc, __in_ecount(cpt) LPPOINT ppt, __in_ecount(cpt) LPBYTE pjAttr,
              __in ULONG cpt);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiPolyTextOutW(__in HDC hdc, __in_ecount(cStr) POLYTEXTW *pptw, __in UINT cStr,
                  __in DWORD dwCodePage);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiGetServerMetaFileBits(__in HANDLE hmo, __in ULONG cjData,
                           __out_bcount_opt(cjData) LPBYTE pjData, __out PDWORD piType,
                           __out PDWORD pmm, __out PDWORD pxExt, __out PDWORD pyExt);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEqualRgn(__in HRGN hrgn1, __in HRGN hrgn2);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetBitmapDimension(__in HBITMAP hbm, __out LPSIZE psize);

__kernel_entry W32KAPI UINT APIENTRY
NtGdiGetNearestPaletteIndex(__in HPALETTE hpal, __in COLORREF crColor);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiPtVisible(__in HDC hdc, __in int x, __in int y);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiRectVisible(__in HDC hdc, __in LPRECT prc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiRemoveFontResourceW(__in_ecount(cwc) WCHAR *pwszFiles, __in ULONG cwc,
                         __in ULONG cFiles, __in ULONG fl, __in DWORD dwPidTid,
                         __in_opt DESIGNVECTOR *pdv);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiResizePalette(__in HPALETTE hpal, __in UINT cEntry);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetBitmapDimension(__in HBITMAP hbm, __in int cx, __in int cy,
                        __out_opt LPSIZE psizeOut);

__kernel_entry W32KAPI int APIENTRY
NtGdiOffsetClipRgn(__in HDC hdc, __in int x, __in int y);

__kernel_entry W32KAPI int APIENTRY
NtGdiSetMetaRgn(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetTextJustification(__in HDC hdc, __in int lBreakExtra, __in int cBreak);

__kernel_entry W32KAPI int APIENTRY
NtGdiGetAppClipBox(__in HDC hdc, __out LPRECT prc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetTextExtentExW(__in HDC hdc, __in_ecount_opt(cwc) LPWSTR lpwsz, __in ULONG cwc,
                      __in ULONG dxMax,
                      __out_opt ULONG *pcCh, // range(0, cwc)
                      __out_ecount_part_opt(cwc, *pcCh) PULONG pdxOut, __out LPSIZE psize,
                      __in FLONG fl);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetCharABCWidthsW(__in HDC hdc, __in UINT wchFirst, __in ULONG cwch,
                       __in_ecount_opt(cwch) PWCHAR pwch, __in FLONG fl,
                       __out_bcount(cwch * sizeof(ABC)) __typefix(ABC *) PVOID pvBuf);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiGetCharacterPlacementW(__in HDC hdc, __in_ecount(nCount) LPWSTR pwsz,
                            __in int nCount, __in int nMaxExtent,
                            __inout LPGCP_RESULTSW pgcpw, __in DWORD dwFlags);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiAngleArc(__in HDC hdc, __in int x, __in int y, __in DWORD dwRadius,
              __in DWORD dwStartAngle, __in DWORD dwSweepAngle);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiBeginPath(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSelectClipPath(__in HDC hdc, __in int iMode);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiCloseFigure(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEndPath(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiAbortPath(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiFillPath(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiStrokeAndFillPath(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiStrokePath(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiWidenPath(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiFlattenPath(__in HDC hdc);

__kernel_entry W32KAPI HRGN APIENTRY
NtGdiPathToRegion(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetMiterLimit(__in HDC hdc, __in DWORD dwNew, __inout_opt PDWORD pdwOut);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetFontXform(__in HDC hdc, __in DWORD dwxScale, __in DWORD dwyScale);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetMiterLimit(__in HDC hdc, __out PDWORD pdwOut);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEllipse(__in HDC hdc, __in int xLeft, __in int yTop, __in int xRight,
             __in int yBottom);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiRectangle(__in HDC hdc, __in int xLeft, __in int yTop, __in int xRight,
               __in int yBottom);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiRoundRect(__in HDC hdc, __in int x1, __in int y1, __in int x2, __in int y2,
               __in int x3, __in int y3);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiPlgBlt(__in HDC hdcTrg, __in_ecount(3) LPPOINT pptlTrg, __in HDC hdcSrc,
            __in int xSrc, __in int ySrc, __in int cxSrc, __in int cySrc,
            __in_opt HBITMAP hbmMask, __in int xMask, __in int yMask,
            __in DWORD crBackColor);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiMaskBlt(__in HDC hdc, __in int xDst, __in int yDst, __in int cx, __in int cy,
             __in HDC hdcSrc, __in int xSrc, __in int ySrc, __in HBITMAP hbmMask,
             __in int xMask, __in int yMask, __in DWORD dwRop4, __in DWORD crBackColor);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiExtFloodFill(__in HDC hdc, __in INT x, __in INT y, __in COLORREF crColor,
                  __in UINT iFillType);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiFillRgn(__in HDC hdc, __in HRGN hrgn, __in HBRUSH hbrush);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiFrameRgn(__in HDC hdc, __in HRGN hrgn, __in HBRUSH hbrush, __in int xWidth,
              __in int yHeight);

__kernel_entry W32KAPI COLORREF APIENTRY
NtGdiSetPixel(__in HDC hdcDst, __in int x, __in int y, __in COLORREF crColor);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiGetPixel(__in HDC hdc, __in int x, __in int y);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiStartPage(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEndPage(__in HDC hdc);

__kernel_entry W32KAPI int APIENTRY
NtGdiStartDoc(__in HDC hdc, __in DOCINFOW *pdi, __out BOOL *pbBanding, __in INT iJob);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEndDoc(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiAbortDoc(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiUpdateColors(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetCharWidthW(__in HDC hdc, __in UINT wcFirst, __in UINT cwc,
                   __in_ecount_opt(cwc) PWCHAR pwc, __in FLONG fl,
                   __out_bcount(cwc * sizeof(ULONG)) __typefix(PULONG) PVOID pvBuf);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetCharWidthInfo(__in HDC hdc, __out PCHWIDTHINFO pChWidthInfo);

__kernel_entry W32KAPI int APIENTRY
NtGdiDrawEscape(__in HDC hdc, __in int iEsc, __in int cjIn,
                __in_bcount_opt(cjIn) LPSTR pjIn);

__kernel_entry W32KAPI int APIENTRY
NtGdiExtEscape(__in_opt HDC hdc, __in_ecount_opt(nDriver) PWCHAR pDriver,
               __in int nDriver, __in int iEsc, __in int cjIn,
               __in_bcount_opt(cjIn) LPSTR pjIn, __in int cjOut,
               __out_bcount_opt(cjOut) LPSTR pjOut);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiGetFontData(__in HDC hdc, __in DWORD dwTable, __in DWORD dwOffset,
                 __out_bcount_part_opt(cjBuf, return ) PVOID pvBuf, __in ULONG cjBuf);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiGetFontFileData(__in UINT uFileCollectionID, __in UINT uFileIndex,
                     __in ULONGLONG *pullFileOffset, __out_bcount(cbSize) void *pBuffer,
                     __in SIZE_T cbSize);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiGetFontFileInfo(__in UINT uFileCollectionID, __in UINT uFileIndex,
                     __out_bcount(cbSize) FONT_FILE_INFO *pfi, __in SIZE_T cbSize,
                     __out_opt SIZE_T *pcbActualSize);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiGetGlyphOutline(__in HDC hdc, __in WCHAR wch, __in UINT iFormat,
                     __out LPGLYPHMETRICS pgm, __in ULONG cjBuf,
                     __out_bcount_opt(cjBuf) PVOID pvBuf, __in LPMAT2 pmat2,
                     __in BOOL bIgnoreRotation);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetETM(__in HDC hdc, __out_opt EXTTEXTMETRIC *petm);

__kernel_entry W32KAPI __success(return == 1) BOOL APIENTRY
    NtGdiGetRasterizerCaps(__out_bcount(cjBytes) LPRASTERIZER_STATUS praststat,
                           __in ULONG cjBytes);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiGetKerningPairs(__in HDC hdc, __in ULONG cPairs,
                     __out_ecount_part_opt(cPairs, return ) KERNINGPAIR *pkpDst);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiMonoBitmap(__in HBITMAP hbm);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiGetObjectBitmapHandle(__in HBRUSH hbr, __out UINT *piUsage);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiEnumObjects(__in HDC hdc, __in int iObjectType, __in ULONG cjBuf,
                 __out_bcount_opt(cjBuf) PVOID pvBuf);

//
// NtGdiResetDC
//
// The actual size of the buffer at pdm is pdm->dmSize + pdm->dmDriverExtra.
// But this can not be specified with current annotation language.
//

typedef struct _DRIVER_INFO_2W DRIVER_INFO_2W;

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiResetDC(__in HDC hdc, __in LPDEVMODEW pdm, __out PBOOL pbBanding,
             __in_opt DRIVER_INFO_2W *pDriverInfo2,
             __out __typefix(PUMDHPDEV *) VOID *ppUMdhpdev);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiSetBoundsRect(__in HDC hdc, __in LPRECT prc, __in DWORD f);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetColorAdjustment(__in HDC hdc, __out PCOLORADJUSTMENT pcaOut);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetColorAdjustment(__in HDC hdc, __in PCOLORADJUSTMENT pca);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiCancelDC(__in HDC hdc);

//
// See comment on NtGdiResetDC
//

__kernel_entry W32KAPI HDC APIENTRY
NtGdiOpenDCW(__in_opt PUNICODE_STRING pustrDevice, __in DEVMODEW *pdm,
             __in PUNICODE_STRING pustrLogAddr, __in ULONG iType, __in BOOL bDisplay,
             __in_opt HANDLE hspool, __in_opt DRIVER_INFO_2W *pDriverInfo2,
             __out __typefix(PUMDHPDEV *) VOID *pUMdhpdev);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetDCDword(__in HDC hdc, __in UINT u, __out DWORD *Result);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetDCPoint(__in HDC hdc, __in UINT iPoint, __out PPOINTL pptOut);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiScaleViewportExtEx(__in HDC hdc, __in int xNum, __in int xDenom, __in int yNum,
                        __in int yDenom, __out_opt LPSIZE pszOut);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiScaleWindowExtEx(__in HDC hdc, __in int xNum, __in int xDenom, __in int yNum,
                      __in int yDenom, __out_opt LPSIZE pszOut);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetVirtualResolution(__in HDC hdc, __in int cxVirtualDevicePixel,
                          __in int cyVirtualDevicePixel, __in int cxVirtualDeviceMm,
                          __in int cyVirtualDeviceMm);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetSizeDevice(__in HDC hdc, __in int cxVirtualDevice, __in int cyVirtualDevice);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetTransform(__in HDC hdc, __in DWORD iXform, __out LPXFORM pxf);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiModifyWorldTransform(__in HDC hdc, __in_opt LPXFORM pxf, __in DWORD iXform);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiCombineTransform(__out LPXFORM pxfDst, __in LPXFORM pxfSrc1, __in LPXFORM pxfSrc2);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiTransformPoints(__in HDC hdc, __in_ecount(c) PPOINT pptIn,
                     __out_ecount(c) PPOINT pptOut, __in int c, __in int iMode);

__kernel_entry W32KAPI LONG APIENTRY
NtGdiConvertMetafileRect(__in HDC hdc, __inout PRECTL prect);

__kernel_entry W32KAPI int APIENTRY
NtGdiGetTextCharsetInfo(__in HDC hdc, __out_opt LPFONTSIGNATURE lpSig,
                        __in DWORD dwFlags);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDoBanding(__in HDC hdc, __in BOOL bStart, __out POINTL *pptl, __out PSIZE pSize);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiGetPerBandInfo(__in HDC hdc, __inout PERBANDINFO *ppbi);

#define GS_NUM_OBJS_ALL 0
#define GS_HANDOBJ_CURRENT 1
#define GS_HANDOBJ_MAX 2
#define GS_HANDOBJ_ALLOC 3
#define GS_LOOKASIDE_INFO 4

__kernel_entry W32KAPI NTSTATUS APIENTRY
NtGdiGetStats(__in HANDLE hProcess, __in int iIndex, __in int iPidType,
              __out_bcount(cjResultSize) PVOID pResults, __in UINT cjResultSize);

// API's used by USER
__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetMagicColors(__in HDC hdc, __in PALETTEENTRY peMagic, __in ULONG Index);

__kernel_entry W32KAPI HBRUSH APIENTRY
NtGdiSelectBrush(__in HDC hdc, __in HBRUSH hbrush);

__kernel_entry W32KAPI HPEN APIENTRY
NtGdiSelectPen(__in HDC hdc, __in HPEN hpen);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiSelectBitmap(__in HDC hdc, __in HBITMAP hbm);

__kernel_entry W32KAPI HFONT APIENTRY
NtGdiSelectFont(__in HDC hdc, __in HFONT hf);

__kernel_entry W32KAPI int APIENTRY
NtGdiExtSelectClipRgn(__in HDC hdc, __in_opt HRGN hrgn, __in int iMode);

__kernel_entry W32KAPI HPEN APIENTRY
NtGdiCreatePen(__in int iPenStyle, __in int iPenWidth, __in COLORREF cr,
               __in_opt HBRUSH hbr);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiBitBlt(__in HDC hdcDst, __in int x, __in int y, __in int cx, __in int cy,
            __in_opt HDC hdcSrc, __in int xSrc, __in int ySrc, __in DWORD rop4,
            __in DWORD crBackColor, __in FLONG fl);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiTileBitBlt(__in HDC hdcDst, __in RECTL *prectDst, __in HDC hdcSrc,
                __in RECTL *prectSrc, __in POINTL *pptlOrigin, __in DWORD rop4,
                __in DWORD crBackColor);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiTransparentBlt(__in HDC hdcDst, __in int xDst, __in int yDst, __in int cxDst,
                    __in int cyDst, __in HDC hdcSrc, __in int xSrc, __in int ySrc,
                    __in int cxSrc, __in int cySrc, __in COLORREF TransColor);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetTextExtent(__in HDC hdc, __in_ecount(cwc) LPWSTR lpwsz, __in int cwc,
                   __out LPSIZE psize, __in UINT flOpts);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetTextMetricsW(__in HDC hdc, __out_bcount(cj) TMW_INTERNAL *ptm, __in ULONG cj);

__kernel_entry W32KAPI int APIENTRY
NtGdiGetTextFaceW(__in HDC hdc, __in int cChar,
                  __out_ecount_part_opt(cChar, return ) LPWSTR pszOut,
                  __in BOOL bAliasName);

__kernel_entry W32KAPI int APIENTRY
NtGdiGetRandomRgn(__in HDC hdc, __in HRGN hrgn, __in int iRgn);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiExtTextOutW(__in HDC hdc, __in int x, __in int y, __in UINT flOpts,
                 __in_opt LPRECT prcl, __in_ecount_opt(cwc) LPWSTR pwsz,
                 __in_range(0, 0xffff) int cwc, __in_xcount_opt(cwc) LPINT pdx,
                 __in DWORD dwCodePage);

__kernel_entry W32KAPI int APIENTRY
NtGdiIntersectClipRect(__in HDC hdc, __in int xLeft, __in int yTop, __in int xRight,
                       __in int yBottom);

__kernel_entry W32KAPI HRGN APIENTRY
NtGdiCreateRectRgn(__in int xLeft, __in int yTop, __in int xRight, __in int yBottom);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiPatBlt(__in HDC hdcDst, __in int x, __in int y, __in int cx, __in int cy,
            __in DWORD rop4);

typedef struct _POLYPATBLT POLYPATBLT, *PPOLYPATBLT;
__kernel_entry W32KAPI BOOL APIENTRY
NtGdiPolyPatBlt(__in HDC hdc, __in DWORD rop4, __in_ecount(Count) PPOLYPATBLT pPoly,
                __in DWORD Count, __in DWORD Mode);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiUnrealizeObject(__in HANDLE h);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiGetStockObject(__in int iObject);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiCreateCompatibleBitmap(__in HDC hdc, __in int cx, __in int cy);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiCreateBitmapFromDxSurface(__in HDC hdc, __in UINT uiWidth, __in UINT uiHeight,
                               __in DWORD Format, __in_opt HANDLE hDxSharedSurface);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiBeginGdiRendering(__in HBITMAP hbm, __in BOOL bDiscard);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEndGdiRendering(__in HBITMAP hbm, __in BOOL bDiscard, __out BOOL *pbDeviceRemoved);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiLineTo(__in HDC hdc, __in int x, __in int y);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiMoveTo(__in HDC hdc, __in int x, __in int y, __out_opt LPPOINT pptOut);

__kernel_entry W32KAPI int APIENTRY
NtGdiExtGetObjectW(__in HANDLE h, __in int cj, __out_bcount_opt(cj) LPVOID pvOut);

__kernel_entry W32KAPI int APIENTRY
NtGdiGetDeviceCaps(__in HDC hdc, __in int i);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiGetDeviceCapsAll(__in_opt HDC hdc, __out PDEVCAPS pDevCaps);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiStretchBlt(__in HDC hdcDst, __in int xDst, __in int yDst, __in int cxDst,
                __in int cyDst, __in_opt HDC hdcSrc, __in int xSrc, __in int ySrc,
                __in int cxSrc, __in int cySrc, __in DWORD dwRop, __in DWORD dwBackColor);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetBrushOrg(__in HDC hdc, __in int x, __in int y, __out LPPOINT pptOut);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiCreateBitmap(__in int cx, __in int cy, __in UINT cPlanes, __in UINT cBPP,
                  __in_opt LPBYTE pjInit);

__kernel_entry W32KAPI HPALETTE APIENTRY
NtGdiCreateHalftonePalette(__in HDC hdc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiRestoreDC(__in HDC hdc, __in int iLevel);

__kernel_entry W32KAPI int APIENTRY
NtGdiExcludeClipRect(__in HDC hdc, __in int xLeft, __in int yTop, __in int xRight,
                     __in int yBottom);

__kernel_entry W32KAPI int APIENTRY
NtGdiSaveDC(__in HDC hdc);

__kernel_entry W32KAPI int APIENTRY
NtGdiCombineRgn(__in HRGN hrgnDst, __in HRGN hrgnSrc1, __in HRGN hrgnSrc2,
                __in int iMode);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSetRectRgn(__in HRGN hrgn, __in int xLeft, __in int yTop, __in int xRight,
                __in int yBottom);

__kernel_entry W32KAPI LONG APIENTRY
NtGdiSetBitmapBits(__in HBITMAP hbm, __in ULONG cj, __in_bcount(cj) PBYTE pjInit);

__kernel_entry W32KAPI int APIENTRY
NtGdiGetDIBitsInternal(__in HDC hdc, __in HBITMAP hbm, __in UINT iStartScan,
                       __in UINT cScans, __out_bcount_opt(cjMaxBits) LPBYTE pBits,
                       __inout LPBITMAPINFO pbmi, __in UINT iUsage, __in UINT cjMaxBits,
                       __in UINT cjMaxInfo);

__kernel_entry W32KAPI int APIENTRY
NtGdiOffsetRgn(__in HRGN hrgn, __in int cx, __in int cy);

__kernel_entry W32KAPI int APIENTRY
NtGdiGetRgnBox(__in HRGN hrgn, __out LPRECT prcOut);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiRectInRegion(__in HRGN hrgn, __inout LPRECT prcl);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiGetBoundsRect(__in HDC hdc, __out LPRECT prc, __in DWORD f);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiPtInRegion(__in HRGN hrgn, __in int x, __in int y);

__kernel_entry W32KAPI COLORREF APIENTRY
NtGdiGetNearestColor(__in HDC hdc, __in COLORREF cr);

__kernel_entry W32KAPI UINT APIENTRY
NtGdiGetSystemPaletteUse(__in HDC hdc);

__kernel_entry W32KAPI UINT APIENTRY
NtGdiSetSystemPaletteUse(__in HDC hdc, __in UINT ui);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiGetRegionData(__in HRGN hrgn, __in DWORD nCount,
                   __out_bcount_part_opt(nCount, return ) LPRGNDATA lpRgnData);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiInvertRgn(__in HDC hdc, __in HRGN hrgn);

// MISC FONT API's

__kernel_entry int W32KAPI APIENTRY
NtGdiAddFontResourceW(__in_ecount(cwc) WCHAR *pwszFiles, __in ULONG cwc,
                      __in ULONG cFiles, __in FLONG f, __in DWORD dwPidTid,
                      __in_opt DESIGNVECTOR *pdv);

#if (_WIN32_WINNT >= 0x0500)
__kernel_entry W32KAPI HFONT APIENTRY
NtGdiHfontCreate(__in_bcount(cjElfw) ENUMLOGFONTEXDVW *pelfw, __in ULONG cjElfw,
                 __in LFTYPE lft, __in FLONG fl, __in PVOID pvCliData);

#else
__kernel_entry W32KAPI HFONT APIENTRY
NtGdiHfontCreate(__in LPEXTLOGFONTW pelfw, __in ULONG cjElfw, __in LFTYPE lft,
                 __in FLONG fl, __in PVOID pvCliData);

#endif

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiSetFontEnumeration(__in ULONG ulType);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEnumFonts(__in HDC hdc, __in ULONG iEnumType, __in FLONG flWin31Compat,
               __in ULONG cchFaceName, __in_ecount_opt(cchFaceName) LPCWSTR pwszFaceName,
               __in ULONG lfCharSet, __inout ULONG *pulCount,
               __out_bcount_opt(*pulCount) void *pvUserModeBuffer);

#define TYPE_ENUMFONTS 1
#define TYPE_ENUMFONTFAMILIES 2
#define TYPE_ENUMFONTFAMILIESEX 3

__kernel_entry W32KAPI INT APIENTRY
NtGdiQueryFonts(__out_ecount(nBufferSize) PUNIVERSAL_FONT_ID pufiFontList,
                __in ULONG nBufferSize, __out PLARGE_INTEGER pTimeStamp);

// needed for win95 functionality

__kernel_entry W32KAPI DWORD
NtGdiGetCharSet(__in HDC hdc);

// needed for fontlinking

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEnableEudc(__in BOOL);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEudcLoadUnloadLink(__in_ecount_opt(cwcBaseFaceName) LPCWSTR pBaseFaceName,
                        __in UINT cwcBaseFaceName,
                        __in_ecount(cwcEudcFontPath) LPCWSTR pEudcFontPath,
                        __in UINT cwcEudcFontPath, __in INT iPriority,
                        __in INT iFontLinkType, __in BOOL bLoadLin);

__kernel_entry W32KAPI UINT APIENTRY
NtGdiGetStringBitmapW(__in HDC hdc, __in LPWSTR pwsz, __in UINT cwc, __in UINT cj,
                      __out_bcount(cj) BYTE *lpSB);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiGetEudcTimeStampEx(__in_ecount_opt(cwcBaseFaceName) LPWSTR lpBaseFaceName,
                        __in ULONG cwcBaseFaceName, __in BOOL bSystemTimeStamp);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiQueryFontAssocInfo(__in HDC hdc);

#if (_WIN32_WINNT >= 0x0500)
__kernel_entry W32KAPI DWORD
NtGdiGetFontUnicodeRanges(__in HDC hdc, __out_post_bcount_opt(return ) LPGLYPHSET pgs);

#endif

__kernel_entry W32KAPI BOOL
NtGdiGetRealizationInfo(__in HDC hdc, __out PFONT_REALIZATION_INFO pri);

typedef struct tagDOWNLOADDESIGNVECTOR {
    UNIVERSAL_FONT_ID ufiBase;
    DESIGNVECTOR dv;
} DOWNLOADDESIGNVECTOR;

__kernel_entry W32KAPI BOOL
NtGdiAddRemoteMMInstanceToDC(__in HDC hdc, __in_bcount(cjDDV) DOWNLOADDESIGNVECTOR *pddv,
                             __in ULONG cjDDV);

// user-mode printer support

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiUnloadPrinterDriver(__in_bcount(cbDriverName) LPWSTR pDriverName,
                         __in ULONG cbDriverName);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngAssociateSurface(__in HSURF hsurf, __in HDEV hdev, __in FLONG flHooks);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngEraseSurface(__in SURFOBJ *pso, __in RECTL *prcl, __in ULONG iColor);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiEngCreateBitmap(__in SIZEL sizl, __in LONG lWidth, __in ULONG iFormat, __in FLONG fl,
                     __in_opt PVOID pvBits);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngDeleteSurface(__in HSURF hsurf);

__kernel_entry W32KAPI SURFOBJ *APIENTRY
NtGdiEngLockSurface(__in HSURF hsurf);

__kernel_entry W32KAPI VOID APIENTRY
NtGdiEngUnlockSurface(__in SURFOBJ *);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngMarkBandingSurface(__in HSURF hsurf);

__kernel_entry W32KAPI HSURF APIENTRY
NtGdiEngCreateDeviceSurface(__in DHSURF dhsurf, __in SIZEL sizl,
                            __in ULONG iFormatCompat);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiEngCreateDeviceBitmap(__in DHSURF dhsurf, __in SIZEL sizl, __in ULONG iFormatCompat);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngCopyBits(__in SURFOBJ *psoDst, __in SURFOBJ *psoSrc, __in_opt CLIPOBJ *pco,
                 __in XLATEOBJ *pxlo, __in RECTL *prclDst, __in POINTL *pptlSrc);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngStretchBlt(__in SURFOBJ *psoDest, __in SURFOBJ *psoSrc, __in SURFOBJ *psoMask,
                   __in CLIPOBJ *pco, __in XLATEOBJ *pxlo, __in COLORADJUSTMENT *pca,
                   __in POINTL *pptlHTOrg, __in RECTL *prclDest, __in RECTL *prclSrc,
                   __in POINTL *pptlMask, __in ULONG iMode);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngBitBlt(__in SURFOBJ *psoDst, __in SURFOBJ *psoSrc, __in SURFOBJ *psoMask,
               __in CLIPOBJ *pco, __in XLATEOBJ *pxlo, __in RECTL *prclDst,
               __in POINTL *pptlSrc, __in POINTL *pptlMask, __in BRUSHOBJ *pbo,
               __in POINTL *pptlBrush, __in ROP4 rop4);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngPlgBlt(__in SURFOBJ *psoTrg, __in SURFOBJ *psoSrc, __in_opt SURFOBJ *psoMsk,
               __in CLIPOBJ *pco, __in XLATEOBJ *pxlo, __in COLORADJUSTMENT *pca,
               __in POINTL *pptlBrushOrg, __in POINTFIX *pptfxDest, __in RECTL *prclSrc,
               __in_opt POINTL *pptlMask, __in ULONG iMode);

__kernel_entry W32KAPI HPALETTE APIENTRY
NtGdiEngCreatePalette(__in ULONG iMode, __in ULONG cColors, __in ULONG *pulColors,
                      __in FLONG flRed, __in FLONG flGreen, __in FLONG flBlue);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngDeletePalette(__in HPALETTE hPal);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngStrokePath(__in SURFOBJ *pso, __in PATHOBJ *ppo, __in CLIPOBJ *pco,
                   __in XFORMOBJ *pxo, __in BRUSHOBJ *pbo, __in POINTL *pptlBrushOrg,
                   __in LINEATTRS *plineattrs, __in MIX mix);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngFillPath(__in SURFOBJ *pso, __in PATHOBJ *ppo, __in CLIPOBJ *pco,
                 __in BRUSHOBJ *pbo, __in POINTL *pptlBrushOrg, __in MIX mix,
                 __in FLONG flOptions);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngStrokeAndFillPath(__in SURFOBJ *pso, __in PATHOBJ *ppo, __in CLIPOBJ *pco,
                          __in XFORMOBJ *pxo, __in BRUSHOBJ *pboStroke,
                          __in LINEATTRS *plineattrs, __in BRUSHOBJ *pboFill,
                          __in POINTL *pptlBrushOrg, __in MIX mix, __in FLONG flOptions);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngPaint(__in SURFOBJ *pso, __in CLIPOBJ *pco, __in BRUSHOBJ *pbo,
              __in POINTL *pptlBrushOrg, __in MIX mix);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngLineTo(__in SURFOBJ *pso, __in CLIPOBJ *pco, __in BRUSHOBJ *pbo, __in LONG x1,
               __in LONG y1, __in LONG x2, __in LONG y2, __in RECTL *prclBounds,
               __in MIX mix);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngAlphaBlend(__in SURFOBJ *psoDest, __in SURFOBJ *psoSrc, __in CLIPOBJ *pco,
                   __in XLATEOBJ *pxlo, __in RECTL *prclDest, __in RECTL *prclSrc,
                   __in BLENDOBJ *pBlendObj);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngGradientFill(__in SURFOBJ *psoDest, __in CLIPOBJ *pco, __in XLATEOBJ *pxlo,
                     __in_ecount(nVertex) TRIVERTEX *pVertex, __in ULONG nVertex,
                     __in_ecount(nMesh) PVOID pMesh, __in ULONG nMesh,
                     __in RECTL *prclExtents, __in POINTL *pptlDitherOrg,
                     __in ULONG ulMode);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngTransparentBlt(__in SURFOBJ *psoDst, __in SURFOBJ *psoSrc, __in CLIPOBJ *pco,
                       __in XLATEOBJ *pxlo, __in RECTL *prclDst, __in RECTL *prclSrc,
                       __in ULONG iTransColor, __in ULONG ulReserved);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngTextOut(__in SURFOBJ *pso, __in STROBJ *pstro, __in FONTOBJ *pfo,
                __in CLIPOBJ *pco, __in RECTL *prclExtra, __in RECTL *prclOpaque,
                __in BRUSHOBJ *pboFore, __in BRUSHOBJ *pboOpaque, __in POINTL *pptlOrg,
                __in MIX mix);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngStretchBltROP(__in SURFOBJ *psoTrg, __in SURFOBJ *psoSrc, __in SURFOBJ *psoMask,
                      __in CLIPOBJ *pco, __in XLATEOBJ *pxlo, __in COLORADJUSTMENT *pca,
                      __in POINTL *pptlBrushOrg, __in RECTL *prclTrg, __in RECTL *prclSrc,
                      __in POINTL *pptlMask, __in ULONG iMode, __in BRUSHOBJ *pbo,
                      __in ROP4 rop4);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiXLATEOBJ_cGetPalette(__in XLATEOBJ *pxlo, __in ULONG iPal, __in ULONG cPal,
                          __out_ecount(cPal) ULONG *pPal);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiCLIPOBJ_cEnumStart(__in CLIPOBJ *pco, __in BOOL bAll, __in ULONG iType,
                        __in ULONG iDirection, __in ULONG cLimit);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiCLIPOBJ_bEnum(__in CLIPOBJ *pco, __in ULONG cj, __out_bcount(cj) ULONG *pul);

__kernel_entry W32KAPI PATHOBJ *APIENTRY
NtGdiCLIPOBJ_ppoGetPath(__in CLIPOBJ *pco);

__kernel_entry W32KAPI CLIPOBJ *APIENTRY
NtGdiEngCreateClip();

__kernel_entry W32KAPI VOID APIENTRY
NtGdiEngDeleteClip(__in CLIPOBJ *pco);

__kernel_entry W32KAPI PVOID APIENTRY
NtGdiBRUSHOBJ_pvAllocRbrush(__in BRUSHOBJ *pbo, __in ULONG cj);

__kernel_entry W32KAPI PVOID APIENTRY
NtGdiBRUSHOBJ_pvGetRbrush(__in BRUSHOBJ *pbo);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiBRUSHOBJ_ulGetBrushColor(__in BRUSHOBJ *pbo);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiBRUSHOBJ_hGetColorTransform(__in BRUSHOBJ *pbo);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiXFORMOBJ_bApplyXform(__in XFORMOBJ *pxo, __in ULONG iMode, __in ULONG cPoints,
                          __in_ecount(cPoints) POINTL *pvIn,
                          __out_ecount(cPoints) POINTL *pvOut);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiXFORMOBJ_iGetXform(__in XFORMOBJ *pxo, __out_opt XFORML *pxform);

__kernel_entry W32KAPI VOID APIENTRY
NtGdiFONTOBJ_vGetInfo(__in FONTOBJ *pfo, __in ULONG cjSize,
                      __out_bcount(cjSize) FONTINFO *pfi);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiFONTOBJ_cGetGlyphs(__in FONTOBJ *pfo, __in ULONG iMode, __in ULONG cGlyph,
                        __in HGLYPH *phg,
                        __deref_out __typefix(GLYPHDATA **) PVOID *ppvGlyph);

__kernel_entry W32KAPI XFORMOBJ *APIENTRY
NtGdiFONTOBJ_pxoGetXform(__in FONTOBJ *pfo);

__kernel_entry W32KAPI IFIMETRICS *APIENTRY
NtGdiFONTOBJ_pifi(__in FONTOBJ *pfo);

__kernel_entry W32KAPI FD_GLYPHSET *APIENTRY
NtGdiFONTOBJ_pfdg(__in FONTOBJ *pfo);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiFONTOBJ_cGetAllGlyphHandles(__in FONTOBJ *pfo,
                                 __out_post_ecount_opt(return ) HGLYPH *phg);

__kernel_entry W32KAPI PVOID APIENTRY
NtGdiFONTOBJ_pvTrueTypeFontFile(__in FONTOBJ *pfo, __out ULONG *pcjFile);

__kernel_entry W32KAPI PFD_GLYPHATTR APIENTRY
NtGdiFONTOBJ_pQueryGlyphAttrs(__in FONTOBJ *pfo, __in ULONG iMode);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSTROBJ_bEnum(__in STROBJ *pstro, __out ULONG *pc,
                  __deref_out_ecount(*pc) PGLYPHPOS *ppgpos);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSTROBJ_bEnumPositionsOnly(__in STROBJ *pstro, __out ULONG *pc,
                               __deref_out_ecount(*pc) PGLYPHPOS *ppgpos);

__kernel_entry W32KAPI VOID APIENTRY
NtGdiSTROBJ_vEnumStart(__in STROBJ *pstro);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiSTROBJ_dwGetCodePage(__in STROBJ *pstro);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiSTROBJ_bGetAdvanceWidths(__in STROBJ *pstro, __in ULONG iFirst, __in ULONG c,
                              __out_ecount(c) POINTQF *pptqD);

__kernel_entry W32KAPI FD_GLYPHSET *APIENTRY
NtGdiEngComputeGlyphSet(__in INT nCodePage, __in INT nFirstChar, __in INT cChars);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiXLATEOBJ_iXlate(__in XLATEOBJ *pxlo, __in ULONG iColor);

__kernel_entry W32KAPI HANDLE APIENTRY
NtGdiXLATEOBJ_hGetColorTransform(__in XLATEOBJ *pxlo);

__kernel_entry W32KAPI VOID APIENTRY
NtGdiPATHOBJ_vGetBounds(__in PATHOBJ *ppo, __out PRECTFX prectfx);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiPATHOBJ_bEnum(__in PATHOBJ *ppo, __out PATHDATA *ppd);

__kernel_entry W32KAPI VOID APIENTRY
NtGdiPATHOBJ_vEnumStart(__in PATHOBJ *ppo);

__kernel_entry W32KAPI VOID APIENTRY
NtGdiEngDeletePath(__in PATHOBJ *ppo);

__kernel_entry W32KAPI VOID APIENTRY
NtGdiPATHOBJ_vEnumStartClipLines(__in PATHOBJ *ppo, __in CLIPOBJ *pco, __in SURFOBJ *pso,
                                 __in LINEATTRS *pla);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiPATHOBJ_bEnumClipLines(__in PATHOBJ *ppo, __in ULONG cb,
                            __out_bcount(cb) CLIPLINE *pcl);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiEngCheckAbort(__in SURFOBJ *pso);

__kernel_entry W32KAPI DHPDEV
NtGdiGetDhpdev(__in HDEV hdev);

__kernel_entry W32KAPI LONG APIENTRY
NtGdiHT_Get8BPPFormatPalette(__out_post_ecount_opt(return ) LPPALETTEENTRY pPaletteEntry,
                             __in USHORT RedGamma, __in USHORT GreenGamma,
                             __in USHORT BlueGamma);

__kernel_entry W32KAPI LONG APIENTRY
NtGdiHT_Get8BPPMaskPalette(__out_post_ecount_opt(return ) LPPALETTEENTRY pPaletteEntry,
                           __in BOOL Use8BPPMaskPal, __in BYTE CMYMask,
                           __in USHORT RedGamma, __in USHORT GreenGamma,
                           __in USHORT BlueGamma);

__kernel_entry W32KAPI BOOL
NtGdiUpdateTransform(__in HDC hdc);

__kernel_entry W32KAPI DWORD APIENTRY
NtGdiSetLayout(__in HDC hdc, __in LONG wox, __in DWORD dwLayout);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiMirrorWindowOrg(__in HDC hdc);

__kernel_entry W32KAPI LONG APIENTRY
NtGdiGetDeviceWidth(__in HDC hdc);

__kernel_entry W32KAPI BOOL
NtGdiSetPUMPDOBJ(__in_opt HUMPD humpd, __in BOOL bStoreID, __inout_opt HUMPD *phumpd,
                 __out_opt BOOL *pbWOW64);

__kernel_entry W32KAPI BOOL
NtGdiBRUSHOBJ_DeleteRbrush(__in_opt BRUSHOBJ *pbo, __in_opt BRUSHOBJ *pboB);

__kernel_entry W32KAPI BOOL
NtGdiUMPDEngFreeUserMem(__in KERNEL_PVOID *ppv);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiSetBitmapAttributes(__in HBITMAP hbm, __in DWORD dwFlags);

__kernel_entry W32KAPI HBITMAP APIENTRY
NtGdiClearBitmapAttributes(__in HBITMAP hbm, __in DWORD dwFlags);

__kernel_entry W32KAPI HBRUSH APIENTRY
NtGdiSetBrushAttributes(__in HBRUSH hbm, __in DWORD dwFlags);

__kernel_entry W32KAPI HBRUSH APIENTRY
NtGdiClearBrushAttributes(__in HBRUSH hbr, __in DWORD dwFlags);

// Private draw stream interface

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiDrawStream(__in HDC hdcDst, __in ULONG cjIn, __in_bcount(cjIn) VOID *pvIn);

// Private Xfer interfaces

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiMakeObjectXferable(__in HANDLE h, __in DWORD dwProcessId);
__kernel_entry W32KAPI BOOL APIENTRY
NtGdiMakeObjectUnXferable(__in HANDLE h);

// Private DWM interfaces

__kernel_entry W32KAPI BOOL
NtGdiSfmGetNotificationTokens(
    __in UINT cBytes, __out UINT *pNrOfTokensReturned,
    __out_bcount(cBytes)
        PVOID pTokenBuffer); // XXX(bruening): ecount_part(,*pNrOfTokensReturned)?

__kernel_entry W32KAPI BOOL
NtGdiSfmRegisterLogicalSurfaceForSignaling(__in HLSURF hlsurf, __in BOOL fSignalOnDirty);

__kernel_entry W32KAPI BOOL
NtGdiDwmGetHighColorMode(__out DXGI_FORMAT *pdxgiFormat);

__kernel_entry W32KAPI BOOL
NtGdiDwmSetHighColorMode(__in DXGI_FORMAT dxgiFormat);

__kernel_entry W32KAPI HANDLE
NtGdiDwmCaptureScreen(__in const RECT *prcCapture, __in DXGI_FORMAT dxgiFormat);

__kernel_entry W32KAPI NTSTATUS APIENTRY
NtGdiDdCreateFullscreenSprite(__in HDC hdc, __in COLORREF crKey, __out HANDLE *phSprite,
                              __out HDC *phdcSprite);

__kernel_entry W32KAPI NTSTATUS APIENTRY
NtGdiDdNotifyFullscreenSpriteUpdate(__in HDC hdc, __in HANDLE hSprite);

__kernel_entry W32KAPI NTSTATUS APIENTRY
NtGdiDdDestroyFullscreenSprite(__in HDC hdc, __in HANDLE hSprite);

__kernel_entry W32KAPI ULONG APIENTRY
NtGdiDdQueryVisRgnUniqueness();

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiHLSurfGetInformation(__in HLSURF hLsurf,
                          __in HLSURF_INFORMATION_CLASS InformationClass,
                          __in_bcount_opt(*pulInformationLength) PVOID InformationBuffer,
                          __inout PULONG pulInformationLength);

__kernel_entry W32KAPI BOOL APIENTRY
NtGdiHLSurfSetInformation(__in HLSURF hLsurf,
                          __in HLSURF_INFORMATION_CLASS InformationClass,
                          __in_bcount_opt(InformationLength) PVOID InformationBuffer,
                          __in ULONG InformationLength);
