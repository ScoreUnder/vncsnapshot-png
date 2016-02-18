/*
 *  Copyright (C) 2001,2002 Constantin Kaplinsky.  All Rights Reserved.
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
 * cursor.c - code to support cursor shape updates (XCursor and
 * RichCursor preudo-encodings).
 */

#include <assert.h>
#include <stdbool.h>

#include "vncsnapshot.h"


#define OPER_SAVE     0
#define OPER_RESTORE  1

#define RGB24_TO_PIXEL(bpp,r,g,b)                                       \
   ((((uint##bpp##_t)(r) & 0xFF) * myFormat.redMax + 127) / 255             \
    << myFormat.redShift |                                              \
    (((uint##bpp##_t)(g) & 0xFF) * myFormat.greenMax + 127) / 255           \
    << myFormat.greenShift |                                            \
    (((uint##bpp##_t)(b) & 0xFF) * myFormat.blueMax + 127) / 255            \
    << myFormat.blueShift)


static bool prevSoftCursorSet = false;
static uint8_t *rcSavedArea, *rcSource, *rcMask;
static int rcHotX, rcHotY, rcWidth, rcHeight;
static int rcCursorX = 0, rcCursorY = 0;
static int rcLockX, rcLockY, rcLockWidth, rcLockHeight;
static bool rcCursorHidden, rcLockSet;

static bool SoftCursorInLockedArea(void);
static void SoftCursorCopyArea(int oper);
static void SoftCursorDraw(void);
static void FreeSoftCursor(void);


/*********************************************************************
 * HandleCursorShape(). Support for XCursor and RichCursor shape
 * updates. We emulate cursor operating on the frame buffer (that is
 * why we call it "software cursor").
 ********************************************************************/

bool HandleCursorShape(int xhot, int yhot, int width, int height, uint32_t enc)
{
  int bytesPerPixel;
  size_t bytesPerRow, bytesMaskData;
/*  Drawable dr;*/
  rfbXCursorColors rgb;

  assert(width >= 0);
  assert(height >= 0);
  assert(SIZE_MAX / (size_t) width >= (size_t) height);  /* Overflow check for safety since we malloc this */

  bytesPerPixel = myFormat.bitsPerPixel / 8;
  bytesPerRow = (size_t) ((width + 7) / 8);
  bytesMaskData = bytesPerRow * (size_t) height;
/*  dr = DefaultRootWindow(dpy);*/

  FreeSoftCursor();

  if (width * height == 0)
    return true;

  /* Allocate memory for pixel data and temporary mask data. */

  rcSource = malloc((size_t) (width * height * bytesPerPixel));
  if (rcSource == NULL)
    return false;

  uint8_t *buf = malloc(bytesMaskData);
  if (buf == NULL) {
    free(rcSource);
    return false;
  }

  /* Read and decode cursor pixel data, depending on the encoding type. */

  if (enc == rfbEncodingXCursor) {

    /* Read and convert background and foreground colors. */
    if (!ReadFromRFBServer((uint8_t *)&rgb, sz_rfbXCursorColors)) {
      free(rcSource);
      free(buf);
      return false;
    }
    uint32_t colors[2] = {
      RGB24_TO_PIXEL(32, rgb.backRed, rgb.backGreen, rgb.backBlue),
      RGB24_TO_PIXEL(32, rgb.foreRed, rgb.foreGreen, rgb.foreBlue),
    };

    /* Read 1bpp pixel data into a temporary buffer. */
    if (!ReadFromRFBServer(buf, bytesMaskData)) {
      free(rcSource);
      free(buf);
      return false;
    }

    /* Convert 1bpp data to byte-wide color indices. */
    uint8_t *ptr = rcSource;
    for (size_t y = 0; y < (size_t) height; y++) {
      size_t x;
      for (x = 0; x < (size_t) width / 8; x++) {
        for (int b = 7; b >= 0; b--) {
          *ptr = buf[y * bytesPerRow + x] >> b & 1;
          ptr += bytesPerPixel;
        }
      }
      for (int b = 7; b > 7 - width % 8; b--) {
        *ptr = buf[y * bytesPerRow + x] >> b & 1;
        ptr += bytesPerPixel;
      }
    }

    /* Convert indices into the actual pixel values. */
    switch (bytesPerPixel) {
    case 1:
      for (size_t x = 0; x < (size_t)(width * height); x++)
        rcSource[x] = (uint8_t) colors[rcSource[x]];
      break;
    case 2:
      for (size_t x = 0; x < (size_t)(width * height); x++)
        ((uint16_t *)rcSource)[x] = (uint16_t) colors[rcSource[x * 2]];
      break;
    case 4:
      for (size_t x = 0; x < (size_t)(width * height); x++)
        ((uint32_t *)rcSource)[x] = colors[rcSource[x * 4]];
      break;
    }

  } else {                      /* enc == rfbEncodingRichCursor */

    if (!ReadFromRFBServer((uint8_t *)rcSource, (size_t)(width * height * bytesPerPixel))) {
      free(rcSource);
      free(buf);
      return false;
    }

  }

  /* Read and decode mask data. */

  if (!ReadFromRFBServer(buf, bytesMaskData)) {
    free(rcSource);
    free(buf);
    return false;
  }

  // Overflow checked earlier in the function
  rcMask = malloc((size_t)(width * height));
  if (rcMask == NULL) {
    free(rcSource);
    free(buf);
    return false;
  }

  uint8_t *ptr = rcMask;
  for (size_t y = 0; y < (size_t)height; y++) {
    size_t x;
    for (x = 0; x < (size_t)width / 8; x++) {
      for (int b = 7; b >= 0; b--) {
        *ptr++ = buf[y * bytesPerRow + x] >> b & 1;
      }
    }
    for (int b = 7; b > 7 - width % 8; b--) {
      *ptr++ = buf[y * bytesPerRow + x] >> b & 1;
    }
  }

  free(buf);

  /* Set remaining data associated with cursor. */

/*  dr = DefaultRootWindow(dpy);
  rcSavedArea = XCreatePixmap(dpy, dr, width, height, visdepth);*/
  rcHotX = xhot;
  rcHotY = yhot;
  rcWidth = width;
  rcHeight = height;
  /* Do not draw. Only draw when we have the position. */
  SoftCursorCopyArea(OPER_SAVE);
  /*SoftCursorDraw();*/

  rcCursorHidden = false;
  rcLockSet = false;

  prevSoftCursorSet = true;
  return true;
}

/*********************************************************************
 * HandleCursorPos(). Support for the PointerPos pseudo-encoding used
 * to transmit changes in pointer position from server to clients.
 * PointerPos encoding is used together with cursor shape updates.
 ********************************************************************/

bool HandleCursorPos(int x, int y)
{

  if (x >= si.framebufferWidth)
    x = si.framebufferWidth - 1;
  if (y >= si.framebufferHeight)
    y = si.framebufferHeight - 1;

  SoftCursorMove(x, y);
  return true;
}

/*********************************************************************
 * SoftCursorLockArea(). This function should be used to prevent
 * collisions between simultaneous framebuffer update operations and
 * cursor drawing operations caused by movements of pointing device.
 * The parameters denote a rectangle where mouse cursor should not be
 * drawn. Every next call to this function expands locked area so
 * previous locks remain active.
 ********************************************************************/

void SoftCursorLockArea(int x, int y, int w, int h)
{
  int newX, newY;

  if (!BufferWritten()) {
      return;    /* no cursor to hide */
  }

  if (!prevSoftCursorSet)
    return;

  if (!rcLockSet) {
    rcLockX = x;
    rcLockY = y;
    rcLockWidth = w;
    rcLockHeight = h;
    rcLockSet = true;
  } else {
    newX = (x < rcLockX) ? x : rcLockX;
    newY = (y < rcLockY) ? y : rcLockY;
    rcLockWidth = (x + w > rcLockX + rcLockWidth) ?
      (x + w - newX) : (rcLockX + rcLockWidth - newX);
    rcLockHeight = (y + h > rcLockY + rcLockHeight) ?
      (y + h - newY) : (rcLockY + rcLockHeight - newY);
    rcLockX = newX;
    rcLockY = newY;
  }

  if (!rcCursorHidden && SoftCursorInLockedArea()) {
    SoftCursorCopyArea(OPER_RESTORE);
    rcCursorHidden = true;
  }
}

/*********************************************************************
 * SoftCursorUnlockScreen(). This function discards all locks
 * performed since previous SoftCursorUnlockScreen() call.
 ********************************************************************/

void SoftCursorUnlockScreen(void)
{
  if (!prevSoftCursorSet)
    return;

  if (rcCursorHidden) {
    SoftCursorCopyArea(OPER_SAVE);
    if (appData.useRemoteCursor == 1) {
    SoftCursorDraw();
    }
    rcCursorHidden = false;
  }
  rcLockSet = false;
}

/*********************************************************************
 * SoftCursorMove(). Moves soft cursor into a particular location. 
 * This function respects locking of screen areas so when the cursor
 * is moved into the locked area, it becomes invisible until
 * SoftCursorUnlock() functions is called.
 ********************************************************************/

void SoftCursorMove(int x, int y)
{
  if (prevSoftCursorSet && !rcCursorHidden) {
    SoftCursorCopyArea(OPER_RESTORE);
    rcCursorHidden = true;
  }

  rcCursorX = x;
  rcCursorY = y;

  if (prevSoftCursorSet && !(rcLockSet && SoftCursorInLockedArea())) {
    SoftCursorCopyArea(OPER_SAVE);
   if (appData.useRemoteCursor == 1) {
    SoftCursorDraw();
   }
    rcCursorHidden = false;
  }
}


/*********************************************************************
 * Internal (static) low-level functions.
 ********************************************************************/

static bool SoftCursorInLockedArea(void)
{
  return (rcLockX < rcCursorX - rcHotX + rcWidth &&
          rcLockY < rcCursorY - rcHotY + rcHeight &&
          rcLockX + rcLockWidth > rcCursorX - rcHotX &&
          rcLockY + rcLockHeight > rcCursorY - rcHotY);
}

static void SoftCursorCopyArea(int oper)
{
  int32_t x = rcCursorX - rcHotX;
  int32_t y = rcCursorY - rcHotY;
  if (x >= si.framebufferWidth || y >= si.framebufferHeight)
    return;

  int32_t w = rcWidth;
  int32_t h = rcHeight;
  if (x < 0) {
    w += x;
    x = 0;
  } else if (x + w > si.framebufferWidth) {
    w = si.framebufferWidth - x;
  }
  if (y < 0) {
    h += y;
    y = 0;
  } else if (y + h > si.framebufferHeight) {
    h = si.framebufferHeight - y;
  }

  if (oper == OPER_SAVE) {
    /* Save screen area in memory. */
    if (rcSavedArea != NULL) {
        free(rcSavedArea);
        rcSavedArea = NULL;
    }
    rcSavedArea = CopyScreenToData((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h);
  } else {
    /* Restore screen area. */
    CopyDataToScreen(rcSavedArea, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h);
  }
}

static void SoftCursorDraw(void)
{
  uint_fast8_t bytesPerPixel;
  uint8_t *pos;

  bytesPerPixel = myFormat.bitsPerPixel / 8;

  /* FIXME: Speed optimization is possible. */
  for (int32_t y = 0; y < rcHeight; y++) {
    int32_t y0 = rcCursorY - rcHotY + y;
    if (y0 >= 0 && y0 < si.framebufferHeight) {
      for (int32_t x = 0; x < rcWidth; x++) {
        int32_t x0 = rcCursorX - rcHotX + x;
        if (x0 >= 0 && x0 < si.framebufferWidth) {
          size_t offset = (size_t)(y * rcWidth + x);
          if (rcMask[offset]) {
            pos = &rcSource[offset * bytesPerPixel];
            CopyDataToScreen(pos, (uint32_t)x0, (uint32_t)y0, 1, 1);
          }
        }
      }
    }
  }
}

static void FreeSoftCursor(void)
{
  if (prevSoftCursorSet) {
    SoftCursorCopyArea(OPER_RESTORE);
    if (rcSavedArea != NULL) {
        free(rcSavedArea);
        rcSavedArea = NULL;
    }
    free(rcSource);
    free(rcMask);
    prevSoftCursorSet = false;
  }
}

