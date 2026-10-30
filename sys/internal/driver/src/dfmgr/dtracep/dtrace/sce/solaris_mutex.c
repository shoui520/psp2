/*
 * Solaris kernel mutexes (Sony's dtrace/sce/solaris_mutex.c): an adaptive
 * mutex is a thread manager mutex, a spin mutex a spinlock with its owner's
 * thread ID.
 */
#include <dtrace/logging.h>
#include <dtrace/sce/solaris_mutex.h>
#include <dtrace/utility.h>
#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr.h>
#include <sceerror.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadcb.h>
#include <threadmgr/mutex.h>
#include <threadmgr/threadmgr.h>

#define MUTEX_NAME_PREFIX	"SceDeci4pDtracep"

#define UID_PUID		0x40000000	/* a process's own UID for an object */

void mutex_name(kmutex_t *mp, char *name)
{
	SCE_DBG_ASSERT(mp != NULL, 19);

	if (mp->kind == DTRACE_MUTEX_KERNEL) {
		SceUID uid = mp->uid;
		SceKernelMutexInfo info;
		int res;

		info = (SceKernelMutexInfo){ .size = sizeof(info) };
		res = sceKernelGetMutexInfo(uid, &info);
		if (res != SCE_OK) {
			DTRACE_LOG_ERROR_AT(33, "failed to get info for mutex 0x%08x (0x%08x)", uid, res);
			snprintf(name, sizeof(mp->name), "0x%x", uid);
		} else if (info.name[0] != '\0') {
			snprintf(name, sizeof(mp->name), "%s", info.name);
		} else {
			snprintf(name, sizeof(mp->name), "0x%x", uid);
		}
	} else if (mp->kind == DTRACE_MUTEX_SPIN) {
		snprintf(name, sizeof(mp->name), "%s", mp->name);
	} else {
		SCE_DBG_ASSERT(0, 44);
	}
	name[sizeof(mp->name) - 1] = '\0';
}

/* the thread holding a mutex, or a negative error */
static inline SceUID mutex_owner(kmutex_t *mp)
{
	SceUID owner;

	SCE_DBG_ASSERT(mp != NULL, 52);

	if (mp->kind == DTRACE_MUTEX_KERNEL) {
		SceKernelMutexInfo info;

		info = (SceKernelMutexInfo){ .size = sizeof(info) };
		owner = sceKernelGetMutexInfo(mp->uid, &info);
		if (owner == SCE_OK) {
			owner = info.currentOwnerId;
			if (owner & UID_PUID) {
				owner = scePUIDtoGUID(dtrace_get_current_process_id(), owner);
				if (owner < 0)
					DTRACE_LOG_ERROR_AT(72, "failed to convert '%s' mutex owner PUID 0x%08x to GUID (0x%08x)",
							    info.name, info.currentOwnerId, owner);
			}
		}
	} else if (mp->kind == DTRACE_MUTEX_SPIN) {
		owner = mp->owner;
	} else {
		SCE_DBG_ASSERT(0, 84);
		owner = -1;
	}
	return owner;
}

void mutex_init(kmutex_t *mp, const char *name, kmutex_type_t type, void *arg)
{
	char mutexName[256];

	SCE_DBG_ASSERT(mp != NULL, 99);
	memset(mp, 0, sizeof(*mp));

	if (name != NULL && name[0] != '\0')
		snprintf(mutexName, sizeof(mutexName), MUTEX_NAME_PREFIX "%s", name);
	else
		__builtin_strcpy(mutexName, MUTEX_NAME_PREFIX);

	if (type == MUTEX_ADAPTIVE || type == MUTEX_DEFAULT) {
		mp->kind = DTRACE_MUTEX_KERNEL;
		mp->uid = sceKernelCreateMutex(mutexName, 0, 0, NULL);
		if (mp->uid < 0)
			DTRACE_LOG_ERROR_AT(117, "failed to init '%s' mutex (0x%x)", mutexName, mp->uid);
	} else if (type == MUTEX_SPIN || type == MUTEX_DRIVER) {
		mp->kind = DTRACE_MUTEX_SPIN;
		snprintf(mp->name, sizeof(mp->name) - 1, "%s", mutexName);
		mp->lock = 0;
		mp->owner = -1;
	} else {
		SCE_DBG_ASSERT(0, 128);
	}
}

void mutex_destroy(kmutex_t *mp)
{
	char name[32];
	int res;

	SCE_DBG_ASSERT(mp != NULL, 134);

	if (mp->kind == DTRACE_MUTEX_KERNEL) {
		SCE_DBG_ASSERT(mp->uid >= 0, 137);
		res = sceKernelDeleteMutex(mp->uid);
		if (res < 0) {
			mutex_name(mp, name);
			DTRACE_LOG_ERROR_AT(143, "failed to destroy '%s' mutex (0x%x)", name, res);
		}
		memset(mp, 0, sizeof(*mp));
		mp->uid = -1;
	} else if (mp->kind == DTRACE_MUTEX_SPIN) {
		memset(mp, 0, sizeof(*mp));
	} else {
		SCE_DBG_ASSERT(0, 151);
	}
}

void mutex_enter(kmutex_t *mp)
{
	char name[32];
	SceUID thid;
	int res;

	SCE_DBG_ASSERT(mp != NULL, 157);

	if (mp->kind == DTRACE_MUTEX_KERNEL) {
		SCE_DBG_ASSERT(mp->uid >= 0, 160);
		res = sceKernelLockMutex(mp->uid, 1, NULL);
		if (res < 0) {
			mutex_name(mp, name);
			DTRACE_LOG_ERROR_AT(172, "failed to enter '%s' mutex (0x%x) - pid:0x%08x tid:0x%08x intr:%d excp:%d thr:%d",
					    name, res, sceKernelGetProcessId(), sceKernelGetThreadId(),
					    sceKernelIsIntrContext(), SceExcpmgrForKernel_B615A7DA(),
					    getCurrentThreadCB() != NULL);
		}
	} else if (mp->kind == DTRACE_MUTEX_SPIN) {
		thid = dtrace_get_current_thread_id();
		SCE_DBG_ASSERT(thid >= 0, 177);
		if (mp->owner == thid)
			DTRACE_LOG_ERROR_AT(180, "mutex '%s' can not be acquired recursively by thread 0x%08x",
					    mp->name, thid);
		SCE_DBG_ASSERT(mp->owner != thid, 182);

		sceKernelSpinlockLowLock(&mp->lock);
		if (mp->owner != -1)
			DTRACE_LOG_ERROR_AT(187, "mutex '%s' cannot be acquired by thread 0x%08x - already owned by thread 0x%08x",
					    mp->name, thid, mp->owner);
		SCE_DBG_ASSERT(mp->owner == -1, 189);
		mp->owner = thid;
	} else {
		SCE_DBG_ASSERT(0, 193);
	}
}

void mutex_exit(kmutex_t *mp)
{
	char name[32];
	SceUID thid;
	int res;

	SCE_DBG_ASSERT(mp != NULL, 199);

	if (mp->kind == DTRACE_MUTEX_KERNEL) {
		SCE_DBG_ASSERT(mp->uid >= 0, 202);
		res = sceKernelUnlockMutex(mp->uid, 1);
		if (res < 0) {
			mutex_name(mp, name);
			DTRACE_LOG_ERROR_AT(214, "failed to exit '%s' mutex (0x%x) - pid:0x%08x tid:0x%08x intr:%d excp:%d thr:%d",
					    name, res, sceKernelGetProcessId(), sceKernelGetThreadId(),
					    sceKernelIsIntrContext(), SceExcpmgrForKernel_B615A7DA(),
					    getCurrentThreadCB() != NULL);
		}
	} else if (mp->kind == DTRACE_MUTEX_SPIN) {
		thid = dtrace_get_current_thread_id();
		SCE_DBG_ASSERT(thid >= 0, 219);
		if (mp->owner != thid)
			DTRACE_LOG_ERROR_AT(222, "mutex '%s' cannot be released by thread 0x%08x - owned by thread 0x%08x",
					    mp->name, thid, mp->owner);
		SCE_DBG_ASSERT(mp->owner == thid, 224);
		mp->owner = -1;
		sceKernelSpinlockLowUnlock(&mp->lock);
	} else {
		SCE_DBG_ASSERT(0, 230);
	}
}

int mutex_owned(kmutex_t *mp)
{
	char name[32];
	SceUID thid, owner;

	SCE_DBG_ASSERT(mp != NULL, 239);

	if (mp->kind == DTRACE_MUTEX_KERNEL) {
		SCE_DBG_ASSERT(mp->uid >= 0, 242);
		thid = dtrace_get_current_thread_id();
		if (thid < -1)
			return SCE_ERROR_ERRNO_ENOENT;

		owner = mutex_owner(mp);
		if (owner < 0) {
			mutex_name(mp, name);
			return SCE_ERROR_ERRNO_ENOENT;
		}
		return owner == thid;
	} else if (mp->kind == DTRACE_MUTEX_SPIN) {
		thid = dtrace_get_current_thread_id();
		SCE_DBG_ASSERT(thid >= 0, 261);
		return mp->owner == thid;
	}

	SCE_DBG_ASSERT(0, 265);
	return SCE_ERROR_ERRNO_EINVAL;
}
