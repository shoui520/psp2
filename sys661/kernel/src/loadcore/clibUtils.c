/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include "clibUtils.h"

static void *wmemcpy(u32 *dest, const u32 *src, u32 n);

s32 strcmp(const char *lhs, const char *rhs)
{
    if (lhs != NULL && rhs != NULL) {
        while (*lhs++ == *rhs++) {
            if (*(lhs - 1) == '\0')
                return 0;
        }
        return *(lhs - 1) - *(rhs - 1);
    }
    if (lhs == rhs)
        return 0;

    if (lhs != NULL)
        return 1;

    return -1;
}

char *strncpy(char *dest, const char *src, s32 n)
{
    char *tmpDest;
    int i;

    if (dest == NULL || src == NULL)
        return 0;

    tmpDest = dest;
    for (i = 0; i < n; i++) {
        if ((*tmpDest++ = *src++) == '\0') {
            while (++i < n)
                *tmpDest++ = '\0';
            break;
        }
    }
    return dest;
}

u32 strlen(const char *str)
{
    u32 len;

    len = 0;
    if (str == NULL)
        return len;

    while (*str++ != '\0')
        len++;

    return len;
}

void *memcpy(void *dest, const void *src, u32 n)
{
    u8 *tmpDest;
    const u8 *tmpSrc;

    if (dest == NULL)
        return NULL;

    /* Check if word-aligned (minimum alignment). */
    if ((((u32)dest | (u32)src | n) & 0x3) == 0)
        return wmemcpy(dest, src, n);

    /* Byte alignment. */
    tmpDest = dest;
    tmpSrc = src;
    while (n-- != 0)
        *tmpDest++ = *tmpSrc++;

    return dest;
}

/*
 * Copy n bytes from src into dest (n must be a multiple of 4).
 *
 * Returns the value of dest.
 */
static void *wmemcpy(u32 *dest, const u32 *src, u32 n)
{
    u32 *tmpDest;
    u32 *end;

    n &= ~0x3;
    if (n == 0)
        return dest;

    tmpDest = dest;
    end = dest + (n >> 2);
    /* Copy initial 4-byte words until remaining count is a multiple of 16 bytes. */
    if (n & 0xC) {
        u32 *remEnd = dest + ((n & 0xC) >> 2);
        while (tmpDest != remEnd)
            *tmpDest++ = *src++;

        if (tmpDest == end)
            return dest;
    }
    /* Copy 16-byte chunks. */
    while (tmpDest != end) {
        tmpDest[0] = src[0];
        tmpDest[1] = src[1];
        tmpDest[2] = src[2];
        tmpDest[3] = src[3];
        tmpDest += 4;
        src += 4;
    }
    return dest;
}
