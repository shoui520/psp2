/* Sony's VSH platform layer is being converted around the upstream core. */

#include <psp2/types.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/io/devctl.h>
#include <psp2/appmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include "sqlite3.h"
#include "vfs_time.h"

extern void *sce_paf_memset(void *buffer, int value, unsigned int size);
extern void *sce_paf_memcpy(void *dest, const void *source, unsigned int size);
extern char *sceClibStrchr(const char *text, int character);
extern char *sceClibStrrchr(const char *text, int character);
extern void *ScePafStdc_75ECC54E(void *buffer, int value, unsigned int size);
extern unsigned int sce_paf_strlen(const char *text);
extern int sce_paf_strcmp(const char *left, const char *right);
extern int sceKernelOpenEventFlag(const char *name);
extern int sceKernelCloseEventFlag(SceUID uid);
extern int sceIoFlockForSystem(SceUID fd, unsigned int flags);

typedef struct psp2Shared {
	SceUID fd;
	char *zPath;
	signed char nShared;
	signed char eLock;
	signed char nRef;
	signed char unkB;
	int openFlags;
	sqlite3_mutex *mutex;
	struct psp2Shared *pNext;
} psp2Shared;

typedef struct psp2File {
	const sqlite3_io_methods *pMethod;
	psp2Shared *pShared;
	int locktype;
} psp2File;

typedef struct psp2MountState {
	char path[16];
	SceUID mutex;
	int createdMutex;
	SceUID event;
	int createdEvent;
} psp2MountState;

static psp2MountState g_mount = {
	.mutex = -1,
	.event = -1,
};

static const unsigned int g_lockFlags[] = { 272, 257, 260, 0, 258 };
static psp2Shared *g_sharedList;
static const sqlite3_io_methods g_ioMethods;
static sqlite3_vfs g_vfs;

typedef struct psp2IoCacheConfig {
	unsigned int unk00;
	unsigned int unk04;
	unsigned int unk08;
	unsigned int pageSize;
	unsigned int unk10;
	unsigned int pageCount;
	unsigned int unk18;
	unsigned int unk1c;
} psp2IoCacheConfig;

int psp2OpenShared(psp2File *file, const char *path, int flags,
		   int deleteOnClose, int powerLock)
{
	sqlite3_mutex *master = sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER);
	psp2Shared *shared;
	psp2IoCacheConfig cache;
	char directory[256];
	const char *separator;
	char *copy;
	sqlite3_mutex *mutex;
	unsigned int length;
	int result = SQLITE_OK;

	sqlite3_mutex_enter(master);
	for (shared = g_sharedList; shared; shared = shared->pNext) {
		if (sce_paf_strcmp(path, shared->zPath) == 0)
			break;
	}
	if (shared) {
		sqlite3_mutex_enter(shared->mutex);
		if (flags & SCE_O_EXCL) {
			result = SQLITE_CANTOPEN;
		} else {
			if (!(shared->openFlags & SCE_O_WRONLY) && (flags & SCE_O_WRONLY)) {
				result = sceIoClose(shared->fd);
				if (result == 0) {
					shared->fd = 0;
					result = sceIoOpen(shared->zPath, shared->openFlags | 3, 6);
					if (result >= 0) {
						shared->fd = result;
						shared->openFlags |= 3;
						result = SQLITE_OK;
					}
				}
			}
			if (result == SQLITE_OK) {
				shared->unkB |= powerLock;
				shared->nRef++;
				file->pShared = shared;
			}
		}
		sqlite3_mutex_leave(shared->mutex);
		if (result != SQLITE_OK)
			goto out;
	} else {
		length = sce_paf_strlen(path);
		copy = sqlite3_malloc(length + 1);
		shared = sqlite3_malloc(sizeof(*shared));
		mutex = sqlite3_mutex_alloc(SQLITE_MUTEX_FAST);
		if (!copy || !shared || !mutex) {
			sqlite3_free(copy);
			sqlite3_free(shared);
			sqlite3_mutex_free(mutex);
			result = SQLITE_NOMEM;
			goto out;
		}
		sce_paf_memset(shared, 0, sizeof(*shared));
		sce_paf_memcpy(copy, path, length + 1);
		shared->zPath = copy;
		shared->openFlags = flags;
		shared->mutex = mutex;
		shared->nRef = 1;
		shared->unkB = powerLock;
		file->pShared = shared;
		result = sceIoOpen(shared->zPath, shared->openFlags, 6);
		if (result < 0) {
			sqlite3_free(shared->zPath);
			sqlite3_mutex_free(shared->mutex);
			sqlite3_free(shared);
			file->pShared = NULL;
			goto out;
		}
		shared->fd = result;
		shared->pNext = g_sharedList;
		g_sharedList = shared;
	}
	sce_paf_memset(&cache, 0, sizeof(cache));
	cache.pageSize = 4096;
	cache.pageCount = 8;
	sceIoIoctl(file->pShared->fd, 0x1001, &cache, sizeof(cache), NULL, 0);
	if (deleteOnClose)
		sceIoRemove(path);
	result = SQLITE_OK;
	if (!deleteOnClose && (flags & SCE_O_CREAT)) {
		separator = sceClibStrrchr(path, '/');
		if (!separator)
			separator = sceClibStrchr(path, ':');
		if (separator) {
			length = separator - path;
			if (length < sizeof(directory)) {
				sce_paf_memcpy(directory, path, length);
				directory[length] = '\0';
				sceIoSync(directory, 0);
			}
		}
	}
out:
	sqlite3_mutex_leave(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER));
	return result;
}

static inline int psp2FlockResult(int error)
{
	if (error == 0)
		return SQLITE_OK;
	if (error == (int)0x8002a005 || error == (int)0x80010010)
		return SQLITE_BUSY;
	return SQLITE_IOERR_LOCK;
}

int psp2Unlock(sqlite3_file *id, int locktype)
{
	psp2File *file = (psp2File *)id;
	psp2Shared *shared;
	int result = SQLITE_OK;
	int error;

	sqlite3_mutex_enter(file->pShared->mutex);
	shared = file->pShared;
	if (file->locktype != locktype && shared->nShared) {
		if (locktype == SQLITE_LOCK_NONE) {
			if (shared->nShared < 2) {
				if (g_lockFlags[locktype]) {
					error = sceIoFlockForSystem(shared->fd,
							     g_lockFlags[locktype]);
					result = psp2FlockResult(error);
					if (error == (int)0x80010013)
						result = SQLITE_OK;
					shared = file->pShared;
				}
				if (result == SQLITE_OK) {
					if (shared->nShared < 2) {
						shared->nShared = 0;
						shared->eLock = SQLITE_LOCK_NONE;
					} else {
						shared->nShared--;
					}
					file->locktype = SQLITE_LOCK_NONE;
					if (shared->unkB) {
						sceKernelPowerUnlock(0);
						shared = file->pShared;
					}
				}
			} else {
				shared->nShared--;
				file->locktype = SQLITE_LOCK_NONE;
			}
		} else if (locktype == SQLITE_LOCK_SHARED) {
			if (g_lockFlags[locktype]) {
				error = sceIoFlockForSystem(shared->fd,
						     g_lockFlags[locktype]);
				result = psp2FlockResult(error);
				if (error == (int)0x80010013)
					result = SQLITE_OK;
				shared = file->pShared;
			}
			if (result == SQLITE_OK) {
				if (file->locktype == SQLITE_LOCK_EXCLUSIVE)
					shared->nShared = 1;
				shared->eLock = SQLITE_LOCK_SHARED;
				file->locktype = SQLITE_LOCK_SHARED;
			}
		}
	}
	sqlite3_mutex_leave(shared->mutex);
	return result;
}

int psp2Close(sqlite3_file *id)
{
	psp2File *file = (psp2File *)id;
	psp2Shared *shared = file->pShared;
	psp2Shared *entry, *previous;
	sqlite3_mutex *master;
	int last = 0;
	int result;

	if (!shared)
		return SQLITE_IOERR;
	psp2Unlock(id, SQLITE_LOCK_NONE);
	sqlite3_mutex_enter(shared->mutex);
	shared->nRef--;
	if (shared->nRef < 1) {
		master = sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER);
		sqlite3_mutex_enter(master);
		previous = NULL;
		for (entry = g_sharedList; entry; entry = entry->pNext) {
			if (entry == shared) {
				if (previous)
					previous->pNext = entry->pNext;
				else
					g_sharedList = entry->pNext;
				break;
			}
			previous = entry;
		}
		sqlite3_mutex_leave(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER));
		last = 1;
	}
	sqlite3_mutex_leave(shared->mutex);
	if (last) {
		result = sceIoClose(shared->fd);
		if (result < 0)
			result = SQLITE_IOERR;
		sqlite3_free(shared->zPath);
		sqlite3_mutex_free(shared->mutex);
		sqlite3_free(shared);
		if (result != SQLITE_OK)
			return result;
	}
	file->pShared = NULL;
	return SQLITE_OK;
}

int psp2CheckReservedLock(sqlite3_file *id, int *reserved)
{
	psp2File *file = (psp2File *)id;
	psp2Shared *shared;
	int result = SQLITE_OK;
	int held = 0;
	signed char previous;

	sqlite3_mutex_enter(file->pShared->mutex);
	shared = file->pShared;
	if (shared->nShared < 0 || shared->eLock > SQLITE_LOCK_SHARED) {
		held = 1;
	} else {
		if (shared->unkB) {
			sceKernelPowerLock(0);
			shared = file->pShared;
		}
		if (g_lockFlags[SQLITE_LOCK_RESERVED]) {
			result = psp2FlockResult(sceIoFlockForSystem(shared->fd,
						 g_lockFlags[SQLITE_LOCK_RESERVED]));
			shared = file->pShared;
		}
		if (result == SQLITE_OK) {
			previous = shared->eLock;
			shared->eLock = SQLITE_LOCK_RESERVED;
			if ((unsigned int)previous < 5 && g_lockFlags[previous]) {
				sceIoFlockForSystem(shared->fd, g_lockFlags[previous]);
				shared = file->pShared;
			}
			shared->eLock = previous;
		} else if (result == SQLITE_BUSY) {
			held = 1;
		}
		if (shared->unkB) {
			sceKernelPowerUnlock(0);
			shared = file->pShared;
		}
	}
	sqlite3_mutex_leave(shared->mutex);
	*reserved = held;
	return result;
}

int psp2Lock(sqlite3_file *id, int locktype)
{
	psp2File *file = (psp2File *)id;
	psp2Shared *shared;
	int result = SQLITE_OK;

	sqlite3_mutex_enter(file->pShared->mutex);
	shared = file->pShared;
	if (file->locktype >= locktype)
		goto out;
	if (locktype > SQLITE_LOCK_NONE && file->locktype < SQLITE_LOCK_SHARED) {
		if (shared->eLock > SQLITE_LOCK_SHARED || shared->nShared < 0) {
			result = SQLITE_BUSY;
			goto out;
		}
		if (shared->nShared == 0) {
			if (shared->unkB)
				result = sceKernelPowerLock(0);
			if (result == SQLITE_OK && g_lockFlags[SQLITE_LOCK_SHARED]) {
				result = psp2FlockResult(sceIoFlockForSystem(shared->fd,
							g_lockFlags[SQLITE_LOCK_SHARED]));
				if (result != SQLITE_OK && shared->unkB)
					sceKernelPowerUnlock(0);
			}
			shared = file->pShared;
			if (result != SQLITE_OK)
				goto out;
		}
		shared->nShared++;
		file->locktype = SQLITE_LOCK_SHARED;
		if (shared->eLock < SQLITE_LOCK_SHARED)
			shared->eLock = SQLITE_LOCK_SHARED;
	}
	if (locktype > SQLITE_LOCK_SHARED && file->locktype < SQLITE_LOCK_RESERVED) {
		if (shared->eLock > SQLITE_LOCK_SHARED) {
			result = SQLITE_BUSY;
			goto out;
		}
		if (g_lockFlags[SQLITE_LOCK_RESERVED]) {
			result = psp2FlockResult(sceIoFlockForSystem(shared->fd,
						 g_lockFlags[SQLITE_LOCK_RESERVED]));
			shared = file->pShared;
		}
		if (result == SQLITE_OK) {
			file->locktype = SQLITE_LOCK_RESERVED;
			if (shared->eLock < SQLITE_LOCK_RESERVED)
				shared->eLock = SQLITE_LOCK_RESERVED;
		}
	}
	if (result == SQLITE_OK && locktype == SQLITE_LOCK_EXCLUSIVE) {
		if (shared->nShared == 0 ||
		    (shared->nShared == 1 && file->locktype > SQLITE_LOCK_SHARED &&
		     file->locktype < SQLITE_LOCK_EXCLUSIVE)) {
			if (g_lockFlags[SQLITE_LOCK_EXCLUSIVE]) {
				result = psp2FlockResult(sceIoFlockForSystem(shared->fd,
							g_lockFlags[SQLITE_LOCK_EXCLUSIVE]));
				shared = file->pShared;
			}
			if (result == SQLITE_OK) {
				shared->nShared = -1;
				file->locktype = SQLITE_LOCK_EXCLUSIVE;
				if (shared->eLock < SQLITE_LOCK_EXCLUSIVE)
					shared->eLock = SQLITE_LOCK_EXCLUSIVE;
			} else {
				file->locktype = SQLITE_LOCK_PENDING;
				if (shared->eLock < SQLITE_LOCK_PENDING)
					shared->eLock = SQLITE_LOCK_PENDING;
			}
		} else {
			result = SQLITE_BUSY;
			if (shared->eLock < SQLITE_LOCK_PENDING) {
				file->locktype = SQLITE_LOCK_PENDING;
				shared->eLock = SQLITE_LOCK_PENDING;
			}
		}
	}
out:
	sqlite3_mutex_leave(shared->mutex);
	return result;
}

int psp2Mount(void)
{
	char name[32];
	SceUID process;
	int result;

	if (g_mount.path[0])
		return SQLITE_OK;
	process = sceKernelGetProcessId();
	sqlite3_snprintf(sizeof(name), name, "SceSqliteMtx%08x", process);
	result = sceKernelCreateMutex(name, 0x80, 1, NULL);
	if (result < 0) {
		if (result == (int)0x80024e02) {
			g_mount.mutex = sceKernelOpenMutex(name);
			sceKernelLockMutex(g_mount.mutex, 1, NULL);
		}
	} else {
		g_mount.createdMutex = 1;
		g_mount.mutex = result;
	}
	sqlite3_snprintf(sizeof(name), name, "SceSqliteEvf%08x", process);
	result = sceKernelCreateEventFlag(name, 0x80, 1, NULL);
	if (result < 0) {
		if (result == (int)0x80024e02) {
			SceKernelEventFlagInfo info = { .size = sizeof(info) };

			g_mount.event = sceKernelOpenEventFlag(name);
			sceKernelGetEventFlagInfo(g_mount.event, &info);
			sceKernelSetEventFlag(g_mount.event, info.currentPattern << 1);
		}
	} else {
		g_mount.createdEvent = 1;
		g_mount.event = result;
	}
	result = sceAppMgrWorkDirMount(200, g_mount.path);
	if (result == 0 || result == (int)0x80800003) {
		sceKernelUnlockMutex(g_mount.mutex, 1);
		return SQLITE_OK;
	}
	{
		SceKernelEventFlagInfo info = { .size = sizeof(info) };

		sceKernelGetEventFlagInfo(g_mount.event, &info);
		info.currentPattern >>= 1;
		sceKernelClearEventFlag(g_mount.event, info.currentPattern);
	}
	sceKernelUnlockMutex(g_mount.mutex, 1);
	if (g_mount.createdEvent)
		sceKernelDeleteEventFlag(g_mount.event);
	else
		sceKernelCloseEventFlag(g_mount.event);
	g_mount.event = -1;
	g_mount.createdEvent = 0;
	if (g_mount.createdMutex)
		sceKernelDeleteMutex(g_mount.mutex);
	else
		sceKernelCloseMutex(g_mount.mutex);
	g_mount.mutex = -1;
	g_mount.createdMutex = 0;
	return result;
}

int psp2Read(sqlite3_file *id, void *buffer, int amount, sqlite3_int64 offset)
{
	psp2File *file = (psp2File *)id;
	int count = sceIoPread(file->pShared->fd, buffer, amount, offset);

	if (count == amount)
		return SQLITE_OK;
	if (count < 0)
		return SQLITE_IOERR_READ;
	sce_paf_memset((char *)buffer + count, 0, amount - count);
	return SQLITE_IOERR_SHORT_READ;
}

int psp2Write(sqlite3_file *id, const void *buffer, int amount,
	      sqlite3_int64 offset)
{
	psp2File *file = (psp2File *)id;
	int count = 0;

	while (amount > 0) {
		count = sceIoPwrite(file->pShared->fd, buffer, amount, offset);
		if (count <= 0)
			break;
		amount -= count;
		buffer = (const char *)buffer + count;
		offset += count;
	}
	if (amount <= 0)
		return SQLITE_OK;
	if (count == 0 || count == (int)0x8001001c)
		return SQLITE_FULL;
	return SQLITE_IOERR_WRITE;
}

int psp2Sync(sqlite3_file *id, int flags)
{
	psp2File *file = (psp2File *)id;
	int result;

	(void)flags;
	result = sceIoSyncByFd(file->pShared->fd, 0);
	if (result < 0 && result != (int)0x80010030)
		return SQLITE_IOERR_FSYNC;
	return SQLITE_OK;
}

int psp2Truncate(sqlite3_file *id, sqlite3_int64 size)
{
	psp2File *file = (psp2File *)id;
	SceIoStat status = {
		.st_mode = 0,
		.st_attr = 0,
		.st_size = size,
		.st_ctime = { 0 },
		.st_atime = { 0 },
		.st_mtime = { 0 },
		.st_private = { 0 },
	};

	if (sceIoChstatByFd(file->pShared->fd, &status, 4) < 0)
		return SQLITE_IOERR_TRUNCATE;
	return SQLITE_OK;
}

int psp2FileSize(sqlite3_file *id, sqlite3_int64 *size)
{
	psp2File *file = (psp2File *)id;

	*size = sceIoLseek(file->pShared->fd, 0, SCE_SEEK_END);
	return *size < 0 ? SQLITE_IOERR_FSTAT : SQLITE_OK;
}

int psp2FileControl(sqlite3_file *id, int operation, void *argument)
{
	psp2File *file = (psp2File *)id;

	if (operation < 2) {
		if (operation < SQLITE_FCNTL_LOCKSTATE)
			return SQLITE_ERROR;
		sqlite3_mutex_enter(file->pShared->mutex);
		*(int *)argument = file->locktype;
		sqlite3_mutex_leave(file->pShared->mutex);
		return SQLITE_OK;
	}
	if (operation == SQLITE_LAST_ERRNO)
		return SQLITE_OK;
	return SQLITE_ERROR;
}

int psp2Delete(sqlite3_vfs *vfs, const char *path, int syncDirectory)
{
	char directory[256];
	const char *separator;
	unsigned int length;
	int result;

	(void)vfs;
	result = sceIoRemove(path);
	if (result == 0 && syncDirectory) {
		separator = sceClibStrrchr(path, '/');
		if (!separator)
			separator = sceClibStrchr(path, ':');
		if (separator) {
			length = separator - path;
			if (length < sizeof(directory)) {
				sce_paf_memcpy(directory, path, length);
				directory[length] = '\0';
				sceIoSync(directory, 0);
			}
		}
	}
	if (result == 0 || result == (int)0x80010002)
		return SQLITE_OK;
	return SQLITE_IOERR_DELETE;
}

int psp2Access(sqlite3_vfs *vfs, const char *path, int flags, int *result)
{
	SceIoStat status;
	int error;

	(void)vfs;
	status.st_mode = 0;
	ScePafStdc_75ECC54E(&status.st_attr, 0,
			 sizeof(status) - sizeof(status.st_mode));
	error = sceIoGetstat(path, &status);
	if (error == (int)0x80010002) {
		*result = 0;
		return SQLITE_OK;
	}
	if (error < 0) {
		*result = 0;
		return SQLITE_IOERR_ACCESS;
	}
	if (flags == SQLITE_ACCESS_EXISTS || flags == SQLITE_ACCESS_READ)
		*result = status.st_mode & 4;
	else if (flags == SQLITE_ACCESS_READWRITE)
		*result = status.st_mode & 6;
	return SQLITE_OK;
}

int psp2Open(sqlite3_vfs *vfs, const char *path, sqlite3_file *id,
	     int flags, int *outputFlags)
{
	static const char alphabet[] =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	char fullPath[257];
	SceIoStat status;
	unsigned int length, index;
	int openFlags = 0;
	int result, deleteOnClose = 0, powerLock = 0;

	if (!path) {
		if (!sqlite3_temp_directory) {
			if (psp2Mount() != SQLITE_OK)
				return SQLITE_ERROR;
			length = sce_paf_strlen(g_mount.path);
			sce_paf_memcpy(fullPath, g_mount.path, length);
		} else {
			sqlite3_snprintf(sizeof(fullPath), fullPath, "%s/etilqs_",
					 sqlite3_temp_directory);
			length = sce_paf_strlen(fullPath);
		}
		do {
			sqlite3_randomness(15, fullPath + length);
			for (index = 0; index < 15; index++)
				fullPath[length + index] = alphabet[
					(unsigned char)fullPath[length + index] % 62];
			length += 15;
			fullPath[length] = '\0';
		} while (sceIoGetstat(fullPath, &status) == 0);
	} else if (sceAppMgrGetRawPath((char *)path, fullPath, sizeof(fullPath)) != 0) {
		sqlite3_snprintf(sizeof(fullPath), fullPath, "%s", path);
	}
	sce_paf_memset(id, 0, sizeof(psp2File));
	if (flags & SQLITE_OPEN_READWRITE)
		openFlags = SCE_O_RDWR;
	if (flags & SQLITE_OPEN_READONLY)
		openFlags |= SCE_O_RDONLY;
	if (flags & SQLITE_OPEN_CREATE)
		openFlags |= SCE_O_CREAT;
	if (flags & SQLITE_OPEN_EXCLUSIVE)
		openFlags |= SCE_O_EXCL;
	if (flags & SQLITE_OPEN_DELETEONCLOSE)
		deleteOnClose = 1;
	if (flags & 0x10000000)
		powerLock = 1;
	result = psp2OpenShared((psp2File *)id, fullPath, openFlags | 0x10000,
				deleteOnClose, powerLock);
	if (result == SQLITE_OK) {
		if (outputFlags)
			*outputFlags = flags;
		id->pMethods = &g_ioMethods;
		return SQLITE_OK;
	}
	if (result < 0 && (flags & SQLITE_OPEN_READWRITE))
		return psp2Open(vfs, path, id,
			       (flags & ~SQLITE_OPEN_READWRITE) | SQLITE_OPEN_READONLY,
			       outputFlags);
	return result > 0 ? result : SQLITE_CANTOPEN;
}

int psp2FullPathname(sqlite3_vfs *vfs, const char *path, int size, char *output)
{
	(void)vfs;
	sqlite3_snprintf(size, output, "%s", path);
	return SQLITE_OK;
}

void *psp2DlOpen(sqlite3_vfs *vfs, const char *name)
{
	(void)vfs;
	(void)name;
	return NULL;
}

void psp2DlError(sqlite3_vfs *vfs, int size, char *output)
{
	(void)vfs;
	(void)size;
	(void)output;
}

void (*psp2DlSym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void)
{
	(void)vfs;
	(void)handle;
	(void)symbol;
	return NULL;
}

void psp2DlClose(sqlite3_vfs *vfs, void *handle)
{
	(void)vfs;
	(void)handle;
}

int psp2GetLastError(sqlite3_vfs *vfs, int size, char *output)
{
	(void)vfs;
	(void)size;
	(void)output;
	return 0;
}

int psp2SectorSize(sqlite3_file *id)
{
	(void)id;
	return 512;
}

int psp2DeviceCharacteristics(sqlite3_file *id)
{
	(void)id;
	return SQLITE_IOCAP_ATOMIC4K | SQLITE_IOCAP_SAFE_APPEND |
		SQLITE_IOCAP_SEQUENTIAL;
}

static const sqlite3_io_methods g_ioMethods = {
	.iVersion = 1,
	.xClose = psp2Close,
	.xRead = psp2Read,
	.xWrite = psp2Write,
	.xTruncate = psp2Truncate,
	.xSync = psp2Sync,
	.xFileSize = psp2FileSize,
	.xLock = psp2Lock,
	.xUnlock = psp2Unlock,
	.xCheckReservedLock = psp2CheckReservedLock,
	.xFileControl = psp2FileControl,
	.xSectorSize = psp2SectorSize,
	.xDeviceCharacteristics = psp2DeviceCharacteristics,
};

static sqlite3_vfs g_vfs = {
	.iVersion = 1,
	.szOsFile = sizeof(psp2File),
	.mxPathname = 256,
	.zName = "psp2",
	.xOpen = psp2Open,
	.xDelete = psp2Delete,
	.xAccess = psp2Access,
	.xFullPathname = psp2FullPathname,
	.xDlOpen = psp2DlOpen,
	.xDlError = psp2DlError,
	.xDlSym = psp2DlSym,
	.xDlClose = psp2DlClose,
	.xRandomness = psp2Randomness,
	.xSleep = psp2Sleep,
	.xCurrentTime = psp2CurrentTime,
	.xGetLastError = psp2GetLastError,
};

int sqlite3_os_init(void)
{
	sqlite3_vfs_register(&g_vfs, 1);
	return SQLITE_OK;
}

int sqlite3_os_end(void)
{
	SceKernelEventFlagInfo info = { .size = sizeof(info) };

	if (g_mount.path[0]) {
		sceKernelLockMutex(g_mount.mutex, 1, NULL);
		sceKernelGetEventFlagInfo(g_mount.event, &info);
		info.currentPattern >>= 1;
		sceKernelClearEventFlag(g_mount.event, info.currentPattern);
		if (info.currentPattern)
			g_mount.path[0] = '\0';
		else if (sceAppMgrUmount(g_mount.path) == 0)
			g_mount.path[0] = '\0';
		sceKernelUnlockMutex(g_mount.mutex, 1);
		if (g_mount.createdEvent)
			sceKernelDeleteEventFlag(g_mount.event);
		else
			sceKernelCloseEventFlag(g_mount.event);
		g_mount.event = -1;
		g_mount.createdEvent = 0;
		if (g_mount.createdMutex)
			sceKernelDeleteMutex(g_mount.mutex);
		else
			sceKernelCloseMutex(g_mount.mutex);
		g_mount.mutex = -1;
		g_mount.createdMutex = 0;
	}
	return SQLITE_OK;
}
