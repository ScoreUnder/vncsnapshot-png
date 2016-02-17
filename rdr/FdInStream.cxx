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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#ifdef _WIN32
#include <winsock.h>
#include <sys/timeb.h>
#define read(s,b,l) recv(s,(char*)b,l,0)
#undef errno
#define errno WSAGetLastError()
#else
#include <unistd.h>
#include <sys/time.h>
#endif

// XXX should use autoconf HAVE_SYS_SELECT_H
#ifdef _AIX
#include <sys/select.h>
#endif

#include "FdInStream.h"
#include "Exception.h"

using namespace rdr;

enum { DEFAULT_BUF_SIZE = 8192,
       MIN_BULK_SIZE = 1024 };

FdInStream::FdInStream(int fd_, int timeout_, size_t bufSize_)
  : fd(fd_), timeout(timeout_), blockCallback(0), blockCallbackArg(0),
    timeWaitedIn100us(5), timedKbits(0),
    bufSize(bufSize_ ? bufSize_ : DEFAULT_BUF_SIZE), offset(0)
{
  ptr = end = start = new uint8_t[bufSize];
}

FdInStream::FdInStream(int fd_, void (*blockCallback_)(void*),
                       void* blockCallbackArg_, size_t bufSize_)
  : fd(fd_), timeout(0), blockCallback(blockCallback_),
    blockCallbackArg(blockCallbackArg_),
    timeWaitedIn100us(5), timedKbits(0),
    bufSize(bufSize_ ? bufSize_ : DEFAULT_BUF_SIZE), offset(0)
{
  ptr = end = start = new uint8_t[bufSize];
}

FdInStream::~FdInStream()
{
  delete [] start;
}


size_t FdInStream::pos()
{
  return offset + ptr - start;
}

void FdInStream::readBytes(void* data, size_t length)
{
  if (length < MIN_BULK_SIZE) {
    InStream::readBytes(data, length);
    return;
  }

  uint8_t* dataPtr = (uint8_t*)data;

  size_t n = end - ptr;
  if (n > length) n = length;

  memcpy(dataPtr, ptr, n);
  dataPtr += n;
  length -= n;
  ptr += n;

  while (length > 0) {
    n = readWithTimeoutOrCallback(dataPtr, length);
    dataPtr += n;
    length -= n;
    offset += n;
  }
}


size_t FdInStream::overrun(size_t itemSize, size_t nItems)
{
  if (itemSize > bufSize)
    throw Exception("FdInStream overrun: max itemSize exceeded");

  if (end - ptr != 0)
    memmove(start, ptr, end - ptr);

  offset += ptr - start;
  end -= ptr - start;
  ptr = start;

  while (end < start + itemSize) {
    size_t n = readWithTimeoutOrCallback((uint8_t*)end, start + bufSize - end);
    end += n;
  }

  if (itemSize * nItems > (size_t)(end - ptr))
    nItems = (end - ptr) / itemSize;

  return nItems;
}

int FdInStream::checkReadable(int fd, int timeout)
{
  while (true) {
    fd_set rfds;
    struct timeval tv;
    
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    int n = select(fd+1, &rfds, 0, 0, &tv);
    if (n != -1 || errno != EINTR)
      return n;
    fprintf(stderr,"select returned EINTR\n");
  }
}

size_t FdInStream::readWithTimeoutOrCallback(void* buf, size_t len)
{
  int fds_available = checkReadable(fd, timeout);

  if (fds_available < 0) throw SystemException("select",errno);

  if (fds_available == 0) {
    if (timeout) throw TimedOut();
    if (blockCallback) (*blockCallback)(blockCallbackArg);
  }

  ssize_t n_read;
  while (true) {
    n_read = ::read(fd, buf, len);
    if (n_read != -1 || errno != EINTR)
      break;
    fprintf(stderr,"read returned EINTR\n");
  }

  if (n_read < 0) throw SystemException("read",errno);
  if (n_read == 0) throw EndOfStream();
  return n_read;
}
