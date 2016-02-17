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
// rdr::OutStream marshalls data into a buffer stored in RDR (RFB Data
// Representation).
//

#ifndef __RDR_OUTSTREAM_H__
#define __RDR_OUTSTREAM_H__

#include <assert.h>
#include <string.h>
#include <stdint.h>

namespace rdr {

  class OutStream {

  protected:

    OutStream() {}

  public:

    virtual ~OutStream() {}

    // check() ensures there is buffer space for at least one item of size
    // itemSize bytes.  Returns the number of items which fit (up to a maximum
    // of nItems).

    inline size_t check(size_t itemSize, size_t nItems=1)
    {
      if (ptr + itemSize * nItems > end) {
        if (ptr + itemSize > end)
          return overrun(itemSize, nItems);

        nItems = (end - ptr) / itemSize;
      }
      return nItems;
    }

    // writeU/SN() methods write unsigned and signed N-bit integers.

    inline void writeU8( uint8_t  u) { check(1); *ptr++ = u; }
    inline void writeU16(uint16_t u) { check(2);
                                       *ptr++ = (uint8_t)(u >> 8);
                                       *ptr++ = (uint8_t)u; }
    inline void writeU32(uint32_t u) { check(4);
                                       *ptr++ = (uint8_t)(u >> 24);
                                       *ptr++ = (uint8_t)(u >> 16);
                                       *ptr++ = (uint8_t)(u >> 8);
                                       *ptr++ = (uint8_t)u; }

    inline void writeS8( int8_t  s) { writeU8( (uint8_t) s); }
    inline void writeS16(int16_t s) { writeU16((uint16_t)s); }
    inline void writeS32(int32_t s) { writeU32((uint32_t)s); }

    // writeString() writes a string - a uint32_t length followed by the data.  The
    // given string should be null-terminated (but the terminating null is not
    // written to the stream).

    inline void writeString(const char* str) {
      size_t len = strlen(str);
      assert(len < UINT32_MAX);
      writeU32((uint32_t)len);
      writeBytes(str, len);
    }

    inline void pad(size_t bytes) {
      while (bytes-- > 0) writeU8(0);
    }

    inline void skip(size_t bytes) {
      while (bytes > 0) {
        size_t n = check(1, bytes);
        ptr += n;
        bytes -= n;
      }
    }

    // writeBytes() writes an exact number of bytes.

    virtual void writeBytes(const void* data, size_t length) {
      const uint8_t* dataPtr = (const uint8_t*)data;
      const uint8_t* dataEnd = dataPtr + length;
      while (dataPtr < dataEnd) {
        size_t n = check(1, dataEnd - dataPtr);
        memcpy(ptr, dataPtr, n);
        ptr += n;
        dataPtr += n;
      }
    }

    // writeOpaqueN() writes a quantity without byte-swapping.

    inline void writeOpaque8( uint8_t  u) { writeU8(u); }
    inline void writeOpaque16(uint16_t u) { check(2);
                                            *ptr++ = ((uint8_t*)&u)[0];
                                            *ptr++ = ((uint8_t*)&u)[1]; }
    inline void writeOpaque32(uint32_t u) { check(4);
                                            *ptr++ = ((uint8_t*)&u)[0];
                                            *ptr++ = ((uint8_t*)&u)[1];
                                            *ptr++ = ((uint8_t*)&u)[2];
                                            *ptr++ = ((uint8_t*)&u)[3]; }
    inline void writeOpaque24A(uint32_t u) { check(3);
                                             *ptr++ = ((uint8_t*)&u)[0];
                                             *ptr++ = ((uint8_t*)&u)[1];
                                             *ptr++ = ((uint8_t*)&u)[2]; }
    inline void writeOpaque24B(uint32_t u) { check(3);
                                             *ptr++ = ((uint8_t*)&u)[1];
                                             *ptr++ = ((uint8_t*)&u)[2];
                                             *ptr++ = ((uint8_t*)&u)[3]; }

    // length() returns the length of the stream.

    virtual size_t length() = 0;

    // flush() requests that the stream be flushed.

    virtual void flush() {}

    // getptr(), getend() and setptr() are "dirty" methods which allow you to
    // manipulate the buffer directly.  This is useful for a stream which is a
    // wrapper around an underlying stream.

    inline uint8_t* getptr() { return ptr; }
    inline uint8_t* getend() { return end; }
    inline void setptr(uint8_t* p) { ptr = p; }

  private:

    // overrun() is implemented by a derived class to cope with buffer overrun.
    // It ensures there are at least itemSize bytes of buffer space.  Returns
    // the number of items which fit (up to a maximum of nItems).  itemSize is
    // supposed to be "small" (a few bytes).

    virtual size_t overrun(size_t itemSize, size_t nItems) = 0;

  protected:

    uint8_t* ptr;
    uint8_t* end;
  };

}

#endif
