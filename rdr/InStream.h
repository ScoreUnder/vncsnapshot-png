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
// rdr::InStream marshalls data from a buffer stored in RDR (RFB Data
// Representation).
//

#ifndef __RDR_INSTREAM_H__
#define __RDR_INSTREAM_H__

#include <stdint.h>
#include <string.h> // for memcpy

namespace rdr {

  class InStream {

  public:

    virtual ~InStream() {}

    // check() ensures there is buffer data for at least one item of size
    // itemSize bytes.  Returns the number of items in the buffer (up to a
    // maximum of nItems).

    inline size_t check(size_t itemSize, size_t nItems=1)
    {
      if (ptr + itemSize * nItems > end) {
        if (ptr + itemSize > end)
          return overrun(itemSize, nItems);

        nItems = (end - ptr) / itemSize;
      }
      return nItems;
    }

    // readU/SN() methods read unsigned and signed N-bit integers.

    inline uint8_t  readU8()  { check(1); return *ptr++; }
    inline uint16_t readU16() { check(2); int b0 = *ptr++; int b1 = *ptr++;
                           return (uint16_t) (b0 << 8 | b1); }
    inline uint32_t readU32() { check(4); int b0 = *ptr++; int b1 = *ptr++;
                                     int b2 = *ptr++; int b3 = *ptr++;
                           return (uint32_t) (b0 << 24 | b1 << 16 | b2 << 8 | b3); }

    inline int8_t  readS8()  { return (int8_t) readU8();  }
    inline int16_t readS16() { return (int16_t)readU16(); }
    inline int32_t readS32() { return (int32_t)readU32(); }

    // readString() reads a string - a uint32_t length followed by the data.
    // Returns a null-terminated string - the caller should delete[] it
    // afterwards.

    char* readString();

    // maxStringLength protects against allocating a huge buffer.  Set it
    // higher if you need longer strings.

    static uint32_t maxStringLength;

    inline void skip(size_t bytes) {
      while (bytes > 0) {
        size_t n = check(1, bytes);
        ptr += n;
        bytes -= n;
      }
    }

    // readBytes() reads an exact number of bytes.

    virtual void readBytes(void* data, size_t length) {
      uint8_t* dataPtr = (uint8_t*)data;
      uint8_t* dataEnd = dataPtr + length;
      while (dataPtr < dataEnd) {
        size_t n = check(1, dataEnd - dataPtr);
        memcpy(dataPtr, ptr, n);
        ptr += n;
        dataPtr += n;
      }
    }

    // readOpaqueN() reads a quantity without byte-swapping.

    inline uint8_t  readOpaque8()  { return readU8(); }
    inline uint16_t readOpaque16() { check(2); uint16_t r; ((uint8_t*)&r)[0] = *ptr++;
                                ((uint8_t*)&r)[1] = *ptr++; return r; }
    inline uint32_t readOpaque32() { check(4); uint32_t r; ((uint8_t*)&r)[0] = *ptr++;
                                ((uint8_t*)&r)[1] = *ptr++; ((uint8_t*)&r)[2] = *ptr++;
                                ((uint8_t*)&r)[3] = *ptr++; return r; }
    inline uint32_t readOpaque24A() { check(3); uint32_t r=0; ((uint8_t*)&r)[0] = *ptr++;
                                 ((uint8_t*)&r)[1] = *ptr++; ((uint8_t*)&r)[2] = *ptr++;
                                 return r; }
    inline uint32_t readOpaque24B() { check(3); uint32_t r=0; ((uint8_t*)&r)[1] = *ptr++;
                                 ((uint8_t*)&r)[2] = *ptr++; ((uint8_t*)&r)[3] = *ptr++;
                                 return r; }

    // pos() returns the position in the stream.

    virtual size_t pos() = 0;

    // getptr(), getend() and setptr() are "dirty" methods which allow you to
    // manipulate the buffer directly.  This is useful for a stream which is a
    // wrapper around an underlying stream.

    inline const uint8_t* getptr() const { return ptr; }
    inline const uint8_t* getend() const { return end; }
    inline void setptr(const uint8_t* p) { ptr = p; }

  private:

    // overrun() is implemented by a derived class to cope with buffer overrun.
    // It ensures there are at least itemSize bytes of buffer data.  Returns
    // the number of items in the buffer (up to a maximum of nItems).  itemSize
    // is supposed to be "small" (a few bytes).

    virtual size_t overrun(size_t itemSize, size_t nItems) = 0;

  protected:

    InStream() {}
    const uint8_t* ptr;
    const uint8_t* end;
  };

}

#endif
