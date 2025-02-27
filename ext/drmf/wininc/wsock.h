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

#ifndef _WSOCK_H_
#define _WSOCK_H_ 1

/***************************************************************************
 * from DDK/WDK winsock2.h
 */

typedef UINT_PTR SOCKET;

typedef unsigned int GROUP;

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;


/***************************************************************************
 * from DDK/WDK qos.h
 */

typedef ULONG SERVICETYPE;

typedef struct _flowspec {
  ULONG TokenRate;
  ULONG TokenBucketSize;
  ULONG PeakBandwidth;
  ULONG Latency;
  ULONG DelayVariation;
  SERVICETYPE ServiceType;
  ULONG MaxSduSize;
  ULONG MinimumPolicedSize;
} FLOWSPEC, *PFLOWSPEC, *LPFLOWSPEC;


/***************************************************************************
 * from DDK/WDK ws2def.h
 */

typedef USHORT ADDRESS_FAMILY;

#define AF_UNSPEC       0
#define AF_UNIX         1
#define AF_INET         2
#define AF_IMPLINK      3
#define AF_PUP          4
#define AF_CHAOS        5
#define AF_NS           6
#define AF_IPX          AF_NS
#define AF_ISO          7
#define AF_OSI          AF_ISO
#define AF_ECMA         8
#define AF_DATAKIT      9
#define AF_CCITT        10
#define AF_SNA          11
#define AF_DECnet       12
#define AF_DLI          13
#define AF_LAT          14
#define AF_HYLINK       15
#define AF_APPLETALK    16
#define AF_NETBIOS      17
#define AF_VOICEVIEW    18
#define AF_FIREFOX      19
#define AF_UNKNOWN1     20
#define AF_BAN          21
#define AF_ATM          22
#define AF_INET6        23
#define AF_CLUSTER      24
#define AF_12844        25
#define AF_IRDA         26
#define AF_NETDES       28

typedef struct sockaddr {
#if (_WIN32_WINNT < 0x0600)
  u_short sa_family;
#else
  ADDRESS_FAMILY sa_family;
#endif
  CHAR sa_data[14];
} SOCKADDR, *PSOCKADDR, FAR *LPSOCKADDR;

typedef struct _WSABUF {
  ULONG len;
  CHAR FAR *buf;
} WSABUF, FAR * LPWSABUF;

typedef struct _QualityOfService {
  FLOWSPEC SendingFlowspec;
  FLOWSPEC ReceivingFlowspec;
  WSABUF ProviderSpecific;
} QOS, *LPQOS;

typedef struct {
  union {
    struct {
      ULONG Zone:28;
      ULONG Level:4;
    };
    ULONG Value;
  };
} SCOPE_ID, *PSCOPE_ID;


/***************************************************************************
 * from DDK/WDK winsock2.h
 */

typedef int
(CALLBACK *LPCONDITIONPROC)(
  IN LPWSABUF lpCallerId,
  IN LPWSABUF lpCallerData,
  IN OUT LPQOS lpSQOS,
  IN OUT LPQOS lpGQOS,
  IN LPWSABUF lpCalleeId,
  IN LPWSABUF lpCalleeData,
  OUT GROUP FAR *g,
  IN DWORD_PTR dwCallbackData);

struct linger {
  u_short l_onoff;
  u_short l_linger;
};

typedef struct in_addr {
  union {
    struct { u_char s_b1,s_b2,s_b3,s_b4; } S_un_b;
    struct { u_short s_w1,s_w2; } S_un_w;
    u_long S_addr;
  } S_un;
} IN_ADDR, *PIN_ADDR;


/***************************************************************************
 * from DDK/WDK winsock.h
 */

struct sockaddr_in {
  short sin_family;
  u_short sin_port;
  struct in_addr sin_addr;
  char sin_zero[8];
};


/***************************************************************************
 * from DDK/WDK in6addr.h
 */

typedef struct in6_addr {
  union {
    UCHAR Byte[16];
    USHORT Word[8];
  } u;
} IN6_ADDR, *PIN6_ADDR, FAR *LPIN6_ADDR;


/***************************************************************************
 * from DDK/WDK ws2ipdef.h
 */

typedef struct sockaddr_in6 {
  ADDRESS_FAMILY sin6_family;
  USHORT sin6_port;
  ULONG sin6_flowinfo;
  IN6_ADDR sin6_addr;
  union {
    ULONG sin6_scope_id;
    SCOPE_ID sin6_scope_struct;
  };
} SOCKADDR_IN6_LH, *PSOCKADDR_IN6_LH, FAR *LPSOCKADDR_IN6_LH;



#endif /* _WSOCK_H_ */
