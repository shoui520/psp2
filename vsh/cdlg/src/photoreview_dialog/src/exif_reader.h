/*
 * A JPEG's segments and its EXIF (APP1) tags, read from a paf::File: ExifReader hands each
 * segment and each IFD entry to a virtual method, which a subclass overrides for what it wants;
 * MakerNoteReader reads the PlayStation MakerNote's text tags. MpoInfo says how the pictures of
 * an MPO file make a 3D image. No binary names any of this; the classes, their methods and this
 * file's name are ours.
 */
#ifndef PHOTOREVIEW_DIALOG_EXIF_READER_H
#define PHOTOREVIEW_DIALOG_EXIF_READER_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/file.h>
#include <memory/heap_allocator.h>
#include <kernel/iofilemgr.h>
#include "../../../../app/common/src/mpo/mpo.h"

/* what the reader returns besides paf's file errors */
enum {
	EXIF_ERROR_READ = -1,		/* a short read, or a seek that failed */
	EXIF_ERROR_FORMAT = -2,		/* not a JPEG, or not EXIF */
	EXIF_ERROR_DATA = -3,		/* a broken TIFF header or IFD */
	EXIF_ERROR_NO_MEMORY = -4
};

/* the JPEG markers the reader knows */
enum {
	JPEG_MARKER_SOI = 0xFFD8,
	JPEG_MARKER_EOI = 0xFFD9,
	JPEG_MARKER_SOS = 0xFFDA,
	JPEG_MARKER_APP1 = 0xFFE1,
	JPEG_MARKER_APP2 = 0xFFE2
};

/* the IFDs, as ExifIfd::type */
enum {
	EXIF_IFD_PRIMARY = 1,
	EXIF_IFD_EXIF = 2,
	EXIF_IFD_INTEROPERABILITY = 4
};

/* the tags the readers look for */
enum {
	EXIF_TAG_MAKE = 271,
	EXIF_TAG_EXIF_IFD = 34665,
	EXIF_TAG_MAKER_NOTE = 37500
};

/* an EXIF field type's size in bytes, by type (1 to 12) */
#define EXIF_TYPE_LONG		4
#define EXIF_TYPE_COUNT		13

/* a file read through its SharedPtr: the errors are paf's while there is none */
class ExifFile {
public:
	~ExifFile();

	/* file, at its start */
	int Open(const paf::common::SharedPtr<paf::File> &file);

	int Read(void *buf, size_t size)
	{
		if (m_file.get() == NULL)
			return SCE_PAF_ERROR_FILE_NOT_OPENED;
		return m_file->Read(buf, size);
	}

	int64_t Seek(int64_t offset, int whence)
	{
		if (m_file.get() == NULL)
			return SCE_PAF_ERROR_FILE_NOT_OPENED;
		return m_file->Seek(offset, whence);
	}

	int64_t Tell()
	{
		return Seek(0, SCE_SEEK_CUR);
	}

	bool IsEof()
	{
		if (m_file.get() == NULL)
			return true;
		return m_file->IsEof();
	}

	paf::common::SharedPtr<paf::File> m_file;
};

/* a JPEG segment: its marker, where its data starts (after the marker) and its length */
class JpegSegment {
public:
	uint16_t marker;
	uint16_t reserved0;
	uint32_t reserved1;
	int64_t offset;
	uint32_t length;
	uint32_t reserved2;
};

/* a TIFF header's byte order, and where the TIFF data starts and how long it is */
class TiffContext {
public:
	uint32_t Read32(const void *p) const;

	int64_t base;
	uint32_t size;
	/* 0: little endian ("II"), 1: big endian ("MM") */
	uint32_t order;
	uint32_t (*read16)(const void *p);
	uint32_t (*read32)(const void *p);
};

/* an IFD: which one, and its offset in the TIFF data */
class ExifIfd {
public:
	uint32_t type;
	uint32_t offset;
};

/* an IFD entry, its value or value offset in byte order */
class ExifEntry {
public:
	uint16_t tag;
	uint16_t type;
	uint32_t count;
	uint32_t value;
	/* the value's size in bytes */
	uint32_t size;
	uint8_t raw[4];
};

class ExifReader {
public:
	ExifReader();
	virtual ~ExifReader();

	virtual void *Alloc(size_t size) = 0;
	virtual void Free(void *ptr) = 0;

	/* each segment that is neither APP1 nor APP2 */
	virtual int OnSegment(const JpegSegment &segment)
	{
		return 0;
	}

	/* reads the EXIF IFDs */
	virtual int OnApp1(const JpegSegment &segment);

	virtual int OnApp2(const JpegSegment &segment)
	{
		return 0;
	}

	/* each IFD entry, to the method for its IFD */
	virtual int OnTag(const TiffContext *ctx, const ExifIfd *ifd, const ExifEntry *entry);

	virtual int OnPrimaryTag(const TiffContext *ctx, const ExifIfd *ifd,
				 const ExifEntry *entry)
	{
		return 0;
	}

	virtual int OnExifTag(const TiffContext *ctx, const ExifIfd *ifd, const ExifEntry *entry)
	{
		return 0;
	}

	virtual int OnInteropTag(const TiffContext *ctx, const ExifIfd *ifd,
				 const ExifEntry *entry)
	{
		return 0;
	}

	/* the JPEG's segments up to its image data */
	int Parse(paf::common::SharedPtr<paf::File> file);
	/* the IFD's entries; next is cleared */
	int ReadIfd(const TiffContext *ctx, const ExifIfd *ifd, ExifIfd *next);
	/* the TIFF header at ctx->base: its byte order, and the offset of its first IFD */
	int ReadHeader(TiffContext *ctx, uint32_t *ifdOffset);
	/* an entry's value, from its offset */
	int ReadValue(const TiffContext *ctx, const ExifEntry *entry, void *buf, size_t size);

	ExifFile m_file;
};

/* reads the text tags of the PlayStation MakerNote */
class MakerNoteReader : public ExifReader {
public:
	MakerNoteReader(paf::memory::HeapAllocator *heap);
	virtual ~MakerNoteReader();

	virtual void *Alloc(size_t size);
	virtual void Free(void *ptr);
	virtual int OnPrimaryTag(const TiffContext *ctx, const ExifIfd *ifd,
				 const ExifEntry *entry);
	virtual int OnExifTag(const TiffContext *ctx, const ExifIfd *ifd, const ExifEntry *entry);

	/* the index-th tag (GDSC, GPNM, GCID): 0, or -1 */
	int GetString(int index, paf::string &out);
	int ReadMakerNote(const TiffContext *ctx, const ExifEntry *entry);

	paf::memory::HeapAllocator *m_heap;
	paf::string m_gdsc;
	paf::string m_gpnm;
	paf::string m_gcid;
};

/* what an MPO file's pictures make */
enum {
	MPO_3D_NONE = 0,
	MPO_3D_SONY_STEREO = 1,		/* a Sony camera's two disparity pictures */
	MPO_3D_SONY_MULTI = 2,		/* a Sony camera's more than two */
	MPO_3D_STEREO = 3,		/* another camera's two */
	MPO_3D_PANORAMA = 4,
	MPO_3D_MULTI_ANGLE = 5,
	MPO_3D_MULTI = 6		/* another camera's more than two */
};

class MpoInfoResult {
public:
	int32_t kind;
	int32_t width;
	int32_t height;
};

class MpoInfo {
public:
	MpoInfo(paf::memory::HeapAllocator *heap);

	/* the first picture's Make is SONY */
	bool IsSonyMake(MpoHandle *mpo);
	int Get(const paf::string &path, MpoInfoResult *result);

	paf::memory::HeapAllocator *m_heap;
};

#endif
