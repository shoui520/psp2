/*
 * psarc.cpp: a PSARC archive's header and TOC. The header's words are big-endian; each TOC
 * entry holds its path's MD5 digest and, big-endian, its first block, size and offset. The
 * block size table follows the TOC (blockSizeBytes per block, 0 for a full stored block);
 * PackBlockSizes keeps every sixteenth block's offset beside it, 5 bytes each, so that
 * GetBlock counts from the nearest one.
 */
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include "fios2_internal.h"

#define PSARC_MAGIC		0x50534152	/* "PSAR" */
#define PSARC_HEADER_SIZE_V1	32		/* version 1 and 2: the TOC follows it */
#define PSARC_MIN_BLOCK_SIZE	256

/* the packed offsets: one per this many blocks */
#define BLOCKS_PER_OFFSET	16
#define OFFSET_BYTES		5

/* a packed offset not filled in yet */
static const uint8_t s_noOffset[OFFSET_BYTES] = { 0 };

/* the manifest's entry: its digest is all zeros */
static const uint8_t s_manifestHash[16] = { 0 };

static inline uint32_t swap32(uint32_t value)
{
	return value >> 24 | (value >> 8 & 0xff00) | (value << 8 & 0xff0000) | value << 24;
}

static inline uint64_t swap64(uint64_t value)
{
	return (uint64_t)swap32((uint32_t)value) << 32 | swap32((uint32_t)(value >> 32));
}

/* a big-endian number of size bytes */
static inline uint64_t readBE(const uint8_t *p, int size)
{
	uint64_t value = 0;
	int n = size - 1;

	if (size > 0) {
		do
			value = value << 8 | *p++;
		while (n-- > 0);
	}
	return value;
}

static inline int32_t entryFirstBlock(const uint8_t *pEntry)
{
	return (int32_t)readBE(((const PsarcTOCEntry *)pEntry)->firstBlock, 4);
}

static inline uint64_t entrySize(const uint8_t *pEntry)
{
	return readBE(((const PsarcTOCEntry *)pEntry)->size, 5);
}

static inline uint64_t entryOffset(const uint8_t *pEntry)
{
	return readBE(((const PsarcTOCEntry *)pEntry)->offset, 5);
}

void PsarcHeader::Parse(const uint8_t *pData)
{
	const uint32_t *pWords = (const uint32_t *)pData;

	magic = swap32(pWords[0]);
	version = swap32(pWords[1]);
	compression = swap32(pWords[2]);
	tocLength = swap32(pWords[3]);
	tocEntrySize = swap32(pWords[4]);
	numFiles = swap32(pWords[5]);
	blockSize = swap32(pWords[6]);
	archiveFlags = swap32(pWords[7]);
	if ((version >> 16) < 3) {
		tocLength = tocLength > PSARC_HEADER_SIZE_V1 ? tocLength - PSARC_HEADER_SIZE_V1 : 0;
		tocSize = tocLength;
		tocOffset = PSARC_HEADER_SIZE_V1;
	} else {
		const uint64_t *pLongs = (const uint64_t *)(pData + PSARC_HEADER_SIZE_V1);

		tocOffset = swap64(pLongs[0]);
		tocSize = swap64(pLongs[1]);
	}
	if (blockSize <= 0x100)
		blockSizeBytes = 1;
	else if (blockSize <= 0x10000)
		blockSizeBytes = 2;
	else if (blockSize <= 0x1000000)
		blockSizeBytes = 3;
	else
		blockSizeBytes = 4;
	sorted = (archiveFlags & PSARC_FLAG_SORTED) != 0;
}

bool PsarcHeader::IsValid()
{
	uint16_t major = version >> 16;
	bool magicValid = magic == PSARC_MAGIC;
	bool versionValid = major == 3 || major == 1;
	bool tocValid = tocLength != 0 && tocLength >= tocEntrySize * numFiles;
	bool entryValid = tocEntrySize >= sizeof(PsarcTOCEntry);
	bool blockValid = blockSize >= PSARC_MIN_BLOCK_SIZE;

	return magicValid && versionValid && tocValid && entryValid && blockValid;
}

uint64_t PsarcHeader::GetFileOffset(const uint8_t *pTOC, const uint8_t *pEntry)
{
	return entryOffset(pEntry);
}

/* the offset of every sixteenth block of each compressed entry into pBlockSizes */
void PsarcHeader::PackBlockSizes(const uint8_t *pTOC, uint8_t *pBlockSizes, uint32_t blockSize)
{
	uint64_t count = (tocLength - (uint64_t)(numFiles * tocEntrySize)) / blockSizeBytes;
	const uint8_t *pEntry = pTOC;
	uint32_t i, files;

	sceClibMemset(pBlockSizes, 0, OFFSET_BYTES * ((uint32_t)(count + 15) >> 4));
	files = numFiles;
	for (i = 0; i < files; i++, pEntry += tocEntrySize) {
		uint64_t size = entrySize(pEntry);
		int64_t first = entryFirstBlock(pEntry);
		uint32_t lastInEntry = size != 0 ? (size - 1) / this->blockSize : 0;
		int64_t last = lastInEntry + first;

		if (first != -1 && size >= blockSize &&
		    (sceClibMemcmp(&pBlockSizes[OFFSET_BYTES * (first / BLOCKS_PER_OFFSET)],
				   s_noOffset, OFFSET_BYTES) == 0 ||
		     sceClibMemcmp(&pBlockSizes[OFFSET_BYTES * (last / BLOCKS_PER_OFFSET)],
				   s_noOffset, OFFSET_BYTES) == 0)) {
			int bytes = blockSizeBytes;
			const uint8_t *pSize = pTOC + numFiles * tocEntrySize + first * bytes;
			uint64_t offset = entryOffset(pEntry);
			int64_t block;

			for (block = first; block <= last; block++) {
				uint64_t stored;

				if (block % BLOCKS_PER_OFFSET == 0) {
					uint8_t *p = &pBlockSizes[OFFSET_BYTES *
								  (block / BLOCKS_PER_OFFSET)];

					p[0] = offset >> 32;
					p[1] = offset >> 24;
					p[2] = offset >> 16;
					p[3] = offset >> 8;
					p[4] = offset;
					bytes = blockSizeBytes;
				}
				stored = readBE(pSize, bytes);
				offset += stored != 0 ? stored : blockSize;
				pSize += bytes;
			}
		}
	}
}

static int compareHash(const void *pA, const void *pB)
{
	return sceClibMemcmp(pA, pB, 16);
}

/* sort the TOC by digest, unless the archive says it is */
bool PsarcHeader::SortTOC(uint8_t *pTOC)
{
	if (!(archiveFlags & PSARC_FLAG_SORTED))
		qsort(pTOC, numFiles, tocEntrySize, compareHash);
	sorted = true;
	return true;
}

const uint8_t *PsarcHeader::FindEntryByHash(const uint8_t *pTOC, const uint8_t *pHash)
{
	const uint8_t *pEntry;
	uint32_t i, count;

	if (pTOC == NULL)
		return NULL;
	if (sorted) {
		PsarcTOCEntry key;

		sceClibMemcpy(key.hash, pHash, sizeof(key.hash));
		return (const uint8_t *)bsearch(&key, pTOC, numFiles, tocEntrySize, compareHash);
	}
	count = numFiles;
	for (i = 0, pEntry = pTOC; i < count; i++, pEntry += tocEntrySize)
		if (sceClibMemcmp(pEntry, pHash, 16) == 0)
			return pEntry;
	return NULL;
}

const uint8_t *PsarcHeader::FindManifest(const uint8_t *pTOC)
{
	uint8_t hash[16];

	memcpy(hash, s_manifestHash, sizeof(hash));
	return FindEntryByHash(pTOC, hash);
}

/* an archive that ignores case keeps its paths' digests upper case */
const uint8_t *PsarcHeader::FindEntry(const uint8_t *pTOC, const char *pPath)
{
	size_t length = fiosStrlen(pPath);
	uint8_t hash[16];

	if (archiveFlags & PSARC_FLAG_IGNORE_CASE) {
		char *pUpper = (char *)alloca(length + 1);
		size_t i;

		for (i = 0; i <= length; i++)
			pUpper[i] = s_upperCase[(uint8_t)pPath[i]];
		fiosMD5(pUpper, length, hash);
	} else {
		fiosMD5(pPath, length, hash);
	}
	return FindEntryByHash(pTOC, hash);
}

uint64_t PsarcHeader::GetFileSize(const uint8_t *pTOC, const uint8_t *pEntry)
{
	return entrySize(pEntry);
}

/*
 * A block of an entry: where it is, its stored size and whether it is compressed. An entry
 * without blocks is stored whole.
 */
bool PsarcHeader::GetBlock(const uint8_t *pTOC, const uint8_t *pEntry, const uint8_t *pBlockSizes,
			   uint32_t block, uint32_t blockSize, uint64_t *pSize,
			   SceFiosOffset *pOffset, bool *pCompressed)
{
	int64_t first = entryFirstBlock(pEntry);
	uint32_t entryBytes, files;
	int bytes;
	uint64_t size, stored;
	int64_t last, target, current;
	uint32_t lastInEntry, group, groupIndex;
	SceFiosOffset offset;
	const uint8_t *pStored;

	if (first == -1) {
		*pCompressed = false;
		*pSize = this->blockSize;
		*pOffset = entryOffset(pEntry) + block * this->blockSize;
		return true;
	}
	entryBytes = tocEntrySize;
	files = numFiles;
	bytes = blockSizeBytes;
	size = entrySize(pEntry);
	lastInEntry = size != 0 ? (size - 1) / this->blockSize : 0;
	target = block + first;
	last = lastInEntry + first;
	group = (uint32_t)(target + BLOCKS_PER_OFFSET / 2) & ~(BLOCKS_PER_OFFSET - 1);
	groupIndex = (uint32_t)(target + BLOCKS_PER_OFFSET / 2) >> 4;
	if (group > last) {
		group -= BLOCKS_PER_OFFSET;
		groupIndex--;
	}
	if (pBlockSizes != NULL && group > first) {
		current = group;
		offset = readBE(pBlockSizes + OFFSET_BYTES * groupIndex, OFFSET_BYTES);
	} else {
		current = first;
		offset = entryOffset(pEntry);
	}
	pStored = pTOC + files * entryBytes + current * bytes;
	for (; current < target; current++) {
		stored = readBE(pStored, bytes);
		pStored += bytes;
		offset += stored != 0 ? stored : blockSize;
	}
	for (; current > target; current--) {
		pStored -= bytes;
		stored = readBE(pStored, bytes);
		offset -= stored != 0 ? stored : blockSize;
	}
	stored = readBE(pStored, bytes);
	if (stored == 0 || stored == blockSize) {
		*pCompressed = false;
		stored = blockSize;
	} else {
		uint32_t remainder = size % this->blockSize;

		*pCompressed = target != last || stored != remainder;
	}
	*pSize = stored;
	*pOffset = offset;
	return true;
}

/* an entry the archive does not compress, or one without blocks */
bool PsarcHeader::IsUncompressed(const uint8_t *pTOC, const uint8_t *pEntry)
{
	bool uncompressed = compression == 0;

	return uncompressed | (entryFirstBlock(pEntry) == -1);
}

/* the range is in the entry, and stored uncompressed */
bool PsarcHeader::IsStored(const uint8_t *pTOC, const uint8_t *pEntry, const uint8_t *pBlockSizes,
			   SceFiosOffset offset, SceFiosSize length)
{
	uint64_t size = entrySize(pEntry);
	uint64_t end, blockLength;
	SceFiosOffset blockOffset;
	uint32_t blockSize, block, lastBlock;
	bool compressed;

	if (size < (uint64_t)offset || (uint64_t)(offset + length) > size)
		return false;
	end = offset + length;
	if (IsUncompressed(pTOC, pEntry))
		return true;
	blockSize = this->blockSize;
	block = (uint64_t)offset / blockSize;
	lastBlock = end / blockSize - (end % blockSize == 0 ? 1 : 0);
	compressed = true;
	for (; block <= lastBlock; block++) {
		if (!GetBlock(pTOC, pEntry, pBlockSizes, block, blockSize, &blockLength,
			      &blockOffset, &compressed) || compressed)
			return false;
		blockSize = this->blockSize;
	}
	return true;
}
