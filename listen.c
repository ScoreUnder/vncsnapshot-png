/*
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
 * listen.c - listen for incoming connections
 */

#ifndef WIN32
#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/resource.h>

typedef int SOCKET;
#else
#include <winsock2.h>
#define close(x) closesocket(x)
#endif

#include <stdint.h>

#include "vncsnapshot.h"

#define FLASHWIDTH 50   /* pixels */
#define FLASHDELAY 1    /* seconds */

Bool listenSpecified = False;
uint16_t listenPort = 0, flashPort = 0;



/*
 * listenForIncomingConnections() - listen for incoming connections from
 * servers, and fork a new process to deal with each connection.  We must do
 * all this before invoking any Xt functions - this is because Xt doesn't
 * cope with forking very well.
 */

void
listenForIncomingConnections(int *argc, char **argv, int listenArgIndex)
{
  listenSpecified = True;

  if (listenArgIndex+1 < *argc && argv[listenArgIndex+1][0] >= '0' &&
                                            argv[listenArgIndex+1][0] <= '9') {

    listenPort = (uint16_t) (LISTEN_PORT_OFFSET + atoi(argv[listenArgIndex+1]));
    flashPort = (uint16_t) (FLASH_PORT_OFFSET + atoi(argv[listenArgIndex+1]));
    removeArgs(argc, argv, listenArgIndex, 2);

  } else {
      fprintf(stderr,"%s: Please specify which display to listen on with -listen <num>\n",
              programName);
      exit(1);
  }

  SOCKET listenSocket = ListenAtTcpPort(listenPort);
  SOCKET flashSocket = ListenAtTcpPort(flashPort);

  if ((listenSocket < 0) || (flashSocket < 0)) exit(1);

  fprintf(stderr,"%s -listen: Listening on port %d (flash port %d)\n",
          programName,listenPort,flashPort);
  fprintf(stderr,"%s -listen: Command line errors are not reported until "
          "a connection comes in.\n", programName);

  while (True) {

#ifndef WIN32
    /* reap any zombies */
    int status, pid;
    while ((pid = wait3(&status, WNOHANG, (struct rusage *)0))>0);
#endif

    fd_set fds;
    FD_ZERO(&fds);

    FD_SET(flashSocket, &fds);
    FD_SET(listenSocket, &fds);

    select(FD_SETSIZE, &fds, NULL, NULL, NULL);

    if (FD_ISSET(flashSocket, &fds)) {

      SOCKET sock = AcceptTcpConnection(flashSocket);
      if (sock < 0) exit(1);
      char flashUser[256];
      ssize_t n = recv(sock, flashUser, 255, 0);
      if (n > 0) {
        flashUser[n] = 0;
      }
      close(sock);
    }

    if (FD_ISSET(listenSocket, &fds)) {
      rfbsock = AcceptTcpConnection(listenSocket);
      if (rfbsock < 0) exit(1);

      /* Unlike a standard VNC client, we don't continue to listen. */
      /* Return to caller. */
      close(listenSocket);
      close(flashSocket);
      return;

    }
  }
}
