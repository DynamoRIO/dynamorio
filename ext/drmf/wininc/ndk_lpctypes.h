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

/* from reactos/include/ndk/lpctypes.h */

#ifndef _LPCTYPES_H
#define _LPCTYPES_H

//
// Information Classes for NtQueryInformationPort
//
typedef enum _PORT_INFORMATION_CLASS {
    PortBasicInformation
#if DEVL
,   PortDumpInformation
#endif
} PORT_INFORMATION_CLASS;


#endif /*_LPCTYPES_H */
