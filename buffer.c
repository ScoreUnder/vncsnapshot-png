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
 * buffer.c - functions to deal with the raw image buffer.
 */

#include "vncsnapshot.h"

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <jpeglib.h>
#include <stdio.h>

#include <png.h>      /* PNG lib */
#include <zlib.h>

static void BufferPixelToRGB(uint32_t pixel, uint16_t *r, uint16_t *g, uint16_t *b);

static uint8_t * rawBuffer = NULL;
static bool bufferBlank = true;
static bool bufferWritten = false;

#define RAW_BYTES_PER_PIXEL 3   /* size of pixel in raw buffer */
#define MY_BYTES_PER_PIXEL 4    /* size of pixel in VNC buffer */
#define MY_BITS_PER_PIXEL (MY_BYTES_PER_PIXEL*8)

int
AllocateBuffer()
{
    uint32_t bytes;
    static const short testEndian = 1;

    /* Determine 'endian' nature of this machine */
    /* On big-endian machines, the address of a short (16 bit) is the
     * most significant byte (and is therefore 0). On little-endian,
     * it is the address of the least significant byte - and is therefore
     * 1.
     *
     * Intel 8x86 (including Pentium) are big-endian. Motorola, PDP-11,
     * and Sparc are little-endian.
     */
    bool bigEndian = 0 == *(char *)&testEndian;

    /* Format is RGBA. Due to the way we store the pixels,
     * the 'bigEndian' is the *opposite* of the hardware value.
     */
    myFormat.bitsPerPixel = MY_BITS_PER_PIXEL;
    myFormat.depth = 24;
    myFormat.trueColour = 1;
    myFormat.bigEndian = bigEndian;
    if (bigEndian) {
        myFormat.redShift = 24;
        myFormat.greenShift = 16;
        myFormat.blueShift = 8;
    } else {
        myFormat.redShift = 0;
        myFormat.greenShift = 8;
        myFormat.blueShift = 16;
    }
    myFormat.redMax = 0xFF;
    myFormat.greenMax = 0xFF;
    myFormat.blueMax = 0xFF;

    assert(SIZE_MAX / (myFormat.depth / 8) / si.framebufferWidth >= si.framebufferHeight);
    bytes = (uint32_t) (si.framebufferWidth * si.framebufferHeight * myFormat.depth / 8);
    rawBuffer = malloc(bytes);   /* allocate initialized to 0 */
    if (rawBuffer == NULL) {
        fprintf(stderr, "Failed to allocate memory frame buffer, %" PRId32 " bytes\n",
                bytes);
        return 0;
    }

    memset(rawBuffer, 0xBA, bytes);

    return 1;
}

void
CopyDataToScreen(uint8_t *buffer, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    size_t start;
    size_t stride;
    size_t row, col;
    assert(si.framebufferWidth > w);
    stride = (size_t)(si.framebufferWidth * RAW_BYTES_PER_PIXEL - (int32_t)w * RAW_BYTES_PER_PIXEL);
    start = (x + y * si.framebufferWidth) * RAW_BYTES_PER_PIXEL;

    bufferWritten = 1;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            bufferBlank &= buffer[0] == 0 &&
                            buffer[1] == 0 &&
                            buffer[2] == 0;
            rawBuffer[start++] = *buffer++;
            rawBuffer[start++] = *buffer++;
            rawBuffer[start++] = *buffer++;
            buffer++;   /* ignore 4th byte */
        }
        start += stride;
    }
}

uint8_t *
CopyScreenToData(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    size_t start;
    size_t stride;
    size_t row, col;
    uint8_t *buffer;
    uint8_t *cp;

    assert(si.framebufferWidth > w);
    stride = (size_t)(si.framebufferWidth * RAW_BYTES_PER_PIXEL - (int32_t)w * RAW_BYTES_PER_PIXEL);
    start = (x + y * si.framebufferWidth) * RAW_BYTES_PER_PIXEL;

    assert(SIZE_MAX / w / MY_BYTES_PER_PIXEL >= (size_t) h);  /* Overflow check */

    /* Allocate a buffer at the VNC size, not the raw size */
    buffer = malloc((size_t)(h * w * MY_BYTES_PER_PIXEL));
    cp = buffer;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            *cp++ = rawBuffer[start++];
            *cp++ = rawBuffer[start++];
            *cp++ = rawBuffer[start++];
            *cp++ = 0;
        }
        start += stride;
    }

    return buffer;
}

void
FillBufferRectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pixel)
{
    uint16_t r, g, b;
    size_t start;
    size_t stride;
    size_t row, col;

    BufferPixelToRGB(pixel, &r, &g, &b);

    bufferBlank &= r == 0 && g == 0 && b == 0;
    bufferWritten = 1;

    stride = (size_t)(si.framebufferWidth * RAW_BYTES_PER_PIXEL - (int32_t)w * RAW_BYTES_PER_PIXEL);
    start = (x + y * si.framebufferWidth) * RAW_BYTES_PER_PIXEL;
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            rawBuffer[start++] = (uint8_t) r;
            rawBuffer[start++] = (uint8_t) g;
            rawBuffer[start++] = (uint8_t) b;
        }
        start += stride;
    }
}

int
BufferIsBlank()
{
    return bufferBlank;
}

int
BufferWritten()
{
    return bufferWritten;
}

extern void write_PNG(char *filename, int interlace, uint32_t width, uint32_t height)
{
    int bit_depth=0, color_type;
    png_bytep row_pointers[height];
    png_structp png_ptr;
    png_infop info_ptr;

    for (uint32_t i=0; i<height; i++)
    {
        //row_pointers[i] = rawBuffer + i * 4 * width; //XXX
        row_pointers[i] = & rawBuffer[i * 3 * width];
    }

    FILE *outfile = fopen(filename, "wb");
    if (!outfile) err(1, "couldn't fopen %s", filename);

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
        (png_voidp) NULL, (png_error_ptr) NULL, (png_error_ptr) NULL);

    if (!png_ptr) errx(1, "couldn't create PNG write struct");

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
                png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
                fprintf(stderr, "Error: Couldn't create PNG info struct.");
                exit(1);
    }

    png_init_io(png_ptr, outfile);

    png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

    bit_depth = 8;
    color_type = PNG_COLOR_TYPE_RGB;
    //png_set_invert_alpha(png_ptr);
    //png_set_bgr(png_ptr);

    png_set_IHDR(png_ptr, info_ptr, width, height,
                 bit_depth, color_type, interlace,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    printf ("Now writing PNG file\n");

    png_write_image(png_ptr, row_pointers);

    png_write_end(png_ptr, info_ptr);
    /* puh, done, now freeing memory... */
    png_destroy_write_struct(&png_ptr, &info_ptr);

    if (outfile != NULL)
        (void) fclose(outfile);
    /*@i2@*/ } /* tell splint to ignore false warning for not
                  released memory of png_ptr and info_ptr */

static void
BufferPixelToRGB(uint32_t pixel, uint16_t *r, uint16_t *g, uint16_t *b)
{
    *r = (uint16_t) ((pixel >> myFormat.redShift) & myFormat.redMax);
    *b = (uint16_t) ((pixel >> myFormat.blueShift) & myFormat.blueMax);
    *g = (uint16_t) ((pixel >> myFormat.greenShift) & myFormat.greenMax);
}

void
ShrinkBuffer(uint32_t x, uint32_t y, uint32_t req_width, uint32_t req_height)
{
    size_t start;
    size_t stride;
    size_t row, col;
    uint8_t *cp;


    /*
     * Don't bother if x and y are zero and the width is the same.
     */
    if (x == 0 && y == 0 && req_width == si.framebufferWidth) {
        return;
    }

    /*
     * Rather than creating a copy, we just move in-place. Since we are
     * doing this from the start of the image, there is no problem
     * with overlapping moves.
     */

    stride = (size_t)(si.framebufferWidth * RAW_BYTES_PER_PIXEL - (int32_t)req_width * RAW_BYTES_PER_PIXEL);
    start = (x + y * si.framebufferWidth) * RAW_BYTES_PER_PIXEL;


    cp = rawBuffer;

    for (row = 0; row < req_height; row++) {
        for (col = 0; col < req_width; col++) {
            *cp++ = rawBuffer[start++];
            *cp++ = rawBuffer[start++];
            *cp++ = rawBuffer[start++];
        }
        start += stride;
    }

}
