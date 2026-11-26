/*
 * The DTCP manager object (SwMwManager.cpp): its session lifecycle, the keybin and record
 * setup behind the manager dispatch's commands, and the checked status helper they share.
 * This file starts with the cancel path and the checked status; the session start/stop and
 * the record setup follow.
 */

#include <stddef.h>
#include <string.h>

/* the SDK's <new> asks std::size_t of the freestanding headers */
namespace std {
using ::size_t;
}
#include <new>
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

/* the session helpers the manager's ensure path calls (the SWS core and records) */
int SwMwManagerCheckParamTblDownloaded(int mode);
int SwMwManagerInit(struct SwMwManager *manager, int mode);
int SwMwManagerMarkStarted(int *started);
int SwMwManagerStopStarted(int started);
int SwMwManagerOpenRecordA(int *record);
int SwMwManagerOpenRecordB(int *record);
int SwMwManagerOpenRecordC(int *record);
int SwMwManagerOpenRecordD(int *record);
int SwMwManagerCloseRecordA(int record);
int SwMwManagerCloseRecordB(int record);
int SwMwManagerCloseRecordC(int record);
int SwMwManagerCloseRecordD(int record);
int SwMwManagerClearSlots(void *slots);
int SwMwManagerCloseKeyBin(int keyBin);
struct SwMwManager *SwMwManagerFree(struct SwMwManager *manager);
int SwMwManagerFlushRecords(struct SwMwManager *manager);
int SwMwManagerCancelDownload(void);
int SwMwCoreRecordWrite(int keyBin, void *data, int size, int type);

/* the shared status mapper (SwMwContent.h) */
int SwMwStatus(int status);

/* the manager's base constructor and the core/key-bin/session calls (the SWS library) */
struct SwMwManager *SwMwManagerBaseConstruct(struct SwMwManager *manager);
void SwMwCoreInitKey(void);
int SwMwCoreKeyBinCreate(int *keyBin);
int SwMwCoreKeyBinWrite(int keyBin, const void *data, int size);
int SwMwSessionInit(struct SwMwManager *manager);

/* SceRegMgr (the SDK's libregmgr; the tree has no header for it yet) */
int sceRegMgrGetKeyBin(const char *category, const char *name, void *buffer, int size);
int sceRegMgrSetKeyBin(const char *category, const char *name, const void *buffer, int size);
}

/* the manager's configuration word (0x8109e0e4) and the certificate template (0x8108ce64) */
static int g_swMwManagerConfig;
extern unsigned char g_swMwCertificateTemplate[];


/* the manager the ensure path keeps for the process (0x8109e0e0) */
static struct SwMwManager *g_swMwManager;


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

/* the parameter table and the thread that downloads it (0x8109e9a4 / 0x8109e980) */
static struct SwMwManagerParamTbl g_swMwParamTbl;
static struct SwMwManagerThread g_swMwParamTblThread;

/* start the parameter table's download in the background (0x8100536e) */
extern "C" int SwMwManagerDownloadParamTbl(void)
{
	int status;
	int failed;

	swMwTrace("sw mw:SwMwManagerDownloadParamTbl\n");
	g_swMwParamTbl.state = 0;
	status = SwMwManagerThreadCreate(&g_swMwParamTblThread, SwMwManagerParamTblThread,
					 &g_swMwParamTbl, 0);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1654);
		failed = 1;
	}
	if (failed != 0)
		return status;
	return 0;
}

/* ensure the manager's session: the parameter table, the manager init, the session start,
 * then the key bin, the started flag and the four records (0x810032d2) */
extern "C" int SwMwManagerEnsureSession(struct SwMwManager *manager, int mode)
{
	int status;
	int failed;

	if (g_swMwManager == NULL) {
		status = SwMwManagerCheckParamTblDownloaded(mode);
		if (status == 0)
			status = SwMwManagerInit(manager, 0);
		if (status != 0)
			return status;
		g_swMwManager = manager;
	}
	status = SwMwManagerSessionStart(&manager->slots, (int)manager);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1257);
		failed = 1;
	}
	if (failed == 0) {
		status = SwMwManagerMarkStarted(&manager->started);
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1276);
			failed = 1;
		}
	}
	if (failed == 0) {
		status = SwMwManagerOpenRecordA(&manager->recordA);
		if (status != 12005 && status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1281);
			failed = 1;
		}
	}
	if (failed == 0) {
		status = SwMwManagerOpenRecordB(&manager->recordB);
		if (status != 12005 && status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1284);
			failed = 1;
		}
	}
	if (failed == 0 && manager->parameter1 == 0)
		return 0;
	if (failed == 0) {
		status = SwMwManagerOpenRecordC(&manager->recordC);
		if (status != 12005 && status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1289);
			failed = 1;
		}
	}
	if (failed == 0) {
		status = SwMwManagerOpenRecordD(&manager->recordD);
		if (status != 12005 && status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1291);
			failed = 1;
		}
	}
	if (failed == 0)
		return 0;
	SwMwManagerClearSlots(&manager->slots);
	if (manager->object != NULL)
		manager->object->close();
	if (manager->keyBin != 0)
		SwMwManagerCloseKeyBin(manager->keyBin);
	if (manager->started != 0)
		SwMwManagerStopStarted(manager->started);
	if (manager->recordA != 0)
		SwMwManagerCloseRecordA(manager->recordA);
	if (manager->recordB != 0)
		SwMwManagerCloseRecordB(manager->recordB);
	if (manager->parameter1 != 0) {
		SwMwManagerCloseRecordC(manager->recordC);
		SwMwManagerCloseRecordD(manager->recordD);
	}
	operator delete(SwMwManagerFree(manager));
	return status;
}

/* tear the manager down: flush the records, close the threads, key bin and slots, cancel
 * the downloads and free the object (0x8100356a) */
extern "C" int SwMwManagerTeardown(struct SwMwManager *manager)
{
	int status;

	if (manager->object == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 1355);
	if (manager->keyBin == 0)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 1356);
	if (manager->keyBin != 0) {
		status = SwMwManagerFlushRecords(manager);
		if (status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1360);
	}
	if (manager->parameter1 != 0) {
		SwMwManagerCloseRecordD(manager->recordD);
		SwMwManagerCloseRecordC(manager->recordC);
	}
	if (manager->recordB != 0)
		SwMwManagerCloseRecordB(manager->recordB);
	if (manager->recordA != 0)
		SwMwManagerCloseRecordA(manager->recordA);
	if (manager->started != 0)
		SwMwManagerStopStarted(manager->started);
	SwMwManagerClearSlots(&manager->slots);
	if (manager->object != NULL)
		manager->object->close();
	if (manager->keyBin != 0)
		SwMwManagerCloseKeyBin(manager->keyBin);
	if (g_swMwParamTbl.state == 0) {
		status = SwMwManagerCancelDownload();
		swMwTrace("sw mw:SwMwManagerCancelDownload err = %d\n", status);
	}
	if (g_swMwParamTbl.state != -1) {
		status = SwMwManagerCheckParamTblDownloaded(0);
		swMwTrace("sw mw:SwMwManagerCheckParamTblDownloaded err = %d\n", status);
	}
	::operator delete(SwMwManagerFree(manager));
	g_swMwManager = NULL;
	return 0;
}

/* write one record through the session (0x8100376a) */
extern "C" int SwMwManagerWriteRecord(struct SwMwManager *manager, int value, int size)
{
	int status;
	int failed;

	if (manager == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 1454);
	if (manager->object == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 1455);
	status = manager->object->write(value, size);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 1458);
		failed = 1;
	}
	if (failed != 0)
		return SwMwStatus(status);
	return 0;
}

/* write one record and mark it done (0x810036e4) */
extern "C" int SwMwManagerWriteRecordDone(struct SwMwManager *manager, int value, int size,
					  int *done)
{
	int status;

	if (manager->object == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 1432);
	status = manager->object->write(value, size);
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 1435);
		return SwMwStatus(status);
	}
	*done = 1;
	return 0;
}

/* exchange a record buffer with the session (0x81003802) */
extern "C" int SwMwManagerExchangeRecord(struct SwMwManager *manager, void *buffer,
					 unsigned int size, int *value)
{
	int local;
	int status;
	int failed;

	if (manager == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 1480);
	if (manager->object == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 1481);
	if (size < 0xC10)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MANAGER_SOURCE, 1482);
	status = manager->object->exchange(8, buffer, &local, manager);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 1488);
		failed = 1;
	}
	if (failed != 0)
		return SwMwStatus(status);
	*value = local;
	return 0;
}

/* flush the session's records into the key bin (0x810039d2) */
extern "C" int SwMwManagerFlushRecords(struct SwMwManager *manager)
{
	unsigned char *buffer;
	int value;
	int status;
	int failed;

	buffer = (unsigned char *)malloc(0x1000);
	failed = 0;
	if (buffer == NULL) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 1546);
		failed = 1;
	}
	if (failed != 0) {
		status = 12008;
	} else {
		status = SwMwManagerExchangeRecord(manager, buffer, 4096, &value);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_MANAGER_SOURCE, 1549);
			failed = 1;
		}
		if (failed == 0) {
			status = SwMwCoreRecordWrite(manager->keyBin, buffer, value, 3);
			failed = 0;
			if (status != 0) {
				swMwTrace("[SWMW] Error %d in %s:%d\n", status,
					  SWMW_MANAGER_SOURCE, 1554);
				failed = 1;
			}
		}
		if (buffer != NULL)
			free(buffer);
	}
	return status;
}

/* create the manager: its base, the key bin, the certificate descriptor's template and
 * device certificate, then the session, the init and the parameter table (0x81002de2) */
extern "C" int SwMwManagerCreate(struct SwMwManager **out, const int *config,
				 struct SwMwManagerCertificate *certificate, int argument)
{
	struct SwMwManager *manager;
	int status;
	int failed;
	int type;
	int i;
	int j;

	manager = (struct SwMwManager *)::operator new(sizeof(struct SwMwManager),
						       std::nothrow);
	if (manager != NULL)
		manager = SwMwManagerBaseConstruct(manager);
	failed = 0;
	if (manager == NULL) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 1089);
		failed = 1;
	}
	if (failed != 0)
		return 12008;
	manager->parameter0 = config[0];
	manager->parameter1 = config[1];
	g_swMwManagerConfig = config[2];
	if (config[3] != 0)
		SwMwCoreInitKey();
	status = SwMwCoreKeyBinCreate(&manager->keyBin);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 1101);
		failed = 1;
	}
	type = 0;
	if (failed == 0 && certificate != NULL)
		type = certificate->type;
	if (failed == 0 && type > 2) {
		if (type <= 3) {
			if (certificate->device[0] != 0 || certificate->device[1] != 0 ||
			    certificate->device[2] != 0) {
				if (certificate != NULL) {
					status = sceRegMgrSetKeyBin("/CONFIG/DTCPIP", "devcert",
								   certificate->device, 3);
					failed = 0;
					if (status < 0) {
						swMwTrace("[SWMW] ERROR in %s:%d\n",
							  SWMW_MANAGER_SOURCE, 1149);
						failed = 1;
					}
					if (failed == 0) {
						status = SwMwCoreKeyBinWrite(manager->keyBin,
									     certificate, argument);
						if (status != 12005 && status != 0) {
							swMwTrace("[SWMW] Error %d in %s:%d\n",
								  status, SWMW_MANAGER_SOURCE, 1152);
							failed = 1;
						}
					}
				}
			} else {
				status = sceRegMgrGetKeyBin("/CONFIG/DTCPIP", "devcert",
							    certificate->device, 3);
				failed = 0;
				if (status < 0) {
					swMwTrace("[SWMW] ERROR in %s:%d\n",
						  SWMW_MANAGER_SOURCE, 1130);
					failed = 1;
				}
				for (i = 0; i < 3; ++i) {
					if (certificate->device[i] != 0)
						break;
				}
				if (i == 3) {
					status = 13001;
					failed = 1;
				}
				if (failed == 0) {
					certificate->type = 3;
					certificate->templatePtr = g_swMwCertificateTemplate;
					status = SwMwCoreKeyBinWrite(manager->keyBin, certificate, 140);
					if (status != 12005 && status != 0) {
						swMwTrace("[SWMW] Error %d in %s:%d\n", status,
							  SWMW_MANAGER_SOURCE, 1144);
						failed = 1;
					}
				}
			}
		}
	} else if (failed == 0 && type != 0) {
		status = 12003;
		failed = 1;
	} else if (failed == 0) {
		status = sceRegMgrGetKeyBin("/CONFIG/DTCPIP", "devcert", certificate->device, 3);
		failed = 0;
		if (status < 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 1115);
			failed = 1;
		}
		for (j = 0; j < 3; ++j) {
			if (certificate->device[j] != 0)
				break;
		}
		if (j == 3) {
			status = 13001;
			failed = 1;
		}
	}
	if (failed == 0) {
		status = SwMwSessionInit(manager);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 1171);
			failed = 1;
		}
	}
	if (failed == 0) {
		status = manager->object->start();
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 1176);
			failed = 1;
		}
	}
	if (failed == 0) {
		g_swMwParamTbl.state = -1;
		status = SwMwManagerInit(manager, 1);
		if (status != 0) {
			if (status == 13002 || status == 13017 || status == 13018 ||
			    status == 13019)
				status = SwMwManagerDownloadParamTbl();
		} else {
			g_swMwManager = manager;
		}
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_MANAGER_SOURCE, 1194);
			failed = 1;
		}
	}
	if (failed == 0) {
		*out = manager;
		return 0;
	}
	status = SwMwStatus(status);
	if (manager->object != NULL)
		manager->object->close();
	if (manager->keyBin != 0)
		SwMwManagerCloseKeyBin(manager->keyBin);
	SwMwManagerStopStarted(manager->started);
	SwMwManagerCloseRecordA(manager->recordA);
	SwMwManagerCloseRecordB(manager->recordB);
	if (manager->parameter1 != 0) {
		SwMwManagerCloseRecordC(manager->recordC);
		SwMwManagerCloseRecordD(manager->recordD);
	}
	::operator delete(SwMwManagerFree(manager));
	return status;
}
