/*
 * A JPEG's segments and EXIF tags, the PlayStation MakerNote's text tags and an MPO file's 3D
 * pictures. Names ours; see exif_reader.h.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <std/string>
#include <std/string.h>
#include <std/stdlib.h>
#include <common/shared_ptr.h>
#include <file/file.h>
#include <memory/heap_allocator.h>
#include "../../../../app/common/src/photo_makernote/photo_makernote.h"
#include "../../../../app/common/src/mpo/mpo.h"
#include "exif_reader.h"

using namespace paf;

/* the size of one value of each EXIF field type */
static const uint32_t s_typeSize[EXIF_TYPE_COUNT] = {
	0,	/* none */
	1,	/* BYTE */
	1,	/* ASCII */
	2,	/* SHORT */
	4,	/* LONG */
	8,	/* RATIONAL */
	1,	/* SBYTE */
	1,	/* UNDEFINED */
	2,	/* SSHORT */
	4,	/* SLONG */
	8,	/* SRATIONAL */
	4,	/* FLOAT */
	8	/* DOUBLE */
};

static uint32_t ReadLE16(const void *p);
static uint32_t ReadLE32(const void *p);
static uint32_t ReadBE32(const void *p);
static void *MpoAlloc(size_t size, void *arg);
static int MpoFree(void *ptr, void *arg);

__attribute__((noinline)) uint32_t TiffContext::Read32(const void *p) const
{
	return read32(p);
}

ExifFile::~ExifFile()
{
}

int ExifFile::Open(const common::SharedPtr<File> &file)
{
	if (file.get() == NULL)
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	m_file = file;
	return (int)Seek(0, SCE_SEEK_SET);
}

__attribute__((noinline)) ExifReader::ExifReader()
{
}

ExifReader::~ExifReader()
{
}

static uint32_t ReadBE16(const void *p)
{
	const uint8_t *b = (const uint8_t *)p;

	return (b[0] << 8) | b[1];
}

int ExifReader::Parse(common::SharedPtr<File> file)
{
	uint8_t buf[2];
	bool eoi;
	bool sos;
	int ret;

	ret = m_file.Open(file);
	if (ret < 0)
		return ret;
	ret = m_file.Read(buf, 2);
	if (ret != 2)
		return ret < 0 ? ret : EXIF_ERROR_READ;
	if (ReadBE16(buf) != JPEG_MARKER_SOI)
		return EXIF_ERROR_FORMAT;
	sos = false;
	eoi = false;
	while (!m_file.IsEof() && !eoi && !sos) {
		JpegSegment segment = {0, 0, 0, 0, 0, 0};

		ret = m_file.Read(buf, 2);
		if (ret != 2)
			return ret < 0 ? ret : EXIF_ERROR_READ;
		segment.marker = ReadBE16(buf);
		segment.offset = m_file.Tell();
		if (segment.marker != JPEG_MARKER_SOI && segment.marker != JPEG_MARKER_EOI) {
			ret = m_file.Read(buf, 2);
			if (ret != 2)
				return ret < 0 ? ret : EXIF_ERROR_READ;
			segment.length = ReadBE16(buf);
			if (segment.length < 2)
				return EXIF_ERROR_FORMAT;
		}
		switch (segment.marker) {
		case JPEG_MARKER_SOI:
			break;
		case JPEG_MARKER_EOI:
			eoi = true;
			break;
		case JPEG_MARKER_SOS:
			sos = true;
			break;
		case JPEG_MARKER_APP1:
			ret = OnApp1(segment);
			break;
		case JPEG_MARKER_APP2:
			ret = OnApp2(segment);
			break;
		default:
			ret = OnSegment(segment);
			break;
		}
		if (ret < 0)
			break;
		ret = (int)m_file.Seek(segment.offset + segment.length, SCE_SEEK_SET);
		if (ret < 0)
			break;
		ret = 0;
	}
	return ret;
}

int ExifReader::ReadIfd(const TiffContext *ctx, const ExifIfd *ifd, ExifIfd *next)
{
	uint8_t buf[2];
	uint8_t *entries;
	int count;
	int ret = 0;
	int r;

	next->type = 0;
	next->offset = 0;
	if (m_file.Seek(ctx->base + ifd->offset, SCE_SEEK_SET) < 0)
		return EXIF_ERROR_READ;
	r = m_file.Read(buf, 2);
	if (r != 2)
		return r < 0 ? r : EXIF_ERROR_READ;
	count = ctx->read16(buf);
	m_file.Tell();
	entries = (uint8_t *)Alloc(count * 12);
	if (entries == NULL)
		return EXIF_ERROR_NO_MEMORY;
	r = m_file.Read(entries, count * 12);
	if (r != count * 12) {
		ret = r < 0 ? r : EXIF_ERROR_READ;
	} else {
		const uint8_t *p = entries;

		for (unsigned int i = 0; i < (uint16_t)count; i++, p += 12) {
			ExifEntry entry;

			entry.tag = ctx->read16(p);
			entry.type = ctx->read16(p + 2);
			entry.count = ctx->read32(p + 4);
			entry.value = ctx->read32(p + 8);
			if (entry.type != 0 && entry.type < EXIF_TYPE_COUNT)
				entry.size = s_typeSize[entry.type] * entry.count;
			else
				entry.size = 0;
			sce_paf_memcpy(entry.raw, p + 8, sizeof(entry.raw));
			if (entry.type == 0 || entry.type >= EXIF_TYPE_COUNT) {
				ret = EXIF_ERROR_DATA;
				break;
			}
			if (entry.size == 0 || entry.size > ctx->size) {
				ret = EXIF_ERROR_DATA;
				break;
			}
			ret = OnTag(ctx, ifd, &entry);
			if (ret < 0)
				break;
		}
	}
	Free(entries);
	return ret;
}

int ExifReader::ReadHeader(TiffContext *ctx, uint32_t *ifdOffset)
{
	uint8_t header[8];
	int64_t pos;
	int r;

	*ifdOffset = 0;
	pos = m_file.Seek(ctx->base, SCE_SEEK_SET);
	if (pos < 0)
		return (int)pos;
	r = m_file.Read(header, sizeof(header));
	if (r != sizeof(header))
		return r < 0 ? r : EXIF_ERROR_READ;
	if (sce_paf_memcmp(header, "II", 2) == 0) {
		ctx->order = 0;
		ctx->read16 = ReadLE16;
		ctx->read32 = ReadLE32;
	} else if (sce_paf_memcmp(header, "MM", 2) == 0) {
		ctx->order = 1;
		ctx->read16 = ReadBE16;
		ctx->read32 = ReadBE32;
	} else {
		return EXIF_ERROR_DATA;
	}
	if (ctx->read16(header + 2) != 42)
		return EXIF_ERROR_DATA;
	*ifdOffset = ctx->read32(header + 4);
	return 0;
}

int ExifReader::OnApp1(const JpegSegment &segment)
{
	char exif[] = "Exif";
	char id[7] = {0};
	TiffContext ctx;
	uint32_t ifdOffset;
	ExifIfd next;
	unsigned int n;
	int64_t pos;
	int ret;

	if (segment.length <= 8)
		return 0;
	pos = m_file.Seek(segment.offset + 2, SCE_SEEK_SET);
	if (pos < 0)
		return (int)pos;
	ret = m_file.Read(id, 6);
	if (ret != 6)
		return ret < 0 ? ret : EXIF_ERROR_READ;
	if (sce_paf_strncmp(id, exif, 5) != 0)
		return EXIF_ERROR_FORMAT;
	ctx.base = segment.offset + 8;
	ctx.size = segment.length;
	ctx.order = 0;
	ctx.read16 = NULL;
	ctx.read32 = NULL;
	ifdOffset = 0;
	ret = ReadHeader(&ctx, &ifdOffset);
	if (ret < 0)
		return ret;
	next.type = EXIF_IFD_PRIMARY;
	next.offset = ifdOffset;
	n = 0;
	do {
		ExifIfd ifd = next;

		ret = ReadIfd(&ctx, &ifd, &next);
		if (ret < 0)
			break;
		if (++n > 20) {
			ret = EXIF_ERROR_DATA;
			break;
		}
	} while (next.offset != 0);
	return ret;
}

int ExifReader::OnTag(const TiffContext *ctx, const ExifIfd *ifd, const ExifEntry *entry)
{
	switch (ifd->type) {
	case EXIF_IFD_PRIMARY:
		return OnPrimaryTag(ctx, ifd, entry);
	case EXIF_IFD_EXIF:
		return OnExifTag(ctx, ifd, entry);
	case EXIF_IFD_INTEROPERABILITY:
		return OnInteropTag(ctx, ifd, entry);
	}
	return 0;
}

int ExifReader::ReadValue(const TiffContext *ctx, const ExifEntry *entry, void *buf, size_t size)
{
	int64_t pos;
	int r;

	pos = m_file.Seek(ctx->base + entry->value, SCE_SEEK_SET);
	if (pos < 0)
		return (int)pos;
	r = m_file.Read(buf, size);
	if (r != (int)size)
		return r < 0 ? r : EXIF_ERROR_READ;
	return 0;
}

static uint32_t ReadLE16(const void *p)
{
	const uint8_t *b = (const uint8_t *)p;

	return b[0] | (b[1] << 8);
}

static uint32_t ReadLE32(const void *p)
{
	const uint8_t *b = (const uint8_t *)p;

	return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

static uint32_t ReadBE32(const void *p)
{
	const uint8_t *b = (const uint8_t *)p;

	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

MpoInfo::MpoInfo(memory::HeapAllocator *heap) : m_heap(heap)
{
}

bool MpoInfo::IsSonyMake(MpoHandle *mpo)
{
	MpoTagInfo info;
	size_t size;
	char *make;
	bool sony;

	if (MpoSelectImage(mpo, 0) != 0)
		return false;
	if (MpoSelectSegment(mpo, JPEG_MARKER_APP1) < 0)
		return false;
	if (MpoSelectIfd(mpo, 0) != 0)
		return false;
	if (MpoGetTag(mpo, EXIF_TAG_MAKE, &info, NULL, 0) < 0)
		return false;
	size = info.size;
	make = (char *)sce_paf_malloc(size);
	if (make == NULL)
		return false;
	sony = false;
	if (MpoGetTag(mpo, EXIF_TAG_MAKE, &info, make, size) >= 0) {
		make[size - 1] = 0;
		sony = sce_paf_strncmp("SONY", make, sce_paf_strlen("SONY")) == 0;
	}
	sce_paf_free(make);
	return sony;
}

int MpoInfo::Get(const string &path, MpoInfoResult *result)
{
	MpoHandle *mpo;
	MpoCallbacks callbacks;
	int ret;

	callbacks.reader = NULL;
	callbacks.readerArg = NULL;
	callbacks.alloc = MpoAlloc;
	callbacks.allocArg = m_heap;
	callbacks.free = MpoFree;
	callbacks.freeArg = m_heap;
	ret = MpoOpen(&mpo, &callbacks, path.c_str());
	if (ret == 0) {
		int index = MpoGetRepresentativeImage(mpo);

		if (index < 0)
			index = 0;
		int width = MpoGetImageWidth(mpo, index);

		if (width >= 0) {
			int height = MpoGetImageHeight(mpo, index);

			if (height >= 0) {
				int count = MpoGetImageCount(mpo);
				int kind;

				if (count >= 0) {
					int disparity = 0;

					for (int i = 0; i < count; i++) {
						if (MpoGetImageType(mpo, i) == MPO_TYPE_DISPARITY)
							disparity++;
					}
					int type = MpoGetImageType(mpo, index);

					if (type == MPO_TYPE_PANORAMA) {
						kind = MPO_3D_PANORAMA;
					} else if (type == MPO_TYPE_MULTI_ANGLE) {
						kind = MPO_3D_MULTI_ANGLE;
					} else if (type == MPO_TYPE_DISPARITY) {
						if (IsSonyMake(mpo)) {
							if (disparity == 2)
								kind = MPO_3D_SONY_STEREO;
							else if (disparity > 2)
								kind = MPO_3D_SONY_MULTI;
							else
								kind = MPO_3D_NONE;
						} else {
							if (disparity == 2)
								kind = MPO_3D_STEREO;
							else if (disparity > 2)
								kind = MPO_3D_MULTI;
							else
								kind = MPO_3D_NONE;
						}
					} else {
						kind = MPO_3D_NONE;
					}
				} else {
					kind = MPO_3D_NONE;
				}
				result->kind = kind;
				result->width = width;
				result->height = height;
			} else {
				ret = height;
			}
		} else {
			ret = width;
		}
		MpoClose(mpo);
	}
	return ret;
}

static void *MpoAlloc(size_t size, void *arg)
{
	memory::HeapAllocator *heap = (memory::HeapAllocator *)arg;

	if (heap != NULL)
		return heap->sce_paf_malloc(size);
	return sce_paf_malloc(size);
}

static int MpoFree(void *ptr, void *arg)
{
	memory::HeapAllocator *heap = (memory::HeapAllocator *)arg;

	if (heap != NULL)
		heap->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
	return 0;
}

MakerNoteReader::MakerNoteReader(memory::HeapAllocator *heap) : m_heap(heap)
{
}

MakerNoteReader::~MakerNoteReader()
{
}

int MakerNoteReader::GetString(int index, string &out)
{
	if (index == 0)
		out = m_gdsc;
	else if (index == 1)
		out = m_gpnm;
	else if (index == 2)
		out = m_gcid;
	else
		return -1;
	return 0;
}

void *MakerNoteReader::Alloc(size_t size)
{
	if (m_heap != NULL)
		return m_heap->sce_paf_malloc(size);
	return NULL;
}

void MakerNoteReader::Free(void *ptr)
{
	if (m_heap != NULL)
		m_heap->sce_paf_free(ptr);
}

int MakerNoteReader::OnPrimaryTag(const TiffContext *ctx, const ExifIfd *ifd,
				  const ExifEntry *entry)
{
	ExifIfd exif;
	ExifIfd next;

	if (entry->tag != EXIF_TAG_EXIF_IFD)
		return 0;
	if (entry->type != EXIF_TYPE_LONG || entry->count != 1)
		return EXIF_ERROR_DATA;
	exif.type = EXIF_IFD_EXIF;
	exif.offset = ctx->Read32(entry->raw);
	return ReadIfd(ctx, &exif, &next);
}

int MakerNoteReader::ReadMakerNote(const TiffContext *ctx, const ExifEntry *entry)
{
	PhotoMakerNote *note = new PhotoMakerNote();
	void *buf;
	int ret;

	if (note != NULL) {
		buf = Alloc(entry->size);
		if (buf != NULL) {
			ret = ReadValue(ctx, entry, buf, entry->size);
			if (ret >= 0) {
				ret = note->Parse((const uint8_t *)buf, entry->size);
				if (ret >= 0) {
					PhotoMakerNoteText text;

					if (note->GetTag(PHOTO_MAKERNOTE_TAG_GDSC, &text, 1) == 0 &&
					    text.length < 0x10000)
						m_gdsc.assign(text.text, text.length);
					if (note->GetTag(PHOTO_MAKERNOTE_TAG_GPNM, &text, 1) == 0 &&
					    text.length < 0x10000)
						m_gpnm.assign(text.text, text.length);
					if (note->GetTag(PHOTO_MAKERNOTE_TAG_GCID, &text, 1) == 0 &&
					    text.length < 0x10000)
						m_gcid.assign(text.text, text.length);
					ret = 0;
				}
			}
		} else {
			ret = EXIF_ERROR_NO_MEMORY;
		}
		if (buf != NULL)
			Free(buf);
	} else {
		ret = EXIF_ERROR_NO_MEMORY;
	}
	if (note != NULL)
		delete note;
	return ret;
}

int MakerNoteReader::OnExifTag(const TiffContext *ctx, const ExifIfd *ifd, const ExifEntry *entry)
{
	if (entry->tag == EXIF_TAG_MAKER_NOTE)
		return ReadMakerNote(ctx, entry);
	return 0;
}
