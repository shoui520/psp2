/*
 * vmc_manager.c: ScePspemu PS1 Virtual Memory Card (VMP) block filesystem
 * and 4-slot mount/list/copy/delete manager (0x810314b0..0x81032313).
 */
#include "host_common.h"

#include <string.h>

#define VMC_ERR_BUSY            ((int)0x81018005)
#define VMC_ERR_NOT_READY       ((int)0x81018004)
#define VMC_ERR_INVALID_PARAM   ((int)0x81010002)
#define VMC_ERR_NO_ENTRY        ((int)0x81010006)
#define VMC_ERR_ENTRY_EXISTS    ((int)0x81010014)
#define VMC_ERR_CARD_FULL       ((int)0x81010023)

#define VMC_ERR_GENERIC         ((int)0x80000001)
#define VMC_ERR_IO              ((int)0x80010005)
#define VMC_ERR_INVAL           ((int)0x80010016)

#define VMC_MAX_INTERNAL_CARDS  5U
#define VMC_MAX_MOUNT_SLOTS     4U
#define VMC_BLOCK_SIZE          0x2000U
#define VMC_SECTOR_SIZE         0x80U
#define VMC_HEADER_SIZE         0x80U
#define VMC_IMAGE_SIZE          0x20080U
#define VMC_WORK_SIZE           122880
#define VMC_MAX_FILES           15
#define VMC_RAW_LIMIT           0x840000U

#define VMC_DIR_STATE_FREE      0xA0U
#define VMC_DIR_STATE_HEAD      0x51U
#define VMC_DIR_STATE_MID       0x52U
#define VMC_DIR_STATE_TAIL      0x53U
#define VMC_DIR_STATE_USED_MASK 0xF0U
#define VMC_DIR_STATE_USED_VAL  0x50U

typedef struct VmcDirEntry {
	uint32_t allocState;
	uint32_t fileSize;
	uint16_t nextBlock;
	char fileName[21];
	uint8_t reserved[95];
	uint8_t reservedFlag;
	uint8_t checksum;
} __attribute__((packed)) VmcDirEntry;

_Static_assert(sizeof(VmcDirEntry) == 128U, "VmcDirEntry size");

typedef struct VmcBrokenSectorEntry {
	int32_t sectorNumber;
	uint32_t words[30];
	uint16_t tailHalf;
	uint8_t tailByte;
	uint8_t checksum;
} __attribute__((packed)) VmcBrokenSectorEntry;

_Static_assert(sizeof(VmcBrokenSectorEntry) == 128U, "VmcBrokenSectorEntry size");

typedef struct VmcCardCache {
	uint8_t reservedHeader[16];
	uint8_t superBlock[128];
	VmcDirEntry dirEntries[15];
	VmcBrokenSectorEntry brokenSectors[20];
	uint8_t remainingHeaderBlock[3584];
	uint8_t *rawCardBase;
	uint32_t reservedTail;
} VmcCardCache;

_Static_assert(sizeof(VmcCardCache) == 8216U, "VmcCardCache size");
_Static_assert(offsetof(VmcCardCache, superBlock) == 16U, "superBlock offset");
_Static_assert(offsetof(VmcCardCache, dirEntries) == 144U, "dirEntries offset");
_Static_assert(offsetof(VmcCardCache, brokenSectors) == 2064U, "brokenSectors offset");
_Static_assert(offsetof(VmcCardCache, rawCardBase) == 8208U, "rawCardBase offset");

typedef struct VmcSlotRecord {
	uint8_t *vmpBuffer;
	char filePath[256];
} VmcSlotRecord;

_Static_assert(sizeof(VmcSlotRecord) == 260U, "VmcSlotRecord size");

typedef struct VmcListPreviewEntry {
	uint8_t iconPixels[768];
	uint8_t headerBlock[128];
	char fileName[32];
} VmcListPreviewEntry;

_Static_assert(sizeof(VmcListPreviewEntry) == 928U, "VmcListPreviewEntry size");

static VmcCardCache g_vmcCards[VMC_MAX_INTERNAL_CARDS];
static int g_vmcLocked;
static VmcSlotRecord g_vmcSlots[VMC_MAX_MOUNT_SLOTS];
static uint8_t *g_vmcCopyWorkBuf;

/* 0x810314b0 */
static int __attribute__((noinline, noclone))
vmcRawWrite(unsigned int cardIndex, const void *src, uint32_t offset, size_t size)
{
	size_t maxCopy;

	if (g_vmcLocked != 0)
		return VMC_ERR_BUSY;
	if (cardIndex >= VMC_MAX_INTERNAL_CARDS || src == NULL)
		return VMC_ERR_INVALID_PARAM;

	maxCopy = VMC_RAW_LIMIT - offset;
	if (maxCopy >= size)
		maxCopy = size;
	memcpy(&g_vmcCards[cardIndex].rawCardBase[offset], src, maxCopy);
	return (int)maxCopy;
}

/* 0x81031510 */
static int __attribute__((noinline, noclone))
vmcRawRead(unsigned int cardIndex, void *dst, uint32_t offset, size_t size)
{
	size_t maxCopy;

	if (g_vmcLocked != 0)
		return VMC_ERR_BUSY;
	if (cardIndex >= VMC_MAX_INTERNAL_CARDS || dst == NULL)
		return VMC_ERR_INVALID_PARAM;
	if (offset >= VMC_RAW_LIMIT)
		return 0;

	maxCopy = VMC_RAW_LIMIT - offset;
	if (maxCopy >= size)
		maxCopy = size;
	memcpy(dst, &g_vmcCards[cardIndex].rawCardBase[offset], maxCopy);
	return (int)maxCopy;
}

/* 0x81031570 */
static int __attribute__((noinline)) vmcCheckReady(void)
{
	if (g_vmcLocked != 0)
		return VMC_ERR_NOT_READY;
	return 0;
}

/* 0x8103158c */
static const uint32_t *__attribute__((noinline, noclone))
vmcReadLe32(const uint32_t *src, uint32_t *outVal)
{
	*outVal = *src;
	return src + 1;
}

/* 0x81031594 */
static const uint16_t *__attribute__((noinline, noclone))
vmcReadLe16(const uint16_t *src, uint16_t *outVal)
{
	*outVal = *src;
	return src + 1;
}

/* 0x8103159c */
static uint32_t *__attribute__((noinline, noclone))
vmcWriteLe32(uint32_t *dst, uint32_t val)
{
	*dst = val;
	return dst + 1;
}

/* 0x810315a0 */
static uint16_t *__attribute__((noinline, noclone))
vmcWriteLe16(uint16_t *dst, uint16_t val)
{
	*dst = val;
	return dst + 1;
}

/* 0x810315a8 */
static char *__attribute__((noinline, noclone))
vmcCopyBoundedString(char *dst, const char *src, int maxLen)
{
	int idx = 0;

	if (maxLen > 1) {
		while (idx < maxLen - 1 && src[idx] != '\0') {
			dst[idx] = src[idx];
			idx++;
		}
	}
	dst[idx] = '\0';
	return dst;
}

/* 0x810315e0 */
static int __attribute__((noinline, noclone))
vmcWriteSectors(unsigned int cardIndex, const uint8_t *src, uint32_t cardOffset, uint32_t byteSize)
{
	uint32_t sectorCount = byteSize >> 7;
	uint32_t idx;

	for (idx = 0; idx < sectorCount; idx++) {
		int ret = vmcRawWrite(cardIndex, &src[idx * VMC_SECTOR_SIZE],
		                      cardOffset + idx * VMC_SECTOR_SIZE, VMC_SECTOR_SIZE);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x81031610 */
static int __attribute__((noinline, noclone))
vmcFlushDirEntry(unsigned int cardIndex, VmcDirEntry *entry)
{
	uint8_t sectorBuf[VMC_SECTOR_SIZE];
	uint32_t *wordCursor = (uint32_t *)(void *)sectorBuf;
	uint16_t *halfCursor;
	uint8_t *byteCursor;
	uint8_t xorSum = 0U;
	uint32_t entryIdx = (uint32_t)(entry - g_vmcCards[cardIndex].dirEntries);
	uint32_t idx;

	wordCursor = vmcWriteLe32(wordCursor, entry->allocState);
	wordCursor = vmcWriteLe32(wordCursor, entry->fileSize);
	halfCursor = vmcWriteLe16((uint16_t *)(void *)wordCursor, entry->nextBlock);
	byteCursor = (uint8_t *)(void *)halfCursor;
	memcpy(byteCursor, entry->fileName, sizeof(entry->fileName));
	memcpy(&byteCursor[21], entry->reserved, sizeof(entry->reserved));
	byteCursor[116] = entry->reservedFlag;

	for (idx = 0; idx < VMC_SECTOR_SIZE - 1U; idx++)
		xorSum ^= sectorBuf[idx];

	entry->checksum = xorSum;
	sectorBuf[VMC_SECTOR_SIZE - 1U] = xorSum;
	return vmcWriteSectors(cardIndex, sectorBuf, (entryIdx + 1U) * VMC_SECTOR_SIZE, VMC_SECTOR_SIZE);
}

/* 0x810316d0 */
static int __attribute__((noinline, noclone))
vmcReadSectors(unsigned int cardIndex, uint8_t *dst, uint32_t cardOffset, uint32_t byteSize)
{
	uint32_t sectorCount = byteSize >> 7;
	uint32_t idx;

	for (idx = 0; idx < sectorCount; idx++) {
		int ret = vmcRawRead(cardIndex, &dst[idx * VMC_SECTOR_SIZE],
		                     cardOffset + idx * VMC_SECTOR_SIZE, VMC_SECTOR_SIZE);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x81031700 */
static int __attribute__((noinline, noclone)) vmcFormatCardInternal(unsigned int cardIndex)
{
	VmcCardCache *card = &g_vmcCards[cardIndex];
	uint8_t sectorBuf[VMC_SECTOR_SIZE];
	int ret;
	int idx;

	memset(card->superBlock, 0, VMC_BLOCK_SIZE);
	card->superBlock[0] = 'M';
	card->superBlock[1] = 'C';
	card->superBlock[127] = 0x0EU;

	ret = vmcWriteSectors(cardIndex, card->superBlock, 0U, VMC_SECTOR_SIZE);
	if (ret < 0)
		return ret;

	for (idx = 0; idx < VMC_MAX_FILES; idx++) {
		card->dirEntries[idx].allocState = VMC_DIR_STATE_FREE;
		card->dirEntries[idx].nextBlock = 0xFFFFU;
		card->dirEntries[idx].reservedFlag = 0U;
		ret = vmcFlushDirEntry(cardIndex, &card->dirEntries[idx]);
		if (ret < 0)
			return ret;
	}

	for (idx = 0; idx < 20; idx++) {
		VmcBrokenSectorEntry *broken = &card->brokenSectors[idx];
		uint32_t *wordOut = (uint32_t *)(void *)sectorBuf;
		uint8_t xorSum = 0U;
		int wordIdx;
		int byteIdx;

		broken->sectorNumber = -1;
		broken->words[1] = 0xFFFFU;
		wordOut = vmcWriteLe32(wordOut, (uint32_t)broken->sectorNumber);
		for (wordIdx = 0; wordIdx < 30; wordIdx++)
			wordOut = vmcWriteLe32(wordOut, broken->words[wordIdx]);
		memcpy(&sectorBuf[124], &broken->tailHalf, 2U);
		sectorBuf[126] = broken->tailByte;

		for (byteIdx = 0; byteIdx < 127; byteIdx++)
			xorSum ^= sectorBuf[byteIdx];

		broken->checksum = xorSum;
		sectorBuf[127] = xorSum;
		ret = vmcWriteSectors(cardIndex, sectorBuf,
		                      (uint32_t)(idx + 16) * VMC_SECTOR_SIZE, VMC_SECTOR_SIZE);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* 0x81031830 */
int __attribute__((noinline, noclone)) vmcLoadCardCache(unsigned int cardIndex)
{
	VmcCardCache *card = &g_vmcCards[cardIndex];
	uint8_t sectorBuf[VMC_SECTOR_SIZE];
	int ret;
	int idx;

	memset(card->superBlock, 0, VMC_BLOCK_SIZE);
	ret = vmcReadSectors(cardIndex, card->superBlock, 0U, VMC_SECTOR_SIZE);
	if (ret < 0)
		return ret;

	for (idx = 0; idx < VMC_MAX_FILES; idx++) {
		VmcDirEntry *entry = &card->dirEntries[idx];
		const uint32_t *wordIn = (const uint32_t *)(const void *)sectorBuf;
		const uint16_t *halfIn;
		const uint8_t *byteIn;

		ret = vmcReadSectors(cardIndex, sectorBuf,
		                     (uint32_t)(idx + 1) * VMC_SECTOR_SIZE, VMC_SECTOR_SIZE);
		if (ret < 0)
			return ret;

		wordIn = vmcReadLe32(wordIn, &entry->allocState);
		wordIn = vmcReadLe32(wordIn, &entry->fileSize);
		halfIn = vmcReadLe16((const uint16_t *)(const void *)wordIn, &entry->nextBlock);
		byteIn = (const uint8_t *)(const void *)halfIn;
		memcpy(entry->fileName, byteIn, sizeof(entry->fileName));
		memcpy(entry->reserved, &byteIn[21], sizeof(entry->reserved));
		entry->checksum = byteIn[117];
	}

	for (idx = 0; idx < 20; idx++) {
		VmcBrokenSectorEntry *broken = &card->brokenSectors[idx];
		const uint32_t *wordIn = (const uint32_t *)(const void *)sectorBuf;
		uint32_t rawWord;
		int wordIdx;

		broken->sectorNumber = -1;
		ret = vmcReadSectors(cardIndex, sectorBuf,
		                     (uint32_t)(idx + 16) * VMC_SECTOR_SIZE, VMC_SECTOR_SIZE);
		if (ret < 0)
			return ret;

		wordIn = vmcReadLe32(wordIn, &rawWord);
		broken->sectorNumber = (int32_t)rawWord;
		for (wordIdx = 0; wordIdx < 30; wordIdx++)
			wordIn = vmcReadLe32(wordIn, &broken->words[wordIdx]);
		memcpy(&broken->tailHalf, wordIn, 2U);
		broken->tailByte = ((const uint8_t *)(const void *)wordIn)[2];
		broken->checksum = ((const uint8_t *)(const void *)wordIn)[3];
	}

	return 0;
}

/* 0x81031978 */
static int __attribute__((noinline, noclone))
vmcFindDirEntry(unsigned int cardIndex, const char *name, VmcDirEntry **outEntry)
{
	VmcCardCache *card = &g_vmcCards[cardIndex];
	int idx;

	for (idx = 0; idx < VMC_MAX_FILES; idx++) {
		VmcDirEntry *entry = &card->dirEntries[idx];
		if ((entry->allocState & VMC_DIR_STATE_USED_MASK) == VMC_DIR_STATE_USED_VAL) {
			int chIdx;
			for (chIdx = 0; chIdx < 257; chIdx++) {
				uint8_t left = (uint8_t)entry->fileName[chIdx];
				uint8_t right = (uint8_t)name[chIdx];
				if (left != right)
					break;
				if (left == 0U) {
					*outEntry = entry;
					return VMC_ERR_ENTRY_EXISTS;
				}
			}
		}
	}
	return VMC_ERR_NO_ENTRY;
}

/* 0x810319d8 */
static int __attribute__((noinline, noclone))
vmcCreateDirEntry(unsigned int cardIndex, const char *name)
{
	VmcDirEntry *existing = NULL;
	VmcCardCache *card = &g_vmcCards[cardIndex];
	int ret;
	int idx;

	ret = vmcFindDirEntry(cardIndex, name, &existing);
	if (ret != VMC_ERR_NO_ENTRY)
		return ret;

	for (idx = 0; idx < VMC_MAX_FILES; idx++) {
		VmcDirEntry *entry = &card->dirEntries[idx];
		if ((entry->allocState & VMC_DIR_STATE_USED_MASK) != VMC_DIR_STATE_USED_VAL) {
			entry->allocState = VMC_DIR_STATE_HEAD;
			entry->fileSize = VMC_BLOCK_SIZE;
			entry->nextBlock = 0xFFFFU;
			entry->reservedFlag = 0U;
			vmcCopyBoundedString(entry->fileName, name, sizeof(entry->fileName));
			memset(entry->reserved, 0, sizeof(entry->reserved));
			return vmcFlushDirEntry(cardIndex, entry);
		}
	}

	return VMC_ERR_CARD_FULL;
}

/* 0x81031a78 */
static int __attribute__((noinline, noclone))
vmcExtendFileBlocks(unsigned int cardIndex, VmcDirEntry *headEntry, uint32_t newSize)
{
	VmcCardCache *card = &g_vmcCards[cardIndex];
	VmcDirEntry *tail = headEntry;
	uint32_t remaining;
	int ret;

	if (headEntry->fileSize < newSize) {
		headEntry->fileSize = newSize;
		ret = vmcFlushDirEntry(cardIndex, headEntry);
		if (ret < 0)
			return ret;
	}

	remaining = newSize - VMC_BLOCK_SIZE;
	while (tail->nextBlock != 0xFFFFU) {
		remaining -= VMC_BLOCK_SIZE;
		tail = &card->dirEntries[tail->nextBlock];
	}

	if (remaining < VMC_BLOCK_SIZE)
		return VMC_ERR_INVALID_PARAM;

	if (tail->allocState != VMC_DIR_STATE_HEAD)
		tail->allocState = VMC_DIR_STATE_MID;

	for (;;) {
		VmcDirEntry *freeSlot = NULL;
		int idx;

		for (idx = 0; idx < VMC_MAX_FILES; idx++) {
			if ((card->dirEntries[idx].allocState & VMC_DIR_STATE_USED_MASK) != VMC_DIR_STATE_USED_VAL) {
				freeSlot = &card->dirEntries[idx];
				break;
			}
		}
		if (freeSlot == NULL)
			return VMC_ERR_CARD_FULL;

		tail->nextBlock = (uint16_t)(freeSlot - card->dirEntries);
		ret = vmcFlushDirEntry(cardIndex, tail);
		if (ret < 0)
			return ret;

		remaining -= VMC_BLOCK_SIZE;
		freeSlot->reservedFlag = 0U;
		if (remaining == 0U) {
			freeSlot->allocState = VMC_DIR_STATE_TAIL;
			freeSlot->nextBlock = 0xFFFFU;
			ret = vmcFlushDirEntry(cardIndex, freeSlot);
			return (ret < 0) ? ret : 0;
		}
		freeSlot->allocState = VMC_DIR_STATE_MID;
		tail = freeSlot;
	}
}

/* 0x81031b5c */
static int __attribute__((noinline, noclone))
vmcDeleteFileInternal(unsigned int cardIndex, const char *name)
{
	VmcCardCache *card = &g_vmcCards[cardIndex];
	VmcDirEntry *entry = NULL;
	int ret;

	ret = vmcFindDirEntry(cardIndex, name, &entry);
	if (ret != VMC_ERR_ENTRY_EXISTS)
		return ret;

	for (;;) {
		uint16_t next = entry->nextBlock;

		entry->allocState = VMC_DIR_STATE_FREE;
		entry->fileSize = 0U;
		memset(entry->fileName, 0, sizeof(entry->fileName));
		entry->reservedFlag = 0U;
		entry->nextBlock = 0xFFFFU;
		ret = vmcFlushDirEntry(cardIndex, entry);
		if (ret < 0)
			return ret;
		if (next == 0xFFFFU)
			return 0;
		entry = &card->dirEntries[next];
	}
}

/* 0x81031bf0 */
static int __attribute__((noinline, noclone))
vmcWriteFileData(unsigned int cardIndex, VmcDirEntry *entry, const uint8_t *src,
                 uint32_t offset, uint32_t size)
{
	VmcCardCache *card = &g_vmcCards[cardIndex];
	uint32_t curOffset = offset;
	uint32_t remaining = size;
	const uint8_t *cursor = src;

	while (curOffset >= VMC_BLOCK_SIZE) {
		if (entry->nextBlock == 0xFFFFU)
			return 0;
		curOffset -= VMC_BLOCK_SIZE;
		entry = &card->dirEntries[entry->nextBlock];
	}

	for (;;) {
		uint32_t blockIdx = (uint32_t)(entry - card->dirEntries) + 1U;
		uint32_t chunk = VMC_BLOCK_SIZE - curOffset;
		int ret;

		if (chunk > remaining)
			chunk = remaining;
		ret = vmcWriteSectors(cardIndex, cursor, (blockIdx << 13) + curOffset, chunk);
		if (ret < 0)
			return ret;

		cursor += chunk;
		curOffset = 0U;
		remaining -= chunk;
		if (remaining == 0U)
			return (int)size;
		if (entry->nextBlock == 0xFFFFU)
			return 0;
		entry = &card->dirEntries[entry->nextBlock];
	}
}

/* 0x81031ca8 */
static int __attribute__((noinline, noclone))
vmcReadFileData(unsigned int cardIndex, VmcDirEntry *entry, uint8_t *dst,
                uint32_t offset, uint32_t size)
{
	VmcCardCache *card = &g_vmcCards[cardIndex];
	uint32_t curOffset = offset;
	uint32_t remaining = size;
	uint8_t *cursor = dst;

	while (curOffset >= VMC_BLOCK_SIZE) {
		if (entry->nextBlock == 0xFFFFU)
			return 0;
		curOffset -= VMC_BLOCK_SIZE;
		entry = &card->dirEntries[entry->nextBlock];
	}

	for (;;) {
		uint32_t blockIdx = (uint32_t)(entry - card->dirEntries) + 1U;
		uint32_t chunk = VMC_BLOCK_SIZE - curOffset;
		int ret;

		if (chunk > remaining)
			chunk = remaining;
		ret = vmcReadSectors(cardIndex, cursor, (blockIdx << 13) + curOffset, chunk);
		if (ret < 0)
			return ret;

		cursor += chunk;
		curOffset = 0U;
		remaining -= chunk;
		if (remaining == 0U)
			return (int)size;
		if (entry->nextBlock == 0xFFFFU)
			return 0;
		entry = &card->dirEntries[entry->nextBlock];
	}
}

/* 0x81031d60 */
static int __attribute__((noinline, noclone))
vmcFsDelete(unsigned int cardIndex, const char *name)
{
	int ret = vmcCheckReady();
	if (ret >= 0)
		return vmcDeleteFileInternal(cardIndex, name);
	return ret;
}

/* 0x81031d78 */
static int __attribute__((noinline, noclone))
vmcFsCreate(unsigned int cardIndex, const char *name)
{
	int ret = vmcCheckReady();
	if (ret >= 0) {
		if (*name == '/')
			name++;
		return vmcCreateDirEntry(cardIndex, name);
	}
	return ret;
}

/* 0x81031d9c */
static int __attribute__((noinline, noclone))
vmcFsRead(unsigned int cardIndex, const char *name, void *dst, uint32_t offset, uint32_t size)
{
	VmcDirEntry *entry = NULL;
	int ret;

	if ((offset & 0x7FU) != 0U || (size & 0x7FU) != 0U)
		return VMC_ERR_INVALID_PARAM;

	ret = vmcCheckReady();
	if (ret < 0)
		return ret;

	if (*name == '/')
		name++;
	ret = vmcFindDirEntry(cardIndex, name, &entry);
	if (ret != VMC_ERR_ENTRY_EXISTS)
		return ret;

	if (offset >= entry->fileSize)
		return 0;
	if (offset + size > entry->fileSize)
		size = entry->fileSize - offset;

	return vmcReadFileData(cardIndex, entry, (uint8_t *)dst, offset, size);
}

/* 0x81031e24 */
static int __attribute__((noinline, noclone))
vmcFsWrite(unsigned int cardIndex, const char *name, const void *src, uint32_t offset, uint32_t size)
{
	VmcDirEntry *entry = NULL;
	int ret;

	if ((offset & 0x7FU) != 0U || (size & 0x7FU) != 0U)
		return VMC_ERR_INVALID_PARAM;

	ret = vmcCheckReady();
	if (ret < 0)
		return ret;

	if (*name == '/')
		name++;
	ret = vmcFindDirEntry(cardIndex, name, &entry);
	if (ret == VMC_ERR_NO_ENTRY) {
		ret = vmcCreateDirEntry(cardIndex, name);
		if (ret < 0)
			return ret;
		ret = vmcFindDirEntry(cardIndex, name, &entry);
		if (ret < 0)
			return ret;
	} else if (ret != VMC_ERR_ENTRY_EXISTS) {
		return ret;
	}

	if (offset > entry->fileSize)
		return VMC_ERR_INVALID_PARAM;

	if (offset + size > entry->fileSize) {
		ret = vmcExtendFileBlocks(cardIndex, entry, offset + size);
		if (ret < 0)
			return ret;
	}

	ret = vmcWriteFileData(cardIndex, entry, (const uint8_t *)src, offset, size);
	return (ret < 0) ? ret : (int)size;
}

/* 0x81031ee0 */
int vmcManagerFormatSlot(unsigned int cardIndex)
{
	int ret = vmcCheckReady();
	if (ret >= 0)
		return vmcFormatCardInternal(cardIndex);
	return ret;
}

/* 0x81031ef4 */
static const char *__attribute__((noinline, noclone))
vmcGetHeadEntryName(unsigned int cardIndex, int entryIndex)
{
	const VmcDirEntry *entry = &g_vmcCards[cardIndex].dirEntries[entryIndex];
	if (entry->allocState == VMC_DIR_STATE_HEAD)
		return entry->fileName;
	return NULL;
}

/* 0x81031f1c */
int vmcManagerFlushAndVerify(unsigned int slot)
{
	SceUID fd;
	SceSSize written;
	int ret;

	if (slot >= VMC_MAX_MOUNT_SLOTS)
		return VMC_ERR_GENERIC;

	if ((remotePocsHasMountedMedia() == 0 && slot == 3U) ||
	    (remotePocsHasMountedMedia() != 0 && slot == 0U)) {
		return remotePocsFlushVmc();
	}

	ret = vmpSignHeader(g_vmcSlots[slot].vmpBuffer);
	if (ret < 0)
		return ret;

	fd = sceIoOpen(g_vmcSlots[slot].filePath, SCE_O_WRONLY, 0x1B6);
	if (fd < 0)
		return fd;

	sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, g_vmcSlots[slot].vmpBuffer, VMC_IMAGE_SIZE);
	written = sceIoWrite(fd, g_vmcSlots[slot].vmpBuffer, VMC_IMAGE_SIZE);
	sceIoClose(fd);
	return (written == (SceSSize)VMC_IMAGE_SIZE) ? 0 : VMC_ERR_GENERIC;
}

/* 0x81031fa4 */
int vmcManagerInitWork(void *workBuf, int workSize)
{
	if (workSize != VMC_WORK_SIZE)
		return VMC_ERR_INVAL;
	g_vmcCopyWorkBuf = (uint8_t *)workBuf;
	memset(g_vmcSlots, 0, sizeof(g_vmcSlots));
	return 0;
}

/* 0x81031fd8 */
int vmcManagerRegisterSlot(unsigned int slot, void *slotBuf, int slotSize)
{
	if (slotSize != (int)VMC_IMAGE_SIZE || ((uintptr_t)slotBuf & 3U) != 0U ||
	    slot >= VMC_MAX_MOUNT_SLOTS) {
		return VMC_ERR_INVAL;
	}

	g_vmcSlots[slot].vmpBuffer = (uint8_t *)slotBuf;
	g_vmcCards[slot].rawCardBase = &((uint8_t *)slotBuf)[VMC_HEADER_SIZE];
	return 0;
}

/* 0x81032028 */
int vmcManagerMountFile(unsigned int slot, int fromPocsMem, const char *path)
{
	uint32_t headerSize = 0U;
	int ret;

	if (strlen(path) > 255U)
		return VMC_ERR_INVAL;
	if (slot >= VMC_MAX_MOUNT_SLOTS)
		return VMC_ERR_GENERIC;

	if (slot == 2U) {
		if (fromPocsMem != 0) {
			unsigned int srcSlot = (remotePocsHasMountedMedia() != 0) ? 0U : 3U;
			memcpy(g_vmcSlots[2].vmpBuffer, g_vmcSlots[srcSlot].vmpBuffer, VMC_IMAGE_SIZE);
		} else {
			SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
			SceSSize bytesRead;
			if (fd < 0)
				return fd;
			bytesRead = sceIoRead(fd, g_vmcSlots[2].vmpBuffer, VMC_IMAGE_SIZE);
			sceIoClose(fd);
			if (bytesRead != (SceSSize)VMC_IMAGE_SIZE)
				return VMC_ERR_GENERIC;
		}
		ret = vmpVerifyHeader(g_vmcSlots[2].vmpBuffer, &headerSize);
		if (ret != 0)
			return ret;
	}

	if (vmcLoadCardCache(slot) < 0)
		return VMC_ERR_GENERIC;

	strncpy(g_vmcSlots[slot].filePath, path, sizeof(g_vmcSlots[slot].filePath));
	return 0;
}

/* 0x8103210c */
int vmcManagerListEntries(unsigned int slot, void *outEntries)
{
	VmcListPreviewEntry *entries = (VmcListPreviewEntry *)outEntries;
	int outCount = 0;
	int usedBlocks = 0;
	int idx;

	memset(outEntries, 0, sizeof(VmcListPreviewEntry) * VMC_MAX_FILES);

	for (idx = 0; idx < VMC_MAX_FILES; idx++) {
		const char *name = vmcGetHeadEntryName(slot, idx);
		VmcListPreviewEntry *rec;
		int iconFrames;
		int frameIdx;

		if (name == NULL || name[0] == '\0')
			continue;

		rec = &entries[outCount];
		memcpy(rec->fileName, name, sizeof(rec->fileName));
		if (vmcFsRead(slot, name, rec->headerBlock, 0U, VMC_SECTOR_SIZE) < 0)
			return VMC_ERR_IO;

		iconFrames = (int)(rec->headerBlock[2] & 3U);
		if (iconFrames == 0 || (VMC_MAX_FILES - usedBlocks) < (int)rec->headerBlock[3])
			return VMC_ERR_IO;

		for (frameIdx = 0; frameIdx < iconFrames; frameIdx++) {
			if (vmcFsRead(slot, name, &rec->iconPixels[frameIdx * 256],
			              (uint32_t)(frameIdx + 1) * VMC_SECTOR_SIZE, VMC_SECTOR_SIZE) < 0) {
				return VMC_ERR_IO;
			}
		}

		usedBlocks += (int)rec->headerBlock[3];
		outCount++;
	}

	return outCount;
}

/* 0x81032218 */
int vmcManagerCopyEntry(unsigned int dstSlot, unsigned int srcSlot, const void *entryName)
{
	const char *name = (const char *)entryName;
	VmcDirEntry *entry = NULL;

	if (dstSlot >= VMC_MAX_MOUNT_SLOTS || srcSlot >= VMC_MAX_MOUNT_SLOTS ||
	    vmcFindDirEntry(srcSlot, name, &entry) != VMC_ERR_ENTRY_EXISTS) {
		return VMC_ERR_INVAL;
	}

	if (vmcFsRead(srcSlot, name, g_vmcCopyWorkBuf, 0U, entry->fileSize) != (int)entry->fileSize)
		return VMC_ERR_IO;

	vmcFsDelete(dstSlot, name);
	if (vmcFsCreate(dstSlot, name) < 0)
		return VMC_ERR_IO;

	if (vmcFsWrite(dstSlot, name, g_vmcCopyWorkBuf, 0U, entry->fileSize) != (int)entry->fileSize)
		return VMC_ERR_IO;

	return vmcManagerFlushAndVerify(dstSlot);
}

/* 0x810322c8 */
int vmcManagerDeleteEntry(unsigned int slot, const void *entryName)
{
	if (vmcFsDelete(slot, (const char *)entryName) < 0)
		return VMC_ERR_IO;
	return vmcManagerFlushAndVerify(slot);
}

/* 0x810322e4 */
int vmcManagerFindEntryIndex(unsigned int slot, const char *entryName)
{
	int idx;

	for (idx = 0; idx < VMC_MAX_FILES; idx++) {
		const char *name = vmcGetHeadEntryName(slot, idx);
		if (name != NULL && strncmp(entryName, name, 21U) == 0)
			return idx + 1;
	}
	return 0;
}
