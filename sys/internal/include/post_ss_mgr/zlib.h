/*
 * SceZlibForDriver (zlib 1.2.5 in the kernel), as used so far. The library exports each
 * function twice, under two NIDs; the less imported one's link name is
 * SceZlibForDriver_<name>.
 */

#pragma once

#include <scetypes.h>

#define SCE_ZLIB_VERSION	"1.2.5"

#define Z_OK			0
#define Z_STREAM_END		1
#define Z_NO_FLUSH		0
#define Z_FINISH		4

typedef void *(*SceZlibAlloc)(void *opaque, SceUInt32 items, SceUInt32 size);
typedef void (*SceZlibFree)(void *opaque, void *address);

typedef struct SceZlibStream {
	const SceUInt8 *next_in;
	SceUInt32 avail_in;
	SceUInt32 total_in;
	SceUInt8 *next_out;
	SceUInt32 avail_out;
	SceUInt32 total_out;
	const char *msg;
	void *state;
	SceZlibAlloc zalloc;
	SceZlibFree zfree;
	void *opaque;
	int data_type;
	SceUInt32 adler;
	SceUInt32 reserved;
} SceZlibStream;

int deflateInit_(SceZlibStream *strm, int level, const char *version, int size);	/* deflateInit_ */
int deflate(SceZlibStream *strm, int flush);					/* deflate */
int deflateEnd(SceZlibStream *strm);						/* deflateEnd */
int deflateInit2Padded_(SceZlibStream *strm, int level, int method, int windowBits,
	int memLevel, int strategy, const char *version, int size);
int SceZlibForDriver_inflateInitNocheck_(SceZlibStream *strm, const char *version, int size);
int inflate(SceZlibStream *strm, int flush);
int SceZlibForDriver_inflateEnd(SceZlibStream *strm);
