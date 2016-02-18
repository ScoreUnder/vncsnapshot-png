/*
 *  Copyright (C) 2002 RealVNC Ltd.
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
 * rfbproto.c - functions to deal with client side of RFB protocol.
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <strings.h>

#ifdef WIN32
#include "vncauth.h"

#define strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) strnicmp(a, b, n)

#else
#include <unistd.h>
#include <pwd.h>
#endif

#include <errno.h>

#include "vncsnapshot.h"
#include "vncauth.h"

#define INT16 jpegINT16
#include <zlib.h>
#include <jpeglib.h>
#undef INT16

#include "getpass.h"

/* do not need non-32 bit versions of these */
static bool HandleRRE32(uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh);
static bool HandleCoRRE32(uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh);
static bool HandleHextile32(uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh);
static bool HandleZlib32(uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh);
static bool HandleTight32(uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh);

static int32_t ReadCompactLen (void);

/* JPEG */
static void JpegInitSource(j_decompress_ptr cinfo);
static boolean JpegFillInputBuffer(j_decompress_ptr cinfo);
static void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes);
static void JpegTermSource(j_decompress_ptr cinfo);
static void JpegSetSrcManager(j_decompress_ptr cinfo, uint8_t *compressedData,
                              size_t compressedLen);

char *desktopName;

rfbPixelFormat myFormat;
bool pendingFormatChange = false;

/*
 * Macro to compare pixel formats.
 */

#define PF_EQ(x,y)                                                      \
        ((x.bitsPerPixel == y.bitsPerPixel) &&                          \
         (x.depth == y.depth) &&                                        \
         ((x.bigEndian == y.bigEndian) || (x.bitsPerPixel == 8)) &&     \
         (x.trueColour == y.trueColour) &&                              \
         (!x.trueColour || ((x.redMax == y.redMax) &&                   \
                            (x.greenMax == y.greenMax) &&               \
                            (x.blueMax == y.blueMax) &&                 \
                            (x.redShift == y.redShift) &&               \
                            (x.greenShift == y.greenShift) &&           \
                            (x.blueShift == y.blueShift))))

int currentEncoding = rfbEncodingZRLE;
bool pendingEncodingChange = false;
int supportedEncodings[] = {
  rfbEncodingZRLE, rfbEncodingHextile, rfbEncodingCoRRE, rfbEncodingRRE,
  rfbEncodingRaw, rfbEncodingTight
};
#define NUM_SUPPORTED_ENCODINGS (sizeof(supportedEncodings)/sizeof(int))

rfbServerInitMsg si;
uint8_t *serverCutText = NULL;
bool newServerCutText = false;

/* note that the CoRRE encoding uses this buffer and assumes it is big enough
   to hold 255 * 255 * 32 bits -> 260100 bytes.  640*480 = 307200 bytes */
/* also hextile assumes it is big enough to hold 16 * 16 * 32 bits */
#define BUFFER_SIZE (640*480)
static uint8_t buffer[BUFFER_SIZE];


/* The zlib encoding requires expansion/decompression/deflation of the
   compressed data in the "buffer" above into another, result buffer.
   However, the size of the result buffer can be determined precisely
   based on the bitsPerPixel, height and width of the rectangle.  We
   allocate this buffer one time to be the full size of the buffer. */

static size_t raw_buffer_size = 0;
static uint8_t *raw_buffer = NULL;

static z_stream decompStream;
static bool decompStreamInited = false;


/*
 * Variables for the ``tight'' encoding implementation.
 */

/* Separate buffer for compressed data. */
#define ZLIB_BUFFER_SIZE 512
static char zlib_buffer[ZLIB_BUFFER_SIZE];

/* Four independent compression streams for zlib library. */
static z_stream zlibStream[4];
static bool zlibStreamActive[4] = {
  false, false, false, false
};

/* Filter stuff. Should be initialized by filter initialization code. */
static bool cutZeros;
static uint32_t rectWidth, rectColors;
static char tightPalette[256*4];
static uint8_t tightPrevRow[2048*3*sizeof(uint16_t)];

/* JPEG decoder state. */
static bool jpegError;


/*
 * InitialiseRFBConnection.
 */

bool
InitialiseRFBConnection()
{
  rfbProtocolVersionMsg pv;
  int major,minor;
  uint32_t authScheme, reasonLen, authResult;
  char *reason;
  uint8_t challenge[CHALLENGESIZE];
  char *passwd;
  rfbClientInitMsg ci;

  if (!ReadFromRFBServer((uint8_t*)pv, sz_rfbProtocolVersionMsg)) return false;

  pv[sz_rfbProtocolVersionMsg] = 0;

  if (sscanf(pv,rfbProtocolVersionFormat,&major,&minor) != 2) {
    fprintf(stderr,"Not a valid VNC server\n");
    return false;
  }

  if (!appData.quiet) {
     fprintf(stderr,"VNC server supports protocol version %d.%d (viewer %d.%d)\n",
             major, minor, rfbProtocolMajorVersion, rfbProtocolMinorVersion);
  }

  major = rfbProtocolMajorVersion;
  minor = rfbProtocolMinorVersion;

  sprintf(pv,rfbProtocolVersionFormat,major,minor);

  if (!WriteToRFBServer((uint8_t*)pv, sz_rfbProtocolVersionMsg)) return false;
  if (!ReadFromRFBServer((uint8_t *)&authScheme, 4)) return false;

  authScheme = Swap32IfLE(authScheme);

  switch (authScheme) {

  case rfbConnFailed:
    if (!ReadFromRFBServer((uint8_t *)&reasonLen, 4)) return false;
    reasonLen = Swap32IfLE(reasonLen);

    reason = malloc(reasonLen);

    if (!ReadFromRFBServer((uint8_t *)reason, reasonLen)) return false;

    fprintf(stderr,"VNC connection failed: %.*s\n",(int)reasonLen, reason);
    return false;

  case rfbNoAuth:
    if (!appData.quiet) {
      fprintf(stderr,"No authentication needed\n");
    }
    break;

  case rfbVncAuth:
    if (!ReadFromRFBServer((uint8_t *)challenge, CHALLENGESIZE)) return false;

    if (appData.passwordFile) {
      passwd = vncDecryptPasswdFromFile(appData.passwordFile);
      if (!passwd) {
        fprintf(stderr,"Cannot read valid password from file \"%s\"\n",
                appData.passwordFile);
        return false;
      }
    } else {
      passwd = getpass("Password: ");
    }

    if ((!passwd) || (strlen(passwd) == 0)) {
      fprintf(stderr,"Reading password failed\n");
      return false;
    }
    if (strlen(passwd) > 8) {
      passwd[8] = '\0';
    }

    vncEncryptBytes(challenge, passwd);

    /* Lose the password from memory */
    for (ssize_t i = (ssize_t) strlen(passwd); i >= 0; i--) {
      passwd[i] = '\0';
    }

    if (!WriteToRFBServer((uint8_t *)challenge, CHALLENGESIZE)) return false;

    if (!ReadFromRFBServer((uint8_t *)&authResult, 4)) return false;

    authResult = Swap32IfLE(authResult);

    switch (authResult) {
    case rfbVncAuthOK:
      if (!appData.quiet) {
        fprintf(stderr,"VNC authentication succeeded\n");
      }
      break;
    case rfbVncAuthFailed:
      fprintf(stderr,"VNC authentication failed\n");
      return false;
    case rfbVncAuthTooMany:
      fprintf(stderr,"VNC authentication failed - too many tries\n");
      return false;
    default:
      fprintf(stderr,"Unknown VNC authentication result: %d\n",
              (int)authResult);
      return false;
    }
    break;

  default:
    fprintf(stderr,"Unknown authentication scheme from VNC server: %d\n",
            (int)authScheme);
    return false;
  }

  ci.shared = 1;

  if (!WriteToRFBServer((uint8_t *)&ci, sz_rfbClientInitMsg)) return false;

  if (!ReadFromRFBServer((uint8_t *)&si, sz_rfbServerInitMsg)) return false;

  si.framebufferWidth = Swap16IfLE(si.framebufferWidth);
  si.framebufferHeight = Swap16IfLE(si.framebufferHeight);
  si.format.redMax = Swap16IfLE(si.format.redMax);
  si.format.greenMax = Swap16IfLE(si.format.greenMax);
  si.format.blueMax = Swap16IfLE(si.format.blueMax);
  si.nameLength = Swap32IfLE(si.nameLength);

  desktopName = malloc(si.nameLength + 1);
  if (!desktopName) {
    fprintf(stderr, "Error allocating memory for desktop name, %" PRIu32 " bytes\n",
            si.nameLength);
    return false;
  }

  if (!ReadFromRFBServer((uint8_t *)desktopName, si.nameLength)) return false;

  desktopName[si.nameLength] = 0;

  if (!appData.quiet) {
    fprintf(stderr,"Desktop name \"%s\"\n",desktopName);

    fprintf(stderr,"Connected to VNC server, using protocol version %d.%d\n",
            rfbProtocolMajorVersion, rfbProtocolMinorVersion);

    fprintf(stderr,"VNC server default format:\n");
    PrintPixelFormat(&si.format);
  }

  return true;
}


bool SendFramebufferUpdateRequest(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool incremental)
{
  rfbFramebufferUpdateRequestMsg fur;

  fur.type = rfbFramebufferUpdateRequest;
  fur.incremental = incremental ? 1 : 0;
  fur.x = Swap16IfLE(x);
  fur.y = Swap16IfLE(y);
  fur.w = Swap16IfLE(w);
  fur.h = Swap16IfLE(h);

  if (!WriteToRFBServer((uint8_t *)&fur, sz_rfbFramebufferUpdateRequestMsg))
    return false;

  return true;
}


bool SendSetPixelFormat()
{
  rfbSetPixelFormatMsg spf;

  spf.type = rfbSetPixelFormat;
  spf.format = myFormat;
  spf.format.redMax = Swap16IfLE(spf.format.redMax);
  spf.format.greenMax = Swap16IfLE(spf.format.greenMax);
  spf.format.blueMax = Swap16IfLE(spf.format.blueMax);
    PrintPixelFormat(&myFormat);

  return WriteToRFBServer((uint8_t *)&spf, sz_rfbSetPixelFormatMsg);
}


bool SendSetEncodings()
{
  uint8_t buf[sz_rfbSetEncodingsMsg + MAX_ENCODINGS * 4];
  rfbSetEncodingsMsg *se = (rfbSetEncodingsMsg *)buf;
  uint32_t *encs = (uint32_t *)(&buf[sz_rfbSetEncodingsMsg]);
  bool requestCompressLevel = false;
  bool requestQualityLevel = false;
  bool requestLastRectEncoding = false;

  se->type = rfbSetEncodings;
  se->nEncodings = 0;

  if (appData.encodingsString) {
    char *encStr = appData.encodingsString;
    size_t encStrLen;
    do {
      char *nextEncStr = strchr(encStr, ' ');
      if (nextEncStr) {
        encStrLen = (size_t)(nextEncStr - encStr);
        nextEncStr++;
      } else {
        encStrLen = strlen(encStr);
      }

      if (strncasecmp(encStr,"raw",encStrLen) == 0) {
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRaw);
      } else if (strncasecmp(encStr,"copyrect",encStrLen) == 0) {
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
      } else if (strncasecmp(encStr,"tight",encStrLen) == 0) {
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingTight);
        requestLastRectEncoding = true;
        if (appData.compressLevel >= 0 && appData.compressLevel <= 9)
          requestCompressLevel = true;
        if (appData.enableJPEG)
          requestQualityLevel = true;
      } else if (strncasecmp(encStr,"hextile",encStrLen) == 0) {
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingHextile);
      } else if (strncasecmp(encStr,"zlib",encStrLen) == 0) {
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZlib);
        if (appData.compressLevel >= 0 && appData.compressLevel <= 9)
          requestCompressLevel = true;
      } else if (strncasecmp(encStr,"corre",encStrLen) == 0) {
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCoRRE);
      } else if (strncasecmp(encStr,"rre",encStrLen) == 0) {
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRRE);
      } else if (strncasecmp(encStr,"zrle",encStrLen) == 0) {
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZRLE);
      } else {
        fprintf(stderr,"Unknown encoding '%.*s'\n",(int)encStrLen,encStr);
      }

      encStr = nextEncStr;
    } while (encStr && se->nEncodings < MAX_ENCODINGS);

    if (se->nEncodings < MAX_ENCODINGS && requestCompressLevel) {
      encs[se->nEncodings++] = Swap32IfLE((uint32_t)appData.compressLevel +
                                          rfbEncodingCompressLevel0);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestQualityLevel) {
      if (appData.qualityLevel < 0 || appData.qualityLevel > 9)
        appData.qualityLevel = 5;
      encs[se->nEncodings++] = Swap32IfLE((uint32_t)appData.qualityLevel +
                                          rfbEncodingQualityLevel0);
    }

      if (se->nEncodings < MAX_ENCODINGS)
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingXCursor);
      if (se->nEncodings < MAX_ENCODINGS)
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRichCursor);
      if (se->nEncodings < MAX_ENCODINGS)
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingPointerPos);

    if (se->nEncodings < MAX_ENCODINGS && requestLastRectEncoding) {
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
    }
  } else {
    if (sameMachine) {
      if (!tunnelSpecified) {
        if (!appData.quiet) {
          fprintf(stderr,"Same machine: preferring raw encoding\n");
        }
        currentEncoding = rfbEncodingRaw;
      } else {
        if (!appData.quiet) {
          fprintf(stderr,"Tunneling active: preferring tight encoding\n");
        }
        currentEncoding = rfbEncodingTight;
      }
    }

    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
    
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
    encs[se->nEncodings++] = Swap32IfLE(currentEncoding);
    for (size_t i = 0; i < NUM_SUPPORTED_ENCODINGS; i++) {
      if (supportedEncodings[i] != currentEncoding)
        encs[se->nEncodings++] = Swap32IfLE(supportedEncodings[i]);
    }

    if (appData.compressLevel >= 0 && appData.compressLevel <= 9) {
      encs[se->nEncodings++] = Swap32IfLE((uint32_t)appData.compressLevel +
                                          rfbEncodingCompressLevel0);
    } else if (!tunnelSpecified) {
      /* If -tunnel option was provided, we assume that server machine is
         not in the local network so we use default compression level for
         tight encoding instead of fast compression. Thus we are
         requesting level 1 compression only if tunneling is not used. */
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCompressLevel1);
    }

    if (appData.enableJPEG) {
      if (appData.qualityLevel < 0 || appData.qualityLevel > 9)
        appData.qualityLevel = 5;
      encs[se->nEncodings++] = Swap32IfLE((uint32_t)appData.qualityLevel +
                                          rfbEncodingQualityLevel0);
    }

      if (se->nEncodings < MAX_ENCODINGS)
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingXCursor);
      if (se->nEncodings < MAX_ENCODINGS)
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRichCursor);
      if (se->nEncodings < MAX_ENCODINGS)
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingPointerPos);

  }

  size_t len = sz_rfbSetEncodingsMsg + (size_t)se->nEncodings * 4;

  se->nEncodings = Swap16IfLE(se->nEncodings);

  return WriteToRFBServer(buf, len);
}


/*
 * SendIncrementalFramebufferUpdateRequest.
 */

bool
SendIncrementalFramebufferUpdateRequest()
{
  return SendFramebufferUpdateRequest(0, 0, si.framebufferWidth,
                                      si.framebufferHeight, true);
}

bool RequestNewUpdate()
{
  if (!SendFramebufferUpdateRequest((uint16_t)appData.rectX, (uint16_t)appData.rectY, (uint16_t)appData.rectWidth,
                                      (uint16_t)appData.rectHeight, true)) {
      return false;
  }

  return true;
}

/*
 * HandleRFBServerMessage.
 */

bool
HandleRFBServerMessage()
{
  rfbServerToClientMsg msg;

  if (!ReadFromRFBServer((uint8_t *)&msg, 1))
    return false;

  switch (msg.type) {

  case rfbSetColourMapEntries:
  {
    fprintf(stderr, "Received unsupported rfbSetColourMapEntries\n");
    return false; /* unsupported */
  }

  case rfbFramebufferUpdate:
  {
    rfbFramebufferUpdateRectHeader rect;

    if (!ReadFromRFBServer(((uint8_t *)&msg.fu) + 1,
                           sz_rfbFramebufferUpdateMsg - 1))
      return false;

    msg.fu.nRects = Swap16IfLE(msg.fu.nRects);

    for (size_t i = 0; i < msg.fu.nRects; i++) {
      if (!ReadFromRFBServer((uint8_t *)&rect, sz_rfbFramebufferUpdateRectHeader))
        return false;

      rect.encoding = Swap32IfLE(rect.encoding);
      if (rect.encoding == rfbEncodingLastRect)
          break;

      rect.r.x = Swap16IfLE(rect.r.x);
      rect.r.y = Swap16IfLE(rect.r.y);
      rect.r.w = Swap16IfLE(rect.r.w);
      rect.r.h = Swap16IfLE(rect.r.h);

      if (rect.encoding == rfbEncodingXCursor || rect.encoding == rfbEncodingRichCursor) {
        if (!HandleCursorShape(rect.r.x, rect.r.y, rect.r.w, rect.r.h, rect.encoding)) {
          return false;
        }
        continue;
      }

      if (rect.encoding == rfbEncodingPointerPos) {
        if (!HandleCursorPos(rect.r.x, rect.r.y)) {
          return false;
        }
        appData.gotCursorPos = 1;
        continue;
      }

      if ((rect.r.x + rect.r.w > si.framebufferWidth) ||
          (rect.r.y + rect.r.h > si.framebufferHeight))
        {
          fprintf(stderr,"Rect too large: %dx%d at (%d, %d)\n",
                  rect.r.w, rect.r.h, rect.r.x, rect.r.y);
          return false;
        }

      if ((rect.r.h * rect.r.w) == 0) {
        fprintf(stderr,"Zero size rect - ignoring\n");
        continue;
      }

      /* If RichCursor encoding is used, we should prevent collisions
         between framebuffer updates and cursor drawing operations. */
      SoftCursorLockArea(rect.r.x, rect.r.y, rect.r.w, rect.r.h);

      switch (rect.encoding) {

      case rfbEncodingRaw:
      {
        size_t bytesPerLine = (size_t)rect.r.w * myFormat.bitsPerPixel / 8;
        size_t linesToRead = BUFFER_SIZE / bytesPerLine;

        while (rect.r.h > 0) {
          if (linesToRead > rect.r.h)
            linesToRead = rect.r.h;

          if (!ReadFromRFBServer(buffer, bytesPerLine * linesToRead))
            return false;
          assert(linesToRead <= UINT32_MAX);
          CopyDataToScreen(buffer, rect.r.x, rect.r.y, rect.r.w,
                           (uint32_t) linesToRead);

          rect.r.h = (uint16_t) (rect.r.h - linesToRead);
          rect.r.y = (uint16_t) (rect.r.y + linesToRead);

        }
        break;
      }

      case rfbEncodingCopyRect:
      {
        rfbCopyRect cr;

        if (!ReadFromRFBServer((uint8_t *)&cr, sz_rfbCopyRect))
          return false;

          if (!BufferWritten()) {
            /* Ignore attempts to do copy-rect when we have nothing to
             * copy from.
             */
            break;
        }

        cr.srcX = Swap16IfLE(cr.srcX);
        cr.srcY = Swap16IfLE(cr.srcY);

        /* If RichCursor encoding is used, we should extend our
           "cursor lock area" (previously set to destination
           rectangle) to the source rectangle as well. */
        SoftCursorLockArea(cr.srcX, cr.srcY, rect.r.w, rect.r.h);

        uint8_t *buffer = CopyScreenToData(cr.srcX, cr.srcY, rect.r.w, rect.r.h);
        CopyDataToScreen(buffer, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
        free(buffer);

        break;
      }

      case rfbEncodingRRE:
      {
        if (!HandleRRE32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return false;
        break;
      }

      case rfbEncodingCoRRE:
      {
        if (!HandleCoRRE32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
          return false;
        break;
      }

      case rfbEncodingHextile:
      {
        if (!HandleHextile32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return false;
        break;
      }

      case rfbEncodingZlib:
      {
        if (!HandleZlib32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return false;
        break;
     }

      case rfbEncodingTight:
      {
        if (!HandleTight32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return false;
        break;
      }

      case rfbEncodingZRLE:
        if (!zrleDecode(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
          return false;
        break;

      default:
        fprintf(stderr,"Unknown rect encoding %d\n",
                (int)rect.encoding);
        return false;
      }

      /* Now we may discard "soft cursor locks". */
      SoftCursorUnlockScreen();

        /* Done. Save the screen image. */
    }

      /* RealVNC sometimes returns an initial black screen. */
      if (BufferIsBlank() && appData.ignoreBlank) {
          if (!appData.quiet && appData.ignoreBlank != 1) {
              /* user did not specify either -quiet or -ignoreblank */
              fprintf(stderr, "Warning: discarding received blank screen (use -allowblank to accept,\n   or -ignoreblank to suppress this message)\n");
              appData.ignoreBlank = 1;
          }
          RequestNewUpdate();
      } else {
          return false;
      }

    break;
  }

  case rfbBell:
    /* ignore */
    break;

  case rfbServerCutText:
  {
    if (!ReadFromRFBServer(((uint8_t *)&msg) + 1,
                           sz_rfbServerCutTextMsg - 1))
      return false;

    msg.sct.length = Swap32IfLE(msg.sct.length);

    if (serverCutText)
      free(serverCutText);

    serverCutText = malloc(msg.sct.length+1);

    if (!ReadFromRFBServer(serverCutText, msg.sct.length))
      return false;

    serverCutText[msg.sct.length] = 0;

    newServerCutText = true;

    break;
  }

  default:
    fprintf(stderr,"Unknown message type %d from VNC server\n",msg.type);
    return false;
  }

  return true;
}


#define GET_PIXEL8(pix, ptr) ((pix) = *(ptr)++)

#define GET_PIXEL16(pix, ptr) (((uint8_t*)&(pix))[0] = *(ptr)++, \
                               ((uint8_t*)&(pix))[1] = *(ptr)++)

#ifdef __APPLE__
/* Apple compilation appears to be broken.*/
static inline void GET_PIXEL32(void *pix, uint8_t *ptr)
{
    ((uint8_t*)&(pix))[0] = *(ptr)++;
    ((uint8_t*)&(pix))[1] = *(ptr)++;
    ((uint8_t*)&(pix))[2] = *(ptr)++;
    ((uint8_t*)&(pix))[3] = *(ptr)++;
}
#endif
#define GET_PIXEL32(pix, ptr) (((uint8_t*)&(pix))[0] = *(ptr)++, \
                               ((uint8_t*)&(pix))[1] = *(ptr)++, \
                               ((uint8_t*)&(pix))[2] = *(ptr)++, \
                               ((uint8_t*)&(pix))[3] = *(ptr)++)

/* CONCAT2 concatenates its two arguments.  CONCAT2E does the same but also
   expands its arguments if they are macros */

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)

#define BPP 32
#include "protocols/rre.c"
#include "protocols/corre.c"
#include "protocols/hextile.c"
#include "protocols/zlib.c"
#include "protocols/tight.c"
#undef BPP


/*
 * PrintPixelFormat.
 */

void
PrintPixelFormat(format)
    rfbPixelFormat *format;
{
  if (format->bitsPerPixel == 1) {
    fprintf(stderr,"  Single bit per pixel.\n");
    fprintf(stderr,
            "  %s significant bit in each byte is leftmost on the screen.\n",
            (format->bigEndian ? "Most" : "Least"));
  } else {
    fprintf(stderr,"  %d bits per pixel.\n",format->bitsPerPixel);
    if (format->bitsPerPixel != 8) {
      fprintf(stderr,"  %s significant byte first in each pixel.\n",
              (format->bigEndian ? "Most" : "Least"));
    }
    if (format->trueColour) {
      fprintf(stderr,"  true colour: max red %d green %d blue %d",
              format->redMax, format->greenMax, format->blueMax);
      fprintf(stderr,", shift red %d green %d blue %d\n",
              format->redShift, format->greenShift, format->blueShift);
    } else {
      fprintf(stderr,"  Colour map (not true colour).\n");
    }
  }
}

static int32_t
ReadCompactLen (void)
{
  uint32_t len;
  uint8_t b;

  if (!ReadFromRFBServer((uint8_t *)&b, 1))
    return -1;
  len = (uint32_t)b & 0x7F;
  if (b & 0x80) {
    if (!ReadFromRFBServer((uint8_t *)&b, 1))
      return -1;
    len |= ((uint32_t)b & 0x7F) << 7;
    if (b & 0x80) {
      if (!ReadFromRFBServer((uint8_t *)&b, 1))
        return -1;
      len |= ((uint32_t)b & 0xFF) << 14;
    }
  }
  return (int32_t)len;
}

/*
 * JPEG source manager functions for JPEG decompression in Tight decoder.
 */

static struct jpeg_source_mgr jpegSrcManager;
static JOCTET *jpegBufferPtr;
static size_t jpegBufferLen;

static void
JpegInitSource(j_decompress_ptr cinfo)
{
  (void) cinfo;
  jpegError = false;
}

static boolean
JpegFillInputBuffer(j_decompress_ptr cinfo)
{
  (void) cinfo;
  jpegError = true;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;
  jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;

  return TRUE;
}

static void
JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes)
{
  (void) cinfo;
  if (num_bytes < 0 || (unsigned int) num_bytes > jpegSrcManager.bytes_in_buffer) {
    jpegError = true;
    jpegSrcManager.bytes_in_buffer = jpegBufferLen;
    jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;
  } else {
    jpegSrcManager.next_input_byte += (size_t) num_bytes;
    jpegSrcManager.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
JpegTermSource(j_decompress_ptr cinfo)
{
  /* No work necessary here. */
  (void) cinfo;
}

static void
JpegSetSrcManager(j_decompress_ptr cinfo, uint8_t *compressedData,
                  size_t compressedLen)
{
  jpegBufferPtr = (JOCTET *)compressedData;
  jpegBufferLen = compressedLen;

  jpegSrcManager.init_source = JpegInitSource;
  jpegSrcManager.fill_input_buffer = JpegFillInputBuffer;
  jpegSrcManager.skip_input_data = JpegSkipInputData;
  jpegSrcManager.resync_to_restart = jpeg_resync_to_restart;
  jpegSrcManager.term_source = JpegTermSource;
  jpegSrcManager.next_input_byte = jpegBufferPtr;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;

  cinfo->src = &jpegSrcManager;
}

