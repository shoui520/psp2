/*
 * dearchiver.cpp: the PSARC dearchiver I/O filter (sceFiosIOFilterPsarcDearchiver) and the
 * codecs it decompresses with. The method names are Sony's, from the profiling messages
 * ("LookupPath", "Mount", ...); the class and other names are ours.
 */
#include <stdlib.h>
#include <sce_atomic.h>
#include "fios2_internal.h"

static FiosCodec *volatile s_codecs[4];
static uint8_t s_threadCount;			/* the most decompressor threads */
static uint8_t s_decompressorThreadCount;

void fiosDearchiverSetThreadCount(uint8_t count)
{
	s_decompressorThreadCount = count;
	s_threadCount = count;
}

/* take the codec out of the registry */
static inline bool removeCodec(FiosCodec *pCodec)
{
	bool removed = false;
	int i;

	for (i = 0; i < 4 && !removed; i++)
		if ((FiosCodec *)sceAtomicCompareAndSwap32Relaxed((volatile int32_t *)&s_codecs[i],
								  (int32_t)pCodec, 0) == pCodec)
			removed = true;
	return removed;
}

__attribute__((noinline)) bool fiosDearchiverUnregisterCodec(FiosCodec *pCodec)
{
	bool removed = removeCodec(pCodec);

	if (removed && pCodec != NULL && pCodec->term != NULL)
		pCodec->term();
	return removed;
}

void fiosDearchiverTerm()
{
	int i;

	for (i = 0; i < 4; i++) {
		FiosCodec *pCodec = s_codecs[i];

		if (pCodec != NULL && fiosDearchiverUnregisterCodec(pCodec))
			sceFiosPrintf("WARNING: Unregistered codec during termination\n");
	}
	s_decompressorThreadCount = 0;
	s_threadCount = 0;
}

/* the work buffer is the dearchiver's: no codec uses it */
int fiosDearchiverRegisterCodec(FiosCodec *pCodec)
{
	bool added = false;
	int ret, i;

	for (i = 0; i < 4 && !added; i++)
		if (sceAtomicCompareAndSwap32Relaxed((volatile int32_t *)&s_codecs[i], 0,
						     (int32_t)pCodec) == 0)
			added = true;
	if (!added || pCodec == NULL || pCodec->init == NULL)
		return -1;
	ret = pCodec->init(s_threadCount);
	if (ret != 0)
		removeCodec(pCodec);
	return ret;
}

/* a codec of the kind that has the flags */
FiosCodec *fiosDearchiverFindCodec(uint32_t codec, uint16_t flags)
{
	int i;

	for (i = 0; i < 4; i++) {
		FiosCodec *pCodec = s_codecs[i];

		if (pCodec != NULL && pCodec->codec == codec && (pCodec->flags & flags) == flags)
			return pCodec;
	}
	return NULL;
}

uint8_t fiosDearchiverSetDecompressorThreadCount(uint8_t threadCount)
{
	int i;

	if (threadCount == 0)
		threadCount = 1;
	else if (threadCount > s_threadCount)
		threadCount = s_threadCount;
	for (i = 0; i < 4; i++) {
		FiosCodec *pCodec = s_codecs[i];

		if (pCodec != NULL && pCodec->setThreadCount != NULL)
			pCodec->setThreadCount(threadCount);
	}
	s_decompressorThreadCount = threadCount;
	return threadCount;
}

uint8_t fiosDearchiverGetDecompressorThreadCount()
{
	return s_decompressorThreadCount;
}

/* what LookupPath finds for a path ("FINDRESULT_NOT_OURS" is Sony's) */
enum {
	FINDRESULT_NOT_OURS,		/* no archive is mounted there */
	FINDRESULT_PATH,		/* under an archive's mount point, but nothing is there */
	FINDRESULT_FILE,
	FINDRESULT_DIR,
};

/*
 * The dearchiver, in the caller's SceFiosPsarcDearchiverContext: the SDK's fields, then the
 * mounted archives in its reserved ones.
 */
class PsarcDearchiver {
public:
	size_t sizeOfContext;
	size_t workBufferSize;
	void *pWorkBuffer;
	intptr_t flags;
	ArchiveTracker *pArchives;
	uint32_t reserved[2];

	void GetStat(ArchiveTracker *pTracker, const uint8_t *pEntry, SceFiosStat *pOutStat);
	int FindDirectoryEntry(ArchiveTracker *pTracker, const char *pPath, int index,
			       char *pOutName);
	int LookupPath(FiosPath *pPath, ArchiveTracker **ppOutTracker,
		       const uint8_t **ppOutEntry);
	ArchiveTracker *FindArchive(uint32_t id);
	bool LookupFH(SceFiosFH fh, ArchiveTracker **ppOutTracker, const uint8_t **ppOutEntry);
	bool LookupDH(SceFiosDH dh, ArchiveTracker **ppOutTracker);
	int ReadDirectory(ArchiveTracker *pTracker, FiosChunk *pChunk, uint16_t *pOutCallback);
	int OpenDirectory(FiosDH *pDH, FiosChunk *pChunk, uint16_t *pOutCallback);
	int CreateReadChunksUnlocked(uint16_t *pOutChunk, ArchiveTracker *pTracker,
				     const uint8_t *pEntry, uint8_t *pBuf, SceFiosOffset offset,
				     SceFiosSize length, SceFiosSize *pActCount);
	bool OwnsFH(SceFiosFH fh);
	uint16_t FilterChunk(uint16_t chunkID);
	void FilterChildren(uint16_t chunkID);
	uint16_t FilterTree(uint16_t chunkID);
	static int Terminate();
	static int Init(SceFiosPsarcDearchiverContext *pContext);
	static bool IsValid(SceFiosPsarcDearchiverContext *pContext);
	uint32_t AllocateID(ArchiveTracker *pNew);
};

/* OpenDirCallback's data: the native directory's open */
struct OpenDirData {
	uint16_t openChunk;
	FiosDH *pDH;
};

/* ReadDirCallback's data: a read of the native directory under the archives */
struct ReadDirData {
	SceFiosSize reqCount;
	uint16_t readChunk;		/* CHUNKID_INVALID until it reads */
	uint8_t type;			/* CHUNK_DH_READ or CHUNK_DH_READ_BULK */
	uint8_t reserved0;
	uint32_t reserved1;
	PsarcDearchiver *pDearchiver;
	FiosDH *pDH;
	SceFiosDirEntry *pEntry;
	int err;			/* what the archives' read gave */
};

int OpenDirCallback(FiosChunk *pChunk, int run);
int ReadDirCallback(FiosChunk *pChunk, int run);
int DecompressCallback(FiosChunk *pChunk, int run);
int BufferCallback(FiosChunk *pChunk, int run);
int MountHeaderCallback(FiosChunk *pChunk, int run);
int MountTOCCallback(FiosChunk *pChunk, int run);
int MountCallback(FiosChunk *pChunk, int run);

/* MountHeaderCallback's data: the archive header's read */
struct MountHeaderData {
	SceFiosSize actCount;
	uint8_t *pHeader;
	void *pMountBuffer;
	size_t mountBufferLength;
	const char *pMountPoint;	/* NULL for a query */
	uint16_t tocReadChunk;
	uint16_t reserved;
	SceFiosFH fh;
	SceFiosStat *pStat;		/* the archive file's, for a mount */
};

/* MountTOCCallback's data: the TOC's read */
struct MountTOCData {
	SceFiosSize actCount;
	uint8_t *pHeader;
	void *pMountBuffer;
	size_t mountBufferLength;
	const char *pMountPoint;
	SceFiosSize *pActCount;		/* the op's: the mount buffer's size */
	PsarcDearchiver *pDearchiver;
	uint16_t tocReadChunk;
	uint16_t mountChunk;		/* MountCallback's, or CHUNKID_INVALID for a query */
};

static uint32_t s_nextArchiveID = 1;
static int s_initCount;
static Wws::Platform::Mutex s_dearchiverMutex;	/* "SceFiosDearchiver", recursive */
static bool s_corrupt;				/* the dearchiver is disabled */
static int s_initialReads;			/* the blocks a read starts with */

/* the dearchiver's methods run with s_dearchiverMutex held */
#define ASSERT_DEARCHIVER_LOCKED()							\
	((void)Wws::Platform::ThreadIdEqual(Wws::Platform::GetCurrentThreadId(),	\
					    s_dearchiverMutex.GetOwner()))

static inline bool isSeparator(char c)
{
	return c == '/' || c == '\\';
}

/* the path of an archive's file, for the messages */
static inline const char *archiveName(ArchiveTracker *pTracker)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pTracker->fh);

	if (pFH == NULL)
		return "<invalid-fh>";
	return fiosPathGet(&pFH->path);
}

/* an entry's stat: the archive file's, with the entry's size; NULL for a directory */
void PsarcDearchiver::GetStat(ArchiveTracker *pTracker, const uint8_t *pEntry,
			      SceFiosStat *pOutStat)
{
	ASSERT_DEARCHIVER_LOCKED();
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s ", "GetStat");
	if (pOutStat == NULL)
		return;
	*pOutStat = pTracker->stat;
	if (pEntry != NULL) {
		pOutStat->fileSize = pTracker->header.GetFileSize(pTracker->pTOC, pEntry);
		pOutStat->statFlags = SCE_FIOS_STATUS_READABLE;
	} else {
		pOutStat->fileSize = 0;
		pOutStat->statFlags = SCE_FIOS_STATUS_DIRECTORY | SCE_FIOS_STATUS_READABLE;
	}
}

/*
 * Whether the path is the mount point or under it: the rest of the path goes to pOutRest,
 * from the separator on.
 */
static bool matchMountPoint(const char *pMountPoint, const char *pPath, char *pOutRest,
			    size_t restSize, bool ignoreCase)
{
	size_t length = fiosStrlen(pMountPoint);
	int (*compare)(const char *, const char *, size_t) = fiosStrncmp;
	char last, next;
	size_t back = 0;

	if (ignoreCase)
		compare = fiosStrncasecmp;
	if (compare(pMountPoint, pPath, length) != 0)
		return false;
	last = 0;
	if (length != 0)
		last = pMountPoint[length - 1];
	next = pPath[length];
	if (last != 0 && !isSeparator(last) && next != 0 && !isSeparator(next))
		return false;
	if (pOutRest != NULL) {
		if (isSeparator(last))
			back = 1;
		fiosStrlcpy(pOutRest, pPath + length - back, restSize);
	}
	return true;
}

/* the length of a path's first element */
static inline size_t elementLength(const char *pPath)
{
	size_t length = 0;

	while (*pPath != 0 && !isSeparator(*pPath)) {
		pPath++;
		length++;
	}
	return length;
}

/*
 * A directory of the archive: the manifest entry of its first file (index -1) or of the
 * next file after index that is in another of its entries, with that entry's name.
 */
int PsarcDearchiver::FindDirectoryEntry(ArchiveTracker *pTracker, const char *pPath, int index,
					char *pOutName)
{
	char path[1024];
	size_t length, count, n, i;
	const char *pSrc;
	char last, next, c;
	bool atSeparator, ignoreCase;
	char *pDst;
	int match;

	ASSERT_DEARCHIVER_LOCKED();
	if (pOutName != NULL)
		*pOutName = 0;
	match = fiosStrncmp(pPath, pTracker->pMountPoint, pTracker->mountPointLength);
	length = pTracker->mountPointLength;
	last = pPath[length - 1];
	next = 0;
	atSeparator = isSeparator(last) || (next = pPath[length]) == 0 || isSeparator(next);
	if (match != 0 || !atSeparator)
		return -1;
	fiosStrlen(pPath);
	path[0] = '/';
	n = (pTracker->header.archiveFlags & PSARC_FLAG_ABSOLUTE) != 0;
	pSrc = pPath + pTracker->mountPointLength + isSeparator(next);
	pDst = path + n;
	do {
		c = *pSrc++;
		if (c == '\\')
			c = '/';
		n++;
		*pDst++ = c;
	} while (c != 0 && n < sizeof(path));
	ignoreCase = pTracker->header.archiveFlags & PSARC_FLAG_IGNORE_CASE;
	count = pTracker->manifestPaths;
	length = n - 1;
	if (index == -1) {
		size_t lo = 0, hi = count - 1, span = count - 1;
		char **manifest = pTracker->manifest;

		while (span != 0) {
			size_t mid = lo + (span >> 1);
			size_t half = span >> 1;
			int cmp;

			if (ignoreCase)
				cmp = fiosStrncasecmp(path, manifest[mid], length);
			else
				cmp = fiosStrncmp(path, manifest[mid], length);
			manifest = pTracker->manifest;
			if (cmp <= 0) {
				hi = mid;
				span = half;
			} else {
				lo = mid + 1;
				span = hi - (mid + 1);
			}
		}
		if (ignoreCase) {
			if (fiosStrcasecmp(manifest[lo], path) == 0)
				return -1;
		} else if (fiosStrcmp(manifest[lo], path) == 0) {
			return -1;
		}
		if (!matchMountPoint(path, pTracker->manifest[lo], NULL, 0, ignoreCase))
			return -1;
		if (pOutName != NULL) {
			fiosStrlcpy(pOutName, pTracker->manifest[lo], sizeof(path));
			pOutName[n + elementLength(pOutName + n)] = 0;
		}
		return (int)lo;
	}
	{
		const char *pName = pTracker->manifest[index] + length;
		size_t nameLength;

		if (isSeparator(*pName))
			pName++;
		nameLength = elementLength(pName);
		for (i = index + 1; i < count; i++) {
			const char *pOther;

			if (!matchMountPoint(path, pTracker->manifest[i], NULL, 0, ignoreCase))
				return -1;
			pOther = pTracker->manifest[i] + length;
			if (isSeparator(*pOther))
				pOther++;
			if (nameLength != elementLength(pOther))
				break;
			if (ignoreCase) {
				if (fiosStrncasecmp(pName, pOther, nameLength) != 0)
					break;
			} else if (fiosStrncmp(pName, pOther, nameLength) != 0) {
				break;
			}
			count = pTracker->manifestPaths;
		}
		if (i >= count)
			return -1;
		if (pOutName != NULL) {
			fiosStrlcpy(pOutName, pTracker->manifest[i], sizeof(path));
			pOutName[n + elementLength(pOutName + n)] = 0;
		}
		return (int)i;
	}
}

/* "LookupPath": the archive and the entry a path names */
int PsarcDearchiver::LookupPath(FiosPath *pPath, ArchiveTracker **ppOutTracker,
				const uint8_t **ppOutEntry)
{
	ArchiveTracker *pFound = NULL;
	const uint8_t *pEntry;
	ArchiveTracker *pTracker;
	const char *path;
	size_t size;
	int result;

	ASSERT_DEARCHIVER_LOCKED();
	if (ppOutTracker == NULL)
		ppOutTracker = &pFound;
	if (ppOutEntry == NULL)
		ppOutEntry = &pEntry;
	*ppOutTracker = NULL;
	*ppOutEntry = NULL;
	path = fiosPathGet(pPath);
	size = fiosStrlen(path) + 1;

	char rest[size];

	result = FINDRESULT_NOT_OURS;
	for (pTracker = pArchives; pTracker != NULL; pTracker = pTracker->pNext) {
		const char *p;
		char c;

		if (!matchMountPoint(pTracker->pMountPoint, path, rest, size,
				     pTracker->header.archiveFlags & PSARC_FLAG_IGNORE_CASE))
			continue;
		pFound = pTracker;
		*ppOutTracker = pTracker;
		p = rest;
		c = *p;
		if (!(pTracker->header.archiveFlags & PSARC_FLAG_ABSOLUTE) && c == '/') {
			do
				c = *++p;
			while (c == '/');
		}
		if (c == 0 || (c == '/' && p[1] == 0)) {
			result = FINDRESULT_DIR;
			*ppOutEntry = NULL;
			break;
		}
		pEntry = pTracker->header.FindEntry(pTracker->pTOC, rest);
		if (pEntry != NULL) {
			result = FINDRESULT_FILE;
			*ppOutEntry = pEntry;
			break;
		}
		if (FindDirectoryEntry(pTracker, path, -1, NULL) != -1) {
			result = FINDRESULT_DIR;
			*ppOutEntry = NULL;
			break;
		}
		result = FINDRESULT_PATH;
		*ppOutEntry = NULL;
	}
	if (result != FINDRESULT_NOT_OURS) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s archive \"%s\" owns %s \"%s\"",
			   "LookupPath", archiveName(pFound),
			   result == FINDRESULT_FILE ? "file" :
			   result == FINDRESULT_DIR ? "dir" : "path, but no file/dir was found:",
			   path);
	} else {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s %s = FINDRESULT_NOT_OURS",
			   "LookupPath", path);
	}
	return result;
}

/* the mounted archive with the id */
ArchiveTracker *PsarcDearchiver::FindArchive(uint32_t id)
{
	ArchiveTracker *pTracker;

	ASSERT_DEARCHIVER_LOCKED();
	for (pTracker = pArchives; pTracker != NULL; pTracker = pTracker->pNext)
		if (pTracker->id == id)
			return pTracker;
	return NULL;
}

/* "LookupFH": the archive and the entry of a file opened in one */
bool PsarcDearchiver::LookupFH(SceFiosFH fh, ArchiveTracker **ppOutTracker,
			       const uint8_t **ppOutEntry)
{
	ArchiveTracker *pFound = NULL;
	const uint8_t *pEntry;
	FiosFH *pFH;

	ASSERT_DEARCHIVER_LOCKED();
	if (ppOutTracker == NULL)
		ppOutTracker = &pFound;
	if (ppOutEntry == NULL)
		ppOutEntry = &pEntry;
	*ppOutTracker = NULL;
	*ppOutEntry = NULL;
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	if (pFH == NULL || pFH->pArchiver != this)
		return false;
	pFound = FindArchive(pFH->archiveID);
	*ppOutTracker = pFound;
	if (pFound == NULL) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s fh is from an already-closed archive: %s", "LookupFH",
			   sceFiosDebugDumpFH(fh, NULL, 0));
		return false;
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s archive \"%s\" owns %s",
		   "LookupFH", archiveName(pFound), sceFiosDebugDumpFH(fh, NULL, 0));
	*ppOutEntry = pFound->pTOC + pFound->header.tocEntrySize * pFH->entryIndex;
	return true;
}

/* "LookupDH": the archive of a directory opened in one */
bool PsarcDearchiver::LookupDH(SceFiosDH dh, ArchiveTracker **ppOutTracker)
{
	ArchiveTracker *pFound = NULL;
	FiosDH *pDH;

	ASSERT_DEARCHIVER_LOCKED();
	if (ppOutTracker == NULL)
		ppOutTracker = &pFound;
	*ppOutTracker = NULL;
	pDH = (FiosDH *)s_pDHTable->Lookup(dh);
	if (pDH == NULL || pDH->pArchiver != this)
		return false;
	pFound = FindArchive(pDH->archiveID);
	*ppOutTracker = pFound;
	if (pFound == NULL) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s dh is from an already-closed archive: %s", "LookupDH",
			   sceFiosDebugDumpDH(dh, NULL, 0));
		return false;
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s archive \"%s\" owns %s",
		   "LookupDH", archiveName(pFound), sceFiosDebugDumpDH(dh, NULL, 0));
	return true;
}

/*
 * "ReadDirectory": the next entry of a directory in the archives mounted there, then, with
 * translucent mounts, of the native directory under them.
 */
int PsarcDearchiver::ReadDirectory(ArchiveTracker *pTracker, FiosChunk *pChunk,
				   uint16_t *pOutCallback)
{
	char name[1024];
	FiosPathBuffer entryPath;
	ArchiveTracker *pFound;
	const uint8_t *pEntry;
	SceFiosDirEntry *pOut;
	char *pScratch;
	FiosDH *pDH;
	int index, i;
	int err;

	ASSERT_DEARCHIVER_LOCKED();
	pDH = (FiosDH *)s_pDHTable->Lookup(pChunk->h);
	pScratch = (char *)pChunk->pBuf;
	index = pDH->entryIndex;
	err = SCE_FIOS_ERROR_EOF;
	for (;;) {
		const char *pName;
		size_t length, nameStart;
		size_t n;

		for (;;) {
			i = FindDirectoryEntry(pTracker, fiosPathGet(&pDH->path), index, name);
			if (i != -1)
				break;
			for (pTracker = pTracker->pNext; pTracker != NULL; pTracker = pTracker->pNext)
				if (matchMountPoint(pTracker->pMountPoint, fiosPathGet(&pDH->path), NULL, 0,
						    pTracker->header.archiveFlags & PSARC_FLAG_IGNORE_CASE))
					break;
			if (pTracker == NULL) {
				pDH->flags |= FIOS_DH_EOF;
				goto done;
			}
			pDH->archiveID = pTracker->id;
			pDH->entryIndex = -1;
			index = -1;
		}
		pDH->entryIndex = i;
		index = i;

		/* the entry's path: the directory's, then the entry's last element */
		n = fiosStrlen(name) - 1;
		pName = name + n;
		while (*pName != '/' && n != 0) {
			pName--;
			n--;
		}
		if (*pName == '/')
			pName = name + n + 1;
		fiosStrlcpy(pScratch, fiosPathGet(&pDH->path), sizeof(name));
		fiosStrlcat(pScratch, "/", sizeof(name));
		fiosStrlcat(pScratch, pName, sizeof(name));
		fiosPathNormalize(name, pScratch, sizeof(name));
		length = fiosStrlen(name);
		nameStart = length - 1;
		while (name[nameStart] != '/' && name[nameStart] != '\\' && nameStart != 0)
			nameStart--;
		if (isSeparator(name[nameStart]))
			nameStart = (uint16_t)(nameStart + 1);

		/* an entry another archive mounted over this one hides is skipped */
		pFound = NULL;
		pEntry = NULL;
		entryPath.path.Clear();
		entryPath.path.SetPointer(name);
		LookupPath(&entryPath.path, &pFound, &pEntry);
		if (pFound == pTracker && pChunk->type == CHUNK_DH_READ) {
			size_t pathLength;

			pOut = (SceFiosDirEntry *)pChunk->pBuf;
			if (pEntry != NULL) {
				pOut->fileSize = pTracker->header.GetFileSize(pTracker->pTOC, pEntry);
				pOut->statFlags = SCE_FIOS_STATUS_READABLE;
			} else {
				pOut->fileSize = 0;
				pOut->statFlags = SCE_FIOS_STATUS_DIRECTORY | SCE_FIOS_STATUS_READABLE;
			}
			err = SCE_OK;
			pathLength = fiosPathNormalize(pOut->fullPath, name, sizeof(name));
			pOut->fullPathLength = pathLength;
			pOut->reserved[2] = 0;
			pOut->nameLength = length - nameStart;
			pOut->offsetToName = pathLength - (length - nameStart);
			pOut->reserved[1] = 0;
			pOut->reserved[0] = 0;
			entryPath.path.Clear();
			break;
		}
		entryPath.path.Clear();
	}
done:
	if ((pDH->flags & FIOS_DH_EOF) && (flags & SCE_FIOS_PSARC_DEARCHIVER_MOUNT_TRANSLUCENT) &&
	    pDH->nativeDH != 0) {
		uint16_t callbackID;
		ReadDirData *pData;
		FiosChunk *pCallback;

		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s Trying loose files. Dir = %s",
			   "ReadDirectory", fiosPathGet(&pDH->path));
		s_pDHTable->Lookup(pDH->nativeDH);
		callbackID = fiosChunkAllocateType(CHUNK_CALLBACK);
		pData = (ReadDirData *)fiosChunkAllocateBuffer(callbackID, sizeof(ReadDirData));
		if (callbackID == CHUNKID_INVALID || pData == NULL) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s Failed to allocate data",
				   "ReadDirectory");
			err = SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
			goto eof;
		}
		pCallback = fiosChunkAt(callbackID);
		pCallback->callback.pData = pData;
		pCallback->callback.pCallback = ReadDirCallback;
		pData->pDearchiver = this;
		pData->pDH = pDH;
		pData->readChunk = CHUNKID_INVALID;
		pData->type = pChunk->type;
		pData->pEntry = (SceFiosDirEntry *)pChunk->pBuf;
		pData->reqCount = pChunk->reqCount;
		pData->err = err;
		*pOutCallback = callbackID;
		return SCE_OK;
	}
	if (err == SCE_OK)
		return err;
eof:
	pDH->flags |= FIOS_DH_EOF;
	return err;
}

/* "OpenDirectory": open the native directory under a directory of the archives */
int PsarcDearchiver::OpenDirectory(FiosDH *pDH, FiosChunk *pChunk, uint16_t *pOutCallback)
{
	uint16_t callbackID, openID;
	OpenDirData *pData;
	SceFiosDH nativeDH;
	FiosChunk *pCallback, *pOpen;
	FiosDH *pNative;

	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
		   "%-15s Opening underlying native directory. Dir = %s", "OpenDirectory",
		   fiosPathGet(&pDH->path));
	callbackID = fiosChunkAllocateType(CHUNK_CALLBACK);
	pData = (OpenDirData *)fiosChunkAllocateBuffer(callbackID, sizeof(OpenDirData));
	if (callbackID == CHUNKID_INVALID || pData == NULL) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s Failed to allocate callback or callback data", "OpenDirectory");
		return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
	}
	openID = fiosChunkAllocateChild(callbackID, CHUNK_DH_OPEN);
	if (openID == CHUNKID_INVALID) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s Failed to allocate open chunk",
			   "OpenDirectory");
		fiosChunkDelete(callbackID);
		return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
	}
	nativeDH = s_pDHTable->Allocate();
	if (nativeDH == 0) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s Failed to allocate native DH",
			   "OpenDirectory");
		fiosChunkDelete(callbackID);
		return SCE_FIOS_ERROR_CANT_ALLOCATE_DH;
	}
	pDH->nativeDH = nativeDH;
	pOpen = fiosChunkAt(openID);
	pOpen->open.h = nativeDH;
	pOpen->open.pPath = &pDH->path;
	pNative = (FiosDH *)s_pDHTable->Lookup(nativeDH);
	pNative->nativeDH = SCE_FIOS_NATIVEDH_INVALID;
	pNative->pBuf = pDH->pBuf;
	pNative->bufSize = pDH->bufSize;
	pNative->path.SetPointer(fiosPathGet(&pDH->path));
	pCallback = fiosChunkAt(callbackID);
	pCallback->callback.pData = pData;
	pCallback->callback.pCallback = OpenDirCallback;
	pCallback->filtered = 1;
	pData->openChunk = openID;
	pData->pDH = pDH;
	if (pOutCallback != NULL)
		*pOutCallback = callbackID;
	return SCE_OK;
}

/* a block's decompression finished: the scheduler looks at its chunk again */
static void blockJobDone(FiosZlibJob *job)
{
	g_fiosScheduler.Kick();
}

/*
 * Set up the read of one of the entry's blocks: decompressID's job and readID's read. 1 when
 * it did, 0 when the block holds nothing of the range read, else an error.
 */
int ArchiveRead::CreateBlockRead(SceFiosOffset block, uint16_t decompressID, uint16_t readID)
{
	SceFiosOffset blockSize = this->blockSize;
	SceFiosOffset blockStart = block * blockSize;
	FiosCodec *pCodec = this->pCodec;
	ArchiveTracker *pTracker = this->pTracker;
	const uint8_t *pEntry = this->pEntry;
	uint8_t *pBuf = this->pBuf;
	SceFiosSize *pActCount = this->pActCount;
	SceFiosOffset blockEnd = blockSize + blockStart;
	SceFiosOffset start = this->start;
	SceFiosOffset copyStart, copyEnd, copyLength, offset = 0, sectorStart;
	uint64_t srcSize = 0, sectorLength;
	uint32_t copyOffset, mask, skip, blockLength;
	uint16_t bufferID, flags;
	bool compressed, aligned, partial, inPlaceWhole, inPlacePartial, ownInput, ownOutput;
	uint8_t *pDst, *pSrc;
	FiosChunk *pDecompress, *pReadChunk, *pBuffer;
	FiosZlibJob *pJob;

	if (blockEnd > fileSize)
		blockEnd = fileSize;
	if (blockStart >= fileSize)
		return 0;
	copyStart = start;
	copyEnd = end;
	if (blockStart > copyStart)
		copyStart = blockStart;
	if (blockEnd < copyStart)
		copyStart = blockEnd;
	if (blockStart > copyEnd)
		copyEnd = blockStart;
	if (blockEnd < copyEnd)
		copyEnd = blockEnd;
	copyLength = copyEnd - copyStart;
	if (copyLength <= 0)
		return 0;
	compressed = false;
	if (!pTracker->header.GetBlock(pTracker->pTOC, pEntry, pTracker->pBlockSizes,
				       (uint32_t)block, (uint32_t)blockSize, &srcSize, &offset,
				       &compressed))
		return SCE_FIOS_ERROR_BAD_ARCHIVE;
	offset += pTracker->baseOffset;
	copyOffset = copyStart - blockStart;
	pDst = pBuf + (copyStart - start);
	mask = (1 << pCodec->alignShift) - 1;
	aligned = ((uintptr_t)pDst & mask) == 0;
	partial = blockStart != copyStart || blockEnd != copyEnd;
	pDecompress = fiosChunkAt(decompressID);
	pJob = pDecompress->decompress.pJob;
	pDecompress->decompress.pRead = this;
	sceClibMemset(pJob, 0, sizeof(*pJob));
	pReadChunk = fiosChunkAt(readID);
	if (!compressed) {
		pReadChunk->h = pTracker->fh;
		pReadChunk->offset = offset + copyOffset;
		pReadChunk->reqCount = copyLength;
		pReadChunk->pActCount = pActCount;
		pReadChunk->pBuf = pDst;
		return 1;
	}
	pReadChunk->whole = 1;
	flags = pCodec->flags;
	inPlaceWhole = srcSize <= (uint64_t)copyLength && aligned && !partial &&
		       (flags & FIOS_CODEC_IN_PLACE);
	inPlacePartial = srcSize <= (uint64_t)copyLength && aligned && partial &&
			 (flags & FIOS_CODEC_IN_PLACE) && (flags & FIOS_CODEC_PARTIAL);
	ownInput = !(inPlaceWhole | inPlacePartial);
	ownOutput = (flags & FIOS_CODEC_NEEDS_BUFFER) ||
		    (partial && !(flags & FIOS_CODEC_PARTIAL)) || !aligned;
	bufferID = CHUNKID_INVALID;
	if (pJob == NULL)
		return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
	pJob->next = NULL;
	pJob->codec = pCodec->codec;
	pJob->flags = FIOS_ZLIB_JOB_VALID;
	pJob->reserved0 = 0;
	pJob->reserved1 = 0;
	pJob->pCallback = blockJobDone;
	blockLength = blockEnd - blockStart;
	pSrc = NULL;
	if (!ownInput)
		pSrc = (uint8_t *)((uintptr_t)(pDst + copyLength - srcSize) & ~mask);
	pJob->pSrc = pSrc;
	pJob->srcSize = srcSize;
	pJob->pDst = (ownOutput || (flags & FIOS_CODEC_PARTIAL)) ? NULL : pDst;
	pJob->pCopyDst = pDst;
	pJob->size = blockLength;
	pJob->copyOffset = copyOffset;
	pJob->copyLength = copyLength;
	pJob->result = 0;
	if (ownInput || ownOutput) {
		bufferID = fiosChunkAllocateChild(readID, CHUNK_CALLBACK);
		if (bufferID == CHUNKID_INVALID)
			return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
		pBuffer = fiosChunkAt(bufferID);
		pBuffer->scheduled = 1;
		pBuffer->completed = 1;
		pBuffer->blockBuffer.pCallback = BufferCallback;
		pBuffer->blockBuffer.pDearchiver = pDearchiver;
		pBuffer->blockBuffer.readChunk = readID;
		pBuffer->blockBuffer.decompressChunk = decompressID;
		pBuffer->blockBuffer.buffers = (ownInput ? BLOCK_BUFFER_INPUT : 0) |
					       (ownOutput ? BLOCK_BUFFER_OUTPUT : 0);
	}
	pReadChunk->bufferChunk = bufferID;
	pReadChunk->h = pTracker->fh;
	/* read whole sectors where the bytes around the block fit */
	sectorStart = offset & ~(SceFiosOffset)0x1ff;
	skip = 0;
	sectorLength = (srcSize + (offset - sectorStart) + 0x1ff) & ~(uint64_t)0x1ff;
	if (ownInput ? sectorLength <= pDearchiver->workBufferSize / 3 :
		       sectorLength <= (uint64_t)copyLength) {
		skip = offset - sectorStart;
		pReadChunk->offset = sectorStart;
		pReadChunk->reqCount = sectorLength;
		if (skip != 0)
			goto read;
	}
	pReadChunk->offset = offset;
	pReadChunk->reqCount = srcSize;
read:
	pReadChunk->pActCount = NULL;
	pReadChunk->pBuf = (uint8_t *)pJob->pSrc;
	pDecompress->decompress.pCallback = DecompressCallback;
	pDecompress->decompress.pCodec = pCodec;
	pDecompress->decompress.pJob = pJob;
	pDecompress->decompress.bufferChunk = bufferID;
	pDecompress->decompress.blockLength = blockEnd - blockStart;
	pDecompress->decompress.copyLength = copyLength;
	pDecompress->decompress.skip = skip;
	pDecompress->decompress.pActCount = pActCount;
	return 1;
}

/* the checks a mount buffer's tracker passes */
bool ArchiveTracker::IsValid()
{
	bool valid = this != NULL;

	valid &= ((uintptr_t)this & 7) == 0;
	if (valid) {
		valid = (intptr_t)manifest > (intptr_t)pTOC;
		valid &= header.IsValid();
		valid &= (intptr_t)pBlockSizes > (intptr_t)pMountPoint;
		valid &= s_pFHTable->Lookup(fh) != NULL;
		valid &= (intptr_t)pTOC > (intptr_t)pBlockSizes;
		valid &= (intptr_t)pManifestData > (intptr_t)manifest;
		valid &= pMountPoint != NULL;
		valid &= (intptr_t)pMountPoint > (intptr_t)this;
		valid &= header.sorted;
		valid &= baseOffset == 0;
		if (valid)
			return true;
	}
	sceFiosPrintf("FIOS2 ERROR: The archive mountBuffer at 0x%08llX is corrupt!\n",
		      (long long)(intptr_t)this);
	return false;
}

/* the reads of an entry's range: one read when it is stored, else its blocks' reads */
int PsarcDearchiver::CreateReadChunksUnlocked(uint16_t *pOutChunk, ArchiveTracker *pTracker,
					      const uint8_t *pEntry, uint8_t *pBuf,
					      SceFiosOffset offset, SceFiosSize length,
					      SceFiosSize *pActCount)
{
	SceFiosSize fileSize;
	SceFiosOffset start, end, firstBlock, lastBlock, block, blockOffset;
	uint32_t blockSize;
	uint64_t size;
	bool compressed;
	FiosCodec *pCodec;
	ArchiveRead *pRead;
	FiosZlibJob *pJob;
	FiosChunk *pChunk;
	uint16_t rootID, decompressID, readID, jobID;
	int reads, ret;

	if (!pTracker->IsValid())
		return SCE_FIOS_ERROR_BAD_ARCHIVE;
	fileSize = pTracker->header.GetFileSize(pTracker->pTOC, pEntry);
	start = offset;
	end = length + offset;
	if (offset < 0)
		return SCE_FIOS_ERROR_BAD_OFFSET;
	if (fileSize < start)
		start = fileSize;
	if (fileSize < end)
		end = fileSize;
	if (end < 0)
		end = 0;
	if (end <= start) {
		if (length == 0)
			return SCE_OK;
		if (fileSize == start)
			return SCE_FIOS_ERROR_EOF;
		return SCE_FIOS_ERROR_BAD_OFFSET;
	}
	blockSize = pTracker->header.blockSize;
	firstBlock = start / (SceFiosOffset)blockSize;
	lastBlock = end / (SceFiosOffset)blockSize;
	if (pTracker->header.IsStored(pTracker->pTOC, pEntry, pTracker->pBlockSizes, start,
				      end - start)) {
		size = 0;
		blockOffset = 0;
		compressed = false;
		if (!pTracker->header.GetBlock(pTracker->pTOC, pEntry, pTracker->pBlockSizes,
					       (uint32_t)firstBlock, blockSize, &size, &blockOffset,
					       &compressed))
			return SCE_FIOS_ERROR_BAD_ARCHIVE;
		readID = fiosChunkAllocateType(CHUNK_FH_READ);
		*pOutChunk = readID;
		if (readID == CHUNKID_INVALID)
			return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
		pChunk = fiosChunkAt(readID);
		pChunk->h = pTracker->fh;
		pChunk->offset = blockOffset + (uint32_t)(start - blockSize * firstBlock);
		pChunk->reqCount = end - start;
		pChunk->pActCount = pActCount;
		pChunk->pBuf = pBuf;
		return SCE_OK;
	}
	pCodec = fiosDearchiverFindCodec(pTracker->header.compression, 4);
	if (pCodec == NULL) {
		sceFiosPrintf("FIOS2 warning: cannot find a decompressor!\n");
		return SCE_FIOS_ERROR_DECOMPRESSION;
	}
	rootID = fiosChunkAllocateType(CHUNK_NULL);
	*pOutChunk = rootID;
	if (rootID == CHUNKID_INVALID)
		return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
	pRead = (ArchiveRead *)fiosChunkAllocateBuffer(rootID, sizeof(ArchiveRead));
	if (pRead == NULL)
		return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
	pRead->pCodec = pCodec;
	pRead->lastBlock = lastBlock;
	pRead->blockSize = blockSize;
	pRead->nextBlock = firstBlock;
	pRead->fileSize = fileSize;
	pRead->pTracker = pTracker;
	pRead->pEntry = pEntry;
	pRead->start = start;
	pRead->end = end;
	pRead->pBuf = pBuf;
	pRead->pActCount = pActCount;
	pRead->pDearchiver = this;
	reads = 0;
	decompressID = CHUNKID_INVALID;
	readID = CHUNKID_INVALID;
	for (block = firstBlock; block <= lastBlock && reads < s_initialReads; ) {
		if (decompressID == CHUNKID_INVALID) {
			decompressID = fiosChunkAllocateChild(rootID, CHUNK_CALLBACK);
			pJob = (FiosZlibJob *)fiosChunkAllocateBuffer(decompressID, sizeof(FiosZlibJob));
			readID = fiosChunkAllocateChild(decompressID, CHUNK_FH_READ);
			if (decompressID == CHUNKID_INVALID || pJob == NULL ||
			    readID == CHUNKID_INVALID)
				return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
			jobID = fiosChunkGetChild(decompressID, 0);
			pChunk = fiosChunkAt(decompressID);
			pChunk->decompress.jobChunk = jobID;
			pChunk->decompress.pJob = pJob;
			pChunk->decompress.pCallback = DecompressCallback;
			pChunk->decompress.pRead = pRead;
		}
		ret = pRead->CreateBlockRead(block++, decompressID, readID);
		if (ret < 0)
			return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
		if (ret == 0)
			continue;
		decompressID = CHUNKID_INVALID;
		reads++;
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s created %d initial reads",
		   "CreateReadChunksUnlocked", reads);
	pRead->nextBlock = block;
	if (decompressID != CHUNKID_INVALID)
		fiosChunkDelete(decompressID);
	return SCE_OK;
}

/* whether fh is one of the archives' files */
bool PsarcDearchiver::OwnsFH(SceFiosFH fh)
{
	ArchiveTracker *pTracker;

	ASSERT_DEARCHIVER_LOCKED();
	for (pTracker = pArchives; pTracker != NULL; pTracker = pTracker->pNext) {
		if (pTracker->fh == fh)
			return true;
	}
	return false;
}

/*
 * "FilterChunk": the dearchiver's part of a chunk. The chunk that runs in its place: the
 * chunk itself, the chunks that read the archive, or CHUNKID_INVALID when nothing needs to
 * run (CHUNKID_INVALID with the chunk's error set when it failed).
 */
uint16_t PsarcDearchiver::FilterChunk(uint16_t chunkID)
{
	uint16_t newID = chunkID, extraID = CHUNKID_INVALID;
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	SceFiosHandle h = fiosChunkGetHandle(chunkID);
	FiosPath *pPath = fiosChunkGetPath(chunkID);
	bool opaque = !(flags & SCE_FIOS_PSARC_DEARCHIVER_MOUNT_TRANSLUCENT);
	ArchiveTracker *pTracker, **ppPrev;
	const uint8_t *pEntry;
	FiosFH *pFH;
	FiosDH *pDH, *pNative;
	int found, err;

	if (pChunk->completed)
		return chunkID;
	switch (pChunk->type) {
	case CHUNK_FH_OPEN:
		s_dearchiverMutex.Lock();
		pTracker = NULL;
		pEntry = NULL;
		found = FINDRESULT_NOT_OURS;
		if (pArchives != NULL)
			found = LookupPath(pChunk->open.pPath, &pTracker, &pEntry);
		if (found == FINDRESULT_FILE) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s \"%s\" owns %s", "Open",
				   archiveName(pTracker), sceFiosDebugDumpFH(pChunk->open.h, NULL, 0));
			pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->open.h);
			pFH->pArchiver = this;
			pFH->archiveID = pTracker->id;
			pFH->entryIndex = (uint32_t)(pEntry - pTracker->pTOC) /
					  pTracker->header.tocEntrySize;
			pFH->size = pTracker->header.GetFileSize(pTracker->pTOC, pEntry);
			if (pFH->openParams.openFlags &
			    (SCE_FIOS_O_WRITE | SCE_FIOS_O_APPEND | SCE_FIOS_O_TRUNC)) {
				err = SCE_FIOS_ERROR_READ_ONLY;
				s_dearchiverMutex.Unlock();
				goto fail;
			}
			pFH->isOpen = 1;
			pChunk->scheduled = 1;
			pChunk->completed = 1;
			pChunk->open.date = pTracker->stat.modificationDate;
		} else if (found == FINDRESULT_DIR) {
			err = SCE_FIOS_ERROR_NOT_A_FILE;
			s_dearchiverMutex.Unlock();
			goto fail;
		} else if (opaque && found == FINDRESULT_PATH) {
			((FiosFH *)s_pFHTable->Lookup(pChunk->open.h))->isOpen = 1;
			err = SCE_FIOS_ERROR_BAD_PATH;
			s_dearchiverMutex.Unlock();
			goto fail;
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_FH_READ:
		s_dearchiverMutex.Lock();
		pTracker = NULL;
		pEntry = NULL;
		if (LookupFH(pChunk->h, &pTracker, &pEntry)) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
				   "%-15s reading from archive \"%s\" for %s", "Read",
				   archiveName(pTracker), sceFiosDebugDumpFH(pChunk->h, NULL, 0));
			err = CreateReadChunksUnlocked(&extraID, pTracker, pEntry, pChunk->pBuf,
						       pChunk->offset, pChunk->reqCount,
						       pChunk->pActCount);
			if (err != SCE_OK) {
				s_dearchiverMutex.Unlock();
				goto fail;
			}
			newID = extraID;
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_FH_WRITE:
		s_dearchiverMutex.Lock();
		if (LookupFH(h, NULL, NULL))
			goto readOnly;
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_FH_BARRIER:
	case CHUNK_PREFETCH:
		break;
	case CHUNK_FH_SYNC:
		s_dearchiverMutex.Lock();
		if (LookupFH(h, NULL, NULL))
			newID = CHUNKID_INVALID;
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_FH_CLOSE:
		s_dearchiverMutex.Lock();
		if (OwnsFH(pChunk->h)) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
				   "%-15s attempt to close an archive FH. Unmounting instead: %s",
				   "Close", sceFiosDebugDumpFH(pChunk->h, NULL, 0));
			s_dearchiverMutex.Unlock();
			goto unmount;
		}
		pTracker = NULL;
		pEntry = NULL;
		if (LookupFH(pChunk->h, &pTracker, &pEntry)) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s \"%s\" owns %s", "Close",
				   archiveName(pTracker), sceFiosDebugDumpFH(pChunk->h, NULL, 0));
			s_pFHTable->Free(pChunk->h);
			newID = CHUNKID_INVALID;
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_ARCHIVE_UNMOUNT:
	unmount:
		s_dearchiverMutex.Lock();
		ppPrev = &pArchives;
		for (pTracker = pArchives; pTracker != NULL; pTracker = pTracker->pNext) {
			if (pTracker->fh == pChunk->h)
				break;
			ppPrev = &pTracker->pNext;
		}
		if (pTracker != NULL) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s \"%s\" <- \"%s\"",
				   "Unmount", archiveName(pTracker), pTracker->pMountPoint);
			*ppPrev = pTracker->pNext;
		}
		s_dearchiverMutex.Unlock();
		if (pTracker != NULL) {
			extraID = fiosChunkAllocateType(CHUNK_FH_CLOSE);
			if (extraID == CHUNKID_INVALID) {
				err = SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
				goto fail;
			}
			fiosChunkAt(extraID)->h = pChunk->h;
			newID = extraID;
			break;
		}
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s error - not a mounted archive FH: %s", "Unmount",
			   sceFiosDebugDumpFH(pChunk->h, NULL, 0));
		err = SCE_FIOS_ERROR_BAD_FH;
		goto fail;
	case CHUNK_DH_OPEN:
		s_dearchiverMutex.Lock();
		pTracker = NULL;
		pDH = (FiosDH *)s_pDHTable->Lookup(pChunk->open.h);
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s pDH: %p, h: %d, path: %s",
			   "DHOpen", pDH, pChunk->open.h, fiosPathGet(pPath));
		found = LookupPath(pPath, &pTracker, NULL);
		if (found == FINDRESULT_DIR || (!opaque && found == FINDRESULT_PATH)) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s \"%s\" owns %s", "Open",
				   archiveName(pTracker), sceFiosDebugDumpDH(pChunk->open.h, NULL, 0));
			pDH->pArchiver = this;
			pDH->archiveID = pTracker->id;
			pDH->entryIndex = -1;
			newID = CHUNKID_INVALID;
			pDH->nativeDH = 0;
			if (!opaque) {
				err = OpenDirectory(pDH, pChunk, &newID);
				if (err != SCE_OK) {
					s_dearchiverMutex.Unlock();
					goto fail;
				}
			}
		} else if (found == FINDRESULT_FILE) {
			err = SCE_FIOS_ERROR_NOT_A_DIRECTORY;
			s_dearchiverMutex.Unlock();
			goto fail;
		} else if (opaque && found == FINDRESULT_PATH) {
			err = SCE_FIOS_ERROR_BAD_PATH;
			s_dearchiverMutex.Unlock();
			goto fail;
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_DH_READ:
	case CHUNK_DH_READ_BULK:
		s_dearchiverMutex.Lock();
		pTracker = NULL;
		if (LookupDH(h, &pTracker)) {
			err = ReadDirectory(pTracker, pChunk, &newID);
			if (err != SCE_OK) {
				s_dearchiverMutex.Unlock();
				goto fail;
			}
			if (newID == chunkID)
				newID = CHUNKID_INVALID;
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_DH_CLOSE:
		s_dearchiverMutex.Lock();
		pTracker = NULL;
		if (LookupDH(h, &pTracker)) {
			newID = CHUNKID_INVALID;
			pDH = (FiosDH *)s_pDHTable->Lookup(h);
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
				   "%-15s pDH: %p, h: %d, m_nativeDH: %lld", "Close", pDH, h,
				   (long long)pDH->nativeDH);
			if (pDH->nativeDH != 0) {
				pNative = (FiosDH *)s_pDHTable->Lookup(pDH->nativeDH);
				if (pNative == NULL || pNative->nativeDH == SCE_FIOS_NATIVEDH_INVALID) {
					s_pDHTable->Free(pDH->nativeDH);
				} else {
					newID = fiosChunkAllocateType(CHUNK_DH_CLOSE);
					fiosChunkAt(newID)->h = pDH->nativeDH;
				}
			}
			s_pDHTable->Free(h);
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_DIR_EXISTS:
		s_dearchiverMutex.Lock();
		found = FINDRESULT_NOT_OURS;
		if (pArchives != NULL &&
		    (found = LookupPath(pPath, NULL, NULL)) == FINDRESULT_DIR) {
			*(bool *)pChunk->query.pResult = true;
			newID = CHUNKID_INVALID;
		} else if (opaque && found == FINDRESULT_PATH) {
			*(bool *)pChunk->query.pResult = false;
			newID = CHUNKID_INVALID;
		} else if (found == FINDRESULT_FILE) {
			*(bool *)pChunk->query.pResult = false;
			newID = CHUNKID_INVALID;
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_DIR_CREATE:
	case CHUNK_DIR_DELETE:
	case CHUNK_FILE_DELETE:
	case CHUNK_DELETE:
	case CHUNK_RENAME:
		s_dearchiverMutex.Lock();
		if (pArchives == NULL) {
			s_dearchiverMutex.Unlock();
			break;
		}
		found = LookupPath(pPath, NULL, NULL);
		if (found == FINDRESULT_FILE && pChunk->type == CHUNK_DIR_DELETE) {
			err = SCE_FIOS_ERROR_NOT_A_DIRECTORY;
			s_dearchiverMutex.Unlock();
			goto fail;
		}
		if (found == FINDRESULT_DIR) {
			if (pChunk->type == CHUNK_FILE_DELETE)
				goto notAFile;
			if (pChunk->type == CHUNK_DIR_CREATE)
				goto badPath;
		}
		if (found == FINDRESULT_PATH) {
			if (opaque && pChunk->type != CHUNK_DIR_CREATE)
				goto badPath;
		} else if (found != FINDRESULT_NOT_OURS) {
			goto readOnly;
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_FILE_EXISTS:
		s_dearchiverMutex.Lock();
		found = FINDRESULT_NOT_OURS;
		if (pArchives != NULL &&
		    (found = LookupPath(pPath, NULL, NULL)) == FINDRESULT_FILE) {
			*(bool *)pChunk->query.pResult = true;
			newID = CHUNKID_INVALID;
		} else if (opaque && found == FINDRESULT_PATH) {
			*(bool *)pChunk->query.pResult = false;
			newID = CHUNKID_INVALID;
		} else if (found == FINDRESULT_DIR) {
			*(bool *)pChunk->query.pResult = false;
			newID = CHUNKID_INVALID;
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_FILE_SIZE:
		s_dearchiverMutex.Lock();
		found = FINDRESULT_NOT_OURS;
		pTracker = NULL;
		pEntry = NULL;
		if (pArchives != NULL) {
			found = LookupPath(pPath, &pTracker, &pEntry);
			if (found == FINDRESULT_FILE) {
				*(SceFiosSize *)pChunk->query.pResult =
					pTracker->header.GetFileSize(pTracker->pTOC, pEntry);
				newID = CHUNKID_INVALID;
				s_dearchiverMutex.Unlock();
				break;
			}
			if (found == FINDRESULT_DIR)
				goto notAFile;
		}
		if (opaque && found == FINDRESULT_PATH)
			goto badPath;
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_EXISTS:
		s_dearchiverMutex.Lock();
		found = FINDRESULT_NOT_OURS;
		if (pArchives != NULL &&
		    ((found = LookupPath(pPath, NULL, NULL)) == FINDRESULT_FILE ||
		     found == FINDRESULT_DIR)) {
			*(bool *)pChunk->query.pResult = true;
			newID = CHUNKID_INVALID;
		} else if (opaque && found == FINDRESULT_PATH) {
			*(bool *)pChunk->query.pResult = false;
			newID = CHUNKID_INVALID;
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_STAT:
		s_dearchiverMutex.Lock();
		pTracker = NULL;
		pEntry = NULL;
		if (LookupFH(h, &pTracker, &pEntry))
			goto stat;
		if (pPath != NULL) {
			found = FINDRESULT_NOT_OURS;
			if (pArchives != NULL &&
			    ((found = LookupPath(pPath, &pTracker, &pEntry)) == FINDRESULT_DIR ||
			     found == FINDRESULT_FILE)) {
stat:
				GetStat(pTracker, pEntry, pChunk->stat.pStatus);
				newID = CHUNKID_INVALID;
			} else if (opaque && found == FINDRESULT_PATH) {
				goto badPath;
			}
		}
		s_dearchiverMutex.Unlock();
		break;
	case CHUNK_TRUNCATE:
		s_dearchiverMutex.Lock();
		if (LookupFH(h, NULL, NULL))
			goto readOnly;
		if (pPath == NULL) {
			s_dearchiverMutex.Unlock();
			break;
		}
		found = FINDRESULT_NOT_OURS;
		if (pArchives != NULL) {
			found = LookupPath(pPath, NULL, NULL);
			if (found == FINDRESULT_DIR)
				goto notAFile;
		}
		if (opaque && found == FINDRESULT_PATH)
			goto badPath;
		if (found != FINDRESULT_FILE) {
			s_dearchiverMutex.Unlock();
			break;
		}
		goto readOnly;
	case CHUNK_RESOLVE:
		err = SCE_FIOS_ERROR_UNIMPLEMENTED;
		goto fail;
	case CHUNK_ARCHIVE_QUERY:
	case CHUNK_ARCHIVE_MOUNT: {
		uint16_t mountID, closeID, rootID, tocID, tocReadID, headerID, headerReadID;
		uint16_t statID, parentID, openID;
		SceFiosStat *pStat;
		uint8_t *pHeader;
		MountHeaderData *pHeaderData;
		MountTOCData *pTOCData;
		FiosChunk *pNew;

		if (pChunk->type == CHUNK_ARCHIVE_QUERY)
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s \"%s\"",
				   "GetMountBufferSize path ", fiosPathGet(pChunk->mount.pPath));
		else
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s \"%s\" <- \"%s\"",
				   "Mount path ", fiosPathGet(pChunk->mount.pPath),
				   pChunk->mount.pMountPoint);
		if (pChunk->mount.pMountBuffer != NULL)
			sceClibMemset(pChunk->mount.pMountBuffer, 0, pChunk->mount.mountBufferLength);
		mountID = CHUNKID_INVALID;
		if (pChunk->type == CHUNK_ARCHIVE_MOUNT) {
			closeID = fiosChunkAllocateType(CHUNK_FH_CLOSE);
			mountID = fiosChunkAllocateType(CHUNK_CALLBACK);
			rootID = mountID;
			extraID = mountID;
		} else {
			closeID = fiosChunkAllocateType(CHUNK_FH_CLOSE);
			extraID = closeID;
			rootID = closeID;
		}
		tocID = fiosChunkAllocateChild(rootID, CHUNK_CALLBACK);
		tocReadID = fiosChunkAllocateChild(tocID, CHUNK_FH_READ);
		headerID = fiosChunkAllocateChild(tocReadID, CHUNK_CALLBACK);
		headerReadID = fiosChunkAllocateChild(headerID, CHUNK_FH_READ);
		pStat = NULL;
		parentID = headerReadID;
		statID = CHUNKID_INVALID;
		if (pChunk->type == CHUNK_ARCHIVE_MOUNT) {
			statID = fiosChunkAllocateChild(headerReadID, CHUNK_STAT);
			parentID = statID;
			pStat = (SceFiosStat *)fiosChunkAllocateBuffer(statID, sizeof(SceFiosStat));
		}
		openID = fiosChunkAllocateChild(parentID, CHUNK_FH_OPEN);
		pHeader = (uint8_t *)fiosChunkAllocateBuffer(headerReadID, PSARC_HEADER_SIZE);
		pHeaderData = (MountHeaderData *)fiosChunkAllocateBuffer(headerID,
									 sizeof(MountHeaderData));
		pTOCData = (MountTOCData *)fiosChunkAllocateBuffer(tocReadID, sizeof(MountTOCData));
		if (openID == CHUNKID_INVALID || closeID == CHUNKID_INVALID || pHeader == NULL ||
		    (statID != CHUNKID_INVALID && pStat == NULL) ||
		    (pChunk->type == CHUNK_ARCHIVE_MOUNT && mountID == CHUNKID_INVALID) ||
		    headerReadID == CHUNKID_INVALID || headerID == CHUNKID_INVALID ||
		    pHeaderData == NULL || pTOCData == NULL) {
			err = SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
			goto fail;
		}
		pNew = fiosChunkAt(openID);
		pNew->open.h = pChunk->mount.fh;
		pNew->open.pPath = pChunk->mount.pPath;
		if (pChunk->type == CHUNK_ARCHIVE_MOUNT) {
			pNew = fiosChunkAt(statID);
			pNew->stat.h = pChunk->mount.fh;
			pNew->stat.pStatus = pStat;
		}
		pNew = fiosChunkAt(headerReadID);
		pNew->h = pChunk->mount.fh;
		pNew->offset = 0;
		pNew->reqCount = PSARC_HEADER_SIZE;
		pNew->pBuf = pHeader;
		pChunk = fiosChunkAt(headerID);
		pChunk->callback.pData = pHeaderData;
		pChunk->callback.pCallback = MountHeaderCallback;
		pChunk = fiosChunkAt(chunkID);
		pHeaderData->pHeader = pHeader;
		pHeaderData->pMountBuffer = pChunk->mount.pMountBuffer;
		pHeaderData->mountBufferLength = pChunk->mount.mountBufferLength;
		pHeaderData->pMountPoint = pChunk->mount.pMountPoint;
		pHeaderData->tocReadChunk = tocReadID;
		pHeaderData->actCount = 0;
		pHeaderData->fh = pChunk->mount.fh;
		pHeaderData->pStat = pStat;
		pNew->pActCount = &pHeaderData->actCount;
		pNew = fiosChunkAt(tocReadID);
		pNew->h = pChunk->mount.fh;
		pNew->offset = 0;
		pNew->reqCount = 0;
		pNew->pBuf = NULL;
		{
			FiosChunk *pTOC = fiosChunkAt(tocID);

			pTOC->callback.pData = pTOCData;
			pTOC->callback.pCallback = MountTOCCallback;
		}
		pTOCData->pDearchiver = this;
		pTOCData->pHeader = pHeader;
		pTOCData->pMountBuffer = pChunk->mount.pMountBuffer;
		pTOCData->mountBufferLength = pChunk->mount.mountBufferLength;
		pTOCData->pMountPoint = pChunk->mount.pMountPoint;
		pTOCData->pActCount = pChunk->mount.pActCount;
		pTOCData->tocReadChunk = tocReadID;
		pTOCData->mountChunk = mountID;
		pTOCData->actCount = 0;
		pNew->pActCount = &pTOCData->actCount;
		if (pChunk->type == CHUNK_ARCHIVE_MOUNT) {
			pNew = fiosChunkAt(mountID);
			pNew->archiveMount.pDearchiver = this;
			pNew->archiveMount.pHeader = pHeader;
			pNew->archiveMount.pCallback = MountCallback;
			pNew->archiveMount.pTracker = (ArchiveTracker *)pChunk->mount.pMountBuffer;
			pNew->archiveMount.closeChunk = closeID;
			pNew->archiveMount.actCount = 0;
		}
		fiosChunkAt(closeID)->h = pChunk->mount.fh;
		newID = extraID;
		break;
	}
	default:
		break;
	}
	if (newID != chunkID)
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s replacing %d with %s",
			   "FilterChunk", chunkID, fiosDebugDumpChunk(newID, NULL, 0));
	return newID;
notAFile:
	err = SCE_FIOS_ERROR_NOT_A_FILE;
	s_dearchiverMutex.Unlock();
	goto fail;
badPath:
	err = SCE_FIOS_ERROR_BAD_PATH;
	s_dearchiverMutex.Unlock();
	goto fail;
readOnly:
	err = SCE_FIOS_ERROR_READ_ONLY;
	s_dearchiverMutex.Unlock();
fail:
	fiosChunkDelete(extraID);
	fiosChunkSetError(newID, err, false);
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s failed with %s", "FilterChunk",
		   sceFiosDebugDumpError(err, NULL, 0));
	return CHUNKID_INVALID;
}

/* filter a chunk's children, each as a tree */
void PsarcDearchiver::FilterChildren(uint16_t chunkID)
{
	uint32_t count = 0, i = 0;

	while (fiosChunkGetChild(chunkID, count) != CHUNKID_INVALID)
		count++;
	if (count != 0) {
		uint16_t children[count];
		uint16_t *p = children;

		for (; i < count; i++) {
			uint16_t childID = fiosChunkGetChild(chunkID, i);

			if (childID == CHUNKID_INVALID)
				break;
			*p++ = childID;
		}
		p = children;
		do {
			FilterTree(*p++);
		} while (--count != 0);
	}
}

/*
 * Filter a chunk and its children until the chunk that runs in its place is one the
 * dearchiver made. A chunk that nothing replaces but that has children becomes a null chunk.
 */
__attribute__((noinline)) uint16_t PsarcDearchiver::FilterTree(uint16_t chunkID)
{
	uint16_t newID, nullID;
	bool again;

	do {
		again = false;
		FilterChildren(chunkID);
		newID = FilterChunk(chunkID);
		if (newID == chunkID)
			continue;
		if (newID != CHUNKID_INVALID) {
			fiosChunkReplace(chunkID, newID);
			again = !fiosChunkAt(newID)->filtered;
		} else if (fiosChunkGetChild(chunkID, 0) != CHUNKID_INVALID) {
			nullID = fiosChunkAllocateType(CHUNK_NULL);
			if (nullID == CHUNKID_INVALID) {
				fiosChunkSetError(chunkID, SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK, false);
				fiosChunkDelete(chunkID);
				return CHUNKID_INVALID;
			}
			fiosChunkReplace(chunkID, nullID);
		}
		fiosChunkDelete(chunkID);
		chunkID = newID;
	} while (again);
	return chunkID;
}

int PsarcDearchiver::Terminate()
{
	if (--s_initCount == 0) {
		s_dearchiverMutex.Term();
		s_corrupt = false;
		fiosDearchiverUnregisterCodec(&g_fiosZlibCodec);
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s ", "Terminate");
	return SCE_OK;
}

__attribute__((noinline)) int PsarcDearchiver::Init(SceFiosPsarcDearchiverContext *pContext)
{
	uint32_t i;
	int err;

	if (pContext == NULL || pContext->pWorkBuffer == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (pContext->sizeOfContext != sizeof(SceFiosPsarcDearchiverContext)) {
		sceFiosPrintf("FIOS2 ERROR: sizeOfContext %lld != expected %lld.\n",
			      (long long)pContext->sizeOfContext,
			      (long long)sizeof(SceFiosPsarcDearchiverContext));
		return SCE_FIOS_ERROR_BAD_SIZE;
	}
	if (pContext->workBufferSize == 0) {
		sceFiosPrintf("FIOS2 ERROR: workBufferSize too small\n");
		return SCE_FIOS_ERROR_BAD_SIZE;
	}
	if ((uintptr_t)pContext->pWorkBuffer & (SCE_FIOS_PSARC_DEARCHIVER_WORK_BUFFER_ALIGNMENT - 1)) {
		sceFiosPrintf("FIOS2 ERROR: The dearchiver's pWorkBuffer must be %d-byte aligned!\n",
			      SCE_FIOS_PSARC_DEARCHIVER_WORK_BUFFER_ALIGNMENT);
		return SCE_FIOS_ERROR_BAD_ALIGNMENT;
	}
	if (pContext->flags & ~SCE_FIOS_PSARC_DEARCHIVER_MOUNT_TRANSLUCENT) {
		sceFiosPrintf("FIOS2 ERROR: Invalid dearchiver flags (0x%08X)\n",
			      pContext->flags & ~SCE_FIOS_PSARC_DEARCHIVER_MOUNT_TRANSLUCENT);
		return SCE_FIOS_ERROR_BAD_FLAGS;
	}
	for (i = 0; i < 3; i++) {
		if (pContext->reserved[i] != 0) {
			sceFiosPrintf("FIOS2 ERROR: The dearchiver's context reserved[%u] field is not zero.\n",
				      i);
			return SCE_FIOS_ERROR_BAD_SIZE;
		}
	}
	if (++s_initCount == 1) {
		s_dearchiverMutex.Init(1, "SceFiosDearchiver");
		err = fiosDearchiverRegisterCodec(&g_fiosZlibCodec);
		if (err != SCE_OK) {
			sceFiosPrintf("FIOS2 ERROR: dearchiver codec registration failed with 0x%x\n",
				      err);
			return SCE_FIOS_ERROR_DECOMPRESSION;
		}
		s_initialReads = 4;
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s ", "Initialize");
	return SCE_OK;
}

/* the context is one Init accepted, and nothing has written over it */
__attribute__((noinline)) bool PsarcDearchiver::IsValid(SceFiosPsarcDearchiverContext *pContext)
{
	bool valid = !s_corrupt;

	valid &= pContext != NULL;
	valid &= ((uintptr_t)pContext & 3) == 0;
	if (valid) {
		valid &= (pContext->flags & ~SCE_FIOS_PSARC_DEARCHIVER_MOUNT_TRANSLUCENT) == 0;
		valid &= (pContext->reserved[1] & ~7) == 0;
		valid &= pContext->workBufferSize != 0;
		valid &= ((uintptr_t)pContext->pWorkBuffer &
			  (SCE_FIOS_PSARC_DEARCHIVER_WORK_BUFFER_ALIGNMENT - 1)) == 0;
		valid &= pContext->sizeOfContext == sizeof(SceFiosPsarcDearchiverContext);
		valid &= (pContext->reserved[0] & 7) == 0;
		valid &= pContext->pWorkBuffer != NULL;
		if (valid)
			return true;
	}
	if (!s_corrupt) {
		sceFiosPrintf("FIOS2 ERROR: The dearchiver's internal state is corrupt and it will be disabled!\n");
		sceFiosPrintf("FIOS2 ERROR: A common cause of this problem is declaring the SceFiosPsarcDearchiverContext on the stack. Both the SceFiosPsarcDearchiverContext and pWorkBuffer must remain valid while the I/O filter is in place!\n");
		s_corrupt = true;
	}
	return false;
}

int sceFiosIOFilterPsarcDearchiver(SceFiosPsarcDearchiverContext *pContext, int command,
				   void *pArgs)
{
	int ret = SCE_FIOS_ERROR_EVENT_NOT_HANDLED;

	if (command != FIOS_FILTER_INIT && !PsarcDearchiver::IsValid(pContext))
		return SCE_FIOS_ERROR_BAD_PTR;
	switch (command) {
	case FIOS_FILTER_CHUNK:
		if (pArgs != NULL) {
			*(uint16_t *)pArgs = ((PsarcDearchiver *)pContext)->FilterTree(*(uint16_t *)pArgs);
			ret = SCE_OK;
		}
		break;
	case FIOS_FILTER_INIT:
		ret = PsarcDearchiver::Init(pContext);
		break;
	case FIOS_FILTER_TERM:
		ret = PsarcDearchiver::Terminate();
		break;
	case FIOS_FILTER_DESCRIBE:
		if (pArgs != NULL) {
			fiosStrlcpy((char *)pArgs, "description not implemented", 256);
			ret = SCE_OK;
		}
		break;
	}
	return ret;
}

/*
 * "AllocateID": a mounting archive's ID. An archive already mounted with the same header, TOC
 * and manifest at the same offset shares its ID; the caller holds the dearchiver's mutex.
 */
uint32_t PsarcDearchiver::AllocateID(ArchiveTracker *pNew)
{
	ArchiveTracker *pTracker;

	ASSERT_DEARCHIVER_LOCKED();
	for (pTracker = pArchives; pTracker != NULL; pTracker = pTracker->pNext) {
		if (pTracker->baseOffset == pNew->baseOffset &&
		    pTracker->manifestLength == pNew->manifestLength &&
		    pTracker->manifestPaths == pNew->manifestPaths &&
		    sceClibMemcmp(&pTracker->header, &pNew->header, PSARC_HEADER_SIZE) == 0 &&
		    sceClibMemcmp(pTracker->pTOC, pNew->pTOC, pTracker->header.tocLength) == 0 &&
		    sceClibMemcmp(pTracker->pManifestData, pNew->pManifestData,
				  pTracker->manifestLength) == 0) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
				   "%-15s duplicate archives detected, assigning same ID to both. FHs 0x%08X (%s) and 0x%08X (%s).",
				   "AllocateID", pNew->fh, archiveName(pNew), pTracker->fh,
				   archiveName(pTracker));
			return pTracker->id;
		}
	}
	return s_nextArchiveID++;
}

/* the manifest's sort orders: PSARC_FLAG_IGNORE_CASE picks the second */
int compareManifestPaths(const void *a, const void *b)
{
	return fiosStrcmp(*(const char *const *)a, *(const char *const *)b);
}

int compareManifestPathsNoCase(const void *a, const void *b)
{
	return fiosStrcasecmp(*(const char *const *)a, *(const char *const *)b);
}

/* print a mounted archive: nothing calls it (Sony's binary keeps it) */
void ArchiveTracker::Dump()
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	const char *pMount = pMountPoint;
	uint32_t numFiles = header.numFiles;
	uint32_t i;

	sceFiosPrintf("ArchiveTracker %s:\n", fiosPathGet(&pFH->path));
	sceFiosPrintf(".id: %d\n", id);
	sceFiosPrintf(".pMountPoint: %s\n", pMount);
	sceFiosPrintf(".numFiles: %u (includes manifest)\n", numFiles);
	sceFiosPrintf(".manifestPaths: %u\n", manifestPaths);
	sceFiosPrintf(".manifest:\n");
	for (i = 0; i < manifestPaths; i++)
		sceFiosPrintf("   %s\n", manifest[i]);
}

/*
 * "MountHeaderCallback": the archive header is read. Check it, lay the mount buffer out
 * (the tracker, its mount point, the block sizes, the TOC, the manifest's pointers and data)
 * as far as it fits, and point the TOC's read at the buffer: the whole TOC, or for a buffer
 * too small (a query's), only its first entry, the manifest's, into a chunk buffer.
 */
int MountHeaderCallback(FiosChunk *pChunk, int run)
{
	MountHeaderData *pData;
	ArchiveTracker *pTracker;
	PsarcHeader header = PsarcHeader();
	SceFiosSize actCount, readLength;
	size_t length, mountPointLength, mountLength, size;
	size_t blockSizesLength = 0, tocLength = 0, manifestLength = 0;
	size_t trackerEnd, blockSizesEnd, tocEnd, manifestEnd;
	const char *pMountPoint;
	SceFiosStat *pStat;
	FiosChunk *pRead;
	uint8_t *pTOC;
	int err = SCE_OK;

	if (!run)
		return SCE_OK;
	pData = (MountHeaderData *)pChunk->callback.pData;
	actCount = pData->actCount;
	pTracker = (ArchiveTracker *)pData->pMountBuffer;
	header.Parse(pData->pHeader);
	if (header.IsValid()) {
		tocLength = (header.tocLength + 7) & ~7;
		blockSizesLength = (((uint32_t)(((uint64_t)header.tocLength -
						  header.numFiles * header.tocEntrySize) /
						 header.blockSizeBytes) + 15) / 16 * 5 + 7) & ~7;
		manifestLength = (header.numFiles * 4 + 3) & ~7;
	}
	length = pData->mountBufferLength;
	pStat = pData->pStat;
	pMountPoint = pData->pMountPoint;
	mountPointLength = 1023;
	if (pMountPoint != NULL)
		mountPointLength = fiosStrlen(pMountPoint);
	/* the mount point and its terminator, at least 256 bytes, in 8-byte units */
	size = mountPointLength + 8;
	if (mountPointLength + 1 < 256)
		size = 256 + 7;
	mountLength = size & ~7;
	trackerEnd = mountLength + sizeof(ArchiveTracker);
	blockSizesEnd = trackerEnd + blockSizesLength;
	tocEnd = blockSizesEnd + tocLength;
	manifestEnd = tocEnd + manifestLength;
	if (actCount < PSARC_HEADER_SIZE) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s failed, not enough bytes (%d) reading %s",
			   pMountPoint != NULL ? "Mount" : "GetMountBufferSize", (int)actCount,
			   "header");
		return SCE_FIOS_ERROR_EOF;
	}
	if (!header.IsValid() || header.compression != FIOS_CODEC_ZLIB) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s failed archive header check",
			   pMountPoint != NULL ? "Mount" : "GetMountBufferSize");
		return SCE_FIOS_ERROR_BAD_ARCHIVE;
	}
	if (length >= sizeof(ArchiveTracker)) {
		sceClibMemset(pTracker, 0, sizeof(ArchiveTracker));
		pTracker->header = header;
		pTracker->fh = pData->fh;
		pTracker->mountPointLength = mountPointLength;
		sceClibMemcpy(&pTracker->stat, pStat, sizeof(SceFiosStat));
	}
	if (length >= trackerEnd) {
		if (pMountPoint == NULL)
			return SCE_FIOS_ERROR_BAD_PTR;
		pTracker->mountPointLength = mountPointLength;
		pTracker->pMountPoint = (char *)(pTracker + 1);
		fiosStrlcpy(pTracker->pMountPoint, pMountPoint, mountLength);
	}
	if (length >= blockSizesEnd)
		pTracker->pBlockSizes = (uint8_t *)pTracker + trackerEnd;
	if (length >= tocEnd)
		pTracker->pTOC = (uint8_t *)pTracker + blockSizesEnd;
	if (length >= manifestEnd)
		pTracker->manifest = (char **)((uint8_t *)pTracker + tocEnd);
	if (length > manifestEnd)
		pTracker->pManifestData = (char *)pTracker + manifestEnd;
	if (length >= tocEnd) {
		readLength = header.tocLength;
		pTOC = (uint8_t *)pTracker + blockSizesEnd;
	} else {
		readLength = header.tocEntrySize;
		if (readLength > 0xffffffff)
			return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
		pTOC = (uint8_t *)fiosChunkAllocateBufferLocked(fiosChunkID(pChunk),
								(uint32_t)readLength);
		if (pTOC == NULL)
			return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
	}
	pRead = fiosChunkAt(pData->tocReadChunk);
	pRead->offset = header.tocOffset;
	pRead->reqCount = readLength;
	pRead->pBuf = pTOC;
	return err;
}

/*
 * "MountTOCCallback": the TOC (or a query's first entry) is read. Find the manifest's entry
 * and size the mount; for a mount whose buffer holds it, sort the TOC, keep the block sizes
 * and read the manifest under MountCallback's chunk.
 */
int MountTOCCallback(FiosChunk *pChunk, int run)
{
	MountTOCData *pData;
	ArchiveTracker *pTracker;
	PsarcDearchiver *pDearchiver;
	PsarcHeader header = PsarcHeader();
	SceFiosSize actCount, manifestSize, *pActCount;
	size_t length, mountPointLength, size, needed, total;
	size_t blockSizesLength = 0, tocLength = 0, manifestLength = 0;
	const char *pMountPoint;
	const uint8_t *pEntry;
	uint8_t *pTOC;
	uint16_t mountID, readID;
	FiosChunk *pMount;
	bool valid;
	int err;

	if (!run)
		return SCE_OK;
	pData = (MountTOCData *)pChunk->callback.pData;
	pTracker = (ArchiveTracker *)pData->pMountBuffer;
	actCount = pData->actCount;
	pDearchiver = pData->pDearchiver;
	header.Parse(pData->pHeader);
	valid = header.IsValid();
	mountID = pData->mountChunk;
	pMountPoint = pData->pMountPoint;
	pActCount = pData->pActCount;
	length = pData->mountBufferLength;
	if (valid) {
		blockSizesLength = (((uint32_t)(((uint64_t)header.tocLength -
						  header.numFiles * header.tocEntrySize) /
						 header.blockSizeBytes) + 15) / 16 * 5 + 7) & ~7;
		manifestLength = (header.numFiles * 4 + 3) & ~7;
		tocLength = (header.tocLength + 7) & ~7;
	}
	mountPointLength = 1023;
	if (pMountPoint != NULL)
		mountPointLength = fiosStrlen(pMountPoint);
	size = mountPointLength + 8;
	if (mountPointLength + 1 < 256)
		size = 256 + 7;
	total = (size & ~7) + blockSizesLength + tocLength + manifestLength +
		sizeof(ArchiveTracker);
	if (actCount <= 0) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s failed, not enough bytes (%d) reading %s",
			   pMountPoint != NULL ? "Mount" : "GetMountBufferSize", (int)actCount, "TOC");
		return SCE_FIOS_ERROR_EOF;
	}
	if (pDearchiver->workBufferSize / SCE_FIOS_PSARC_DEARCHIVER_TEMP_BUFFERS < header.blockSize) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s failed, archive block size of %lld but scratch space size of %lld",
			   pMountPoint != NULL ? "Mount" : "GetMountBufferSize",
			   (long long)header.blockSize,
			   (long long)(pDearchiver->workBufferSize /
				       SCE_FIOS_PSARC_DEARCHIVER_TEMP_BUFFERS));
		return SCE_FIOS_ERROR_BAD_SIZE;
	}
	pTOC = fiosChunkAt(pData->tocReadChunk)->pBuf;
	if (actCount == header.tocLength) {
		if (length >= sizeof(ArchiveTracker))
			pTracker->header.PackBlockSizes(pTOC, (uint8_t *)pTracker->pBlockSizes,
							header.blockSize);
		header.SortTOC(pTOC);
		if (length >= sizeof(ArchiveTracker))
			pTracker->header.sorted = true;
		pEntry = header.FindManifest(pTOC);
		if (pEntry == NULL) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
				   "%-15s failed, couldn't find manifest in TOC", "Mount");
			return SCE_FIOS_ERROR_BAD_ARCHIVE;
		}
	} else if (actCount == header.tocEntrySize) {
		uint8_t manifestHash[16] = {};

		pEntry = pTOC;
		if (sceClibMemcmp(manifestHash, pEntry, sizeof(manifestHash)) != 0) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
				   "%-15s failed, couldn't find manifest at first entry of TOC",
				   "GetMountBufferSize");
			return SCE_FIOS_ERROR_BAD_ARCHIVE;
		}
	} else {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s failed, not enough bytes (%d) reading %s",
			   pMountPoint != NULL ? "Mount" : "GetMountBufferSize", (int)actCount, "TOC");
		return SCE_FIOS_ERROR_EOF;
	}
	manifestSize = header.GetFileSize(pTOC, pEntry);
	if (manifestSize >= 0x7fffffff || manifestSize == 0)
		return SCE_FIOS_ERROR_BAD_ARCHIVE;
	needed = total + (size_t)manifestSize + 1;
	if (pActCount != NULL)
		*pActCount = needed;
	if (needed > length) {
		if (pMountPoint == NULL)
			return SCE_OK;
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s failed, need a bigger mountBuffer (need %d bytes, have %d)", "Mount",
			   needed, length);
		return SCE_FIOS_ERROR_BAD_SIZE;
	}
	pTracker->manifestLength = (uint32_t)manifestSize;
	pMount = fiosChunkAt(mountID);
	readID = CHUNKID_INVALID;
	err = pDearchiver->CreateReadChunksUnlocked(&readID, pTracker, pEntry,
						    (uint8_t *)pTracker + total, 0, manifestSize,
						    &pMount->archiveMount.actCount);
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s failed creating chunks to read manifest", "Mount");
		fiosChunkDeleteLocked(readID);
		return err;
	}
	readID = pDearchiver->FilterTree(readID);
	fiosChunkPrepareLocked(readID, pChunk->priority, pChunk->deadline);
	while (!((volatile FiosChunk *)pMount)->scheduled)
		Wws::Platform::SleepMilliseconds(5);
	if (!fiosChunkLinkLocked(mountID, readID)) {
		fiosChunkDeleteLocked(readID);
		return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
	}
	fiosScheduleTree(mountID, 0, false);
	return SCE_OK;
}

/*
 * "MountCallback": the manifest is read. Split it into its paths, sort them, and put the
 * archive on the dearchiver's list; a failed mount closes the archive file instead.
 */
int MountCallback(FiosChunk *pChunk, int run)
{
	ArchiveTracker *pTracker;
	PsarcDearchiver *pDearchiver;
	PsarcHeader header = PsarcHeader();
	SceFiosSize actCount;
	uint32_t maxPaths = 0, paths, length, i;
	uint16_t closeID;
	char *p;
	bool lineStart;
	int err;

	if (!run)
		return SCE_OK;
	pTracker = pChunk->archiveMount.pTracker;
	err = pChunk->archiveMount.err;
	actCount = pChunk->archiveMount.actCount;
	pDearchiver = pChunk->archiveMount.pDearchiver;
	header.Parse(pChunk->archiveMount.pHeader);
	if (header.IsValid())
		maxPaths = (header.numFiles * 4 + 3) & ~7;
	closeID = pChunk->archiveMount.closeChunk;
	if (err != SCE_OK) {
		s_chunkMutex.Lock();
		if (!fiosChunkLink(fiosChunkGetParent(fiosChunkID(pChunk), 0), closeID)) {
			fiosChunkDeallocate(closeID);
			sceFiosPrintf("FIOS2 Warning: unable to schedule a close after an archive mount failure.\n");
			s_chunkMutex.Unlock();
			return err;
		}
		s_chunkMutex.Unlock();
		fiosScheduleTree(closeID, 0, false);
		return err;
	}
	if (actCount != pTracker->manifestLength) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s failed, not enough bytes (%d) reading %s", "Mount", (int)actCount,
			   "manifest");
		return SCE_FIOS_ERROR_EOF;
	}
	maxPaths /= sizeof(char *);
	p = pTracker->pManifestData;
	lineStart = true;
	p[pTracker->manifestLength] = 0;
	paths = 0;
	length = pTracker->manifestLength;
	for (i = 0; i < length; i++, p++) {
		if (lineStart) {
			pTracker->manifest[paths++] = p;
			lineStart = false;
			if (paths > maxPaths) {
				FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
					   "%-15s failed, more entries in manifest than files in the archive!",
					   "Mount");
				return SCE_FIOS_ERROR_BAD_ARCHIVE;
			}
		}
		if (*p == '\n') {
			*p = 0;
			lineStart = true;
		}
	}
	pTracker->manifestPaths = paths;
	qsort(pTracker->manifest, paths, sizeof(char *),
	      (pTracker->header.archiveFlags & PSARC_FLAG_IGNORE_CASE) ? compareManifestPathsNoCase :
									    compareManifestPaths);
	s_dearchiverMutex.Lock();
	pTracker->id = pDearchiver->AllocateID(pTracker);
	pTracker->pNext = pDearchiver->pArchives;
	pDearchiver->pArchives = pTracker;
	s_dearchiverMutex.Unlock();
	fiosChunkDelete(closeID);
	return err;
}

/*
 * "OpenDirCallback": the native directory under a translucent mount is open. A failed open
 * only leaves the directory without its native part.
 */
int OpenDirCallback(FiosChunk *pChunk, int run)
{
	OpenDirData *pData;
	FiosChunk *pOpen;
	uint16_t chunkID;
	FiosDH *pDH;
	int err;

	if (!run)
		return SCE_OK;
	pData = (OpenDirData *)pChunk->callback.pData;
	chunkID = fiosChunkID(pChunk);
	pOpen = fiosChunkAt(pData->openChunk);
	pDH = pData->pDH;
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
		   "%-15s Open finished. pDH: %p, filesys h: %d, path: %s", "OpenDirCallback", pDH,
		   pOpen->open.h, fiosPathGet(pOpen->open.pPath));
	err = pChunk->callback.err;
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s Open failed with %s",
			   "OpenDirCallback", sceFiosDebugDumpError(pChunk->callback.err, NULL, 0));
		s_pDHTable->Free(pDH->nativeDH);
		pDH->nativeDH = 0;
		if (err != SCE_FIOS_ERROR_BAD_SIZE)
			err = SCE_OK;
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s returning err = %s",
		   "OpenDirCallback", sceFiosDebugDumpError(err, NULL, 0));
	fiosChunkSetError(chunkID, err, true);
	return err;
}

/*
 * "ReadDirCallback": the archives' entries are done (or a read of the native directory under
 * them finished). An entry a mounted archive hides is skipped: the native directory is read
 * again, under this callback, which then runs once more.
 */
int ReadDirCallback(FiosChunk *pChunk, int run)
{
	ReadDirData *pData;
	ArchiveTracker *pTracker;
	PsarcDearchiver *pDearchiver;
	FiosPathBuffer path;
	SceFiosDirEntry *pEntry;
	SceFiosSize reqCount;
	FiosChunk *pRead;
	FiosDH *pDH, *pNative;
	uint16_t chunkID, readID, newID;
	uint8_t type;
	int err, firstErr;

	if (!run)
		return SCE_OK;
	pData = (ReadDirData *)pChunk->callback.pData;
	s_chunkMutex.Lock();
	pDH = pData->pDH;
	readID = pData->readChunk;
	type = pData->type;
	pDearchiver = pData->pDearchiver;
	pEntry = pData->pEntry;
	reqCount = pData->reqCount;
	chunkID = fiosChunkID(pChunk);
	firstErr = pData->err;
	pTracker = NULL;
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s type = %d, original err = %s",
		   "ReadDirCallback", type, sceFiosDebugDumpError(firstErr, NULL, 0));
	if (readID == CHUNKID_INVALID) {
		if (type == CHUNK_DH_READ_BULK && firstErr == SCE_OK) {
			FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
				   "%-15s READ_BULK call, returning %s to let it flush first",
				   "ReadDirCallback", sceFiosDebugDumpError(SCE_OK, NULL, 0));
			pDH->flags &= ~FIOS_DH_EOF;
			s_chunkMutex.Unlock();
			return SCE_OK;
		}
		goto readNative;
	}
	s_dearchiverMutex.Lock();
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s read finished. err = %s ",
		   "ReadDirCallback", sceFiosDebugDumpError(pChunk->callback.err, NULL, 0));
	path.path.Init();
	s_pDHTable->Lookup(fiosChunkAt(readID)->h);
	err = pChunk->callback.err;
	if (err != SCE_OK) {
		if (err == SCE_FIOS_ERROR_EOF) {
			pDH->flags |= FIOS_DH_EOF;
			err = pChunk->callback.err;
		}
		path.path.Clear();
		s_dearchiverMutex.Unlock();
		s_chunkMutex.Unlock();
		return err;
	}
	if (type == CHUNK_DH_READ) {
		path.path.SetPointer(pEntry->fullPath);
		if (pDearchiver->LookupPath(&path.path, &pTracker, NULL) <= FINDRESULT_PATH) {
			pDH->flags &= ~FIOS_DH_EOF;
			fiosChunkSetError(chunkID, SCE_OK, true);
			path.path.Clear();
			s_dearchiverMutex.Unlock();
			s_chunkMutex.Unlock();
			return SCE_OK;
		}
	}
	path.path.Clear();
	s_dearchiverMutex.Unlock();
readNative:
	pNative = (FiosDH *)s_pDHTable->Lookup(pDH->nativeDH);
	if (pDH->nativeDH == 0 || pNative->nativeDH == SCE_FIOS_NATIVEDH_INVALID) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc",
			   "%-15s FileSysFH = SCE_FIOS_NATIVEDH_INVALID! ", "ReadDirCallback");
		pDH->flags |= FIOS_DH_EOF;
		fiosChunkSetError(chunkID, firstErr, true);
		s_chunkMutex.Unlock();
		return SCE_FIOS_ERROR_EOF;
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s starting new read",
		   "ReadDirCallback");
	if (readID != CHUNKID_INVALID)
		fiosChunkDelete(readID);
	newID = fiosChunkAllocateChild(chunkID, type);
	if (newID == CHUNKID_INVALID) {
		FIOS_TRACE(SCE_FIOS_PROFILE_DEARCHIVER, "psarc", "%-15s failed with %s",
			   "ReadDirCallback",
			   sceFiosDebugDumpError(SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK, NULL, 0));
		pDH->flags |= FIOS_DH_EOF;
		s_chunkMutex.Unlock();
		return SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK;
	}
	pRead = fiosChunkAt(newID);
	pRead->h = pDH->nativeDH;
	pRead->pBuf = (uint8_t *)pEntry;
	pRead->reqCount = reqCount;
	pData->readChunk = newID;
	fiosChunkPrepare(newID, pChunk->priority, pChunk->deadline);
	s_chunkMutex.Unlock();
	fiosScheduleTree(newID, 0, false);
	return FIOS_CALLBACK_AGAIN;
}
