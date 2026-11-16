/*
 * ramcache.cpp: the RAM cache I/O filter (sceFiosIOFilterCache). A cache keeps the blocks of
 * the files under its path in the caller's work buffer: a header, the block table, then the
 * blocks' data. Reads it can serve are copied from its blocks; the rest fill a free block
 * from the file and copy from there. Writes, deletes, truncates and renames of a file drop
 * its blocks. The caches are kept in one list, by path, under one lock.
 */
#include <alloca.h>
#include <limits.h>
#include <stdlib.h>
#include <sce_atomic.h>
#include "fios2_internal.h"

/* a block's state */
#define RAMCACHE_BLOCK_FREE	0
#define RAMCACHE_BLOCK_VALID	1	/* holds its range of the file */
#define RAMCACHE_BLOCK_FILLING	2	/* a read is filling it */
#define RAMCACHE_BLOCK_PURGE	3	/* freed once its last reader is done */

/* an op flag the SDK does not name: the RAM cache leaves the op alone */
#define FIOS_OPFLAG_NORAMCACHE	0x200

/* the op flags the RAM cache looks at */
#define RAMCACHE_OPFLAGS	(SCE_FIOS_OPFLAG_DONTUSECACHE | \
				 SCE_FIOS_OPFLAG_DONTFILLRAMCACHE | FIOS_OPFLAG_NORAMCACHE)

/* the start of a cache's work buffer */
struct RamCacheHeader {
	RamCacheHeader *pNext;		/* the next cache in s_ramCaches */
	uint32_t reserved0;
	int32_t blockSize;
	int32_t blockCount;
	int32_t lastBlock;		/* the block filled last: the search for a free one starts behind it */
	uint32_t hits;			/* reads served from the blocks */
	uint32_t hitBytes;
	uint32_t misses;		/* reads that went to the file */
	uint32_t missBytes;
	FiosCacheEntry *pBlocks;	/* behind the header */
	uint8_t *pData;			/* behind the blocks: blockSize bytes each */
	uint32_t reserved2;
	FiosPathBuffer path;		/* the files it caches are under this path */
};

/* a block fill: the callback chunk's data, then the stat its file needs */
struct RamCacheFill {
	struct RamCache *pContext;
	RamCacheHeader *pCache;
	uint64_t reserved0;
	SceFiosSize actCount;		/* the fill's read */
	SceFiosSize *pActCount;		/* the original read's */
	uint32_t reserved1;
	SceFiosDate modificationDate;	/* the file's, or 0 when the stat finds it */
	uint16_t reserved2;
	int32_t block;
	uint32_t length;		/* the bytes the original read copies from the block */
	uint8_t *pSrc;
	uint8_t *pDst;			/* a buffered read: the offset in its buffer */
	uint32_t reserved3;
	SceFiosStat stat;
};

/* a copy a buffered read makes from a block once its buffer is there */
struct RamCacheCopy {
	int32_t block;
	uint32_t offset;		/* in the block */
	uint32_t dstOffset;		/* in the read's buffer */
	uint32_t length;
};

/* the I/O filter's context: the SDK's, and the methods that work on it */
struct RamCache : public SceFiosRamCacheContext {
	RamCacheHeader *Header() { return (RamCacheHeader *)pWorkBuffer; }

	void FlushRange(FiosCacheRange *pRange);
	bool ContainsRange(FiosCacheRange *pRange);
	bool MatchPathToCache(const char *pPath);
	void PurgeEntries(uint16_t chunkID);
	SceFiosDate GetModificationDate(const char *pPath);
	int FindFreeBlock(bool any);
	int FindBlock(const char *pPath, SceFiosOffset offset);
	int GetOpFlags(uint16_t chunkID);
	int ExecuteRead(uint16_t *pChunkID);
	int ExecuteOpen(uint16_t *pChunkID);
	uint16_t FilterChunk(uint16_t chunkID);
	void FilterChildren(uint16_t chunkID);
	uint16_t FilterTree(uint16_t chunkID);
	bool OwnsChunk(uint16_t chunkID);
	int Terminate();
	int Initialize();
	bool IsValid();
	void PrintStatistics();
	void FreeBlock(int block);
};

RamCacheHeader *s_ramCaches = NULL;		/* by path */
static Wws::Platform::Rwlock s_ramCacheLock;	/* "SceFiosRamcache" */
static int s_ramCacheCount;
static bool s_ramCacheCorrupt;			/* the corrupt state was reported */

/* s_ramCacheLock for a scope, read or write; a read lock can become a write lock */
class RamCacheLock {
public:
	RamCacheLock(bool write) : m_write(write), m_read(!write)
	{
		if (write)
			s_ramCacheLock.WriteLock();
		else
			s_ramCacheLock.ReadLock();
	}
	~RamCacheLock()
	{
		if (m_write)
			s_ramCacheLock.WriteUnlock();
		if (m_read)
			s_ramCacheLock.ReadUnlock();
	}
	void Upgrade()
	{
		s_ramCacheLock.ReadUnlock();
		s_ramCacheLock.WriteLock();
		m_write = true;
		m_read = false;
	}

private:
	bool m_write;
	bool m_read;
};

/* s_chunkMutex for a scope */
class ChunkMutexLock {
public:
	ChunkMutexLock() { s_chunkMutex.Lock(); }
	~ChunkMutexLock() { s_chunkMutex.Unlock(); }
};

static int CallbackHandler(FiosChunk *pChunk, int run);
static int PreExecHandler(FiosChunk *pChunk, int run);
static int LinkedFillHandler(FiosChunk *pChunk, int run);
static int OpenHandler(FiosChunk *pChunk, int run);

/* a block emptied */
static inline void freeBlock(RamCacheHeader *pCache, int block)
{
	pCache->pBlocks[block].state = RAMCACHE_BLOCK_FREE;
	pCache->pBlocks[block].offset = 0;
	pCache->pBlocks[block].length = 0;
	pCache->pBlocks[block].hash[1] = 0;
	pCache->pBlocks[block].hash[0] = 0;
}

/* a block's offsets, in order (qsort) */
static int compareOffsets(const void *pA, const void *pB)
{
	int64_t d = *(const int64_t *)pA - *(const int64_t *)pB;

	if (d < 0)
		return -1;
	return d > 0;
}

/* SCE_FIOS_FILTER_CACHE_FLUSH: the blocks of a range are dropped */
void RamCache::FlushRange(FiosCacheRange *pRange)
{
	if (pRange->length > 0) {
		s_ramCacheLock.WriteLock();
		RamCacheHeader *pCache = Header();
		uint64_t hash[2];
		fiosHash(pRange->pPath, stringLength(pRange->pPath), (uint8_t *)hash);
		SceFiosOffset end = pRange->offset + pRange->length;
		SceFiosOffset start = pRange->offset;
		if (end < 0)
			end = LLONG_MAX;
		int32_t blockSize = pCache->blockSize;
		FiosCacheEntry *pBlock = pCache->pBlocks;
		for (int i = 0; i < pCache->blockCount; i++, pBlock++) {
			if (pBlock->hash[0] != hash[0] || pBlock->hash[1] != hash[1])
				continue;
			if (pBlock->offset < end && pBlock->offset + blockSize > start) {
				if (pBlock->state == RAMCACHE_BLOCK_FILLING)
					pBlock->state = RAMCACHE_BLOCK_PURGE;
				else
					pBlock->state = RAMCACHE_BLOCK_FREE;
			}
		}
		s_ramCacheLock.WriteUnlock();
	}
}

/* SCE_FIOS_FILTER_CACHE_CONTAINS: valid blocks hold all of a range */
bool RamCache::ContainsRange(FiosCacheRange *pRange)
{
	s_ramCacheLock.ReadLock();
	RamCacheHeader *pCache = Header();
	uint64_t hash[2];
	fiosHash(pRange->pPath, stringLength(pRange->pPath), (uint8_t *)hash);
	FiosCacheEntry *pBlock = pCache->pBlocks;
	int64_t blockSize = pCache->blockSize;
	int count = pCache->blockCount;
	SceFiosOffset *pOffsets = (SceFiosOffset *)alloca(count * sizeof(SceFiosOffset));
	if (pOffsets == NULL) {
		s_ramCacheLock.ReadUnlock();
		return false;
	}
	sceClibMemset(pOffsets, 0, count * sizeof(SceFiosOffset));
	int found = 0;
	for (int i = 0; i < count; i++, pBlock++) {
		if (pBlock->hash[0] == hash[0] && pBlock->hash[1] == hash[1] &&
		    pBlock->state == RAMCACHE_BLOCK_VALID)
			pOffsets[found++] = pBlock->offset;
	}
	s_ramCacheLock.ReadUnlock();
	if (found == 0)
		return false;
	qsort(pOffsets, found, sizeof(SceFiosOffset), compareOffsets);
	SceFiosOffset offset = pRange->offset / blockSize * blockSize;
	SceFiosSize remaining = pRange->length + (int32_t)(pRange->offset % blockSize);
	int i;
	for (i = 0; i < found; i++)
		if (pOffsets[i] == offset)
			break;
	if (i == found)
		return false;
	remaining -= blockSize;
	offset += blockSize;
	for (i++; i < found; i++) {
		if (remaining <= 0 || pOffsets[i] != offset)
			break;
		offset += blockSize;
		remaining -= blockSize;
	}
	return remaining <= 0;
}

/* a cache's path starts a path: the path is the cache's, or under it */
static inline bool PathMatchCache(RamCacheHeader *pCache, const char *pPath, RamCacheHeader *pThis)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s checking \"%s\" (%p)..", "PathMatchCache",
		   fiosPathGet(&pCache->path.path), pThis);
	size_t length = fiosStrlen(fiosPathGet(&pCache->path.path));
	if (fiosStrncmp(fiosPathGet(&pCache->path.path), pPath, length) != 0)
		return false;
	if (length == 0)
		return true;
	char c = pPath[length];
	return c == '/' || c == '\\' || c == '\0';
}

/* the first cache in the list whose path matches is this one */
bool RamCache::MatchPathToCache(const char *pPath)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s matching \"%s\" against ram caches..",
		   "MatchPathToCache", pPath);
	RamCacheHeader *pThis = Header();
	RamCacheHeader *pCache;
	for (pCache = s_ramCaches; pCache != NULL; pCache = pCache->pNext)
		if (PathMatchCache(pCache, pPath, pThis))
			break;
	if (pCache == pThis) {
		FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s matched \"%s\" with cache \"%s\"",
			   "MatchPathToCache", pPath, fiosPathGet(&pCache->path.path));
		return true;
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s %s", "MatchPathToCache",
		   pCache != NULL ? "Match is a different cache" : "no match found");
	return false;
}

/* a write, delete, truncate or rename drops the blocks of its file */
void RamCache::PurgeEntries(uint16_t chunkID)
{
	RamCacheHeader *pCache = Header();
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	const char *pPath = NULL;
	s_ramCacheLock.WriteLock();
	uint64_t hash[2];
	hash[0] = 0;
	hash[1] = 0;
	switch (pChunk->type) {
	case CHUNK_FH_WRITE: {
		FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->h);
		pPath = fiosPathGet(&pFH->path);
		fiosHash(pPath, stringLength(pPath), (uint8_t *)hash);
		break;
	}
	case CHUNK_FILE_DELETE:
	case CHUNK_DELETE:
		pPath = fiosPathGet(pChunk->pPath);
		fiosHash(pPath, stringLength(pPath), (uint8_t *)hash);
		break;
	case CHUNK_TRUNCATE:
		pPath = fiosPathGet(pChunk->truncate.pPath);
		fiosHash(pPath, stringLength(pPath), (uint8_t *)hash);
		break;
	case CHUNK_RENAME:
		pPath = fiosPathGet(pChunk->rename.pPath);
		fiosHash(pPath, stringLength(pPath), (uint8_t *)hash);
		break;
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s purging entries in \"%s\" cache for %s",
		   "PurgeEntries", fiosPathGet(&pCache->path.path), pPath);
	for (int i = 0; i < pCache->blockCount; i++) {
		FiosCacheEntry *pBlock = &pCache->pBlocks[i];
		if (pBlock->hash[0] != hash[0] || pBlock->hash[1] != hash[1])
			continue;
		FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s found block %d {%s}", "PurgeEntries",
			   i, fiosDebugDumpCacheEntry(*pBlock, NULL, 0));
		if (pBlock->state == RAMCACHE_BLOCK_FILLING)
			pBlock->state = RAMCACHE_BLOCK_PURGE;
		else
			pBlock->state = RAMCACHE_BLOCK_FREE;
	}
	s_ramCacheLock.WriteUnlock();
}

/* the modification date a valid block of a file was filled with, or 0 */
__attribute__((noinline)) SceFiosDate RamCache::GetModificationDate(const char *pPath)
{
	uint64_t hash[2];
	fiosHash(pPath, stringLength(pPath), (uint8_t *)hash);
	RamCacheHeader *pCache = Header();
	FiosCacheEntry *pBlock = pCache->pBlocks;
	for (int i = 0; i < pCache->blockCount; i++, pBlock++)
		if (pBlock->hash[0] == hash[0] && pBlock->hash[1] == hash[1] &&
		    pBlock->state == RAMCACHE_BLOCK_VALID)
			return pBlock->modificationDate;
	return 0;
}

/*
 * The least recently used block no read is using, from behind the last one filled; with
 * any, a free block ends the search. -1 when every block is busy.
 */
int RamCache::FindFreeBlock(bool any)
{
	RamCacheHeader *pCache = Header();
	int found = -1;
	uint64_t oldest = LLONG_MAX;
	int count = pCache->blockCount;
	if (any) {
		for (int i = 0, n = pCache->lastBlock + 1; i < count; i++, n++) {
			int block = n % count;
			FiosCacheEntry *pBlock = &pCache->pBlocks[block];
			if (pBlock->accessDate < oldest && pBlock->state != RAMCACHE_BLOCK_FILLING &&
			    pBlock->state != RAMCACHE_BLOCK_PURGE && pBlock->refCount == 0) {
				oldest = pBlock->accessDate;
				found = block;
				if (pBlock->state == RAMCACHE_BLOCK_FREE)
					break;
			}
		}
	} else {
		for (int i = 0, n = pCache->lastBlock + 1; i < count; i++, n++) {
			int block = n % count;
			FiosCacheEntry *pBlock = &pCache->pBlocks[block];
			if (pBlock->accessDate < oldest && pBlock->state != RAMCACHE_BLOCK_FILLING &&
			    pBlock->state != RAMCACHE_BLOCK_PURGE &&
			    pBlock->state != RAMCACHE_BLOCK_FREE && pBlock->refCount == 0) {
				oldest = pBlock->accessDate;
				found = block;
			}
		}
	}
	return found;
}

/* the block that holds a file's offset, its access date now; -1 when none does */
int RamCache::FindBlock(const char *pPath, SceFiosOffset offset)
{
	uint64_t hash[2];
	hash[0] = 0;
	hash[1] = 0;
	RamCacheHeader *pCache = Header();
	if (pPath != NULL)
		fiosHash(pPath, stringLength(pPath), (uint8_t *)hash);
	FiosCacheEntry *pBlock = pCache->pBlocks;
	for (int i = 0; i < pCache->blockCount; i++, pBlock++) {
		if (pBlock->state == RAMCACHE_BLOCK_FREE || pBlock->state == RAMCACHE_BLOCK_PURGE ||
		    pBlock->offset != offset)
			continue;
		if (pPath != NULL && (pBlock->hash[0] != hash[0] || pBlock->hash[1] != hash[1]))
			continue;
		pCache->pBlocks[i].accessDate = sceFiosDateGetCurrent();
		return i;
	}
	return -1;
}

/* the RAM cache's op flags of a chunk: its file handle's and its ops' */
int RamCache::GetOpFlags(uint16_t chunkID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	int flags = 0;
	if (pChunk->type > CHUNK_FH_OPEN && pChunk->type < CHUNK_FH_CLOSE) {
		FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->h);
		if (pFH != NULL)
			flags = pFH->openParams.opFlags & RAMCACHE_OPFLAGS;
	}
	while ((chunkID = fiosChunkGetParent(chunkID, 0)) != CHUNKID_INVALID) {
		FiosChunk *pParent = fiosChunkAt(chunkID);
		if (pParent->type == CHUNK_ROOT && pParent->op != 0) {
			FiosOp *pOp = (FiosOp *)s_pOpTable->Lookup(pParent->op);
			flags |= pOp->attr.opflags & RAMCACHE_OPFLAGS;
		}
	}
	return flags;
}

/*
 * A read: what the blocks hold is copied (for a buffered read, by a callback once its buffer is
 * there), what they do not is read from the file into a free block and copied from there. A
 * block another read is filling gets a callback linked behind that read.
 */
int RamCache::ExecuteRead(uint16_t *pChunkID)
{
	uint16_t readID = *pChunkID;
	FiosChunk *pRead = fiosChunkAt(readID);
	uint16_t preExecID = CHUNKID_INVALID;
	uint16_t fillID = CHUNKID_INVALID;
	RamCacheCopy *pCopies = NULL;
	RamCacheFill *pFill = NULL;
	int16_t copyCount = 0;
	int err = SCE_OK;
	FiosFH *pFH;
	int flags;
	const char *pPath;
	RamCacheHeader *pCache;
	SceFiosOffset offset;
	SceFiosOffset origOffset;
	SceFiosOffset blockOffset;
	SceFiosSize length;
	SceFiosSize remaining;
	uint8_t *pBuf;
	uint8_t *pOrigBuf;
	int32_t copied;
	uint32_t dstOffset;
	int64_t blockSize;
	uint16_t bufferChunk;
	uint32_t tailDst;
	uint32_t tailOffsetInBlock;
	uint32_t tailLength;
	int fillBlock;

	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s Chunk %d looking in cache ", "ExecuteRead",
		   readID);
	pFH = (FiosFH *)s_pFHTable->Lookup(pRead->h);
	if (pFH == NULL || !pFH->isOpen)
		return err;
	flags = GetOpFlags(readID);
	if (flags & SCE_FIOS_OPFLAG_NOCACHE)
		return err;
	pPath = fiosPathGet(&pFH->path);
	pCache = Header();
	offset = pRead->offset;
	pBuf = pRead->pBuf;
	length = pRead->reqCount;
	copied = 0;
	dstOffset = 0;
	blockSize = pCache->blockSize;
	if ((offset + length) % blockSize == 0)
		flags |= SCE_FIOS_OPFLAG_DONTFILLRAMCACHE;

	RamCacheLock lock(false);
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s Chunk %d looking for  %s (%lld-%lld) in cache",
		   "ExecuteRead", readID, pPath, offset, offset + length - 1);
	if (offset + length > pFH->size) {
		SceFiosSize clipped = length - (offset + length - pFH->size);
		if (clipped <= 0) {
			*pChunkID = CHUNKID_INVALID;
			return SCE_FIOS_ERROR_EOF;
		}
		length = clipped;
		pRead->reqCount = clipped;
	}
	remaining = length;
	origOffset = offset;
	pOrigBuf = pBuf;
	bufferChunk = pRead->bufferChunk;
	tailDst = 0;
	tailOffsetInBlock = 0;
	tailLength = length;
	if (bufferChunk != CHUNKID_INVALID) {
		pCopies = (RamCacheCopy *)alloca((int16_t)(length / blockSize + 2) * sizeof(RamCacheCopy));
		preExecID = fiosChunkAllocateType(CHUNK_CALLBACK);
		if (preExecID == CHUNKID_INVALID)
			return err;
		fiosChunkLink(preExecID, bufferChunk);
		FiosChunk *pPreExec = fiosChunkAt(preExecID);
		pPreExec->ramCachePreExec.pCallback = PreExecHandler;
		pPreExec->ramCachePreExec.pCache = pCache;
		pPreExec->ramCachePreExec.dstOffset = 0;
		pPreExec->ramCachePreExec.readChunk = readID;
		pPreExec->ramCachePreExec.copyCount = 0;
		pPreExec->ramCachePreExec.pFill = NULL;
		pPreExec->ramCachePreExec.pCopies = NULL;
	}

	/* the blocks that hold the read's start */
	blockOffset = 0;
	while (remaining > 0 && !(flags & SCE_FIOS_OPFLAG_DONTUSECACHE)) {
		blockOffset = offset / blockSize * blockSize;
		uint32_t offsetInBlock = offset % blockSize;
		int block = FindBlock(pPath, blockOffset);
		if (block < 0) {
			FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
				   "%-15s Chunk %d offset %lld not found in cache", "ExecuteRead", readID,
				   blockOffset);
			break;
		}
		FiosCacheEntry *pBlock = &pCache->pBlocks[block];
		FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
			   "%-15s Chunk %d found in block[%d] {%s} refcnt %d callback %d", "ExecuteRead",
			   readID, block, fiosDebugDumpCacheEntry(*pBlock, NULL, 0), pBlock->refCount,
			   pBlock->fillChunk);
		uint8_t *pSrc = pCache->pData + block * pCache->blockSize + offsetInBlock;
		uint32_t n = pBlock->length - offsetInBlock;
		if ((uint32_t)remaining < n)
			n = remaining;
		if (bufferChunk == CHUNKID_INVALID) {
			if (pBlock->state == RAMCACHE_BLOCK_VALID) {
				sceClibMemcpy(pBuf, pSrc, n);
				copied += n;
			} else if (pBlock->state == RAMCACHE_BLOCK_FILLING) {
				s_chunkMutex.Lock();
				if (fiosChunkAt(pBlock->fillChunk)->cancelled) {
					pBlock->state = RAMCACHE_BLOCK_PURGE;
					s_chunkMutex.Unlock();
					break;
				}
				if (fillID == CHUNKID_INVALID) {
					fillID = fiosChunkAllocateType(CHUNK_NULL);
					if (fillID == CHUNKID_INVALID) {
						s_chunkMutex.Unlock();
						return err;
					}
				}
				uint16_t linkedID = fiosChunkAllocateChild(fillID, CHUNK_CALLBACK);
				if (linkedID == CHUNKID_INVALID) {
					if (fiosChunkGetChild(fillID, 0) == CHUNKID_INVALID) {
						fiosChunkDeallocate(fillID);
						s_chunkMutex.Unlock();
						return err;
					}
					s_chunkMutex.Unlock();
					goto restore;
				}
				fiosChunkLink(linkedID, pBlock->fillChunk);
				FiosChunk *pLinked = fiosChunkAt(linkedID);
				pLinked->ramCacheLinkedFill.pCallback = LinkedFillHandler;
				pLinked->ramCacheLinkedFill.pCache = pCache;
				pLinked->ramCacheLinkedFill.pDst = pBuf;
				pLinked->ramCacheLinkedFill.block = block;
				pLinked->ramCacheLinkedFill.offset = offsetInBlock;
				pLinked->ramCacheLinkedFill.length = n;
				pLinked->ramCacheLinkedFill.pActCount = pRead->pActCount;
				sceAtomicIncrement32Relaxed(&pBlock->refCount);
				s_chunkMutex.Unlock();
			}
		} else {
			if (pBlock->state == RAMCACHE_BLOCK_FILLING) {
				s_chunkMutex.Lock();
				if (fiosChunkAt(pBlock->fillChunk)->cancelled) {
					pBlock->state = RAMCACHE_BLOCK_PURGE;
					s_chunkMutex.Unlock();
					break;
				}
				if (!fiosChunkLinkLocked(bufferChunk, pBlock->fillChunk)) {
					s_chunkMutex.Unlock();
					break;
				}
				FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
					   "%-15s Chunk %d block[%d] busy, linking pre-execute %d to existing callback chunk %d",
					   "ExecuteRead", readID, block, bufferChunk, pBlock->fillChunk);
				FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s %s", "ExecuteRead",
					   fiosDebugDumpChunk(pBlock->fillChunk, NULL, 0));
				s_chunkMutex.Unlock();
			}
			pCopies[copyCount].block = block;
			pCopies[copyCount].offset = offsetInBlock;
			pCopies[copyCount].dstOffset = dstOffset;
			pCopies[copyCount].length = n;
			copyCount++;
			sceAtomicIncrement32Relaxed(&pBlock->refCount);
		}
		remaining -= n;
		offset += n;
		pBuf += n;
		dstOffset += n;
		pRead->reqCount = remaining;
		pRead->offset = offset;
		if (bufferChunk == CHUNKID_INVALID)
			pRead->pBuf = pBuf;
		pCache->hits++;
		pCache->hitBytes += n;
	}

	/* the block that holds the read's end */
	fillBlock = -1;
	if (remaining > 0) {
		pCache->misses++;
		pCache->missBytes += remaining;
		lock.Upgrade();
		SceFiosOffset tailBlock = (offset + remaining - 1) / blockSize * blockSize;
		if (tailBlock > origOffset) {
			tailDst = tailBlock - origOffset;
			tailLength = length - tailDst;
		} else {
			tailOffsetInBlock = origOffset - tailBlock;
		}
		fillBlock = FindBlock(pPath, tailBlock);
		if (fillBlock >= 0) {
			FiosCacheEntry *pBlock = &pCache->pBlocks[fillBlock];
			FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
				   "%-15s Chunk %d 2nd chance found in cache block[%d] {%s}", "ExecuteRead",
				   readID, fillBlock, fiosDebugDumpCacheEntry(*pBlock, NULL, 0));
			if (bufferChunk == CHUNKID_INVALID) {
				if (pBlock->state == RAMCACHE_BLOCK_FILLING) {
					flags |= SCE_FIOS_OPFLAG_DONTFILLRAMCACHE;
				} else {
					sceClibMemcpy(pOrigBuf + tailDst,
					       pCache->pData + fillBlock * pCache->blockSize + tailOffsetInBlock,
					       tailLength);
					copied += tailLength;
					pRead->reqCount -= tailLength;
					pCache->hits++;
					pCache->hitBytes += tailLength;
					remaining -= tailLength;
				}
			} else {
				bool linked = true;
				if (pBlock->state == RAMCACHE_BLOCK_FILLING) {
					linked = fiosChunkLinkLocked(bufferChunk, pBlock->fillChunk);
					FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
						   "%-15s Chunk %d block[%d] busy, linking to existing callback chunk %d",
						   "ExecuteRead", readID, fillBlock, pBlock->fillChunk);
					if (!linked)
						FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
							   "%-15s Chunk %d Error linking to existing callback chunk %d",
							   "ExecuteRead", readID, pBlock->fillChunk);
				}
				if (linked) {
					pCopies[copyCount].block = fillBlock;
					pCopies[copyCount].offset = tailOffsetInBlock;
					pCopies[copyCount].dstOffset = tailDst;
					pCopies[copyCount].length = tailLength;
					copyCount++;
					pBlock->refCount++;
					pRead->reqCount -= tailLength;
					remaining -= tailLength;
					pCache->hits++;
					pCache->hitBytes += tailLength;
				}
			}
		}
	}

	/* the copies go behind the read */
	if (bufferChunk == CHUNKID_INVALID) {
		if (fillID != CHUNKID_INVALID && !fiosChunkLink(readID, fillID))
			goto restore;
	} else if (copyCount != 0) {
		if (!fiosChunkLink(readID, preExecID))
			goto restore;
		RamCacheCopy *pSaved = (RamCacheCopy *)fiosChunkAllocateBuffer(preExecID,
									      copyCount * sizeof(RamCacheCopy));
		if (pSaved == NULL) {
			fiosChunkUnlink(readID, preExecID);
			fiosChunkUnlink(preExecID, bufferChunk);
			goto restore;
		}
		for (int i = 0; i < copyCount; i++)
			pSaved[i] = pCopies[i];
		FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
			   "%-15s Chunk %d %d deferred copies, linking callback %d to pre-exec callback %d",
			   "ExecuteRead", readID, copyCount, bufferChunk, preExecID);
		FiosChunk *pPreExec = fiosChunkAt(preExecID);
		pPreExec->ramCachePreExec.pCallback = PreExecHandler;
		pPreExec->ramCachePreExec.pCache = pCache;
		pPreExec->ramCachePreExec.dstOffset = dstOffset;
		pPreExec->ramCachePreExec.readChunk = readID;
		pPreExec->ramCachePreExec.copyCount = copyCount;
		pPreExec->ramCachePreExec.pFill = pFill;
		pPreExec->ramCachePreExec.pCopies = pSaved;
		pRead->bufferChunk = CHUNKID_INVALID;
		FiosChunk *pBufferChunk = fiosChunkAt(bufferChunk);
		pBufferChunk->scheduled = 0;
		pBufferChunk->completed = 0;
		fiosChunkUnlink(readID, bufferChunk);
	}
	if (copied != 0 && pRead->pActCount != NULL)
		sceAtomicAdd64Relaxed(pRead->pActCount, copied);

	/* the rest is read into a free block */
	if (remaining > 0 && fillBlock == -1 && !(flags & SCE_FIOS_OPFLAG_DONTFILLRAMCACHE)) {
		int block = FindFreeBlock(true);
		if (block < 0) {
			FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s Chunk %d No free blocks available",
				   "ExecuteRead", readID);
			if (bufferChunk != CHUNKID_INVALID && copyCount == 0) {
				FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
					   "%-15s Chunk %d unlinking and restoring pre-exec callback",
					   "ExecuteRead", readID);
				fiosChunkUnlink(preExecID, bufferChunk);
				fiosChunkDeallocate(preExecID);
			}
			return err;
		}
		pCache->lastBlock = block;
		FiosCacheEntry *pBlock = &pCache->pBlocks[block];
		if (pBlock->state != RAMCACHE_BLOCK_FREE) {
			FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s Chunk %d Flushing cache block[%d]",
				   "ExecuteRead", readID, block);
			pBlock->state = RAMCACHE_BLOCK_FREE;
		}
		SceFiosDate date = GetModificationDate(pPath);
		pBlock->offset = blockOffset;
		fiosHash(pPath, stringLength(pPath), (uint8_t *)pBlock->hash);
		pBlock->length = blockSize;
		pBlock->accessDate = 0;
		pBlock->modificationDate = 0;
		if (blockOffset + blockSize > pFH->size)
			pBlock->length = pFH->size - blockOffset;
		FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
			   "%-15s Chunk %d filling RAM cache block[%d] with %s (%lld-%lld)", "ExecuteRead",
			   readID, block, pPath, blockOffset, blockOffset + pBlock->length - 1);
		uint32_t maxChunk = g_FiosParams.maxChunk;
		int reads = (pBlock->length + maxChunk - 1) / maxChunk;
		uint8_t *pData = pCache->pData + block * pCache->blockSize;
		uint16_t readChunk;
		if (reads == 1) {
			readChunk = fiosChunkAllocateType(CHUNK_FH_READ);
			pFill = (RamCacheFill *)fiosChunkAllocateBuffer(readChunk, sizeof(RamCacheFill));
			if (readChunk == CHUNKID_INVALID || pFill == NULL) {
				fiosChunkDelete(readChunk);
				if (bufferChunk != CHUNKID_INVALID && copyCount == 0)
					fiosChunkDelete(preExecID);
				return err;
			}
			FiosChunk *pFillRead = fiosChunkAt(readChunk);
			pFillRead->h = pRead->h;
			pFillRead->pActCount = &pFill->actCount;
			pFillRead->offset = pBlock->offset;
			pFillRead->pBuf = pData;
			pFillRead->reqCount = pBlock->length;
		} else {
			readChunk = fiosChunkAllocateType(CHUNK_NULL);
			pFill = (RamCacheFill *)fiosChunkAllocateBuffer(readChunk, sizeof(RamCacheFill));
			if (readChunk == CHUNKID_INVALID || pFill == NULL) {
				fiosChunkDelete(readChunk);
				if (bufferChunk != CHUNKID_INVALID && copyCount == 0)
					fiosChunkDelete(preExecID);
				return err;
			}
			SceFiosOffset pieceOffset = pBlock->offset;
			SceFiosSize left = pBlock->length;
			uint8_t *pPiece = pData;
			for (int i = 0; i < reads; i++) {
				uint16_t pieceID = fiosChunkAllocateChild(readChunk, CHUNK_FH_READ);
				if (pieceID == CHUNKID_INVALID) {
					fiosChunkDelete(readChunk);
					if (bufferChunk != CHUNKID_INVALID && copyCount == 0)
						fiosChunkDelete(preExecID);
					return err;
				}
				FiosChunk *pFillRead = fiosChunkAt(pieceID);
				pFillRead->h = pRead->h;
				pFillRead->pBuf = pPiece;
				pFillRead->offset = pieceOffset;
				pFillRead->pActCount = &pFill->actCount;
				pFillRead->reqCount = (uint32_t)left < maxChunk ? (uint32_t)left : maxChunk;
				pieceOffset += maxChunk;
				pPiece += maxChunk;
				left -= maxChunk;
			}
		}
		pFill->pContext = this;
		pFill->pCache = pCache;
		pFill->modificationDate = date;
		pFill->pActCount = pRead->pActCount;
		pFill->actCount = 0;
		pFill->reserved2 = 0;
		pFill->block = block;
		pFill->length = tailLength;
		pFill->pSrc = pData + tailOffsetInBlock;
		pFill->pDst = pOrigBuf + tailDst;
		if (date == 0) {
			uint16_t statID = fiosChunkAllocateChild(readChunk, CHUNK_STAT);
			if (statID == CHUNKID_INVALID) {
				fiosChunkDelete(readChunk);
				if (bufferChunk != CHUNKID_INVALID && copyCount == 0)
					fiosChunkDelete(preExecID);
				return err;
			}
			FiosChunk *pStat = fiosChunkAt(statID);
			pStat->stat.h = pRead->h;
			pStat->stat.pStatus = &pFill->stat;
		}
		FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
			   "%-15s Chunk %d created callback chunk with %d read chunk(s) (%d ...) %s",
			   "ExecuteRead", readID, reads, readChunk, date != 0 ? " " : "and a stat chunk");
		uint16_t callbackID;
		if (bufferChunk != CHUNKID_INVALID) {
			callbackID = preExecID;
			if (!fiosChunkLink(preExecID, readChunk)) {
				fiosChunkDelete(readChunk);
				if (copyCount == 0)
					fiosChunkDelete(preExecID);
				return err;
			}
			fiosChunkAt(preExecID)->ramCachePreExec.pFill = pFill;
			if (copyCount == 0) {
				FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache",
					   "%-15s Chunk %d no deferred copies, linking fill callback %d to original read %d",
					   "ExecuteRead", readID, preExecID, readID);
				if (!fiosChunkLink(readID, preExecID)) {
					fiosChunkDelete(preExecID);
					return err;
				}
				pRead->bufferChunk = CHUNKID_INVALID;
				FiosChunk *pBufferChunk = fiosChunkAt(bufferChunk);
				pBufferChunk->scheduled = 0;
				pBufferChunk->completed = 0;
			}
			FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s %s", "ExecuteRead",
				   fiosDebugDumpChunk(readID, NULL, 0));
			FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s %s", "ExecuteRead",
				   fiosDebugDumpChunk(preExecID, NULL, 0));
			FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s %s", "ExecuteRead",
				   fiosDebugDumpChunk(readChunk, NULL, 0));
			FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s %s", "ExecuteRead",
				   fiosDebugDumpChunk(bufferChunk, NULL, 0));
		} else {
			callbackID = fiosChunkAllocateType(CHUNK_CALLBACK);
			FiosChunk *pCallback = fiosChunkAt(callbackID);
			pCallback->callback.pData = pFill;
			pCallback->callback.pCallback = CallbackHandler;
			if (!fiosChunkLink(callbackID, readChunk)) {
				fiosChunkDelete(readChunk);
				fiosChunkDeallocate(callbackID);
				return err;
			}
			if (!fiosChunkLink(readID, callbackID)) {
				fiosChunkDelete(callbackID);
				return err;
			}
		}
		FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s Chunk %d Fill callback %d", "ExecuteRead",
			   readID, callbackID);
		pCache->pBlocks[block].state = RAMCACHE_BLOCK_FILLING;
		pCache->pBlocks[block].fillChunk = callbackID;
		pCache->pBlocks[block].refCount++;
		pRead->reqCount -= (int32_t)pFill->length;
		return err;
	}
	if (bufferChunk != CHUNKID_INVALID && copyCount == 0) {
		uint16_t childID;
		while ((childID = fiosChunkGetChild(preExecID, 0)) != CHUNKID_INVALID)
			fiosChunkUnlink(preExecID, childID);
		fiosChunkDeallocate(preExecID);
	}
	return err;

restore:
	/* the read is left as it came */
	if (bufferChunk != CHUNKID_INVALID) {
		while (copyCount-- != 0)
			sceAtomicDecrement32Relaxed(&pCache->pBlocks[pCopies[copyCount].block].refCount);
		uint16_t childID;
		while ((childID = fiosChunkGetChild(bufferChunk, 0)) != CHUNKID_INVALID)
			fiosChunkUnlink(bufferChunk, childID);
		fiosChunkDelete(preExecID);
	} else if (fillID != CHUNKID_INVALID) {
		uint16_t childID;
		/* Sony's: the child is never unlinked, so this does not end while there is one */
		while ((childID = fiosChunkGetChild(fillID, 0)) != CHUNKID_INVALID)
			sceAtomicDecrement32Relaxed(
				&pCache->pBlocks[fiosChunkAt(childID)->ramCacheLinkedFill.block].refCount);
		fiosChunkDelete(fillID);
	}
	pRead->reqCount = length;
	pRead->offset = origOffset;
	pRead->pBuf = pOrigBuf;
	return err;
}

/* an open: a callback chunk over it checks the blocks of the file once it is open */
int RamCache::ExecuteOpen(uint16_t *pChunkID)
{
	uint16_t chunkID = *pChunkID;
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	if (GetOpFlags(chunkID) == 0) {
		uint16_t callbackID = fiosChunkAllocateType(CHUNK_CALLBACK);
		if (callbackID != CHUNKID_INVALID) {
			FiosChunk *pCallback = fiosChunkAt(callbackID);
			pCallback->callback.pCallback = OpenHandler;
			pCallback->callback.pData = Header();
			uint16_t openID = fiosChunkAllocateChild(callbackID, CHUNK_FH_OPEN);
			if (openID != CHUNKID_INVALID) {
				FiosChunk *pOpen = fiosChunkAt(openID);
				pOpen->open.h = pChunk->open.h;
				pOpen->open.pPath = pChunk->open.pPath;
				pOpen->open.nativeMode = pChunk->open.nativeMode;
				pOpen->open.date = 0;
				*pChunkID = callbackID;
				return SCE_OK;
			}
			fiosChunkDelete(callbackID);
		}
	}
	return SCE_OK;
}

/* SCE_FIOS_FILTER_CHUNK for one chunk: what replaces it, or CHUNKID_INVALID */
uint16_t RamCache::FilterChunk(uint16_t chunkID)
{
	uint16_t newChunkID = chunkID;
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	int err = SCE_OK;

	if (pChunk->completed)
		return chunkID;
	switch (pChunk->type) {
	case CHUNK_FH_OPEN:
		err = ExecuteOpen(&newChunkID);
		break;
	case CHUNK_FH_READ:
		err = ExecuteRead(&newChunkID);
		break;
	case CHUNK_FH_WRITE:
	case CHUNK_FILE_DELETE:
	case CHUNK_DELETE:
	case CHUNK_TRUNCATE:
	case CHUNK_RENAME:
		PurgeEntries(chunkID);
		break;
	case CHUNK_FH_BARRIER:
	case CHUNK_DIR_EXISTS:
	case CHUNK_EXISTS:
	case CHUNK_PREFETCH:
	case CHUNK_ARCHIVE_UNMOUNT:
		break;
	}
	if (err != SCE_OK) {
		fiosChunkSetError(chunkID, err, false);
		FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s failed with %s", "FilterChunk",
			   sceFiosDebugDumpError(err, NULL, 0));
		return CHUNKID_INVALID;
	}
	if (newChunkID != chunkID)
		FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s replacing %d with %s",
			   "FilterChunk", chunkID, fiosDebugDumpChunk(newChunkID, NULL, 0));
	return newChunkID;
}

/* a chunk's children, each filtered as a tree */
void RamCache::FilterChildren(uint16_t chunkID)
{
	int count = 0;
	while (fiosChunkGetChild(chunkID, count) != CHUNKID_INVALID)
		count++;
	if (count != 0) {
		uint16_t *pChildren = (uint16_t *)alloca(count * sizeof(uint16_t));
		for (int i = 0; i < count; i++) {
			uint16_t childID = fiosChunkGetChild(chunkID, i);
			if (childID == CHUNKID_INVALID)
				break;
			pChildren[i] = childID;
		}
		for (int i = 0; i < count; i++)
			FilterTree(pChildren[i]);
	}
}

/* a chunk and its children filtered: the chunk replaced by what the filter made of it */
uint16_t RamCache::FilterTree(uint16_t chunkID)
{
	FilterChildren(chunkID);
	uint16_t newChunkID = FilterChunk(chunkID);
	if (newChunkID == chunkID)
		return chunkID;
	uint16_t replacementID = newChunkID;
	if (newChunkID == CHUNKID_INVALID) {
		if (fiosChunkGetChild(chunkID, 0) == CHUNKID_INVALID) {
			fiosChunkDelete(chunkID);
			return newChunkID;
		}
		/* its children stay, under a null chunk; Sony's still returns CHUNKID_INVALID */
		replacementID = fiosChunkAllocateType(CHUNK_NULL);
		if (replacementID == CHUNKID_INVALID) {
			fiosChunkSetError(chunkID, SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK, false);
			fiosChunkDelete(chunkID);
			return CHUNKID_INVALID;
		}
	}
	fiosChunkReplace(chunkID, replacementID);
	fiosChunkDelete(chunkID);
	return newChunkID;
}

/* a chunk works on a file under this cache's path */
bool RamCache::OwnsChunk(uint16_t chunkID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	const char *pPath;

	if (pChunk->type == CHUNK_ROOT) {
		FiosOp *pOp = (FiosOp *)s_pOpTable->Lookup(pChunk->op);
		FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pOp->fh);
		if (pFH != NULL) {
			pPath = fiosPathGet(&pFH->path);
		} else {
			if (fiosStrlen(fiosPathGet(&pOp->path)) == 0)
				return false;
			pPath = fiosPathGet(&pOp->path);
		}
		if (pPath != NULL)
			return MatchPathToCache(pPath);
	} else if (pChunk->type == CHUNK_FH_READ) {
		FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->h);
		if (pFH != NULL) {
			pPath = fiosPathGet(&pFH->path);
			if (pPath != NULL)
				return MatchPathToCache(pPath);
		}
	}
	return false;
}

/* SCE_FIOS_FILTER_TERM: the cache leaves the list, the lock with the last one */
int RamCache::Terminate()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s ", "Terminate");
	if (s_ramCaches == NULL)
		return SCE_FIOS_ERROR_BAD_INDEX;
	RamCacheHeader *pThis = Header();
	RamCacheHeader *pPrev = NULL;
	for (RamCacheHeader *pCache = s_ramCaches; pCache != NULL; pCache = pCache->pNext) {
		if (pCache == pThis) {
			if (pPrev != NULL)
				pPrev->pNext = pThis->pNext;
			else
				s_ramCaches = pThis->pNext;
			break;
		}
		pPrev = pCache;
	}
	if (--s_ramCacheCount == 0) {
		s_ramCacheLock.Term();
		s_ramCacheCorrupt = false;
	}
	return SCE_OK;
}

/* SCE_FIOS_FILTER_INIT: the work buffer laid out, the cache put in the list by its path */
int RamCache::Initialize()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s ", "Initialize");
	RamCacheHeader *pCache;
	if (this == NULL || (pCache = Header()) == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (sizeOfContext != sizeof(SceFiosRamCacheContext)) {
		sceFiosPrintf("FIOS2 ERROR: sizeOfContext %lld != expected %lld.\n",
			      (long long)sizeOfContext, (long long)sizeof(SceFiosRamCacheContext));
		return SCE_FIOS_ERROR_BAD_SIZE;
	}
	if (workBufferSize < SCE_FIOS_RAM_CACHE_BUFFER_SIZE) {
		sceFiosPrintf("FIOS2 ERROR: Cache workBufferSize too small\n");
		return SCE_FIOS_ERROR_BAD_SIZE;
	}
	if (((uintptr_t)pCache & (SCE_FIOS_RAM_CACHE_BUFFER_ALIGNMENT - 1)) != 0) {
		sceFiosPrintf("FIOS2 ERROR: The cache's pWorkBuffer must be %d-byte aligned!\n",
			      SCE_FIOS_RAM_CACHE_BUFFER_ALIGNMENT);
		return SCE_FIOS_ERROR_BAD_ALIGNMENT;
	}
	if (blockSize == 0) {
		sceFiosPrintf("FIOS2 ERROR: The cache blocksize has not been specified.\n");
		return SCE_FIOS_ERROR_BAD_SIZE;
	}
	sceClibMemset(pCache, 0, offsetof(RamCacheHeader, path.path.path) + SCE_FIOS_PATH_MAX);
	if (pPath != NULL) {
		char *pResolved = (char *)alloca(g_FiosParams.pathMax);
		pCache->path.path.Set(pPath, false);
		int err = fiosPlatformOverlayResolve(sceKernelGetProcessId(), 0,
						     fiosPathGet(&pCache->path.path), pResolved,
						     g_FiosParams.pathMax, 0, OVERLAY_ORDER_USER_MAX);
		if (err != SCE_OK)
			return err;
		pCache->path.path.Set(pResolved, false);
	}
	if (s_ramCacheCount == 0)
		s_ramCacheLock.Init("SceFiosRamcache");
	s_ramCacheCount++;
	size_t headerSize = (offsetof(RamCacheHeader, path.path.path) + g_FiosParams.pathMax + 8) & ~7;
	pCache->lastBlock = -1;
	FiosCacheEntry *pBlocks = (FiosCacheEntry *)((uint8_t *)pCache + headerSize);
	pCache->pBlocks = pBlocks;
	pCache->blockSize = blockSize;
	pCache->blockCount = (workBufferSize - headerSize) / (blockSize + sizeof(FiosCacheEntry));
	pCache->pData = (uint8_t *)&pBlocks[pCache->blockCount];
	sceClibMemset(pBlocks, 0, pCache->blockCount * sizeof(FiosCacheEntry));
	if (s_ramCaches == NULL) {
		s_ramCaches = pCache;
	} else {
		size_t length = fiosStrlen(fiosPathGet(&pCache->path.path));
		RamCacheHeader *pPrev = NULL;
		RamCacheHeader *pNext;
		for (pNext = s_ramCaches; pNext != NULL; pPrev = pNext, pNext = pNext->pNext) {
			if (fiosStrncmp(fiosPathGet(&pCache->path.path),
					fiosPathGet(&pNext->path.path), length) > 0) {
				pCache->pNext = pNext;
				if (pPrev != NULL)
					pPrev->pNext = pCache;
				else
					s_ramCaches = pCache;
				break;
			}
		}
		if (pNext == NULL)
			pPrev->pNext = pCache;
	}
	return SCE_OK;
}

/* the context and its work buffer are still what Initialize checked */
__attribute__((noinline)) bool RamCache::IsValid()
{
	if (!s_ramCacheCorrupt && this != NULL && ((uintptr_t)this & 3) == 0 &&
	    sizeOfContext == sizeof(SceFiosRamCacheContext) &&
	    workBufferSize >= SCE_FIOS_RAM_CACHE_BUFFER_SIZE && flags == 0 &&
	    pWorkBuffer != NULL && ((uintptr_t)pWorkBuffer & 7) == 0)
		return true;
	if (!s_ramCacheCorrupt) {
		sceFiosPrintf("FIOS2 ERROR: The ramcache's internal state is corrupt and it will be disabled!\n");
		sceFiosPrintf("FIOS2 ERROR: A common cause of this problem is declaring the "
			      "SceFiosRamCacheContext on the stack. Both the SceFiosRamCacheContext "
			      "and pWorkBuffer must remain valid while the I/O filter is in place!\n");
		s_ramCacheCorrupt = true;
	}
	return false;
}

extern "C"
int sceFiosIOFilterCache(SceFiosRamCacheContext *pContext, int command, void *pArgs)
{
	RamCache *pCache = (RamCache *)pContext;
	int err = SCE_FIOS_ERROR_EVENT_NOT_HANDLED;

	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s event = %d", "sceFiosIOFilterCache",
		   command);
	if (command != FIOS_FILTER_INIT && !pCache->IsValid())
		return SCE_FIOS_ERROR_BAD_PTR;
	switch (command) {
	case FIOS_FILTER_CHUNK: {
		uint16_t *pChunkID = (uint16_t *)pArgs;
		if (pChunkID != NULL && pCache->OwnsChunk(*pChunkID)) {
			*pChunkID = pCache->FilterTree(*pChunkID);
			err = SCE_OK;
		}
		break;
	}
	case FIOS_FILTER_INIT:
		err = pCache->Initialize();
		break;
	case FIOS_FILTER_TERM:
		err = pCache->Terminate();
		break;
	case FIOS_FILTER_DESCRIBE:
		if (pArgs != NULL) {
			fiosStrlcpy((char *)pArgs, "description not implemented", 256);
			err = SCE_OK;
		}
		break;
	case FIOS_FILTER_CACHE_CONTAINS:
	case FIOS_FILTER_CACHE_FLUSH: {
		FiosCacheRange *pRange = (FiosCacheRange *)pArgs;
		char *pResolved = (char *)alloca(g_FiosParams.pathMax);
		if (pResolved == NULL)
			return SCE_FIOS_ERROR_UNKNOWN;
		err = fiosPlatformOverlayResolve(sceKernelGetProcessId(), 0, pRange->pPath, pResolved,
						 g_FiosParams.pathMax, 0, OVERLAY_ORDER_USER_MAX);
		if (err != SCE_OK)
			break;
		err = SCE_FIOS_ERROR_EVENT_NOT_HANDLED;
		if (pCache->MatchPathToCache(pResolved)) {
			FiosCacheRange range;
			range.pPath = pResolved;
			range.offset = pRange->offset;
			range.length = pRange->length;
			if (command == FIOS_FILTER_CACHE_CONTAINS && pCache->ContainsRange(&range)) {
				err = SCE_OK;
			} else if (command == FIOS_FILTER_CACHE_FLUSH) {
				pCache->FlushRange(&range);
				err = SCE_OK;
			}
		}
		break;
	}
	}
	return err;
}

/* the cache's hits and misses */
void RamCache::PrintStatistics()
{
	RamCacheHeader *pCache = Header();
	sceFiosPrintf("RamCache \"%s\" statistics:\n  Cache hits:   %-8lu (%8lu)\n"
		      "  Cache misses: %-8lu (%8lu)\n", fiosPathGet(&pCache->path.path),
		      pCache->hits, pCache->hitBytes, pCache->misses, pCache->hitBytes);
}

/* a block emptied */
void RamCache::FreeBlock(int block)
{
	freeBlock(Header(), block);
}

/* the callback over a fill's reads: the block is valid, and the original read gets its part */
static int CallbackHandler(FiosChunk *pChunk, int run)
{
	if (!run)
		return SCE_OK;
	uint16_t chunkID = fiosChunkID(pChunk);
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s Chunk %d Read finished, err = 0x%08x",
		   "CallbackHandler", chunkID, pChunk->callback.err);
	RamCacheLock lock(true);
	ChunkMutexLock chunkLock;
	RamCacheFill *pFill = (RamCacheFill *)pChunk->callback.pData;
	int block = pFill->block;
	RamCacheHeader *pCache = pFill->pCache;
	SceFiosSize *pActCount = pFill->pActCount;
	FiosCacheEntry *pBlock = &pCache->pBlocks[block];
	pBlock->refCount--;
	pBlock->fillChunk = CHUNKID_INVALID;
	pBlock = &pCache->pBlocks[block];
	pBlock->accessDate = sceFiosDateGetCurrent();
	if (pBlock->modificationDate == 0) {
		SceFiosDate modificationDate = pFill->modificationDate;
		if (modificationDate == 0)
			modificationDate = pFill->stat.modificationDate;
		pBlock->modificationDate = modificationDate;
	}
	int err = pChunk->callback.err;
	if (err != SCE_OK ||
	    (pBlock->state == RAMCACHE_BLOCK_PURGE && pBlock->refCount == 0)) {
		pBlock->state = RAMCACHE_BLOCK_FREE;
		pBlock->length = 0;
		pBlock->offset = 0;
		pBlock->hash[1] = 0;
		pBlock->hash[0] = 0;
		if (err != SCE_OK)
			return err;
	} else if (pBlock->state == RAMCACHE_BLOCK_FILLING) {
		pBlock->state = RAMCACHE_BLOCK_VALID;
	}
	uint32_t length = pFill->length;
	sceClibMemcpy(pFill->pDst, pFill->pSrc, length);
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s read/filled block [%d] %s {%s}",
		   "CallbackHandler", block, fiosPathGet(&pCache->path.path),
		   fiosDebugDumpCacheEntry(pCache->pBlocks[block], NULL, 0));
	if (pActCount != NULL)
		sceAtomicAdd64Relaxed(pActCount, (int32_t)length);
	return SCE_OK;
}

/* a buffered read's copies, once its buffer is there; the block it filled is valid */
static int PreExecHandler(FiosChunk *pChunk, int run)
{
	if (!run)
		return SCE_OK;
	uint16_t chunkID = fiosChunkID(pChunk);
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s Chunk %d starting, err = 0x%08x",
		   "PreExecHandler", chunkID, pChunk->ramCachePreExec.err);
	RamCacheLock lock(true);
	ChunkMutexLock chunkLock;
	uint32_t dstOffset = pChunk->ramCachePreExec.dstOffset;
	RamCacheHeader *pCache = pChunk->ramCachePreExec.pCache;
	int16_t count = pChunk->ramCachePreExec.copyCount;
	RamCacheFill *pFill = pChunk->ramCachePreExec.pFill;
	RamCacheCopy *pCopies = pChunk->ramCachePreExec.pCopies;
	FiosChunk *pRead = fiosChunkAt(pChunk->ramCachePreExec.readChunk);
	SceFiosSize *pActCount = pRead->pActCount;
	uint8_t *pBuf = pRead->pBuf;
	int32_t blockSize = pCache->blockSize;
	SceFiosSize total = 0;
	int err = pChunk->ramCachePreExec.err;

	if (err != SCE_OK || pChunk->cancelled) {
		for (int i = 0; i < count; i++)
			pCache->pBlocks[pCopies[i].block].refCount--;
		if (pFill != NULL) {
			freeBlock(pCache, pFill->block);
			pCache->pBlocks[pFill->block].refCount--;
			pCache->pBlocks[pFill->block].fillChunk = CHUNKID_INVALID;
		}
		if (pChunk->cancelled)
			return SCE_OK;
		return err;
	}
	for (int i = 0; i < count; i++) {
		RamCacheCopy *pCopy = &pCopies[i];
		int block = pCopy->block;
		uint32_t length = pCopy->length;
		sceClibMemcpy(pBuf + pCopy->dstOffset, pCache->pData + block * blockSize + pCopy->offset, length);
		pCache->pBlocks[block].refCount--;
		if (pCache->pBlocks[block].refCount == 0 &&
		    pCache->pBlocks[block].state == RAMCACHE_BLOCK_PURGE)
			freeBlock(pCache, block);
		total += (int32_t)length;
	}
	if (pFill != NULL) {
		SceFiosDate accessDate = sceFiosDateGetCurrent();
		SceFiosDate modificationDate = pFill->modificationDate;
		if (modificationDate == 0)
			modificationDate = pFill->stat.modificationDate;
		int block = pFill->block;
		pCache->pBlocks[block].refCount--;
		pCache->pBlocks[block].fillChunk = CHUNKID_INVALID;
		uint32_t length = pFill->length;
		sceClibMemcpy(pBuf + (uintptr_t)pFill->pDst, pFill->pSrc, length);
		total += (int32_t)length;
		if (pCache->pBlocks[block].state == RAMCACHE_BLOCK_PURGE) {
			if (pCache->pBlocks[block].refCount == 0)
				freeBlock(pCache, block);
		} else {
			if (pCache->pBlocks[block].modificationDate == 0)
				pCache->pBlocks[block].modificationDate = modificationDate;
			pCache->pBlocks[block].accessDate = accessDate;
			pCache->pBlocks[block].state = RAMCACHE_BLOCK_VALID;
		}
	}
	if (pActCount != NULL)
		sceAtomicAdd64Relaxed(pActCount, total);
	pRead->pBuf = pBuf + dstOffset;
	return SCE_OK;
}

/* a read's copy from a block another read was filling, once that read is done */
static int LinkedFillHandler(FiosChunk *pChunk, int run)
{
	if (!run)
		return SCE_OK;
	uint16_t chunkID = fiosChunkID(pChunk);
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s Chunk %d Read finished, err = 0x%08x",
		   "LinkedFillHandler", chunkID, pChunk->ramCacheLinkedFill.err);
	RamCacheLock lock(true);
	ChunkMutexLock chunkLock;
	RamCacheHeader *pCache = pChunk->ramCacheLinkedFill.pCache;
	int block = pChunk->ramCacheLinkedFill.block;
	uint8_t *pDst = pChunk->ramCacheLinkedFill.pDst;
	uint32_t length = pChunk->ramCacheLinkedFill.length;
	uint32_t offset = pChunk->ramCacheLinkedFill.offset;
	SceFiosSize *pActCount = pChunk->ramCacheLinkedFill.pActCount;
	int32_t blockSize = pCache->blockSize;
	pCache->pBlocks[block].refCount--;
	if (pCache->pBlocks[block].state == RAMCACHE_BLOCK_PURGE &&
	    pCache->pBlocks[block].refCount == 0)
		freeBlock(pCache, block);
	if (pChunk->cancelled)
		return SCE_OK;
	int err = pChunk->ramCacheLinkedFill.err;
	if (err != SCE_OK)
		return err;
	sceClibMemcpy(pDst, pCache->pData + block * blockSize + offset, length);
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s filled block [%d] %s {%s}",
		   "LinkedFillHandler", block, fiosPathGet(&pCache->path.path),
		   fiosDebugDumpCacheEntry(pCache->pBlocks[block], NULL, 0));
	if (pActCount != NULL)
		sceAtomicAdd64Relaxed(pActCount, (int32_t)length);
	return SCE_OK;
}

/* an open is done: the blocks of the file filled before it changed are stale */
static int OpenHandler(FiosChunk *pChunk, int run)
{
	if (!run)
		return SCE_OK;
	FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s Open finished, err = 0x%08x",
		   "OpenHandler", pChunk->callback.err);
	if (pChunk->callback.err != SCE_OK)
		return pChunk->callback.err;
	RamCacheLock lock(true);
	ChunkMutexLock chunkLock;
	RamCacheHeader *pCache = (RamCacheHeader *)pChunk->callback.pData;
	FiosChunk *pOpen = fiosChunkAt(fiosChunkGetChild(fiosChunkID(pChunk), 0));
	SceFiosDate date = pOpen->open.date;
	const char *pPath = fiosPathGet(pOpen->open.pPath);
	uint64_t hash[2];
	fiosHash(pPath, stringLength(pPath), (uint8_t *)hash);
	for (int i = 0; i < pCache->blockCount; i++) {
		FiosCacheEntry *pBlock = &pCache->pBlocks[i];
		if (pBlock->state == RAMCACHE_BLOCK_FREE || pBlock->state == RAMCACHE_BLOCK_PURGE)
			continue;
		if (pBlock->hash[0] != hash[0] || pBlock->hash[1] != hash[1])
			continue;
		if (pBlock->refCount != 0) {
			pBlock->modificationDate = date;
		} else if (pBlock->modificationDate != date) {
			FIOS_TRACE(SCE_FIOS_PROFILE_CACHE, "cache", "%-15s found stale block %d {%s}",
				   "OpenHandler", i, fiosDebugDumpCacheEntry(*pBlock, NULL, 0));
			freeBlock(pCache, i);
			pCache->pBlocks[i].modificationDate = 0;
		}
	}
	return SCE_OK;
}
