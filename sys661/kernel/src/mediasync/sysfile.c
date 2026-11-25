/*
 * ePSP 6.61 libSysFile (linked into mediasync.prx @ 0x2824..0x3933)
 * Reimplemented from 3.650.011-TOOL pcff/elf/kd/mediasync.prx
 */

#include "sysfile.h"

extern void *sceKernelMemcpy(void *dst, const void *src, SceSize size);
extern void *sceKernelMemset(void *dst, int val, SceSize size);
extern int64_t sceIoLseek(SceUID fileDesc, int64_t offset, int whence);
extern int sceIoRead(SceUID fileDesc, void *data, SceSize size);

static const char g_psfMagic[4] = { '\0', 'P', 'S', 'F' };

static SceSysfileMallocFn g_sfMallocFn;
static SceSysfileFreeFn g_sfFreeFn;

/* === 0x2824 sceSystemFileGetValue === */
__attribute__((noinline)) int sceSystemFileGetValue(SceSysfileContext *ctx, const void *entryHandle, void *outBuf)
{
	if (ctx == NULL || entryHandle == NULL) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	if ((ctx->flags & 2u) != 0) {
		const SceSysfileIndexEntry *roEntry = (const SceSysfileIndexEntry *)entryHandle;
		sceKernelMemcpy(outBuf, ctx->dataTable + roEntry->dataOffset, roEntry->usedSize);
		return 0;
	}

	const SceSysfileRwSlot *slot = (const SceSysfileRwSlot *)entryHandle;
	if (slot->entry == NULL) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	if (slot->dataBuf != NULL) {
		sceKernelMemcpy(outBuf, slot->dataBuf, slot->entry->usedSize);
		return 0;
	}
	if (ctx->fileDesc < 0) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	int64_t pos = sceIoLseek(ctx->fileDesc, (int64_t)(uint64_t)(ctx->header->dataTableOffset + slot->entry->dataOffset), 0);
	if (pos == -1) {
		return SCE_SYSFILE_ERROR_IO;
	}
	int n = sceIoRead(ctx->fileDesc, outBuf, slot->entry->usedSize);
	if ((uint32_t)n != slot->entry->usedSize) {
		return SCE_SYSFILE_ERROR_IO;
	}
	return 0;
}

/* === 0x2980 sceSystemFileGetString === */
__attribute__((noinline)) int sceSystemFileGetString(SceSysfileContext *ctx, const void *entryHandle, void *outBuf, uint32_t outSize)
{
	uint32_t needed;
	if (ctx == NULL || entryHandle == NULL) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	if ((ctx->flags & 2u) != 0) {
		const SceSysfileIndexEntry *roEntry = (const SceSysfileIndexEntry *)entryHandle;
		needed = roEntry->usedSize;
	} else {
		const SceSysfileRwSlot *slot = (const SceSysfileRwSlot *)entryHandle;
		if (slot->entry == NULL) {
			return SCE_SYSFILE_ERROR_EINVAL;
		}
		needed = slot->entry->usedSize;
	}
	if (outSize < needed) {
		return SCE_SYSFILE_ERROR_BUF_TOO_SMALL;
	}
	return sceSystemFileGetValue(ctx, entryHandle, outBuf);
}

/* === 0x2a14 sceSystemFileLoadReadOnly === */
__attribute__((noinline)) int sceSystemFileLoadReadOnly(void *buf, SceSysfileContext **outCtx)
{
	if (outCtx == NULL) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	*outCtx = NULL;
	SceSysfileContext *ctx = sysfileAllocContext(1, 0);
	if (ctx == NULL) {
		return SCE_SYSFILE_ERROR_NOMEM;
	}

	ctx->flags |= 4u;
	ctx->header = (SceSysfileHeader *)buf;

	int err = SCE_SYSFILE_ERROR_BAD_MAGIC;
	if (sysfileMemcmp((const char *)ctx->header, g_psfMagic, 4) == 0) {
		err = SCE_SYSFILE_ERROR_BAD_VERSION;
		if (ctx->header->version == 0x00000101u) {
			err = SCE_SYSFILE_ERROR_EINVAL;
			if (ctx->header->dataTableOffset >= ctx->header->keyTableOffset) {
				uint8_t *base = (uint8_t *)buf;
				SceSysfileIndexEntry *idx = (SceSysfileIndexEntry *)(base + sizeof(SceSysfileHeader));
				ctx->keyTable = (char *)(base + sizeof(SceSysfileHeader) + ctx->header->numEntries * sizeof(SceSysfileIndexEntry));
				ctx->indexTable = idx;
				ctx->dataTable = (uint8_t *)ctx->header + ctx->header->dataTableOffset;
				sysfileInitSlotTable(ctx);
				*outCtx = ctx;
				return 0;
			}
		}
	}
	sceSystemFileExit(ctx);
	return err;
}

/* === 0x2bd0 sceSystemFileLoadAll2 === */
__attribute__((noinline)) int sceSystemFileLoadAll2(const void *buf, uint32_t size, SceSysfileContext **outCtx)
{
	if (buf == NULL) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	const SceSysfileHeader *hdr = (const SceSysfileHeader *)buf;
	int res = sysfileCheckHeader(hdr, size);
	if (res != 0) {
		return res;
	}
	res = sysfileCheckKeyTable((const uint8_t *)buf, hdr->keyTableOffset, hdr, size);
	if (res != 0) {
		return res;
	}
	res = sysfileCheckIndexTable((const uint8_t *)buf, sizeof(SceSysfileHeader), hdr, size);
	if (res != 0) {
		return res;
	}
	return sceSystemFileLoadReadOnly((void *)buf, outCtx);
}

/* === 0x2c80 sceSystemFileExit === */
__attribute__((noinline)) int sceSystemFileExit(SceSysfileContext *ctx)
{
	if (ctx == NULL) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	if ((ctx->flags & 2u) == 0) {
		for (uint32_t i = 0; i < ctx->maxEntries; i++) {
			sysfileClearRwSlot(ctx, &ctx->slots[i]);
		}
	}
	g_sfFreeFn(ctx);
	return 0;
}

/* === 0x2d24 sysfileAllocContext === */
__attribute__((noinline)) SceSysfileContext *sysfileAllocContext(int readOnly, uint32_t maxEntries)
{
	uint32_t allocSize = 20u;
	if (!readOnly) {
		if (maxEntries > 0x40000u) {
			return NULL;
		}
		allocSize = (maxEntries << 6) + 60u;
	}
	SceSysfileContext *ctx = (SceSysfileContext *)g_sfMallocFn(allocSize);
	if (ctx == NULL) {
		return NULL;
	}
	sceKernelMemset(ctx, 0, allocSize);
	if (!readOnly) {
		uint8_t *base = (uint8_t *)ctx;
		ctx->header = (SceSysfileHeader *)(base + 32);
		ctx->maxEntries = maxEntries;
		ctx->slots = (SceSysfileRwSlot *)(base + 60);
		ctx->indexTable = (SceSysfileIndexEntry *)((uint8_t *)ctx->slots + maxEntries * sizeof(SceSysfileRwSlot));
		for (uint32_t i = 0; i < maxEntries; i++) {
			ctx->indexTable[i].keyOffset = 0xffffu;
		}
		ctx->keyStorage = (char *)((uint8_t *)ctx->indexTable + maxEntries * sizeof(SceSysfileIndexEntry));
	} else {
		ctx->flags |= 2u;
	}
	return ctx;
}

/* === 0x2ed4 sysfileInitSlotTable === */
__attribute__((noinline)) void sysfileInitSlotTable(SceSysfileContext *ctx)
{
	for (uint32_t i = 0; i < ctx->header->numEntries; i++) {
		if ((ctx->flags & 2u) == 0) {
			ctx->slots[i].entry = &ctx->indexTable[i];
			ctx->slots[i].key = ctx->keyStorage + 32u * i;
		}
	}
}

/* === 0x2f8c sysfileCheckHeader === */
__attribute__((noinline)) int sysfileCheckHeader(const SceSysfileHeader *hdr, uint32_t size)
{
	if (sysfileMemcmp((const char *)hdr, g_psfMagic, 4) != 0) {
		return SCE_SYSFILE_ERROR_BAD_MAGIC;
	}
	if (hdr->version != 0x00000101u) {
		return SCE_SYSFILE_ERROR_BAD_VERSION;
	}
	if (size < hdr->keyTableOffset) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	if (size < hdr->dataTableOffset) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	if (size < hdr->numEntries * sizeof(SceSysfileIndexEntry) + sizeof(SceSysfileHeader)) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	if (hdr->dataTableOffset < hdr->keyTableOffset) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	return 0;
}

/* === 0x3054 sysfileCheckKeyTable === */
__attribute__((noinline)) int sysfileCheckKeyTable(const uint8_t *buf, uint32_t keyOff, const SceSysfileHeader *hdr, uint32_t size)
{
	const char *cur = (const char *)(buf + keyOff);
	uint32_t count = 0;
	while (1) {
		char ch = *cur;
		if (ch != '\0') {
			if (ch < 33 || ch == 0x7f || ch == '#') {
				return SCE_SYSFILE_ERROR_EINVAL;
			}
			cur++;
		} else {
			count++;
			cur++;
		}
		if (hdr->numEntries == count) {
			break;
		}
		if ((uint32_t)(cur - (const char *)buf) >= size) {
			return SCE_SYSFILE_ERROR_EINVAL;
		}
	}
	if ((((uintptr_t)cur + 3u) & ~3u) == (uintptr_t)(buf + hdr->dataTableOffset)) {
		return 0;
	}
	return SCE_SYSFILE_ERROR_EINVAL;
}

/* === 0x30f4 sysfileCheckIndexTable === */
__attribute__((noinline)) int sysfileCheckIndexTable(const uint8_t *buf, uint32_t indexOff, const SceSysfileHeader *hdr, uint32_t size)
{
	const SceSysfileIndexEntry *entry = (const SceSysfileIndexEntry *)(buf + indexOff);
	uint32_t numEntries = hdr->numEntries;
	if (numEntries == 0) {
		return 0;
	}
	uint32_t keyOff = hdr->keyTableOffset;
	uint32_t dataOff = hdr->dataTableOffset;
	for (uint32_t i = 0; i < numEntries; i++, entry++) {
		if (keyOff + entry->keyOffset >= dataOff) {
			return SCE_SYSFILE_ERROR_EINVAL;
		}
		switch (entry->dataFmt) {
		case 0x0000:
		case 0x0004:
		case 0x0008:
		case 0x0010:
		case 0x0020:
		case 0x0100:
		case 0x0104:
		case 0x0108:
		case 0x0110:
		case 0x0120:
		case 0x0204:
		case 0x0304:
		case 0x0404:
		case 0x0504:
			break;
		default:
			return SCE_SYSFILE_ERROR_EINVAL;
		}
		if (entry->totalSize < entry->usedSize) {
			return SCE_SYSFILE_ERROR_EINVAL;
		}
		if (size < entry->dataOffset + entry->totalSize) {
			return SCE_SYSFILE_ERROR_EINVAL;
		}
		if (i == numEntries - 1u) {
			if (dataOff + entry->dataOffset + entry->totalSize != size) {
				return SCE_SYSFILE_ERROR_EINVAL;
			}
		} else {
			if (entry->dataOffset + entry->totalSize != entry[1].dataOffset) {
				return SCE_SYSFILE_ERROR_EINVAL;
			}
		}
	}
	return 0;
}

/* === 0x3340 sceSystemFileGetIndex === */
__attribute__((noinline)) int sceSystemFileGetIndex(SceSysfileContext *ctx, const char *key, void **outEntry)
{
	SceSysfileRoSearchKey roKey;
	void *found;

	*outEntry = NULL;
	if (ctx == NULL) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	if ((ctx->flags & 2u) != 0) {
		roKey.key = key;
		roKey.keyTable = ctx->keyTable;
		found = sysfileBsearch(&roKey, ctx->indexTable, ctx->header->numEntries, sizeof(SceSysfileIndexEntry), sysfileCompareKeyToRoEntry);
	} else {
		if ((ctx->flags & 4u) == 0) {
			sysfileShellSort(ctx->slots, ctx->sortedCount, sizeof(SceSysfileRwSlot), sysfileCompareRwSlot);
			ctx->flags |= 4u;
			ctx->sortedCount = ctx->header->numEntries;
		}
		found = sysfileBsearch(key, ctx->slots, ctx->header->numEntries, sizeof(SceSysfileRwSlot), sysfileCompareKeyToRwSlot);
	}
	if (found == NULL) {
		return SCE_SYSFILE_ERROR_NOT_FOUND;
	}
	*outEntry = found;
	return 0;
}

/* === 0x3494 sysfileClearRwSlot === */
__attribute__((noinline)) void sysfileClearRwSlot(SceSysfileContext *ctx, SceSysfileRwSlot *slot)
{
	if (ctx == NULL || slot == NULL || (ctx->flags & 2u) != 0) {
		return;
	}
	if (slot->entry != NULL && (slot->slotFlags & 2u) != 0) {
		if (slot->key != NULL) {
			slot->key[0] = '\0';
			slot->key = NULL;
		}
		slot->entry->keyOffset = 0xffffu;
		slot->entry = NULL;
	}
	slot->dataBuf = NULL;
	slot->slotFlags = 0;
	ctx->header->numEntries--;
	if ((ctx->flags & 4u) != 0) {
		ctx->flags ^= 4u;
	}
	ctx->flags |= 0x40u;
}

/* === 0x35e4 sysfileCompareRwSlot === */
__attribute__((noinline)) int sysfileCompareRwSlot(const void *lhs, const void *rhs)
{
	const SceSysfileRwSlot *left = (const SceSysfileRwSlot *)lhs;
	const SceSysfileRwSlot *right = (const SceSysfileRwSlot *)rhs;
	if (left->key == NULL) {
		return 1;
	}
	if (right->key == NULL) {
		return -1;
	}
	return sysfileStrcmp((const uint8_t *)left->key, (const uint8_t *)right->key);
}

/* === 0x3628 sysfileCompareKeyToRwSlot === */
__attribute__((noinline)) int sysfileCompareKeyToRwSlot(const void *key, const void *slot)
{
	const SceSysfileRwSlot *rwSlot = (const SceSysfileRwSlot *)slot;
	return sysfileStrcmp((const uint8_t *)key, (const uint8_t *)rwSlot->key);
}

/* === 0x364c sysfileCompareKeyToRoEntry === */
__attribute__((noinline)) int sysfileCompareKeyToRoEntry(const void *searchKey, const void *entry)
{
	const SceSysfileRoSearchKey *sk = (const SceSysfileRoSearchKey *)searchKey;
	const SceSysfileIndexEntry *roEntry = (const SceSysfileIndexEntry *)entry;
	return sysfileStrcmp((const uint8_t *)sk->key, (const uint8_t *)(sk->keyTable + roEntry->keyOffset));
}

/* === 0x3680 sceSystemFileSetAllocator === */
__attribute__((noinline)) int sceSystemFileSetAllocator(SceSysfileMallocFn mallocFn, SceSysfileFreeFn freeFn)
{
	if ((mallocFn == NULL) | (freeFn == NULL)) {
		return SCE_SYSFILE_ERROR_EINVAL;
	}
	g_sfMallocFn = mallocFn;
	g_sfFreeFn = freeFn;
	return 0;
}

/* === 0x36b4 sysfileMemcmp === */
__attribute__((noinline)) int sysfileMemcmp(const char *lhs, const char *rhs, int len)
{
	while (len-- > 0) {
		if (*lhs > *rhs) {
			return 1;
		}
		if (*lhs < *rhs) {
			return -1;
		}
		lhs++;
		rhs++;
	}
	return 0;
}

/* === 0x3708 sysfileStrcmp === */
__attribute__((noinline)) int sysfileStrcmp(const uint8_t *lhs, const uint8_t *rhs)
{
	int diff;
	uint8_t ch;
	do {
		ch = *lhs++;
		diff = (int)ch - (int)*rhs++;
		if (diff != 0) {
			return diff;
		}
	} while (ch != 0);
	return 0;
}

/* === 0x3734 sysfileBsearch === */
__attribute__((noinline)) void *sysfileBsearch(const void *key, const void *base, uint32_t count, uint32_t elemSize, int (*cmp)(const void *, const void *))
{
	const uint8_t *cur = (const uint8_t *)base;
	while (count != 0) {
		uint32_t half = count >> 1;
		const uint8_t *mid = cur + half * elemSize;
		int diff = cmp(key, mid);
		if (diff == 0) {
			return (void *)mid;
		}
		if (diff > 0) {
			cur = mid + elemSize;
			count = (count - 1u) >> 1;
		} else {
			count = half;
		}
	}
	return NULL;
}

/* === 0x37e0 sysfileShellSort === */
__attribute__((noinline)) void sysfileShellSort(void *base, uint32_t count, uint32_t elemSize, int (*cmp)(const void *, const void *))
{
	uint8_t *bytes = (uint8_t *)base;
	for (uint32_t gap = count >> 1; gap != 0; gap >>= 1) {
		for (uint32_t pos = gap; pos < count; pos++) {
			for (int idx = (int)(pos - gap); idx >= 0; idx -= (int)gap) {
				uint8_t *lhs = bytes + (uint32_t)idx * elemSize;
				uint8_t *rhs = bytes + ((uint32_t)idx + gap) * elemSize;
				if (cmp(lhs, rhs) <= 0) {
					break;
				}
				for (uint32_t bytePos = 0; bytePos < elemSize; bytePos++) {
					lhs[bytePos] ^= rhs[bytePos];
					rhs[bytePos] ^= lhs[bytePos];
					lhs[bytePos] ^= rhs[bytePos];
				}
			}
		}
	}
}

/* === 0x3914 sysfileStrcpy === */
__attribute__((noinline)) char *sysfileStrcpy(char *dst, const char *src)
{
	char *out = dst;
	uint8_t ch;
	do {
		ch = (uint8_t)*src++;
		*out++ = (char)ch;
	} while (ch != 0);
	return dst;
}
