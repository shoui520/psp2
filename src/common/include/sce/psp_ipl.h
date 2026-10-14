/* Host-provided boot-file records consumed by the Allegrex IPL. */
#pragma once

#include <stddef.h>
#include <stdint.h>

/* Entries at 0x8b000000 end at the first NULL buffer, irrespective of name. */
typedef struct SceCompatBootFile {
	const char *name;
	void *buffer;
	uint32_t size;
} SceCompatBootFile;

_Static_assert(sizeof(SceCompatBootFile) == 12, "IPL boot-file record");
_Static_assert(offsetof(SceCompatBootFile, buffer) == 4, "IPL file buffer");
_Static_assert(offsetof(SceCompatBootFile, size) == 8, "IPL file size");
