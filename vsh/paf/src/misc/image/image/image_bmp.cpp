/*
 * BMP: the format's attribute, its check, the reader of uncompressed 1, 4, 8,
 * 16, 24 and 32-bit images (16 and 32-bit with their channel masks) and the
 * writer of 24-bit images.
 */
#include <image/image.h>
#include <buffer/buffer.h>
#include <file/file.h>
#include <file/localfile.h>
#include <kernel/iofilemgr.h>
#include <memory/heap_allocator.h>
#include <std/new>
#include <std/stdlib.h>
#include <std/string.h>
#include "image_codec.h"
#include "image_source.h"
extern "C" {
#include "../../../paf_types.h"
}

using namespace paf;
using namespace paf::image;
using paf::memory::HeapAllocator;

namespace paf {
namespace image {

#pragma pack(push, 1)

struct BmpFileHeader {
	uint16_t bfType;
	uint32_t bfSize;
	uint16_t bfReserved1;
	uint16_t bfReserved2;
	uint32_t bfOffBits;
};

struct BmpInfoHeader {
	uint32_t biSize;
	int32_t biWidth;
	int32_t biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	int32_t biXPelsPerMeter;
	int32_t biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
};

#pragma pack(pop)

enum {
	BMP_COMPRESSION_RGB = 0,
	BMP_COMPRESSION_BITFIELDS = 3
};

/* The colours of a palette, as the file's B, G, R, reserved bytes. */
struct BmpPalette {
	uint16_t count;
	uint8_t *colors;
};

/* A channel of a 16 or 32-bit pixel: its bits, shifted down to max. */
struct BmpChannel {
	uint32_t mask;
	uint32_t shift;
	uint32_t bits;
	uint32_t max;
};

struct BmpMasks {
	BmpChannel red;
	BmpChannel green;
	BmpChannel blue;
	uint32_t alpha;		/* the bits no colour uses */
};

/*
 * A BMP image in a file or in memory, read from offset, and its headers.
 * Sony's layout is 4-byte packed: its 64-bit fields are stored byte by byte.
 */
#pragma pack(push, 4)

class BmpContext {
public:
	BmpContext(common::SharedPtr<File> file, int64_t offset, size_t size,
		   HeapAllocator *allocator);
	BmpContext(common::SharedPtr<Buffer> buf, HeapAllocator *allocator);

	~BmpContext()
	{
		if (!m_failed && m_palette.colors != NULL) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(m_palette.colors);
			else
				sce_paf_free(m_palette.colors);
			m_palette.count = 0;
			m_palette.colors = NULL;
		}
	}

	/* size bytes into buf; a short read is INVALID_DATA. */
	bool Read(void *buf, size_t size)
	{
		if (buf == NULL)
			return false;
		if (m_read(this, buf, size) != size) {
			m_errortype = SCE_PAF_ERROR_IMAGE_INVALID_DATA;
			return false;
		}
		return true;
	}

	bool ReadFileHeader()
	{
		if (m_read(this, &m_fileheader, sizeof(BmpFileHeader)) != sizeof(BmpFileHeader)) {
			m_errortype = SCE_PAF_ERROR_IMAGE_INVALID_DATA;
			return false;
		}
		return true;
	}

	BmpPalette *ReadPalette();
	void TracePalette();
	bool SetDefaultMasks();
	bool ReadMasks();
	bool ReadInfoHeader();
	void Open(common::SharedPtr<File> file, int64_t offset, size_t size) __attribute__((noinline));
	void TraceOpen();

	void Open(common::SharedPtr<Buffer> buf);

	union {
		File *m_file;
		uint8_t *m_addr;
	};
	uint32_t m_padding0;
	int64_t m_size;
	uint32_t m_position;
	uint32_t m_padding1;
	int64_t m_offset;
	BmpFileHeader m_fileheader;
	BmpInfoHeader m_infoheader;
	BmpPalette m_palette;
	BmpMasks m_masks;
	size_t (*m_read)(BmpContext *ctx, void *buf, size_t size);
	int64_t (*m_seek)(BmpContext *ctx, int64_t offset, int32_t whence);
	uint8_t m_topdown;	/* the rows are stored top row first */
	bool m_opened;
	bool m_failed;
	HeapAllocator *m_allocator;
	int32_t m_errortype;
};

#pragma pack(pop)

}
}

static size_t FileRead(BmpContext *ctx, void *buf, size_t size);
static int64_t FileSeek(BmpContext *ctx, int64_t offset, int32_t whence);
static size_t MemoryRead(BmpContext *ctx, void *buf, size_t size);
static int64_t MemorySeek(BmpContext *ctx, int64_t offset, int32_t whence);

/* value, bits wide, scaled to 8 bits by repeating it. */
static inline uint8_t ExpandChannel(uint32_t pixel, const BmpChannel *channel)
{
	uint32_t value = ((pixel & channel->mask) >> channel->shift) & channel->max;
	uint32_t remain = 8;
	uint32_t result = 0;

	while (remain > channel->bits) {
		remain -= channel->bits;
		result |= value << remain;
	}
	if (remain != 0)
		result |= value >> (channel->bits - remain);
	return result;
}

static inline uint32_t GetMaskShift(uint32_t mask)
{
	uint32_t shift = 0;
	uint32_t bit = 1;

	while ((mask & bit) == 0 && shift < 32) {
		bit <<= 1;
		shift++;
	}
	return shift;
}

static inline uint32_t GetMaskBits(uint32_t mask)
{
	uint32_t shift = 0;
	uint32_t bit = 1;

	while ((mask & bit) == 0 && shift < 32) {
		bit <<= 1;
		shift++;
	}
	uint32_t end = shift;
	while (end < 32 && (mask & bit) != 0) {
		bit <<= 1;
		end++;
	}
	return end - shift;
}

static inline uint32_t GetMaskMax(uint32_t bits)
{
	uint32_t max = 0;
	uint32_t bit = 1;

	for (uint32_t i = 0; i < bits; i++) {
		max |= bit;
		bit <<= 1;
	}
	return max;
}

bool BmpAttribute::GetAttribute(uint32_t key, bool &value)
{
	return false;
}

bool BmpAttribute::GetAttribute(uint32_t key, int &value)
{
	return false;
}

bool BmpAttribute::GetAttribute(uint32_t key, float &value)
{
	return false;
}

bool BmpAttribute::GetAttribute(uint32_t key, string &value)
{
	return false;
}

BmpAttribute::BmpAttribute()
{
}

BmpAttribute::~BmpAttribute()
{
}

bool paf::image::IsBmp(common::SharedPtr<Buffer> buf)
{
	if (buf->GetSize() < 2)
		return false;

	const char signature[2] = { 'B', 'M' };
	return sce_paf_memcmp(signature, buf->GetAddr(), 2) == 0;
}

bool BmpCodec::Decode32(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
			DecodeCancelFunc cancelfunc, void *userdata)
{
	uint32_t width = attr->m_extent.w;
	uint32_t height = attr->m_extent.h;
	size_t pitch = attr->GetMemPitch(0);
	int32_t skip = pitch - width * 4;

	if (!ctx->m_topdown) {
		dst += height * pitch - pitch;
		skip = -(pitch + width * 4);
	}

	size_t linesize = (width * 4 + 3) & ~3;
	void *line;
	if (m_allocator != NULL)
		line = m_allocator->sce_paf_malloc(linesize);
	else
		line = sce_paf_malloc(linesize);
	if (line == NULL) {
		SetDecodeError(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	for (uint32_t y = 0; y < height; y++) {
		if (cancelfunc != NULL && cancelfunc(userdata)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(SCE_PAF_ERROR_IMAGE_CANCELED);
			return false;
		}
		if (!ctx->Read(line, linesize)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(ctx->m_errortype);
			return false;
		}

		const uint8_t *src = (const uint8_t *)line;
		for (uint32_t x = 0; x < width; x++) {
			uint32_t pixel = (uint16_t)(src[0] | src[1] << 8);
			pixel |= src[2] << 16;
			pixel |= src[3] << 24;
			src += 4;
			dst[0] = ExpandChannel(pixel, &ctx->m_masks.red);
			dst[1] = ExpandChannel(pixel, &ctx->m_masks.green);
			dst[2] = ExpandChannel(pixel, &ctx->m_masks.blue);
			dst[3] = 0xff;
			dst += 4;
		}
		dst += skip;
	}

	if (m_allocator != NULL)
		m_allocator->sce_paf_free(line);
	else
		sce_paf_free(line);
	return true;
}

bool BmpCodec::Decode24(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
			DecodeCancelFunc cancelfunc, void *userdata)
{
	uint32_t width = attr->m_extent.w;
	uint32_t height = attr->m_extent.h;
	size_t pitch = attr->GetMemPitch(0);
	int32_t skip = pitch - width * 4;

	if (!ctx->m_topdown) {
		skip = -(pitch + width * 4);
		dst += height * pitch - pitch;
	}

	size_t linesize = (width * 3 + 3) & ~3;
	void *line;
	if (m_allocator != NULL)
		line = m_allocator->sce_paf_malloc(linesize);
	else
		line = sce_paf_malloc(linesize);
	if (line == NULL) {
		SetDecodeError(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	for (uint32_t y = 0; y < height; y++) {
		if (cancelfunc != NULL && cancelfunc(userdata)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(SCE_PAF_ERROR_IMAGE_CANCELED);
			return false;
		}
		if (!ctx->Read(line, linesize)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(ctx->m_errortype);
			return false;
		}

		const uint8_t *src = (const uint8_t *)line;
		for (uint32_t x = 0; x < width; x++) {
			dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
			dst[3] = 0xff;
			src += 3;
			dst += 4;
		}
		dst += skip;
	}

	if (m_allocator != NULL)
		m_allocator->sce_paf_free(line);
	else
		sce_paf_free(line);
	return true;
}

bool BmpCodec::Decode16(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
			DecodeCancelFunc cancelfunc, void *userdata)
{
	uint32_t width = attr->m_extent.w;
	uint32_t height = attr->m_extent.h;
	size_t pitch = attr->GetMemPitch(0);
	int32_t skip;

	if (ctx->m_topdown) {
		skip = pitch - (attr->m_mode == ImageMode_RGBA8888 ? width * 4 : width * 2);
	} else {
		dst += height * pitch - pitch;
		skip = -(pitch + (attr->m_mode == ImageMode_RGBA8888 ? width * 4 : width * 2));
	}

	size_t linesize = (width * 2 + 3) & ~3;
	uint8_t *line;
	if (m_allocator != NULL)
		line = (uint8_t *)m_allocator->sce_paf_malloc(linesize);
	else
		line = (uint8_t *)sce_paf_malloc(linesize);
	if (line == NULL) {
		SetDecodeError(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	if (attr->m_mode != ImageMode_RGBA8888) {
		for (uint32_t y = 0; y < height; y++) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(line);
				else
					sce_paf_free(line);
				SetDecodeError(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			if (!ctx->Read(line, linesize)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(line);
				else
					sce_paf_free(line);
				SetDecodeError(ctx->m_errortype);
				return false;
			}

			/* red and blue swapped, the unused bits set */
			const uint8_t *src = line;
			for (uint32_t x = 0; x < width; x++) {
				uint32_t pixel = src[0] | src[1] << 8;
				uint32_t value = (pixel & ctx->m_masks.green.mask) |
						 ((pixel & ctx->m_masks.red.mask) >> ctx->m_masks.red.shift |
						  (pixel & ctx->m_masks.blue.mask) << ctx->m_masks.red.shift) |
						 ctx->m_masks.alpha;
				dst[0] = value;
				dst[1] = value >> 8;
				dst += 2;
				src += 2;
			}
			dst += skip;
		}
	} else {
		for (uint32_t y = 0; y < height; y++) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(line);
				else
					sce_paf_free(line);
				SetDecodeError(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			if (!ctx->Read(line, linesize)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(line);
				else
					sce_paf_free(line);
				SetDecodeError(ctx->m_errortype);
				return false;
			}

			const uint8_t *src = line;
			for (uint32_t x = 0; x < width; x++) {
				uint32_t pixel = src[0] | src[1] << 8;
				src += 2;
				dst[0] = ExpandChannel(pixel, &ctx->m_masks.red);
				dst[1] = ExpandChannel(pixel, &ctx->m_masks.green);
				dst[2] = ExpandChannel(pixel, &ctx->m_masks.blue);
				dst[3] = 0xff;
				dst += 4;
			}
			dst += skip;
		}
	}

	if (m_allocator != NULL)
		m_allocator->sce_paf_free(line);
	else
		sce_paf_free(line);
	return true;
}

bool BmpCodec::Decode8(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
		       DecodeCancelFunc cancelfunc, void *userdata)
{
	uint32_t width = attr->m_extent.w;
	uint32_t height = attr->m_extent.h;
	size_t pitch = attr->GetMemPitch(0);
	int32_t skip = pitch;

	if (!ctx->m_topdown) {
		skip = -pitch;
		dst += height * pitch - pitch;
	}

	void *line;
	if (m_allocator != NULL)
		line = m_allocator->sce_paf_malloc((width + 3) & ~3);
	else
		line = sce_paf_malloc((width + 3) & ~3);
	if (line == NULL) {
		SetDecodeError(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	for (uint32_t y = 0; y < height; y++) {
		if (cancelfunc != NULL && cancelfunc(userdata)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(SCE_PAF_ERROR_IMAGE_CANCELED);
			return false;
		}
		if (!ctx->Read(line, (width + 3) & ~3)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(ctx->m_errortype);
			return false;
		}
		sce_paf_memcpy(dst, line, width);
		dst += skip;
	}

	if (m_allocator != NULL)
		m_allocator->sce_paf_free(line);
	else
		sce_paf_free(line);
	return true;
}

bool BmpCodec::Decode4(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
		       DecodeCancelFunc cancelfunc, void *userdata)
{
	uint32_t width = attr->m_extent.w;
	uint32_t height = attr->m_extent.h;
	size_t pitch = attr->GetMemPitch(0);
	uint32_t bytes = (width + 1) >> 1;
	int32_t skip = pitch - bytes;

	if (!ctx->m_topdown) {
		skip = -(pitch + bytes);
		dst += height * pitch - pitch;
	}

	size_t linesize = (bytes + 3) & ~3;
	uint8_t *line;
	if (m_allocator != NULL)
		line = (uint8_t *)m_allocator->sce_paf_malloc(linesize);
	else
		line = (uint8_t *)sce_paf_malloc(linesize);
	if (line == NULL) {
		SetDecodeError(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	for (uint32_t y = 0; y < height; y++) {
		if (cancelfunc != NULL && cancelfunc(userdata)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(SCE_PAF_ERROR_IMAGE_CANCELED);
			return false;
		}
		if (!ctx->Read(line, linesize)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(ctx->m_errortype);
			return false;
		}

		/* the first pixel of each byte into the low nibble */
		const uint8_t *src = line;
		for (uint32_t x = 0; x < (width + 1) >> 1; x++) {
			uint8_t pixels = *src++;
			*dst++ = (pixels >> 4 & 0x0f) | (pixels << 4 & 0xf0);
		}
		dst += skip;
	}

	if (m_allocator != NULL)
		m_allocator->sce_paf_free(line);
	else
		sce_paf_free(line);
	return true;
}

bool BmpCodec::Decode1(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
		       DecodeCancelFunc cancelfunc, void *userdata)
{
	uint32_t width = attr->m_extent.w;
	uint32_t height = attr->m_extent.h;
	size_t pitch = attr->GetMemPitch(0);
	uint32_t bytes = (width + 1) >> 1;
	int32_t skip = pitch - bytes;

	if (!ctx->m_topdown) {
		skip = -(pitch + bytes);
		dst += height * pitch - pitch;
	}

	size_t linesize = (((width + 7) >> 3) + 3) & ~3;
	uint8_t *line;
	if (m_allocator != NULL)
		line = (uint8_t *)m_allocator->sce_paf_malloc(linesize);
	else
		line = (uint8_t *)sce_paf_malloc(linesize);
	if (line == NULL) {
		SetDecodeError(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	/* two pixels to a byte of the 4-bit image, the first in the low nibble */
	while (height-- != 0) {
		if (cancelfunc != NULL && cancelfunc(userdata)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(SCE_PAF_ERROR_IMAGE_CANCELED);
			return false;
		}
		if (!ctx->Read(line, linesize)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			SetDecodeError(ctx->m_errortype);
			return false;
		}

		uint32_t i;
		const uint8_t *src = line;
		for (i = 0; i < width >> 3; i++) {
			dst[0] = (*src >> 7 & 1) | (*src >> 6 & 1) << 4;
			dst[1] = (*src >> 5 & 1) | (*src >> 4 & 1) << 4;
			dst[2] = (*src >> 3 & 1) | (*src >> 2 & 1) << 4;
			dst[3] = (*src >> 1 & 1) | (*src & 1) << 4;
			src++;
			dst += 4;
		}
		if ((width & 7) != 0) {
			*dst++ = (line[i] >> 7 & 1) | (line[i] >> 6 & 1) << 4;
			if ((width & 7) >= 3)
				*dst++ = (line[i] >> 5 & 1) | (line[i] >> 4 & 1) << 4;
			if ((width & 7) >= 5)
				*dst++ = (line[i] >> 3 & 1) | (line[i] >> 2 & 1) << 4;
			if ((width & 7) >= 7)
				*dst++ = (line[i] >> 1 & 1) | (line[i] & 1) << 4;
		}
		dst += skip;
	}

	if (m_allocator != NULL)
		m_allocator->sce_paf_free(line);
	else
		sce_paf_free(line);
	return true;
}

void BmpCodec::TraceDecode()
{
}

bool BmpCodec::SetPalette(uint8_t *dst, size_t size, const BmpPalette *palette, int32_t count)
{
	if (dst == NULL || palette == NULL || count == 0)
		return false;
	if (size < palette->count * 4u) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return false;
	}

	int32_t i;
	for (i = 0; i < palette->count; i++) {
		dst[0] = palette->colors[i * 4 + 2];
		dst[1] = palette->colors[i * 4 + 1];
		dst[2] = palette->colors[i * 4];
		dst[3] = 0xff;
		dst += 4;
	}
	for (; i < count; i++) {
		dst[0] = 0;
		dst[1] = 0;
		dst[2] = 0;
		dst[3] = 0xff;
		dst += 4;
	}
	return true;
}

BmpPalette *BmpContext::ReadPalette()
{
	if (m_infoheader.biCompression != BMP_COMPRESSION_RGB || m_infoheader.biBitCount > 8) {
		m_errortype = SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT;
		return NULL;
	}

	uint32_t count = m_infoheader.biClrUsed;
	if (count == 0)
		count = 1 << m_infoheader.biBitCount;
	m_palette.count = count;

	uint16_t size = count * 4;
	if (m_allocator != NULL)
		m_palette.colors = (uint8_t *)m_allocator->sce_paf_malloc(size);
	else
		m_palette.colors = (uint8_t *)sce_paf_malloc(size);
	if (m_palette.colors == NULL) {
		m_errortype = SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY;
		return NULL;
	}
	if (m_read(this, m_palette.colors, size) != size) {
		m_errortype = SCE_PAF_ERROR_IMAGE_INVALID_DATA;
		return NULL;
	}
	if (m_fileheader.bfOffBits != 0)
		m_seek(this, m_offset + m_fileheader.bfOffBits, SCE_SEEK_SET);
	return &m_palette;
}

void BmpContext::TracePalette()
{
}

bool BmpContext::SetDefaultMasks()
{
	uint32_t max = 0;

	if (m_infoheader.biCompression != BMP_COMPRESSION_RGB)
		return false;
	if (m_infoheader.biBitCount != 16 && m_infoheader.biBitCount != 32)
		return false;

	if (m_infoheader.biBitCount == 16) {
		/* X1R5G5B5 */
		m_masks.red.mask = 0x7c00;
		m_masks.red.shift = 10;
		m_masks.green.mask = 0x03e0;
		m_masks.green.shift = 5;
		m_masks.blue.mask = 0x001f;
		m_masks.blue.shift = 0;
		m_masks.red.bits = 5;
		m_masks.green.bits = 5;
		m_masks.blue.bits = 5;
		max = GetMaskMax(m_masks.red.bits);
		m_masks.alpha = 0x8000;
	} else {
		/* X8R8G8B8 */
		m_masks.red.mask = 0x00ff0000;
		m_masks.red.shift = 16;
		m_masks.green.mask = 0x0000ff00;
		m_masks.green.shift = 8;
		m_masks.blue.mask = 0x000000ff;
		m_masks.blue.shift = 0;
		m_masks.red.bits = 8;
		m_masks.green.bits = 8;
		m_masks.blue.bits = 8;
		max = GetMaskMax(m_masks.red.bits);
	}
	m_masks.red.max = max;
	m_masks.green.max = max;
	m_masks.blue.max = max;
	return true;
}

static inline uint32_t ReadUInt32(BmpContext *ctx)
{
	uint8_t bytes[4];
	uint32_t value = 0;

	if (ctx->m_read(ctx, bytes, 4) == 4) {
		value = (uint16_t)(bytes[0] | bytes[1] << 8);
		value |= bytes[2] << 16;
		value |= bytes[3] << 24;
	}
	return value;
}

bool BmpContext::ReadMasks()
{
	if (m_infoheader.biCompression != BMP_COMPRESSION_BITFIELDS ||
	    (m_infoheader.biBitCount != 16 && m_infoheader.biBitCount != 32))
		return false;

	m_masks.red.mask = ReadUInt32(this);
	m_masks.green.mask = ReadUInt32(this);
	m_masks.blue.mask = ReadUInt32(this);
	m_masks.red.shift = GetMaskShift(m_masks.red.mask);
	m_masks.green.shift = GetMaskShift(m_masks.green.mask);
	m_masks.blue.shift = GetMaskShift(m_masks.blue.mask);
	m_masks.red.bits = GetMaskBits(m_masks.red.mask);
	m_masks.green.bits = GetMaskBits(m_masks.green.mask);
	m_masks.blue.bits = GetMaskBits(m_masks.blue.mask);
	m_masks.red.max = GetMaskMax(m_masks.red.bits);
	m_masks.green.max = GetMaskMax(m_masks.green.bits);
	m_masks.blue.max = GetMaskMax(m_masks.blue.bits);
	m_masks.alpha = ~(m_masks.red.mask | m_masks.green.mask | m_masks.blue.mask);
	return true;
}

bool BmpContext::ReadInfoHeader()
{
	if (m_read(this, &m_infoheader, sizeof(BmpInfoHeader)) != sizeof(BmpInfoHeader)) {
		m_errortype = SCE_PAF_ERROR_IMAGE_INVALID_DATA;
		return false;
	}
	if (m_infoheader.biSize < sizeof(BmpInfoHeader)) {
		m_errortype = SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT;
		return false;
	}
	if (m_infoheader.biPlanes != 1) {
		m_errortype = SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT;
		return false;
	}

	uint16_t bitcount = m_infoheader.biBitCount;
	if (bitcount >= 16 && m_infoheader.biClrUsed != 0) {
		m_errortype = SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT;
		return false;
	}

	int32_t height = m_infoheader.biHeight;
	uint32_t compression = m_infoheader.biCompression;
	if (height < 0) {
		m_topdown = 1;
		height = -height;
		m_infoheader.biHeight = height;
	}
	if (compression == BMP_COMPRESSION_RGB || compression == BMP_COMPRESSION_BITFIELDS)
		m_infoheader.biSizeImage =
			(((m_infoheader.biWidth * bitcount + 7) / 8 + 3) & ~3) * height;
	if (bitcount == 16 || bitcount == 32) {
		if (compression == BMP_COMPRESSION_BITFIELDS)
			ReadMasks();
		else
			SetDefaultMasks();
	}
	if (m_infoheader.biSize != sizeof(BmpInfoHeader))
		m_seek(this, m_offset + m_infoheader.biSize + sizeof(BmpFileHeader), SCE_SEEK_SET);
	return true;
}

bool BmpCodec::Decode(BmpContext *ctx, const ImageAttribute *attr, ImageArray *images,
		      ImageArray *cluts, DecodeCancelFunc cancelfunc, void *userdata)
{
	if (!ctx->ReadFileHeader()) {
		SetDecodeError(ctx->m_errortype);
		return false;
	}
	if (!ctx->ReadInfoHeader()) {
		SetDecodeError(ctx->m_errortype);
		return false;
	}

	if (ctx->m_infoheader.biBitCount <= 8) {
		if (cluts == NULL)
			return false;
		uint8_t *clut = (uint8_t *)cluts->GetBuffer(0)->buff;
		if (clut == NULL)
			return false;
		size_t size = cluts->GetBuffer(0)->size;
		BmpPalette *palette = ctx->ReadPalette();
		if (palette == NULL) {
			SetDecodeError(ctx->m_errortype);
			return false;
		}
		if (!SetPalette(clut, size, palette, attr->m_clutsize))
			return false;
	}

	uint8_t *dst = (uint8_t *)images->GetBuffer(0)->buff;
	uint16_t bitcount = ctx->m_infoheader.biBitCount;
	uint32_t compression = ctx->m_infoheader.biCompression;
	if (bitcount == 1 && compression == BMP_COMPRESSION_RGB)
		return Decode1(ctx, dst, attr, cancelfunc, userdata);
	if (bitcount == 4 && compression == BMP_COMPRESSION_RGB)
		return Decode4(ctx, dst, attr, cancelfunc, userdata);
	if (bitcount == 8 && compression == BMP_COMPRESSION_RGB)
		return Decode8(ctx, dst, attr, cancelfunc, userdata);
	if (bitcount == 16 &&
	    (compression == BMP_COMPRESSION_RGB || compression == BMP_COMPRESSION_BITFIELDS))
		return Decode16(ctx, dst, attr, cancelfunc, userdata);
	if (bitcount == 24 && compression == BMP_COMPRESSION_RGB)
		return Decode24(ctx, dst, attr, cancelfunc, userdata);
	if (bitcount == 32 &&
	    (compression == BMP_COMPRESSION_RGB || compression == BMP_COMPRESSION_BITFIELDS))
		return Decode32(ctx, dst, attr, cancelfunc, userdata);
	return false;
}

void BmpContext::Open(common::SharedPtr<File> file, int64_t offset, size_t size)
{
	m_offset = offset;
	m_file = file.get();
	size_t filesize = m_file->GetFileSize();
	m_position = 0;
	m_read = FileRead;
	m_seek = FileSeek;
	m_size = filesize;

	if (m_seek(this, 0, SCE_SEEK_SET) < 0) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_SEEK);
		m_errortype = SCE_PAF_ERROR_IMAGE_SEEK;
		m_failed = true;
		m_opened = false;
	}
	if (offset != 0) {
		if (m_seek(this, offset, SCE_SEEK_SET) < 0) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_SEEK);
			m_errortype = SCE_PAF_ERROR_IMAGE_SEEK;
			m_failed = true;
			m_opened = false;
		}
	}
	if (!m_failed)
		m_opened = true;
}

void BmpContext::TraceOpen()
{
}

BmpContext::BmpContext(common::SharedPtr<File> file, int64_t offset, size_t size,
		       HeapAllocator *allocator)
{
	m_file = NULL;
	m_failed = false;
	m_opened = false;
	m_topdown = 0;
	m_position = 0;
	m_palette.colors = NULL;
	m_allocator = allocator;
	m_palette.count = 0;
	m_errortype = 0;
	m_size = 0;
	m_offset = 0;
	Open(file, offset, size);
}

bool BmpCodec::LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
			 const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			 DecodeCancelFunc cancelfunc, void *userdata)
{
	m_mutex.Lock();

	if (images == NULL || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	SetDecodeError(SCE_OK);
	if (images->GetMax() != 1) {
		m_mutex.Unlock();
		return false;
	}
	const ImageBuffer *image = images->GetBuffer(0);
	if (image->buff == NULL || image->size < m_pitch * attr->m_extent.h) {
		m_mutex.Unlock();
		return false;
	}
	if (attr->m_clutsize != 0) {
		if (cluts->GetMax() != 1) {
			m_mutex.Unlock();
			return false;
		}
		const ImageBuffer *clut = cluts->GetBuffer(0);
		if (clut->buff == NULL || clut->size < attr->m_clutsize * 4) {
			m_mutex.Unlock();
			return false;
		}
	}

	BmpContext context(file, offset, size, m_allocator);
	if (context.m_failed) {
		SetDecodeError(context.m_errortype);
		m_mutex.Unlock();
		return false;
	}
	if (!Decode(&context, attr, images, cluts, cancelfunc, userdata)) {
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

BmpContext::BmpContext(common::SharedPtr<Buffer> buf, HeapAllocator *allocator)
{
	m_file = NULL;
	m_failed = false;
	m_opened = false;
	m_topdown = 0;
	m_position = 0;
	m_palette.colors = NULL;
	m_allocator = allocator;
	m_palette.count = 0;
	m_errortype = 0;
	m_size = 0;
	m_offset = 0;
	Open(buf);
}

inline void BmpContext::Open(common::SharedPtr<Buffer> buf)
{
	m_addr = (uint8_t *)buf->GetAddr();
	size_t size = buf->GetSize();
	m_position = 0;
	m_read = MemoryRead;
	m_seek = MemorySeek;
	m_opened = true;
	m_size = size;
}

bool BmpCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
			 void *userdata)
{
	m_mutex.Lock();

	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || images == NULL || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	SetDecodeError(SCE_OK);
	if (images->GetMax() != 1) {
		m_mutex.Unlock();
		return false;
	}
	const ImageBuffer *image = images->GetBuffer(0);
	if (image->buff == NULL || image->size < m_pitch * attr->m_extent.h) {
		m_mutex.Unlock();
		return false;
	}
	if (attr->m_clutsize != 0) {
		if (cluts->GetMax() != 1) {
			m_mutex.Unlock();
			return false;
		}
		const ImageBuffer *clut = cluts->GetBuffer(0);
		if (clut->buff == NULL || clut->size < attr->m_clutsize * 4) {
			m_mutex.Unlock();
			return false;
		}
	}

	BmpContext context(buf, m_allocator);
	if (context.m_failed) {
		SetDecodeError(context.m_errortype);
		m_mutex.Unlock();
		return false;
	}
	if (!Decode(&context, attr, images, cluts, cancelfunc, userdata)) {
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

bool BmpCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	return false;
}

int32_t paf::image::SaveBmp(const char *filename, const void *addr, uint32_t pitch,
			    const ImageExtent &ext, ImageMode mode, uint32_t io_mode,
			    HeapAllocator *allocator)
{
	if (addr == NULL || pitch == 0 || ext.w <= 0 || ext.h <= 0 ||
	    (mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return 0;

	int32_t error;
	common::SharedPtr<File> file = LocalFile::Open(filename, 0x10602, io_mode, &error);
	if (file.get() == NULL)
		return 0;
	if (!file->IsOpened())
		return 1;

	int32_t linebytes = ext.w * 3;
	int32_t padding = 0;
	if (linebytes % 4 > 0)
		padding = 4 - linebytes % 4;
	int32_t imagesize = (linebytes + padding) * ext.h;
	size_t size = imagesize + sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);

	uint8_t *data;
	if (allocator != NULL)
		data = (uint8_t *)allocator->sce_paf_malloc(size);
	else
		data = (uint8_t *)sce_paf_malloc(size);
	if (data == NULL) {
		file->Close();
		return 0;
	}

	BmpFileHeader *fileheader = (BmpFileHeader *)data;
	fileheader->bfType = 'B' | 'M' << 8;
	fileheader->bfSize = size;
	fileheader->bfReserved1 = 0;
	fileheader->bfReserved2 = 0;
	fileheader->bfOffBits = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);

	BmpInfoHeader *infoheader = (BmpInfoHeader *)(data + sizeof(BmpFileHeader));
	infoheader->biSize = sizeof(BmpInfoHeader);
	infoheader->biWidth = ext.w;
	infoheader->biHeight = ext.h;
	infoheader->biPlanes = 1;
	infoheader->biBitCount = 24;
	infoheader->biCompression = BMP_COMPRESSION_RGB;
	infoheader->biSizeImage = imagesize;
	infoheader->biXPelsPerMeter = 0;
	infoheader->biYPelsPerMeter = 0;
	infoheader->biClrUsed = 0;
	infoheader->biClrImportant = 0;

	/* bottom up, B, G, R */
	uint8_t *dst = data + sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);
	const uint8_t *src = (const uint8_t *)addr;
	if (mode == ImageMode_RGBA8888) {
		for (int32_t y = ext.h - 1; y >= 0; y--) {
			for (int32_t x = 0; x < ext.w; x++) {
				dst[x * 3] = src[(y * pitch + x) * 4 + 2];
				dst[x * 3 + 1] = src[(y * pitch + x) * 4 + 1];
				dst[x * 3 + 2] = src[(y * pitch + x) * 4];
			}
			for (int32_t i = 0; i < padding; i++)
				dst[ext.w * 3 + i] = 0;
			dst += ext.w * 3 + padding;
		}
	} else {
		for (int32_t y = ext.h - 1; y >= 0; y--) {
			for (int32_t x = 0; x < ext.w; x++) {
				dst[x * 3] = src[(y * pitch + x) * 4];
				dst[x * 3 + 1] = src[(y * pitch + x) * 4 + 1];
				dst[x * 3 + 2] = src[(y * pitch + x) * 4 + 2];
			}
			for (int32_t i = 0; i < padding; i++)
				dst[ext.w * 3 + i] = 0;
			dst += ext.w * 3 + padding;
		}
	}

	if (file->Write(data, size) != size) {
		if (allocator != NULL)
			allocator->sce_paf_free(data);
		else
			sce_paf_free(data);
		file->Close();
		return 0;
	}
	if (allocator != NULL)
		allocator->sce_paf_free(data);
	else
		sce_paf_free(data);
	file->Close();
	return 1;
}

bool BmpCodec::ReadHeader(BmpContext *ctx, ImageAttribute *attr, ImageArray *images,
			  ImageArray *cluts)
{
	if (!ctx->ReadFileHeader()) {
		SetDecodeError(ctx->m_errortype);
		return false;
	}
	if (!ctx->ReadInfoHeader()) {
		SetDecodeError(ctx->m_errortype);
		return false;
	}

	uint16_t bitcount = ctx->m_infoheader.biBitCount;
	int32_t width = ctx->m_infoheader.biWidth;
	int32_t height = ctx->m_infoheader.biHeight;
	attr->m_extent.w = width;
	attr->m_extent.h = height;
	attr->m_clip.origin.x = 0;
	attr->m_clip.origin.y = 0;
	attr->m_clip.extent.w = width;
	attr->m_clip.extent.h = height;
	attr->m_format = ImageFormat_Bmp;
	attr->m_xtype = ImageXType_ConstantSize;
	attr->m_ytype = ImageYType_Sequence;
	attr->m_pitchalign = 1;
	attr->m_heightalign = 1;
	attr->m_order = ImageOrder_Linear;
	attr->m_mode = ImageMode_NONE;
	attr->m_clutmode = ImageMode_NONE;
	attr->m_clutsize = 0;

	switch (bitcount) {
	case 1:
	case 4:
		attr->m_mode = ImageMode_index4;
		attr->m_clutsize = 16;
		m_pitch = (width + 1) / 2;
		SetLoadType(LoadType_Image);
		break;
	case 8:
		attr->m_mode = ImageMode_index8;
		attr->m_clutsize = 256;
		m_pitch = width;
		SetLoadType(LoadType_Image);
		break;
	case 16: {
		ImageMode mode;
		if (ctx->m_infoheader.biCompression == BMP_COMPRESSION_BITFIELDS) {
			const BmpMasks *masks = &ctx->m_masks;
			if (masks->red.bits == 5 && masks->green.bits == 5 && masks->blue.bits == 5)
				mode = ImageMode_ABGR1555;
			else if (masks->red.bits == 5 && masks->green.bits == 6 &&
				 masks->blue.bits == 5)
				mode = ImageMode_XBGR0565;
			else if (masks->red.bits == 4 && masks->green.bits == 4 &&
				 masks->blue.bits == 4)
				mode = ImageMode_ABGR4444;
			else
				mode = ImageMode_RGBA8888;
		} else {
			mode = ImageMode_ABGR1555;
		}
		switch (mode) {
		case ImageMode_XBGR0565:
			attr->m_mode = ImageMode_XBGR0565;
			m_pitch = width * 2;
			SetLoadType(LoadType_Image);
			break;
		case ImageMode_ABGR1555:
			attr->m_mode = ImageMode_ABGR1555;
			m_pitch = width * 2;
			SetLoadType(LoadType_Image);
			break;
		case ImageMode_ABGR4444:
			attr->m_mode = ImageMode_ABGR4444;
			m_pitch = width * 2;
			SetLoadType(LoadType_Image);
			break;
		default:
			/* masks no 16-bit mode has decode to RGBA8888 */
			attr->m_mode = ImageMode_RGBA8888;
			m_pitch = width * 4;
			SetLoadType(LoadType_Line);
			break;
		}
		break;
	}
	case 24:
	case 32:
		attr->m_mode = ImageMode_RGBA8888;
		m_pitch = width * 4;
		SetLoadType(LoadType_Line);
		break;
	default:
		SetDecodeError(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	images->Init(1, 1);
	if (attr->m_clutsize != 0) {
		cluts->Init(1, 1);
		attr->m_clutmode = ImageMode_RGBA8888;
	}
	return true;
}

bool BmpCodec::GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			 ImageAttribute *attr, ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	m_allocator = images->GetHeapAllocator();
	SetDecodeError(SCE_OK);

	BmpContext context(file, offset, size, m_allocator);
	if (context.m_failed) {
		SetDecodeError(context.m_errortype);
		m_mutex.Unlock();
		return false;
	}
	if (!ReadHeader(&context, attr, images, cluts)) {
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

bool BmpCodec::GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	m_allocator = images->GetHeapAllocator();
	SetDecodeError(SCE_OK);

	BmpContext context(buf, m_allocator);
	if (context.m_failed) {
		m_mutex.Unlock();
		return false;
	}
	if (!ReadHeader(&context, attr, images, cluts)) {
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

bool BmpCodec::Open(common::SharedPtr<File> file, int64_t offset, size_t size,
		    const ImageAttribute *attr)
{
	m_mutex.Lock();
	SetDecodeError(SCE_OK);

	m_context = new BmpContext(file, offset, size, m_allocator);
	if (m_context == NULL || m_context->m_failed) {
		SetDecodeError(m_context->m_errortype);
		m_mutex.Unlock();
		return false;
	}
	if (!m_context->ReadFileHeader()) {
		SetDecodeError(m_context->m_errortype);
		m_mutex.Unlock();
		return false;
	}
	if (!m_context->ReadInfoHeader()) {
		SetDecodeError(m_context->m_errortype);
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

bool BmpCodec::Open(common::SharedPtr<Buffer> buf, const ImageAttribute *attr)
{
	m_mutex.Lock();
	SetDecodeError(SCE_OK);

	m_context = new BmpContext(buf, m_allocator);
	if (m_context == NULL || m_context->m_failed) {
		SetDecodeError(m_context->m_errortype);
		m_mutex.Unlock();
		return false;
	}
	if (!m_context->ReadFileHeader()) {
		SetDecodeError(m_context->m_errortype);
		m_mutex.Unlock();
		return false;
	}
	if (!m_context->ReadInfoHeader()) {
		SetDecodeError(m_context->m_errortype);
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

bool BmpCodec::Close()
{
	if (m_context != NULL) {
		delete m_context;
		m_context = NULL;
	}
	return true;
}

static bool ReadLine16(uint8_t *dst, const ImageRect *rect, BmpContext *ctx, void *buf,
		       size_t size, int32_t arg0, int32_t arg1, const BmpMasks *masks);
static bool ReadLine24(uint8_t *dst, const ImageRect *rect, BmpContext *ctx, void *buf,
		       size_t size, int32_t arg0, int32_t arg1, const BmpMasks *masks);
static bool ReadLine32(uint8_t *dst, const ImageRect *rect, BmpContext *ctx, void *buf,
		       size_t size, int32_t arg0, int32_t arg1, const BmpMasks *masks);

bool BmpCodec::BeginLines(const ImageAttribute *attr, const ImageArray *images, uint32_t *pitch,
			  bool *topdown)
{
	if (images->GetMax() != 1)
		return false;
	if (attr->m_mode != ImageMode_RGBA8888)
		return false;

	BmpContext *ctx = m_context;
	uint16_t bitcount = ctx->m_infoheader.biBitCount;
	uint32_t compression = ctx->m_infoheader.biCompression;
	size_t linesize;
	if (bitcount == 16 &&
	    (compression == BMP_COMPRESSION_RGB || compression == BMP_COMPRESSION_BITFIELDS)) {
		linesize = (ctx->m_infoheader.biWidth * 2 + 3) & ~3;
		m_linesize = linesize;
		m_linefunc = ReadLine16;
		m_masks = &ctx->m_masks;
	} else if (bitcount == 24 && compression == BMP_COMPRESSION_RGB) {
		linesize = (ctx->m_infoheader.biWidth * 3 + 3) & ~3;
		m_linesize = linesize;
		m_linefunc = ReadLine24;
	} else if (bitcount == 32 &&
		   (compression == BMP_COMPRESSION_RGB || compression == BMP_COMPRESSION_BITFIELDS)) {
		linesize = (ctx->m_infoheader.biWidth * 4 + 3) & ~3;
		m_linesize = linesize;
		m_linefunc = ReadLine32;
		m_masks = &ctx->m_masks;
	} else {
		return false;
	}

	if (m_allocator != NULL)
		m_linebuf = m_allocator->sce_paf_malloc(linesize);
	else
		m_linebuf = sce_paf_malloc(linesize);
	if (m_linebuf == NULL) {
		SetDecodeError(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}
	int32_t w = attr->m_subrect.extent.w;
	int32_t h = attr->m_subrect.extent.h;
	if (w <= 0 || h <= 0)
		return false;
	*pitch = w * 4;
	*topdown = m_context->m_topdown != 0;
	return true;
}

bool BmpCodec::EndLines()
{
	if (m_linebuf != NULL) {
		if (m_allocator != NULL)
			m_allocator->sce_paf_free(m_linebuf);
		else
			sce_paf_free(m_linebuf);
		m_linebuf = NULL;
	}
	return true;
}

bool BmpCodec::PrepareLine()
{
	return true;
}

bool BmpCodec::ReadLine(void *dst, const ImageRect *rect)
{
	return m_linefunc((uint8_t *)dst, rect, m_context, m_linebuf, m_linesize, m_linearg[0],
			  m_linearg[1], m_masks);
}

bool BmpCodec::SkipLine()
{
	if (m_context->Read(m_linebuf, m_linesize))
		return true;
	SetDecodeError(m_context->m_errortype);
	return false;
}

static bool ReadLine16(uint8_t *dst, const ImageRect *rect, BmpContext *ctx, void *buf,
		       size_t size, int32_t arg0, int32_t arg1, const BmpMasks *masks)
{
	if (!ctx->Read(buf, size))
		return false;

	const uint8_t *src = (const uint8_t *)buf + rect->origin.x * 2;
	for (int32_t x = 0; x < rect->extent.w; x++) {
		uint32_t pixel = src[0] | src[1] << 8;
		src += 2;
		dst[0] = ExpandChannel(pixel, &masks->red);
		dst[1] = ExpandChannel(pixel, &masks->green);
		dst[2] = ExpandChannel(pixel, &masks->blue);
		dst[3] = 0xff;
		dst += 4;
	}
	return true;
}

static bool ReadLine24(uint8_t *dst, const ImageRect *rect, BmpContext *ctx, void *buf,
		       size_t size, int32_t arg0, int32_t arg1, const BmpMasks *masks)
{
	if (!ctx->Read(buf, size))
		return false;

	const uint8_t *src = (const uint8_t *)buf + rect->origin.x * 3;
	for (int32_t x = 0; x < rect->extent.w; x++) {
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = 0xff;
		src += 3;
		dst += 4;
	}
	return true;
}

static bool ReadLine32(uint8_t *dst, const ImageRect *rect, BmpContext *ctx, void *buf,
		       size_t size, int32_t arg0, int32_t arg1, const BmpMasks *masks)
{
	if (!ctx->Read(buf, size))
		return false;

	const uint8_t *src = (const uint8_t *)buf + rect->origin.x * 4;
	for (int32_t x = 0; x < rect->extent.w; x++) {
		uint32_t pixel = (uint16_t)(src[0] | src[1] << 8);
		pixel |= src[2] << 16;
		pixel |= src[3] << 24;
		src += 4;
		dst[0] = ExpandChannel(pixel, &masks->red);
		dst[1] = ExpandChannel(pixel, &masks->green);
		dst[2] = ExpandChannel(pixel, &masks->blue);
		dst[3] = 0xff;
		dst += 4;
	}
	return true;
}

BmpCodec::BmpCodec() : m_mutex("ScePafImageBmpMutex", 0)
{
	m_pitch = 0;
	m_context = NULL;
	m_linebuf = NULL;
	m_linesize = 0;
	m_linearg[0] = 0;
	m_linearg[1] = 0;
	m_linefunc = NULL;
	m_masks = NULL;
	m_allocator = NULL;
}

BmpCodec::~BmpCodec()
{
}

static size_t FileRead(BmpContext *ctx, void *buf, size_t size)
{
	if (ctx == NULL || buf == NULL)
		return 0;
	return ctx->m_file->Read(buf, size);
}

static int64_t FileSeek(BmpContext *ctx, int64_t offset, int32_t whence)
{
	if (ctx == NULL)
		return 0;
	return ctx->m_file->Seek(offset, whence);
}

static size_t MemoryRead(BmpContext *ctx, void *buf, size_t size)
{
	if (ctx == NULL || buf == NULL)
		return 0;

	int64_t left = ctx->m_size - ctx->m_position;
	if (left <= size) {
		size = left;
		if (size == 0)
			return 0;
		sce_paf_memcpy(buf, ctx->m_addr, size);
	} else {
		sce_paf_memcpy(buf, ctx->m_addr, size);
	}
	ctx->m_addr += size;
	ctx->m_position += size;
	return size;
}

static int64_t MemorySeek(BmpContext *ctx, int64_t offset, int32_t whence)
{
	if (ctx == NULL)
		return 0;

	switch (whence) {
	case SCE_SEEK_SET:
		if (offset > (int32_t)ctx->m_size || offset < 0)
			return -1;
		ctx->m_addr = ctx->m_addr - ctx->m_position + offset;
		ctx->m_position = offset;
		return 0;
	case SCE_SEEK_CUR:
		if (ctx->m_position + offset > ctx->m_size)
			return -1;
		ctx->m_addr += offset;
		ctx->m_position += offset;
		return 0;
	case SCE_SEEK_END:
		if (offset > 0)
			return -1;
		ctx->m_addr = ctx->m_addr + (ctx->m_size - ctx->m_position) + offset;
		ctx->m_position = ctx->m_size + offset;
		return 0;
	}
	return -1;
}
