/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef HASH_H
#define HASH_H

#include "common_header.h"
#include "clibUtils.h"

static inline __attribute__((always_inline)) u32 getCyclicPolynomialHash(const char *str, u32 radix, u32 hashTableSize)
{
    u32 len;
    u32 hash;
    u32 addressMask;
    u32 i;

    len = strlen(str);
    hash = len;
    addressMask = hashTableSize - 1;
    for (i = 0; i < len; i++) {
        hash = (hash << radix) | (hash >> (8 * sizeof(u32) - radix));
        hash ^= str[i];
    }
    hash ^= (hash >> 8) ^ (hash >> 16) ^ (hash >> 24);
    return hash & addressMask;
}

#endif /* HASH_H */
