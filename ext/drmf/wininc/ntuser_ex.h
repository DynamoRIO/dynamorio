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

#ifndef _NTUSER_EX_
#define _NTUSER_EX_ 1

/* The data structure used for NtUserBuildPropList.
 * ReactOS has this as PROPLISTITEM but that does not match true Windows.
 */
typedef struct _USER_PROP_LIST_ENTRY
{
    /* This first field seems to be a pointer to a string, but on x64 it is no
     * larger, or on x86 there is a NULL padding after it.
     */
    DWORD Unknown1;
    /* This one is always 0 */
    DWORD Unknown2;
    ATOM Atom;
    /* This is uninitialized */
    DWORD Unknown3;
} USER_PROP_LIST_ENTRY, *PUSER_PROP_LIST_ENTRY;

#endif /* _NTUSER_EX_ */
