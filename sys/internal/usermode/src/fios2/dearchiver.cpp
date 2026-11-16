/*
 * dearchiver.cpp: the PSARC dearchiver I/O filter (sceFiosIOFilterPsarcDearchiver) and the
 * codecs it decompresses with. The method names are Sony's, from the profiling messages
 * ("LookupPath", "Mount", ...); the class and other names are ours.
 */
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

static Wws::Platform::Mutex s_dearchiverMutex;	/* "SceFiosDearchiver", recursive */

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
