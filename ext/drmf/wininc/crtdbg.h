/****************************************************************************
 * Portions copyright (c) Microsoft Corporation
 ****************************************************************************/

#ifndef _CRTDBG_H_
#define _CRTDBG_H_ 1

/***************************************************************************
 * from crt/src/crtdbg.h
 * identical for VS2005, VS2008, VS2010
 */

/* Memory block identification */
#define _FREE_BLOCK      0
#define _NORMAL_BLOCK    1
#define _CRT_BLOCK       2
#define _IGNORE_BLOCK    3
#define _CLIENT_BLOCK    4
#define _MAX_BLOCKS      5

#define _BLOCK_TYPE(block)          (block & 0xFFFF)
#define _BLOCK_SUBTYPE(block)       (block >> 16 & 0xFFFF)


/***************************************************************************
 * from crt/src/dbgint.h
 * identical for VS2005, VS2008, VS2010
 */

#define _BLOCK_TYPE_IS_VALID(use) (_BLOCK_TYPE(use) == _CLIENT_BLOCK || \
                                              (use) == _NORMAL_BLOCK || \
                                   _BLOCK_TYPE(use) == _CRT_BLOCK    || \
                                              (use) == _IGNORE_BLOCK)

#define nNoMansLandSize 4

typedef struct _CrtMemBlockHeader
{
        struct _CrtMemBlockHeader * pBlockHeaderNext;
        struct _CrtMemBlockHeader * pBlockHeaderPrev;
        char *                      szFileName;
        int                         nLine;
#ifdef _WIN64
        /* These items are reversed on Win64 to eliminate gaps in the struct
         * and ensure that sizeof(struct)%16 == 0, so 16-byte alignment is
         * maintained in the debug heap.
         */
        int                         nBlockUse;
        size_t                      nDataSize;
#else  /* _WIN64 */
        size_t                      nDataSize;
        int                         nBlockUse;
#endif  /* _WIN64 */
        long                        lRequest;
        unsigned char               gap[nNoMansLandSize];
        /* followed by:
         *  unsigned char           data[nDataSize];
         *  unsigned char           anotherGap[nNoMansLandSize];
         */
} _CrtMemBlockHeader;

/* our definitions */
#define DBGCRT_PRE_REDZONE_SIZE sizeof(_CrtMemBlockHeader)
#define DBGCRT_POST_REDZONE_SIZE nNoMansLandSize

#endif /* _CRTDBG_H_ */
