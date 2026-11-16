/*
 * execution.cpp: a chunk's execution. The schedulers run each ready chunk through Execute
 * with their executor table, one function per chunk type: the I/O schedulers' table does
 * the native I/O (SceIofilemgr through platform.cpp), the callback scheduler's only the
 * roots and callbacks. Sony's file name; "Execute" and the "sceFiosNative..." names are the
 * ones the profiling messages give, the rest are ours.
 */
#include <sce_atomic.h>
#include <kernel/iofilemgr_stat.h>
#include "fios2_internal.h"

/*
 * A profiling event around one native call (SceFiosProfileCallback): begun before the call,
 * reported when it returns, both only while a profile callback is set.
 */
struct IoProfile {
	SceFiosIoProfileData data;
	SceFiosProfilingEvent event;
};

static inline void ioProfileBegin(IoProfile *pProfile, FiosChunk *pChunk, const char *pPath,
				  int8_t eventType, SceFiosOffset offset, SceFiosSize length)
{
	if (g_FiosParams.pProfileCallback != NULL) {
		pProfile->data.offset = offset;
		pProfile->data.LBA = SCE_FIOS_INVALID_LBA;
		pProfile->data.length = length;
		pProfile->data.deadline = pChunk->deadline;
		pProfile->data.path = pPath;
		pProfile->data.chunkID = fiosChunkID(pChunk);
		pProfile->data.priority = pChunk->priority;
		pProfile->event.startTime = sceKernelGetProcessTimeWide();
		pProfile->event.eventType = eventType;
		pProfile->event.endTime = 0;
		pProfile->event.data = &pProfile->data;
	}
}

static inline void ioProfileEnd(IoProfile *pProfile)
{
	if (g_FiosParams.pProfileCallback != NULL) {
		pProfile->event.endTime = sceKernelGetProcessTimeWide();
		g_FiosParams.pProfileCallback(&pProfile->event);
	}
}

/* run a chunk: its type's executor, unless it is done or cancelled */
int Execute(uint16_t chunkID, ExecuteFunc *pTable)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	int err;
	uint8_t type;

	if (pChunk->completed) {
		FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s completed: %s", "Execute",
			   fiosDebugDumpChunk(chunkID, NULL, 0));
		err = SCE_OK;
	} else if (!pChunk->cancelled || pChunk->type == CHUNK_CALLBACK ||
		   pChunk->type == CHUNK_ROOT || pChunk->type == CHUNK_FH_CLOSE) {
		SceFiosSchedulerProfileData data;
		SceFiosProfilingEvent event;

		FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s starting: %s", "Execute",
			   fiosDebugDumpChunk(chunkID, NULL, 0));
		if (g_FiosParams.pProfileCallback != NULL) {
			data.chunkID = chunkID;
			data.chunkType = pChunk->type;
			data.threadId = Wws::Platform::GetCurrentThreadId();
		}
		if (g_FiosParams.pProfileCallback != NULL) {
			event.startTime = sceKernelGetProcessTimeWide();
			event.endTime = 0;
			event.eventType = SCE_FIOS_PROFILE_EVENT_EXECUTE;
			event.data = &data;
		}
		err = pTable[pChunk->type](pChunk);
		if (g_FiosParams.pProfileCallback != NULL) {
			event.endTime = sceKernelGetProcessTimeWide();
			g_FiosParams.pProfileCallback(&event);
		}
		FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s finished: err=%d, %s", "Execute",
			   err, fiosDebugDumpChunk(chunkID, NULL, 0));
	} else {
		FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk", "%-15s skipped/cancelled: %s", "Execute",
			   fiosDebugDumpChunk(chunkID, NULL, 0));
		err = SCE_OK;
	}

	type = pChunk->type;
	if (type == CHUNK_FH_OPEN) {
		FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->open.h);

		if (pFH != NULL)
			pFH->isOpen = 1;
	} else if (type == CHUNK_DH_OPEN) {
		FiosDH *pDH = (FiosDH *)s_pDHTable->Lookup(pChunk->open.h);

		if (pDH != NULL)
			pDH->isOpen = 1;
	} else if (type > CHUNK_FH_OPEN && type < CHUNK_FH_CLOSE) {
		FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(fiosChunkGetHandleLocked(chunkID));

		if (pFH != NULL && pFH->pendingIO > 0) {
			int pending = sceAtomicDecrement32Relaxed(&pFH->pendingIO) - 1;

			FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
				   "%-15s Chunk  %d decremented m_pendingIO = %d", "FH", chunkID,
				   pending);
		}
	} else if (type > CHUNK_DH_OPEN && type < CHUNK_DH_CLOSE) {
		FiosDH *pDH = (FiosDH *)s_pDHTable->Lookup(fiosChunkGetHandleLocked(chunkID));

		if (pDH != NULL && pDH->pendingIO > 0) {
			int pending = sceAtomicDecrement32Relaxed(&pDH->pendingIO) - 1;

			FIOS_TRACE(SCE_FIOS_PROFILE_CHUNK, "chunk",
				   "%-15s Chunk %d decremented m_pendingIO = %d", "DH", chunkID,
				   pending);
		}
	}
	return err;
}

/* a chunk with nothing to do */
static int executeNothing(FiosChunk *pChunk)
{
	return SCE_OK;
}

/* a chunk type the table does not run */
static int executeUnimplemented(FiosChunk *pChunk)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

/* an op's root: its tree is done, the file handle moves and the op completes */
static int executeRoot(FiosChunk *pChunk)
{
	FiosOp *pOp = (FiosOp *)s_pOpTable->Lookup(pChunk->op);
	int16_t flags;

	if (pOp == NULL) {
		sceFiosPrintf("FIOS2 unexpected NULL value for op pointer\n");
		return SCE_OK;
	}
	if (pOp->internalFlags & FIOS_OP_UPDATES_FH) {
		FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pOp->fh);

		if (pFH != NULL) {
			if (pOp->err == SCE_OK) {
				SceFiosOffset end = pOp->offset + pOp->actCount;

				if (pFH->size < end)
					pFH->size = end;
				if (pOp->what != OP_FH_PWRITEV)
					pFH->offset = end;
			} else if (pOp->err == SCE_FIOS_ERROR_EOF) {
				if (pOp->what != OP_FH_PWRITEV)
					pFH->offset = pFH->size;
			} else if (pOp->what != OP_FH_PWRITEV) {
				pFH->offset = -1;
			}
		}
	}
	flags = pOp->internalFlags;
	if ((flags & FIOS_OP_PRIVATE_FH) || ((flags & FIOS_OP_OWNS_HANDLE) && pOp->err != SCE_OK)) {
		SceFiosHandle h = pOp->fh;

		if (s_pFHTable->Lookup(h) != NULL)
			s_pFHTable->Free(h);
		else if (s_pDHTable->Lookup(h) != NULL)
			s_pDHTable->Free(h);
	}
	if (pOp->err != SCE_OK)
		fiosOpCompleteWithError(pOp, pOp->err);
	else
		fiosOpComplete(pOp);
	return SCE_OK;
}

/* a callback chunk: its callback, run */
static int executeCallback(FiosChunk *pChunk)
{
	return pChunk->callback.pCallback(pChunk, 1);
}

/* open a file and take its size and date */
static int executeFHOpen(FiosChunk *pChunk)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->open.h);
	const char *pPath;
	int32_t mode;
	uint32_t flags;
	SceFiosNativeFH fd;
	SceIoStat stat;
	IoProfile profile;
	int ret;

	if (pFH == NULL)
		return SCE_FIOS_ERROR_BAD_FH;
	pPath = fiosPathGet(pChunk->open.pPath);
	mode = pChunk->open.nativeMode;
	pFH->nativeFH = SCE_FIOS_NATIVEFH_INVALID;
	flags = pFH->openParams.openFlags;
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\",flags=0x%04X,mode=(octal)0%o)",
		   "sceFiosNativeFHOpen", pPath, flags, mode);
	ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_OPEN, 0, 0);
	ret = fiosPlatformOpen(&fd, pPath, flags, mode);
	ioProfileEnd(&profile);
	if (ret != SCE_OK)
		return fiosPlatformTranslateError(ret);

	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\")", "sceFiosNativeStat", pPath);
	ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_STAT, 0, 0);
	ret = fiosPlatformGetstat(pPath, &stat);
	ioProfileEnd(&profile);
	if (ret != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s error 0x%08X", "sceFiosNativeFHStat",
			   ret);
		ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_CLOSE, 0, 0);
		fiosPlatformClose(fd);
		ret = fiosPlatformTranslateError(ret);
		ioProfileEnd(&profile);
		return ret;
	}
	if (SCE_STM_ISDIR(stat.st_mode)) {
		ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_CLOSE, 0, 0);
		fiosPlatformClose(fd);
		ioProfileEnd(&profile);
		return SCE_FIOS_ERROR_NOT_A_FILE;
	}
	pChunk->open.date = fiosPlatformDateTimeToDate(&stat.st_mtime);
	pFH->nativeFH = fd;
	pFH->size = stat.st_size;
	return SCE_OK;
}

/* a read or a write at an offset: the bytes done are added to the op's count */
static int executeFHReadWrite(FiosChunk *pChunk)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->h);
	IoProfile profile;
	int64_t ret;

	if (pFH == NULL)
		return SCE_FIOS_ERROR_BAD_FH;
	if (pChunk->reqCount < 0)
		return SCE_FIOS_ERROR_BAD_SIZE;
	if (pChunk->type == CHUNK_FH_READ) {
		ioProfileBegin(&profile, pChunk, fiosPathGet(&pFH->path), SCE_FIOS_PROFILE_EVENT_READ,
			       pChunk->offset, pChunk->reqCount);
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io",
			   "%-15s (fd=%d=\"%s\",buf=0x%08llX,nbyte=%lld,offset=%08llX)",
			   "sceFiosNativeFHPread", pFH->nativeFH, fiosPathGet(&pFH->path),
			   (long long)(intptr_t)pChunk->pBuf, pChunk->reqCount, pChunk->offset);
		if (pChunk->reqCount == 0) {
			ret = 0;
		} else {
			if (pFH->nativeFH == SCE_FIOS_NATIVEFH_INVALID) {
				ioProfileEnd(&profile);
				return SCE_FIOS_ERROR_BAD_FH;
			}
			ret = 0;
			if (pChunk->offset < pFH->size)
				ret = fiosPlatformPread(pFH->nativeFH, pChunk->pBuf, pChunk->reqCount,
							pChunk->offset);
		}
		ioProfileEnd(&profile);
	} else {
		ioProfileBegin(&profile, pChunk, fiosPathGet(&pFH->path),
			       SCE_FIOS_PROFILE_EVENT_WRITE, pChunk->offset, pChunk->reqCount);
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io",
			   "%-15s (fd=%d=\"%s\",buf=0x%08llX,nbyte=%lld,offset=%08llX)",
			   "sceFiosNativeFHPwrite", pFH->nativeFH, fiosPathGet(&pFH->path),
			   (long long)(intptr_t)pChunk->pBuf, pChunk->reqCount, pChunk->offset);
		if (pChunk->reqCount == 0) {
			ret = 0;
		} else {
			ret = SCE_FIOS_ERROR_BAD_FH;
			if (pFH->nativeFH != SCE_FIOS_NATIVEFH_INVALID)
				ret = fiosPlatformPwrite(pFH->nativeFH, pChunk->pBuf, pChunk->reqCount,
							 pChunk->offset);
		}
		ioProfileEnd(&profile);
	}

	if (ret < 0) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s error 0x%08X",
			   pChunk->type == CHUNK_FH_READ ? "sceFiosNativeFHPread" :
							   "sceFiosNativeFHPwrite",
			   (int)ret);
		return fiosPlatformTranslateError((int)ret);
	}
	if (ret == 0 && pChunk->reqCount != 0) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s EOF",
			   pChunk->type == CHUNK_FH_READ ? "sceFiosNativeFHPread" :
							   "sceFiosNativeFHPwrite");
		return SCE_FIOS_ERROR_EOF;
	}
	if (ret > 0 && pChunk->type == CHUNK_FH_WRITE) {
		SceFiosOffset end = pChunk->offset + ret;

		if (end > pFH->size)
			pFH->size = end;
	}
	if (pChunk->pActCount != NULL)
		sceAtomicAdd64Relaxed(pChunk->pActCount, ret);
	return SCE_OK;
}

/* a barrier: the file's writes reach the device before the chunks after it run */
static int executeFHBarrier(FiosChunk *pChunk)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->h);

	if (pFH == NULL)
		return SCE_FIOS_ERROR_BAD_FH;
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (fd=%d=\"%s\")", "sceFiosNativeFHBarrier",
		   pFH->nativeFH, fiosPathGet(&pFH->path));
	return fiosPlatformTranslateError(fiosPlatformFlushByFd(pFH->nativeFH));
}

static int executeFHSync(FiosChunk *pChunk)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->h);
	IoProfile profile;
	int err;

	if (pFH == NULL)
		return SCE_FIOS_ERROR_BAD_FH;
	ioProfileBegin(&profile, pChunk, fiosPathGet(&pFH->path), SCE_FIOS_PROFILE_EVENT_SYNC, 0, 0);
	err = fiosPlatformSyncByFd(pFH->nativeFH);
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (fd=%d) path=\"%s\" err = 0x%x",
		   "sceFiosNativeFHSync", pFH->nativeFH, fiosPathGet(&pFH->path), err);
	err = fiosPlatformTranslateError(err);
	ioProfileEnd(&profile);
	return err;
}

/* close a file and free its handle */
static int executeFHClose(FiosChunk *pChunk)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->h);
	IoProfile profile;
	int err;

	if (pFH == NULL)
		return SCE_FIOS_ERROR_BAD_FH;
	ioProfileBegin(&profile, pChunk, fiosPathGet(&pFH->path), SCE_FIOS_PROFILE_EVENT_CLOSE, 0,
		       0);
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (fd=%d=\"%s\")", "sceFiosNativeFHClose",
		   pFH->nativeFH, fiosPathGet(&pFH->path));
	err = SCE_FIOS_ERROR_BAD_FH;
	if (pFH->nativeFH != SCE_FIOS_NATIVEFH_INVALID)
		err = fiosPlatformClose(pFH->nativeFH);
	s_pFHTable->Free(pChunk->h);
	err = fiosPlatformTranslateError(err);
	ioProfileEnd(&profile);
	return err;
}

static int executeDHOpen(FiosChunk *pChunk)
{
	FiosDH *pDH = (FiosDH *)s_pDHTable->Lookup(pChunk->open.h);
	const char *pPath;
	SceFiosNativeDH fd;
	IoProfile profile;
	int err;

	if (pDH == NULL)
		return SCE_FIOS_ERROR_BAD_DH;
	pPath = fiosPathGet(pChunk->open.pPath);
	ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_OPENDIR, 0, 0);
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\")", "sceFiosNativeDHOpen", pPath);
	err = fiosPlatformDopen(&fd, pPath);
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s error 0x%08X", "sceFiosNativeDHOpen",
			   err);
		err = fiosPlatformTranslateError(err);
		ioProfileEnd(&profile);
		return err;
	}
	pDH->nativeDH = fd;
	ioProfileEnd(&profile);
	return SCE_OK;
}

/* a native directory's next entry, its full path normalized */
static int executeDHRead(FiosChunk *pChunk)
{
	FiosDH *pDH = (FiosDH *)s_pDHTable->Lookup(pChunk->h);
	SceFiosDirEntry *pEntry;
	SceIoDirent dirent;
	char path[SCE_FIOS_PATH_MAX];
	IoProfile profile;
	int err;

	if (pDH == NULL)
		return SCE_FIOS_ERROR_BAD_DH;
	ioProfileBegin(&profile, pChunk, fiosPathGet(&pDH->path), SCE_FIOS_PROFILE_EVENT_READDIR, 0,
		       0);
	sceClibMemset(&dirent, 0, sizeof(dirent));
	pEntry = (SceFiosDirEntry *)pChunk->pBuf;
	sceClibMemset(pEntry, 0, sizeof(*pEntry));
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (fd=%d=\"%s\")", "sceFiosNativeDHRead",
		   pDH->nativeDH, fiosPathGet(&pDH->path));
	err = SCE_FIOS_ERROR_BAD_DH;
	if (pDH->nativeDH != SCE_FIOS_NATIVEDH_INVALID) {
		err = fiosPlatformDread(pDH->nativeDH, &dirent);
		if (err == 0) {
			FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s EOF", "sceFiosNativeDHRead");
			ioProfileEnd(&profile);
			return SCE_FIOS_ERROR_EOF;
		}
	}
	if (err <= 0) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s error 0x%08X", "sceFiosNativeDHRead",
			   err);
		err = fiosPlatformTranslateError(err);
		ioProfileEnd(&profile);
		return err;
	}

	fiosStrlcpy(path, fiosPathGet(&pDH->path), sizeof(path));
	fiosStrlcat(path, "/", sizeof(path));
	if (fiosStrlcat(path, dirent.d_name, sizeof(path)) >= sizeof(path)) {
		ioProfileEnd(&profile);
		return SCE_FIOS_ERROR_PATH_TOO_LONG;
	}
	pEntry->fullPathLength = fiosPathNormalize(pEntry->fullPath, path, sizeof(path));
	pEntry->nameLength = fiosStrlen(dirent.d_name);
	pEntry->offsetToName = pEntry->fullPathLength - pEntry->nameLength;
	if (pEntry->fullPathLength >= SCE_FIOS_PATH_MAX) {
		ioProfileEnd(&profile);
		return SCE_FIOS_ERROR_PATH_TOO_LONG;
	}
	sceAtomicIncrement64Relaxed((volatile int64_t *)&pDH->entriesRead);
	if (SCE_STM_ISDIR(dirent.d_stat.st_mode))
		pEntry->statFlags = SCE_FIOS_STATUS_DIRECTORY;
	pEntry->fileSize = dirent.d_stat.st_size;
	if ((sceAppMgrIsGameProgram() && (dirent.d_stat.st_mode & STAT_MODE_OWNER_READ)) ||
	    (!sceAppMgrIsGameProgram() && ((dirent.d_stat.st_mode & STAT_MODE_OWNER_READ) ||
					   (dirent.d_stat.st_mode & STAT_MODE_OTHERS_READ))))
		pEntry->statFlags |= SCE_FIOS_STATUS_READABLE;
	if ((sceAppMgrIsGameProgram() && (dirent.d_stat.st_mode & STAT_MODE_OWNER_WRITE)) ||
	    (!sceAppMgrIsGameProgram() && ((dirent.d_stat.st_mode & STAT_MODE_OWNER_WRITE) ||
					   (dirent.d_stat.st_mode & STAT_MODE_OTHERS_WRITE))))
		pEntry->statFlags |= SCE_FIOS_STATUS_WRITABLE;
	ioProfileEnd(&profile);
	return SCE_OK;
}

/* close a native directory and free its handle */
static int executeDHClose(FiosChunk *pChunk)
{
	FiosDH *pDH = (FiosDH *)s_pDHTable->Lookup(pChunk->h);
	IoProfile profile;
	int err;

	if (pDH == NULL)
		return SCE_FIOS_ERROR_BAD_DH;
	ioProfileBegin(&profile, pChunk, fiosPathGet(&pDH->path), SCE_FIOS_PROFILE_EVENT_CLOSEDIR,
		       0, 0);
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (fd=%d=\"%s\")", "sceFiosNativeDHClose",
		   pDH->nativeDH, fiosPathGet(&pDH->path));
	err = fiosPlatformTranslateError(fiosPlatformDclose(pDH->nativeDH));
	s_pDHTable->Free(pChunk->h);
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s error 0x%08X", "DHClose", err);
		ioProfileEnd(&profile);
		return err;
	}
	ioProfileEnd(&profile);
	return SCE_OK;
}

/* whether a directory, a file, or either is at a path */
static int executeExists(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->query.pPath);
	bool *pExists = (bool *)pChunk->query.pResult;
	SceIoStat stat;
	IoProfile profile;
	int err;

	*pExists = false;
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\")", "Exists", pPath);
	ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_STAT, 0, 0);
	err = fiosPlatformGetstat(pPath, &stat);
	ioProfileEnd(&profile);
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s error 0x%08X", "sceFiosNativeStat",
			   err);
	} else {
		uint8_t type;

		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s found, ISDIR = %d, ISFILE = %d",
			   "Exists", SCE_STM_ISDIR(stat.st_mode), SCE_STM_ISREG(stat.st_mode));
		type = pChunk->type;
		if (type == CHUNK_DIR_EXISTS && SCE_STM_ISDIR(stat.st_mode))
			*pExists = true;
		else if (type == CHUNK_FILE_EXISTS && SCE_STM_ISREG(stat.st_mode))
			*pExists = true;
		else if (type == CHUNK_EXISTS)
			*pExists = true;
	}
	return SCE_OK;
}

static int executeDirCreate(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->create.pPath);
	int32_t mode = pChunk->create.nativeMode;

	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\",mode=(octal)0%o)",
		   "sceFiosNativeDirCreate", pPath, mode);
	return fiosPlatformTranslateError(fiosPlatformMkdir(pPath, mode));
}

static int executeDirDelete(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->pPath);

	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\")", "DirDelete", pPath);
	return fiosPlatformTranslateError(fiosPlatformRmdir(pPath));
}

static int executeFileDelete(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->pPath);

	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\")", "FileDelete", pPath);
	return fiosPlatformTranslateError(fiosPlatformRemove(pPath));
}

static int executeFileSize(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->query.pPath);
	SceIoStat stat;
	IoProfile profile;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\")", "sceFiosNativeStat", pPath);
	ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_STAT, 0, 0);
	err = fiosPlatformGetstat(pPath, &stat);
	ioProfileEnd(&profile);
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s error 0x%08X", "sceFiosNativeStat",
			   err);
		return fiosPlatformTranslateError(err);
	}
	if (SCE_STM_ISDIR(stat.st_mode))
		return SCE_FIOS_ERROR_NOT_A_FILE;
	*(SceFiosSize *)pChunk->query.pResult = stat.st_size;
	return SCE_OK;
}

/* a file or an empty directory, whichever the path is */
static int executeDelete(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->pPath);
	SceIoStat stat;
	IoProfile profile;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\")", "Delete", pPath);
	ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_STAT, 0, 0);
	err = fiosPlatformGetstat(pPath, &stat);
	ioProfileEnd(&profile);
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s sceFiosNativeStat error 0x%08X",
			   "Delete", err);
		return fiosPlatformTranslateError(err);
	}
	if (SCE_STM_ISDIR(stat.st_mode))
		err = fiosPlatformRmdir(pPath);
	else
		err = fiosPlatformRemove(pPath);
	return fiosPlatformTranslateError(err);
}

/* a file's status, by its handle or its path */
static int executeStat(FiosChunk *pChunk)
{
	SceFiosStat *pStatus;
	SceIoStat stat;
	IoProfile profile;
	int err;

	if (pChunk->stat.h != 0) {
		FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->stat.h);

		if (pFH == NULL)
			return SCE_FIOS_ERROR_BAD_FH;
		ioProfileBegin(&profile, pChunk, fiosPathGet(&pFH->path), SCE_FIOS_PROFILE_EVENT_STAT,
			       0, 0);
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (fd=%d) path=\"%s\"",
			   "sceFiosNativeFHStat", pFH->nativeFH, fiosPathGet(&pFH->path));
		err = SCE_FIOS_ERROR_BAD_FH;
		if (pFH->nativeFH != SCE_FIOS_NATIVEFH_INVALID)
			err = fiosPlatformGetstatByFd(pFH->nativeFH, &stat);
		ioProfileEnd(&profile);
	} else {
		const char *pPath = fiosPathGet(pChunk->stat.pPath);

		ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_STAT, 0, 0);
		if (pPath == NULL) {
			ioProfileEnd(&profile);
			return SCE_FIOS_ERROR_BAD_PATH;
		}
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\")", "sceFiosNativeStat", pPath);
		err = fiosPlatformGetstat(pPath, &stat);
		ioProfileEnd(&profile);
	}
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s error 0x%08X",
			   "sceFiosNativeStat/sceFiosNativeFHStat", err);
		return fiosPlatformTranslateError(err);
	}
	pStatus = pChunk->stat.pStatus;
	sceClibMemset(pStatus, 0, sizeof(*pStatus));
	statFromIoStat(&stat, pStatus);
	return SCE_OK;
}

/* a file's new length, by its handle's path or its own */
static int executeTruncate(FiosChunk *pChunk)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->truncate.fh);
	SceFiosNativeFH fd = SCE_FIOS_NATIVEFH_INVALID;
	FiosPath *pPath;
	int err;

	if (pFH != NULL) {
		pPath = &pFH->path;
		fd = pFH->nativeFH;
	} else {
		pPath = pChunk->truncate.pPath;
	}
	if (pPath == NULL)
		return SCE_FIOS_ERROR_BAD_FH;
	if (pChunk->truncate.length < 0 || (uint64_t)pChunk->truncate.length > 0xffffffffULL)
		return SCE_FIOS_ERROR_BAD_SIZE;
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\",fd = %d, size=%lld)",
		   "sceFiosNativeTruncate", pPath->GetPath(), fd, pChunk->truncate.length);
	err = fiosPlatformTruncate(pPath->GetPath(), fd, pChunk->truncate.length);
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s error 0x%08X", "sceFiosNativeTruncate",
			   err);
		return fiosPlatformTranslateError(err);
	}
	if (pFH != NULL)
		pFH->size = pChunk->truncate.length;
	return SCE_OK;
}

/* sceFiosResolve: the tuple a path names, with the range asked for */
static int executeResolve(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->resolve.pPath);
	SceFiosTuple *pTuple;

	if (pPath == NULL)
		return SCE_FIOS_ERROR_BAD_PATH;
	pTuple = pChunk->resolve.pOutTuple;
	fiosStrlcpy(pTuple->path, pPath, sizeof(pTuple->path));
	pTuple->offset = pChunk->resolve.offset;
	pTuple->size = pChunk->resolve.size;
	return SCE_OK;
}

static int executeRename(FiosChunk *pChunk)
{
	const char *pOldPath = fiosPathGet(pChunk->rename.pPath);
	const char *pNewPath = pChunk->rename.pNewPath;

	if (pOldPath == NULL || pNewPath == NULL)
		return SCE_FIOS_ERROR_BAD_PATH;
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (old=\"%s\", new=\"%s\")",
		   "sceFiosNativeRename", pOldPath, pNewPath);
	return fiosPlatformTranslateError(fiosPlatformRename(pOldPath, pNewPath));
}

static int executeDevctl(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->ioctl.pPath);
	SceSize argLength = pChunk->ioctl.argLength;
	SceSize bufferLength = pChunk->ioctl.bufferLength;

	if (pPath == NULL)
		return SCE_FIOS_ERROR_BAD_PATH;
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io",
		   "%-15s (\"%s\",cmd=%d,arg=%p,arglen=%ld,buf=%p,buflen=%ld)",
		   "sceFiosNativeDevCtl", pPath, pChunk->ioctl.cmd, pChunk->ioctl.pArg,
		   argLength, pChunk->ioctl.pBuffer, bufferLength);
	return fiosPlatformTranslateError(fiosPlatformDevctl(pPath, pChunk->ioctl.cmd,
							      pChunk->ioctl.pArg, argLength,
							      pChunk->ioctl.pBuffer, bufferLength));
}

static int executeIoctl(FiosChunk *pChunk)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pChunk->ioctl.h);
	SceFiosNativeFH fd;

	if (pFH == NULL)
		return SCE_FIOS_ERROR_BAD_FH;
	fd = pFH->nativeFH;
	if (fd == SCE_FIOS_NATIVEFH_INVALID)
		return SCE_FIOS_ERROR_BAD_FH;
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io",
		   "%-15s (fh=%ld,cmd=%d,arg=%p,arglen=%ld,buf=%p,buflen=%ld)",
		   "sceFiosNativeIoCtl", fd, pChunk->ioctl.cmd, pChunk->ioctl.pArg,
		   (SceSize)pChunk->ioctl.argLength, pChunk->ioctl.pBuffer,
		   (SceSize)pChunk->ioctl.bufferLength);
	return fiosPlatformTranslateError(fiosPlatformIoctl(fd, pChunk->ioctl.cmd,
							     pChunk->ioctl.pArg,
							     pChunk->ioctl.argLength,
							     pChunk->ioctl.pBuffer,
							     pChunk->ioctl.bufferLength));
}

static int executeChstat(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->chstat.pPath);

	if (pPath == NULL) {
		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"Missing path!\")",
			   "sceFiosNativeChStat");
		return SCE_FIOS_ERROR_BAD_PATH;
	}
	return fiosPlatformTranslateError(fiosPlatformChstat(pPath, pChunk->chstat.pStatus,
							      pChunk->chstat.cbit));
}

/* sceFiosSync: a device's or path's files to the device */
static int executeSync(FiosChunk *pChunk)
{
	const char *pPath = fiosPathGet(pChunk->sync.pPath);
	int flag = pChunk->sync.flag;
	IoProfile profile;
	int err;

	ioProfileBegin(&profile, pChunk, pPath, SCE_FIOS_PROFILE_EVENT_SYNC, 0, 0);
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s for path=\"%s\", flags=0x%x",
		   "sceFiosNativeSync", pPath, flag);
	err = fiosPlatformTranslateError(fiosPlatformSync(pPath, flag));
	ioProfileEnd(&profile);
	return err;
}

/* the callback scheduler's executors: its chunks are the roots and callbacks */
ExecuteFunc s_callbackExecutors[] = {
	executeNothing,		/* CHUNK_NULL */
	executeRoot,		/* CHUNK_ROOT */
	executeUnimplemented,	/* CHUNK_PARENT */
	executeUnimplemented,	/* CHUNK_CHILD */
	executeCallback,	/* CHUNK_CALLBACK */
	executeUnimplemented,	/* CHUNK_BUFFER */
	executeUnimplemented,	/* CHUNK_FH_OPEN */
	executeUnimplemented,	/* CHUNK_FH_READ */
	executeUnimplemented,	/* CHUNK_FH_WRITE */
	executeUnimplemented,	/* CHUNK_FH_BARRIER */
	executeUnimplemented,	/* CHUNK_FH_SYNC */
	executeUnimplemented,	/* CHUNK_FH_CLOSE */
	executeUnimplemented,	/* CHUNK_DH_OPEN */
	executeUnimplemented,	/* CHUNK_DH_READ */
	executeUnimplemented,	/* CHUNK_DH_READ_BULK */
	executeUnimplemented,	/* CHUNK_DH_CLOSE */
	executeUnimplemented,	/* CHUNK_DIR_EXISTS */
	executeUnimplemented,	/* CHUNK_DIR_CREATE */
	executeUnimplemented,	/* CHUNK_DIR_DELETE */
	executeUnimplemented,	/* CHUNK_FILE_EXISTS */
	executeUnimplemented,	/* CHUNK_FILE_DELETE */
	executeUnimplemented,	/* CHUNK_FILE_SIZE */
	executeUnimplemented,	/* CHUNK_DELETE */
	executeUnimplemented,	/* CHUNK_EXISTS */
	executeUnimplemented,	/* CHUNK_STAT */
	executeUnimplemented,	/* CHUNK_TRUNCATE */
	executeUnimplemented,	/* CHUNK_PREFETCH */
	executeUnimplemented,	/* CHUNK_RESOLVE */
	executeUnimplemented,	/* CHUNK_RENAME */
	executeUnimplemented,	/* CHUNK_ARCHIVE_QUERY */
	executeUnimplemented,	/* CHUNK_ARCHIVE_MOUNT */
	NULL,			/* CHUNK_ARCHIVE_UNMOUNT */
	NULL,			/* CHUNK_CHSTAT */
	NULL,			/* CHUNK_DEVCTL */
	NULL,			/* CHUNK_IOCTL */
	NULL,			/* CHUNK_SYNC */
};

/* the I/O schedulers' executors */
ExecuteFunc s_nativeExecutors[] = {
	executeUnimplemented,	/* CHUNK_NULL */
	executeRoot,		/* CHUNK_ROOT */
	executeUnimplemented,	/* CHUNK_PARENT */
	executeUnimplemented,	/* CHUNK_CHILD */
	executeCallback,	/* CHUNK_CALLBACK */
	executeUnimplemented,	/* CHUNK_BUFFER */
	executeFHOpen,		/* CHUNK_FH_OPEN */
	executeFHReadWrite,	/* CHUNK_FH_READ */
	executeFHReadWrite,	/* CHUNK_FH_WRITE */
	executeFHBarrier,	/* CHUNK_FH_BARRIER */
	executeFHSync,		/* CHUNK_FH_SYNC */
	executeFHClose,		/* CHUNK_FH_CLOSE */
	executeDHOpen,		/* CHUNK_DH_OPEN */
	executeDHRead,		/* CHUNK_DH_READ */
	executeUnimplemented,	/* CHUNK_DH_READ_BULK */
	executeDHClose,		/* CHUNK_DH_CLOSE */
	executeExists,		/* CHUNK_DIR_EXISTS */
	executeDirCreate,	/* CHUNK_DIR_CREATE */
	executeDirDelete,	/* CHUNK_DIR_DELETE */
	executeExists,		/* CHUNK_FILE_EXISTS */
	executeFileDelete,	/* CHUNK_FILE_DELETE */
	executeFileSize,	/* CHUNK_FILE_SIZE */
	executeDelete,		/* CHUNK_DELETE */
	executeExists,		/* CHUNK_EXISTS */
	executeStat,		/* CHUNK_STAT */
	executeTruncate,	/* CHUNK_TRUNCATE */
	executeUnimplemented,	/* CHUNK_PREFETCH */
	executeResolve,		/* CHUNK_RESOLVE */
	executeRename,		/* CHUNK_RENAME */
	executeUnimplemented,	/* CHUNK_ARCHIVE_QUERY */
	executeUnimplemented,	/* CHUNK_ARCHIVE_MOUNT */
	executeUnimplemented,	/* CHUNK_ARCHIVE_UNMOUNT */
	executeChstat,		/* CHUNK_CHSTAT */
	executeDevctl,		/* CHUNK_DEVCTL */
	executeIoctl,		/* CHUNK_IOCTL */
	executeSync,		/* CHUNK_SYNC */
};
