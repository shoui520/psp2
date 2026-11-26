/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef CLIBUTILS_H
#define CLIBUTILS_H

#include <common_header.h>

/*
 * Compare the string pointed to by lhs to the string pointed to by rhs.
 *
 * Returns 1, 0 or -1 accordingly as the string pointed to by lhs is
 * greater than, equal to, or less than the string pointed to by rhs.
 */
s32 strcmp(const char *lhs, const char *rhs);

/*
 * Copy the first n characters of the array pointed to by src to the
 * array pointed to by dest.
 */
char *strncpy(char *dest, const char *src, s32 n);

/*
 * Compute the length of the string pointed to by str.
 */
u32 strlen(const char *str);

/*
 * Copy n bytes from src into dest. When all arguments are multiples of 4,
 * copying executes via wmemcpy().
 */
void *memcpy(void *dest, const void *src, u32 n);

#endif /* CLIBUTILS_H */
