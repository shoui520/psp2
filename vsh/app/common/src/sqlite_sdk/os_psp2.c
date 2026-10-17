/*
 * SceSqlite: the PS Vita OS layer of SQLite ("psp2" VFS), Sony's
 * vsh/app/common/src/sqlite_sdk/os_psp2.c.
 *
 * Files are read through sceIo.  Writes, truncation and deletion of files on
 * savedata0: go through SceAppMgr's savedata calls, which keep the mounted
 * savedata image consistent; the descriptor itself is only ever opened
 * read-only.  Every open file has one shared record per path (a list under
 * the static master mutex) carrying the descriptor and the lock state that
 * the connections of this process share: the locks are process-local, the
 * same scheme os_unix.c uses for its inode records.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <psp2/types.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include "../sqlite/vfs_time.h"
#include <appmgr/appmgr_savedata.h>
#include "contrib/sqlite-3.6.23.1/sqlite3.h"

/* SCE errno values (sceerror.h) */
#define SCE_ERROR_ERRNO_EPERM		0x80010001
#define SCE_ERROR_ERRNO_ENOENT		0x80010002
#define SCE_ERROR_ERRNO_EINTR		0x80010004
#define SCE_ERROR_ERRNO_EAGAIN		0x8001000B
#define SCE_ERROR_ERRNO_EACCES		0x8001000D
#define SCE_ERROR_ERRNO_EBUSY		0x80010010
#define SCE_ERROR_ERRNO_ENOLCK		0x8001002E
#define SCE_ERROR_ERRNO_ETIMEDOUT	0x80010074

/* savedata errors whose SDK names are unknown */
#define SCE_SAVEDATA_ERROR_80801006	0x80801006	/* out of memory */
#define SCE_SAVEDATA_ERROR_8080108C	0x8080108C	/* no space */
#define SCE_SAVEDATA_ERROR_8080108D	0x8080108D	/* no space */

#define PSP2_SAVEDATA_MOUNT		"savedata0:"
#define PSP2_SAVEDATA_MOUNT_LEN		10
#define PSP2_MAX_PATHNAME		256
#define PSP2_SECTOR_SIZE		512

/* Sony's messages carry the build path and the line of os_psp2.c */
#define PSP2_SOURCE	"W:\\Build\\J01509658\\vsh\\app\\common\\src\\sqlite_sdk\\os_psp2.c"
#define PSP2_ERROR(line, fmt, ...) \
	printf("ERROR: " PSP2_SOURCE "(" #line "): " fmt, ##__VA_ARGS__)

/* the record all connections to one path share */
typedef struct psp2Shared {
	SceUID fd;
	char *zPath;
	signed char nShared;		/* SHARED holders, -1 while EXCLUSIVE */
	signed char eLock;		/* strongest lock any connection holds */
	signed char nRef;		/* open connections */
	signed char unkB;		/* or of the opens' unkFlags */
	int openFlags;			/* SCE_O_* of the first open */
	sqlite3_mutex *mutex;		/* guards the lock fields */
	int isDelete;			/* remove the file on the last close */
	struct psp2Shared *pNext;
} psp2Shared;

/* sqlite3_file of the psp2 VFS */
typedef struct psp2File {
	const sqlite3_io_methods *pMethod;
	psp2Shared *pShared;
	int locktype;			/* SQLITE_LOCK_* this connection holds */
	int openFlags;			/* SCE_O_* of this connection */
} psp2File;

static int psp2Open(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *id,
		    int flags, int *pOutFlags);
static int psp2Delete(sqlite3_vfs *pVfs, const char *zPath, int dirSync);
static int psp2Access(sqlite3_vfs *pVfs, const char *zPath, int flags,
		      int *pResOut);
static int psp2FullPathname(sqlite3_vfs *pVfs, const char *zPath, int nOut,
			    char *zOut);
static void *psp2DlOpen(sqlite3_vfs *pVfs, const char *zFilename);
static void psp2DlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg);
static void (*psp2DlSym(sqlite3_vfs *pVfs, void *pHandle, const char *zSymbol))(void);
static void psp2DlClose(sqlite3_vfs *pVfs, void *pHandle);
static int psp2GetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf);

static int psp2Close(sqlite3_file *id);
static int psp2Read(sqlite3_file *id, void *pBuf, int amt, sqlite3_int64 offset);
static int psp2Write(sqlite3_file *id, const void *pBuf, int amt,
		     sqlite3_int64 offset);
static int psp2Truncate(sqlite3_file *id, sqlite3_int64 nByte);
static int psp2Sync(sqlite3_file *id, int flags);
static int psp2FileSize(sqlite3_file *id, sqlite3_int64 *pSize);
static int psp2Lock(sqlite3_file *id, int locktype);
static int psp2Unlock(sqlite3_file *id, int locktype);
static int psp2CheckReservedLock(sqlite3_file *id, int *pResOut);
static int psp2FileControl(sqlite3_file *id, int op, void *pArg);
static int psp2SectorSize(sqlite3_file *id);
static int psp2DeviceCharacteristics(sqlite3_file *id);

static psp2Shared *g_sharedList = NULL;
static SceUID g_processId = -1;

static sqlite3_vfs psp2Vfs = {
	1,				/* iVersion */
	sizeof(psp2File),		/* szOsFile */
	PSP2_MAX_PATHNAME,		/* mxPathname */
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

static const sqlite3_io_methods psp2IoMethods = {
	1,				/* iVersion */
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
};

/* the module has nothing to set up: SQLite initialises on first use */
int module_start(SceSize argc, const void *args)
{
	return 0;
}

int module_stop(SceSize argc, const void *args)
{
	return 0;
}

int sqlite3_os_init(void)
{
	sqlite3_vfs_register(&psp2Vfs, 1);
	g_processId = sceKernelGetProcessId();
	return SQLITE_OK;
}

int sqlite3_os_end(void)
{
	return SQLITE_OK;
}

/* SCE error -> SQLite result code; other failures become defaultCode */
static int psp2ErrorCode(int err, int defaultCode)
{
	if (err >= 0)
		return err;

	switch (err) {
	case SCE_SAVEDATA_ERROR_8080108C:
	case SCE_SAVEDATA_ERROR_8080108D:
		return SQLITE_FULL;
	case SCE_SAVEDATA_ERROR_80801006:
		return SQLITE_NOMEM;
	case SCE_ERROR_ERRNO_EINTR:
	case SCE_ERROR_ERRNO_EAGAIN:
	case SCE_ERROR_ERRNO_EBUSY:
	case SCE_ERROR_ERRNO_ENOLCK:
	case SCE_ERROR_ERRNO_ETIMEDOUT:
		return SQLITE_BUSY;
	case SCE_ERROR_ERRNO_EPERM:
	case SCE_ERROR_ERRNO_EACCES:
		return SQLITE_PERM;
	}
	return defaultCode;
}

static void psp2FreeShared(psp2Shared *pShared)
{
	if (pShared != NULL) {
		sqlite3_free(pShared->zPath);
		sqlite3_mutex_free(pShared->mutex);
		sqlite3_free(pShared);
	}
}

/*
 * Attach pFile to the shared record of zPath, creating the record and
 * opening the file if this is the path's first connection.  Returns 0, an
 * SQLite code, or a negative SCE error.
 */
static __attribute__((noinline)) int psp2OpenShared(psp2File *pFile, const char *zPath, int openFlags,
			  char isDelete, int unkFlags)
{
	psp2Shared *pShared;
	char *zCopy;
	sqlite3_mutex *mutex;
	SceIoStat stat;
	SceAppUtilSaveDataDataSaveItem item;
	SceAppMgrSaveDataData param;
	size_t n;
	int rc = 0;

	sqlite3_mutex_enter(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER));

	for (pShared = g_sharedList; pShared != NULL; pShared = pShared->pNext) {
		if (strcmp(zPath, pShared->zPath) == 0)
			break;
	}
	if (pShared != NULL) {
		sqlite3_mutex_enter(pShared->mutex);
		if (openFlags & SCE_O_EXCL) {
			rc = SQLITE_CANTOPEN;
		} else {
			pShared->unkB |= unkFlags;
			pShared->nRef++;
			pFile->pShared = pShared;
			pFile->openFlags = openFlags;
		}
		sqlite3_mutex_leave(pShared->mutex);
		goto out;
	}

	n = strlen(zPath);
	zCopy = sqlite3_malloc(n + 1);
	pShared = sqlite3_malloc(sizeof(psp2Shared));
	mutex = sqlite3_mutex_alloc(SQLITE_MUTEX_FAST);
	if (zCopy == NULL || pShared == NULL || mutex == NULL) {
		if (mutex == NULL)
			PSP2_ERROR(426, "sqlite3_mutex_alloc() failed\n");
		else
			PSP2_ERROR(430, "no mem : orgpath=%s, newpath=0x%x, fid=0x%x\n",
				   zPath, zCopy, pShared);
		sqlite3_free(zCopy);
		sqlite3_free(pShared);
		sqlite3_mutex_free(mutex);
		rc = SQLITE_NOMEM;
	} else {
		memset(pShared, 0, sizeof(psp2Shared));
		memcpy(zCopy, zPath, n + 1);
		pShared->zPath = zCopy;
		pShared->openFlags = openFlags;
		pShared->mutex = mutex;
		pShared->nRef = 1;
		pShared->unkB = unkFlags;
		pFile->pShared = pShared;
	}
	if (rc != 0)
		goto out;

	pFile->pShared->isDelete = isDelete;
	pFile->openFlags = openFlags;

	/* a file created on savedata0: is created through AppMgr first */
	if ((openFlags & SCE_O_CREAT) &&
	    memcmp(pFile->pShared->zPath, PSP2_SAVEDATA_MOUNT,
		   PSP2_SAVEDATA_MOUNT_LEN) == 0) {
		if (pFile->openFlags & SCE_O_EXCL) {
			rc = -1;
			if (sceIoGetstat(pFile->pShared->zPath, &stat) ==
			    (int)SCE_ERROR_ERRNO_ENOENT)
				rc = 0;
		}
		if (rc == 0) {
			item = (SceAppUtilSaveDataDataSaveItem){
				.dataPath = pFile->pShared->zPath + PSP2_SAVEDATA_MOUNT_LEN,
				.mode = SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE,
			};
			memset(&param, 0, sizeof(param));
			param.size = sizeof(param);
			param.slotId = SCE_APPMGR_SAVEDATA_SLOT_NONE;
			param.slotParam = NULL;
			param.items = &item;
			param.itemNum = 1;
			strlcpy(param.mountPoint.data, PSP2_SAVEDATA_MOUNT, 15);
			param.unk44 = 1;
			rc = sceAppMgrSaveDataDataSave(&param);
			if (rc < 0)
				PSP2_ERROR(753, "sceAppMgrSaveDataDataSave(%s) returned %#x.\n",
					   pFile->pShared->zPath, rc);
		}
	}
	if (rc == 0) {
		rc = sceIoOpen(pFile->pShared->zPath, SCE_O_RDONLY, 0400);
		if (rc < 0)
			PSP2_ERROR(763, "sceIoOpen(%s) ret=0x%08x, flag=0x%x, mode=0x%x\n",
				   pFile->pShared->zPath, rc,
				   pFile->pShared->openFlags, 0600);
	}
	pShared = pFile->pShared;
	if (rc < 0) {
		psp2FreeShared(pShared);
		pFile->pShared = NULL;
		goto out;
	}
	pShared->fd = rc;
	pShared->pNext = g_sharedList;
	g_sharedList = pShared;
	rc = 0;

out:
	sqlite3_mutex_leave(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER));
	return rc;
}

/* take locktype for pFile: SHARED counts one more holder, EXCLUSIVE marks the file */
static void psp2SetLock(psp2File *pFile, psp2Shared *pShared, int locktype)
{
	if (locktype == SQLITE_LOCK_SHARED)
		pShared->nShared++;
	else if (locktype == SQLITE_LOCK_EXCLUSIVE)
		pShared->nShared = -1;
	pFile->locktype = locktype;
	if (pShared->eLock < locktype)
		pShared->eLock = locktype;
}

/* drop pFile's lock to locktype (NONE or SHARED) */
static void psp2ClearLock(psp2File *pFile, psp2Shared *pShared, int locktype)
{
	if (locktype == SQLITE_LOCK_NONE) {
		if (pShared->nShared > 1) {
			pShared->nShared--;
		} else {
			pShared->nShared = 0;
			pShared->eLock = SQLITE_LOCK_NONE;
		}
	} else if (locktype == SQLITE_LOCK_SHARED) {
		if (pFile->locktype == SQLITE_LOCK_EXCLUSIVE)
			pShared->nShared = 1;
		pShared->eLock = SQLITE_LOCK_SHARED;
	}
	pFile->locktype = locktype;
}

static __attribute__((noinline)) int psp2Unlock(sqlite3_file *id, int locktype)
{
	psp2File *pFile = (psp2File *)id;
	psp2Shared *pShared;

	sqlite3_mutex_enter(pFile->pShared->mutex);
	pShared = pFile->pShared;
	if (pFile->locktype != locktype && pShared->nShared != 0) {
		if (locktype == SQLITE_LOCK_NONE) {
			if (pShared->nShared > 1)
				psp2ClearLock(pFile, pShared, SQLITE_LOCK_NONE);
			else
				psp2ClearLock(pFile, pShared, SQLITE_LOCK_NONE);
		} else if (locktype == SQLITE_LOCK_SHARED) {
			psp2ClearLock(pFile, pShared, locktype);
		}
	}
	sqlite3_mutex_leave(pShared->mutex);
	return SQLITE_OK;
}

static int psp2Close(sqlite3_file *id)
{
	psp2File *pFile = (psp2File *)id;
	psp2Shared *pShared = pFile->pShared;
	psp2Shared *p, *prev;
	SceAppUtilSaveDataDataRemoveItem item;
	SceAppMgrSaveDataDataDelete param;
	int rc = SQLITE_OK;
	int last = 0;
	int ret;

	if (pShared == NULL) {
		PSP2_ERROR(901, "sceIoClose(): SQLITE_IOERR\n");
		return SQLITE_IOERR;
	}

	psp2Unlock(id, SQLITE_LOCK_NONE);
	sqlite3_mutex_enter(pShared->mutex);
	if (--pShared->nRef <= 0) {
		sqlite3_mutex_enter(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER));
		for (prev = NULL, p = g_sharedList; p != NULL; prev = p, p = p->pNext) {
			if (p == pShared) {
				if (prev != NULL)
					prev->pNext = p->pNext;
				else
					g_sharedList = p->pNext;
				break;
			}
		}
		sqlite3_mutex_leave(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MASTER));
		last = 1;
	}
	sqlite3_mutex_leave(pShared->mutex);

	if (last) {
		rc = sceIoClose(pShared->fd);
		if (rc < 0) {
			PSP2_ERROR(856, "sceIoClose(%s): SQLITE_IOERR ret=0x%08x\n",
				   pShared->zPath, rc);
			rc = SQLITE_IOERR;
		}
		if (pShared->isDelete) {
			item = (SceAppUtilSaveDataDataRemoveItem){
				.dataPath = pShared->zPath + PSP2_SAVEDATA_MOUNT_LEN,
				.mode = SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT,
			};
			memset(&param, 0, sizeof(param));
			param.size = sizeof(param);
			param.slotId = SCE_APPMGR_SAVEDATA_SLOT_NONE;
			param.slotParam = NULL;
			param.items = &item;
			param.itemNum = 1;
			strlcpy(param.mountPoint.data, PSP2_SAVEDATA_MOUNT, 15);
			ret = sceAppMgrSaveDataDataRemove(&param);
			if (ret < 0)
				PSP2_ERROR(878, "sceAppMgrSaveDataDataRemove(%s) ret=%#x\n",
					   pShared->zPath, ret);
		}
		psp2FreeShared(pShared);
		if (rc != SQLITE_OK)
			return rc;
	}
	pFile->pShared = NULL;
	return rc;
}

static int psp2Read(sqlite3_file *id, void *pBuf, int amt, sqlite3_int64 offset)
{
	psp2Shared *pShared = ((psp2File *)id)->pShared;
	int got;

	got = sceIoPread(pShared->fd, pBuf, amt, offset);
	if (got == amt)
		return SQLITE_OK;
	if (got < 0) {
		PSP2_ERROR(949, "sceIoPread(): ret=0x%x, path=%s, amt=%d, offset=0x%llx\n",
			   got, pShared->zPath, amt, offset);
		return SQLITE_IOERR_READ;
	}
	memset((char *)pBuf + got, 0, amt - got);
	return SQLITE_IOERR_SHORT_READ;
}

/* write one item of savedata0: data through AppMgr */
static int psp2Write(sqlite3_file *id, const void *pBuf, int amt,
		     sqlite3_int64 offset)
{
	psp2File *pFile = (psp2File *)id;
	SceAppUtilSaveDataDataSaveItem item;
	SceAppMgrSaveDataData param;
	int ret;

	if (!(pFile->openFlags & SCE_O_WRONLY) || pBuf == NULL)
		return SQLITE_IOERR_WRITE;

	item = (SceAppUtilSaveDataDataSaveItem){
		.dataPath = pFile->pShared->zPath + PSP2_SAVEDATA_MOUNT_LEN,
		.buf = pBuf,
		.bufSize = amt,
		.offset = offset,
		.mode = SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE,
	};
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.slotId = SCE_APPMGR_SAVEDATA_SLOT_NONE;
	param.slotParam = NULL;
	param.items = &item;
	param.itemNum = 1;
	strlcpy(param.mountPoint.data, PSP2_SAVEDATA_MOUNT, 15);
	param.unk44 = 1;
	ret = sceAppMgrSaveDataDataSave(&param);
	if (ret < 0)
		PSP2_ERROR(996, "sceAppMgrSaveDataDataSave(%s) returned %#x.\n",
			   pFile->pShared->zPath, ret);
	if (ret < 0)
		return psp2ErrorCode(ret, SQLITE_IOERR_WRITE);
	return SQLITE_OK;
}

static int psp2Truncate(sqlite3_file *id, sqlite3_int64 nByte)
{
	psp2File *pFile = (psp2File *)id;
	SceAppUtilSaveDataDataSaveItem item;
	SceAppMgrSaveDataData param;
	int ret;

	item = (SceAppUtilSaveDataDataSaveItem){
		.dataPath = pFile->pShared->zPath + PSP2_SAVEDATA_MOUNT_LEN,
		.buf = NULL,
		.bufSize = 0,
		.offset = nByte,
		.mode = SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE,
	};
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.slotId = SCE_APPMGR_SAVEDATA_SLOT_NONE;
	param.slotParam = NULL;
	param.items = &item;
	param.itemNum = 1;
	strlcpy(param.mountPoint.data, PSP2_SAVEDATA_MOUNT, 15);
	param.unk44 = 1;
	ret = sceAppMgrSaveDataDataSave(&param);
	if (ret < 0)
		PSP2_ERROR(1033, "sceAppMgrSaveDataDataSave(%s) returned %#x.\n",
			   pFile->pShared->zPath, ret);
	if (ret < 0) {
		PSP2_ERROR(1036, "sceIoChstat(): SQLITE_IOERR_TRUNCATE\n");
		return psp2ErrorCode(ret, SQLITE_IOERR_TRUNCATE);
	}
	return SQLITE_OK;
}

/* an empty save commits what was written */
static int psp2Sync(sqlite3_file *id, int flags)
{
	psp2File *pFile = (psp2File *)id;
	SceAppUtilSaveDataDataSaveItem item;
	SceAppMgrSaveDataData param;
	int ret;

	item = (SceAppUtilSaveDataDataSaveItem){
		.dataPath = pFile->pShared->zPath + PSP2_SAVEDATA_MOUNT_LEN,
	};
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.slotId = SCE_APPMGR_SAVEDATA_SLOT_NONE;
	param.slotParam = NULL;
	param.items = &item;
	param.itemNum = 1;
	strlcpy(param.mountPoint.data, PSP2_SAVEDATA_MOUNT, 15);
	param.unk44 = 0;
	ret = sceAppMgrSaveDataDataSave(&param);
	if (ret < 0)
		PSP2_ERROR(1069, "sceAppMgrSaveDataDataSave(%s) returned %#x.\n",
			   pFile->pShared->zPath, ret);
	if (ret == 0)
		return SQLITE_OK;
	return psp2ErrorCode(ret, SQLITE_IOERR_FSYNC);
}

static int psp2FileSize(sqlite3_file *id, sqlite3_int64 *pSize)
{
	psp2Shared *pShared = ((psp2File *)id)->pShared;
	int rc = SQLITE_OK;

	*pSize = sceIoLseek(pShared->fd, 0, SCE_SEEK_END);
	if (*pSize < 0) {
		PSP2_ERROR(1089, "sceIoLseek(%s): ret=0x%016llx, SQLITE_IOERR_FSTAT\n",
			   pShared->zPath, *pSize);
		rc = SQLITE_IOERR_FSTAT;
	}
	return rc;
}

static int psp2Lock(sqlite3_file *id, int locktype)
{
	psp2File *pFile = (psp2File *)id;
	psp2Shared *pShared;
	int rc = SQLITE_OK;
	int cur;

	sqlite3_mutex_enter(pFile->pShared->mutex);
	cur = pFile->locktype;
	pShared = pFile->pShared;
	if (cur >= locktype)
		goto out;

	if (locktype > SQLITE_LOCK_NONE && cur < SQLITE_LOCK_SHARED) {
		if (pShared->eLock > SQLITE_LOCK_SHARED || pShared->nShared < 0) {
			rc = SQLITE_BUSY;
			goto out;
		}
		cur = SQLITE_LOCK_SHARED;
		psp2SetLock(pFile, pShared, SQLITE_LOCK_SHARED);
	}
	if (locktype >= SQLITE_LOCK_RESERVED && cur < SQLITE_LOCK_RESERVED) {
		if (pShared->eLock >= SQLITE_LOCK_RESERVED) {
			rc = SQLITE_BUSY;
			goto out;
		}
		cur = SQLITE_LOCK_RESERVED;
		psp2SetLock(pFile, pShared, SQLITE_LOCK_RESERVED);
	}
	if (locktype == SQLITE_LOCK_EXCLUSIVE) {
		if (pShared->nShared == 0 ||
		    (pShared->nShared == 1 && cur > SQLITE_LOCK_SHARED &&
		     cur < SQLITE_LOCK_EXCLUSIVE)) {
			psp2SetLock(pFile, pShared, locktype);
		} else {
			rc = SQLITE_BUSY;
			if (pShared->eLock < SQLITE_LOCK_PENDING)
				psp2SetLock(pFile, pShared, SQLITE_LOCK_PENDING);
		}
	}
out:
	sqlite3_mutex_leave(pShared->mutex);
	return rc;
}

static int psp2CheckReservedLock(sqlite3_file *id, int *pResOut)
{
	psp2File *pFile = (psp2File *)id;
	psp2Shared *pShared;
	int reserved = 0;

	sqlite3_mutex_enter(pFile->pShared->mutex);
	pShared = pFile->pShared;
	if (pShared->nShared < 0 || pShared->eLock >= SQLITE_LOCK_RESERVED)
		reserved = 1;
	sqlite3_mutex_leave(pShared->mutex);
	*pResOut = reserved;
	return SQLITE_OK;
}

static int psp2FileControl(sqlite3_file *id, int op, void *pArg)
{
	psp2File *pFile = (psp2File *)id;
	sqlite3_mutex *mutex;

	switch (op) {
	case SQLITE_FCNTL_LOCKSTATE:
		sqlite3_mutex_enter(pFile->pShared->mutex);
		mutex = pFile->pShared->mutex;
		*(int *)pArg = pFile->locktype;
		sqlite3_mutex_leave(mutex);
		return SQLITE_OK;
	case SQLITE_LAST_ERRNO:
		return SQLITE_OK;
	}
	return SQLITE_ERROR;
}

static int psp2SectorSize(sqlite3_file *id)
{
	return PSP2_SECTOR_SIZE;
}

static int psp2DeviceCharacteristics(sqlite3_file *id)
{
	return 0;
}

/*
 * Open zName (NULL: a new temporary file in sqlite3_temp_directory).  The
 * name is normalised first: "//" and a '/' after the device colon collapse,
 * "/../" removes the previous component.
 */
static int psp2Open(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *id,
		    int flags, int *pOutFlags)
{
	static const char zChars[] =
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789";
	psp2File *pFile = (psp2File *)id;
	SceIoStat stat;
	char zPath[PSP2_MAX_PATHNAME + 1];
	int openFlags = 0;
	char isDelete = 0;
	char unkFlags = 0;
	int isReadWrite;
	int i, j, k;
	int rc;

	if (zName != NULL) {
		/* the device name, up to and including its colon */
		for (i = 0; zName[i] != '\0' && zName[i] != ':'; i++)
			zPath[i] = zName[i];
		j = i;
		if (zName[i] == ':') {
			zPath[i] = zName[i];
			j = ++i;
		}
		while (i > 0 && i < (int)sizeof(zPath) && j > 0 &&
		       j < (int)sizeof(zPath) && zName[i] != '\0') {
			if (zPath[j - 1] == ':' && zName[i] == '/') {
				i++;
			} else if (zName[i] == '/' && zName[i + 1] == '/') {
				i++;
			} else if (memcmp(&zName[i], "/../", 4) == 0) {
				i += 3;
				while (j > 0 && zPath[j - 1] != '/' &&
				       zPath[j - 1] != ':')
					j--;
			} else {
				zPath[j++] = zName[i++];
			}
		}
		zPath[j] = 0;
	} else {
		if (sqlite3_temp_directory == NULL) {
			printf("[sqlite] sqlite3_temp_directory is not set.\n");
			return SQLITE_ERROR;
		}
		sqlite3_snprintf(sizeof(zPath), zPath, "%s/etilqs_",
				 sqlite3_temp_directory);
		j = strlen(zPath);
		do {
			sqlite3_randomness(15, &zPath[j]);
			for (k = 0; k < 15; k++, j++)
				zPath[j] = zChars[(unsigned char)zPath[j] %
						  (sizeof(zChars) - 1)];
			zPath[j] = 0;
		} while (sceIoGetstat(zPath, &stat) == 0);
	}

	pFile->pMethod = NULL;
	pFile->pShared = NULL;
	pFile->locktype = SQLITE_LOCK_NONE;
	pFile->openFlags = 0;

	isReadWrite = flags & SQLITE_OPEN_READWRITE;
	if (isReadWrite)
		openFlags = SCE_O_RDWR;
	if (flags & SQLITE_OPEN_READONLY)
		openFlags |= SCE_O_RDONLY;
	if (flags & SQLITE_OPEN_CREATE)
		openFlags |= SCE_O_CREAT;
	if (flags & SQLITE_OPEN_EXCLUSIVE)
		openFlags |= SCE_O_EXCL;
	if (flags & SQLITE_OPEN_DELETEONCLOSE)
		isDelete = 1;

	rc = psp2OpenShared(pFile, zPath, openFlags, isDelete, unkFlags);
	if (rc == 0) {
		if (pOutFlags != NULL)
			*pOutFlags = flags;
		pFile->pMethod = &psp2IoMethods;
		return SQLITE_OK;
	}
	/* a file that cannot be opened for writing is opened read-only */
	if (rc < 0 && isReadWrite)
		rc = psp2Open(pVfs, zName, id,
			      (flags & ~SQLITE_OPEN_READWRITE) | SQLITE_OPEN_READONLY,
			      pOutFlags);
	else
		rc = psp2ErrorCode(rc, SQLITE_CANTOPEN);
	return rc;
}

static int psp2Delete(sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
	SceAppUtilSaveDataDataRemoveItem item;
	SceAppMgrSaveDataDataDelete param;
	int ret;

	item = (SceAppUtilSaveDataDataRemoveItem){
		.dataPath = zPath + PSP2_SAVEDATA_MOUNT_LEN,
		.mode = SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT,
	};
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.slotId = SCE_APPMGR_SAVEDATA_SLOT_NONE;
	param.slotParam = NULL;
	param.items = &item;
	param.itemNum = 1;
	strlcpy(param.mountPoint.data, PSP2_SAVEDATA_MOUNT, 15);
	ret = sceAppMgrSaveDataDataRemove(&param);
	if (ret < 0)
		PSP2_ERROR(1523, "sceAppMgrSaveDataDataRemove(%s) ret=%#x\n",
			   zPath, ret);
	if (ret < 0)
		ret = psp2ErrorCode(ret, SQLITE_IOERR_DELETE);
	return ret;
}

static int psp2Access(sqlite3_vfs *pVfs, const char *zPath, int flags,
		      int *pResOut)
{
	SceIoStat stat = {0};
	int ret;

	ret = sceIoGetstat(zPath, &stat);
	if (ret == (int)SCE_ERROR_ERRNO_ENOENT) {
		*pResOut = 0;
		return SQLITE_OK;
	}
	if (ret < 0) {
		*pResOut = 0;
		PSP2_ERROR(1553, "sceIoGetstat(%s): ret=0x%08x, SQLITE_IOERR_ACCESS\n",
			   zPath, ret);
		return SQLITE_IOERR_ACCESS;
	}

	switch (flags) {
	case SQLITE_ACCESS_EXISTS:
	case SQLITE_ACCESS_READ:
		*pResOut = stat.st_mode & SCE_S_IRUSR;
		break;
	case SQLITE_ACCESS_READWRITE:
		*pResOut = stat.st_mode & (SCE_S_IRUSR | SCE_S_IWUSR);
		break;
	}
	return SQLITE_OK;
}

static int psp2FullPathname(sqlite3_vfs *pVfs, const char *zPath, int nOut,
			    char *zOut)
{
	sqlite3_snprintf(nOut, zOut, "%s", zPath);
	return SQLITE_OK;
}

/* no loadable extensions */
static void *psp2DlOpen(sqlite3_vfs *pVfs, const char *zFilename)
{
	return NULL;
}

static void psp2DlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg)
{
}

static void (*psp2DlSym(sqlite3_vfs *pVfs, void *pHandle, const char *zSymbol))(void)
{
	return NULL;
}

static void psp2DlClose(sqlite3_vfs *pVfs, void *pHandle)
{
}

static int psp2GetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf)
{
	return 0;
}
