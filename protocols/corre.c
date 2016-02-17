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
 * corre.c - handle CoRRE encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles a CoRRE
 * encoded rectangle with BPP bits per pixel.
 */

#define HandleCoRREBPP CONCAT2E(HandleCoRRE,BPP)
#ifndef CARDBPP
// XXX CARDBPP is defined elsewhere
#define CARDBPP CONCAT2E(CARD,BPP)
#endif

static Bool
HandleCoRREBPP (uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh)
{
    rfbRREHeader hdr;
    if (!ReadFromRFBServer((uint8_t *)&hdr, sz_rfbRREHeader))
        return False;

    hdr.nSubrects = Swap32IfLE(hdr.nSubrects);

    CARDBPP pix;
    if (!ReadFromRFBServer((uint8_t *)&pix, sizeof(pix)))
        return False;

    FillBufferRectangle(rx, ry, rw, rh, pix);

    if (!ReadFromRFBServer(buffer, hdr.nSubrects * (4 + (BPP / 8))))
        return False;

    uint8_t *ptr = buffer;

    for (uint32_t i = 0; i < hdr.nSubrects; i++) {
        pix = *(CARDBPP *)ptr;
        ptr += BPP/8;
        uint32_t x = *ptr++;
        uint32_t y = *ptr++;
        uint32_t w = *ptr++;
        uint32_t h = *ptr++;

        FillBufferRectangle(rx + x, ry + y, w, h, pix);
    }

    return True;
}
