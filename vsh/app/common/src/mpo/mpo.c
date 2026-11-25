/*
 * MPO (CIPA DC-007, Multi-Picture Format) and JPEG/EXIF/MPF segment reader (mpo.h).
 * Compiled by the photo apps, the image dialogs and jpeg_promoter. Names ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include "mpo.h"

#define MPO_ERROR_READ			((int)0x80000001)
#define MPO_ERROR_INTERNAL		((int)0x80000002)
#define MPO_ERROR_ALLOC			((int)0x80000003)
/* MPO_ERROR_INVALID_ARGUMENT = 0x80000004 (in mpo.h) */
#define MPO_ERROR_OUT_OF_RANGE		((int)0x80000005)
#define MPO_ERROR_OPEN			((int)0x80000006)
#define MPO_ERROR_CLOSE			((int)0x80000007)
#define MPO_ERROR_NOT_JPEG		((int)0x80000008)
#define MPO_ERROR_SEGMENT_LIMIT		((int)0x80000009)
#define MPO_ERROR_TIFF_HEADER		((int)0x8000000A)
#define MPO_ERROR_NO_IFD		((int)0x8000000B)
#define MPO_ERROR_NO_MP_INFO		((int)0x8000000D)
#define MPO_ERROR_NO_SEGMENT		((int)0x8000000E)
#define MPO_ERROR_TAG_NOT_FOUND		((int)0x80000010)
#define MPO_ERROR_TAG_UNKNOWN		((int)0x80000011)
#define MPO_ERROR_TAG_TYPE		((int)0x80000012)
#define MPO_ERROR_TAG_COUNT		((int)0x80000013)
#define MPO_ERROR_TAG_BOUNDS		((int)0x80000014)

#define MPO_MAX_SEGMENTS		64
#define MPO_MAX_IFDS			4
#define MPO_READ_BUF_SIZE		0x10004
#define MPO_SEG_BUF_SIZE		0x10000

#define MPO_MARKER_SOF0			0xFFC0
#define MPO_MARKER_SOF2			0xFFC2
#define MPO_MARKER_RST0			0xFFD0
#define MPO_MARKER_EOI			0xFFD9
#define MPO_MARKER_SOS			0xFFDA
#define MPO_MARKER_APP1			0xFFE1
#define MPO_MARKER_APP2			0xFFE2

#define MPO_TYPE_SHORT_OR_LONG		11

#define MPO_IFD_EXIF_PRIMARY		0x01010000
#define MPO_IFD_EXIF_THUMB		0x01020000
#define MPO_IFD_EXIF_SUB		0x01010100
#define MPO_IFD_MAKER_SONY_1		0x01010101
#define MPO_IFD_MAKER_SONY_2		0x01010102
#define MPO_IFD_MPF_INDEX		0x02010000
#define MPO_IFD_MPF_ATTR		0x02020000

#define MPO_TAG_MAKERNOTE		0x927C
#define MPO_TAG_MP_NUM_IMAGES		0xB001
#define MPO_TAG_MP_ENTRY		0xB002

typedef struct MpoTagRule {
	uint32_t ifdId;
	uint32_t tag;
	uint32_t type;
	uint32_t count;
} MpoTagRule;

typedef struct MpoTagRuleTable {
	int count;
	const MpoTagRule *rules;
} MpoTagRuleTable;

typedef struct MpoTagEntry {
	uint16_t tag;
	uint16_t type;
	int count;
	union {
		uint8_t bytes[4];
		uint16_t shorts[2];
		uint32_t u32;
	} value;
} MpoTagEntry;

typedef struct MpoImageLoc {
	int size;
	int offset;
} MpoImageLoc;

typedef struct MpoMpEntry {
	uint32_t attr;
	uint32_t size;
	uint32_t offset;
	uint16_t dep1;
	uint16_t dep2;
} MpoMpEntry;

typedef struct MpoSegment {
	int marker;
	int fileOffset;
	int length;
	uint8_t *data;
	int headerOffset;
	uint8_t *idPtr;
	int idSize;
	uint8_t *tiffBase;
	int tiffSize;
	int byteOrder;
	uint8_t *tiffStart;
	int ifdCount;
	uint8_t *ifdPtrs[MPO_MAX_IFDS];
	uint8_t *curBase;
	uint8_t *curIfd;
	uint8_t hasTiff;
} MpoSegment;

struct MpoHandle {
	MpoCallbacks callbacks;
	int useReader;
	SceUID fd;
	uint8_t *readBuf;
	int imageCount;
	int currentImage;
	int segmentCount;
	MpoSegment segments[MPO_MAX_SEGMENTS];
	MpoSegment *selectedSeg;
	int searchMarker;
	int searchIndex;
	int selectedIfd;
	int subIfdType;
	int subIfdLevel;
	int width;
	int height;
	uint8_t hasExif;
	uint8_t hasMpf;
	MpoImageLoc *images;
	MpoMpEntry *entries;
	uint8_t isMpo;
};

static const MpoTagRule s_rootTagRules[18] = {
	{ MPO_IFD_EXIF_PRIMARY, 0x0100, 11,  1 },
	{ MPO_IFD_EXIF_PRIMARY, 0x0101, 11,  1 },
	{ MPO_IFD_EXIF_PRIMARY, 0x010F,  2,  0 },
	{ MPO_IFD_EXIF_PRIMARY, 0x0110,  2,  0 },
	{ MPO_IFD_EXIF_PRIMARY, 0x0112,  3,  1 },
	{ MPO_IFD_EXIF_PRIMARY, 0x0132,  2, 20 },
	{ MPO_IFD_EXIF_PRIMARY, 0x8769,  4,  1 },
	{ MPO_IFD_MPF_INDEX,    0xB000,  7,  4 },
	{ MPO_IFD_MPF_INDEX,    0xB001,  4,  1 },
	{ MPO_IFD_MPF_INDEX,    0xB002,  7,  0 },
	{ MPO_IFD_MPF_INDEX,    0xB003,  7,  0 },
	{ MPO_IFD_MPF_INDEX,    0xB004,  4,  1 },
	{ MPO_IFD_MPF_ATTR,     0xB000,  7,  4 },
	{ MPO_IFD_MPF_ATTR,     0xB101,  4,  1 },
	{ MPO_IFD_MPF_ATTR,     0xB204,  4,  1 },
	{ MPO_IFD_MPF_ATTR,     0xB205, 10,  1 },
	{ MPO_IFD_MPF_ATTR,     0xB206,  5,  1 },
	{ MPO_IFD_MPF_ATTR,     0xB207, 10,  1 },
};

static const MpoTagRule s_exifTagRules[21] = {
	{ MPO_IFD_EXIF_SUB, 0x829A,  5,  1 },
	{ MPO_IFD_EXIF_SUB, 0x829D,  5,  1 },
	{ MPO_IFD_EXIF_SUB, 0x8822,  3,  1 },
	{ MPO_IFD_EXIF_SUB, 0x8827,  3,  0 },
	{ MPO_IFD_EXIF_SUB, 0x9000,  7,  4 },
	{ MPO_IFD_EXIF_SUB, 0x9003,  2, 20 },
	{ MPO_IFD_EXIF_SUB, 0x9201, 10,  1 },
	{ MPO_IFD_EXIF_SUB, 0x9202,  5,  1 },
	{ MPO_IFD_EXIF_SUB, 0x9204, 10,  1 },
	{ MPO_IFD_EXIF_SUB, 0x9205,  5,  1 },
	{ MPO_IFD_EXIF_SUB, 0x9207,  3,  1 },
	{ MPO_IFD_EXIF_SUB, 0x9208,  3,  1 },
	{ MPO_IFD_EXIF_SUB, 0x9209,  3,  1 },
	{ MPO_IFD_EXIF_SUB, 0x920A,  5,  1 },
	{ MPO_IFD_EXIF_SUB, 0x927C,  7,  0 },
	{ MPO_IFD_EXIF_SUB, 0xA001,  3,  1 },
	{ MPO_IFD_EXIF_SUB, 0xA002, 11,  1 },
	{ MPO_IFD_EXIF_SUB, 0xA003, 11,  1 },
	{ MPO_IFD_EXIF_SUB, 0xA402,  3,  1 },
	{ MPO_IFD_EXIF_SUB, 0xA403,  3,  1 },
	{ MPO_IFD_EXIF_SUB, 0xA405,  3,  1 },
};

static const MpoTagRule s_makerTagRules[2] = {
	{ MPO_IFD_MAKER_SONY_1, 0x1003, 4, 16 },
	{ MPO_IFD_MAKER_SONY_2, 0x1003, 4, 16 },
};

static MpoTagRuleTable s_tagRuleTables[3] = {
	{ 18, s_rootTagRules },
	{ 21, s_exifTagRules },
	{  2, s_makerTagRules },
};

typedef int (*MpoReaderFunc)(int64_t offset, int64_t size, void *buf, void *arg);

static int mpoSelectSegmentByIndex(MpoHandle *mpo, int index);
static int mpoFindSegment(MpoHandle *mpo, int marker);
static int mpoValidateTag(MpoHandle *mpo, const MpoTagEntry *entry);
static int mpoReadTagEntry(MpoHandle *mpo, int index, MpoTagEntry *entry, void *buf, int size);
static int mpoFindAndReadTag(MpoHandle *mpo, uint16_t tag, MpoTagEntry *entry, void *buf, int size);
static int mpoSelectIfdInternal(MpoHandle *mpo, int ifd);
static int mpoGetImageOffsetInternal(MpoHandle *mpo, int index);
static int mpoGetImageTypeInternal(MpoHandle *mpo, int index);
static int mpoGetImageCountInternal(MpoHandle *mpo);
static int mpoGetImageSizeInternal(MpoHandle *mpo, int index);
static int mpoIsRepresentativeInternal(MpoHandle *mpo, int index);
static int mpoParseSegmentTiff(MpoHandle *mpo, const uint8_t *buf);
static int mpoScanSegments(MpoHandle *mpo);
static int mpoFindMpfSegment(MpoHandle *mpo);
static int mpoParseMpIndex(MpoHandle *mpo);
static int mpoAdjustMpfIfdCount(MpoHandle *mpo, int index);
static int mpoOpenFirstImage(MpoHandle *mpo);
static int mpoOpenImage(MpoHandle *mpo, int index);
static uint32_t mpoReadU16(const uint8_t *p, int byteOrder);
static uint32_t mpoReadU32(const uint8_t *p, int byteOrder);
static SceUID mpoFileOpen(const char *path, int16_t flags);
static int mpoFileClose(SceUID fd);
static int mpoFileSeek(SceUID fd, int offset, int whence);
static void *mpoAlloc(MpoHandle *mpo, int size);
static int mpoFree(MpoHandle *mpo, void *ptr);
static int mpoRead(MpoHandle *mpo, int offset, int size, void *buf);

int MpoClose(MpoHandle *mpo)
{
	int i;

	if (mpo == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if (mpo->images != NULL && mpoFree(mpo, mpo->images) < 0)
		return MPO_ERROR_ALLOC;
	if (mpo->entries != NULL && mpoFree(mpo, mpo->entries) < 0)
		return MPO_ERROR_ALLOC;
	if (mpo->readBuf != NULL && mpoFree(mpo, mpo->readBuf) < 0)
		return MPO_ERROR_ALLOC;
	for (i = 0; i < mpo->segmentCount; i++) {
		if (mpo->segments[i].data != NULL && mpoFree(mpo, mpo->segments[i].data) < 0)
			return MPO_ERROR_ALLOC;
	}
	if (mpo->useReader == 0 && mpo->fd >= 0 && mpoFileClose(mpo->fd) < 0)
		return MPO_ERROR_CLOSE;
	if (mpoFree(mpo, mpo) < 0)
		return MPO_ERROR_ALLOC;
	return 0;
}

static int mpoMapScanError(int err)
{
	switch (err) {
	case -2:
		return MPO_ERROR_READ;
	case -4:
		return MPO_ERROR_ALLOC;
	case -5:
		return MPO_ERROR_NOT_JPEG;
	case -6:
		return MPO_ERROR_SEGMENT_LIMIT;
	case -7:
		return MPO_ERROR_TIFF_HEADER;
	case -10:
		return MPO_ERROR_NO_MP_INFO;
	default:
		return MPO_ERROR_INTERNAL;
	}
}

int MpoOpen(MpoHandle **mpo, const MpoCallbacks *callbacks, const char *path)
{
	MpoHandle *h;
	int ret;

	if (mpo == NULL || callbacks == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if (callbacks->alloc == NULL || callbacks->free == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if (path == NULL && callbacks->reader == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;

	h = (MpoHandle *)callbacks->alloc(sizeof(MpoHandle), callbacks->allocArg);
	*mpo = h;
	if (h == NULL)
		return MPO_ERROR_ALLOC;
	sce_paf_memset(h, 0, sizeof(MpoHandle));
	h->callbacks = *callbacks;

	if (path != NULL) {
		h->fd = mpoFileOpen(path, 1);
		if (h->fd < 0) {
			MpoClose(*mpo);
			return MPO_ERROR_OPEN;
		}
		h->useReader = 0;
	} else {
		h->useReader = 1;
	}

	h->readBuf = (uint8_t *)mpoAlloc(h, MPO_READ_BUF_SIZE);
	if (h->readBuf == NULL) {
		MpoClose(*mpo);
		return MPO_ERROR_ALLOC;
	}

	ret = mpoOpenFirstImage(h);
	if (ret < 0) {
		int err = mpoMapScanError(ret);

		MpoClose(*mpo);
		return err;
	}
	return 0;
}

int MpoGetImageCount(MpoHandle *mpo)
{
	if (mpo == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	return mpoGetImageCountInternal(mpo);
}

int MpoGetImageWidth(MpoHandle *mpo, int index)
{
	int ret;

	if (mpo == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if (index < 0 || index >= mpoGetImageCountInternal(mpo))
		return MPO_ERROR_OUT_OF_RANGE;
	ret = mpoGetImageOffsetInternal(mpo, index);
	if (ret < 0)
		return MPO_ERROR_NO_MP_INFO;
	return ret;
}

int MpoGetImageHeight(MpoHandle *mpo, int index)
{
	int ret;

	if (mpo == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if (index < 0 || index >= mpoGetImageCountInternal(mpo))
		return MPO_ERROR_OUT_OF_RANGE;
	ret = mpoGetImageSizeInternal(mpo, index);
	if (ret < 0)
		return MPO_ERROR_NO_MP_INFO;
	return ret;
}

int MpoGetImageType(MpoHandle *mpo, int index)
{
	int ret;

	if (mpo == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if (index < 0 || index >= mpoGetImageCountInternal(mpo))
		return MPO_ERROR_OUT_OF_RANGE;
	ret = mpoGetImageTypeInternal(mpo, index);
	if (ret < 0)
		return MPO_ERROR_NO_MP_INFO;
	return ret;
}

int MpoGetRepresentativeImage(MpoHandle *mpo)
{
	int count;
	int i;

	if (mpo == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	count = mpoGetImageCountInternal(mpo);
	for (i = 0; i < count; i++) {
		int rep = mpoIsRepresentativeInternal(mpo, i);

		if (rep < 0)
			return MPO_ERROR_NO_MP_INFO;
		if (rep == 1)
			return i < 0 ? MPO_ERROR_NO_MP_INFO : i;
	}
	return MPO_ERROR_NO_MP_INFO;
}

int MpoSelectImage(MpoHandle *mpo, int index)
{
	int i;
	int ret;

	if (mpo == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if (index < 0 || index >= mpoGetImageCountInternal(mpo))
		return MPO_ERROR_OUT_OF_RANGE;
	for (i = 0; i < mpo->segmentCount; i++) {
		if (mpo->segments[i].data != NULL) {
			if (mpoFree(mpo, mpo->segments[i].data) < 0)
				return MPO_ERROR_ALLOC;
			mpo->segments[i].data = NULL;
		}
	}
	sce_paf_memset(&mpo->segmentCount, 0,
		       offsetof(MpoHandle, images) - offsetof(MpoHandle, segmentCount));
	ret = mpoOpenImage(mpo, index);
	if (ret < 0)
		return mpoMapScanError(ret);
	return 0;
}

int MpoSelectSegment(MpoHandle *mpo, uint16_t marker)
{
	int idx;

	if (mpo == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if ((marker & 0xFF00) != 0xFF00)
		return MPO_ERROR_OUT_OF_RANGE;
	idx = mpoFindSegment(mpo, marker);
	if (idx < 0)
		return MPO_ERROR_NO_SEGMENT;
	if (mpoSelectSegmentByIndex(mpo, idx) < 0)
		return MPO_ERROR_INTERNAL;
	return idx;
}

int MpoSelectIfd(MpoHandle *mpo, unsigned int ifd)
{
	if (mpo == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if ((int)ifd < 0 || (int)ifd >= MPO_MAX_IFDS)
		return MPO_ERROR_OUT_OF_RANGE;
	if (mpoSelectIfdInternal(mpo, (int)ifd) < 0)
		return MPO_ERROR_NO_IFD;
	return 0;
}

int MpoGetTag(MpoHandle *mpo, int tag, MpoTagInfo *info, void *buf, int size)
{
	int ret;

	if (mpo == NULL || info == NULL)
		return MPO_ERROR_INVALID_ARGUMENT;
	if (size < 0)
		return MPO_ERROR_OUT_OF_RANGE;
	ret = mpoFindAndReadTag(mpo, (uint16_t)tag, (MpoTagEntry *)info, buf, size);
	if (ret < 0) {
		switch (ret) {
		case -8:
			return MPO_ERROR_NO_IFD;
		case -13:
			return MPO_ERROR_TAG_NOT_FOUND;
		case -14:
			return MPO_ERROR_TAG_UNKNOWN;
		case -15:
			return MPO_ERROR_TAG_TYPE;
		case -16:
			return MPO_ERROR_TAG_COUNT;
		case -17:
			return MPO_ERROR_TAG_BOUNDS;
		default:
			return MPO_ERROR_INTERNAL;
		}
	}
	return ret;
}

static __attribute__((noinline)) int mpoSelectSegmentByIndex(MpoHandle *mpo, int index)
{
	if (index >= mpo->segmentCount)
		return -8;
	mpo->selectedSeg = &mpo->segments[index];
	return 0;
}

static inline int mpoFindSegmentInline(MpoHandle *mpo, int marker)
{
	int i;

	mpo->searchMarker = marker;
	mpo->searchIndex = 0;
	for (i = 0; i < mpo->segmentCount; i++) {
		if (marker == 0 || mpo->segments[i].marker == marker)
			return i;
		mpo->searchIndex = i + 1;
	}
	return -11;
}

static __attribute__((noinline)) int mpoFindSegment(MpoHandle *mpo, int marker)
{
	return mpoFindSegmentInline(mpo, marker);
}

static __attribute__((noinline)) int mpoValidateTag(MpoHandle *mpo, const MpoTagEntry *entry)
{
	int level = mpo->subIfdLevel;
	uint32_t ifdId = 0;
	int count;
	const MpoTagRule *rules;
	int i;

	if (level == 0) {
		MpoSegment *seg = mpo->selectedSeg;

		if (seg->hasTiff != 0 && sce_paf_memcmp(seg->idPtr, "Exif\0", 6) == 0) {
			count = s_tagRuleTables[level].count;
			ifdId = (mpo->selectedIfd == 0) ? MPO_IFD_EXIF_PRIMARY : MPO_IFD_EXIF_THUMB;
		} else if (mpo->selectedSeg->hasTiff != 0 &&
			   sce_paf_memcmp(mpo->selectedSeg->idPtr, "MPF", 4) == 0) {
			count = s_tagRuleTables[level].count;
			if (mpo->currentImage == 0) {
				ifdId = (mpo->selectedIfd == 0) ? MPO_IFD_MPF_INDEX
								: MPO_IFD_MPF_ATTR;
			} else if (mpo->selectedIfd == 0) {
				ifdId = MPO_IFD_MPF_ATTR;
			}
		} else {
			return -3;
		}
	} else if (level == 1) {
		MpoSegment *seg = mpo->selectedSeg;

		if (seg->hasTiff == 0 || sce_paf_memcmp(seg->idPtr, "Exif\0", 6) != 0 ||
		    mpo->selectedIfd != 0)
			return -3;
		count = s_tagRuleTables[level].count;
		ifdId = MPO_IFD_EXIF_SUB;
	} else if (level == 2) {
		MpoSegment *seg = mpo->selectedSeg;

		if (seg->hasTiff == 0 || sce_paf_memcmp(seg->idPtr, "Exif\0", 6) != 0 ||
		    mpo->selectedIfd != 0)
			return -3;
		if (mpo->subIfdType == MPO_IFD_MAKER_SONY_1) {
			count = s_tagRuleTables[level].count;
			ifdId = MPO_IFD_MAKER_SONY_1;
		} else if (mpo->subIfdType == MPO_IFD_MAKER_SONY_2) {
			count = s_tagRuleTables[level].count;
			ifdId = MPO_IFD_MAKER_SONY_2;
		} else {
			return -3;
		}
	} else {
		return -3;
	}

	rules = s_tagRuleTables[level].rules;
	for (i = 0; i < count; i++) {
		if (rules[i].ifdId == ifdId && rules[i].tag == entry->tag) {
			uint32_t expectedType = rules[i].type;

			if (expectedType != entry->type &&
			    (expectedType != MPO_TYPE_SHORT_OR_LONG ||
			     (entry->type != 3 && entry->type != 4)))
				return -15;
			if (rules[i].count != 0 && rules[i].count != (uint32_t)entry->count)
				return -16;
			return 0;
		}
	}
	return -14;
}

static __attribute__((noinline)) int mpoReadTagEntry(MpoHandle *mpo, int index, MpoTagEntry *entry,
						     void *buf, int size)
{
	const int typeSizes[10] = { 1, 1, 2, 4, 8, 1, 1, 2, 4, 8 };
	MpoSegment *seg = mpo->selectedSeg;
	const uint8_t *p = seg->curIfd + 12 * index;
	int ret;
	int byteSize;
	int copySize;
	uint32_t off;
	int i;

	entry->tag = (uint16_t)mpoReadU16(p + 2, seg->byteOrder);
	entry->type = (uint16_t)mpoReadU16(p + 4, seg->byteOrder);
	entry->count = (int)mpoReadU32(p + 6, seg->byteOrder);
	sce_paf_memset(&entry->value, 0, sizeof(entry->value));
	if (entry->type == 0 || entry->type > 10)
		return -15;
	ret = mpoValidateTag(mpo, entry);
	if (ret < 0)
		return ret;

	byteSize = entry->count * typeSizes[entry->type - 1];
	if (byteSize <= 4) {
		switch (entry->type) {
		case 2:
		case 7:
			sce_paf_memcpy(&entry->value, p + 10, 4);
			break;
		case 3:
			entry->value.shorts[0] = (uint16_t)mpoReadU16(p + 10, seg->byteOrder);
			entry->value.shorts[1] = (uint16_t)mpoReadU16(p + 12, seg->byteOrder);
			break;
		case 4:
			entry->value.u32 = mpoReadU32(p + 10, seg->byteOrder);
			break;
		default:
			return -3;
		}
		return byteSize;
	}

	if (buf == NULL) {
		if (entry->tag == MPO_TAG_MAKERNOTE && entry->type == 7) {
			off = mpoReadU32(p + 10, seg->byteOrder);
			entry->value.u32 = off;
			if ((int)off + byteSize > mpo->selectedSeg->tiffSize)
				return -17;
		}
		return byteSize;
	}

	copySize = (size < byteSize) ? size : byteSize;
	off = mpoReadU32(p + 10, seg->byteOrder);
	entry->value.u32 = off;
	if ((int)off + copySize > mpo->selectedSeg->tiffSize)
		return -17;

	switch (entry->type) {
	case 2:
	case 7:
		sce_paf_memcpy(buf, seg->curBase + off, copySize);
		return byteSize;
	case 3: {
		uint16_t *dst = (uint16_t *)buf;

		for (i = 0; i < entry->count; i++, off += typeSizes[entry->type - 1])
			*dst++ = (uint16_t)mpoReadU16(seg->curBase + off, seg->byteOrder);
		return byteSize;
	}
	case 4: {
		uint32_t *dst = (uint32_t *)buf;

		for (i = 0; i < entry->count; i++, off += typeSizes[entry->type - 1])
			*dst++ = mpoReadU32(seg->curBase + off, seg->byteOrder);
		return byteSize;
	}
	case 5:
	case 10: {
		uint32_t *dst = (uint32_t *)buf;

		for (i = 0; i < entry->count; i++, off += typeSizes[entry->type - 1]) {
			dst[0] = mpoReadU32(seg->curBase + off, seg->byteOrder);
			dst[1] = mpoReadU32(seg->curBase + off + 4, seg->byteOrder);
			dst += 2;
		}
		return byteSize;
	}
	default:
		return -3;
	}
}

static __attribute__((noinline)) int mpoFindAndReadTag(MpoHandle *mpo, uint16_t tag,
						       MpoTagEntry *entry, void *buf, int size)
{
	MpoSegment *seg = mpo->selectedSeg;
	int count;
	int i;

	if (seg->hasTiff == 0)
		return -8;
	count = (int)mpoReadU16(seg->curIfd, seg->byteOrder);
	if ((int)(seg->curIfd - mpo->selectedSeg->tiffBase) + 12 * count + 2 >=
	    mpo->selectedSeg->tiffSize)
		return -17;
	for (i = 0; i < count; i++) {
		if (mpoReadU16(seg->curIfd + 12 * i + 2, seg->byteOrder) == tag)
			return mpoReadTagEntry(mpo, i, entry, buf, size);
	}
	return -13;
}

static __attribute__((noinline)) int mpoSelectIfdInternal(MpoHandle *mpo, int ifd)
{
	MpoSegment *seg = mpo->selectedSeg;

	if (seg->hasTiff == 0)
		return -8;
	if (ifd >= seg->ifdCount)
		return -8;
	seg->curBase = seg->tiffStart;
	seg->curIfd = seg->ifdPtrs[ifd];
	mpo->selectedIfd = ifd;
	mpo->subIfdType = 0;
	mpo->subIfdLevel = 0;
	return 0;
}

static __attribute__((noinline)) int mpoGetImageOffsetInternal(MpoHandle *mpo, int index)
{
	if (mpo->images == NULL)
		return -10;
	return mpo->images[index].offset;
}

static __attribute__((noinline)) int mpoGetImageTypeInternal(MpoHandle *mpo, int index)
{
	if (mpo->entries == NULL)
		return 0;
	switch (mpo->entries[index].attr & 0xFFFFFF) {
	case 0x000000:
		return 0x000000;
	case 0x010001:
		return 0x010001;
	case 0x010002:
		return 0x010002;
	case 0x020001:
		return MPO_TYPE_PANORAMA;
	case 0x020002:
		return MPO_TYPE_DISPARITY;
	case 0x020003:
		return MPO_TYPE_MULTI_ANGLE;
	case 0x030000:
		return 0x030000;
	default:
		return -10;
	}
}

static __attribute__((noinline)) int mpoGetImageCountInternal(MpoHandle *mpo)
{
	return mpo->imageCount;
}

static __attribute__((noinline)) int mpoGetImageSizeInternal(MpoHandle *mpo, int index)
{
	if (mpo->images == NULL)
		return -10;
	return mpo->images[index].size;
}

static __attribute__((noinline)) int mpoIsRepresentativeInternal(MpoHandle *mpo, int index)
{
	if (mpo->entries == NULL)
		return -10;
	return (mpo->entries[index].attr & 0x20000000U) != 0;
}

static __attribute__((noinline)) int mpoParseSegmentTiff(MpoHandle *mpo, const uint8_t *buf)
{
	MpoSegment *seg = mpo->selectedSeg;
	int isExif;
	int byteOrder;
	uint8_t *tiff;
	uint32_t ifdOff;
	uint32_t prevOff;
	int ifdCount;

	if (seg->marker != MPO_MARKER_APP1 && seg->marker != MPO_MARKER_APP2)
		return 0;
	isExif = 0;
	if (sce_paf_memcmp(buf + 4, "Exif\0", 6) == 0) {
		isExif = 1;
		if (mpo->hasExif != 0)
			return 0;
	} else if (sce_paf_memcmp(buf + 4, "MPF", 4) == 0) {
		if (mpo->hasMpf != 0)
			return 0;
	} else {
		return 0;
	}

	if (seg->data == NULL) {
		seg->data = (uint8_t *)mpoAlloc(mpo, MPO_SEG_BUF_SIZE);
		if (seg->data == NULL)
			return -4;
	}
	sce_paf_memset(seg->data, 0, MPO_SEG_BUF_SIZE);
	sce_paf_memcpy(seg->data, buf, seg->length);
	seg->headerOffset = 4;
	seg->idPtr = seg->data + 4;
	if (isExif != 0) {
		seg->idSize = 6;
		seg->tiffBase = seg->data + 10;
		seg->tiffSize = seg->length - 10;
	} else {
		seg->idSize = 4;
		seg->tiffBase = seg->data + 8;
		seg->tiffSize = seg->length - 8;
	}

	seg = mpo->selectedSeg;
	tiff = seg->tiffBase;
	seg->tiffStart = tiff;
	if (sce_paf_memcmp(tiff, "II", 2) == 0) {
		byteOrder = 0;
	} else if (sce_paf_memcmp(tiff, "MM", 2) == 0) {
		byteOrder = 1;
	} else {
		mpoFree(mpo, seg->data);
		seg->data = NULL;
		return -7;
	}
	seg->byteOrder = byteOrder;
	if (mpoReadU16(tiff + 2, byteOrder) != 42) {
		mpoFree(mpo, seg->data);
		seg->data = NULL;
		return -7;
	}

	ifdOff = mpoReadU32(tiff + 4, seg->byteOrder);
	seg->ifdCount = 0;
	prevOff = ifdOff;
	ifdCount = 0;
	while (ifdOff != 0 && ifdCount < MPO_MAX_IFDS) {
		uint32_t entries;
		uint32_t nextPos;
		uint32_t nextOff;

		if (ifdOff > (uint32_t)(mpo->selectedSeg->tiffSize - 2))
			break;
		seg->ifdPtrs[ifdCount] = seg->tiffStart + ifdOff;
		entries = mpoReadU16(seg->tiffStart + ifdOff, seg->byteOrder);
		if (entries == 0)
			break;
		nextPos = ifdOff + 12 * entries + 2;
		if (nextPos > (uint32_t)(mpo->selectedSeg->tiffSize - 4))
			break;
		nextOff = mpoReadU32(seg->tiffStart + nextPos, seg->byteOrder);
		if (nextOff != 0 && nextOff <= prevOff)
			ifdOff = 0;
		else {
			ifdOff = nextOff;
			prevOff = nextOff;
		}
		ifdCount = seg->ifdCount + 1;
		seg->ifdCount = ifdCount;
	}
	seg->curBase = seg->tiffStart;
	seg->curIfd = seg->ifdPtrs[0];
	if (isExif != 0)
		mpo->hasExif = 1;
	else
		mpo->hasMpf = 1;
	seg->hasTiff = 1;
	return 0;
}

static __attribute__((noinline)) int mpoScanSegments(MpoHandle *mpo)
{
	int filePos = 0;
	int relOffset = 0;
	int marker;

	if (mpo->currentImage != 0)
		filePos = (mpo->images != NULL) ? mpo->images[mpo->currentImage].offset : -10;
	if (mpo->useReader == 0 && mpoFileSeek(mpo->fd, filePos, 0) < 0)
		return -2;
	mpo->segmentCount = 0;
	do {
		int n;
		int segLen;
		MpoSegment *seg;
		int ret;

		n = mpoRead(mpo, filePos, 2, mpo->readBuf);
		if (n < 2)
			return -2;
		marker = (int)mpoReadU16(mpo->readBuf, 1);
		segLen = n;
		filePos += n;
		if ((marker & 0xFF00) != 0xFF00)
			return -5;
		if (marker < MPO_MARKER_RST0 || marker > MPO_MARKER_EOI) {
			int n2;
			int payloadLen;
			int n3;

			n2 = mpoRead(mpo, filePos, 2, mpo->readBuf + n);
			if (n2 < 2)
				return -2;
			payloadLen = (int)mpoReadU16(mpo->readBuf + segLen, 1);
			if (payloadLen > MPO_SEG_BUF_SIZE || payloadLen < 2)
				return -6;
			segLen += n2;
			filePos += n2;
			n3 = mpoRead(mpo, filePos, payloadLen - 2, mpo->readBuf + segLen);
			if (n3 < payloadLen - 2)
				return -2;
			if ((marker == MPO_MARKER_SOF0 || marker == MPO_MARKER_SOF2) &&
			    mpo->readBuf[segLen] == 8) {
				mpo->height = (int)mpoReadU16(mpo->readBuf + segLen + 1, 1);
				mpo->width = (int)mpoReadU16(mpo->readBuf + segLen + 3, 1);
			}
			segLen += n3;
			filePos += n3;
		}
		seg = &mpo->segments[mpo->segmentCount];
		seg->marker = marker;
		seg->fileOffset = relOffset;
		seg->length = segLen;
		seg->hasTiff = 0;
		mpo->selectedSeg = seg;
		ret = mpoParseSegmentTiff(mpo, mpo->readBuf);
		if (ret < 0)
			return ret;
		mpo->segmentCount++;
		if (mpo->segmentCount >= MPO_MAX_SEGMENTS && marker != MPO_MARKER_SOS)
			return -6;
		relOffset += segLen;
	} while (marker != MPO_MARKER_SOS);
	return 0;
}

static __attribute__((noinline)) int mpoFindMpfSegment(MpoHandle *mpo)
{
	int idx;

	idx = mpoFindSegmentInline(mpo, MPO_MARKER_APP2);
	while (idx >= 0) {
		if (idx >= mpo->segmentCount)
			return -3;
		mpo->selectedSeg = &mpo->segments[idx];
		if (mpo->selectedSeg->hasTiff != 0 &&
		    sce_paf_memcmp(mpo->selectedSeg->idPtr, "MPF", 4) == 0)
			return idx;
		mpo->searchIndex++;
		idx = -11;
		while (mpo->searchIndex < mpo->segmentCount) {
			if (mpo->searchMarker == 0 ||
			    mpo->segments[mpo->searchIndex].marker == mpo->searchMarker) {
				idx = mpo->searchIndex;
				break;
			}
			mpo->searchIndex++;
		}
	}
	return -10;
}

static __attribute__((noinline)) int mpoParseMpIndex(MpoHandle *mpo)
{
	MpoTagEntry entry;
	int numImages;
	int i;

	if (mpoFindMpfSegment(mpo) < 0)
		return 1;
	if (mpo->selectedSeg->hasTiff == 0 || mpo->selectedSeg->ifdCount <= 0) {
		mpo->selectedSeg->hasTiff = 0;
		return 1;
	}
	mpo->selectedSeg->curBase = mpo->selectedSeg->tiffStart;
	mpo->selectedSeg->curIfd = mpo->selectedSeg->ifdPtrs[0];
	mpo->selectedIfd = 0;
	mpo->subIfdType = 0;
	mpo->subIfdLevel = 0;

	if (mpoFindAndReadTag(mpo, MPO_TAG_MP_NUM_IMAGES, &entry, NULL, 0) < 0) {
		mpo->selectedSeg->hasTiff = 0;
		return 1;
	}
	numImages = (int)entry.value.u32;
	if (numImages <= 0) {
		mpo->selectedSeg->hasTiff = 0;
		return 1;
	}
	mpo->entries = (MpoMpEntry *)mpoAlloc(mpo, 16 * numImages);
	if (mpo->entries == NULL)
		return 0;
	if (mpoFindAndReadTag(mpo, MPO_TAG_MP_ENTRY, &entry, mpo->entries, 16 * numImages) <= 0 ||
	    entry.count != 16 * numImages) {
		mpoFree(mpo, mpo->entries);
		mpo->entries = NULL;
		mpo->selectedSeg->hasTiff = 0;
		return 1;
	}
	mpo->images = (MpoImageLoc *)mpoAlloc(mpo, 8 * numImages);
	if (mpo->images == NULL) {
		mpoFree(mpo, mpo->entries);
		mpo->entries = NULL;
		return 0;
	}
	for (i = 0; i < numImages; i++) {
		MpoMpEntry *e = &mpo->entries[i];
		int bo = mpo->selectedSeg->byteOrder;

		e->attr = mpoReadU32((const uint8_t *)&e->attr, bo);
		e->size = mpoReadU32((const uint8_t *)&e->size, mpo->selectedSeg->byteOrder);
		e->offset = mpoReadU32((const uint8_t *)&e->offset, mpo->selectedSeg->byteOrder);
		e->dep1 = (uint16_t)mpoReadU16((const uint8_t *)&e->dep1,
					       mpo->selectedSeg->byteOrder);
		e->dep2 = (uint16_t)mpoReadU16((const uint8_t *)&e->dep2,
					       mpo->selectedSeg->byteOrder);
		if (i != 0)
			mpo->images[i].offset = (int)e->offset + mpo->selectedSeg->fileOffset +
						mpo->selectedSeg->headerOffset +
						mpo->selectedSeg->idSize;
		else
			mpo->images[i].offset = 0;
		mpo->images[i].size = (int)e->size;
	}
	mpo->imageCount = numImages;
	mpo->isMpo = 1;
	return 1;
}

static __attribute__((noinline)) int mpoAdjustMpfIfdCount(MpoHandle *mpo, int index)
{
	int type = mpoGetImageTypeInternal(mpo, index);
	int mpfIdx;

	if (type < 0)
		return -10;
	mpfIdx = mpoFindMpfSegment(mpo);
	if (type == 0x010001 || type == 0x010002) {
		if (index == 0 && mpfIdx <= 0)
			return -10;
		if (index != 0 && mpfIdx <= 0)
			return 0;
		if (mpo->selectedSeg->hasTiff == 0 || mpo->selectedSeg->ifdCount < 0)
			return -10;
		if (mpo->selectedSeg->ifdCount > 0)
			mpo->selectedSeg->ifdCount = 0;
	} else {
		if (index == 0 && mpfIdx <= 0)
			return -10;
		if (index != 0 && mpfIdx <= 0)
			return 0;
		if (mpo->selectedSeg->hasTiff == 0 || mpo->selectedSeg->ifdCount < 0)
			return -10;
		if (type == 0x030000) {
			if (mpo->selectedSeg->ifdCount > 1)
				mpo->selectedSeg->ifdCount = 1;
		} else if ((index == 0 && mpo->selectedSeg->ifdCount > 2) ||
			   (index != 0 && mpo->selectedSeg->ifdCount > 1)) {
			mpo->selectedSeg->ifdCount = (index != 0) ? 1 : 2;
		}
	}
	return 0;
}

static __attribute__((noinline)) int mpoOpenFirstImage(MpoHandle *mpo)
{
	int ret;
	int idx;

	mpo->imageCount = 1;
	mpo->currentImage = 0;
	ret = mpoScanSegments(mpo);
	if (ret < 0) {
		mpo->imageCount = 0;
		return ret;
	}
	if (mpoParseMpIndex(mpo) == 0)
		return -4;
	if (mpo->isMpo != 0) {
		ret = mpoAdjustMpfIfdCount(mpo, 0);
		if (ret < 0)
			return ret;
	}
	idx = mpoFindSegmentInline(mpo, MPO_MARKER_APP1);
	if (idx < 0) {
		if (mpoFindMpfSegment(mpo) >= 0)
			return -10;
		return 0;
	}
	if (idx < mpo->segmentCount) {
		MpoSegment *seg = &mpo->segments[idx];

		mpo->selectedSeg = seg;
		if (seg->hasTiff == 0)
			return 0;
		if (sce_paf_memcmp(seg->idPtr, "Exif\0", 6) != 0)
			return 0;
		seg = mpo->selectedSeg;
		if (seg->hasTiff != 0 && seg->ifdCount >= 0) {
			if (seg->ifdCount != 0) {
				seg->curBase = seg->tiffStart;
				seg->curIfd = seg->ifdPtrs[0];
				mpo->selectedIfd = 0;
				mpo->subIfdType = 0;
				mpo->subIfdLevel = 0;
			}
			return 0;
		}
	}
	return -3;
}

static __attribute__((noinline)) int mpoOpenImage(MpoHandle *mpo, int index)
{
	int ret;
	int idx;

	mpo->currentImage = index;
	ret = mpoScanSegments(mpo);
	if (ret < 0)
		return ret;
	if (mpoFindMpfSegment(mpo) >= 0 && mpo->isMpo == 0)
		mpo->selectedSeg->hasTiff = 0;
	if (mpo->isMpo != 0) {
		ret = mpoAdjustMpfIfdCount(mpo, index);
		if (ret < 0)
			return ret;
	}
	idx = mpoFindSegmentInline(mpo, MPO_MARKER_APP1);
	if (idx < 0) {
		if (mpoFindMpfSegment(mpo) >= 0)
			return -10;
		return 0;
	}
	if (idx < mpo->segmentCount) {
		MpoSegment *seg = &mpo->segments[idx];

		mpo->selectedSeg = seg;
		if (seg->hasTiff == 0)
			return 0;
		if (sce_paf_memcmp(seg->idPtr, "Exif\0", 6) != 0)
			return 0;
		seg = mpo->selectedSeg;
		if (seg->hasTiff != 0 && seg->ifdCount >= 0) {
			if (seg->ifdCount != 0) {
				seg->curBase = seg->tiffStart;
				seg->curIfd = seg->ifdPtrs[0];
				mpo->selectedIfd = 0;
				mpo->subIfdType = 0;
				mpo->subIfdLevel = 0;
			}
			return 0;
		}
	}
	return -3;
}

static __attribute__((noinline)) uint32_t mpoReadU16(const uint8_t *p, int byteOrder)
{
	if (byteOrder == 0)
		return *(const uint16_t *)p;
	if (byteOrder == 1)
		return ((uint32_t)p[0] << 8) | p[1];
	return ((uint32_t)p[1] << 8) | p[0];
}

static __attribute__((noinline)) uint32_t mpoReadU32(const uint8_t *p, int byteOrder)
{
	if (byteOrder == 0)
		return *(const uint32_t *)p;
	if (byteOrder == 1)
		return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
		       ((uint32_t)p[2] << 8) | p[3];
	return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[1] << 8) | p[0];
}

static __attribute__((noinline)) SceUID mpoFileOpen(const char *path, int16_t flags)
{
	int sceFlags = 0;
	SceUID fd;

	if ((flags & 1) != 0)
		sceFlags |= SCE_O_RDONLY;
	if ((flags & 2) != 0)
		sceFlags |= SCE_O_WRONLY;
	if ((flags & 4) != 0)
		sceFlags |= SCE_O_RDWR;
	if ((flags & 0x400) != 0)
		sceFlags |= SCE_O_APPEND;
	if ((flags & 0x40) != 0)
		sceFlags |= SCE_O_CREAT;
	if ((flags & 0x200) != 0)
		sceFlags |= SCE_O_TRUNC;
	if ((flags & 0x80) != 0)
		sceFlags |= SCE_O_EXCL;
	fd = sceIoOpen(path, sceFlags, 0);
	if (fd < 0)
		return -1;
	return fd;
}

static __attribute__((noinline)) int mpoFileClose(SceUID fd)
{
	if (sceIoClose(fd) != 0)
		return -1;
	return 0;
}

static __attribute__((noinline)) int mpoFileSeek(SceUID fd, int offset, int whence)
{
	int sceWhence;
	int ret;

	switch (whence) {
	case 0:
		sceWhence = SCE_SEEK_SET;
		break;
	case 1:
		sceWhence = SCE_SEEK_CUR;
		break;
	case 2:
		sceWhence = SCE_SEEK_END;
		break;
	default:
		return -1;
	}
	ret = sceIoLseek32(fd, offset, sceWhence);
	if (ret < 0)
		return -1;
	return ret;
}

static __attribute__((noinline)) void *mpoAlloc(MpoHandle *mpo, int size)
{
	return mpo->callbacks.alloc((size_t)size, mpo->callbacks.allocArg);
}

static __attribute__((noinline)) int mpoFree(MpoHandle *mpo, void *ptr)
{
	if (mpo->callbacks.free(ptr, mpo->callbacks.freeArg) == 0)
		return 0;
	return -1;
}

static __attribute__((noinline)) int mpoRead(MpoHandle *mpo, int offset, int size, void *buf)
{
	int ret;

	if (size == 0)
		return 0;
	if (mpo->useReader == 0) {
		ret = sceIoRead(mpo->fd, buf, size);
		if (ret < 0)
			return -1;
		return ret;
	}
	if (mpo->useReader == 1) {
		MpoReaderFunc reader = (MpoReaderFunc)mpo->callbacks.reader;

		ret = reader((int64_t)offset, (int64_t)size, buf, mpo->callbacks.readerArg);
		if (ret < 0)
			return -1;
		return ret;
	}
	return -1;
}
