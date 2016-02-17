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
 * hextile.c - handle hextile encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles a hextile
 * encoded rectangle with BPP bits per pixel.
 */

#define HandleHextileBPP CONCAT2E(HandleHextile,BPP)
#ifndef CARDBPP
// XXX CARDBPP redefined
#define CARDBPP CONCAT2E(CONCAT2E(uint,BPP),_t)
#endif
#define GET_PIXEL CONCAT2E(GET_PIXEL,BPP)

static Bool
HandleHextileBPP (uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh)
{
  CARDBPP bg, fg;
  uint32_t x, y, w, h;
  uint32_t sx, sy, sw, sh;

  for (y = ry; y < ry+rh; y += 16) {
    for (x = rx; x < rx+rw; x += 16) {
      w = h = 16;
      if (rx+rw - x < 16)
        w = rx+rw - x;
      if (ry+rh - y < 16)
        h = ry+rh - y;

      uint8_t subencoding;
      if (!ReadFromRFBServer((uint8_t *)&subencoding, 1))
        return False;

      if (subencoding & rfbHextileRaw) {
        if (!ReadFromRFBServer(buffer, (size_t)(w * h * (BPP / 8))))
          return False;

        CopyDataToScreen(buffer, x, y, w, h);
        continue;
      }

      if (subencoding & rfbHextileBackgroundSpecified)
        if (!ReadFromRFBServer((uint8_t *)&bg, sizeof(bg)))
          return False;

      FillBufferRectangle(x, y, w, h, bg);

      if (subencoding & rfbHextileForegroundSpecified)
        if (!ReadFromRFBServer((uint8_t *)&fg, sizeof(fg)))
          return False;

      if (!(subencoding & rfbHextileAnySubrects)) {
        continue;
      }

      uint8_t nSubrects;
      if (!ReadFromRFBServer((uint8_t *)&nSubrects, 1))
        return False;

      uint8_t *ptr = buffer;

      if (subencoding & rfbHextileSubrectsColoured) {
        if (!ReadFromRFBServer(buffer, (size_t)nSubrects * (2 + (BPP / 8))))
          return False;

        for (uint_fast8_t i = 0; i < nSubrects; i++) {
          GET_PIXEL(fg, ptr);
          sx = rfbHextileExtractX(*ptr);
          sy = rfbHextileExtractY(*ptr);
          ptr++;
          sw = (uint32_t)rfbHextileExtractW(*ptr);
          sh = (uint32_t)rfbHextileExtractH(*ptr);
          ptr++;
          FillBufferRectangle(x+sx, y+sy, sw, sh, fg);
        }

      } else {
        if (!ReadFromRFBServer(buffer, (size_t)nSubrects * 2))
          return False;


        for (uint_fast8_t i = 0; i < nSubrects; i++) {
          sx = rfbHextileExtractX(*ptr);
          sy = rfbHextileExtractY(*ptr);
          ptr++;
          sw = (uint32_t)rfbHextileExtractW(*ptr);
          sh = (uint32_t)rfbHextileExtractH(*ptr);
          ptr++;
          FillBufferRectangle(x+sx, y+sy, sw, sh, fg);
        }
      }
    }
  }

  return True;
}
