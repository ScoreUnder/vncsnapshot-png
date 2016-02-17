/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
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
 * tight.c - handle ``tight'' encoding.
 *
 * This file shouldn't be compiled directly. It is included multiple
 * times by rfbproto.c, each time with a different definition of the
 * macro BPP. For each value of BPP, this file defines a function
 * which handles a tight-encoded rectangle with BPP bits per pixel.
 *
 */

#define TIGHT_MIN_TO_COMPRESS 12

#define CARDBPP CONCAT2E(CONCAT2E(uint,BPP),_t)
#define filterPtrBPP CONCAT2E(filterPtr,BPP)

#define HandleTightBPP CONCAT2E(HandleTight,BPP)
#define InitFilterCopyBPP CONCAT2E(InitFilterCopy,BPP)
#define InitFilterPaletteBPP CONCAT2E(InitFilterPalette,BPP)
#define InitFilterGradientBPP CONCAT2E(InitFilterGradient,BPP)
#define FilterCopyBPP CONCAT2E(FilterCopy,BPP)
#define FilterPaletteBPP CONCAT2E(FilterPalette,BPP)
#define FilterGradientBPP CONCAT2E(FilterGradient,BPP)

#if BPP != 8
#define DecompressJpegRectBPP CONCAT2E(DecompressJpegRect,BPP)
#endif

#ifndef RGB_TO_PIXEL

#define RGB_TO_PIXEL(bpp,r,g,b)                                         \
  (((CARD##bpp)(r) & myFormat.redMax) << myFormat.redShift |            \
   ((CARD##bpp)(g) & myFormat.greenMax) << myFormat.greenShift |        \
   ((CARD##bpp)(b) & myFormat.blueMax) << myFormat.blueShift)

#define RGB24_TO_PIXEL(bpp,r,g,b)                                       \
   ((((CARD##bpp)(r) & 0xFF) * myFormat.redMax + 127) / 255             \
    << myFormat.redShift |                                              \
    (((CARD##bpp)(g) & 0xFF) * myFormat.greenMax + 127) / 255           \
    << myFormat.greenShift |                                            \
    (((CARD##bpp)(b) & 0xFF) * myFormat.blueMax + 127) / 255            \
    << myFormat.blueShift)

#define RGB24_TO_PIXEL32(r,g,b)                                         \
  (((uint32_t)(r) & 0xFF) << myFormat.redShift |                          \
   ((uint32_t)(g) & 0xFF) << myFormat.greenShift |                        \
   ((uint32_t)(b) & 0xFF) << myFormat.blueShift)

#endif

/* Type declarations */

typedef void (*filterPtrBPP)(size_t, CARDBPP *);

/* Prototypes */

static uint_fast8_t InitFilterCopyBPP (uint32_t rw, uint32_t rh);
static uint_fast8_t InitFilterPaletteBPP (uint32_t rw, uint32_t rh);
static uint_fast8_t InitFilterGradientBPP (uint32_t rw, uint32_t rh);
static void FilterCopyBPP (size_t numRows, CARDBPP *destBuffer);
static void FilterPaletteBPP (size_t numRows, CARDBPP *destBuffer);
static void FilterGradientBPP (size_t numRows, CARDBPP *destBuffer);

static Bool DecompressJpegRectBPP(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

/* Definitions */

static Bool
HandleTightBPP (uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh)
{
  CARDBPP fill_colour;
  uint8_t comp_ctl;
  uint8_t filter_id;
  filterPtrBPP filterFn;
  z_streamp zs;
  uint8_t *buffer2;
  int err, stream_id, compressedLen;
  uint_fast8_t bitsPixel;

  if (!ReadFromRFBServer((uint8_t *)&comp_ctl, 1))
    return False;

  /* Flush zlib streams if we are told by the server to do so. */
  for (stream_id = 0; stream_id < 4; stream_id++) {
    if ((comp_ctl & 1) && zlibStreamActive[stream_id]) {
      if (inflateEnd (&zlibStream[stream_id]) != Z_OK &&
          zlibStream[stream_id].msg != NULL)
        fprintf(stderr, "inflateEnd: %s\n", zlibStream[stream_id].msg);
      zlibStreamActive[stream_id] = False;
    }
    comp_ctl >>= 1;
  }

  /* Handle solid rectangles. */
  if (comp_ctl == rfbTightFill) {
#if BPP == 32
    if (myFormat.depth == 24 && myFormat.redMax == 0xFF &&
        myFormat.greenMax == 0xFF && myFormat.blueMax == 0xFF) {
      if (!ReadFromRFBServer(buffer, 3))
        return False;
      fill_colour = RGB24_TO_PIXEL32(buffer[0], buffer[1], buffer[2]);
    } else {
      if (!ReadFromRFBServer((uint8_t*)&fill_colour, sizeof(fill_colour)))
        return False;
    }
#else
    if (!ReadFromRFBServer((uint8_t*)&fill_colour, sizeof(fill_colour)))
        return False;
#endif

    FillBufferRectangle(rx, ry, rw, rh, fill_colour);
    return True;
  }

#if BPP == 8
  if (comp_ctl == rfbTightJpeg) {
    fprintf(stderr, "Tight encoding: JPEG is not supported in 8 bpp mode.\n");
    return False;
  }
#else
  if (comp_ctl == rfbTightJpeg) {
    return DecompressJpegRectBPP(rx, ry, rw, rh);
  }
#endif

  /* Quit on unsupported subencoding value. */
  if (comp_ctl > rfbTightMaxSubencoding) {
    fprintf(stderr, "Tight encoding: bad subencoding value received.\n");
    return False;
  }

  /*
   * Here primary compression mode handling begins.
   * Data was processed with optional filter + zlib compression.
   */

  /* First, we should identify a filter to use. */
  if ((comp_ctl & rfbTightExplicitFilter) != 0) {
    if (!ReadFromRFBServer((uint8_t*)&filter_id, 1))
      return False;

    switch (filter_id) {
    case rfbTightFilterCopy:
      filterFn = FilterCopyBPP;
      bitsPixel = InitFilterCopyBPP(rw, rh);
      break;
    case rfbTightFilterPalette:
      filterFn = FilterPaletteBPP;
      bitsPixel = InitFilterPaletteBPP(rw, rh);
      break;
    case rfbTightFilterGradient:
      filterFn = FilterGradientBPP;
      bitsPixel = InitFilterGradientBPP(rw, rh);
      break;
    default:
      fprintf(stderr, "Tight encoding: unknown filter code received.\n");
      return False;
    }
  } else {
    filterFn = FilterCopyBPP;
    bitsPixel = InitFilterCopyBPP(rw, rh);
  }
  if (bitsPixel == 0) {
    fprintf(stderr, "Tight encoding: error receiving palette.\n");
    return False;
  }

  /* Determine if the data should be decompressed or just copied. */
  size_t rowSize = (size_t) (rw * bitsPixel + 7) / 8;
  if ((size_t)rh * rowSize < TIGHT_MIN_TO_COMPRESS) {
    if (!ReadFromRFBServer((uint8_t*)buffer, (size_t)rh * rowSize))
      return False;

    buffer2 = &buffer[TIGHT_MIN_TO_COMPRESS * 4];
    filterFn(rh, (CARDBPP *)buffer2);
    CopyDataToScreen(buffer2, rx, ry, rw, rh);

    return True;
  }

  /* Read the length (1..3 bytes) of compressed data following. */
  compressedLen = (int)ReadCompactLen();
  if (compressedLen <= 0) {
    fprintf(stderr, "Incorrect data received from the server.\n");
    return False;
  }

  /* Now let's initialize compression stream if needed. */
  stream_id = comp_ctl & 0x03;
  zs = &zlibStream[stream_id];
  if (!zlibStreamActive[stream_id]) {
    zs->zalloc = Z_NULL;
    zs->zfree = Z_NULL;
    zs->opaque = Z_NULL;
    err = inflateInit(zs);
    if (err != Z_OK) {
      if (zs->msg != NULL)
        fprintf(stderr, "InflateInit error: %s.\n", zs->msg);
      return False;
    }
    zlibStreamActive[stream_id] = True;
  }

  /* Read, decode and draw actual pixel data in a loop. */

  size_t bufferSize = BUFFER_SIZE * (size_t)bitsPixel / ((size_t)bitsPixel + BPP) & 0xFFFFFFFC;
  buffer2 = &buffer[bufferSize];
  if (rowSize > bufferSize) {
    /* Should be impossible when BUFFER_SIZE >= 16384 */
    fprintf(stderr, "Internal error: incorrect buffer size.\n");
    return False;
  }

  size_t rowsProcessed = 0;
  size_t extraBytes = 0;

  while (compressedLen > 0) {
    size_t portionLen;
    if (compressedLen > ZLIB_BUFFER_SIZE)
      portionLen = ZLIB_BUFFER_SIZE;
    else
      portionLen = (size_t) compressedLen;

    if (!ReadFromRFBServer((uint8_t*)zlib_buffer, portionLen))
      return False;

    assert((size_t)compressedLen > portionLen);
    compressedLen -= (int) portionLen;

    zs->next_in = (Bytef *)zlib_buffer;
    zs->avail_in = (uInt) portionLen;

    do {
      zs->next_out = (Bytef *)&buffer[extraBytes];
      zs->avail_out = (uInt) (bufferSize - extraBytes);

      err = inflate(zs, Z_SYNC_FLUSH);
      if (err == Z_BUF_ERROR)   /* Input exhausted -- no problem. */
        break;
      if (err != Z_OK && err != Z_STREAM_END) {
        if (zs->msg != NULL) {
          fprintf(stderr, "Inflate error: %s.\n", zs->msg);
        } else {
          fprintf(stderr, "Inflate error: %d.\n", err);
        }
        return False;
      }

      size_t numRows = (bufferSize - zs->avail_out) / rowSize;

      assert(rowsProcessed <= UINT32_MAX);
      assert(ry + rowsProcessed <= SIZE_MAX);
      assert(numRows <= UINT32_MAX);

      filterFn(numRows, (CARDBPP *)buffer2);

      extraBytes = bufferSize - zs->avail_out - numRows * rowSize;
      if (extraBytes > 0)
        memcpy(buffer, &buffer[numRows * rowSize], extraBytes);

      CopyDataToScreen(buffer2, rx, ry + (uint32_t)rowsProcessed, rw, (uint32_t)numRows);
      rowsProcessed += numRows;
    }
    while (zs->avail_out == 0);
  }

  if (rowsProcessed != rh) {
    fprintf(stderr, "Incorrect number of scan lines after decompression.\n");
    return False;
  }

  return True;
}

/*----------------------------------------------------------------------------
 *
 * Filter stuff.
 *
 */

/*
   The following variables are defined in rfbproto.c:
     static Bool cutZeros;
     static uint32_t rectWidth, rectColors;
     static uint8_t tightPalette[256*4];
     static uint8_t tightPrevRow[2048*3*sizeof(uint16_t)];
*/

static uint_fast8_t
InitFilterCopyBPP (uint32_t rw, uint32_t rh)
{
  (void) rh;
  rectWidth = rw;

#if BPP == 32
  if (myFormat.depth == 24 && myFormat.redMax == 0xFF &&
      myFormat.greenMax == 0xFF && myFormat.blueMax == 0xFF) {
    cutZeros = True;
    return 24;
  } else {
    cutZeros = False;
  }
#endif

  return BPP;
}

static void
FilterCopyBPP (size_t numRows, CARDBPP *dst)
{
#if BPP == 32
  size_t x, y;

  if (cutZeros) {
    for (y = 0; y < numRows; y++) {
      for (x = 0; x < rectWidth; x++) {
        dst[y*rectWidth+x] =
          RGB24_TO_PIXEL32(buffer[(y*rectWidth+x)*3],
                           buffer[(y*rectWidth+x)*3+1],
                           buffer[(y*rectWidth+x)*3+2]);
      }
    }
    return;
  }
#endif

  memcpy (dst, buffer, numRows * rectWidth * (BPP / 8));
}

static uint_fast8_t
InitFilterGradientBPP (uint32_t rw, uint32_t rh)
{
  uint_fast8_t bits;

  bits = InitFilterCopyBPP(rw, rh);
  if (cutZeros)
    memset(tightPrevRow, 0, rw * 3);
  else
    memset(tightPrevRow, 0, rw * 3 * sizeof(uint16_t));

  return bits;
}

#if BPP == 32

static void
FilterGradient24 (size_t numRows, uint32_t *dst)
{
  size_t x, y, c;
  uint8_t thisRow[2048*3];
  uint8_t pix[3];
  int est[3];

  for (y = 0; y < numRows; y++) {

    /* First pixel in a row */
    for (c = 0; c < 3; c++) {
      pix[c] = (uint8_t) (tightPrevRow[c] + buffer[y*rectWidth*3+c]);
      thisRow[c] = pix[c];
    }
    dst[y*rectWidth] = RGB24_TO_PIXEL32(pix[0], pix[1], pix[2]);

    /* Remaining pixels of a row */
    for (x = 1; x < rectWidth; x++) {
      for (c = 0; c < 3; c++) {
        est[c] = (int)tightPrevRow[x*3+c] + (int)pix[c] -
                 (int)tightPrevRow[(x-1)*3+c];
        if (est[c] > 0xFF) {
          est[c] = 0xFF;
        } else if (est[c] < 0x00) {
          est[c] = 0x00;
        }
        pix[c] = (uint8_t)(est[c] + buffer[(y*rectWidth+x)*3+c]);
        thisRow[x*3+c] = pix[c];
      }
      dst[y*rectWidth+x] = RGB24_TO_PIXEL32(pix[0], pix[1], pix[2]);
    }

    memcpy(tightPrevRow, thisRow, rectWidth * 3);
  }
}

#endif

static void
FilterGradientBPP (size_t numRows, CARDBPP *dst)
{
  size_t x, y, c;
  CARDBPP *src = (CARDBPP *)buffer;
  uint16_t *thatRow = (uint16_t *)tightPrevRow;
  uint16_t thisRow[2048*3];
  uint16_t pix[3];
  uint16_t max[3];
  int shift[3];
  int est[3];

#if BPP == 32
  if (cutZeros) {
    FilterGradient24(numRows, dst);

    return;
  }
#endif

  max[0] = myFormat.redMax;
  max[1] = myFormat.greenMax;
  max[2] = myFormat.blueMax;

  shift[0] = myFormat.redShift;
  shift[1] = myFormat.greenShift;
  shift[2] = myFormat.blueShift;

  for (y = 0; y < numRows; y++) {

    /* First pixel in a row */
    for (c = 0; c < 3; c++) {
      pix[c] = (uint16_t)(((src[y*rectWidth] >> shift[c]) + thatRow[c]) & max[c]);
      thisRow[c] = pix[c];
    }
    dst[y*rectWidth] = RGB_TO_PIXEL(BPP, pix[0], pix[1], pix[2]);

    /* Remaining pixels of a row */
    for (x = 1; x < rectWidth; x++) {
      for (c = 0; c < 3; c++) {
        est[c] = (int)thatRow[x*3+c] + (int)pix[c] - (int)thatRow[(x-1)*3+c];
        if (est[c] > (int)max[c]) {
          est[c] = (int)max[c];
        } else if (est[c] < 0) {
          est[c] = 0;
        }
        pix[c] = (uint16_t)(((src[y*rectWidth+x] >> shift[c]) + (uint16_t)est[c]) & max[c]);
        thisRow[x*3+c] = pix[c];
      }
      dst[y*rectWidth+x] = RGB_TO_PIXEL(BPP, pix[0], pix[1], pix[2]);
    }
    memcpy(thatRow, thisRow, rectWidth * 3 * sizeof(uint16_t));
  }
}

static uint_fast8_t
InitFilterPaletteBPP (uint32_t rw, uint32_t rh)
{
  (void) rh;

  rectWidth = rw;

  uint8_t numColors;
  if (!ReadFromRFBServer((uint8_t*)&numColors, 1))
    return 0;

  rectColors = (int)numColors;
  if (++rectColors < 2)
    return 0;

#if BPP == 32
  CARDBPP *palette = (CARDBPP *)tightPalette;
  if (myFormat.depth == 24 && myFormat.redMax == 0xFF &&
      myFormat.greenMax == 0xFF && myFormat.blueMax == 0xFF) {
    if (!ReadFromRFBServer((uint8_t*)&tightPalette, rectColors * 3))
      return 0;
    for (ssize_t i = rectColors - 1; i >= 0; i--) {
      palette[i] = RGB24_TO_PIXEL32(tightPalette[i*3],
                                    tightPalette[i*3+1],
                                    tightPalette[i*3+2]);
    }
    return (rectColors == 2) ? 1 : 8;
  }
#endif

  if (!ReadFromRFBServer((uint8_t*)&tightPalette, rectColors * (BPP / 8)))
    return 0;

  return (rectColors == 2) ? 1 : 8;
}

static void
FilterPaletteBPP (size_t numRows, CARDBPP *dst)
{
  size_t x, y, w;
  uint8_t *src = (uint8_t *)buffer;
  CARDBPP *palette = (CARDBPP *)tightPalette;

  if (rectColors == 2) {
    w = (rectWidth + 7) / 8;
    for (y = 0; y < numRows; y++) {
      for (x = 0; x < rectWidth / 8; x++) {
        for (int b = 7; b >= 0; b--)
          dst[y*rectWidth+x*8+7-(size_t)b] = palette[src[y*w+x] >> b & 1];
      }
      for (int b = 7; b >= (int)(8 - rectWidth % 8); b--) {
        dst[y*rectWidth+x*8+7-(size_t)b] = palette[src[y*w+x] >> b & 1];
      }
    }
  } else {
    for (y = 0; y < numRows; y++)
      for (x = 0; x < rectWidth; x++)
        dst[y*rectWidth+x] = palette[(int)src[y*rectWidth+x]];
  }
}

#if BPP != 8

/*----------------------------------------------------------------------------
 *
 * JPEG decompression.
 *
 */

/*
   The following variables are defined in rfbproto.c:
     static Bool jpegError;
     static struct jpeg_source_mgr jpegSrcManager;
     static JOCTET *jpegBufferPtr;
     static size_t *jpegBufferLen;
*/

static Bool
DecompressJpegRectBPP(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  int compressedLen;
  uint8_t *compressedData;
  CARDBPP *pixelPtr;
  JSAMPROW rowPointer[1];

  compressedLen = (int)ReadCompactLen();
  if (compressedLen <= 0) {
    fprintf(stderr, "Incorrect data received from the server.\n");
    return False;
  }

  compressedData = malloc((size_t)compressedLen);
  if (compressedData == NULL) {
    fprintf(stderr, "Memory allocation error.\n");
    return False;
  }

  if (!ReadFromRFBServer((uint8_t*)compressedData, (size_t)compressedLen)) {
    free(compressedData);
    return False;
  }

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);

  JpegSetSrcManager(&cinfo, compressedData, (size_t)compressedLen);

  jpeg_read_header(&cinfo, TRUE);
  cinfo.out_color_space = JCS_RGB;

  jpeg_start_decompress(&cinfo);
  if (cinfo.output_width != (unsigned int) w || cinfo.output_height != (unsigned int) h ||
      cinfo.output_components != 3) {
    fprintf(stderr, "Tight Encoding: Wrong JPEG data received.\n");
    jpeg_destroy_decompress(&cinfo);
    free(compressedData);
    return False;
  }

  rowPointer[0] = (JSAMPROW)buffer;
  uint32_t dy = 0;
  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, rowPointer, 1);
    if (jpegError) {
      break;
    }
    pixelPtr = (CARDBPP *)&buffer[BUFFER_SIZE / 2];
    for (size_t dx = 0; dx < w; dx++) {
      *pixelPtr++ =
        RGB24_TO_PIXEL(BPP, buffer[dx*3], buffer[dx*3+1], buffer[dx*3+2]);
    }
    CopyDataToScreen(&buffer[BUFFER_SIZE / 2], x, y + dy, w, 1);
    dy++;
  }

  if (!jpegError)
    jpeg_finish_decompress(&cinfo);

  jpeg_destroy_decompress(&cinfo);
  free(compressedData);

  return !jpegError;
}

#endif

