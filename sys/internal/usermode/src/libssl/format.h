/* Private descriptive identifiers and filename are ours, not Sony names. */
#ifndef LIBSSL_FORMAT_H
#define LIBSSL_FORMAT_H

#include <scetypes.h>
#include <stdarg.h>

char *sslCopyString(char *destination, const char *source, SceUInt32 size);
char *sslAppendString(char *destination, const char *source, SceUInt32 size);
int sslVFormatString(char *destination, SceUInt32 size, const char *format,
		     va_list arguments);
int sslFormatString(char *destination, SceUInt32 size, const char *format, ...);

#endif
