/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
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
 * vncviewer.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

#include "rfb.h"

#define MAX_ENCODINGS 20

#define FLASH_PORT_OFFSET 5400
#define LISTEN_PORT_OFFSET 5500
#define TUNNEL_PORT_OFFSET 5500
#define SERVER_PORT_OFFSET 5900

#ifdef WIN32
/* This is for the NetworkSimplicty installation of SSH */
#define DEFAULT_SSH_CMD "C:\\Program Files\\NetworkSimplicity\\ssh.exe"
#else
#define DEFAULT_SSH_CMD "/usr/bin/ssh"
#endif

#define DEFAULT_TUNNEL_CMD  \
  (DEFAULT_SSH_CMD " -f -L %L:localhost:%R %H sleep 20")
#define DEFAULT_VIA_CMD     \
  (DEFAULT_SSH_CMD " -f -L %L:%H:%R %G sleep 20")


typedef char Bool;
#ifndef True
#define True 1
#endif
#ifndef False
#define False 0
#endif


/* argsresources.c */

typedef struct {

  char *encodingsString;

  char *passwordFile;

  Bool debug;


  int32_t compressLevel;
  int32_t qualityLevel;
  Bool useRemoteCursor;
  Bool ignoreBlank; /* ignore blank screens */
  Bool enableJPEG;

  int  saveQuality;
  char *outputFilename;

  int quiet;

  char rectXNegative; /* if non-zero, X or Y relative to opposite edge */
  char rectYNegative;
  uint32_t rectWidth;
  uint32_t rectHeight;
  int32_t rectX;
  int32_t rectY;
  char gotCursorPos;
  int fps;
  int count;    /* number of snapshots to grab */
} AppData;

extern AppData appData;

extern char *fallback_resources[];
extern char vncServerHost[];
extern char *vncServerName;
extern uint16_t vncServerPort;
extern Bool listenSpecified;
extern uint16_t listenPort, flashPort;


extern void removeArgs(int *argc, char** argv, int idx, int nargs);
extern void usage(void);
extern void GetArgsAndResources(int argc, char **argv);

/* buffer.c */
extern int AllocateBuffer();
extern void CopyDataToScreen(uint8_t *buffer, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
extern uint8_t *CopyScreenToData(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
extern void FillBufferRectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pixel);
extern void ShrinkBuffer(uint32_t x, uint32_t y, uint32_t req_width, uint32_t req_height);
extern void write_PNG (char * filename, int quality, uint32_t width, uint32_t height);
extern int BufferIsBlank();
extern int BufferWritten();

/* colour.c */

extern uint32_t BGR233ToPixel[];


/* cursor.c */

extern Bool HandleCursorShape(int xhot, int yhot, int width, int height, uint32_t enc);
extern Bool HandleCursorPos(int x, int y);
extern void SoftCursorLockArea(int x, int y, int w, int h);
extern void SoftCursorUnlockScreen(void);
extern void SoftCursorMove(int x, int y);

/* listen.c */

extern void listenForIncomingConnections();

/* rfbproto.c */

extern Bool canUseCoRRE;
extern Bool canUseHextile;
extern char *desktopName;
extern rfbPixelFormat myFormat;
extern rfbServerInitMsg si;
extern uint8_t *serverCutText;
extern Bool newServerCutText;

extern Bool ConnectToRFBServer(const char *hostname, uint16_t port);
extern Bool InitialiseRFBConnection();
extern Bool SendSetPixelFormat();
extern Bool SendSetEncodings();
extern Bool SendIncrementalFramebufferUpdateRequest();
extern Bool SendFramebufferUpdateRequest(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                         Bool incremental);
extern Bool SendPointerEvent(int x, int y, int buttonMask);
extern Bool SendKeyEvent(uint32_t key, Bool down);
extern Bool SendClientCutText(char *str, int len);
extern Bool HandleRFBServerMessage();

extern void PrintPixelFormat(rfbPixelFormat *format);

/* sockets.cxx */

extern Bool sameMachine;
extern int rfbsock;

extern Bool InitializeSockets(void);
extern Bool ConnectToRFBServer(const char *hostname, uint16_t port);
extern Bool SetRFBSock(int sock);
extern void StartTiming();
extern void StopTiming();
extern int KbitsPerSecond();
extern int TimeWaitedIn100us();
extern Bool ReadFromRFBServer(uint8_t *out, size_t n);
extern Bool WriteToRFBServer(uint8_t *buf, size_t n);
extern int ConnectToTcpAddr(const char* hostname, uint16_t port);
extern uint16_t FindFreeTcpPort();
extern int ListenAtTcpPort(uint16_t port);
extern int AcceptTcpConnection(int listenSock);

extern Bool StringToIPAddr(const char *str, unsigned int *addr);


/* tunnel.c */

extern Bool tunnelSpecified;

extern Bool createTunnel(int *argc, char **argv, int tunnelArgIndex);

/* vncviewer.c */

extern char *programName;

/* zrle.cxx */
extern Bool zrleDecode(int x, int y, int w, int h);

/* getpass.c (win32) */
#ifdef WIN32
extern char *getpass(const char * prompt);
#endif
