/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was created to make information necessary for userspace
 ***   to call into the Windows kernel available to Dr. Memory.  It contains
 ***   only constants, structures, and macros, and thus, contains no
 ***   copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _KTMTYPES_
#define _KTMTYPES_ 1

/* Data types added since VS2005, typically in SDK6.0a.
 * We assume that cl 14.00 is not being used in conjunction with SDK6.0a
 * and instead is being used with the VS2005-included SDK (there's
 * no other way to test for SDK versions until post-7.0 in WinSDKVer.h).
 */

#if _MSC_VER < 1500

typedef GUID CRM_PROTOCOL_ID, *PCRM_PROTOCOL_ID;

typedef ULONG NOTIFICATION_MASK;

#endif /* _MSC_VER < 1500 */

#endif /* _KTMTYPES_ */
