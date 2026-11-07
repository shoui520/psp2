/* Private descriptive identifiers and filename are ours, not Sony names. */
#ifndef LIBSSL_FORMAT_H
#define LIBSSL_FORMAT_H

#include <scetypes.h>
#include <stdarg.h>

SceUInt32 sslStringLength(const char *string);
int sslCompareCString(const char *left, const char *right);
char *sslCopyCString(char *destination, const char *source);
void *sslBinarySearch(const void *key, const void *base, int count,
		      int size, int (*compare)(const void *, const void *));
char *sslCopyString(char *destination, const char *source, SceUInt32 size);
char *sslAppendString(char *destination, const char *source, SceUInt32 size);
int sslCompareStrings(const char *left, const char *right, SceUInt32 size);
int sslVFormatString(char *destination, SceUInt32 size, const char *format,
		     va_list arguments);
int sslFormatString(char *destination, SceUInt32 size, const char *format, ...);
int sslToUpper(int character);

#endif
