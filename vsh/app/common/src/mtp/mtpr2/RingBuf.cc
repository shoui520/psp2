/*
 * RingBuf.cc: a ring buffer (0x81137894-0x81137A18). One byte stays free, so an empty
 * buffer and a full one differ.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>

#include "mtpr2_internal.h"

using namespace mtpr2;

RingBuf::RingBuf(unsigned int size, int *error)
{
	*error = 0;
	buf = (char *)sce_paf_malloc(size);
	if (buf == NULL) {
		MTPR2_PRINT_ERROR();
		*error = MTPR2_ERROR_NO_MEMORY;
	} else {
		this->size = size;
		writePos = buf;
		readPos = buf;
		*error = 0;
	}
}

RingBuf::~RingBuf()
{
	if (buf != NULL) {
		sce_paf_free(buf);
		buf = NULL;
		size = 0;
		writePos = NULL;
		readPos = NULL;
	}
}

unsigned int RingBuf::freeSize(void)
{
	unsigned int n = readPos - writePos;

	if (readPos <= writePos)
		n = (buf + size - writePos) + (readPos - buf);
	return n - 1;
}

unsigned int RingBuf::dataSize(void)
{
	unsigned int n = writePos - readPos;

	if (readPos > writePos)
		n = (buf + size - readPos) + (writePos - buf);
	return n;
}

void RingBuf::read(void *dst, unsigned int size, unsigned int *read)
{
	unsigned int n;
	unsigned int first;
	char *pos;

	if (size == 0) {
		*read = 0;
		return;
	}
	n = dataSize();
	if (n > size)
		n = size;
	first = buf + this->size - readPos;
	sce_paf_memcpy(dst, readPos, n < first ? n : first);
	if (n <= first) {
		pos = readPos + n;
		*read = n;
		readPos = pos;
	} else {
		readPos = buf;
		sce_paf_memcpy((char *)dst + first, buf, n - first);
		pos = readPos + (n - first);
		*read = n;
		readPos = pos;
	}
}

void RingBuf::write(void *src, unsigned int size, unsigned int *written)
{
	unsigned int n;
	unsigned int first;
	char *pos;

	if (size == 0) {
		*written = 0;
		return;
	}
	n = freeSize();
	if (n > size)
		n = size;
	first = buf + this->size - writePos;
	sce_paf_memcpy(writePos, src, n < first ? n : first);
	if (n <= first) {
		pos = writePos + n;
		*written = n;
		writePos = pos;
	} else {
		writePos = buf;
		sce_paf_memcpy(buf, (char *)src + first, n - first);
		pos = writePos + (n - first);
		*written = n;
		writePos = pos;
	}
}
