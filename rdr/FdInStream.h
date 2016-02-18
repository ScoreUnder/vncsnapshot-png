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

//
// FdInStream streams from a file descriptor.
//

#ifndef __RDR_FDINSTREAM_H__
#define __RDR_FDINSTREAM_H__

#include <stdbool.h>
#include <stdint.h>
#include "InStream.h"

namespace rdr {

  class FdInStream : public InStream {

  public:

    FdInStream(int fd, int timeout=0, size_t bufSize=0);
    FdInStream(int fd, void (*blockCallback)(void*), void* blockCallbackArg=0,
		  size_t bufSize=0);
    virtual ~FdInStream();

    size_t getFd() { return fd; }
    size_t pos();
    void readBytes(void* data, size_t length);
    size_t bytesInBuf() { return end - ptr; }

    void startTiming();
    void stopTiming();
    unsigned int kbitsPerSecond();
    unsigned int timeWaited() { return timeWaitedIn100us; }

  protected:
    size_t overrun(size_t itemSize, size_t nItems);

  private:
    int checkReadable(int fd, int timeout);
    size_t readWithTimeoutOrCallback(void* buf, size_t len);

    int fd;
    int timeout;
    void (*blockCallback)(void*);
    void* blockCallbackArg;

    bool timing;
    unsigned int timeWaitedIn100us;
    unsigned int timedKbits;

    size_t bufSize;
    size_t offset;
    uint8_t* start;
  };

} // end of namespace rdr

#endif
