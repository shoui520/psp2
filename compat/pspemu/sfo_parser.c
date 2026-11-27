/*
 * sfo_parser.c: ScePspemu PARAM.SFO (PSF) parser linked from Sony's libpsf
 * (0x81030e3c..0x810314af).
 */
#include "host_common.h"

#include <stdlib.h>

#define SFO_ERR_NO_MEMORY       ((int)0x80120001)
#define SFO_ERR_INVALID_ARG     ((int)0x80120003)
#define SFO_ERR_BAD_MAGIC       ((int)0x80120004)
#define SFO_ERR_NOT_FOUND       ((int)0x80120005)
#define SFO_ERR_BAD_VERSION     ((int)0x80120007)
#define SFO_ERR_BAD_OFFSETS     ((int)0x80120009)

#define SFO_FLAG_READONLY       2U
#define SFO_FLAG_SORTED         4U
#define SFO_FLAG_DIRTY          0x40U
#define SFO_ENTRY_OWNED         2U

typedef struct SfoDiskHeader {
	uint8_t magic[4];
	uint32_t version;
	uint32_t keyTableOffset;
	uint32_t valueTableOffset;
	uint32_t entryCount;
} SfoDiskHeader;

typedef struct SfoDiskEntry {
	uint16_t keyOffset;
	uint16_t dataFmt;
	uint32_t dataLen;
	uint32_t dataMaxLen;
	uint32_t dataOffset;
} SfoDiskEntry;

typedef struct SfoMutableEntry {
	SfoDiskEntry *diskEntry;
	char *key;
	void *value;
	uint32_t flags;
} SfoMutableEntry;

typedef struct SfoSearchKey {
	const char *key;
	const uint8_t *keyTable;
} SfoSearchKey;

typedef struct SfoContext {
	uint32_t flags;
	SfoDiskHeader *header;
	SfoDiskEntry *diskEntries;
	const uint8_t *keyTable;
	union {
		const uint8_t *valueTable;
		uint32_t entryCapacity;
	} storage;
	uint32_t entryHighWater;
	uint32_t reservedA;
	uint32_t reservedB;
	SfoDiskHeader inlineHeader;
	SfoMutableEntry *entries;
	char (*keys)[32];
} SfoContext;

_Static_assert(sizeof(SfoDiskHeader) == 20U, "SfoDiskHeader size");
_Static_assert(sizeof(SfoDiskEntry) == 16U, "SfoDiskEntry size");
_Static_assert(sizeof(SfoMutableEntry) == 16U, "SfoMutableEntry size");
_Static_assert(offsetof(SfoContext, inlineHeader) == 32U, "inlineHeader offset");
_Static_assert(offsetof(SfoContext, entries) == 52U, "entries offset");
_Static_assert(offsetof(SfoContext, keys) == 56U, "keys offset");

static const uint8_t g_sfoMagic[4] = { 0x00U, 'P', 'S', 'F' };

static void *sfoMemset(void *dst, uint8_t value, size_t size);
static int sfoMemcmp(const void *lhs, const void *rhs, size_t size);
static int sfoStrcmp(const char *lhs, const char *rhs);
static void *sfoBinarySearch(const void *key, const void *base, size_t count, size_t stride,
                             int (*cmp)(const void *, const void *));
static void sfoShellSort(void *base, size_t count, size_t stride,
                         int (*cmp)(const void *, const void *));

/* 0x81030e3c */
static int sfoCompareMutableEntries(const void *lhsPtr, const void *rhsPtr)
{
	const SfoMutableEntry *lhs = (const SfoMutableEntry *)lhsPtr;
	const SfoMutableEntry *rhs = (const SfoMutableEntry *)rhsPtr;

	if (lhs->key == NULL)
		return 1;
	if (rhs->key != NULL)
		return sfoStrcmp(lhs->key, rhs->key);
	return -1;
}

/* 0x81030e5c */
static int sfoCompareDiskKey(const void *keyPtr, const void *entryPtr)
{
	const SfoSearchKey *search = (const SfoSearchKey *)keyPtr;
	const SfoDiskEntry *entry = (const SfoDiskEntry *)entryPtr;

	return sfoStrcmp(search->key, (const char *)&search->keyTable[entry->keyOffset]);
}

/* 0x81030e6c */
static int sfoCompareMutableKey(const void *keyPtr, const void *entryPtr)
{
	const char *key = (const char *)keyPtr;
	const SfoMutableEntry *entry = (const SfoMutableEntry *)entryPtr;

	return sfoStrcmp(key, entry->key);
}

/* 0x81030e78 */
static void __attribute__((noinline, noclone)) sfoSortMutableEntries(SfoContext *ctx)
{
	if ((ctx->flags & (SFO_FLAG_READONLY | SFO_FLAG_SORTED)) == 0U) {
		sfoShellSort(ctx->entries, ctx->entryHighWater, sizeof(SfoMutableEntry),
		             sfoCompareMutableEntries);
		ctx->flags |= SFO_FLAG_SORTED;
		ctx->entryHighWater = ctx->header->entryCount;
	}
}

/* 0x81030ea8 */
static void *__attribute__((noinline, noclone))
sfoFindMutableEntry(SfoContext *ctx, const char *key)
{
	sfoSortMutableEntries(ctx);
	return sfoBinarySearch(key, ctx->entries, ctx->header->entryCount,
	                       sizeof(SfoMutableEntry), sfoCompareMutableKey);
}

/* 0x81030ed0 */
int sfoGetEntryValue(void *handle, void *entryObj, char **outValue)
{
	SfoContext *ctx = (SfoContext *)handle;

	if (ctx != NULL && entryObj != NULL) {
		*outValue = NULL;
		if ((ctx->flags & SFO_FLAG_READONLY) != 0U) {
			const SfoDiskEntry *disk = (const SfoDiskEntry *)entryObj;
			*outValue = (char *)(uintptr_t)&ctx->storage.valueTable[disk->dataOffset];
			return 0;
		}
		{
			const SfoMutableEntry *entry = (const SfoMutableEntry *)entryObj;
			if (entry->value != NULL) {
				*outValue = (char *)entry->value;
				return 0;
			}
		}
	}
	return SFO_ERR_INVALID_ARG;
}

/* 0x81030f04 */
int sfoFindKey(void *handle, const char *key, void **outEntry)
{
	SfoContext *ctx = (SfoContext *)handle;
	void *found;

	*outEntry = NULL;
	if (ctx == NULL)
		return SFO_ERR_INVALID_ARG;

	if ((ctx->flags & SFO_FLAG_READONLY) != 0U) {
		SfoSearchKey search;
		search.key = key;
		search.keyTable = ctx->keyTable;
		found = sfoBinarySearch(&search, ctx->diskEntries, ctx->header->entryCount,
		                        sizeof(SfoDiskEntry), sfoCompareDiskKey);
	} else {
		found = sfoFindMutableEntry(ctx, key);
	}

	if (found != NULL) {
		*outEntry = found;
		return 0;
	}
	return SFO_ERR_NOT_FOUND;
}

/* 0x81030f80 */
static void __attribute__((noinline, noclone))
sfoRemoveMutableEntry(SfoContext *ctx, SfoMutableEntry *entry)
{
	if (ctx == NULL || entry == NULL)
		return;
	if ((ctx->flags & SFO_FLAG_READONLY) != 0U)
		return;

	if (entry->diskEntry != NULL && (entry->flags & SFO_ENTRY_OWNED) != 0U) {
		if (entry->key != NULL) {
			entry->key[0] = '\0';
			entry->key = NULL;
		}
		entry->diskEntry->keyOffset = 0xFFFFU;
		entry->diskEntry = NULL;
	}

	entry->value = NULL;
	entry->flags = 0U;
	ctx->header->entryCount--;
	ctx->flags &= ~SFO_FLAG_SORTED;
	ctx->flags |= SFO_FLAG_DIRTY;
}

/* 0x81030fec */
static void __attribute__((noinline, noclone)) sfoBindMutableEntries(SfoContext *ctx)
{
	uint32_t count = ctx->header->entryCount;
	uint32_t idx;

	if ((ctx->flags & SFO_FLAG_READONLY) != 0U)
		return;

	for (idx = 0; idx < count; idx++) {
		ctx->entries[idx].key = ctx->keys[idx];
		ctx->entries[idx].diskEntry = &ctx->diskEntries[idx];
	}
}

/* 0x81031020 */
static int __attribute__((noinline, noclone))
sfoValidateEntries(const uint8_t *base, size_t entriesOffset,
                   const SfoDiskHeader *header, uint32_t totalSize)
{
	const SfoDiskEntry *entries = (const SfoDiskEntry *)(const void *)&base[entriesOffset];
	uint32_t count = header->entryCount;
	uint32_t keyBase = header->keyTableOffset;
	uint32_t valBase = header->valueTableOffset;
	uint32_t idx;

	if (count == 0U)
		return 0;

	for (idx = 0; idx < count; idx++) {
		uint16_t fmt = entries[idx].dataFmt;
		uint32_t endOff;

		if (keyBase + entries[idx].keyOffset >= valBase)
			return SFO_ERR_INVALID_ARG;

		switch (fmt) {
		case 0x0000U:
		case 0x0004U:
		case 0x0008U:
		case 0x0010U:
		case 0x0020U:
		case 0x0100U:
		case 0x0104U:
		case 0x0108U:
		case 0x0110U:
		case 0x0120U:
		case 0x0204U:
		case 0x0304U:
		case 0x0404U:
		case 0x0504U:
			break;
		default:
			return SFO_ERR_INVALID_ARG;
		}

		if (entries[idx].dataMaxLen < entries[idx].dataLen)
			return SFO_ERR_INVALID_ARG;

		endOff = entries[idx].dataOffset + entries[idx].dataMaxLen;
		if (endOff > totalSize)
			return SFO_ERR_INVALID_ARG;

		if (idx == count - 1U) {
			if (endOff + valBase != totalSize)
				return SFO_ERR_INVALID_ARG;
		} else {
			if (endOff != entries[idx + 1U].dataOffset)
				return SFO_ERR_INVALID_ARG;
		}
	}

	return 0;
}

/* 0x810310f8 */
static int __attribute__((noinline, noclone))
sfoValidateHeader(const SfoDiskHeader *header, uint32_t totalSize)
{
	if (sfoMemcmp(header->magic, g_sfoMagic, sizeof(g_sfoMagic)) != 0)
		return SFO_ERR_BAD_MAGIC;
	if (header->version != 0x0101U)
		return SFO_ERR_BAD_VERSION;
	if (header->keyTableOffset > totalSize ||
	    header->valueTableOffset > totalSize ||
	    header->entryCount * sizeof(SfoDiskEntry) + sizeof(SfoDiskHeader) > totalSize ||
	    header->keyTableOffset > header->valueTableOffset) {
		return SFO_ERR_INVALID_ARG;
	}
	return 0;
}

/* 0x81031158 */
static SfoContext *__attribute__((noinline, noclone))
sfoAllocContext(int readOnly, uint32_t capacity)
{
	size_t allocSize = (readOnly == 0) ? (size_t)(capacity * 64U + 60U) : 20U;
	SfoContext *ctx = (SfoContext *)malloc(allocSize);

	if (ctx == NULL)
		return NULL;
	sfoMemset(ctx, 0U, allocSize);

	if (readOnly == 0) {
		uint8_t *raw = (uint8_t *)ctx;
		uint32_t idx;

		ctx->header = &ctx->inlineHeader;
		ctx->storage.entryCapacity = capacity;
		ctx->entries = (SfoMutableEntry *)(void *)&raw[60];
		ctx->diskEntries = (SfoDiskEntry *)(void *)&raw[60U + capacity * 16U];
		ctx->keys = (char (*)[32])(void *)&raw[60U + capacity * 32U];
		for (idx = 0; idx < capacity; idx++)
			ctx->diskEntries[idx].keyOffset = 0xFFFFU;
	} else {
		ctx->flags |= SFO_FLAG_READONLY;
	}

	return ctx;
}

/* 0x810311c8 */
int sfoDestroy(void *handle)
{
	SfoContext *ctx = (SfoContext *)handle;

	if (ctx == NULL)
		return SFO_ERR_INVALID_ARG;

	if ((ctx->flags & SFO_FLAG_READONLY) == 0U) {
		uint32_t idx;
		for (idx = 0; idx < ctx->storage.entryCapacity; idx++)
			sfoRemoveMutableEntry(ctx, &ctx->entries[idx]);
	}

	free(ctx);
	return 0;
}

/* 0x81031200 */
int sfoParseBuffer(const void *buf, uint32_t size, void **outHandle)
{
	const uint8_t *bytes = (const uint8_t *)buf;
	const SfoDiskHeader *hdr = (const SfoDiskHeader *)buf;
	const uint8_t *cursor;
	uint32_t keyTerminators = 0U;
	SfoContext *ctx;
	int ret;

	if (buf == NULL)
		return SFO_ERR_INVALID_ARG;

	ret = sfoValidateHeader(hdr, size);
	if (ret != 0)
		return ret;

	cursor = &bytes[hdr->keyTableOffset];
	for (;;) {
		uint8_t ch = *cursor++;
		if (ch != 0U) {
			if ((uint32_t)(ch - 33U) > 0x5DU || ch == '#')
				return SFO_ERR_INVALID_ARG;
		} else {
			keyTerminators++;
		}
		if (keyTerminators == hdr->entryCount)
			break;
		if ((uint32_t)(cursor - bytes) >= size)
			return SFO_ERR_INVALID_ARG;
	}

	if ((((uintptr_t)cursor + 3U) & ~3U) != (uintptr_t)&bytes[hdr->valueTableOffset])
		return SFO_ERR_INVALID_ARG;

	ret = sfoValidateEntries(bytes, sizeof(SfoDiskHeader), hdr, size);
	if (ret != 0)
		return ret;
	if (outHandle == NULL)
		return SFO_ERR_INVALID_ARG;

	*outHandle = NULL;
	ctx = sfoAllocContext(1, 0U);
	if (ctx == NULL)
		return SFO_ERR_NO_MEMORY;

	ctx->header = (SfoDiskHeader *)(uintptr_t)hdr;
	ctx->flags |= SFO_FLAG_SORTED;

	if (sfoMemcmp(hdr->magic, g_sfoMagic, sizeof(g_sfoMagic)) != 0) {
		ret = SFO_ERR_BAD_MAGIC;
	} else if (hdr->version != 0x0101U) {
		ret = SFO_ERR_BAD_VERSION;
	} else if (hdr->valueTableOffset < hdr->keyTableOffset) {
		ret = SFO_ERR_BAD_OFFSETS;
	} else {
		ctx->diskEntries = (SfoDiskEntry *)(uintptr_t)&bytes[sizeof(SfoDiskHeader)];
		ctx->storage.valueTable = &bytes[hdr->valueTableOffset];
		ctx->keyTable = (const uint8_t *)&ctx->diskEntries[hdr->entryCount];
		sfoBindMutableEntries(ctx);
		*outHandle = ctx;
		return 0;
	}

	sfoDestroy(ctx);
	return ret;
}

/* 0x81031300 */
int sfoGetEntryMeta(void *handle, void *entryObj, const char **outKey,
                    size_t *outUsedSize, size_t *outMaxSize)
{
	SfoContext *ctx = (SfoContext *)handle;

	if (ctx != NULL && entryObj != NULL) {
		const SfoDiskEntry *disk;

		if ((ctx->flags & SFO_FLAG_READONLY) != 0U) {
			disk = (const SfoDiskEntry *)entryObj;
			*outKey = (const char *)&ctx->keyTable[disk->keyOffset];
		} else {
			const SfoMutableEntry *entry = (const SfoMutableEntry *)entryObj;
			if (entry->diskEntry == NULL)
				return SFO_ERR_INVALID_ARG;
			*outKey = entry->key;
			disk = entry->diskEntry;
		}
		*outUsedSize = disk->dataLen;
		*outMaxSize = disk->dataMaxLen;
		return 0;
	}
	return SFO_ERR_INVALID_ARG;
}

/* 0x81031338 */
static void *__attribute__((noinline, noclone))
sfoMemset(void *dst, uint8_t value, size_t size)
{
	uint8_t *out = (uint8_t *)dst;
	size_t idx;

	for (idx = 0; idx < size; idx++)
		out[idx] = value;
	return dst;
}

/* 0x8103134c */
static int __attribute__((noinline, noclone))
sfoMemcmp(const void *lhs, const void *rhs, size_t size)
{
	const signed char *leftBytes = (const signed char *)lhs;
	const signed char *rightBytes = (const signed char *)rhs;
	size_t idx;

	for (idx = 0; idx < size; idx++) {
		if (leftBytes[idx] > rightBytes[idx])
			return 1;
		if (leftBytes[idx] < rightBytes[idx])
			return -1;
	}
	return 0;
}

/* 0x81031390 */
static int __attribute__((noinline, noclone))
sfoStrcmp(const char *lhs, const char *rhs)
{
	const uint8_t *left = (const uint8_t *)lhs;
	const uint8_t *right = (const uint8_t *)rhs;

	for (;;) {
		int diff = (int)*left - (int)*right;
		if (diff != 0 || *left == 0U)
			return diff;
		left++;
		right++;
	}
}

/* 0x810313bc */
static void *__attribute__((noinline, noclone))
sfoBinarySearch(const void *key, const void *base, size_t count, size_t stride,
                int (*cmp)(const void *, const void *))
{
	const uint8_t *table = (const uint8_t *)base;
	size_t low = 0U;
	size_t high;

	if (count == 0U)
		return NULL;
	high = count - 1U;

	while (low <= high) {
		size_t mid = (low + high) >> 1;
		const void *elem = &table[mid * stride];
		int order = cmp(key, elem);

		if (order == 0)
			return (void *)(uintptr_t)elem;
		if (low == high)
			return NULL;
		if (order < 0) {
			if (mid == 0U)
				return NULL;
			high = mid - 1U;
		} else {
			low = mid + 1U;
		}
	}
	return NULL;
}

/* 0x8103140c */
static void __attribute__((noinline, noclone))
sfoShellSort(void *base, size_t count, size_t stride,
             int (*cmp)(const void *, const void *))
{
	uint8_t *table = (uint8_t *)base;
	size_t gap;

	for (gap = count >> 1; gap != 0U; gap >>= 1) {
		size_t idx;
		for (idx = gap; idx < count; idx++) {
			int cur = (int)(idx - gap);
			while (cur >= 0) {
				uint8_t *left = &table[(size_t)cur * stride];
				uint8_t *right = &table[((size_t)cur + gap) * stride];
				size_t byteIdx;

				if (cmp(left, right) <= 0)
					break;
				for (byteIdx = 0; byteIdx < stride; byteIdx++) {
					uint8_t swap = left[byteIdx];
					left[byteIdx] = right[byteIdx];
					right[byteIdx] = swap;
				}
				cur -= (int)gap;
			}
		}
	}
}
