/* winsock.h -- Winsock names mapped onto BSD sockets. */
#ifndef WWPORT_COMPAT_WINSOCK_H
#define WWPORT_COMPAT_WINSOCK_H
#include "windows.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
/* SOCKET, INVALID_SOCKET and SOCKET_ERROR come from windows.h above,
** mirroring how real <windows.h> exposes them via <winsock.h>. */
#define closesocket    close

/*
**	Winsock's uppercase aliases for the BSD structures. Same types, different
**	spelling; CODE/tcpip.h declares its members with these names.
*/
typedef struct in_addr      IN_ADDR,     *LPIN_ADDR;
typedef struct sockaddr     SOCKADDR,    *LPSOCKADDR;
typedef struct sockaddr_in  SOCKADDR_IN, *LPSOCKADDR_IN;
typedef struct hostent      HOSTENT,     *LPHOSTENT;

/*
**	WSADATA -- filled in by WSAStartup(), which has no BSD equivalent because
**	sockets need no per-process initialisation on macOS.
**
**	Laid out rather than left opaque because CODE/tcpip.h embeds one by value
**	(`WSADATA WinsockInfo;`). The field widths are Winsock's. Nothing in the
**	engine reads these fields -- the struct is passed to WSAStartup and then
**	ignored -- so the layout only has to exist, not be populated.
*/
#define WSADESCRIPTION_LEN 256
#define WSASYS_STATUS_LEN  128

/*
**	Size of the buffer WSAAsyncGetHostByName() fills in. Winsock's value; the
**	engine uses it only to size a `char HostBuff[...]` member in tcpip.h.
*/
#define MAXGETHOSTSTRUCT 1024

typedef struct WSAData {
	WORD           wVersion;
	WORD           wHighVersion;
	char           szDescription[WSADESCRIPTION_LEN + 1];
	char           szSystemStatus[WSASYS_STATUS_LEN + 1];
	unsigned short iMaxSockets;
	unsigned short iMaxUdpDg;
	char *         lpVendorInfo;
} WSADATA, *LPWSADATA;

#endif
