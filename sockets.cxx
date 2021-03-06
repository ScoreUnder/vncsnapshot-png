/*
 *  Copyright (C) 2002 RealVNC Ltd.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

/*
 * sockets.cxx - functions to deal with sockets.
 */

#ifdef WIN32
#include <winsock2.h>
#define close(x) closesocket(x)
typedef int socklen_t;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif

extern "C" {
#include "vncsnapshot.h"
}

#include "rdr/FdInStream.h"
#include "rdr/FdOutStream.h"
#include "rdr/Exception.h"


int rfbsock;
rdr::FdInStream* fis;
rdr::FdOutStream* fos;
bool sameMachine = false;

/*static bool rfbsockReady = false;*/

/*
 * InitializeSockets is called on startup. It will do any required one-time setup
 * for the network.
 *
 * On *nix systems, it does nothing.
 *
 * On Windows, it initializes Windows Sockets.
 */
extern "C" bool
InitializeSockets(void)
{
#ifdef WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    wVersionRequested = MAKEWORD( 2, 2 );

    err = WSAStartup( wVersionRequested, &wsaData );
    if ( err != 0 ) {
        fprintf(stderr, "Cannot initialize Windows Sockets\n");
        return false;
    }

/* Confirm that the WinSock DLL supports 2.2.*/
/* Note that if the DLL supports versions greater    */
/* than 2.2 in addition to 2.2, it will still return */
/* 2.2 in wVersion since that is the version we      */
/* requested.                                        */
 
    if ( LOBYTE( wsaData.wVersion ) != 2 ||
         HIBYTE( wsaData.wVersion ) != 2 ) {
        fprintf(stderr, "Cannot get proper version of Windows Sockets\n");
        WSACleanup( );
        return false; 
    }

/* The WinSock DLL is acceptable. Proceed. */
#endif
    return true;
}

/*
 * ConnectToRFBServer.
 */

bool ConnectToRFBServer(const char *hostname, uint16_t port)
{
  int sock = ConnectToTcpAddr(hostname, port);

  if (sock < 0) {
    fprintf(stderr,"Unable to connect to VNC server\n");
    return false;
  }

  return SetRFBSock(sock);
}

bool SetRFBSock(int sock)
{
  try {
    rfbsock = sock;
    fis = new rdr::FdInStream(rfbsock);
    fos = new rdr::FdOutStream(rfbsock);

    struct sockaddr_in peeraddr, myaddr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    getpeername(sock, (struct sockaddr *)&peeraddr, &addrlen);
    getsockname(sock, (struct sockaddr *)&myaddr, &addrlen);

    sameMachine = (peeraddr.sin_addr.s_addr == myaddr.sin_addr.s_addr);

    return true;
  } catch (rdr::Exception& e) {
    fprintf(stderr,"initialiseInStream: %s\n",e.str());
  }
  return false;
}

bool ReadFromRFBServer(uint8_t *out, size_t n)
{
  try {
    fis->readBytes(out, n);
    return true;
  } catch (rdr::Exception& e) {
    fprintf(stderr,"ReadFromRFBServer: %s\n",e.str());
  }
  return false;
}


/*
 * Write an exact number of bytes, and don't return until you've sent them.
 */

bool WriteToRFBServer(uint8_t *buf, size_t n)
{
  try {
    fos->writeBytes(buf, n);
    fos->flush();
    return true;
  } catch (rdr::Exception& e) {
    fprintf(stderr,"WriteExact: %s\n",e.str());
  }
  return false;
}


/*
 * ConnectToTcpAddr connects to the given host and port.
 */

int ConnectToTcpAddr(const char* hostname, uint16_t port)
{
  int sock;
  struct sockaddr_in addr;
  int one = 1;
  unsigned int host;

  if (!StringToIPAddr(hostname, &host)) {
    fprintf(stderr,"Couldn't convert '%s' to host address\n", hostname);
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = host;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    fprintf(stderr,programName);
    perror(": ConnectToTcpAddr: socket");
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fprintf(stderr,programName);
    perror(": ConnectToTcpAddr: connect");
    close(sock);
    return -1;
  }

  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                 (char *)&one, sizeof(one)) < 0) {
    fprintf(stderr,programName);
    perror(": ConnectToTcpAddr: setsockopt");
    close(sock);
    return -1;
  }

  return sock;
}



/*
 * FindFreeTcpPort tries to find unused TCP port in the range
 * (TUNNEL_PORT_OFFSET, TUNNEL_PORT_OFFSET + 99]. Returns 0 on failure.
 */

uint16_t
FindFreeTcpPort(void)
{
  int sock;
  uint16_t port;
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    fprintf(stderr,programName);
    perror(": FindFreeTcpPort: socket");
    return 0;
  }

  for (port = TUNNEL_PORT_OFFSET + 99; port > TUNNEL_PORT_OFFSET; port--) {
    addr.sin_port = htons((unsigned short)port);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
      close(sock);
      return port;
    }
  }

  close(sock);
  return 0;
}


/*
 * ListenAtTcpPort starts listening at the given TCP port.
 */

int ListenAtTcpPort(uint16_t port)
{
  int sock;
  struct sockaddr_in addr;
  int one = 1;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    fprintf(stderr,programName);
    perror(": ListenAtTcpPort: socket");
    return -1;
  }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                 (const char *)&one, sizeof(one)) < 0) {
    fprintf(stderr,programName);
    perror(": ListenAtTcpPort: setsockopt");
    close(sock);
    return -1;
  }

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fprintf(stderr,programName);
    perror(": ListenAtTcpPort: bind");
    close(sock);
    return -1;
  }

  if (listen(sock, 5) < 0) {
    fprintf(stderr,programName);
    perror(": ListenAtTcpPort: listen");
    close(sock);
    return -1;
  }

  return sock;
}


/*
 * AcceptTcpConnection accepts a TCP connection.
 */

int AcceptTcpConnection(int listenSock)
{
  int sock;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int one = 1;

  sock = accept(listenSock, (struct sockaddr *) &addr, &addrlen);
  if (sock < 0) {
    fprintf(stderr,programName);
    perror(": AcceptTcpConnection: accept");
    return -1;
  }

  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                 (char *)&one, sizeof(one)) < 0) {
    fprintf(stderr,programName);
    perror(": AcceptTcpConnection: setsockopt");
    close(sock);
    return -1;
  }

  return sock;
}


/*
 * StringToIPAddr - convert a host string to an IP address.
 */

bool StringToIPAddr(const char *str, unsigned int *addr)
{
  struct hostent *hp;

  if (strcmp(str,"") == 0) {
    *addr = 0; /* local */
    return true;
  }

  *addr = inet_addr(str);

  if (*addr != (unsigned int)-1)
    return true;

  hp = gethostbyname(str);

  if (hp) {
    *addr = *(unsigned int *)hp->h_addr;
    return true;
  }

  return false;
}
