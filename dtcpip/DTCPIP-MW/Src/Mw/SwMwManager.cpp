/*
 * The DTCP manager object (SwMwManager.cpp): its session lifecycle, the keybin and record
 * setup behind the manager dispatch's commands, and the checked status helper they share.
 * This file starts with the cancel path and the checked status; the session start/stop and
 * the record setup follow.
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwManager.h"

#define SWMW_MANAGER_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwManager.cpp"

extern "C" {
int swMwTrace(const char *format, ...);

/* the activation and download cancels (dtcpip_activate.c / dtcpip_download.c) */
int _sceDtcpIpCancelActivation(void);
int _sceDtcpIpCancelDownload(void);
int _sceDtcpIpCheckActivation(int *activated);

/* the thread helpers (SwMwCommServer.cpp) */
int SwsGetToolMode(void);
int SwsGetAffinity(void);
int SwMwThreadTrampoline(SceSize args, void *argument);

/* the activation's worker entry (0x810056a6) and the calls it makes */
int SwMwManagerActivationThread(void *argument);
int SwMwCoreGetPort(unsigned short *port);
int SwMwCoreImportKeyBin(const void *data, int size);
int _sceDtcpIpDownload(unsigned short port, void *buffer, int *size);
int _sceDtcpIpActivate(int *result);
int SwMwManagerParamTblThread(void *argument);
}

extern "C" int SwMwManagerThreadCreate(struct SwMwManagerThread *thread, int (*entry)(void *),
				void *argument, int type)
{
	char name[20];
	SceSize stackSize;
	int priority;
	int affinity;
	int id;
	struct SwMwThreadAttr {
		int reserved[3];
		int priority;
	} attr;

	thread->entry = entry;
	thread->argument = argument;
	stackSize = 0x4000;
	memcpy(name, "SWS_THREAD_NO_NAME", 0x13);
	name[19] = 0;
	attr.reserved[0] = 0;
	attr.reserved[1] = 0;
	attr.reserved[2] = 0;
	attr.priority = 111;
	if (SwsGetToolMode() == 0)
		attr.priority = 0x10000100;
	priority = attr.priority;
	affinity = 0;
	switch (type) {
	case 0:
		strncpy(name, "SWS_THREAD_TYPE_ANONYMOUS", 0x1F);
		break;
	case 1:
		strncpy(name, "SWS_THREAD_TYPE_AKE_CMD_TIMER", 0x1F);
		break;
	case 2:
		strncpy(name, "SWS_THREAD_TYPE_KX_EXP", 0x1F);
		break;
	case 3:
		strncpy(name, "SWS_THREAD_TYPE_SINK_AUTH", 0x1F);
		stackSize = 0x8000;
		break;
	case 4:
		strncpy(name, "SWS_THREAD_TYPE_SINK_AKE_TIMER", 0x1F);
		break;
	case 5:
		strncpy(name, "SWS_THREAD_TYPE_SINK_CKC", 0x1F);
		break;
	case 6:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AUTH", 0x1F);
		stackSize = 0x8000;
		break;
	case 7:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AUTH_SERVER", 0x1F);
		stackSize = 0x8000;
		break;
	case 8:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AKE_TIMER", 0x1F);
		stackSize = 0x8000;
		break;
	case 9:
		strncpy(name, "SWS_THREAD_TYPE_RTT_REG", 0x1F);
		stackSize = 0x8000;
		break;
	case 10:
		strncpy(name, "SWS_THREAD_TYPE_TTL_CHECK", 0x1F);
		break;
	case 11:
		strncpy(name, "SWS_THREAD_TYPE_MEDIA_DISCONNECT", 0x1F);
		break;
	case 16:
		strncpy(name, "SWS_THREAD_TYPE_SW_TIMER", 0x1F);
		break;
	case 17:
		strncpy(name, "SWS_THREAD_TYPE_SW_DEP", 0x1F);
		affinity = SwsGetAffinity();
		if (affinity != 0) {
			affinity |= 0x80000;
			if (affinity != 0x80000)
				priority = 192;
		}
		break;
	case 18:
		stackSize = 0x20000;
		strncpy(name, "SWS_THREAD_TYPE_TOOL", 0x1F);
		break;
	case 19:
		strncpy(name, "SWS_THREAD_TYPE_ANONYMOUS", 0x1F);
		affinity = SwsGetAffinity();
		if (affinity != 0) {
			affinity |= 0x80000;
			if (affinity != 0x80000)
				priority = 192;
		}
		break;
	default:
		break;
	}
	id = sceKernelCreateThread(name, SwMwThreadTrampoline, priority, stackSize, 0,
				   affinity, NULL);
	if (id >= 0) {
		thread->id = id;
		return sceKernelStartThread(id, 0xC, thread);
	}
	return id;
}


/* the manager's error translation: -7..0 map to the manager statuses, anything else to
 * 12001 (0x81005776, the jump table at 0x8100578e) */
extern "C" int SwMwManagerMapStatus(int status)
{
	switch (status) {
	case 0:
		return 0;
	case -1:
		return 12015;
	case -2:
		return 12003;
	case -3:
		return 13017;
	case -4:
		return 12002;
	case -5:
		return 13003;
	case -6:
		return 13002;
	case -7:
		return 13019;
	default:
		return 12001;
	}
}

/* cancel the activation and the download in flight (0x8100458c) */
extern "C" int SwMwManagerCancel(void)
{
	int status;

	_sceDtcpIpCancelDownload();
	status = _sceDtcpIpCancelActivation();
	if (status != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 2367);
		return SwMwManagerMapStatus(status);
	}
	return 0;
}

/* give a move sink a slot in the device's move table (0x810050fc) */
extern "C" int SwMwManagerRegisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink)
{
	int i;

	for (i = 0; i < 35; ++i) {
		if (slots->moveSinks[i] == 0) {
			slots->moveSinks[i] = sink;
			return 0;
		}
	}
	return 12008;
}

/* free a move sink's slot in the device's move table (0x8100514a) */
extern "C" int SwMwManagerUnregisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink)
{
	int i;

	for (i = 0; i < 35; ++i) {
		if (slots->moveSinks[i] == sink) {
			slots->moveSinks[i] = 0;
			return 0;
		}
	}
	return 12008;
}

/* start a session: the support singleton, the content and the manager (0x81004e40) */
extern "C" int SwMwManagerSessionStart(struct SwMwManagerSinkSlots *slots, int mode)
{
	int status;
	int failed;
	int i;

	if (slots->starting != 0)
		return 12014;
	slots->starting = mode;
	for (i = 0; i < 35; ++i) {
		slots->sinks[i] = 0;
		slots->moveSinks[i] = 0;
	}
	status = SwMwContentSupportCreate((struct SwMwContentSupport **)&slots->support);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 284);
		failed = 1;
	}
	if (failed != 0)
		return SwMwManagerMapStatus(status);
	status = ((struct SwMwContentSupport *)slots->support)->createContent(&slots->content);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 291);
		failed = 1;
	}
	if (failed != 0)
		return SwMwManagerMapStatus(status);
	status = ((struct SwMwContentSupport *)slots->support)->createManager(&slots->manager);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 298);
		failed = 1;
	}
	if (failed == 0)
		return 0;
	status = SwMwManagerMapStatus(status);
	((struct SwMwContentSupport *)slots->support)->release();
	slots->content = NULL;
	slots->starting = 0;
	return status;
}

/* whether the device certificate exists: 0 active, 13001 to activate (0x810045dc) */
extern "C" int SwMwManagerCheckActivation(void)
{
	int activated;
	int status;
	int failed;

	status = _sceDtcpIpCheckActivation(&activated);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 2385);
		failed = 1;
	}
	if (failed != 0)
		return SwMwManagerMapStatus(status);
	if (activated == 1)
		return 0;
	return 13001;
}

/* the activation's worker state and the thread that runs it */
static struct SwMwManagerActivation g_swMwActivation;
static struct SwMwManagerThread g_swMwActivationThread;

/* wait for a manager thread and delete it (0x810044bc) */
extern "C" int SwMwManagerJoinThread(struct SwMwManagerThread *thread)
{
	int result;

	result = sceKernelWaitThreadEnd(thread->id, NULL, NULL);
	sceKernelDeleteThread(thread->id);
	return result;
}

/* start the activation's worker thread (0x8100444c) */
extern "C" int SwMwManagerStartActivation(void)
{
	int status;
	int failed;

	g_swMwActivation.state = 0;
	status = SwMwManagerThreadCreate(&g_swMwActivationThread,
					 SwMwManagerActivationThread,
					 &g_swMwActivation, 0);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 2317);
		failed = 1;
	}
	if (failed != 0)
		return status;
	return 0;
}

/* the activation's outcome: 12007 while it runs, 12004 if it did not start (0x810044ea) */
extern "C" int SwMwManagerGetActivationResult(int *result)
{
	int failed;

	if (g_swMwActivation.state == 0)
		return 12007;
	if (g_swMwActivation.state != 1)
		return 12004;
	SwMwManagerJoinThread(&g_swMwActivationThread);
	if (result != NULL)
		*result = g_swMwActivation.result;
	failed = 0;
	if (g_swMwActivation.status != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", g_swMwActivation.status,
			  SWMW_MANAGER_SOURCE, 2340);
		failed = 1;
	}
	if (failed != 0)
		return SwMwManagerMapStatus(g_swMwActivation.status);
	return 0;
}

/* the activation's worker: activate, and on the -3 answer download and import the key
 * bin, then activate again (0x810056a6) */
extern "C" int SwMwManagerActivationThread(void *argument)
{
	struct SwMwManagerActivation *activation = (struct SwMwManagerActivation *)argument;
	unsigned short port;
	int size;
	unsigned char buffer[612];
	int status;

	activation->state = 0;
	activation->status = _sceDtcpIpActivate(&activation->result);
	if (activation->status == -3) {
		size = 612;
		SwMwCoreGetPort(&port);
		status = _sceDtcpIpDownload(port, buffer, &size);
		if (status >= 0) {
			status = SwMwCoreImportKeyBin(buffer, size);
			if (status == 13002)
				activation->status = -6;
			else if (status != 0)
				activation->status = -7;
			else
				activation->status = _sceDtcpIpActivate(&activation->result);
		} else {
			activation->result = status;
			activation->status = -3;
		}
	}
	activation->state = 1;
	return 0;
}

/* the parameter table's worker: download it and import it (0x810052f6) */
extern "C" int SwMwManagerParamTblThread(void *argument)
{
	struct SwMwManagerParamTbl *table = (struct SwMwManagerParamTbl *)argument;
	unsigned short port;
	int size;

	size = 612;
	SwMwCoreGetPort(&port);
	table->state = 0;
	table->status = _sceDtcpIpDownload(port, table->data, &size);
	if (table->status >= 0)
		SwMwCoreImportKeyBin(table->data, size);
	table->state = 1;
	return 0;
}

/* start the parameter table's download in the background (0x8100536e) */
extern "C" int SwMwManagerDownloadParamTbl(void)
{
	static struct SwMwManagerParamTbl table;
	static struct SwMwManagerThread thread;
	int status;
	int failed;

	swMwTrace("sw mw:SwMwManagerDownloadParamTbl\n");
	table.state = 0;
	status = SwMwManagerThreadCreate(&thread, SwMwManagerParamTblThread, &table, 0);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1654);
		failed = 1;
	}
	if (failed != 0)
		return status;
	return 0;
}
