/*
 *  Copyright (C) 2016 Score_Under. All Rights Reserved.
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include "getpass.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>

static char buffer[512];

char *getpass(const char * prompt)
{
    struct termios old_termios;
    bool have_termios = tcgetattr(STDIN_FILENO, &old_termios) == 0;

    if (have_termios) {
        struct termios new_termios = old_termios;
        new_termios.c_lflag &= (tcflag_t) ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    }

    fputs(prompt, stdout);
    char *fgets_result = fgets(buffer, sizeof buffer, stdin);
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    if (!fgets_result)
        return NULL;

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
        buffer[len - 1] = 0;

    return buffer;
}

