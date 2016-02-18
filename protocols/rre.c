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
 * rre.c - handle RRE encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles an RRE
 * encoded rectangle with BPP bits per pixel.
 */

#include <stdbool.h>
#define HandleRREBPP CONCAT2E(HandleRRE,BPP)
#define CARDBPP CONCAT2E(CONCAT2E(uint,BPP),_t)

static bool
HandleRREBPP (uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh)
{
  rfbRREHeader hdr;
  CARDBPP pix;
  rfbRectangle subrect;

  if (!ReadFromRFBServer((uint8_t *)&hdr, sz_rfbRREHeader))
    return false;

  hdr.nSubrects = Swap32IfLE(hdr.nSubrects);

  if (!ReadFromRFBServer((uint8_t *)&pix, sizeof(pix)))
    return false;

  FillBufferRectangle(rx, ry, rw, rh, pix);

  for (uint32_t i = 0; i < hdr.nSubrects; i++) {
    if (!ReadFromRFBServer((uint8_t *)&pix, sizeof(pix)))
      return false;

    if (!ReadFromRFBServer((uint8_t *)&subrect, sz_rfbRectangle))
      return false;

    subrect.x = Swap16IfLE(subrect.x);
    subrect.y = Swap16IfLE(subrect.y);
    subrect.w = Swap16IfLE(subrect.w);
    subrect.h = Swap16IfLE(subrect.h);

    FillBufferRectangle(rx + subrect.x, ry + subrect.y,
                   subrect.w, subrect.h, pix);
  }

  return true;
}
