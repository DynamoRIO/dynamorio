/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from a Windows DDK header to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

/***************************************************************************
 * from DDK/WDK tdi.h
 */

#ifndef _TDI_H_
#define _TDI_H_

typedef LONG TDI_STATUS;
typedef PVOID CONNECTION_CONTEXT;

typedef struct _TDI_CONNECTION_INFORMATION {
  LONG  UserDataLength;
  PVOID  UserData;
  LONG  OptionsLength;
  PVOID  Options;
  LONG  RemoteAddressLength;
  PVOID  RemoteAddress;
} TDI_CONNECTION_INFORMATION, *PTDI_CONNECTION_INFORMATION;

typedef struct _TDI_REQUEST {
  union {
    HANDLE  AddressHandle;
    CONNECTION_CONTEXT  ConnectionContext;
    HANDLE  ControlChannel;
  } Handle;
  PVOID  RequestNotifyObject;
  PVOID  RequestContext;
  TDI_STATUS  TdiStatus;
} TDI_REQUEST, *PTDI_REQUEST;

typedef struct _TDI_REQUEST_SEND_DATAGRAM {
  TDI_REQUEST  Request;
  PTDI_CONNECTION_INFORMATION  SendDatagramInformation;
} TDI_REQUEST_SEND_DATAGRAM, *PTDI_REQUEST_SEND_DATAGRAM;

typedef struct _TA_ADDRESS {
  USHORT  AddressLength;
  USHORT  AddressType;
  UCHAR  Address[1];
} TA_ADDRESS, *PTA_ADDRESS;

typedef struct _TRANSPORT_ADDRESS {
  LONG  TAAddressCount;
  TA_ADDRESS  Address[1];
} TRANSPORT_ADDRESS, *PTRANSPORT_ADDRESS;

#endif /* _TDI_H_ */
