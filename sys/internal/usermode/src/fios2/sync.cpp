/*
 * sync.cpp: the Sync calls (the SDK's fios2_api.h), each the op of its asynchronous call
 * waited for. Before sceFiosInitialize, and on a pass-through file handle, they go to the
 * platform layer directly instead.
 */
#include <sce_atomic.h>
#include "fios2_internal.h"

#define PASSTHRU_FD4	-1, -1, -1, -1
#define PASSTHRU_FD16	PASSTHRU_FD4, PASSTHRU_FD4, PASSTHRU_FD4, PASSTHRU_FD4
#define PASSTHRU_FD64	PASSTHRU_FD16, PASSTHRU_FD16, PASSTHRU_FD16, PASSTHRU_FD16

/* the pass-through file handles' native descriptors, -1 when free */
static volatile int32_t s_passthruFds[FIOS_PASSTHRU_FH_MAX] = {
	PASSTHRU_FD64, PASSTHRU_FD64, PASSTHRU_FD64, PASSTHRU_FD64
};

/* the attributes of a Sync call's op: the caller's or the defaults, its paths kept */
static inline void getSyncOpAttr(SceFiosOpAttr *pSyncAttr, const SceFiosOpAttr *pAttr)
{
	if (pAttr != NULL)
		*pSyncAttr = *pAttr;
	else
		sceFiosGetDefaultOpAttr(pSyncAttr);
	pSyncAttr->opflags |= SCE_FIOS_OPFLAG_PERSISTENTPATH | FIOS_OPFLAG_SYNC;
}

/* the same for an archive's op, which is not run as a Sync op */
static inline void getArchiveOpAttr(SceFiosOpAttr *pSyncAttr, const SceFiosOpAttr *pAttr)
{
	getSyncOpAttr(pSyncAttr, pAttr);
	pSyncAttr->opflags &= ~FIOS_OPFLAG_SYNC;
}

/* a free pass-through handle for fd, or 0 when all are taken */
static inline SceFiosFH allocatePassthruFH(SceFiosNativeFH fd)
{
	uint32_t i;

	for (i = 0; i < FIOS_PASSTHRU_FH_MAX; i++)
		if (s_passthruFds[i] == -1 &&
		    sceAtomicCompareAndSwap32Relaxed(&s_passthruFds[i], -1, fd) == -1)
			return FIOS_PASSTHRU_FH(i);
	return 0;
}

static inline SceFiosNativeFH passthruFd(SceFiosFH fh)
{
	return s_passthruFds[(uint8_t)fh];
}

static inline bool isDirectFH(SceFiosFH fh)
{
	return !g_FiosParams.initialized || fiosIsPassthruFH(fh);
}

bool sceFiosIsValidHandle(SceFiosHandle h)
{
	uint32_t table = FIOS_HANDLE_TABLE(h);

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosIsValidHandle");
	if (h <= 0 || table >= 3)
		return false;
	if (fiosIsPassthruFH(h))
		return true;
	if (((h >> 16) & 0x7fff) == 0x7fff && FIOS_HANDLE_TABLE(h) == 2)
		return true;
	return s_handleTables[table].Lookup(h) != NULL;
}

bool sceFiosFileExistsSync(const SceFiosOpAttr *pAttr, const char *pPath)
{
	bool exists = false;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileExistsSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		err = sceFiosOpSyncWait(sceFiosFileExists(&attr, pPath, &exists));
	} else {
		SceIoStat stat;

		err = fiosPlatformGetstat(pPath, &stat);
		if (err != 0)
			err = fiosPlatformTranslateError(err);
		else
			exists = SCE_STM_ISREG(stat.st_mode);
	}
	if (err != 0)
		exists = false;
	return exists;
}

SceFiosSize sceFiosFileGetSizeSync(const SceFiosOpAttr *pAttr, const char *pPath)
{
	SceFiosSize size = 0;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileGetSizeSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		err = sceFiosOpSyncWait(sceFiosFileGetSize(&attr, pPath, &size));
	} else {
		SceIoStat stat;

		err = fiosPlatformGetstat(pPath, &stat);
		if (err != 0)
			err = fiosPlatformTranslateError(err);
		else if (SCE_STM_ISREG(stat.st_mode))
			size = stat.st_size;
	}
	if (err == 0)
		return size;
	return err;
}

int sceFiosFileDeleteSync(const SceFiosOpAttr *pAttr, const char *pPath)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileDeleteSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosFileDelete(&attr, pPath));
	}
	err = fiosPlatformRemove(pPath);
	if (err != 0)
		err = fiosPlatformTranslateError(err);
	return err;
}

bool sceFiosDirectoryExistsSync(const SceFiosOpAttr *pAttr, const char *pPath)
{
	bool exists = false;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDirectoryExistsSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		err = sceFiosOpSyncWait(sceFiosDirectoryExists(&attr, pPath, &exists));
	} else {
		SceIoStat stat;

		err = fiosPlatformGetstat(pPath, &stat);
		if (err != 0)
			err = fiosPlatformTranslateError(err);
		else
			exists = SCE_STM_ISDIR(stat.st_mode);
	}
	if (err != 0)
		exists = false;
	return exists;
}

__attribute__((noinline))
int sceFiosDirectoryCreateWithModeSync(const SceFiosOpAttr *pAttr, const char *pPath,
				       int32_t nativeMode)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDirectoryCreateWithModeSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosDirectoryCreateWithMode(&attr, pPath, nativeMode));
	}
	err = fiosPlatformMkdir(pPath, nativeMode);
	if (err != 0)
		err = fiosPlatformTranslateError(err);
	return err;
}

int sceFiosDirectoryCreateSync(const SceFiosOpAttr *pAttr, const char *pPath)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDirectoryCreateSync");
	return sceFiosDirectoryCreateWithModeSync(pAttr, pPath, -1);
}

int sceFiosDirectoryDeleteSync(const SceFiosOpAttr *pAttr, const char *pPath)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDirectoryDeleteSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosDirectoryDelete(&attr, pPath));
	}
	err = fiosPlatformRmdir(pPath);
	if (err != 0)
		err = fiosPlatformTranslateError(err);
	return err;
}

bool sceFiosExistsSync(const SceFiosOpAttr *pAttr, const char *pPath)
{
	bool exists;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosExistsSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		err = sceFiosOpSyncWait(sceFiosExists(&attr, pPath, &exists));
	} else {
		SceIoStat stat;

		exists = true;
		err = fiosPlatformGetstat(pPath, &stat);
		if (err == 0)
			return exists;
		err = fiosPlatformTranslateError(err);
	}
	if (err != 0)
		exists = false;
	return exists;
}

int sceFiosRenameSync(const SceFiosOpAttr *pAttr, const char *pOldPath, const char *pNewPath)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosRenameSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosRename(&attr, pOldPath, pNewPath));
	}
	if (pOldPath == NULL || pNewPath == NULL)
		return SCE_FIOS_ERROR_BAD_PATH;
	err = fiosPlatformRename(pOldPath, pNewPath);
	if (err != 0)
		err = fiosPlatformTranslateError(err);
	return err;
}

int sceFiosResolveSync(const SceFiosOpAttr *pAttr, const SceFiosTuple *pInTuple,
		       SceFiosTuple *pOutTuple)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosResolveSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosResolve(&attr, pInTuple, pOutTuple));
	}
	if (pInTuple == NULL || pOutTuple == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	sceClibMemcpy(pOutTuple, pInTuple, sizeof(SceFiosTuple));
	return 0;
}

void fiosStatFromIoStat(const SceIoStat *pIoStat, SceFiosStat *pStat)
{
	statFromIoStat(pIoStat, pStat);
}

int sceFiosStatSync(const SceFiosOpAttr *pAttr, const char *pPath, SceFiosStat *pOutStatus)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosStatSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		err = sceFiosOpSyncWait(sceFiosStat(&attr, pPath, pOutStatus));
	} else {
		SceIoStat stat;

		if (pOutStatus == NULL)
			return SCE_FIOS_ERROR_BAD_PTR;
		sceClibMemset(pOutStatus, 0, sizeof(*pOutStatus));
		err = fiosPlatformGetstat(pPath, &stat);
		if (err != 0)
			err = fiosPlatformTranslateError(err);
		else
			fiosStatFromIoStat(&stat, pOutStatus);
	}
	return err;
}

int sceFiosDeleteSync(const SceFiosOpAttr *pAttr, const char *pPath)
{
	SceIoStat stat;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDeleteSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosDelete(&attr, pPath));
	}
	err = fiosPlatformGetstat(pPath, &stat);
	if (err == 0) {
		if (SCE_STM_ISDIR(stat.st_mode))
			err = fiosPlatformRmdir(pPath);
		else
			err = fiosPlatformRemove(pPath);
	}
	return fiosPlatformTranslateError(err);
}

SceFiosSize sceFiosFileReadSync(const SceFiosOpAttr *pAttr, const char *pPath, void *pBuf,
				SceFiosSize length, SceFiosOffset offset)
{
	SceFiosNativeFH fd;
	SceFiosSize result;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s (buf=0x%p,length=%lld)", "sceFiosFileReadSync",
		   pBuf, length);
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWaitForIO(sceFiosFileRead(&attr, pPath, pBuf, length, offset));
	}
	if (pBuf == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	fd = SCE_FIOS_NATIVEFH_INVALID;
	err = fiosPlatformOpen(&fd, pPath, SCE_FIOS_O_READ, -1);
	if (fd == SCE_FIOS_NATIVEFH_INVALID)
		return fiosPlatformTranslateError(err);
	if (length >= 0) {
		result = fiosPlatformPread(fd, pBuf, (SceSize)length, offset);
		if (result < 0)
			result = fiosPlatformTranslateError(result);
	} else {
		result = SCE_FIOS_ERROR_BAD_SIZE;
	}
	fiosPlatformClose(fd);
	return result;
}

SceFiosSize sceFiosFileWriteSync(const SceFiosOpAttr *pAttr, const char *pPath, const void *pBuf,
				 SceFiosSize length, SceFiosOffset offset)
{
	SceUID fd;
	SceFiosSize result;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s (buf=0x%p,length=%lld)", "sceFiosFileWriteSync",
		   pBuf, length);
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWaitForIO(sceFiosFileWrite(&attr, pPath, pBuf, length, offset));
	}
	if (pBuf == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	fd = sceIoOpen(pPath, SCE_O_RDWR | SCE_O_CREAT, 0606);
	if (fd <= 0)
		return fiosPlatformTranslateError(fd);
	if (length >= 0) {
		result = fiosPlatformPwrite(fd, pBuf, (SceSize)length, offset);
		if (result < 0)
			result = fiosPlatformTranslateError(result);
	} else {
		result = SCE_FIOS_ERROR_BAD_SIZE;
	}
	fiosPlatformClose(fd);
	return result;
}

int sceFiosFileTruncateSync(const SceFiosOpAttr *pAttr, const char *pPath, SceFiosSize length)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileTruncateSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosFileTruncate(&attr, pPath, length));
	}
	err = fiosPlatformTruncate(pPath, SCE_FIOS_NATIVEFH_INVALID, length);
	if (err != 0)
		err = fiosPlatformTranslateError(err);
	return err;
}

__attribute__((noinline))
int sceFiosFHOpenWithModeSync(const SceFiosOpAttr *pAttr, SceFiosFH *pOutFH, const char *pPath,
			      const SceFiosOpenParams *pOpenParams, int32_t nativeMode)
{
	SceFiosNativeFH fd;
	uint32_t flags;
	SceFiosFH fh;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHOpenWithModeSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosFHOpenWithMode(&attr, pOutFH, pPath, pOpenParams,
							       nativeMode));
	}
	if (pOutFH == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	fd = SCE_FIOS_NATIVEFH_INVALID;
	flags = SCE_FIOS_O_READ;
	if (pOpenParams != NULL)
		flags = pOpenParams->openFlags;
	err = fiosPlatformOpen(&fd, pPath, flags, nativeMode);
	if (fd == SCE_FIOS_NATIVEFH_INVALID)
		return fiosPlatformTranslateError(err);
	fh = allocatePassthruFH(fd);
	*pOutFH = fh;
	if (fh == 0) {
		err = SCE_FIOS_ERROR_CANT_ALLOCATE_FH;
		fiosPlatformClose(fd);
	}
	return err;
}

int sceFiosFHOpenSync(const SceFiosOpAttr *pAttr, SceFiosFH *pOutFH, const char *pPath,
		      const SceFiosOpenParams *pOpenParams)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHOpenSync");
	return sceFiosFHOpenWithModeSync(pAttr, pOutFH, pPath, pOpenParams, -1);
}

int sceFiosFHStatSync(const SceFiosOpAttr *pAttr, SceFiosFH fh, SceFiosStat *pOutStatus)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHStatSync");
	if (isDirectFH(fh)) {
		SceIoStat stat;

		if (pOutStatus == NULL)
			return SCE_FIOS_ERROR_BAD_PTR;
		sceClibMemset(pOutStatus, 0, sizeof(*pOutStatus));
		err = fiosPlatformGetstatByFd(passthruFd(fh), &stat);
		if (err != 0)
			err = fiosPlatformTranslateError(err);
		else
			fiosStatFromIoStat(&stat, pOutStatus);
	} else {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		err = sceFiosOpSyncWait(sceFiosFHStat(&attr, fh, pOutStatus));
	}
	return err;
}

int sceFiosFHTruncateSync(const SceFiosOpAttr *pAttr, SceFiosFH fh, SceFiosSize length)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHTruncateSync");
	if (isDirectFH(fh))
		return SCE_FIOS_ERROR_UNIMPLEMENTED;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosFHTruncate(&attr, fh, length));
}

int sceFiosFHSyncSync(const SceFiosOpAttr *pAttr, SceFiosFH fh)
{
	SceFiosOpAttr attr;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHSyncSync");
	if (isDirectFH(fh)) {
		err = fiosPlatformSyncByFd(passthruFd(fh));
		if (err != 0)
			err = fiosPlatformTranslateError(err);
		return err;
	}
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosFHSync(&attr, fh));
}

SceFiosSize sceFiosFHReadSync(const SceFiosOpAttr *pAttr, SceFiosFH fh, void *pBuf,
			      SceFiosSize length)
{
	SceFiosSize result;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s (buf=0x%p,length=%lld)", "sceFiosFHReadSync",
		   pBuf, length);
	if (length < 0)
		return SCE_FIOS_ERROR_BAD_SIZE;
	if (isDirectFH(fh)) {
		if (pBuf == NULL)
			return SCE_FIOS_ERROR_BAD_PTR;
		result = fiosPlatformRead(passthruFd(fh), pBuf, (SceSize)length);
		if (result < 0)
			result = fiosPlatformTranslateError(result);
	} else {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		result = sceFiosOpSyncWaitForIO(sceFiosFHRead(&attr, fh, pBuf, length));
	}
	return result;
}

SceFiosSize sceFiosFHWriteSync(const SceFiosOpAttr *pAttr, SceFiosFH fh, const void *pBuf,
			       SceFiosSize length)
{
	SceFiosSize result;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s (buf=0x%p,length=%lld)", "sceFiosFHWriteSync",
		   pBuf, length);
	if (length < 0)
		return SCE_FIOS_ERROR_BAD_SIZE;
	if (isDirectFH(fh)) {
		if (pBuf == NULL)
			return SCE_FIOS_ERROR_BAD_PTR;
		result = fiosPlatformWrite(passthruFd(fh), pBuf, (SceSize)length);
		if (result < 0)
			result = fiosPlatformTranslateError(result);
	} else {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		result = sceFiosOpSyncWaitForIO(sceFiosFHWrite(&attr, fh, pBuf, length));
	}
	return result;
}

/* Sony's pass-through vector calls read and write every buffer at offset 0 */
SceFiosSize sceFiosFHReadvSync(const SceFiosOpAttr *pAttr, SceFiosFH fh,
			       const SceFiosBuffer iov[], int iovcnt)
{
	SceFiosOpAttr attr;
	SceFiosSize total, n = 0;
	SceFiosNativeFH fd;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHReadvSync");
	if (iov == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (!isDirectFH(fh)) {
		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWaitForIO(sceFiosFHReadv(&attr, fh, iov, iovcnt));
	}
	fd = passthruFd(fh);
	for (total = 0; iovcnt > 0; iov++) {
		if (iov->pPtr == NULL)
			break;
		n = fiosPlatformPread(fd, iov->pPtr, iov->length, 0);
		if (n < 0)
			break;
		iovcnt--;
		total += n;
	}
	if (n < 0)
		total = fiosPlatformTranslateError(n);
	return total;
}

SceFiosSize sceFiosFHWritevSync(const SceFiosOpAttr *pAttr, SceFiosFH fh,
				const SceFiosBuffer iov[], int iovcnt)
{
	SceFiosOpAttr attr;
	SceFiosSize total, n = 0;
	SceFiosNativeFH fd;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHWritevSync");
	if (iov == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (!isDirectFH(fh)) {
		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWaitForIO(sceFiosFHWritev(&attr, fh, iov, iovcnt));
	}
	fd = passthruFd(fh);
	for (total = 0; iovcnt > 0; iov++) {
		if (iov->pPtr == NULL)
			break;
		n = fiosPlatformPwrite(fd, iov->pPtr, iov->length, 0);
		if (n < 0)
			break;
		iovcnt--;
		total += n;
	}
	if (n < 0)
		total = fiosPlatformTranslateError(n);
	return total;
}

SceFiosSize sceFiosFHPreadSync(const SceFiosOpAttr *pAttr, SceFiosFH fh, void *pBuf,
			       SceFiosSize length, SceFiosOffset offset)
{
	SceFiosSize result;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s (buf=0x%p,length=%lld)", "sceFiosFHPreadSync",
		   pBuf, length);
	if (length < 0)
		return SCE_FIOS_ERROR_BAD_SIZE;
	if (isDirectFH(fh)) {
		if (pBuf == NULL)
			return SCE_FIOS_ERROR_BAD_PTR;
		result = fiosPlatformPread(passthruFd(fh), pBuf, (SceSize)length, offset);
		if (result < 0)
			result = fiosPlatformTranslateError(result);
	} else {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		result = sceFiosOpSyncWaitForIO(sceFiosFHPread(&attr, fh, pBuf, length, offset));
	}
	return result;
}

SceFiosSize sceFiosFHPwriteSync(const SceFiosOpAttr *pAttr, SceFiosFH fh, const void *pBuf,
				SceFiosSize length, SceFiosOffset offset)
{
	SceFiosSize result;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s (buf=0x%p,length=%lld)", "sceFiosFHPwriteSync",
		   pBuf, length);
	if (length < 0)
		return SCE_FIOS_ERROR_BAD_SIZE;
	if (isDirectFH(fh)) {
		if (pBuf == NULL)
			return SCE_FIOS_ERROR_BAD_PTR;
		result = fiosPlatformPwrite(passthruFd(fh), pBuf, (SceSize)length, offset);
		if (result < 0)
			result = fiosPlatformTranslateError(result);
	} else {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		result = sceFiosOpSyncWaitForIO(sceFiosFHPwrite(&attr, fh, pBuf, length, offset));
	}
	return result;
}

SceFiosSize sceFiosFHPreadvSync(const SceFiosOpAttr *pAttr, SceFiosFH fh,
				const SceFiosBuffer iov[], int iovcnt, SceFiosOffset offset)
{
	SceFiosOpAttr attr;
	SceFiosSize total, n = 0;
	SceFiosNativeFH fd;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHPreadvSync");
	if (iov == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (!isDirectFH(fh)) {
		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWaitForIO(sceFiosFHPreadv(&attr, fh, iov, iovcnt, offset));
	}
	total = 0;
	fd = passthruFd(fh);
	for (; iovcnt > 0; iov++) {
		if (iov->pPtr == NULL)
			break;
		n = fiosPlatformPread(fd, iov->pPtr, iov->length, offset);
		if (n < 0)
			break;
		iovcnt--;
		offset += n;
		total += n;
	}
	if (n < 0)
		total = fiosPlatformTranslateError(n);
	return total;
}

SceFiosSize sceFiosFHPwritevSync(const SceFiosOpAttr *pAttr, SceFiosFH fh,
				 const SceFiosBuffer iov[], int iovcnt, SceFiosOffset offset)
{
	SceFiosOpAttr attr;
	SceFiosSize total, n = 0;
	SceFiosNativeFH fd;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHPwritevSync");
	if (iov == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (!isDirectFH(fh)) {
		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWaitForIO(sceFiosFHPwritev(&attr, fh, iov, iovcnt, offset));
	}
	total = 0;
	fd = passthruFd(fh);
	for (; iovcnt > 0; iov++) {
		if (iov->pPtr == NULL)
			break;
		n = fiosPlatformPwrite(fd, iov->pPtr, iov->length, offset);
		if (n < 0)
			break;
		iovcnt--;
		offset += n;
		total += n;
	}
	if (n < 0)
		total = fiosPlatformTranslateError(n);
	return total;
}

SceFiosOffset sceFiosFHSeek(SceFiosFH fh, SceFiosOffset offset, SceFiosWhence whence)
{
	SceFiosOffset result;
	FiosFH *pFH;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHSeek");
	if (isDirectFH(fh)) {
		result = fiosPlatformLseek(passthruFd(fh), offset, whence);
		if (result < 0)
			result = fiosPlatformTranslateError(result);
		return result;
	}
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	if (pFH == NULL)
		return -1;
	result = pFH->offset;
	switch (whence) {
	case SCE_FIOS_SEEK_SET:
		result = offset;
		break;
	case SCE_FIOS_SEEK_CUR:
		result += offset;
		break;
	case SCE_FIOS_SEEK_END:
		result = pFH->size + offset;
		break;
	}
	sceAtomicExchange64Relaxed((volatile int64_t *)&pFH->offset, result);
	return result;
}

int sceFiosFHCloseSync(const SceFiosOpAttr *pAttr, SceFiosFH fh)
{
	SceFiosOpAttr attr;
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHCloseSync");
	if (isDirectFH(fh)) {
		err = fiosPlatformClose(passthruFd(fh));
		if (err != 0)
			err = fiosPlatformTranslateError(err);
		sceAtomicExchange32Relaxed(&s_passthruFds[fh & 0x3fff], -1);
		return err;
	}
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosFHClose(&attr, fh));
}

int sceFiosDHOpenSync(const SceFiosOpAttr *pAttr, SceFiosDH *pOutDH, const char *pPath,
		      SceFiosBuffer buf)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDHOpenSync");
	if (!g_FiosParams.initialized)
		return SCE_FIOS_ERROR_UNIMPLEMENTED;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosDHOpen(&attr, pOutDH, pPath, buf));
}

int sceFiosDHReadSync(const SceFiosOpAttr *pAttr, SceFiosDH dh, SceFiosDirEntry *pOutEntry)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDHReadSync");
	if (!g_FiosParams.initialized)
		return SCE_FIOS_ERROR_UNIMPLEMENTED;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosDHRead(&attr, dh, pOutEntry));
}

int sceFiosDHCloseSync(const SceFiosOpAttr *pAttr, SceFiosDH dh)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDHCloseSync");
	if (!g_FiosParams.initialized)
		return SCE_FIOS_ERROR_UNIMPLEMENTED;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosDHClose(&attr, dh));
}

int sceFiosCachePrefetchFileSync(const SceFiosOpAttr *pAttr, const char *pPath)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCachePrefetchFileSync");
	if (!g_FiosParams.initialized)
		return SCE_FIOS_ERROR_UNIMPLEMENTED;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosCachePrefetchFile(&attr, pPath));
}

int sceFiosCachePrefetchFileRangeSync(const SceFiosOpAttr *pAttr, const char *pPath,
				      SceFiosOffset offset, SceFiosSize length)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCachePrefetchFileRangeSync");
	if (!g_FiosParams.initialized)
		return 0;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosCachePrefetchFileRange(&attr, pPath, offset, length));
}

int sceFiosCachePrefetchFHSync(const SceFiosOpAttr *pAttr, SceFiosFH fh)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCachePrefetchFHSync");
	if (isDirectFH(fh))
		return 0;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosCachePrefetchFH(&attr, fh));
}

int sceFiosCachePrefetchFHRangeSync(const SceFiosOpAttr *pAttr, SceFiosFH fh,
				    SceFiosOffset offset, SceFiosSize length)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCachePrefetchFHRangeSync");
	if (isDirectFH(fh))
		return 0;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosCachePrefetchFHRange(&attr, fh, offset, length));
}

SceFiosSize sceFiosArchiveGetMountBufferSizeSync(const SceFiosOpAttr *pAttr,
						 const char *pArchivePath,
						 const SceFiosOpenParams *pOpenParams)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosArchiveGetMountBufferSizeSync");
	if (!g_FiosParams.initialized)
		return SCE_FIOS_ERROR_UNIMPLEMENTED;
	getArchiveOpAttr(&attr, pAttr);
	return sceFiosOpSyncWaitForIO(sceFiosArchiveGetMountBufferSize(&attr, pArchivePath,
								       pOpenParams));
}

int sceFiosArchiveMountSync(const SceFiosOpAttr *pAttr, SceFiosFH *pOutFH,
			    const char *pArchivePath, const char *pMountPoint,
			    SceFiosBuffer mountBuffer, const SceFiosOpenParams *pOpenParams)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosArchiveMountSync");
	if (!g_FiosParams.initialized)
		return SCE_FIOS_ERROR_UNIMPLEMENTED;
	getArchiveOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosArchiveMount(&attr, pOutFH, pArchivePath, pMountPoint,
						     mountBuffer, pOpenParams));
}

int sceFiosArchiveUnmountSync(const SceFiosOpAttr *pAttr, SceFiosFH fh)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosArchiveUnmountSync");
	if (!g_FiosParams.initialized)
		return SCE_FIOS_ERROR_UNIMPLEMENTED;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosArchiveUnmount(&attr, fh));
}

int sceFiosChangeStatSync(const SceFiosOpAttr *pAttr, const char *pPath, SceFiosStat *pStatus,
			  int cbit)
{
	SceFiosOpAttr attr;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosChangeStatSync");
	if (!g_FiosParams.initialized)
		return SCE_FIOS_ERROR_UNIMPLEMENTED;
	getSyncOpAttr(&attr, pAttr);
	return sceFiosOpSyncWait(sceFiosChangeStat(&attr, pPath, pStatus, cbit));
}

int sceFiosDevctlSync(const SceFiosOpAttr *pAttr, const char *pPath, int cmd, void *pArg,
		      size_t arglen, void *pBuffer, size_t buflen)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDevctlSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosDevctl(&attr, pPath, cmd, pArg, arglen, pBuffer,
						       buflen));
	}
	err = sceIoDevctl(pPath, cmd, pArg, arglen, pBuffer, buflen);
	if (err < 0)
		err = fiosPlatformTranslateError(err);
	return err;
}

int sceFiosFHIoctlSync(const SceFiosOpAttr *pAttr, const SceFiosFH fh, int cmd, void *pArg,
		       size_t arglen, void *pBuffer, size_t buflen)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHIoctlSync");
	if (fiosIsPassthruFH(fh)) {
		SceFiosNativeFH fd = passthruFd(fh);

		FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (fd=%d,cmd=%d)", "sceIoIoctl*", fd,
			   cmd);
		err = sceIoIoctl(fd, cmd, pArg, arglen, pBuffer, buflen);
		if (err < 0)
			err = fiosPlatformTranslateError(err);
	} else {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		err = sceFiosOpSyncWait(sceFiosFHIoctl(&attr, fh, cmd, pArg, arglen, pBuffer,
						       buflen));
	}
	return err;
}

int sceFiosSyncSync(const SceFiosOpAttr *pAttr, const char *pPath, int flag)
{
	int err;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosSyncSync");
	if (g_FiosParams.initialized) {
		SceFiosOpAttr attr;

		getSyncOpAttr(&attr, pAttr);
		return sceFiosOpSyncWait(sceFiosSync(&attr, pPath, flag));
	}
	err = sceIoSync(pPath, flag);
	if (err < 0)
		err = fiosPlatformTranslateError(err);
	return err;
}

SceFiosOffset sceFiosFHTell(SceFiosFH fh)
{
	FiosFH *pFH;
	SceFiosOffset offset;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHTell");
	if (fiosIsPassthruFH(fh)) {
		offset = fiosPlatformLseek(passthruFd(fh), 0, SCE_FIOS_SEEK_CUR);
		if (offset < 0)
			offset = fiosPlatformTranslateError(offset);
		return offset;
	}
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	return pFH != NULL ? pFH->offset : -1;
}

SceFiosNativeFH sceFiosFHToFileno(SceFiosFH fh)
{
	FiosFH *pFH;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHToFileno");
	if (fiosIsPassthruFH(fh))
		return passthruFd(fh);
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	return pFH != NULL ? pFH->nativeFH : SCE_FIOS_NATIVEFH_INVALID;
}

SceFiosFH sceFiosFilenoToFH(SceFiosNativeFH fd)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFilenoToFH");
	if (fd == SCE_FIOS_NATIVEFH_INVALID)
		return 0;
	return allocatePassthruFH(fd);
}

int sceFiosDeallocatePassthruFH(SceFiosFH fh)
{
	if (fiosIsPassthruFH(fh))
		sceAtomicExchange32Relaxed(&s_passthruFds[fh & 0x3fff], -1);
	return 0;
}
