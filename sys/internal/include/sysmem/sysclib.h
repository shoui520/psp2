/* SceSysclibForDriver: the ABI helpers and the functions modules use without vitasdk's header. */
#pragma once

#include <psp2kern/types.h>

/* NID 0xAC86B4BA: quotient in r0, signed remainder in r1. */
SceUInt64 __aeabi_idivmod(SceInt32 numerator, SceInt32 denominator);

/* sysclib's memory compare, as vitasdk's psp2kern/kernel/sysclib.h declares it */
int memcmp(const void *s1, const void *s2, unsigned int len);

void *__memset_chk(void *destination, int value, size_t size, size_t capacity);
void __stack_chk_fail(void);
char *__strncat_chk(char *destination, const char *source, size_t count, size_t capacity);
char *__strncat_chk2(char *destination, const char *source, size_t capacity);
char *__strncpy_chk(char *destination, const char *source, size_t count, size_t capacity);
void *memchr(const void *buffer, int value, size_t count);
void *memcpy(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size);
int snprintf(char *destination, size_t capacity, const char *format, ...);
char *strchr(const char *string, int value);
int strcmp(const char *left, const char *right);
size_t strlen(const char *string);
char *strncat(char *destination, const char *source, size_t count);
int strncmp(const char *left, const char *right, size_t count);
char *strncpy(char *destination, const char *source, size_t count);
char *strncpy_s(char *destination, const char *source, size_t limit);	/* copies at most limit characters, always terminated */
size_t strnlen(const char *string, size_t count);
int timingsafe_memcmp(const void *left, const void *right, size_t count);
