# Makefile for vncsnapshot-png, Unix/Linux platforms.

INCLUDES =
LIBS = -lz -ljpeg -lpng

# Compilation Flags. Season to taste.
export CC = gcc
export CFLAGS = -O2 -Wall -Wextra -pedantic -Wconversion -std=c99
export CXXFLAGS = -O2 -Wall -Wextra -pedantic -Wconversion -std=c++98
export CPPFLAGS = $(INCLUDES)
export CXX=g++

# Solaris 8 uses CCC and CCFLAGS
export CCC=$(CXX)
export CCFLAGS = $(CFLAGS)

SRCS = \
  argsresources.c \
  buffer.c \
  cursor.c \
  listen.c \
  rfbproto.c \
  sockets.cxx \
  tunnel.c \
  vncsnapshot.c \
  d3des.c vncauth.c \
  getpass.c \
  zrle.cxx

OBJS1 = $(SRCS:.cxx=.o)
OBJS  = $(OBJS1:.c=.o)

PASSWD_OBJS1 = $(PASSWD_SRCS:.c=.o)
PASSWD_OBJS  = $(PASSWD_OBJS1:.cxx=.o)

SUBDIRS=rdr.dir

all: $(SUBDIRS:.dir=.all) vncsnapshot

vncsnapshot: $(OBJS)
	$(LINK.cc) $(CFLAGS) -o $@ $(OBJS) rdr/librdr.a $(LIBS)

clean: $(SUBDIRS:.dir=.clean) $(FINAL_SUBDIRS:.dir=.clean)
	-rm -f $(OBJS) $(PASSWD_OBJS) vncsnapshot

reallyclean: clean $(SUBDIRS:.dir=.reallyclean) $(FINAL_SUBDIRS:.dir=.reallyclean)
	-rm -f *~

rdr.all:
	cd rdr;$(MAKE) all
rdr.clean:
	cd rdr;$(MAKE) clean
rdr.reallyclean:
	cd rdr;$(MAKE) reallyclean

%.o: %.cxx
	$(CXX) -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

.PHONY: rdr.all rdr.clean rdr.reallyclean all clean reallyclean

# dependencies:

argsresources.o: argsresources.c vncsnapshot.h rfb.h rfbproto.h
buffer.o: buffer.c vncsnapshot.h rfb.h rfbproto.h
cursor.o: cursor.c vncsnapshot.h rfb.h rfbproto.h
listen.o: listen.c vncsnapshot.h rfb.h rfbproto.h
rfbproto.o: rfbproto.c vncsnapshot.h rfb.h rfbproto.h vncauth.h \
  protocols/rre.c protocols/corre.c \
  protocols/hextile.c protocols/zlib.c protocols/tight.c
sockets.o: sockets.cxx vncsnapshot.h rfb.h rfbproto.h
tunnel.o: tunnel.c vncsnapshot.h rfb.h rfbproto.h
vncsnapshot.o: vncsnapshot.c vncsnapshot.h rfb.h rfbproto.h
vncauth.o: vncauth.c stdhdrs.h rfb.h rfbproto.h vncauth.h d3des.h
zrle.o: zrle.cxx vncsnapshot.h
