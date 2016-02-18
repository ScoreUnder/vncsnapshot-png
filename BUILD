BUILDING vncsnapshot (1.0 release) for Unix and Linux
======================================================

This distibution requires two third-party libraries for successful
compilation. They are NOT included in this archive. These are are zlib
and JPEG libraries freely available in the source form from following
locations:

  JPEG library: http://www.ijg.org/
  zlib library: http://zlib.net

If your system already has these packages installed, simply edit the
Makefile to use the correct include and library paths. The defaults in the
Makefile are suitable for most systems with the libraries in the standard
locations.

If you do not have them, please download the packages and compile. They can
be installed anywhere (root permission is not required), as long as you edit
the Makefile appropriately.

The Makefile does not include an 'install' target; to install, simply copy
'vncsnapshot' to the desired location (such as /usr/local/bin).

Other Makefile configuration items:
  EXTRALIBS needs to be set to any extra libraries you need. On Solaris,
  this will be '-lsocket -lnsl'.

  EXTRAINCLUDES needs to be set to any extra include options you need.
  Most systems should not require anything extra here.

  CC is set 'gcc'. On non-Linux systems, such as Solaris, you may want
  to set this to something else such as 'cc' or 'acc'.

  Likewise, CXX is set 'g++'. Again, you may want to set this to something
  else such as 'CC'.

  CDEBUGFLAGS is set to '-O2'. This is appropriate for gcc; other
  compilers may want '-O'; or, you may wish to set this to '-g' for
  debugging.

You can also look at make_release_bin; this script was used by the previous
maintainer to build vncsnapshot on various flavours of Unix and Linux.
