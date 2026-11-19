/*
 * The USB monitor ("UDCD_MON:"): a thread that keeps a SceUdcd callback registered while
 * the responder runs over USB and notes when the controller is deactivated, which the
 * core reads as the cable having been pulled.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <udcd/udcd_user.h>
#include "mtp_internal.h"

#define MTP_TAG_UDCD_MON	"UDCD_MON:"

#define UDCD_MONITOR_THREAD_PRIORITY	0x10000100
#define UDCD_MONITOR_THREAD_STACK	0x1000
#define UDCD_MONITOR_THREAD_AFFINITY	0x70000

static int s_created;
static int s_started;
/* the controller was deactivated since the last look, under s_stateMutex */
static int s_disconnected;
/* signalled to end the monitoring thread */
static SceUID s_connectSema = -1;
static SceUID s_monitorThread = -1;
static SceUID s_stateMutex = -1;

static int udcdMonitoringThread(SceSize args, void *argp);

int mtpUsbMonitorIsCableConnected(void)
{
	UdcdUserDeviceState state;

	mtpUtilMemset(&state, 0, sizeof(state));
	if (sceUdcdGetDeviceState(&state) < 0)
		return 0;
	return (state.cable & (SCE_UDCD_STATUS_CABLE_DISCONNECTED |
			       SCE_UDCD_STATUS_CABLE_CONNECTED)) == SCE_UDCD_STATUS_CABLE_CONNECTED;
}

int mtpUsbMonitorCreate(void)
{
	int res = 0;
	SceUID id;

	if (s_created) {
		MTP_ERR(MTP_TAG_UDCD_MON, 56, "mtpUsbMonitor is already created\n");
		res = -1;
		goto error;
	}

	id = sceKernelCreateMutex("SceMtpUdcdState", 0, 0, NULL);
	if (id < 0) {
		MTP_ERR(MTP_TAG_UDCD_MON, 63, "sceKernelCreateMutex(); res = 0x%08x\n", id);
		res = id;
		goto error;
	}
	s_stateMutex = id;

	id = sceKernelCreateSema("SceMtpUdcdConnect", 0, 0, 1, NULL);
	if (id < 0) {
		MTP_ERR(MTP_TAG_UDCD_MON, 71, "sceKernelCreateSema(); res = 0x%08x\n", id);
		res = id;
		goto error;
	}
	s_connectSema = id;

	id = sceKernelCreateThread("SceMtpUdcdMonitoring", udcdMonitoringThread,
				   UDCD_MONITOR_THREAD_PRIORITY, UDCD_MONITOR_THREAD_STACK, 0,
				   UDCD_MONITOR_THREAD_AFFINITY, NULL);
	if (id < 0) {
		MTP_ERR(MTP_TAG_UDCD_MON, 82, "sceKernelCreateThread(); res = 0x%08x\n", id);
		res = id;
		goto error;
	}
	s_monitorThread = id;
	s_created = 1;
	return res;

error:
	if (res >= 0)
		return res;
	if (s_stateMutex != -1) {
		int ret = sceKernelDeleteMutex(s_stateMutex);

		if (ret < 0)
			MTP_ERR(MTP_TAG_UDCD_MON, 94, "sceKernelDeleteMutex(); res = 0x%08x\n", ret);
		s_stateMutex = -1;
	}
	if (s_connectSema != -1) {
		int ret = sceKernelDeleteSema(s_connectSema);

		if (ret < 0)
			MTP_ERR(MTP_TAG_UDCD_MON, 102, "sceKernelDeleteSema(); res = 0x%08x\n", ret);
		s_connectSema = -1;
	}
	return res;
}

int mtpUsbMonitorDelete(void)
{
	int res = 0;
	int ret;

	if (!s_created) {
		MTP_ERR(MTP_TAG_UDCD_MON, 120, "mtp udcd monitor is not created\n");
		return -1;
	}
	if (s_started) {
		MTP_ERR(MTP_TAG_UDCD_MON, 125, "mtp udcd monitor is not stopped\n");
		return -1;
	}
	s_created = 0;

	if (s_monitorThread != -1) {
		ret = sceKernelDeleteThread(s_monitorThread);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_UDCD_MON, 134, "sceKernelDeleteThread(); res = 0x%08x\n", ret);
			res = -1;
		}
		s_monitorThread = -1;
	}
	if (s_stateMutex != -1) {
		ret = sceKernelDeleteMutex(s_stateMutex);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_UDCD_MON, 143, "sceKernelDeleteMutex(); res = 0x%08x\n", ret);
			res = -1;
		}
		s_stateMutex = -1;
	}
	if (s_connectSema != -1) {
		ret = sceKernelDeleteSema(s_connectSema);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_UDCD_MON, 152, "sceKernelDeleteSema(); res = 0x%08x\n", ret);
			res = -1;
		}
		s_connectSema = -1;
	}
	return res;
}

int mtpUsbMonitorIsStarted(void)
{
	return s_started;
}

int mtpUsbMonitorStart(void)
{
	int ret;

	if (!s_created) {
		MTP_ERR(MTP_TAG_UDCD_MON, 174, "mtp udcd monitor is not created\n");
		return -1;
	}
	if (s_started) {
		MTP_ERR(MTP_TAG_UDCD_MON, 179, "mtp udcd monitor is already started\n");
		return -1;
	}
	ret = sceKernelStartThread(s_monitorThread, 0, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UDCD_MON, 185, "sceKernelStartThread(); res = 0x%08x\n", ret);
		return ret;
	}
	s_started = 1;
	return 0;
}

int mtpUsbMonitorStop(void)
{
	int ret;

	if (!s_created) {
		MTP_ERR(MTP_TAG_UDCD_MON, 202, "mtp udcd monitor is not created\n");
		return -1;
	}
	if (!s_started) {
		MTP_ERR(MTP_TAG_UDCD_MON, 207, "mtp udcd monitor is not started\n");
		return -1;
	}
	s_started = 0;
	ret = sceKernelSignalSema(s_connectSema, 1);
	if (ret < 0)
		MTP_ERR(MTP_TAG_UDCD_MON, 215, "sceKernelSignalSema(); res = 0x%08x\n", ret);
	ret = sceKernelWaitThreadEnd(s_monitorThread, NULL, NULL);
	if (ret < 0)
		MTP_ERR(MTP_TAG_UDCD_MON, 220, "sceKernelWaitThreadEnd(); res = 0x%08x\n", ret);
	return 0;
}

static inline void udcdMutexLock(void)
{
	int ret = sceKernelLockMutex(s_stateMutex, 1, NULL);

	if (ret < 0)
		MTP_ERR(MTP_TAG_UDCD_MON, 253, "sceKernelLockMutex(); res = 0x%08x\n", ret);
}

static inline void udcdMutexUnlock(void)
{
	int ret = sceKernelUnlockMutex(s_stateMutex, 1);

	if (ret < 0)
		MTP_ERR(MTP_TAG_UDCD_MON, 262, "sceKernelUnlockMutex(); res = 0x%08x\n", ret);
}

int mtpUsbMonitorHasBeenDisconnected(void)
{
	int disconnected = 0;

	if (!s_created) {
		MTP_ERR(MTP_TAG_UDCD_MON, 234, "mtp udcd monitor is not created\n");
		return 0;
	}
	udcdMutexLock();
	if (s_disconnected) {
		s_disconnected = 0;
		disconnected = 1;
	}
	udcdMutexUnlock();
	return disconnected;
}

static int udcdCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	udcdMutexLock();
	switch (notifyArg) {
	case SCE_UDCD_STATUS_DEACTIVATED:
		s_disconnected = 1;
		break;
	case SCE_UDCD_STATUS_ACTIVATED:
		break;
	default:
		break;
	}
	udcdMutexUnlock();
	return 0;
}

static int udcdMonitoringThread(SceSize args, void *argp)
{
	SceUID callback;
	int ret;

	s_disconnected = 0;
	callback = sceKernelCreateCallback("SceMtpUdcdCallback", 0, udcdCallback, NULL);
	if (callback < 0) {
		MTP_ERR(MTP_TAG_UDCD_MON, 298, "sceKernelCreateCallback(); res = 0x%08x\n",
			callback);
		SceThreadmgrCoredumpTime_sceKernelExitThread(-1);
		return 0;
	}
	ret = sceUdcdRegisterCallback(callback,
				      SCE_UDCD_STATUS_DEACTIVATED | SCE_UDCD_STATUS_ACTIVATED);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_UDCD_MON, 306, "sceUdcdRegisterCallback(); res = 0x%08x\n", ret);
		SceThreadmgrCoredumpTime_sceKernelExitThread(-1);
		return 0;
	}
	ret = sceKernelWaitSemaCB(s_connectSema, 1, NULL);
	if (ret < 0)
		MTP_ERR(MTP_TAG_UDCD_MON, 314, "sceKernelWaitSemaCB(); res = 0x%08x\n", ret);
	ret = sceUdcdUnregisterCallback();
	if (ret < 0)
		MTP_ERR(MTP_TAG_UDCD_MON, 320, "sceUdcdUnregisterCallback(); res = 0x%08x\n",
			ret);
	ret = sceKernelDeleteCallback(callback);
	if (ret < 0)
		MTP_ERR(MTP_TAG_UDCD_MON, 325, "sceKernelDeleteCallback(); res = 0x%08x\n", ret);
	s_disconnected = 0;
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}
