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

#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
extern "C" {
#include <sys/types.h>
#ifdef _WIN32
#include <winsock.h>
#define write(s,b,l) send(s,(const char*)b,l,0)
#undef errno
#define errno WSAGetLastError()
#else
#include <unistd.h>
#include <sys/time.h>
#endif
}

#include "FdOutStream.h"
#include "Exception.h"


using namespace rdr;

enum { DEFAULT_BUF_SIZE = 16384,
       MIN_BULK_SIZE = 1024 };

FdOutStream::FdOutStream(int fd_, size_t bufSize_)
  : fd(fd_), bufSize(bufSize_ ? bufSize_ : DEFAULT_BUF_SIZE), offset(0)
{
  ptr = start = new uint8_t[bufSize];
  end = start + bufSize;
}

FdOutStream::~FdOutStream()
{
  try {
    flush();
  } catch (Exception&) {
  }
  delete [] start;
}


void FdOutStream::writeBytes(const void* data, size_t length)
{
  if (length < MIN_BULK_SIZE) {
    OutStream::writeBytes(data, length);
    return;
  }

  const uint8_t* dataPtr = (const uint8_t*)data;

  flush();

  while (length > 0) {
    ssize_t n = write(fd, dataPtr, length);

    if (n < 0) throw SystemException("write",errno);

    length -= n;
    dataPtr += n;
    offset += n;
  }
}

size_t FdOutStream::length()
{
  return offset + ptr - start;
}

void FdOutStream::flush()
{
  uint8_t* sentUpTo = start;
  while (sentUpTo < ptr) {
    ssize_t n = write(fd, (const void*) sentUpTo, ptr - sentUpTo);

    if (n < 0) throw SystemException("write",errno);

    sentUpTo += n;
    offset += n;
  }

  ptr = start;
}


size_t FdOutStream::overrun(size_t itemSize, size_t nItems)
{
  if (itemSize > bufSize)
    throw Exception("FdOutStream overrun: max itemSize exceeded");

  flush();

  if (itemSize * nItems > (size_t)(end - ptr))
    nItems = (end - ptr) / itemSize;

  return nItems;
}
