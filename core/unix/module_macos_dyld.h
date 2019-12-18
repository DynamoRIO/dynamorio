/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was created to make information necessary for userspace
 ***   to call into the Mac OS X kernel available to DynamoRIO.  It contains
 ***   only constants, structures, and macros, and thus, contains no
 ***   copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _MODULE_MACOS_DYLD_H_
#define _MODULE_MACOS_DYLD_H_ 1

struct dyld_cache_header {
    char magic[16];
    uint32_t mappingOffset;
    uint32_t mappingCount;
    uint32_t imagesOffset;
    uint32_t imagesCount;
    uint64_t dyldBaseAddress;
    uint64_t codeSignatureOffset;
    uint64_t codeSignatureSize;
    uint64_t slideInfoOffset;
    uint64_t slideInfoSize;
};

struct dyld_cache_mapping_info {
    uint64_t address;
    uint64_t size;
    uint64_t fileOffset;
    uint32_t maxProt;
    uint32_t initProt;
};

#endif /* _MODULE_MACOS_DYLD_H_ */
