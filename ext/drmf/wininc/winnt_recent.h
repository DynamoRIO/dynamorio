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

#ifndef _WINNT_RECENT_
#define _WINNT_RECENT_

/* Data types added since VS2005, typically in SDK6.0a.
 * We assume that cl 14.00 is not being used in conjunction with SDK6.0a
 * and instead is being used with the VS2005-included SDK (there's
 * no other way to test for SDK versions until post-7.0 in WinSDKVer.h).
 */

#if _MSC_VER < 1500

//
// Types of objects known to the kernel transaction manager.
//

typedef enum _KTMOBJECT_TYPE {

    KTMOBJECT_TRANSACTION,
    KTMOBJECT_TRANSACTION_MANAGER,
    KTMOBJECT_RESOURCE_MANAGER,
    KTMOBJECT_ENLISTMENT,
    KTMOBJECT_INVALID

} KTMOBJECT_TYPE,
    *PKTMOBJECT_TYPE;

typedef enum _ENLISTMENT_INFORMATION_CLASS {
    EnlistmentBasicInformation,
    EnlistmentRecoveryInformation,
    EnlistmentCrmInformation
} ENLISTMENT_INFORMATION_CLASS;

typedef enum _RESOURCEMANAGER_INFORMATION_CLASS {
    ResourceManagerBasicInformation,
    ResourceManagerCompletionInformation,
} RESOURCEMANAGER_INFORMATION_CLASS;

// begin_wdm
typedef enum _TRANSACTION_INFORMATION_CLASS {
    TransactionBasicInformation,
    TransactionPropertiesInformation,
    TransactionEnlistmentInformation,
    TransactionSuperiorEnlistmentInformation
    // end_wdm
    ,
    // The following info-classes are intended for DTC's use only; it will be
    // deprecated, and no one else should take a dependency on it.
    TransactionBindInformation,      // private and deprecated
    TransactionDTCPrivateInformation // private and deprecated
    ,
    // begin_wdm
} TRANSACTION_INFORMATION_CLASS;

// begin_wdm
typedef enum _TRANSACTIONMANAGER_INFORMATION_CLASS {
    TransactionManagerBasicInformation,
    TransactionManagerLogInformation,
    TransactionManagerLogPathInformation,
    TransactionManagerRecoveryInformation = 4
    // end_wdm
    ,
    // The following info-classes are intended for internal use only; they
    // are considered deprecated, and no one else should take a dependency
    // on them.
    TransactionManagerOnlineProbeInformation = 3,
    TransactionManagerOldestTransactionInformation = 5
    // end_wdm

    // begin_wdm
} TRANSACTIONMANAGER_INFORMATION_CLASS;

#endif

#endif /* _WINNT_RECENT_ */
