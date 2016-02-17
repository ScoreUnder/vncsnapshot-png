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
// FdOutStream streams to a file descriptor.
//

#ifndef __RDR_FDOUTSTREAM_H__
#define __RDR_FDOUTSTREAM_H__

#include <stdint.h>
#include "OutStream.h"

namespace rdr {

  class FdOutStream : public OutStream {

  public:

    FdOutStream(int fd, size_t bufSize=0);
    virtual ~FdOutStream();

    int getFd() { return fd; }

    void flush();
    size_t length();
    void writeBytes(const void* data, size_t length);

  private:
    size_t overrun(size_t itemSize, size_t nItems);
    int fd;
    size_t bufSize;
    size_t offset;
    uint8_t* start;
  };

}

#endif
