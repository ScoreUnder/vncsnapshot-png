
vncsnapshot-png 1.3: VNC snapshot utility based on VNC
================================================================

TightVNC is Copyright (C) 2001 Const Kaplinsky.  All Rights Reserved.
VNC is Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
This software is distributed under the GNU General Public Licence as published
by the Free Software Foundation.

Adapted from the TightVNC viewer by Grant McDorman, February 2002.
Further adapted from vncsnapshot and the original vncsnapshot-png, both of which appear to be abandoned.

Features
--------

* Always connects to server in 'shared' mode.
* Saves image to a PNG file, unlike the usual vncsnapshot which saves a lossy JPEG.
* The remote cursor is NOT included - unfortunately, the server doesn't provide a way of including the cursor in the snapshot. (TODO: verify this - sometimes it is included against our will)
* Standard VNC/TightVNC options (encoding, etc.) are available.
