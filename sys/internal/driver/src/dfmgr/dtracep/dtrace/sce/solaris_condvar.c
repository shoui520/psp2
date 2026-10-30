/*
 * Solaris condition variables (Sony's dtrace/sce/solaris_condvar.c), as thread
 * manager condition variables on the kernel mutex they are used with.
 */
#include <dtrace/logging.h>
#include <dtrace/sce/solaris_condvar.h>
#include <dtrace/sce/solaris_hrtime.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>

#define CV_NAME_PREFIX	"SceDeci4pDtracep"

static SceKernelCondInfo s_condInfo;

/* the name of a condition variable, for the log: its thread manager name, or its UID */
static inline const char *cv_name(SceUID uid)
{
	s_condInfo = (SceKernelCondInfo){ .size = sizeof(SceKernelCondInfo) };
	if (SceThreadmgrForDriver_D1FBDFA6(uid, &s_condInfo) != SCE_OK ||
	    s_condInfo.name[0] != '\0')
		snprintf(s_condInfo.name, sizeof(s_condInfo.name) - 1, "0x%x", uid);

	return s_condInfo.name;
}

void cv_init(kcondvar_t *cvp, kmutex_t *mp, const char *name, kcv_type_t type, void *arg)
{
	char cvName[256];

	SCE_DBG_ASSERT(cvp != NULL, 38);
	SCE_DBG_ASSERT(mp != NULL, 39);
	SCE_DBG_ASSERT(mp->kind == DTRACE_MUTEX_KERNEL, 40);

	if (name != NULL && name[0] != '\0')
		snprintf(cvName, sizeof(cvName), CV_NAME_PREFIX "%s", name);
	else
		__builtin_strcpy(cvName, CV_NAME_PREFIX);

	cvp->mutex = mp;
	cvp->uid = sceKernelCreateCond(cvName, 0, mp->uid, NULL);
	if (cvp->uid < 0)
		DTRACE_LOG_ERROR_AT(57, "failed to init '%s' condvar (0x%x)", cvName, cvp->uid);
}

void cv_destroy(kcondvar_t *cvp)
{
	const char *name;
	int res;

	SCE_DBG_ASSERT(cvp != NULL, 65);
	SCE_DBG_ASSERT(cvp->uid >= 0, 66);

	name = cv_name(cvp->uid);
	res = sceKernelDeleteCond(cvp->uid);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(73, "failed to destroy '%s' condvar (0x%x)", name, res);

	cvp->mutex = NULL;
	cvp->uid = -1;
}

void cv_wait(kcondvar_t *cvp, kmutex_t *mp)
{
	int res;

	SCE_DBG_ASSERT(cvp != NULL, 85);
	SCE_DBG_ASSERT(mp != NULL, 86);
	SCE_DBG_ASSERT(cvp->mutex->uid == mp->uid, 87);
	SCE_DBG_ASSERT(cvp->uid >= 0, 88);

	res = sceKernelWaitCond(cvp->uid, NULL);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(92, "failed to wait for '%s' condvar (0x%x)",
				    cv_name(cvp->uid), res);
}

void cv_signal(kcondvar_t *cvp)
{
	int res;

	SCE_DBG_ASSERT(cvp != NULL, 100);
	SCE_DBG_ASSERT(cvp->uid >= 0, 101);

	res = sceKernelSignalCond(cvp->uid);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(105, "failed to signal '%s' condvar (0x%x)",
				    cv_name(cvp->uid), res);
}

void cv_broadcast(kcondvar_t *cvp)
{
	int res;

	SCE_DBG_ASSERT(cvp != NULL, 113);
	SCE_DBG_ASSERT(cvp->uid >= 0, 114);

	res = sceKernelSignalCondAll(cvp->uid);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(118, "failed to broadcast signal '%s' condvar (0x%x)",
				    cv_name(cvp->uid), res);
}

clock_t cv_timedwait(kcondvar_t *cvp, kmutex_t *mp, clock_t tim)
{
	SceUInt32 timeout;
	int res;

	SCE_DBG_ASSERT(cvp != NULL, 127);
	SCE_DBG_ASSERT(mp != NULL, 128);
	SCE_DBG_ASSERT(cvp->mutex->uid == mp->uid, 129);
	SCE_DBG_ASSERT(cvp->uid >= 0, 130);

	timeout = (SceUInt32)tim / (SceUInt32)hz * 1000000;
	res = sceKernelWaitCond(cvp->uid, &timeout);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(136, "failed to wait less than %dus for '%s' condvar (0x%x)",
				    timeout, cv_name(cvp->uid), res);
		return -1;
	}

	return hz * timeout / 1000000;
}
