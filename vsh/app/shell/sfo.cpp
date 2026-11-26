/*
 * sfo.cpp: user-mode PSF (param.sfo) reader/writer library linked by SceShell
 * (0x81342590..0x81343593). Matches Sony's libpsf / kernel appmgr PSF layout
 * with paf heap allocation and 512 writable entry capacity.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>

#define SFO_ERR_NO_MEMORY		-2146412031	/* 0x80105A01 */
#define SFO_ERR_WRITE_FAILED		-2146412030	/* 0x80105A02 */
#define SFO_ERR_INVALID_ARG		-2146412029	/* 0x80105A03 */
#define SFO_ERR_BAD_MAGIC		-2146412028	/* 0x80105A04 */
#define SFO_ERR_NOT_FOUND		-2146412027	/* 0x80105A05 */
#define SFO_ERR_DUPLICATE		-2146412026	/* 0x80105A06 */
#define SFO_ERR_BAD_VERSION		-2146412025	/* 0x80105A07 */
#define SFO_ERR_BAD_HEADER		-2146412023	/* 0x80105A09 */

#define SFO_FLAG_READONLY		0x01u
#define SFO_FLAG_PARSED			0x02u
#define SFO_FLAG_SORTED			0x04u
#define SFO_FLAG_DIRTY			0x40u

#define SFO_ENTRY_FLAG_MODIFIED		0x01u
#define SFO_ENTRY_FLAG_ALLOCATED	0x02u

#define SFO_MAX_WRITE_ENTRIES		512u
#define SFO_KEY_MAX_LEN			31u
#define SFO_KEY_SLOT_SIZE		32u
#define SFO_DISK_KEY_UNUSED		0xFFFFu
#define SFO_VERSION_1_1			0x00000101u

static const unsigned char kSfoMagic[4] = { 0x00, 'P', 'S', 'F' };

struct SfoDiskEntry {
	SceUInt16 keyOffset;				/* +0x00 */
	SceUInt16 format;				/* +0x02 */
	SceSize length;					/* +0x04 */
	SceSize capacity;				/* +0x08 */
	SceUInt32 valueOffset;				/* +0x0C */
};

struct SfoRuntimeEntry {
	SfoDiskEntry *diskEntry;			/* +0x00 */
	unsigned char *key;				/* +0x04 */
	const void *value;				/* +0x08 */
	SceUInt32 entryFlags;				/* +0x0C */
};

struct SfoSearchKey {
	const unsigned char *key;			/* +0x00 */
	const unsigned char *keyTable;			/* +0x04 */
};

struct SfoHeader {
	unsigned char magic[4];				/* +0x00 */
	SceUInt32 version;				/* +0x04 */
	SceUInt32 keyTableOffset;			/* +0x08 */
	SceUInt32 valueTableOffset;			/* +0x0C */
	SceSize entryCount;				/* +0x10 */
};

/* Base 20-byte handle returned by sfoOpen (and head of SfoWriteHandle) */
struct SfoHandle {
	SceUInt32 handleFlags;				/* +0x00 */
	SfoHeader *header;				/* +0x04 */
	SfoDiskEntry *diskEntries;			/* +0x08 */
	unsigned char *keyTable;			/* +0x0C */
	union {
		unsigned char *valueTable;
		SceSize entryCapacity;
	} store;					/* +0x10 */
};

/* Mutable handle allocated by sfoCreate (60 + 64 * capacity bytes) */
struct SfoWriteHandle {
	SfoHandle base;					/* +0x00 (20 bytes) */
	SceSize entryHighWater;				/* +0x14 */
	SceSize diskHighWater;				/* +0x18 */
	SceSize keyHighWater;				/* +0x1C */
	SfoHeader embeddedHeader;			/* +0x20 (20 bytes) */
	SfoRuntimeEntry *entries;			/* +0x34 */
	unsigned char (*keys)[SFO_KEY_SLOT_SIZE];	/* +0x38 */
};

struct SfoBufferWriter {
	unsigned char *buffer;				/* +0x00 */
	SceSize offset;					/* +0x04 */
	SceSize remaining;				/* +0x08 */
};

struct SfoWriter {
	int (*write)(void *context, const void *src, SceSize size);
	void *context;
};

/* --- Internal byte/string/sort/search primitives (0x813433CC..0x81343593) --- */

/* 0x813433CC */
static void *sfoCopyMemory(void *dst, const void *src, SceSize size)
{
	unsigned char *dstBytes = static_cast<unsigned char *>(dst);
	const unsigned char *srcBytes = static_cast<const unsigned char *>(src);
	for (SceSize idx = 0; idx < size; idx++)
		dstBytes[idx] = srcBytes[idx];
	return dst;
}

/* 0x813433EE */
static void *sfoSetMemory(void *dst, int fillByte, SceSize size)
{
	unsigned char *dstBytes = static_cast<unsigned char *>(dst);
	for (SceSize idx = 0; idx < size; idx++)
		dstBytes[idx] = static_cast<unsigned char>(fillByte);
	return dst;
}

/* 0x8134340A */
static int sfoCompareMemory(const void *left, const void *right, SceSize size)
{
	const signed char *leftBytes = static_cast<const signed char *>(left);
	const signed char *rightBytes = static_cast<const signed char *>(right);
	for (SceSize idx = 0; idx < size; idx++) {
		if (leftBytes[idx] > rightBytes[idx])
			return 1;
		if (leftBytes[idx] < rightBytes[idx])
			return -1;
	}
	return 0;
}

/* 0x81343440 */
static SceSize sfoStringLength(const char *str)
{
	SceSize len = 0;
	while (str[len] != '\0')
		len++;
	return len;
}

/* 0x81343458 */
static int sfoCompareString(const unsigned char *left, const unsigned char *right)
{
	while (*left == *right) {
		if (*left == '\0')
			return 0;
		left++;
		right++;
	}
	return static_cast<int>(*left) - static_cast<int>(*right);
}

/* 0x8134347C */
static void *sfoBinarySearch(const void *key, void *base, SceSize count, SceSize width,
			     int (*compare)(const void *, const void *))
{
	if (count == 0)
		return NULL;

	unsigned char *array = static_cast<unsigned char *>(base);
	SceSize low = 0;
	SceSize high = count - 1;
	while (low <= high) {
		SceSize mid = (low + high) >> 1;
		unsigned char *elem = array + mid * width;
		int cmp = compare(key, elem);
		if (cmp == 0)
			return elem;
		if (low == high)
			break;
		if (cmp <= 0) {
			if (mid == 0)
				high = 0;
			else
				high = mid - 1;
		} else {
			low = mid + 1;
		}
	}
	return NULL;
}

/* 0x813434CE */
static void sfoShellSort(void *base, SceSize count, SceSize width,
			 int (*compare)(const void *, const void *))
{
	unsigned char *array = static_cast<unsigned char *>(base);
	for (SceSize gap = count >> 1; gap != 0; gap >>= 1) {
		for (SceSize outer = gap; outer < count; outer++) {
			for (SceInt32 pos = static_cast<SceInt32>(outer - gap); pos >= 0;
			     pos -= static_cast<SceInt32>(gap)) {
				unsigned char *left = array + static_cast<SceSize>(pos) * width;
				unsigned char *right =
					array + (static_cast<SceSize>(pos) + gap) * width;
				if (compare(left, right) <= 0)
					break;
				for (SceSize byteIdx = 0; byteIdx < width; byteIdx++) {
					unsigned char swapByte = left[byteIdx];
					left[byteIdx] = right[byteIdx];
					right[byteIdx] = swapByte;
				}
			}
		}
	}
}

/* --- Comparators (0x81342940..0x81342977) --- */

/* 0x81342940 */
static int sfoCompareRuntimeEntries(const void *left, const void *right)
{
	const SfoRuntimeEntry *entryLeft = static_cast<const SfoRuntimeEntry *>(left);
	const SfoRuntimeEntry *entryRight = static_cast<const SfoRuntimeEntry *>(right);
	if (entryLeft->key == NULL)
		return 1;
	if (entryRight->key == NULL)
		return -1;
	return sfoCompareString(entryLeft->key, entryRight->key);
}

/* 0x8134295A */
static int sfoCompareKeyToRuntimeEntry(const void *key, const void *elem)
{
	const SfoRuntimeEntry *entry = static_cast<const SfoRuntimeEntry *>(elem);
	return sfoCompareString(static_cast<const unsigned char *>(key), entry->key);
}

/* 0x81342964 */
static int sfoCompareKeyToDiskEntry(const void *searchKey, const void *elem)
{
	const SfoSearchKey *sk = static_cast<const SfoSearchKey *>(searchKey);
	const SfoDiskEntry *disk = static_cast<const SfoDiskEntry *>(elem);
	return sfoCompareString(sk->key, sk->keyTable + disk->keyOffset);
}

/* --- Internal helpers (0x81342590, 0x813428E6..0x81342B6D, 0x81342EAE..0x813433CC) --- */

/* 0x81342590 */
static void sfoSortEntries(SfoHandle *handle)
{
	if ((handle->handleFlags & (SFO_FLAG_SORTED | SFO_FLAG_PARSED)) != 0)
		return;

	SfoWriteHandle *writeHandle = reinterpret_cast<SfoWriteHandle *>(handle);
	sfoShellSort(writeHandle->entries, writeHandle->entryHighWater, sizeof(SfoRuntimeEntry),
		     sfoCompareRuntimeEntries);
	handle->handleFlags |= SFO_FLAG_SORTED;
	writeHandle->entryHighWater = handle->header->entryCount;
}

/* 0x813428E6 */
static void sfoRemoveEntry(SfoHandle *handle, SfoRuntimeEntry *entry)
{
	if (handle == NULL || entry == NULL || (handle->handleFlags & SFO_FLAG_PARSED) != 0)
		return;

	if (entry->diskEntry != NULL && (entry->entryFlags & SFO_ENTRY_FLAG_ALLOCATED) != 0) {
		if (entry->key != NULL) {
			entry->key[0] = '\0';
			entry->key = NULL;
		}
		entry->diskEntry->keyOffset = SFO_DISK_KEY_UNUSED;
		entry->diskEntry = NULL;
	}
	entry->value = NULL;
	entry->entryFlags = 0;
	handle->header->entryCount--;
	handle->handleFlags &= ~SFO_FLAG_SORTED;
	handle->handleFlags |= SFO_FLAG_DIRTY;
}

/* 0x813429B6 */
static SfoHandle *sfoAllocHandle(int isParsed, SceSize capacity)
{
	SceSize totalSize = sizeof(SfoHandle);
	if (!isParsed) {
		totalSize = sizeof(SfoWriteHandle) +
			    capacity * (sizeof(SfoRuntimeEntry) + sizeof(SfoDiskEntry) +
					SFO_KEY_SLOT_SIZE);
	}

	unsigned char *rawMem = static_cast<unsigned char *>(sce_paf_malloc(totalSize));
	if (rawMem == NULL)
		return NULL;

	sfoSetMemory(rawMem, 0, totalSize);
	SfoHandle *handle = reinterpret_cast<SfoHandle *>(rawMem);
	if (isParsed) {
		handle->handleFlags |= SFO_FLAG_PARSED;
		return handle;
	}

	SfoWriteHandle *writeHandle = reinterpret_cast<SfoWriteHandle *>(rawMem);
	unsigned char *tail = rawMem + sizeof(SfoWriteHandle);
	SfoRuntimeEntry *entries = reinterpret_cast<SfoRuntimeEntry *>(tail);
	SfoDiskEntry *diskEntries =
		reinterpret_cast<SfoDiskEntry *>(tail + capacity * sizeof(SfoRuntimeEntry));
	unsigned char (*keys)[SFO_KEY_SLOT_SIZE] = reinterpret_cast<unsigned char (*)[SFO_KEY_SLOT_SIZE]>(
		tail + capacity * (sizeof(SfoRuntimeEntry) + sizeof(SfoDiskEntry)));

	writeHandle->base.store.entryCapacity = capacity;
	writeHandle->base.header = &writeHandle->embeddedHeader;
	writeHandle->base.diskEntries = diskEntries;
	writeHandle->entries = entries;
	writeHandle->keys = keys;

	for (SceSize idx = 0; idx < capacity; idx++)
		diskEntries[idx].keyOffset = SFO_DISK_KEY_UNUSED;

	return handle;
}

static bool sfoIsValidFormat(SceUInt16 format)
{
	if (format > 0x0204u)
		return (format == 0x0304u || format == 0x0404u || format == 0x0504u);

	if (format == 0x0204u)
		return true;

	SceUInt16 lowCode = format;
	if (lowCode > 0x00FFu) {
		if (lowCode > 0x0120u)
			return false;
		lowCode = static_cast<SceUInt16>(lowCode - 0x0100u);
	} else if (lowCode > 0x0020u) {
		return false;
	}

	return (lowCode == 0x00u || lowCode == 0x04u || lowCode == 0x08u ||
		lowCode == 0x10u || lowCode == 0x20u);
}

/* 0x81342A26 */
static int sfoValidateEntries(const void *rawSfo, SceSize entryOffset, const SfoHeader *header,
			      SceSize totalSize)
{
	const unsigned char *baseBytes = static_cast<const unsigned char *>(rawSfo);
	const SfoDiskEntry *entries =
		reinterpret_cast<const SfoDiskEntry *>(baseBytes + entryOffset);
	SceSize count = header->entryCount;

	for (SceSize idx = 0; idx < count; idx++) {
		if (header->valueTableOffset <= header->keyTableOffset + entries[idx].keyOffset)
			return SFO_ERR_INVALID_ARG;
		if (!sfoIsValidFormat(entries[idx].format))
			return SFO_ERR_INVALID_ARG;
		if (entries[idx].capacity < entries[idx].length)
			return SFO_ERR_INVALID_ARG;
		if (totalSize < entries[idx].valueOffset + entries[idx].capacity)
			return SFO_ERR_INVALID_ARG;

		if (idx == count - 1) {
			if (header->valueTableOffset + entries[idx].valueOffset +
			    entries[idx].capacity != totalSize)
				return SFO_ERR_INVALID_ARG;
		} else {
			if (entries[idx].valueOffset + entries[idx].capacity !=
			    entries[idx + 1].valueOffset)
				return SFO_ERR_INVALID_ARG;
		}
	}
	return 0;
}

/* 0x81342EAE */
static int sfoInitializeHeader(SfoHandle *handle, const void *magic, SceUInt8 majorVersion,
			       SceUInt8 minorVersion)
{
	if (handle == NULL || handle->header == NULL)
		return SFO_ERR_INVALID_ARG;
	if ((handle->handleFlags & SFO_FLAG_PARSED) != 0)
		return SFO_ERR_INVALID_ARG;

	if (magic != NULL)
		sfoCopyMemory(handle->header->magic, magic, 4);
	else
		sfoCopyMemory(handle->header->magic, kSfoMagic, 4);

	handle->header->version =
		static_cast<SceUInt32>(minorVersion) | (static_cast<SceUInt32>(majorVersion) << 8);
	return 0;
}

/* 0x81342F08 */
static void sfoComputeLayout(SfoHandle *handle, SceSize *outKeyTableSize)
{
	sfoSortEntries(handle);

	SfoWriteHandle *writeHandle = reinterpret_cast<SfoWriteHandle *>(handle);
	SceSize count = handle->header->entryCount;
	SceSize valueCursor = 0;
	SceSize keyCursor = 0;

	for (SceSize idx = 0; idx < count; idx++) {
		SfoDiskEntry *disk = writeHandle->entries[idx].diskEntry;
		SceSize alignedCap = (disk->capacity + 3u) & ~3u;
		disk->keyOffset = static_cast<SceUInt16>(keyCursor);
		disk->valueOffset = valueCursor;
		disk->capacity = alignedCap;

		SceSize nextValue = valueCursor + alignedCap;
		if (idx + 1 >= count) {
			valueCursor = (nextValue + 3u) & ~3u;
		} else {
			SceSize alignUnit =
				static_cast<SceSize>(writeHandle->entries[idx + 1].diskEntry->format & 0xFFu);
			valueCursor = ((nextValue + alignUnit - 1u) / alignUnit) * alignUnit;
		}

		keyCursor += sfoStringLength(reinterpret_cast<const char *>(writeHandle->entries[idx].key)) + 1u;
	}

	*outKeyTableSize = keyCursor;
}

static int sfoWriteZeroPadding(SfoWriter *writer, SceSize padBytes)
{
	if (padBytes == 0)
		return 0;

	unsigned char zeros[32];
	sfoSetMemory(zeros, 0, sizeof(zeros));

	SceSize fullChunks = padBytes / sizeof(zeros);
	for (SceSize idx = 0; idx < fullChunks; idx++) {
		if (writer->write(writer->context, zeros, sizeof(zeros)) !=
		    static_cast<int>(sizeof(zeros)))
			return SFO_ERR_WRITE_FAILED;
	}

	SceSize remainder = padBytes % sizeof(zeros);
	if (remainder != 0) {
		if (writer->write(writer->context, zeros, remainder) != static_cast<int>(remainder))
			return SFO_ERR_WRITE_FAILED;
	}
	return 0;
}

/* 0x81342F8C */
static int sfoSerialize(SfoWriter *writer, SfoHandle *handle)
{
	if (handle == NULL || (handle->handleFlags & SFO_FLAG_PARSED) != 0)
		return SFO_ERR_INVALID_ARG;

	SfoWriteHandle *writeHandle = reinterpret_cast<SfoWriteHandle *>(handle);
	SceSize keyTableSize = 0;
	sfoComputeLayout(handle, &keyTableSize);

	SceSize alignedKeySize = (keyTableSize + 3u) & ~3u;
	handle->header->keyTableOffset =
		static_cast<SceUInt32>(sizeof(SfoHeader) + handle->header->entryCount * sizeof(SfoDiskEntry));
	handle->header->valueTableOffset = handle->header->keyTableOffset + alignedKeySize;

	if (writer->write(writer->context, handle->header, sizeof(SfoHeader)) !=
	    static_cast<int>(sizeof(SfoHeader)))
		return SFO_ERR_WRITE_FAILED;

	for (SceSize idx = 0; idx < handle->header->entryCount; idx++) {
		if (writer->write(writer->context, writeHandle->entries[idx].diskEntry,
				  sizeof(SfoDiskEntry)) != static_cast<int>(sizeof(SfoDiskEntry)))
			return SFO_ERR_WRITE_FAILED;
	}

	for (SceSize idx = 0; idx < handle->header->entryCount; idx++) {
		const char *keyStr = reinterpret_cast<const char *>(writeHandle->entries[idx].key);
		SceSize keyBytes = sfoStringLength(keyStr) + 1u;
		if (writer->write(writer->context, keyStr, keyBytes) != static_cast<int>(keyBytes))
			return SFO_ERR_WRITE_FAILED;
	}

	int padRet = sfoWriteZeroPadding(writer, alignedKeySize - keyTableSize);
	if (padRet < 0)
		return padRet;

	SceSize count = handle->header->entryCount;
	if (count == 0)
		return 0;

	SceSize writtenValues = 0;
	for (SceSize idx = 0; idx < count; idx++) {
		SfoDiskEntry *disk = writeHandle->entries[idx].diskEntry;
		if (writtenValues < disk->valueOffset) {
			padRet = sfoWriteZeroPadding(writer, disk->valueOffset - writtenValues);
			if (padRet < 0)
				return padRet;
			writtenValues = disk->valueOffset;
		}

		const void *valuePtr = writeHandle->entries[idx].value;
		SceSize valueLen = disk->length;
		if (valuePtr != NULL) {
			if (disk->format == 0x0504u) {
				SceUInt32 rawWord = *static_cast<const SceUInt32 *>(valuePtr);
				unsigned char beBytes[4];
				beBytes[0] = static_cast<unsigned char>((rawWord >> 24) & 0xFFu);
				beBytes[1] = static_cast<unsigned char>((rawWord >> 16) & 0xFFu);
				beBytes[2] = static_cast<unsigned char>((rawWord >> 8) & 0xFFu);
				beBytes[3] = static_cast<unsigned char>(rawWord & 0xFFu);
				if (writer->write(writer->context, beBytes, valueLen) !=
				    static_cast<int>(valueLen))
					return SFO_ERR_WRITE_FAILED;
			} else if (disk->format == 0x0404u) {
				SceUInt32 leWord = *static_cast<const SceUInt32 *>(valuePtr);
				if (writer->write(writer->context, &leWord, valueLen) !=
				    static_cast<int>(valueLen))
					return SFO_ERR_WRITE_FAILED;
			} else {
				if (writer->write(writer->context, valuePtr, valueLen) !=
				    static_cast<int>(valueLen))
					return SFO_ERR_WRITE_FAILED;
			}
			writtenValues += valueLen;
		}
	}

	SfoDiskEntry *lastDisk = writeHandle->entries[count - 1].diskEntry;
	SceSize finalSize = (lastDisk->valueOffset + lastDisk->capacity + 3u) & ~3u;
	if (finalSize > writtenValues) {
		padRet = sfoWriteZeroPadding(writer, finalSize - writtenValues);
		if (padRet < 0)
			return padRet;
	}
	return 0;
}

/* 0x81343398 */
static int sfoBufferWrite(void *context, const void *src, SceSize size)
{
	SfoBufferWriter *bw = static_cast<SfoBufferWriter *>(context);
	if (bw->remaining == 0)
		return -1;

	SceSize copyLen = (bw->remaining < size) ? bw->remaining : size;
	sfoCopyMemory(bw->buffer + bw->offset, src, copyLen);
	bw->offset += copyLen;
	bw->remaining -= copyLen;
	return static_cast<int>(copyLen);
}

/* --- Public SFO API (declared in game_appinfo.h:340-358) --- */

/* 0x81342B6E */
int sfoOpen(const void *sfo, SceSize size, SfoHandle **handle)
{
	if (sfo == NULL)
		return SFO_ERR_INVALID_ARG;

	const SfoHeader *header = static_cast<const SfoHeader *>(sfo);
	if (sfoCompareMemory(header->magic, kSfoMagic, 4) != 0)
		return SFO_ERR_BAD_MAGIC;
	if (header->version != SFO_VERSION_1_1)
		return SFO_ERR_BAD_VERSION;
	if (size < header->keyTableOffset || size < header->valueTableOffset)
		return SFO_ERR_INVALID_ARG;
	if (size < sizeof(SfoHeader) + header->entryCount * sizeof(SfoDiskEntry))
		return SFO_ERR_INVALID_ARG;
	if (header->valueTableOffset < header->keyTableOffset)
		return SFO_ERR_INVALID_ARG;

	const unsigned char *rawBytes = static_cast<const unsigned char *>(sfo);
	const unsigned char *cursor = rawBytes + header->keyTableOffset;
	SceSize keyCount = 0;
	while (keyCount < header->entryCount) {
		unsigned char ch = *cursor;
		if (ch == '\0') {
			keyCount++;
			if (keyCount == header->entryCount)
				break;
			if (static_cast<SceSize>(cursor + 1 - rawBytes) >= size)
				return SFO_ERR_INVALID_ARG;
			cursor++;
			continue;
		}
		if (ch < 33 || ch > 126 || ch == '#')
			return SFO_ERR_INVALID_ARG;
		if (static_cast<SceSize>(cursor + 1 - rawBytes) >= size)
			return SFO_ERR_INVALID_ARG;
		cursor++;
	}

	SceSize keyEndOffset = (static_cast<SceSize>(cursor - rawBytes) + 4u) & ~3u;
	if (keyEndOffset != header->valueTableOffset)
		return SFO_ERR_INVALID_ARG;

	int validRet = sfoValidateEntries(sfo, sizeof(SfoHeader), header, size);
	if (validRet != 0)
		return validRet;
	if (handle == NULL)
		return SFO_ERR_INVALID_ARG;

	*handle = NULL;
	SfoHandle *parsed = sfoAllocHandle(1, 0);
	if (parsed == NULL)
		return SFO_ERR_NO_MEMORY;

	parsed->header = const_cast<SfoHeader *>(header);
	parsed->handleFlags |= SFO_FLAG_SORTED;

	if (sfoCompareMemory(parsed->header->magic, kSfoMagic, 4) != 0) {
		sce_paf_free(parsed);
		return SFO_ERR_BAD_MAGIC;
	}
	if (parsed->header->version != SFO_VERSION_1_1) {
		sce_paf_free(parsed);
		return SFO_ERR_BAD_VERSION;
	}
	if (parsed->header->valueTableOffset < parsed->header->keyTableOffset) {
		sce_paf_free(parsed);
		return SFO_ERR_BAD_HEADER;
	}

	unsigned char *mutableBytes = const_cast<unsigned char *>(rawBytes);
	parsed->diskEntries = reinterpret_cast<SfoDiskEntry *>(mutableBytes + sizeof(SfoHeader));
	parsed->keyTable = mutableBytes + sizeof(SfoHeader) +
			   parsed->header->entryCount * sizeof(SfoDiskEntry);
	parsed->store.valueTable = mutableBytes + parsed->header->valueTableOffset;

	*handle = parsed;
	return 0;
}

/* 0x81342978 */
int sfoClose(SfoHandle *handle)
{
	if (handle == NULL)
		return SFO_ERR_INVALID_ARG;

	if ((handle->handleFlags & SFO_FLAG_PARSED) == 0) {
		SfoWriteHandle *writeHandle = reinterpret_cast<SfoWriteHandle *>(handle);
		for (SceSize idx = 0; idx < handle->store.entryCapacity; idx++)
			sfoRemoveEntry(handle, &writeHandle->entries[idx]);
	}

	sce_paf_free(handle);
	return 0;
}

/* 0x81342606 */
int sfoFindParam(SfoHandle *handle, const char *key, void **param)
{
	*param = NULL;
	if (handle == NULL)
		return SFO_ERR_INVALID_ARG;

	void *found = NULL;
	if ((handle->handleFlags & SFO_FLAG_PARSED) != 0) {
		SfoSearchKey searchKey;
		searchKey.key = reinterpret_cast<const unsigned char *>(key);
		searchKey.keyTable = handle->keyTable;
		found = sfoBinarySearch(&searchKey, handle->diskEntries,
					handle->header->entryCount, sizeof(SfoDiskEntry),
					sfoCompareKeyToDiskEntry);
	} else {
		SfoWriteHandle *writeHandle = reinterpret_cast<SfoWriteHandle *>(handle);
		sfoSortEntries(handle);
		found = sfoBinarySearch(key, writeHandle->entries, handle->header->entryCount,
					sizeof(SfoRuntimeEntry), sfoCompareKeyToRuntimeEntry);
	}

	if (found == NULL)
		return SFO_ERR_NOT_FOUND;

	*param = found;
	return 0;
}

/* 0x81342E4E */
int sfoGetParamInfo(SfoHandle *handle, void *param, const char **key, SceSize *length,
		    SceSize *maxLength)
{
	if (handle == NULL || param == NULL)
		return SFO_ERR_INVALID_ARG;

	SfoDiskEntry *disk = NULL;
	if ((handle->handleFlags & SFO_FLAG_PARSED) != 0) {
		disk = static_cast<SfoDiskEntry *>(param);
		*key = reinterpret_cast<const char *>(handle->keyTable + disk->keyOffset);
	} else {
		SfoRuntimeEntry *entry = static_cast<SfoRuntimeEntry *>(param);
		if (entry->diskEntry == NULL)
			return SFO_ERR_INVALID_ARG;
		disk = entry->diskEntry;
		*key = reinterpret_cast<const char *>(entry->key);
	}

	*length = disk->length;
	*maxLength = disk->capacity;
	return 0;
}

/* 0x813425C4 */
int sfoGetParamValue(SfoHandle *handle, void *param, const void **value)
{
	if (handle == NULL || param == NULL)
		return SFO_ERR_INVALID_ARG;

	*value = NULL;
	if ((handle->handleFlags & SFO_FLAG_PARSED) != 0) {
		SfoDiskEntry *disk = static_cast<SfoDiskEntry *>(param);
		*value = handle->store.valueTable + disk->valueOffset;
		return 0;
	}

	SfoRuntimeEntry *entry = static_cast<SfoRuntimeEntry *>(param);
	if (entry->value == NULL)
		return SFO_ERR_INVALID_ARG;
	*value = entry->value;
	return 0;
}

/* 0x81342E04 */
int sfoCreate(SfoHandle **handle)
{
	if (handle == NULL)
		return SFO_ERR_INVALID_ARG;

	SfoHandle *created = sfoAllocHandle(0, SFO_MAX_WRITE_ENTRIES);
	if (created == NULL)
		return SFO_ERR_NO_MEMORY;

	created->keyTable = reinterpret_cast<unsigned char *>(static_cast<SceIntPtr>(-1));
	created->handleFlags |= SFO_FLAG_DIRTY;
	sfoInitializeHeader(created, NULL, 1, 1);
	*handle = created;
	return 0;
}

/* 0x813426CE */
int sfoAddParam(SfoHandle *handle, const char *key, const void *value, SceUInt16 format,
		SceSize length, SceSize maxLength)
{
	if (handle == NULL || (handle->handleFlags & SFO_FLAG_PARSED) != 0)
		return SFO_ERR_INVALID_ARG;

	SceSize keyLen = sfoStringLength(key);
	if (keyLen == 0 || keyLen > SFO_KEY_MAX_LEN)
		return SFO_ERR_INVALID_ARG;

	SfoWriteHandle *writeHandle = reinterpret_cast<SfoWriteHandle *>(handle);
	sfoSortEntries(handle);

	if (sfoBinarySearch(key, writeHandle->entries, handle->header->entryCount,
			    sizeof(SfoRuntimeEntry), sfoCompareKeyToRuntimeEntry) != NULL)
		return SFO_ERR_DUPLICATE;

	SceSize capacity = handle->store.entryCapacity;
	if (handle->header->entryCount + 1u > capacity)
		return SFO_ERR_NO_MEMORY;

	SfoRuntimeEntry *entrySlot = NULL;
	if (writeHandle->entryHighWater < capacity) {
		entrySlot = &writeHandle->entries[writeHandle->entryHighWater++];
	} else {
		for (SceSize idx = 0; idx < capacity; idx++) {
			if (writeHandle->entries[idx].diskEntry == NULL) {
				entrySlot = &writeHandle->entries[idx];
				break;
			}
		}
	}

	unsigned char *keySlot = NULL;
	if (writeHandle->keyHighWater < capacity) {
		keySlot = writeHandle->keys[writeHandle->keyHighWater++];
	} else {
		for (SceSize idx = 0; idx < capacity; idx++) {
			if (writeHandle->keys[idx][0] == '\0') {
				keySlot = writeHandle->keys[idx];
				break;
			}
		}
	}
	if (keySlot == NULL)
		return SFO_ERR_NO_MEMORY;

	SfoDiskEntry *diskSlot = NULL;
	if (writeHandle->diskHighWater < capacity) {
		diskSlot = &handle->diskEntries[writeHandle->diskHighWater++];
	} else {
		for (SceSize idx = 0; idx < capacity; idx++) {
			if (handle->diskEntries[idx].keyOffset == SFO_DISK_KEY_UNUSED) {
				diskSlot = &handle->diskEntries[idx];
				break;
			}
		}
	}
	if (diskSlot == NULL) {
		keySlot[0] = '\0';
		return SFO_ERR_NO_MEMORY;
	}

	sfoCopyMemory(keySlot, key, keyLen);
	sfoSetMemory(diskSlot, 0, sizeof(SfoDiskEntry));

	if (entrySlot == NULL)
		return SFO_ERR_NO_MEMORY;

	entrySlot->diskEntry = diskSlot;
	entrySlot->key = keySlot;
	entrySlot->entryFlags = SFO_ENTRY_FLAG_MODIFIED | SFO_ENTRY_FLAG_ALLOCATED;
	handle->handleFlags &= ~SFO_FLAG_SORTED;
	handle->header->entryCount++;
	handle->handleFlags |= SFO_FLAG_DIRTY;

	diskSlot->keyOffset = 0;
	diskSlot->format = format;
	diskSlot->length = length;
	diskSlot->capacity = (maxLength + 3u) & ~3u;
	diskSlot->valueOffset = 0;
	entrySlot->value = value;
	return 0;
}

/* 0x81342D82 */
int sfoSetParamValue(SfoHandle *handle, void *param, const void *value, SceSize length)
{
	if (handle == NULL || param == NULL)
		return SFO_ERR_INVALID_ARG;
	if ((handle->handleFlags & SFO_FLAG_READONLY) != 0)
		return SFO_ERR_INVALID_ARG;

	if ((handle->handleFlags & SFO_FLAG_PARSED) != 0) {
		SfoDiskEntry *disk = static_cast<SfoDiskEntry *>(param);
		if (length > disk->capacity)
			return SFO_ERR_INVALID_ARG;
		sfoCopyMemory(handle->store.valueTable + disk->valueOffset, value, length);
		disk->length = length;
	} else {
		SfoRuntimeEntry *entry = static_cast<SfoRuntimeEntry *>(param);
		if (length > entry->diskEntry->capacity)
			return SFO_ERR_INVALID_ARG;
		sfoCopyMemory(const_cast<void *>(entry->value), value, length);
		entry->diskEntry->length = length;
		entry->entryFlags |= SFO_ENTRY_FLAG_MODIFIED;
	}
	return 0;
}

/* 0x8134331E */
int sfoGetSize(SfoHandle *handle, SceSize *size)
{
	if (handle == NULL || (handle->handleFlags & SFO_FLAG_PARSED) != 0)
		return SFO_ERR_INVALID_ARG;

	SfoWriteHandle *writeHandle = reinterpret_cast<SfoWriteHandle *>(handle);
	SceSize keyTableSize = 0;
	sfoComputeLayout(handle, &keyTableSize);

	SceSize count = handle->header->entryCount;
	SfoDiskEntry *lastDisk = writeHandle->entries[count - 1].diskEntry;
	SceSize alignedKeySize = (keyTableSize + 3u) & ~3u;
	SceSize alignedValues = (lastDisk->valueOffset + lastDisk->capacity + 3u) & ~3u;

	*size = sizeof(SfoHeader) + count * sizeof(SfoDiskEntry) + alignedKeySize + alignedValues;
	return 0;
}

/* 0x813432DE */
int sfoWrite(void *buf, SceSize size, SfoHandle *handle)
{
	SfoBufferWriter bufferWriter;
	bufferWriter.buffer = static_cast<unsigned char *>(buf);
	bufferWriter.offset = 0;
	bufferWriter.remaining = size;

	SfoWriter writer;
	writer.write = sfoBufferWrite;
	writer.context = &bufferWriter;

	return sfoSerialize(&writer, handle);
}
