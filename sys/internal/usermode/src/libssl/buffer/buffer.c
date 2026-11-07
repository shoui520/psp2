/* SSLeay buffer management with measured SSL-C changes in buffer_sce.patch. */
#include "buffer.h"
#include "../memory.h"
#include "../string_inline.h"

#define Malloc sslAllocate
#define Free sslFree
#define memcpy sslCopyMemory
#define memmove sslMoveMemory
#define memset sslFillMemory
#define strlen sslStringLengthInline
#include "buffer_sce.inc"

int sslBufferAppend(BUF_MEM *str, const void *data, int len)
{
	int offset;
	int target;
	int ret;

	offset = str->length;
	target = (int)((unsigned int)offset + (unsigned int)len);
	ret = BUF_MEM_grow(str, target);
	if (ret == target)
		memcpy(&str->data[offset], data, (SceUInt32)len);
	return ret;
}

int sslBufferRead(BUF_MEM *str, void *data, int len)
{
	if (len < str->length) {
		if (data != NULL)
			memcpy(data, str->data, (SceUInt32)len);
		memmove(str->data, &str->data[len], (SceUInt32)(str->length - len));
		str->length -= len;
	} else {
		if (data != NULL)
			memcpy(data, str->data, (SceUInt32)str->length);
		len = str->length;
		str->length = 0;
	}
	return len;
}
