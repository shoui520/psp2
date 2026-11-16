/*
 * platform.cpp: the platform layer under the scheduler: SceIofilemgr's calls with FIOS2's
 * flags, modes, stat and errors, and the overlay calls of SceFios2User (driver_us).
 */
#include "fios2_internal.h"

/* 0x800F090D: an error of the I/O manager the SDK headers do not name */
#define FIOS_IO_ERROR_NOT_PERMITTED	-2146498291

/* the permission bits SCE_STM_RWU and SCE_STM_RU leave unnamed */
#define FIOS_STM_WU	00200	/* the owner may write */
#define FIOS_STM_RO	00004	/* others may read */
#define FIOS_STM_WO	00002	/* others may write */

/* SCE_ERROR_ERRNO_* of the I/O manager as SCE_FIOS_ERROR_*; other errors pass through */
int fiosPlatformTranslateError(int err)
{
	if (!SCE_ERROR_ISFAILED(err))
		return err;
	switch (err) {
	case SCE_ERROR_ERRNO_EPERM:
	case SCE_ERROR_ERRNO_EACCES:
	case FIOS_IO_ERROR_NOT_PERMITTED:
		return SCE_FIOS_ERROR_ACCESS;
	case SCE_ERROR_ERRNO_ENOENT:
	case SCE_ERROR_ERRNO_EINVAL:
	case SCE_ERROR_ERRNO_ENOTSUP:
		return SCE_FIOS_ERROR_BAD_PATH;
	case SCE_ERROR_ERRNO_EIO:
	case SCE_ERROR_ERRNO_ENXIO:
	case SCE_ERROR_ERRNO_ENODEV:
	case SCE_ERROR_ERRNO_ENOSYS:
		return SCE_FIOS_ERROR_MEDIA_GONE;
	case SCE_ERROR_ERRNO_EBADF:
	case SCE_ERROR_ERRNO_EBADFD:
		return SCE_FIOS_ERROR_BAD_FH;
	case SCE_ERROR_ERRNO_EEXIST:
		return SCE_FIOS_ERROR_ALREADY_EXISTS;
	case SCE_ERROR_ERRNO_ENOTDIR:
		return SCE_FIOS_ERROR_NOT_A_DIRECTORY;
	case SCE_ERROR_ERRNO_EISDIR:
		return SCE_FIOS_ERROR_NOT_A_FILE;
	case SCE_ERROR_ERRNO_EROFS:
		return SCE_FIOS_ERROR_READ_ONLY;
	case SCE_ERROR_ERRNO_ERANGE:
		return SCE_FIOS_ERROR_EOF;
	case SCE_ERROR_ERRNO_ENAMETOOLONG:
		return SCE_FIOS_ERROR_PATH_TOO_LONG;
	default:
		return err;
	}
}

/* SCE_FIOS_O_* as sceIoOpen's flags */
int fiosPlatformOpenFlags(uint32_t flags)
{
	int access;

	switch (flags & SCE_FIOS_O_RDWR) {
	case SCE_FIOS_O_WRITE:
		access = SCE_O_WRONLY;
		break;
	case SCE_FIOS_O_RDWR:
		access = SCE_O_RDWR;
		break;
	default:
		access = SCE_O_RDONLY;
		break;
	}
	return access | ((flags & SCE_FIOS_O_APPEND) ? SCE_O_APPEND : 0) |
	       ((flags & SCE_FIOS_O_CREAT) ? SCE_O_CREAT : 0) |
	       ((flags & SCE_FIOS_O_TRUNC) ? SCE_O_TRUNC : 0);
}

/*
 * The mode a file is created with: -1 is the default (0600 for a game, else 0006), and
 * outside a game the owner's read and write bits are given to others too.
 */
SceIoMode fiosPlatformMode(SceIoMode mode)
{
	bool game = sceAppMgrIsGameProgram() != 0;

	if (mode == (SceIoMode)-1) {
		mode = FIOS_STM_RO | FIOS_STM_WO;
		if (game)
			mode = SCE_STM_RWU;
	} else if (!game) {
		if (mode & SCE_STM_RU)
			mode |= FIOS_STM_RO;
		if (mode & FIOS_STM_WU)
			mode |= FIOS_STM_WO;
	}
	return mode;
}

int fiosPlatformOpen(SceFiosNativeFH *pFd, const char *pPath, uint32_t flags, SceIoMode mode)
{
	int ioFlags;
	SceUID fd;

	mode = fiosPlatformMode(mode);
	ioFlags = fiosPlatformOpenFlags(flags);
	*pFd = SCE_FIOS_NATIVEFH_INVALID;
	FIOS_TRACE(SCE_FIOS_PROFILE_IO, "io", "%-15s (\"%s\",flags=0x%04X,mode=0%o)",
		   "sceIoOpen", pPath, ioFlags, mode);
	fd = sceIoOpen(pPath, ioFlags, mode);
	if (fd < 0)
		return fd;
	*pFd = fd;
	return SCE_FIOS_OK;
}

SceOff fiosPlatformLseek(SceFiosNativeFH fd, SceOff offset, int whence)
{
	return sceIoLseek(fd, offset, whence);
}

int64_t fiosPlatformRead(SceFiosNativeFH fd, void *pBuf, SceSize size)
{
	return sceIoRead(fd, pBuf, size);
}

int64_t fiosPlatformWrite(SceFiosNativeFH fd, const void *pBuf, SceSize size)
{
	return sceIoWrite(fd, pBuf, size);
}

int64_t fiosPlatformPread(SceFiosNativeFH fd, void *pBuf, SceSize size, SceOff offset)
{
	return sceIoPread(fd, pBuf, size, offset);
}

int64_t fiosPlatformPwrite(SceFiosNativeFH fd, const void *pBuf, SceSize size, SceOff offset)
{
	return sceIoPwrite(fd, pBuf, size, offset);
}

int fiosPlatformClose(SceFiosNativeFH fd)
{
	return sceIoClose(fd);
}

int fiosPlatformGetstatByFd(SceFiosNativeFH fd, SceIoStat *pStat)
{
	return sceIoGetstatByFd(fd, pStat);
}

int fiosPlatformSyncByFd(SceFiosNativeFH fd)
{
	return sceIoSyncByFd(fd, 0);
}

int fiosPlatformFlushByFd(SceFiosNativeFH fd)
{
	return sceIoSyncByFd(fd, 0);
}

/* the device of a path ("ux0:" of "ux0:data/x"): up to 16 letters or digits and a colon */
int fiosPlatformGetDevice(char *pDevice, const char *pPath, size_t size)
{
	int ret = SCE_FIOS_OK;
	char *pEnd = pDevice + size;
	int length = 0;
	const char *p = pPath;

	for (;;) {
		int c = *p;

		if (c == '/' || c == '\0')
			return SCE_FIOS_ERROR_BAD_PATH;
		if (length > 0 && c == ':')
			break;
		if (((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || (c >= '0' && c <= '9')) {
			length++;
			p++;
			if (length <= 16)
				continue;
		}
		return SCE_FIOS_ERROR_BAD_PATH;
	}
	for (; length >= 0; length--) {
		*pDevice++ = *pPath++;
		if (pDevice >= pEnd)
			return SCE_FIOS_ERROR_BAD_PATH;
	}
	*pDevice = '\0';
	if (pDevice + 1 > pEnd)
		return SCE_FIOS_ERROR_BAD_PATH;
	return ret;
}

int fiosPlatformSync(const char *pPath, int flag)
{
	char device[18];
	int ret;

	ret = fiosPlatformGetDevice(device, pPath, sizeof(device));
	if (ret == SCE_FIOS_OK)
		ret = sceIoSync(device, flag);
	return ret;
}

int fiosPlatformRemove(const char *pPath)
{
	return sceIoRemove(pPath);
}

int fiosPlatformRename(const char *pOldPath, const char *pNewPath)
{
	return sceIoRename(pOldPath, pNewPath);
}

/* the native handle is not used: the path's file is truncated */
int fiosPlatformTruncate(const char *pPath, SceFiosNativeFH fd, SceOff size)
{
	SceIoStat stat;

	sceClibMemset(&stat, 0, sizeof(stat));
	stat.st_size = size;
	return sceIoChstat(pPath, &stat, SCE_CST_SIZE);
}

int fiosPlatformMkdir(const char *pPath, SceIoMode mode)
{
	return sceIoMkdir(pPath, fiosPlatformMode(mode));
}

int fiosPlatformRmdir(const char *pPath)
{
	return sceIoRmdir(pPath);
}

int fiosPlatformDevctl(const char *pDevice, int cmd, const void *pArg, SceSize argSize,
		       void *pBuf, SceSize bufSize)
{
	return sceIoDevctl(pDevice, cmd, pArg, argSize, pBuf, bufSize);
}

int fiosPlatformIoctl(SceFiosNativeFH fd, int cmd, const void *pArg, SceSize argSize,
		      void *pBuf, SceSize bufSize)
{
	return sceIoIoctl(fd, cmd, pArg, argSize, pBuf, bufSize);
}

/* sceFiosChangeStat: the SceFiosStat fields cbit selects */
int fiosPlatformChstat(const char *pPath, const SceFiosStat *pStat, unsigned int cbit)
{
	SceIoStat stat;

	sceClibMemset(&stat, 0, sizeof(stat));
	stat.st_size = pStat->fileSize;
	sceFiosDateToSceDateTime(pStat->accessDate, &stat.st_atime);
	sceFiosDateToSceDateTime(pStat->creationDate, &stat.st_ctime);
	sceFiosDateToSceDateTime(pStat->modificationDate, &stat.st_mtime);
	if (pStat->statFlags & (SCE_FIOS_STATUS_READABLE | SCE_FIOS_STATUS_WRITABLE)) {
		SceIoMode readable = SCE_STM_RU, writable = FIOS_STM_WU;

		if (sceAppMgrIsGameProgram() == 0) {
			readable = SCE_STM_RU | FIOS_STM_RO;
			writable = FIOS_STM_WU | FIOS_STM_WO;
		}
		if (pStat->statFlags & SCE_FIOS_STATUS_WRITABLE)
			stat.st_mode |= writable;
		if (pStat->statFlags & SCE_FIOS_STATUS_READABLE)
			stat.st_mode |= readable;
	} else {
		stat.st_mode = fiosPlatformMode((SceIoMode)pStat->mode);
	}
	return sceIoChstat(pPath, &stat, cbit);
}

int fiosPlatformGetstat(const char *pPath, SceIoStat *pStat)
{
	return sceIoGetstat(pPath, pStat);
}

int fiosPlatformInit()
{
	return SCE_FIOS_OK;
}

int fiosPlatformTerm()
{
	return SCE_FIOS_OK;
}

void fiosPlatformIdle()
{
}

int fiosPlatformDopen(SceFiosNativeDH *pFd, const char *pPath)
{
	SceUID fd = sceIoDopen(pPath);

	if (fd < 0)
		return fd;
	*pFd = fd;
	return SCE_FIOS_OK;
}

/* the next entry other than "." and "..": 1, or 0 at the end, or an error */
int fiosPlatformDread(SceFiosNativeDH fd, SceIoDirent *pDirent)
{
	int ret;

	sceClibMemset(pDirent, 0, sizeof(*pDirent));
	for (;;) {
		ret = sceIoDread(fd, pDirent);
		if (ret <= 0)
			return ret;
		if (pDirent->d_name[0] == '.' && pDirent->d_name[1] == '\0')
			continue;
		if (pDirent->d_name[0] == '.' && pDirent->d_name[1] == '.' && pDirent->d_name[2] == '\0')
			continue;
		return 1;
	}
}

int fiosPlatformDclose(SceFiosNativeDH fd)
{
	return sceIoDclose(fd);
}

/* nanoseconds since the Unix epoch, 0 when the time cannot be converted */
SceFiosDate fiosPlatformDateTimeToDate(const SceDateTime *pTime)
{
	SceFiosDate date = 0;
	SceUInt64 seconds = 0;

	if (sceRtcGetTime64_t(pTime, &seconds) == 0)
		date = seconds * SCE_FIOS_NANOSECONDS_PER_SECOND;
	return date;
}

int fiosPlatformOverlayAdd(SceUID pid, const SceFiosOverlay *pOverlay, SceFiosOverlayID *pOutId)
{
	return sceFiosOverlayAddForProcess02(pid, pOverlay, pOutId);
}

int fiosPlatformOverlayRemove(SceUID pid, SceFiosOverlayID id)
{
	return sceFiosOverlayRemoveForProcess02(pid, id);
}

int fiosPlatformOverlayGetInfo(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *pOutOverlay)
{
	return sceFiosOverlayGetInfoForProcess02(pid, id, pOutOverlay);
}

int fiosPlatformOverlayModify(SceUID pid, SceFiosOverlayID id, const SceFiosOverlay *pOverlay)
{
	return sceFiosOverlayModifyForProcess02(pid, id, pOverlay);
}

int fiosPlatformOverlayGetList(SceUID pid, uint8_t minOrder, uint8_t maxOrder,
			       SceFiosOverlayID *pOutIds, SceSize maxIds, SceSize *pActualIds)
{
	return sceFiosOverlayGetList02(pid, minOrder, maxOrder, pOutIds, maxIds, pActualIds);
}

int fiosPlatformOverlayResolve(SceUID pid, int resolveFlag, const char *pInPath, char *pOutPath,
			       SceSize maxPath, uint8_t minOrder, uint8_t maxOrder)
{
	return sceFiosOverlayResolveWithRangeSync02(pid, resolveFlag, pInPath, pOutPath, maxPath,
						    minOrder, maxOrder);
}

/* the allocator's last allocated unit: called by Allocator::Init and Term */
uint32_t Allocator::GetLastIndex() const
{
	return m_lastIndex;
}
