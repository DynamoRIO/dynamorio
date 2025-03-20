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

/* from reactos/include/reactos/drivers/afd/shared.h */

#ifndef __AFD_SHARED_H
#define __AFD_SHARED_H

#include "tdi.h"
#include "wsock.h"

#define AFD_MAX_EVENTS                  10
#define AFD_PACKET_COMMAND_LENGTH	15
#define AfdCommand "AfdOpenPacketXX"

/* Extra definition of WSABUF for AFD so that I don't have to include any
 * userland winsock headers. */
typedef struct _AFD_WSABUF {
    UINT  len;
    PCHAR buf;
} AFD_WSABUF, *PAFD_WSABUF;

typedef struct _AFD_CREATE_PACKET {
    DWORD				EndpointFlags;
    DWORD				GroupID;
    DWORD				SizeOfTransportName;
    WCHAR				TransportName[1];
} AFD_CREATE_PACKET, *PAFD_CREATE_PACKET;

typedef struct _AFD_INFO {
    ULONG			        InformationClass;
    union {
        ULONG			        Ulong;
        LARGE_INTEGER	                LargeInteger;
        BOOLEAN                         Boolean;
    }					Information;
    ULONG				Padding;
} AFD_INFO, *PAFD_INFO;

typedef struct _AFD_BIND_DATA {
    ULONG				ShareType;
#if 1 /* bruening: based on win7 observation: i#376 */
    SOCKADDR    	                Address;
#else
    TRANSPORT_ADDRESS	                Address;
#endif
} AFD_BIND_DATA, *PAFD_BIND_DATA;

typedef struct _AFD_LISTEN_DATA {
    BOOLEAN				UseSAN;
    ULONG				Backlog;
    BOOLEAN				UseDelayedAcceptance;
} AFD_LISTEN_DATA, *PAFD_LISTEN_DATA;

typedef struct _AFD_HANDLE_ {
    SOCKET				Handle;
    ULONG				Events;
    NTSTATUS			        Status;
} AFD_HANDLE, *PAFD_HANDLE;

typedef struct _AFD_POLL_INFO {
    LARGE_INTEGER		        Timeout;
    ULONG				HandleCount;
    BOOLEAN                             Exclusive;
    AFD_HANDLE			        Handles[1];
} AFD_POLL_INFO, *PAFD_POLL_INFO;

typedef struct _AFD_ACCEPT_DATA {
    BOOLEAN				UseSAN;
    ULONG				SequenceNumber;
    HANDLE				ListenHandle;
} AFD_ACCEPT_DATA, *PAFD_ACCEPT_DATA;

typedef struct _AFD_RECEIVED_ACCEPT_DATA {
    ULONG				SequenceNumber;
    TRANSPORT_ADDRESS			Address;
} AFD_RECEIVED_ACCEPT_DATA, *PAFD_RECEIVED_ACCEPT_DATA;

typedef struct _AFD_PENDING_ACCEPT_DATA {
    ULONG				SequenceNumber;
    ULONG				SizeOfData;
    ULONG				ReturnSize;
} AFD_PENDING_ACCEPT_DATA, *PAFD_PENDING_ACCEPT_DATA;

typedef struct _AFD_DEFER_ACCEPT_DATA {
    ULONG				SequenceNumber;
    BOOLEAN				RejectConnection;
} AFD_DEFER_ACCEPT_DATA, *PAFD_DEFER_ACCEPT_DATA;

typedef struct  _AFD_RECV_INFO {
    PAFD_WSABUF				BufferArray;
    ULONG				BufferCount;
    ULONG				AfdFlags;
    ULONG				TdiFlags;
} AFD_RECV_INFO , *PAFD_RECV_INFO ;

typedef struct _AFD_RECV_INFO_UDP {
    PAFD_WSABUF				BufferArray;
    ULONG				BufferCount;
    ULONG				AfdFlags;
    ULONG				TdiFlags;
    PVOID				Address;
    PINT				AddressLength;
} AFD_RECV_INFO_UDP, *PAFD_RECV_INFO_UDP;

typedef struct  _AFD_SEND_INFO {
    PAFD_WSABUF				BufferArray;
    ULONG				BufferCount;
    ULONG				AfdFlags;
    ULONG				TdiFlags;
} AFD_SEND_INFO , *PAFD_SEND_INFO ;

typedef struct _AFD_SEND_INFO_UDP {
    PAFD_WSABUF				BufferArray;
    ULONG				BufferCount;
    ULONG				AfdFlags;
#if 1 /* timurrrr: based on XP+win7 observation: i#418 */
    ULONG				UnknownGap[9];
    ULONG				SizeOfRemoteAddress;
    PVOID				RemoteAddress;
#else
    TDI_REQUEST_SEND_DATAGRAM		TdiRequest;
    TDI_CONNECTION_INFORMATION		TdiConnection;
#endif
} AFD_SEND_INFO_UDP, *PAFD_SEND_INFO_UDP;

typedef struct  _AFD_CONNECT_INFO {
    BOOLEAN				UseSAN;
    ULONG				Root;
    ULONG				Unknown;
#if 1 /* bruening: based on win7 observation: i#376 */
    SOCKADDR    	                RemoteAddress;
#else
    TRANSPORT_ADDRESS			RemoteAddress;
#endif
} AFD_CONNECT_INFO , *PAFD_CONNECT_INFO ;

typedef struct _AFD_EVENT_SELECT_INFO {
    HANDLE				EventObject;
    ULONG				Events;
} AFD_EVENT_SELECT_INFO, *PAFD_EVENT_SELECT_INFO;

typedef struct _AFD_ENUM_NETWORK_EVENTS_INFO {
    HANDLE Event;
    ULONG PollEvents;
    NTSTATUS EventStatus[AFD_MAX_EVENTS];
} AFD_ENUM_NETWORK_EVENTS_INFO, *PAFD_ENUM_NETWORK_EVENTS_INFO;

typedef struct _AFD_DISCONNECT_INFO {
    ULONG				DisconnectType;
    LARGE_INTEGER			Timeout;
} AFD_DISCONNECT_INFO, *PAFD_DISCONNECT_INFO;

typedef struct _AFD_VALIDATE_GROUP_DATA
{
    LONG GroupId;
    TRANSPORT_ADDRESS Address;
} AFD_VALIDATE_GROUP_DATA, *PAFD_VALIDATE_GROUP_DATA;

typedef struct _AFD_TDI_HANDLE_DATA
{
    HANDLE TdiAddressHandle;
    HANDLE TdiConnectionHandle;
} AFD_TDI_HANDLE_DATA, *PAFD_TDI_HANDLE_DATA;

/* AFD Packet Endpoint Flags */
#define AFD_ENDPOINT_CONNECTIONLESS	0x1
#define AFD_ENDPOINT_MESSAGE_ORIENTED	0x10
#define AFD_ENDPOINT_RAW		0x100
#define AFD_ENDPOINT_MULTIPOINT		0x1000
#define AFD_ENDPOINT_C_ROOT		0x10000
#define AFD_ENDPOINT_D_ROOT	        0x100000

/* AFD TDI Query Flags */
#define AFD_ADDRESS_HANDLE      0x1L
#define AFD_CONNECTION_HANDLE   0x2L

/* AFD event bits */
#define AFD_EVENT_RECEIVE_BIT                   0
#define AFD_EVENT_OOB_RECEIVE_BIT               1
#define AFD_EVENT_SEND_BIT                      2
#define AFD_EVENT_DISCONNECT_BIT                3
#define AFD_EVENT_ABORT_BIT                     4
#define AFD_EVENT_CLOSE_BIT                     5
#define AFD_EVENT_CONNECT_BIT                   6
#define AFD_EVENT_ACCEPT_BIT                    7
#define AFD_EVENT_CONNECT_FAIL_BIT              8
#define AFD_EVENT_QOS_BIT                       9
#define AFD_EVENT_GROUP_QOS_BIT                 10
#define AFD_EVENT_ROUTING_INTERFACE_CHANGE_BIT  11
#define AFD_EVENT_ADDRESS_LIST_CHANGE_BIT       12
#define AFD_MAX_EVENT                           13
#define AFD_ALL_EVENTS                          ((1 << AFD_MAX_EVENT) - 1)

/* AFD Info Flags */
#define AFD_INFO_INLINING_MODE		0x01L
#define AFD_INFO_BLOCKING_MODE		0x02L
#define AFD_INFO_SENDS_IN_PROGRESS	0x04L
#define AFD_INFO_RECEIVE_WINDOW_SIZE	0x06L
#define AFD_INFO_SEND_WINDOW_SIZE	0x07L
#define AFD_INFO_GROUP_ID_TYPE	        0x10L
#define AFD_INFO_RECEIVE_CONTENT_SIZE   0x11L

/* AFD Share Flags */
#define AFD_SHARE_UNIQUE		0x0L
#define AFD_SHARE_REUSE			0x1L
#define AFD_SHARE_WILDCARD		0x2L
#define AFD_SHARE_EXCLUSIVE		0x3L

/* AFD Disconnect Flags */
#define AFD_DISCONNECT_SEND		0x01L
#define AFD_DISCONNECT_RECV		0x02L
#define AFD_DISCONNECT_ABORT		0x04L
#define AFD_DISCONNECT_DATAGRAM		0x08L

/* AFD Event Flags */
#define AFD_EVENT_RECEIVE                   (1 << AFD_EVENT_RECEIVE_BIT)
#define AFD_EVENT_OOB_RECEIVE               (1 << AFD_EVENT_OOB_RECEIVE_BIT)
#define AFD_EVENT_SEND                      (1 << AFD_EVENT_SEND_BIT)
#define AFD_EVENT_DISCONNECT                (1 << AFD_EVENT_DISCONNECT_BIT)
#define AFD_EVENT_ABORT                     (1 << AFD_EVENT_ABORT_BIT)
#define AFD_EVENT_CLOSE                     (1 << AFD_EVENT_CLOSE_BIT)
#define AFD_EVENT_CONNECT                   (1 << AFD_EVENT_CONNECT_BIT)
#define AFD_EVENT_ACCEPT                    (1 << AFD_EVENT_ACCEPT_BIT)
#define AFD_EVENT_CONNECT_FAIL              (1 << AFD_EVENT_CONNECT_FAIL_BIT)
#define AFD_EVENT_QOS                       (1 << AFD_EVENT_QOS_BIT)
#define AFD_EVENT_GROUP_QOS                 (1 << AFD_EVENT_GROUP_QOS_BIT)
#define AFD_EVENT_ROUTING_INTERFACE_CHANGE  (1 << AFD_EVENT_ROUTING_INTERFACE_CHANGE_BIT)
#define AFD_EVENT_ADDRESS_LIST_CHANGE       (1 << AFD_EVENT_ADDRESS_LIST_CHANGE_BIT)

/* AFD SEND/RECV Flags */
#define AFD_SKIP_FIO			0x1L
#define AFD_OVERLAPPED			0x2L
#define AFD_IMMEDIATE                   0x4L

/* IOCTL Generation */
#define FSCTL_AFD_BASE                  FILE_DEVICE_NETWORK
#define _AFD_CONTROL_CODE(Operation,Method) \
  ((FSCTL_AFD_BASE)<<12 | (Operation<<2) | Method)

/* AFD Commands */
#define AFD_BIND			0
#define AFD_CONNECT			1
#define AFD_START_LISTEN		2
#define AFD_WAIT_FOR_LISTEN		3
#define AFD_ACCEPT			4
#define AFD_RECV			5
#define AFD_RECV_DATAGRAM		6
#define AFD_SEND			7
#define AFD_SEND_DATAGRAM		8
#define AFD_SELECT			9
#define AFD_DISCONNECT			10
#define AFD_GET_SOCK_NAME		11
#define AFD_GET_PEER_NAME               12
#define AFD_GET_TDI_HANDLES		13
#define AFD_SET_INFO			14
#define AFD_GET_CONTEXT_SIZE		15
#define AFD_GET_CONTEXT			16
#define AFD_SET_CONTEXT			17
#define AFD_SET_CONNECT_DATA		18
#define AFD_SET_CONNECT_OPTIONS		19
#define AFD_SET_DISCONNECT_DATA		20
#define AFD_SET_DISCONNECT_OPTIONS	21
#define AFD_GET_CONNECT_DATA		22
#define AFD_GET_CONNECT_OPTIONS		23
#define AFD_GET_DISCONNECT_DATA		24
#define AFD_GET_DISCONNECT_OPTIONS	25
#define AFD_SET_CONNECT_DATA_SIZE       26
#define AFD_SET_CONNECT_OPTIONS_SIZE    27
#define AFD_SET_DISCONNECT_DATA_SIZE    28
#define AFD_SET_DISCONNECT_OPTIONS_SIZE 29
#define AFD_GET_INFO			30
#define AFD_EVENT_SELECT		33
#define AFD_ENUM_NETWORK_EVENTS         34
#define AFD_DEFER_ACCEPT		35
#define AFD_GET_PENDING_CONNECT_DATA	41
#define AFD_VALIDATE_GROUP		42

/* AFD IOCTLs */

#define IOCTL_AFD_BIND \
  _AFD_CONTROL_CODE(AFD_BIND, METHOD_NEITHER)
#define IOCTL_AFD_CONNECT \
  _AFD_CONTROL_CODE(AFD_CONNECT, METHOD_NEITHER)
#define IOCTL_AFD_START_LISTEN \
  _AFD_CONTROL_CODE(AFD_START_LISTEN, METHOD_NEITHER)
#define IOCTL_AFD_WAIT_FOR_LISTEN \
  _AFD_CONTROL_CODE(AFD_WAIT_FOR_LISTEN, METHOD_BUFFERED )
#define IOCTL_AFD_ACCEPT \
  _AFD_CONTROL_CODE(AFD_ACCEPT, METHOD_BUFFERED )
#define IOCTL_AFD_RECV \
  _AFD_CONTROL_CODE(AFD_RECV, METHOD_NEITHER)
#define IOCTL_AFD_RECV_DATAGRAM \
  _AFD_CONTROL_CODE(AFD_RECV_DATAGRAM, METHOD_NEITHER)
#define IOCTL_AFD_SEND \
  _AFD_CONTROL_CODE(AFD_SEND, METHOD_NEITHER)
#define IOCTL_AFD_SEND_DATAGRAM \
  _AFD_CONTROL_CODE(AFD_SEND_DATAGRAM, METHOD_NEITHER)
#define IOCTL_AFD_SELECT \
  _AFD_CONTROL_CODE(AFD_SELECT, METHOD_BUFFERED )
#define IOCTL_AFD_DISCONNECT \
  _AFD_CONTROL_CODE(AFD_DISCONNECT, METHOD_NEITHER)
#define IOCTL_AFD_GET_SOCK_NAME \
  _AFD_CONTROL_CODE(AFD_GET_SOCK_NAME, METHOD_NEITHER)
#define IOCTL_AFD_GET_PEER_NAME \
  _AFD_CONTROL_CODE(AFD_GET_PEER_NAME, METHOD_NEITHER)
#define IOCTL_AFD_GET_TDI_HANDLES \
  _AFD_CONTROL_CODE(AFD_GET_TDI_HANDLES, METHOD_NEITHER)
#define IOCTL_AFD_SET_INFO \
  _AFD_CONTROL_CODE(AFD_SET_INFO, METHOD_NEITHER)
#define IOCTL_AFD_GET_CONTEXT_SIZE \
  _AFD_CONTROL_CODE(AFD_GET_CONTEXT_SIZE, METHOD_NEITHER)
#define IOCTL_AFD_GET_CONTEXT \
  _AFD_CONTROL_CODE(AFD_GET_CONTEXT, METHOD_NEITHER)
#define IOCTL_AFD_SET_CONTEXT \
  _AFD_CONTROL_CODE(AFD_SET_CONTEXT, METHOD_NEITHER)
#define IOCTL_AFD_SET_CONNECT_DATA \
  _AFD_CONTROL_CODE(AFD_SET_CONNECT_DATA, METHOD_NEITHER)
#define IOCTL_AFD_SET_CONNECT_OPTIONS \
  _AFD_CONTROL_CODE(AFD_SET_CONNECT_OPTIONS, METHOD_NEITHER)
#define IOCTL_AFD_SET_DISCONNECT_DATA \
  _AFD_CONTROL_CODE(AFD_SET_DISCONNECT_DATA, METHOD_NEITHER)
#define IOCTL_AFD_SET_DISCONNECT_OPTIONS \
  _AFD_CONTROL_CODE(AFD_SET_DISCONNECT_OPTIONS, METHOD_NEITHER)
#define IOCTL_AFD_GET_CONNECT_DATA \
  _AFD_CONTROL_CODE(AFD_GET_CONNECT_DATA, METHOD_NEITHER)
#define IOCTL_AFD_GET_CONNECT_OPTIONS \
  _AFD_CONTROL_CODE(AFD_GET_CONNECT_OPTIONS, METHOD_NEITHER)
#define IOCTL_AFD_GET_DISCONNECT_DATA \
  _AFD_CONTROL_CODE(AFD_GET_DISCONNECT_DATA, METHOD_NEITHER)
#define IOCTL_AFD_GET_DISCONNECT_OPTIONS \
  _AFD_CONTROL_CODE(AFD_GET_DISCONNECT_OPTIONS, METHOD_NEITHER)
#define IOCTL_AFD_SET_CONNECT_DATA_SIZE \
  _AFD_CONTROL_CODE(AFD_SET_CONNECT_DATA_SIZE, METHOD_NEITHER)
#define IOCTL_AFD_SET_CONNECT_OPTIONS_SIZE \
  _AFD_CONTROL_CODE(AFD_SET_CONNECT_OPTIONS_SIZE, METHOD_NEITHER)
#define IOCTL_AFD_SET_DISCONNECT_DATA_SIZE \
  _AFD_CONTROL_CODE(AFD_SET_DISCONNECT_DATA_SIZE, METHOD_NEITHER)
#define IOCTL_AFD_SET_DISCONNECT_OPTIONS_SIZE \
  _AFD_CONTROL_CODE(AFD_SET_DISCONNECT_OPTIONS_SIZE, METHOD_NEITHER)
#define IOCTL_AFD_GET_INFO \
  _AFD_CONTROL_CODE(AFD_GET_INFO, METHOD_NEITHER)
#define IOCTL_AFD_EVENT_SELECT \
  _AFD_CONTROL_CODE(AFD_EVENT_SELECT, METHOD_NEITHER)
#define IOCTL_AFD_DEFER_ACCEPT \
  _AFD_CONTROL_CODE(AFD_DEFER_ACCEPT, METHOD_NEITHER)
#define IOCTL_AFD_GET_PENDING_CONNECT_DATA \
  _AFD_CONTROL_CODE(AFD_GET_PENDING_CONNECT_DATA, METHOD_NEITHER)
#define IOCTL_AFD_ENUM_NETWORK_EVENTS \
  _AFD_CONTROL_CODE(AFD_ENUM_NETWORK_EVENTS, METHOD_NEITHER)
#define IOCTL_AFD_VALIDATE_GROUP \
  _AFD_CONTROL_CODE(AFD_VALIDATE_GROUP, METHOD_NEITHER)

typedef struct _AFD_SOCKET_INFORMATION {
    BOOL CommandChannel;
    INT AddressFamily;
    INT SocketType;
    INT Protocol;
    PVOID HelperContext;
    DWORD NotificationEvents;
    UNICODE_STRING TdiDeviceName;
    SOCKADDR Name;
} AFD_SOCKET_INFORMATION, *PAFD_SOCKET_INFORMATION;

typedef struct _FILE_REQUEST_BIND {
    SOCKADDR Name;
} FILE_REQUEST_BIND, *PFILE_REQUEST_BIND;

typedef struct _FILE_REPLY_BIND {
    INT Status;
    HANDLE TdiAddressObjectHandle;
    HANDLE TdiConnectionObjectHandle;
} FILE_REPLY_BIND, *PFILE_REPLY_BIND;

typedef struct _FILE_REQUEST_LISTEN {
    INT Backlog;
} FILE_REQUEST_LISTEN, *PFILE_REQUEST_LISTEN;

typedef struct _FILE_REPLY_LISTEN {
    INT Status;
} FILE_REPLY_LISTEN, *PFILE_REPLY_LISTEN;

typedef struct _FILE_REQUEST_SENDTO {
    LPWSABUF Buffers;
    DWORD BufferCount;
    DWORD Flags;
    SOCKADDR To;
    INT ToLen;
} FILE_REQUEST_SENDTO, *PFILE_REQUEST_SENDTO;

typedef struct _FILE_REPLY_SENDTO {
    INT Status;
    DWORD NumberOfBytesSent;
} FILE_REPLY_SENDTO, *PFILE_REPLY_SENDTO;

typedef struct _FILE_REQUEST_RECVFROM {
    LPWSABUF Buffers;
    DWORD BufferCount;
    LPDWORD Flags;
    LPSOCKADDR From;
    LPINT FromLen;
} FILE_REQUEST_RECVFROM, *PFILE_REQUEST_RECVFROM;

typedef struct _FILE_REPLY_RECVFROM {
    INT Status;
    DWORD NumberOfBytesRecvd;
} FILE_REPLY_RECVFROM, *PFILE_REPLY_RECVFROM;

typedef struct _FILE_REQUEST_RECV {
    LPWSABUF Buffers;
    DWORD BufferCount;
    LPDWORD Flags;
} FILE_REQUEST_RECV, *PFILE_REQUEST_RECV;

typedef struct _FILE_REPLY_RECV {
    INT Status;
    DWORD NumberOfBytesRecvd;
} FILE_REPLY_RECV, *PFILE_REPLY_RECV;


typedef struct _FILE_REQUEST_SEND {
    LPWSABUF Buffers;
    DWORD BufferCount;
    DWORD Flags;
} FILE_REQUEST_SEND, *PFILE_REQUEST_SEND;

typedef struct _FILE_REPLY_SEND {
    INT Status;
    DWORD NumberOfBytesSent;
} FILE_REPLY_SEND, *PFILE_REPLY_SEND;


typedef struct _FILE_REQUEST_ACCEPT {
    LPSOCKADDR addr;
    INT addrlen;
    LPCONDITIONPROC lpfnCondition;
    DWORD dwCallbackData;
} FILE_REQUEST_ACCEPT, *PFILE_REQUEST_ACCEPT;

typedef struct _FILE_REPLY_ACCEPT {
    INT Status;
    INT addrlen;
    SOCKET Socket;
} FILE_REPLY_ACCEPT, *PFILE_REPLY_ACCEPT;


typedef struct _FILE_REQUEST_CONNECT {
    LPSOCKADDR name;
    INT namelen;
    LPWSABUF lpCallerData;
    LPWSABUF lpCalleeData;
    LPQOS lpSQOS;
    LPQOS lpGQOS;
} FILE_REQUEST_CONNECT, *PFILE_REQUEST_CONNECT;

typedef struct _FILE_REPLY_CONNECT {
    INT Status;
} FILE_REPLY_CONNECT, *PFILE_REPLY_CONNECT;

#endif /*__AFD_SHARED_H */
