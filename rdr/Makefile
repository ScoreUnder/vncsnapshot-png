# Makefile for vncsnapshot, Unix/Linux platforms.

SRCS = \
	FdInStream.cxx   InStream.cxx       ZlibInStream.cxx \
	FdOutStream.cxx  NullOutStream.cxx  ZlibOutStream.cxx

OBJS1 = $(SRCS:.c=.o)
OBJS  = $(OBJS1:.cxx=.o)

TARGET=librdr.a

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) cr $(TARGET) $(OBJS)
	ranlib $(TARGET)

%.o: %.cxx
	$(CXX) -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)


clean:
	-rm -f -- $(OBJS) $(TARGET) core

reallyclean: clean
	-rm -f -- *~

.PHONY: clean reallyclean all
