/*
 * paf::compress. Compress deflates through paf's zlib in 1 KiB steps, with the
 * stream's memory from the given heap (the global heap by default), writing
 * each full output block to the file; Decompress inflates through paf's copy
 * of the SDK's deflate library.
 */
#include <compress/compress.h>
#include <file/file.h>
#include <memory/heap_allocator.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <zlib.h>

#define sceZlibDecompress ScePafLowlayer_D7DAA151
extern "C" {
#include <deflt.h>
}
#include <libdeflt.h>

/* GetGlobalHeapAllocator, exported by its C name. */
extern "C" paf::memory::HeapAllocator *ScePafStdc_261CAF72(void);

namespace paf {
namespace compress {

static voidpf Alloc(voidpf opaque, uInt items, uInt size);
static void Free(voidpf opaque, voidpf address);

static uint8_t s_out[1024];
static uint8_t s_in[1024];

int32_t Compress(common::SharedPtr<File> file, const void *src, size_t srcsize, CompressType type,
		 memory::HeapAllocator *heap)
{
	int32_t ret;

	if (heap == NULL)
		heap = ScePafStdc_261CAF72();
	if (type == CompressType_Zlib) {
		common::SharedPtr<File> out(file);
		z_stream stream;
		int32_t result;
		int flush;
		size_t written, rest;

		stream.zalloc = Alloc;
		stream.zfree = Free;
		stream.opaque = heap;
		if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
			sce_paf_printf_dummy();
			ret = SCE_PAF_ERROR_COMPRESS_INIT;
			goto end;
		}
		written = 0;
		stream.avail_in = 0;
		stream.next_out = s_out;
		stream.avail_out = sizeof(s_out);
		flush = Z_NO_FLUSH;
		for (;;) {
			if (stream.avail_in == 0) {
				stream.next_in = s_in;
				if (srcsize >= sizeof(s_in)) {
					sce_paf_memcpy(s_in, src, sizeof(s_in));
					stream.avail_in = sizeof(s_in);
					src = (const uint8_t *)src + sizeof(s_in);
					srcsize -= sizeof(s_in);
				} else if (srcsize != 0) {
					sce_paf_memcpy(s_in, src, srcsize);
					stream.avail_in = srcsize;
					srcsize = 0;
					src = NULL;
					flush = Z_FINISH;
				}
			}
			result = deflate(&stream, flush);
			if (result == Z_STREAM_END)
				break;
			if (result != Z_OK) {
				sce_paf_printf_dummy();
				ret = SCE_PAF_ERROR_COMPRESS_DEFLATE;
				goto end;
			}
			if (stream.avail_out == 0) {
				result = out->Write(s_out, sizeof(s_out));
				if (result != sizeof(s_out)) {
					sce_paf_printf_dummy();
					ret = result;
					goto end;
				}
				stream.next_out = s_out;
				stream.avail_out = sizeof(s_out);
				written += sizeof(s_out);
			}
		}
		rest = sizeof(s_out) - stream.avail_out;
		if (stream.avail_out != sizeof(s_out)) {
			result = out->Write(s_out, rest);
			if (rest != (size_t)result) {
				sce_paf_printf_dummy();
				ret = result;
				goto end;
			}
			written += result;
		}
		if (deflateEnd(&stream) != Z_OK) {
			sce_paf_printf_dummy();
			ret = SCE_PAF_ERROR_COMPRESS_END;
		} else {
			ret = written;
		}
end:
		;
	} else {
		sce_paf_printf_dummy();
		ret = SCE_PAF_ERROR_COMPRESS_INVALID_FORMAT;
	}
	return ret;
}

static voidpf Alloc(voidpf opaque, uInt items, uInt size)
{
	return ((memory::HeapAllocator *)opaque)->sce_paf_malloc(items * size);
}

static void Free(voidpf opaque, voidpf address)
{
	((memory::HeapAllocator *)opaque)->sce_paf_free(address);
}

int32_t Decompress(void *dst, size_t dstsize, const void *src, size_t srcsize,
		   DecompressType type)
{
	int32_t ret;

	if (type == DecompressType_Zlib) {
		ret = sceZlibDecompress(dst, dstsize, src, NULL);
		if (ret < 0) {
			switch (ret) {
			case SCE_DEFLATE_ERROR_INVALID_FORMAT:
				return SCE_PAF_ERROR_COMPRESS_INVALID_FORMAT;
			case SCE_DEFLATE_ERROR_INVALID_SIZE:
				return SCE_PAF_ERROR_COMPRESS_INVALID_SIZE;
			}
		}
		return ret;
	}
	sce_paf_printf_dummy();
	return SCE_PAF_ERROR_COMPRESS_INVALID_FORMAT;
}

}
}
