/*
 * paf::compress: a buffer deflated into a file as a zlib stream, and a zlib
 * buffer inflated into memory. C code calls Decompress by its link name.
 */
#ifndef PAF_COMPRESS_COMPRESS_H
#define PAF_COMPRESS_COMPRESS_H

#include <stddef.h>
#include <stdint.h>

#define SCE_PAF_ERROR_COMPRESS_INVALID_FORMAT	(-2135992576)	/* 0x80AF5700 */
#define SCE_PAF_ERROR_COMPRESS_INIT		(-2135992575)	/* 0x80AF5701 */
#define SCE_PAF_ERROR_COMPRESS_END		(-2135992574)	/* 0x80AF5702 */
#define SCE_PAF_ERROR_COMPRESS_DEFLATE		(-2135992573)	/* 0x80AF5703 */
#define SCE_PAF_ERROR_COMPRESS_INVALID_SIZE	(-2135992572)	/* 0x80AF5704 */

#ifdef __cplusplus

#include <common/shared_ptr.h>

namespace paf {

class File;

namespace memory {
class HeapAllocator;
}

namespace compress {

enum CompressType {
	CompressType_None = 0,
	CompressType_Zlib = 1
};

enum DecompressType {
	DecompressType_None = 0,
	DecompressType_Zlib = 1
};

int32_t Compress(common::SharedPtr<File> file, const void *src, size_t srcsize, CompressType type,
		 memory::HeapAllocator *heap);
int32_t Decompress(void *dst, size_t dstsize, const void *src, size_t srcsize,
		   DecompressType type);

}
}

#else

typedef enum DecompressType {
	DecompressType_None = 0,
	DecompressType_Zlib = 1
} DecompressType;

#define Decompress _ZN3paf8compress10DecompressEPvjPKvjNS0_14DecompressTypeE

int32_t Decompress(void *dst, size_t dstsize, const void *src, size_t srcsize,
		   DecompressType type);

#endif

#endif
