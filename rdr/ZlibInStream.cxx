//
// Copyright (C) 2002 RealVNC Ltd.  All Rights Reserved.
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
// USA.

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "ZlibInStream.h"
#include "Exception.h"
#include <zlib.h>

using namespace rdr;

enum { DEFAULT_BUF_SIZE = 16384 };

ZlibInStream::ZlibInStream(size_t bufSize_)
  : underlying(0), bufSize(bufSize_ ? bufSize_ : DEFAULT_BUF_SIZE), offset(0),
    bytesIn(0)
{
  zs = new z_stream;
  zs->zalloc    = Z_NULL;
  zs->zfree     = Z_NULL;
  zs->opaque    = Z_NULL;
  zs->next_in   = Z_NULL;
  zs->avail_in  = 0;
  if (inflateInit(zs) != Z_OK) {
    delete zs;
    throw Exception("ZlibInStream: inflateInit failed");
  }
  ptr = end = start = new uint8_t[bufSize];
}

ZlibInStream::~ZlibInStream()
{
  delete [] start;
  inflateEnd(zs);
  delete zs;
}

void ZlibInStream::setUnderlying(InStream* is, size_t bytesIn_)
{
  underlying = is;
  bytesIn = bytesIn_;
  ptr = end = start;
}

size_t ZlibInStream::pos()
{
  return offset + ptr - start;
}

void ZlibInStream::reset()
{
  ptr = end = start;
  if (!underlying) return;

  while (bytesIn > 0) {
    decompress();
    end = start; // throw away any data
  }
  underlying = 0;
}

size_t ZlibInStream::overrun(size_t itemSize, size_t nItems)
{
  if (itemSize > bufSize)
    throw Exception("ZlibInStream overrun: max itemSize exceeded");
  if (!underlying)
    throw Exception("ZlibInStream overrun: no underlying stream");

  if (end - ptr != 0)
    memmove(start, ptr, end - ptr);

  offset += ptr - start;
  end -= ptr - start;
  ptr = start;

  while ((size_t)(end - ptr) < itemSize) {
    decompress();
  }

  if (itemSize * nItems > (size_t)(end - ptr))
    nItems = (end - ptr) / itemSize;

  return nItems;
}

// decompress() calls the decompressor once.  Note that this won't necessarily
// generate any output data - it may just consume some input data.

void ZlibInStream::decompress()
{
  zs->next_out = (uint8_t*)end;
  ptrdiff_t avail_out = start + bufSize - end;
  zs->avail_out = (uInt) avail_out;
  assert(zs->avail_out == avail_out);

  underlying->check(1);
  zs->next_in = (uint8_t*)underlying->getptr();
  ptrdiff_t avail_in = underlying->getend() - underlying->getptr();
  zs->avail_in = (uInt) avail_in;
  assert(zs->avail_in == avail_in);
  if ((size_t)zs->avail_in > bytesIn) {
    zs->avail_in = (uInt) bytesIn;
    assert(zs->avail_in == bytesIn);
  }

  int rc = inflate(zs, Z_SYNC_FLUSH);
  if (rc != Z_OK) {
    throw Exception("ZlibInStream: inflate failed");
  }

  bytesIn -= zs->next_in - underlying->getptr();
  end = zs->next_out;
  underlying->setptr(zs->next_in);
}
