/* Copyright (C) 2011, 2012, 2013 The uOFW team
   See the file COPYING for copying permission.
*/

#include <common_imp.h>

#include "libcUtils.h"

void strcpy256(char *dest, char *src)
{
    s32 remaining = 255;
    do {
        char ch = *src++;
        remaining--;
        if (remaining == 0)
            ch = '\0';
        *dest++ = ch;
        if (ch == '\0')
            break;
    } while (remaining != 0);
}

void wcscpy256(short *dest, short *src)
{
    s32 remaining = 255;
    do {
        short ch = *src++;
        remaining--;
        if (remaining == 0)
            ch = 0;
        *dest++ = ch;
        if (ch == 0)
            break;
    } while (remaining != 0);
}
