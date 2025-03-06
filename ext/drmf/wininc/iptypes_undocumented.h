/****************************************************************************
 * Reverse engineered type information from ioctls called from IPHLPAPI.dll.
 * Likely related to structures in ddk/inc/api/iptypes.h, but new in Vista+ and
 * undocumented.
 ****************************************************************************/

#ifndef _IPTYPES_UNDOCUMENTED_H_
#define _IPTYPES_UNDOCUMENTED_H_

/* From DDK/WDK iptypes.h .  Double-define should succeed if values are equal.
 */
#define MAX_ADAPTER_NAME_LENGTH         256

/* This is an output parameter for net ioctl 0x003 that has information about
 * the network adapter.  The only fields we understand are adapter_name_len and
 * adapter_name.  Might be related to MIB_IFROW/MIB_IFTABLE structs in ifmib.h.
 * Alternatively see IP_ADAPTER_INFO in iptypes.h.
 */
typedef struct _ip_adapter_info_t {
    uint adapter_name_len;
    /* Contains "FBroadcom NetXtreme Gigabit Ethernet" on my workstation. */
    wchar_t adapter_name[MAX_ADAPTER_NAME_LENGTH + 2];
    ushort unknown_a;
    ushort uninit_a;  /* Not initialized by kernel. */
    uint unknown_b[7];
    byte unknown_c;
    byte uninit_b[3];  /* Not initialized by kernel. */
    uint unknown_d[3];
} ip_adapter_info_t;

/* Reverse engineered input/output structure for net ioctl 0x003.
 */
typedef struct _net_ioctl_003_inout_t {
    uint unknown_a[2];  /* Usually 0 */
    HANDLE probable_handle;
    uint unknown_b[3];
    void *buf1;         /* In-param, usually stack. */
    uint buf1_sz;       /* Probable size, usually 8. */
    void *buf2;         /* Out-param, usually stack. */
    uint buf2_sz;       /* Probable size, usually 4. */
    uint unknown_c[2];
    ip_adapter_info_t *adapter_info; /* Out-param, usually stack. */
    uint adapter_info_sz;            /* Probable size. */
} net_ioctl_003_inout_t;

/* Reverse engineered input/output structure for net ioctl 0x006.
 *
 * All of the buffers referred to by this structure are output parameters to the
 * ioctl.  Filling them with 0xcc has no effect, and they are fully initialized
 * afterwards.  num_elts contains the number of elements in every buffer
 * present, and each buffer specifies an element size.  num_elts is updated by
 * the ioctl to indicate how many elements were written.
 */
typedef struct _net_ioctl_006_inout_t {
    uint unknown_a[2];
    HANDLE probable_handle;
    uint unknown_b[3];  /* These are usually all 1. */
    void *buf1;
    uint buf1_elt_sz;
    void *buf2;
    uint buf2_elt_sz;
    void *buf3;
    uint buf3_elt_sz;
    void *buf4;         /* Contains wide strings referring to adapter info. */
    uint buf4_elt_sz;
    uint num_elts;      /* in/out: input is available elts, out is num written. */
} net_ioctl_006_inout_t;

#endif /* _IPTYPES_UNDOCUMENTED_H_ */
