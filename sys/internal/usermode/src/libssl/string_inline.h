/* Private descriptive name and filename are ours, not Sony names.
 * Shared with the out-of-line length routine and the native inlined BIO path.
 */
#ifndef LIBSSL_STRING_INLINE_H
#define LIBSSL_STRING_INLINE_H

#include <scetypes.h>

static inline SceUInt32 sslStringLengthInline(const char *string)
{
	SceUInt32 length = 0;

	while (*string++)
		length++;
	return length;
}

#endif
