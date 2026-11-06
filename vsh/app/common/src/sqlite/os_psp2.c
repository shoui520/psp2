/*
 * SceSqliteVsh's operating system layer: the "psp2" VFS SQLite runs on. Files are opened
 * through SceIofilemgr and shared by every connection of the process that opens the same
 * path (one psp2Shared each, with its descriptor, its locks and its open count); SQLite's
 * locks map onto sceIoFlockForSystem. Temporary files go to the work directory SceAppMgr
 * mounts for the process, kept mounted while any SQLite module of the process uses it (a
 * named mutex and event flag count them). Time and randomness come from SceRtc.
 */

#include <stddef.h>
#include <time.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/error.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <kernel/processmgr.h>
#include <kernel/threadmgr.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <std/string.h>
#include "sqlite3.h"

/* the platform's open flag: hold a power lock while the file is locked */
/* st_mode's system access bits (iofilemgr/iofilemgr.h's IO_STM_RS, IO_STM_RWS) */
#define PSP2_STM_RS				0004
#define PSP2_STM_RWS				0006
#define SQLITE_OPEN_PSP2_POWER_LOCK		0x10000000

/* sceIoFlockForSystem's lock operations, by SQLite lock level */
#define PSP2_FLOCK_UNLOCK			0x110
#define PSP2_FLOCK_SHARED			0x101
#define PSP2_FLOCK_RESERVED			0x104
#define PSP2_FLOCK_EXCLUSIVE			0x102

/* st_mode's access bits xAccess reads (others' read and write) */

/* sceIoOpen's flag that every database file is opened with */
#define PSP2_IO_OPEN_DATABASE			0x10000

/* the SceIofilemgr cache a database file gets (sceIoIoctl 0x1001) */
#define PSP2_IOCTL_SET_CACHE			0x1001

typedef struct psp2IoCacheParam {
	unsigned int reserved0[3];
	unsigned int pageSize;		/* 4096 */
	unsigned int reserved1;
	unsigned int pageCount;		/* 8 */
	unsigned int reserved2[2];
} psp2IoCacheParam;

/* one open file, shared by the connections of the process that open its path */
typedef struct psp2Shared {
	SceUID fd;
	char *zPath;
	signed char nShared;		/* shared locks held; -1: exclusive */
	signed char eLock;		/* the strongest lock held: SQLITE_LOCK_* */
	signed char nRef;		/* psp2File objects open on it */
	signed char powerLock;		/* a power lock while it is locked */
	int openFlags;			/* sceIoOpen's flags */
	sqlite3_mutex *mutex;
	struct psp2Shared *pNext;
} psp2Shared;

typedef struct psp2File {
	const sqlite3_io_methods *pMethod;
	psp2Shared *pShared;
	int locktype;			/* this connection's lock: SQLITE_LOCK_* */
} psp2File;

/* the process's work directory, mounted while a SQLite module of the process uses it */
typedef struct psp2MountState {
	char path[16];
	SceUID mutex;
	int createdMutex;
	SceUID event;			/* its pattern counts the users, one bit each */
	int createdEvent;
} psp2MountState;

static int psp2Close(sqlite3_file *id);
static int psp2Read(sqlite3_file *id, void *buffer, int amount, sqlite3_int64 offset);
static int psp2Write(sqlite3_file *id, const void *buffer, int amount, sqlite3_int64 offset);
static int psp2Truncate(sqlite3_file *id, sqlite3_int64 size);
static int psp2Sync(sqlite3_file *id, int flags);
static int psp2FileSize(sqlite3_file *id, sqlite3_int64 *size);
static int psp2Lock(sqlite3_file *id, int locktype);
static int psp2Unlock(sqlite3_file *id, int locktype);
static int psp2CheckReservedLock(sqlite3_file *id, int *reserved);
static int psp2FileControl(sqlite3_file *id, int operation, void *argument);
static int psp2SectorSize(sqlite3_file *id);
static int psp2DeviceCharacteristics(sqlite3_file *id);

static const sqlite3_io_methods g_ioMethods = {
	1,
	psp2Close,
	psp2Read,
	psp2Write,
	psp2Truncate,
	psp2Sync,
	psp2FileSize,
	psp2Lock,
	psp2Unlock,
	psp2CheckReservedLock,
	psp2FileControl,
	psp2SectorSize,
	psp2DeviceCharacteristics,
	NULL,
	NULL,
	NULL,
	NULL,
};

static psp2Shared *g_sharedList;
static psp2MountState g_mount = {
	{ 0 },
	-1,
	0,
	-1,
	0,
};
static sqlite3_vfs g_vfs;

/* SQLite's date functions convert through this (the SDK has no localtime_r) */
struct tm *localtime_r(const time_t *timer, struct tm *result)
{
	SceRtcTick tick;
	SceDateTime date;

	sceRtcSetTime_t(&date, *timer);
	sceRtcGetTick(&date, &tick);
	sceRtcConvertUtcToLocalTime(&tick, &tick);
	sceRtcSetTick(&date, &tick);
	result->tm_mon = date.month - 1;
	result->tm_year = date.year - 1900;
	result->tm_hour = date.hour;
	result->tm_mday = date.day;
	result->tm_sec = date.second;
	result->tm_min = date.minute;
	return result;
}

int sqlite3_os_init(void)
{
	sqlite3_vfs_register(&g_vfs, 1);
	return SQLITE_OK;
}

/* drops this module from the work directory's users, and unmounts it after the last */
int sqlite3_os_end(void)
{
	if (g_mount.path[0]) {
		SceKernelEventFlagInfo info = { sizeof(info) };

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

/* mounts the work directory for temporary files, or adds this module to its users */
static __attribute__((noinline)) int psp2Mount(void)
{
	char name[32];
	SceUID process;
	int result;

	if (g_mount.path[0])
		return SQLITE_OK;
	process = sceKernelGetProcessId();
	sqlite3_snprintf(sizeof(name), name, "SceSqliteMtx%08x", process);
	result = sceKernelCreateMutex(name, SCE_KERNEL_ATTR_OPENABLE, 1, NULL);
	if (result < 0) {
		if (result == SCE_KERNEL_ERROR_DUPLICATE_NAME) {
			g_mount.mutex = sceKernelOpenMutex(name);
			sceKernelLockMutex(g_mount.mutex, 1, NULL);
		}
	} else {
		g_mount.mutex = result;
		g_mount.createdMutex = 1;
	}
	sqlite3_snprintf(sizeof(name), name, "SceSqliteEvf%08x", process);
	result = sceKernelCreateEventFlag(name, SCE_KERNEL_ATTR_OPENABLE, 1, NULL);
	if (result < 0) {
		if (result == SCE_KERNEL_ERROR_DUPLICATE_NAME) {
			g_mount.event = sceKernelOpenEventFlag(name);
			SceKernelEventFlagInfo info = { sizeof(info) };

			sceKernelGetEventFlagInfo(g_mount.event, &info);
			sceKernelSetEventFlag(g_mount.event, info.currentPattern << 1);
		}
	} else {
		g_mount.event = result;
		g_mount.createdEvent = 1;
	}
	result = sceAppMgrWorkDirMount(200, (struct ScePfsRndDriveId *)g_mount.path);
	if (result == 0 || result == APPMGR_ERROR_ALREADY_MOUNTED) {
		sceKernelUnlockMutex(g_mount.mutex, 1);
		return SQLITE_OK;
	} else {
		SceKernelEventFlagInfo info = { sizeof(info) };

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

static const unsigned int g_lockFlags[] = {
	PSP2_FLOCK_UNLOCK,		/* SQLITE_LOCK_NONE */
	PSP2_FLOCK_SHARED,		/* SQLITE_LOCK_SHARED */
	PSP2_FLOCK_RESERVED,		/* SQLITE_LOCK_RESERVED */
	0,				/* SQLITE_LOCK_PENDING: none of its own */
	PSP2_FLOCK_EXCLUSIVE,		/* SQLITE_LOCK_EXCLUSIVE */
};

/* takes the file lock of SQLite lock level locktype */
static int psp2LockFile(psp2Shared *shared, int locktype)
{
	int result = SQLITE_OK;

	if (locktype < sizeof(g_lockFlags) / sizeof(g_lockFlags[0]) && g_lockFlags[locktype]) {
		result = sceIoFlockForSystem(shared->fd, g_lockFlags[locktype]);
		if (result != 0) {
			if (result == SCE_KERNEL_ERROR_IO_WOULD_BLOCK ||
			    result == SCE_ERROR_ERRNO_EBUSY)
				result = SQLITE_BUSY;
			else
				result = SQLITE_IOERR_LOCK;
		}
	}
	return result;
}

/* drops the file lock to SQLite lock level locktype; a device without locks has none to drop */
static int psp2UnlockFile(psp2Shared *shared, int locktype)
{
	int result = SQLITE_OK;

	if (locktype < sizeof(g_lockFlags) / sizeof(g_lockFlags[0]) && g_lockFlags[locktype]) {
		result = sceIoFlockForSystem(shared->fd, g_lockFlags[locktype]);
		if (result != 0) {
			if (result == SCE_KERNEL_ERROR_IO_WOULD_BLOCK ||
			    result == SCE_ERROR_ERRNO_EBUSY)
				result = SQLITE_BUSY;
			else if (result == SCE_ERROR_ERRNO_ENODEV)
				result = SQLITE_OK;
			else
				result = SQLITE_IOERR_LOCK;
		}
	}
	return result;
}

/*
 * Opens the path for the file, sharing the psp2Shared of the process when one has it open.
 * A shared file opened read-only is reopened read-write for a writer.
 */
static __attribute__((noinline)) int psp2OpenShared(psp2File *file, const char *path, int flags, char deleteOnClose,
			  char powerLock)
{
	psp2Shared *shared;
	psp2IoCacheParam cache;
	char directory[256];
	const char *separator;
	char *copy;
	sqlite3_mutex *mutex;
	unsigned int length;
	int result = SQLITE_OK;

	sqlite3_mutex_enter(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER));
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
				if (result != 0)
					goto leave;
				shared->fd = 0;
				result = sceIoOpen(shared->zPath, shared->openFlags | SCE_O_RDWR, 6);
				if (result < 0)
					goto leave;
				shared->fd = result;
				result = SQLITE_OK;
				shared->openFlags |= SCE_O_RDWR;
			}
			shared->nRef++;
			shared->powerLock |= powerLock;
			file->pShared = shared;
		}
leave:
		sqlite3_mutex_leave(shared->mutex);
	} else {
		length = sce_paf_strlen(path) + 1;
		copy = sqlite3_malloc(length);
		shared = sqlite3_malloc(sizeof(*shared));
		mutex = sqlite3_mutex_alloc(SQLITE_MUTEX_FAST);
		if (copy && shared && mutex) {
			sce_paf_memset(shared, 0, sizeof(*shared));
			sce_paf_memcpy(copy, path, length);
			shared->zPath = copy;
			shared->openFlags = flags;
			shared->mutex = mutex;
			shared->nRef = 1;
			shared->powerLock = powerLock;
			file->pShared = shared;
		} else {
			sqlite3_free(copy);
			sqlite3_free(shared);
			sqlite3_mutex_free(mutex);
			result = SQLITE_NOMEM;
		}
		if (result != SQLITE_OK)
			goto sync;
		result = sceIoOpen(file->pShared->zPath, file->pShared->openFlags, 6);
		shared = file->pShared;
		if (result < 0) {
			if (shared) {
				sqlite3_free(shared->zPath);
				sqlite3_mutex_free(shared->mutex);
				sqlite3_free(shared);
			}
			file->pShared = NULL;
			goto sync;
		}
		shared->fd = result;
		shared->pNext = g_sharedList;
		g_sharedList = shared;
		result = SQLITE_OK;
	}
	if (result == SQLITE_OK) {
		sce_paf_memset(&cache, 0, sizeof(cache));
		cache.pageSize = 4096;
		cache.pageCount = 8;
		sceIoIoctl(file->pShared->fd, PSP2_IOCTL_SET_CACHE, &cache, sizeof(cache), NULL, 0);
		if (deleteOnClose)
			sceIoRemove(path);
	}
sync:
	/* a new file's directory entry is written at once */
	if (result == SQLITE_OK && !deleteOnClose && (flags & SCE_O_CREAT)) {
		separator = sceClibStrrchr(path, '/');
		if (separator || (separator = sceClibStrchr(path, ':'))) {
			length = separator - path;
			if (length < sizeof(directory)) {
				sce_paf_memcpy(directory, path, length);
				directory[length] = '\0';
				sceIoSync(directory, 0);
			}
		}
	}
	sqlite3_mutex_leave(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER));
	return result;
}

/* gives up one shared lock of the file: the last one leaves it unlocked */
static void psp2ReleaseShared(psp2File *file, psp2Shared *shared, int locktype)
{
	if (shared->nShared <= 1) {
		shared->nShared = 0;
		shared->eLock = locktype;
	} else {
		shared->nShared--;
	}
	file->locktype = locktype;
}

static __attribute__((noinline)) int psp2Unlock(sqlite3_file *id, int locktype)
{
	psp2File *file = (psp2File *)id;
	psp2Shared *shared;
	int result = SQLITE_OK;

	sqlite3_mutex_enter(file->pShared->mutex);
	shared = file->pShared;
	if (file->locktype != locktype && shared->nShared) {
		if (locktype == SQLITE_LOCK_NONE) {
			if (shared->nShared > 1) {
				psp2ReleaseShared(file, shared, locktype);
			} else {
				result = psp2UnlockFile(shared, locktype);
				shared = file->pShared;
				if (result == SQLITE_OK) {
					psp2ReleaseShared(file, shared, locktype);
					if (shared->powerLock) {
						sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
						shared = file->pShared;
					}
				}
			}
		} else if (locktype == SQLITE_LOCK_SHARED) {
			result = psp2UnlockFile(shared, locktype);
			shared = file->pShared;
			if (result == SQLITE_OK) {
				if (locktype == SQLITE_LOCK_SHARED) {
					if (file->locktype == SQLITE_LOCK_EXCLUSIVE)
						shared->nShared = 1;
					shared->eLock = locktype;
				}
				file->locktype = locktype;
			}
		}
	}
	sqlite3_mutex_leave(shared->mutex);
	return result;
}

static int psp2Close(sqlite3_file *id)
{
	psp2File *file = (psp2File *)id;
	psp2Shared *shared = file->pShared;
	psp2Shared *entry, *previous;
	int result = SQLITE_OK;
	int last = 0;

	if (!shared)
		return SQLITE_IOERR;
	psp2Unlock(id, SQLITE_LOCK_NONE);
	sqlite3_mutex_enter(shared->mutex);
	if (--shared->nRef <= 0) {
		sqlite3_mutex_enter(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER));
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
		if (shared)
			sqlite3_free(shared->zPath);
		sqlite3_mutex_free(shared->mutex);
		sqlite3_free(shared);
	}
	if (result == SQLITE_OK)
		file->pShared = NULL;
	return result;
}

static int psp2Read(sqlite3_file *id, void *buffer, int amount, sqlite3_int64 offset)
{
	psp2File *file = (psp2File *)id;
	int count;

	count = sceIoPread(file->pShared->fd, buffer, amount, offset);
	if (count == amount)
		return SQLITE_OK;
	if (count < 0)
		return SQLITE_IOERR_READ;
	/* SQLite expects the rest of a short read cleared */
	sce_paf_memset((char *)buffer + count, 0, amount - count);
	return SQLITE_IOERR_SHORT_READ;
}

static int psp2Write(sqlite3_file *id, const void *buffer, int amount, sqlite3_int64 offset)
{
	psp2File *file = (psp2File *)id;
	psp2Shared *shared = file->pShared;
	int count = 0;

	while (amount > 0) {
		count = sceIoPwrite(shared->fd, buffer, amount, offset);
		if (count <= 0)
			break;
		amount -= count;
		offset += count;
		buffer = (const char *)buffer + count;
	}
	if (amount <= 0)
		return SQLITE_OK;
	if (count == 0 || count == SCE_ERROR_ERRNO_ENOSPC)
		return SQLITE_FULL;
	return SQLITE_IOERR_WRITE;
}

static int psp2Truncate(sqlite3_file *id, sqlite3_int64 size)
{
	psp2File *file = (psp2File *)id;
	SceIoStat status;

	memset(&status, 0, sizeof(status));
	status.st_size = size;
	if (sceIoChstatByFd(file->pShared->fd, &status, SCE_CST_SIZE) < 0)
		return SQLITE_IOERR_TRUNCATE;
	return SQLITE_OK;
}

static int psp2Sync(sqlite3_file *id, int flags)
{
	psp2File *file = (psp2File *)id;
	int result;

	result = sceIoSyncByFd(file->pShared->fd, 0);
	if (result >= 0 || result == SCE_ERROR_ERRNO_EUNSUP)
		return SQLITE_OK;
	return SQLITE_IOERR_FSYNC;
}

static int psp2FileSize(sqlite3_file *id, sqlite3_int64 *size)
{
	psp2File *file = (psp2File *)id;

	*size = sceIoLseek(file->pShared->fd, 0, SCE_SEEK_END);
	if (*size < 0)
		return SQLITE_IOERR_FSTAT;
	return SQLITE_OK;
}

/* records that the connection now holds lock level locktype */
static void psp2SetLock(psp2File *file, psp2Shared *shared, int locktype)
{
	if (locktype == SQLITE_LOCK_SHARED)
		shared->nShared++;
	else if (locktype == SQLITE_LOCK_EXCLUSIVE)
		shared->nShared = -1;
	file->locktype = locktype;
	if (shared->eLock < locktype)
		shared->eLock = locktype;
}

static int psp2Lock(sqlite3_file *id, int locktype)
{
	psp2File *file = (psp2File *)id;
	psp2Shared *shared;
	int result = SQLITE_OK;

	sqlite3_mutex_enter(file->pShared->mutex);
	shared = file->pShared;
	if (locktype <= file->locktype)
		goto out;
	if (locktype > SQLITE_LOCK_NONE && file->locktype < SQLITE_LOCK_SHARED) {
		if (shared->eLock > SQLITE_LOCK_SHARED) {
			result = SQLITE_BUSY;
		} else if (shared->nShared > 0) {
			psp2SetLock(file, shared, SQLITE_LOCK_SHARED);
		} else if (shared->nShared < 0) {
			result = SQLITE_BUSY;
		} else {
			if (!shared->powerLock ||
			    (result = sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT)) == 0) {
				result = psp2LockFile(shared, SQLITE_LOCK_SHARED);
				if (result != SQLITE_OK && shared->powerLock)
					sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
			}
			shared = file->pShared;
			if (result == SQLITE_OK)
				psp2SetLock(file, shared, SQLITE_LOCK_SHARED);
		}
	}
	if (result == SQLITE_OK && locktype >= SQLITE_LOCK_RESERVED &&
	    file->locktype < SQLITE_LOCK_RESERVED) {
		if (shared->eLock >= SQLITE_LOCK_RESERVED) {
			result = SQLITE_BUSY;
			goto out;
		}
		result = psp2LockFile(shared, SQLITE_LOCK_RESERVED);
		shared = file->pShared;
		if (result == SQLITE_OK)
			psp2SetLock(file, shared, SQLITE_LOCK_RESERVED);
	}
	if (result == SQLITE_OK && locktype == SQLITE_LOCK_EXCLUSIVE) {
		if (shared->nShared != 0 && (shared->nShared != 1 ||
		    file->locktype < SQLITE_LOCK_RESERVED || file->locktype >= SQLITE_LOCK_EXCLUSIVE)) {
			result = SQLITE_BUSY;
			if (shared->eLock < SQLITE_LOCK_PENDING)
				psp2SetLock(file, shared, SQLITE_LOCK_PENDING);
		} else {
			result = psp2LockFile(shared, locktype);
			shared = file->pShared;
			if (result != SQLITE_OK)
				psp2SetLock(file, shared, SQLITE_LOCK_PENDING);
			else
				psp2SetLock(file, shared, locktype);
		}
	}
out:
	sqlite3_mutex_leave(shared->mutex);
	return result;
}

/* whether a connection holds a reserved lock: tried, and released again, when none here does */
static int psp2CheckReservedLock(sqlite3_file *id, int *reserved)
{
	psp2File *file = (psp2File *)id;
	psp2Shared *shared;
	int result = SQLITE_OK;
	int held = 0;
	unsigned int previous;

	sqlite3_mutex_enter(file->pShared->mutex);
	shared = file->pShared;
	if (shared->nShared >= 0 && shared->eLock < SQLITE_LOCK_RESERVED) {
		if (shared->powerLock) {
			sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
			shared = file->pShared;
		}
		result = psp2LockFile(shared, SQLITE_LOCK_RESERVED);
		shared = file->pShared;
		if (result != SQLITE_OK) {
			if (result == SQLITE_BUSY)
				held = 1;
		} else {
			/* SQLITE_LOCK_RESERVED was free: give it back */
			previous = shared->eLock;
			shared->eLock = SQLITE_LOCK_RESERVED;
			psp2UnlockFile(shared, previous);
			shared = file->pShared;
			shared->eLock = previous;
		}
		if (shared->powerLock) {
			sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
			shared = file->pShared;
		}
	} else {
		held = 1;
	}
	sqlite3_mutex_leave(shared->mutex);
	*reserved = held;
	return result;
}

static int psp2FileControl(sqlite3_file *id, int operation, void *argument)
{
	psp2File *file = (psp2File *)id;

	switch (operation) {
	case SQLITE_FCNTL_LOCKSTATE:
		sqlite3_mutex_enter(file->pShared->mutex);
		*(int *)argument = file->locktype;
		sqlite3_mutex_leave(file->pShared->mutex);
		return SQLITE_OK;
	case SQLITE_LAST_ERRNO:
		return SQLITE_OK;
	}
	return SQLITE_ERROR;
}

static int psp2SectorSize(sqlite3_file *id)
{
	return 512;
}

static int psp2DeviceCharacteristics(sqlite3_file *id)
{
	return SQLITE_IOCAP_ATOMIC4K | SQLITE_IOCAP_SAFE_APPEND | SQLITE_IOCAP_SEQUENTIAL;
}

/*
 * Opens a database or journal file; one without a path is a temporary file with a random
 * name, in sqlite3_temp_directory or else the work directory. A file that cannot be opened
 * read-write is opened read-only.
 */
static int psp2Open(sqlite3_vfs *vfs, const char *path, sqlite3_file *id, int flags,
		    int *outputFlags)
{
	static const char alphabet[] =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	char fullPath[257];
	SceIoStat status;
	unsigned int length, index;
	int openFlags = 0;
	char deleteOnClose = 0, powerLock = 0;
	int result;

	if (path) {
		if (sceAppMgrGetRawPath(path, fullPath, sizeof(fullPath)) != 0)
			sqlite3_snprintf(sizeof(fullPath), fullPath, "%s", path);
	} else {
		if (sqlite3_temp_directory) {
			sqlite3_snprintf(sizeof(fullPath), fullPath, "%s/etilqs_",
					 sqlite3_temp_directory);
			length = sce_paf_strlen(fullPath);
		} else {
			if (psp2Mount() != SQLITE_OK)
				return SQLITE_ERROR;
			length = sce_paf_strlen(g_mount.path);
			sce_paf_memcpy(fullPath, g_mount.path, length);
		}
		do {
			sqlite3_randomness(15, &fullPath[length]);
			for (index = 0; index < 15; index++)
				fullPath[length + index] =
					alphabet[(unsigned char)fullPath[length + index] %
						 (sizeof(alphabet) - 1)];
			fullPath[length + 15] = '\0';
			length += 15;
		} while (sceIoGetstat(fullPath, &status) == 0);
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
	if (flags & SQLITE_OPEN_PSP2_POWER_LOCK)
		powerLock = 1;
	result = psp2OpenShared((psp2File *)id, fullPath, openFlags | PSP2_IO_OPEN_DATABASE,
				deleteOnClose, powerLock);
	if (result == SQLITE_OK) {
		if (outputFlags)
			*outputFlags = flags;
		id->pMethods = &g_ioMethods;
		return SQLITE_OK;
	}
	if (result < 0 && (flags & SQLITE_OPEN_READWRITE))
		return psp2Open(vfs, path, id,
				(flags & ~SQLITE_OPEN_READWRITE) | SQLITE_OPEN_READONLY, outputFlags);
	return result > 0 ? result : SQLITE_CANTOPEN;
}

static int psp2Delete(sqlite3_vfs *vfs, const char *path, int syncDirectory)
{
	char directory[256];
	const char *separator;
	unsigned int length;
	int result;

	result = sceIoRemove(path);
	if (result == 0 && syncDirectory) {
		separator = sceClibStrrchr(path, '/');
		if (separator || (separator = sceClibStrchr(path, ':'))) {
			length = separator - path;
			if (length < sizeof(directory)) {
				sce_paf_memcpy(directory, path, length);
				directory[length] = '\0';
				sceIoSync(directory, 0);
			}
		}
	}
	if (result == 0 || result == SCE_ERROR_ERRNO_ENOENT)
		return SQLITE_OK;
	return SQLITE_IOERR_DELETE;
}

static int psp2Access(sqlite3_vfs *vfs, const char *path, int flags, int *result)
{
	SceIoStat status = { 0 };
	int error;

	error = sceIoGetstat(path, &status);
	if (error == SCE_ERROR_ERRNO_ENOENT) {
		*result = 0;
		return SQLITE_OK;
	}
	if (error < 0) {
		*result = 0;
		return SQLITE_IOERR_ACCESS;
	}
	switch (flags) {
	case SQLITE_ACCESS_EXISTS:
	case SQLITE_ACCESS_READ:
		*result = status.st_mode & PSP2_STM_RS;
		break;
	case SQLITE_ACCESS_READWRITE:
		*result = status.st_mode & (PSP2_STM_RWS);
		break;
	}
	return SQLITE_OK;
}

static int psp2FullPathname(sqlite3_vfs *vfs, const char *path, int size, char *output)
{
	sqlite3_snprintf(size, output, "%s", path);
	return SQLITE_OK;
}

static void *psp2DlOpen(sqlite3_vfs *vfs, const char *name)
{
	return NULL;
}

static void psp2DlError(sqlite3_vfs *vfs, int size, char *output)
{
}

static void (*psp2DlSym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void)
{
	return NULL;
}

static void psp2DlClose(sqlite3_vfs *vfs, void *handle)
{
}

/* the current tick, then the process ID */
static int psp2Randomness(sqlite3_vfs *vfs, int size, char *buffer)
{
	SceRtcTick tick;
	SceUID pid;
	int count, extra;

	sce_paf_memset(buffer, 0, size);
	sceRtcGetCurrentTick(&tick);
	count = (unsigned int)size > sizeof(tick) ? (int)sizeof(tick) : size;
	sce_paf_memcpy(buffer, &tick, count);
	if (count < size) {
		pid = sceKernelGetProcessId();
		extra = (unsigned int)(size - count) > sizeof(pid) ? (int)sizeof(pid) : size - count;
		sce_paf_memcpy(buffer + count, &pid, extra);
		count += extra;
	}
	return count;
}

static int psp2Sleep(sqlite3_vfs *vfs, int microseconds)
{
	sceKernelDelayThread(microseconds);
	return microseconds;
}

/* the current UTC time as a Julian day number */
static int psp2CurrentTime(sqlite3_vfs *vfs, double *time)
{
	SceDateTime now;
	time_t seconds = 0;

	sceRtcGetCurrentClock(&now, 0);
	sceRtcGetTime_t(&now, &seconds);
	*time = seconds / 86400.0 + 2440587.5 + (int)now.microsecond / 86400000000.0;
	return SQLITE_OK;
}

static int psp2GetLastError(sqlite3_vfs *vfs, int size, char *output)
{
	return 0;
}

static sqlite3_vfs g_vfs = {
	1,				/* iVersion */
	sizeof(psp2File),		/* szOsFile */
	256,				/* mxPathname */
	NULL,				/* pNext */
	"psp2",				/* zName */
	NULL,				/* pAppData */
	psp2Open,
	psp2Delete,
	psp2Access,
	psp2FullPathname,
	psp2DlOpen,
	psp2DlError,
	psp2DlSym,
	psp2DlClose,
	psp2Randomness,
	psp2Sleep,
	psp2CurrentTime,
	psp2GetLastError,
};
