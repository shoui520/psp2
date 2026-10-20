/*
 * SceSysclib (sysmem): the kernel's C library, SceSysclibForDriver. Each function as sysmem
 * defines it (strings.c, formatter_buffer.c, numeric.c, ctype.c, bsd_prnt.c, ...).
 */
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the number of elements of an array (ours) */
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))

/* memory */
void *memchr(const void *buffer, int value, size_t count);
int memcmp(const void *left, const void *right, size_t size);
void *memcpy(void *destination, const void *source, size_t size);
void *memmove(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size);
int timingsafe_memcmp(const void *left, const void *right, size_t count);

/* strings */
char *strchr(const char *string, int value);
int strcmp(const char *left, const char *right);
size_t strlcat(char *destination, const char *source, size_t capacity);
size_t strlcpy(char *destination, const char *source, size_t capacity);
size_t strlen(const char *string);
char *strncat(char *destination, const char *source, size_t count);
int strncmp(const char *left, const char *right, size_t count);
char *strncpy(char *destination, const char *source, size_t count);
char *strncpy_s(char *destination, const char *source, size_t limit);	/* always terminated */
size_t strnlen(const char *string, size_t count);
char *strrchr(const char *string, int value);
char *strstr(const char *string, const char *substring);

/* numbers */
long strtol(const char *string, char **end, int requestedBase);
long long strtoll(const char *string, char **end, int base);
unsigned long strtoul(const char *string, char **end, int requestedBase);

/* character classes: the class bits are sysmem/ctype.h's */
int look_ctype_table(int value) __attribute__((pure));
int tolower(int value);
int toupper(int value);

/* formatting: __prnt hands each character to output (0x200 and 0x201 mark the start and end) */
typedef void (*PrntOutput)(void *context, int character);

int __prnt(PrntOutput output, void *context, const char *format, va_list arguments);
int snprintf(char *destination, size_t capacity, const char *format, ...);
int vsnprintf(char *destination, size_t capacity, const char *format, va_list arguments);

/* the checked forms (-D_FORTIFY_SOURCE): capacity is the destination's size */
void *__memcpy_chk(void *destination, const void *source, size_t size, size_t capacity);
void *__memmove_chk(void *destination, const void *source, size_t size, size_t capacity);
void *__memset_chk(void *destination, int value, size_t size, size_t capacity);
int __snprintf_chk(char *destination, size_t size, int flags, size_t capacity, const char *format,
		   ...);
char *__strcat_chk(char *destination, const char *source, size_t capacity);
char *__strcpy_chk(char *destination, const char *source, size_t capacity);
size_t __strlcat_chk(char *destination, const char *source, size_t size, size_t capacity);
size_t __strlcpy_chk(char *destination, const char *source, size_t size, size_t capacity);
char *__strncat_chk(char *destination, const char *source, size_t count, size_t capacity);
char *__strncpy_chk(char *destination, const char *source, size_t count, size_t capacity);
int __vsnprintf_chk(char *destination, size_t size, int flags, size_t capacity,
		    const char *format, va_list arguments);

/* the stack protector's failure (stack_chk.c; noreturn there) */
void __stack_chk_fail(void);
void sceLibSspStackChkFail(void);

/* the AEABI division helpers (signed_division.S, unsigned_division.S, signed_division64.S, ...) */
int __aeabi_idiv(int numerator, int denominator);
/* NID 0xAC86B4BA: quotient in r0, signed remainder in r1 */
SceUInt64 __aeabi_idivmod(SceInt32 numerator, SceInt32 denominator);
unsigned int __aeabi_uidiv(unsigned int numerator, unsigned int denominator);
/* quotient in r0, remainder in r1 */
SceUInt64 __aeabi_uidivmod(SceUInt32 numerator, SceUInt32 denominator);
/* the quotient (the remainder follows in r2, r3) */
SceInt64 __aeabi_ldivmod(SceInt64 numerator, SceInt64 denominator);
SceUInt64 __aeabi_uldivmod(SceUInt64 numerator, SceUInt64 denominator);

#ifdef __cplusplus
}
#endif
