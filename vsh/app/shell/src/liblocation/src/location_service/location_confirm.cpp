/*
 * location_confirm.cpp: location permission confirmation dialog manager ("location_plugin"
 * integration, per-app dialog queue, and app focus suspend/resume; 0x81187F14-0x81188F3E).
 * The file's and internal names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <liblocation.h>
#include <std/string.h>
#include <std/new>
#include <plugin.h>
#include <system/system.h>
#include <datetime/datetime.h>
#include "../../../../shell.h"
#include "../../../../plugins.h"
#include "location_service.h"

using namespace paf;

/* PLACEHOLDER externs for the rest of liblocation */
struct LocationContext;
extern void *g_locationManager;					/* (0x815496B4) */

LocationContext *locationGetContext(void);			/* (0x8118BA84) */
int locationCtxGetPermission(LocationContext *ctx, int appId, SceLocationHandle handle,
			     SceLocationPermissionInfo *info);	/* (0x8118D9E4) */
int locationCtxIsHandlePending(LocationContext *ctx, SceLocationHandle handle,
			       bool *pending);			/* (0x8118DFF0) */

/* location manager timer/task dispatch (0x8118A780-0x8118AB62) */
int locationTaskSchedule(void *task, int delay);		/* (0x8118A780) */
int locationTaskCancel(void *task);				/* (0x8118A800) */
int locationMgrRegisterTask(void *mgr, void *task);		/* (0x8118AACE) */
int locationMgrUnregisterTask(void *mgr, void *task);		/* (0x8118AB62) */

/* shell location_plugin loader (0x813C4C68, 0x813C4E96) */
typedef void (*LocationPluginDoneFn)(int result, void *arg);
typedef void (*LocationPluginLoadedFn)(int unused, LocationPluginDoneFn doneCb, void *arg);
int locationPluginLoad(LocationPluginLoadedFn loadedCb, LocationPluginDoneFn doneCb,
		       void *arg);				/* (0x813C4C68) */
int locationPluginUnload(void);					/* (0x813C4E96) */

typedef void (*LocationConfirmCallback)(int appId, int handle, int dialogType, int result,
					int canceled);

struct LocationPluginInterface {
	void *reserved[3];
	int (*Show)(LocationPluginDoneFn doneCb, void *entry);	/* +12 */
	int (*GetResult)(void);					/* +16 */
	int (*GetStatus)(void);					/* +20 */
	int (*Abort)(void);					/* +24 */
	void *reserved28;
	int (*Suspend)(void);					/* +32 */
	int (*Close)(void);					/* +36 */
};

struct LocationConfirmApp {
	int appId;						/* +0 */
	SceLocationHandle activeHandle;				/* +4 */
	signed char handleCount;				/* +8 */
	SceLocationHandle handles[8];				/* +12..+43 */
};

struct LocationConfirmEntry {
	int paused;						/* +0 */
	int dialogType;						/* +4 */
	unsigned char abortFlag;				/* +8 */
	unsigned char shownPrompt;				/* +9 */
	unsigned char reservedA;				/* +10 */
	unsigned char padB;					/* +11 */
	int step;						/* +12 */
	SceLocationDialogStatus status;				/* +16 */
	SceLocationDialogResult result;				/* +20 */
	int appId;						/* +24 */
	SceLocationHandle handle;				/* +28 */
	SceUInt32 arg;						/* +32 */
	SceUInt32 dataArg;					/* +36 */
	char data[0x800];					/* +40..+2087 */
	LocationConfirmCallback notifyFn;			/* +2088 */
	int globalPerm;						/* +2092 */
	int appPerm;						/* +2096 */
	SceLocationPermissionInfo permInfo;			/* +2100..+2119 */
};

class LocationConfirmTask {
public:
	virtual int OnNotify();					/* (0x81187F20) */

	int m_queued;						/* +4 */
	int m_active;						/* +8 */
	int m_reservedC;					/* +12 */
	int m_dialogType;					/* +16 */
	int m_reserved14[3];					/* +20..+28 */
	int m_result;						/* +32 */
	int m_appId;						/* +36 */
	int m_handle;						/* +40 */
	char m_pad[2088];					/* +44..+2131 */
	int m_canceled;						/* +2132 */
};

class LocationConfirmMgr {
public:
	LocationConfirmMgr(LocationConfirmCallback cb);
	~LocationConfirmMgr();

	void OnAppDeactivated(int appId);
	void OnAppActivated(int appId);
	int AllocEntry(int appId, SceLocationHandle handle, SceUInt32 arg, SceUInt32 dataArg,
		       const void *data);
	int Show(int appId, SceLocationHandle handle, SceUInt32 arg, SceUInt32 dataArg,
		 const void *data, bool allowBusy);
	SceLocationDialogResult GetResult(int appId, SceLocationHandle handle);
	SceLocationDialogStatus GetStatus(int appId, SceLocationHandle handle);
	int Abort(int appId, SceLocationHandle handle);
	int RemoveEntry(int appId, SceLocationHandle handle);
	int CloseApp(int appId, SceLocationHandle handle);

	int m_activeAppId;					/* +0 */
	LocationConfirmApp m_apps[8];				/* +4..+355 */
	LocationConfirmEntry m_entries[8];			/* +356..+17315 */
	signed char m_entryCount;				/* +17316 */
	int m_pluginState;					/* +17320 */
	LocationConfirmCallback m_callback;			/* +17324 */
	LocationContext *m_context;				/* +17328 */
	LocationPluginDoneFn m_pluginDoneCb;			/* +17332 */
	int m_lastResult;					/* +17336 */
	LocationConfirmTask m_task;				/* +17340..+19475 */
};

/* the singleton (0x81544CD8) */
static LocationConfirmMgr *s_confirmMgr;

static void locationConfirmOnAppEvent(int event, const void *data, SceSize size, void *arg);
static void locationConfirmOnDialogNotify(int appId, int handle, int dialogType, int result,
					  int canceled);

/* (0x81187F14) */
LocationConfirmMgr *locationConfirmGetInstance(void)
{
	return s_confirmMgr;
}

/* (0x81187F20) */
int LocationConfirmTask::OnNotify()
{
	s_confirmMgr->m_callback(m_appId, m_handle, m_dialogType, m_result, m_canceled);
	return 0;
}

/* (0x81187F4E) */
__attribute__((noinline))
LocationConfirmMgr::LocationConfirmMgr(LocationConfirmCallback cb)
{
	m_entryCount = 0;
	m_task.m_active = 0;
	m_task.m_queued = 0;
	m_callback = cb;
	m_pluginState = -1;
	m_context = locationGetContext();
	m_activeAppId = -1;
	m_pluginDoneCb = NULL;
	m_lastResult = 0;
	for (signed char i = 0; i < 8; i++) {
		sce_paf_memset(&m_entries[i], 0, sizeof(m_entries[i]));
		m_entries[i].appId = -1;
		m_apps[i].appId = -1;
		m_apps[i].activeHandle = (SceLocationHandle)-1;
		m_apps[i].handleCount = 0;
		for (signed char j = 0; j < 8; j++)
			m_apps[i].handles[j] = (SceLocationHandle)-1;
	}
	locationMgrRegisterTask(g_locationManager, &m_task);
}

/* (0x81187FEE) */
LocationConfirmMgr::~LocationConfirmMgr()
{
	if (m_task.m_queued != 0) {
		locationTaskCancel(&m_task);
		locationMgrUnregisterTask(g_locationManager, &m_task);
	}
	m_pluginState = -1;
	m_callback = NULL;
	m_entryCount = 0;
	m_context = NULL;
	m_activeAppId = -1;
	m_pluginDoneCb = NULL;
	m_lastResult = 0;
	for (signed char i = 0; i < 8; i++) {
		sce_paf_memset(&m_entries[i], 0, sizeof(m_entries[i]));
		m_entries[i].appId = -1;
		m_apps[i].appId = -1;
		m_apps[i].activeHandle = (SceLocationHandle)-1;
		m_apps[i].handleCount = 0;
		for (signed char j = 0; j < 8; j++)
			m_apps[i].handles[j] = (SceLocationHandle)-1;
	}
}

/* (0x81188096) */
int locationConfirmInit(LocationConfirmCallback cb)
{
	if (s_confirmMgr == NULL) {
		LocationConfirmMgr *mgr = new (g_locationHeap) LocationConfirmMgr(cb);

		s_confirmMgr = mgr;
		if (mgr == NULL)
			return SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
		SceShell_83D16CA8(SHELL_APP_EVENT_ACTIVATED, locationConfirmOnAppEvent, NULL);
		SceShell_83D16CA8(SHELL_APP_EVENT_DEACTIVATED, locationConfirmOnAppEvent, NULL);
	}
	return 0;
}

/* (0x811880FE) */
int locationConfirmTerm(void)
{
	if (s_confirmMgr != NULL) {
		SceShell_E3D3A162(SHELL_APP_EVENT_ACTIVATED, locationConfirmOnAppEvent, NULL);
		SceShell_E3D3A162(SHELL_APP_EVENT_DEACTIVATED, locationConfirmOnAppEvent, NULL);
		delete s_confirmMgr;
		s_confirmMgr = NULL;
	}
	return 0;
}

/* (0x8118814C) */
static void locationConfirmOnPluginDone(int result, void *arg)
{
	LocationConfirmEntry *entry = (LocationConfirmEntry *)arg;

	s_confirmMgr->m_pluginState = -1;
	if ((unsigned int)result > 1) {
		if (result < 0) {
			entry->status = SCE_LOCATION_DIALOG_STATUS_IDLE;
			entry->dialogType = 0;
			entry->result = SCE_LOCATION_DIALOG_RESULT_NONE;
			s_confirmMgr->m_lastResult = result;
			if (entry->notifyFn != NULL) {
				s_confirmMgr->m_task.m_result = entry->result;
				s_confirmMgr->m_task.m_appId = entry->appId;
				s_confirmMgr->m_task.m_handle = entry->handle;
				s_confirmMgr->m_task.m_dialogType = entry->dialogType;
				s_confirmMgr->m_task.m_canceled = 0;
				locationTaskSchedule(&s_confirmMgr->m_task, 1);
			}
		}
	} else {
		if (entry->step == 11) {
			s_confirmMgr->m_task.m_result = entry->result;
			s_confirmMgr->m_task.m_appId = entry->appId;
			s_confirmMgr->m_task.m_handle = entry->handle;
			s_confirmMgr->m_task.m_dialogType = entry->dialogType;
			s_confirmMgr->m_task.m_canceled = 0;
			locationTaskSchedule(&s_confirmMgr->m_task, 1);
		}
		s_confirmMgr->m_lastResult = 0;
	}
}

/* (0x8118821C) */
static void locationConfirmOnPluginLoaded(int unused, LocationPluginDoneFn doneCb, void *arg)
{
	LocationConfirmEntry *entry = (LocationConfirmEntry *)arg;

	s_confirmMgr->m_pluginState = 1;
	s_confirmMgr->m_pluginDoneCb = doneCb;
	if (entry->appId == -1 || entry->paused != 0 ||
	    entry->status != SCE_LOCATION_DIALOG_STATUS_RUNNING) {
		doneCb(2, entry);
		return;
	}

	Plugin *plugin = Plugin::Find("location_plugin");

	if (plugin == NULL)
		return;

	LocationPluginInterface *iface = (LocationPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return;

	int step = entry->step;

	if (step != 10) {
		int globalPerm;
		int appPerm;
		SceLocationPermissionInfo permInfo;

		locationCtxGetPermission(s_confirmMgr->m_context, entry->appId, entry->handle,
					 &permInfo);
		locationSvcSetPermissionMode((int)(intptr_t)&globalPerm);
		entry->globalPerm = globalPerm;
		locationSvcSetAppPermission((const char *)&appPerm, false);
		entry->appPerm = appPerm;
		entry->permInfo = permInfo;

		if (system::Model::GetType() == 1) {
			step = 1;
			entry->step = 1;
		} else if (permInfo.applicationstatus == SCE_LOCATION_PERMISSION_APPLICATION_INIT &&
			   entry->reservedA == 0) {
			step = 2;
			entry->step = 2;
			entry->shownPrompt = 1;
		} else if (globalPerm == 0) {
			step = 3;
			entry->step = 3;
		} else if (appPerm == 0) {
			step = 4;
			entry->step = 4;
		} else if (permInfo.parentalstatus == SCE_LOCATION_PERMISSION_DENY) {
			step = 5;
			entry->step = 5;
		} else if (permInfo.applicationstatus == SCE_LOCATION_PERMISSION_APPLICATION_INIT) {
			step = 6;
			entry->step = 6;
			entry->shownPrompt = 1;
		} else if (permInfo.mainstatus == SCE_LOCATION_PERMISSION_DENY) {
			step = 7;
			entry->step = 7;
		} else if (permInfo.applicationstatus == SCE_LOCATION_PERMISSION_APPLICATION_DENY) {
			step = 8;
			entry->step = 8;
		} else {
			step = 11;
			entry->step = 11;
		}
	}

	entry->paused = 0;
	entry->abortFlag = 0;
	if (step == 11) {
		entry->status = SCE_LOCATION_DIALOG_STATUS_FINISHED;
		entry->result = SCE_LOCATION_DIALOG_STATUS_ENABLE;
		entry->dialogType = 0;
		doneCb(0, entry);
	} else {
		int ret = iface->Show(doneCb, entry);

		if (ret != 0)
			doneCb(ret, entry);
	}
}

/* (0x81188396) */
void LocationConfirmMgr::OnAppDeactivated(int appId)
{
	Plugin *plugin = Plugin::Find("location_plugin");
	LocationPluginInterface *iface =
		(plugin != NULL) ? (LocationPluginInterface *)plugin->GetInterface(1) : NULL;

	if (iface != NULL) {
		signed char i = 0;

		for (; i < 8; i++) {
			if (m_apps[i].appId == appId)
				break;
		}
		if (i >= 8)
			return;

		SceLocationHandle handle = m_apps[i].activeHandle;

		if (appId < 0)
			return;

		signed char j = 0;

		for (; j < 8; j++) {
			if (m_entries[j].appId == appId && m_entries[j].handle == handle)
				break;
		}
		if (j >= 8)
			return;
		if (m_entries[j].status == SCE_LOCATION_DIALOG_STATUS_RUNNING) {
			m_entries[j].paused = 1;
			iface->Suspend();
		}
	} else {
		signed char i = 0;

		for (; i < 8; i++) {
			if (m_apps[i].appId == appId)
				break;
		}
		if (i >= 8)
			return;

		SceLocationHandle handle = m_apps[i].activeHandle;

		if (appId < 0)
			return;

		signed char j = 0;

		for (; j < 8; j++) {
			if (m_entries[j].appId == appId && m_entries[j].handle == handle)
				break;
		}
		if (j >= 8)
			return;
		if (m_entries[j].status == SCE_LOCATION_DIALOG_STATUS_RUNNING)
			m_entries[j].paused = 1;
	}
}

/* (0x81188482) */
void LocationConfirmMgr::OnAppActivated(int appId)
{
	Plugin *plugin = Plugin::Find("location_plugin");
	LocationPluginInterface *iface =
		(plugin != NULL) ? (LocationPluginInterface *)plugin->GetInterface(1) : NULL;

	if (iface != NULL) {
		signed char i = 0;

		for (; i < 8; i++) {
			if (m_apps[i].appId == appId)
				break;
		}
		if (i >= 8)
			return;

		SceLocationHandle handle = m_apps[i].activeHandle;

		if (appId < 0)
			return;

		signed char j = 0;

		for (; j < 8; j++) {
			if (m_entries[j].appId == appId && m_entries[j].handle == handle)
				break;
		}
		if (j >= 8)
			return;

		LocationConfirmEntry *entry = &m_entries[j];

		m_activeAppId = appId;
		entry->paused = 0;
		if (entry->status == SCE_LOCATION_DIALOG_STATUS_RUNNING && entry->step == 10) {
			SceLocationPermissionInfo permInfo;

			locationCtxGetPermission(m_context, appId, entry->handle, &permInfo);
			if (permInfo.mainstatus != SCE_LOCATION_PERMISSION_DENY) {
				if (entry->shownPrompt != 0 ||
				    permInfo.applicationstatus !=
					    SCE_LOCATION_PERMISSION_APPLICATION_DENY) {
					entry->step = 11;
					entry->status = SCE_LOCATION_DIALOG_STATUS_FINISHED;
					entry->dialogType = 3;
					entry->result = SCE_LOCATION_DIALOG_STATUS_ENABLE;
					m_pluginDoneCb(0, entry);
				} else {
					entry->step = 8;
				}
			} else {
				entry->step = 11;
				entry->status = SCE_LOCATION_DIALOG_STATUS_FINISHED;
				entry->result = SCE_LOCATION_DIALOG_RESULT_DISABLE;
				entry->dialogType = 3;
				m_pluginDoneCb(0, entry);
			}
		}
		return;
	}

	signed char i = 0;

	for (; i < 8; i++) {
		if (m_apps[i].appId == appId)
			break;
	}
	if (i >= 8)
		return;

	SceLocationHandle handle = m_apps[i].activeHandle;

	if (appId < 0)
		return;

	signed char j = 0;

	for (; j < 8; j++) {
		if (m_entries[j].appId == appId && m_entries[j].handle == handle)
			break;
	}
	if (j >= 8)
		return;

	LocationConfirmEntry *entry = &m_entries[j];

	m_activeAppId = appId;
	if (entry->status != SCE_LOCATION_DIALOG_STATUS_RUNNING)
		return;

	entry->result = SCE_LOCATION_DIALOG_RESULT_NONE;
	entry->shownPrompt = 0;

	int globalPerm;
	int appPerm;
	SceLocationPermissionInfo permInfo;

	locationCtxGetPermission(m_context, appId, entry->handle, &permInfo);
	entry->permInfo = permInfo;
	locationSvcSetPermissionMode((int)(intptr_t)&globalPerm);
	locationSvcSetAppPermission((const char *)&appPerm, false);

	int step;

	if (system::Model::GetType() == 1) {
		step = 1;
		entry->step = 1;
	} else if (permInfo.applicationstatus == SCE_LOCATION_PERMISSION_APPLICATION_INIT &&
		   entry->reservedA == 0) {
		step = 2;
		entry->step = 2;
		entry->shownPrompt = 1;
	} else if (globalPerm == 0) {
		step = 3;
		entry->step = 3;
	} else if (appPerm == 0) {
		step = 4;
		entry->step = 4;
	} else if (permInfo.parentalstatus == SCE_LOCATION_PERMISSION_DENY) {
		step = 5;
		entry->step = 5;
	} else if (permInfo.applicationstatus == SCE_LOCATION_PERMISSION_APPLICATION_INIT) {
		step = 6;
		entry->step = 6;
		entry->shownPrompt = 1;
	} else if (permInfo.mainstatus == SCE_LOCATION_PERMISSION_DENY) {
		step = 7;
		entry->step = 7;
	} else if (permInfo.applicationstatus == SCE_LOCATION_PERMISSION_APPLICATION_DENY) {
		step = 8;
		entry->step = 8;
	} else {
		step = 11;
		entry->step = 11;
	}

	entry->paused = 0;
	if (step == 11) {
		entry->dialogType = 0;
		entry->status = SCE_LOCATION_DIALOG_STATUS_FINISHED;
		entry->result = SCE_LOCATION_DIALOG_STATUS_ENABLE;
		s_confirmMgr->m_task.m_result = SCE_LOCATION_DIALOG_STATUS_ENABLE;
		s_confirmMgr->m_task.m_appId = entry->appId;
		s_confirmMgr->m_task.m_handle = entry->handle;
		s_confirmMgr->m_task.m_dialogType = 0;
		s_confirmMgr->m_task.m_canceled = 0;
		locationTaskSchedule(&s_confirmMgr->m_task, 1);
	} else if (locationPluginLoad(locationConfirmOnPluginLoaded, locationConfirmOnPluginDone,
				      entry) != 0) {
		entry->step = 0;
		entry->status = SCE_LOCATION_DIALOG_STATUS_IDLE;
		entry->result = SCE_LOCATION_DIALOG_RESULT_NONE;
		entry->dialogType = 0;
		s_confirmMgr->m_task.m_result = SCE_LOCATION_DIALOG_RESULT_NONE;
		s_confirmMgr->m_task.m_appId = entry->appId;
		s_confirmMgr->m_task.m_handle = entry->handle;
		s_confirmMgr->m_task.m_dialogType = 0;
		s_confirmMgr->m_task.m_canceled = 0;
		locationTaskSchedule(&s_confirmMgr->m_task, 1);
	} else {
		m_pluginState = 0;
	}
}

/* (0x811887B6) */
static void locationConfirmOnAppEvent(int event, const void *data, SceSize size, void *arg)
{
	int appId = ((const int *)data)[1];

	if (event == 5)
		s_confirmMgr->OnAppActivated(appId);
	else
		s_confirmMgr->OnAppDeactivated(appId);
}

/* (0x811887D6) */
int LocationConfirmMgr::AllocEntry(int appId, SceLocationHandle handle, SceUInt32 arg,
				   SceUInt32 dataArg, const void *data)
{
	signed char idx = 0;

	for (; idx < 8; idx++) {
		if (m_entries[idx].appId == -1)
			break;
	}
	if (idx < 8) {
		LocationConfirmEntry *entry = &m_entries[idx];

		entry->appId = appId;
		entry->handle = handle;
		entry->arg = arg;
		entry->dataArg = dataArg;
		if (data != NULL)
			sce_paf_memcpy(entry->data, data, sizeof(entry->data));
		entry->step = 0;
		entry->result = SCE_LOCATION_DIALOG_RESULT_NONE;
		entry->notifyFn = locationConfirmOnDialogNotify;
		entry->status = SCE_LOCATION_DIALOG_STATUS_RUNNING;
		entry->abortFlag = 0;
		entry->reservedA = 0;
		m_activeAppId = appId;
		m_entryCount++;
	}
	if (idx < 8) {
		signed char i = 0;

		for (; i < 8; i++) {
			if (m_apps[i].appId == appId) {
				signed char j = 0;

				for (; j < 8; j++) {
					if (m_apps[i].handles[j] == (SceLocationHandle)-1) {
						m_apps[i].handles[j] = handle;
						m_apps[i].activeHandle = handle;
						m_apps[i].handleCount++;
						break;
					}
				}
				if (j < 8)
					break;
			}
		}
		if (i >= 8) {
			for (signed char j = 0; j < 8; j++) {
				if (m_apps[j].appId == -1) {
					m_apps[j].appId = appId;
					signed char k = 0;

					for (; k < 8; k++) {
						if (m_apps[j].handles[k] == (SceLocationHandle)-1) {
							m_apps[j].handles[k] = handle;
							m_apps[j].activeHandle = handle;
							m_apps[j].handleCount++;
							break;
						}
					}
					if (k < 8)
						break;
				}
			}
		}
	}
	return idx;
}

/* (0x81188902) */
int LocationConfirmMgr::Show(int appId, SceLocationHandle handle, SceUInt32 arg,
			     SceUInt32 dataArg, const void *data, bool allowBusy)
{
	int ret = 0;
	signed char idx = -1;

	if (m_entryCount > 0) {
		for (signed char i = 0; i < 8; i++) {
			if (m_entries[i].appId == appId && m_entries[i].handle == handle) {
				idx = i;
				break;
			}
		}
	}

	if (idx == -1) {
		if (m_entryCount < 8)
			idx = (signed char)AllocEntry(appId, handle, arg, dataArg, data);
	} else if (m_entries[idx].status == SCE_LOCATION_DIALOG_STATUS_RUNNING) {
		bool pending = false;

		locationCtxIsHandlePending(m_context, handle, &pending);
		if (!pending && !allowBusy)
			return SCE_LOCATION_ERROR_MULTIPLE_CONFIRM;
		return 0;
	} else {
		m_activeAppId = appId;
		for (signed char i = 0; i < 8; i++) {
			if (m_apps[i].appId == appId) {
				m_apps[i].activeHandle = handle;
				break;
			}
		}
	}

	if (idx < 8) {
		int globalPerm;
		int appPerm;
		SceLocationPermissionInfo permInfo;

		ret = locationSvcSetPermissionMode((int)(intptr_t)&globalPerm);
		if (ret == 0) {
			ret = locationSvcSetAppPermission((const char *)&appPerm, false);
			if (ret == 0) {
				ret = locationCtxGetPermission(m_context, appId, handle, &permInfo);
				if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
					LocationConfirmEntry *entry = &m_entries[idx];

					entry->status = SCE_LOCATION_DIALOG_STATUS_RUNNING;
					entry->result = SCE_LOCATION_DIALOG_RESULT_NONE;
					entry->step = 0;
					entry->abortFlag = 0;
					entry->shownPrompt = 0;
					entry->reservedA = 0;
					entry->permInfo = permInfo;
					m_lastResult = 0;

					if (system::Model::GetType() == 1) {
						entry->step = 1;
					} else if (permInfo.applicationstatus ==
						   SCE_LOCATION_PERMISSION_APPLICATION_INIT) {
						entry->step = 2;
						entry->shownPrompt = 1;
					} else if (globalPerm == 0) {
						entry->step = 3;
					} else if (appPerm == 0) {
						entry->step = 4;
					} else if (permInfo.parentalstatus ==
						   SCE_LOCATION_PERMISSION_DENY) {
						entry->step = 5;
					} else if (permInfo.mainstatus ==
						   SCE_LOCATION_PERMISSION_DENY) {
						entry->step = 7;
					} else if (permInfo.applicationstatus ==
						   SCE_LOCATION_PERMISSION_APPLICATION_DENY) {
						entry->step = 8;
					} else {
						entry->result = SCE_LOCATION_DIALOG_STATUS_ENABLE;
						entry->status = SCE_LOCATION_DIALOG_STATUS_FINISHED;
						entry->step = 11;
						m_callback(appId, handle, 0,
							   SCE_LOCATION_DIALOG_STATUS_ENABLE, 0);
						return 0;
					}

					if (m_pluginState == -1) {
						m_pluginState = 0;
						ret = locationPluginLoad(
							locationConfirmOnPluginLoaded,
							locationConfirmOnPluginDone, entry);
						if (ret != 0) {
							m_pluginState = -1;
							entry->step = 0;
							entry->status =
								SCE_LOCATION_DIALOG_STATUS_IDLE;
							entry->result =
								SCE_LOCATION_DIALOG_RESULT_NONE;
							return SCE_LOCATION_ERROR_FATAL_ERROR;
						}
					} else if (m_pluginState == 1) {
						Plugin *plugin = Plugin::Find("location_plugin");

						if (plugin != NULL) {
							LocationPluginInterface *iface =
								(LocationPluginInterface *)
									plugin->GetInterface(1);

							if (iface != NULL)
								return iface->Show(m_pluginDoneCb,
										   entry);
						}
					}
				}
			}
		}
	}
	return ret;
}

/* (0x81188BB6) */
SceLocationDialogResult LocationConfirmMgr::GetResult(int appId, SceLocationHandle handle)
{
	if (appId < 0)
		return SCE_LOCATION_DIALOG_RESULT_NONE;

	signed char i = 0;

	for (; i < 8; i++) {
		if (m_entries[i].appId == appId && m_entries[i].handle == handle)
			break;
	}
	if (i >= 8)
		return SCE_LOCATION_DIALOG_RESULT_NONE;

	if (m_pluginState == 1) {
		Plugin *plugin = Plugin::Find("location_plugin");

		if (plugin != NULL) {
			LocationPluginInterface *iface =
				(LocationPluginInterface *)plugin->GetInterface(1);

			if (iface != NULL)
				m_entries[i].result = (SceLocationDialogResult)iface->GetResult();
		}
	}
	return m_entries[i].result;
}

/* (0x81188C26) */
SceLocationDialogStatus LocationConfirmMgr::GetStatus(int appId, SceLocationHandle handle)
{
	if (appId < 0)
		return SCE_LOCATION_DIALOG_STATUS_IDLE;

	signed char i = 0;

	for (; i < 8; i++) {
		if (m_entries[i].appId == appId && m_entries[i].handle == handle)
			break;
	}
	if (i >= 8)
		return SCE_LOCATION_DIALOG_STATUS_IDLE;

	if (m_pluginState == 1) {
		Plugin *plugin = Plugin::Find("location_plugin");

		if (plugin != NULL) {
			LocationPluginInterface *iface =
				(LocationPluginInterface *)plugin->GetInterface(1);

			if (iface != NULL)
				m_entries[i].status = (SceLocationDialogStatus)iface->GetStatus();
		}
	}
	return m_entries[i].status;
}

/* (0x81188C98) */
int LocationConfirmMgr::Abort(int appId, SceLocationHandle handle)
{
	int ret = 0;

	m_lastResult = 0;
	if (appId < 0)
		return ret;

	signed char i = 0;

	for (; i < 8; i++) {
		if (m_entries[i].appId == appId && m_entries[i].handle == handle)
			break;
	}
	if (i >= 8)
		return ret;

	m_entries[i].abortFlag = 1;
	if (m_pluginState == 1) {
		Plugin *plugin = Plugin::Find("location_plugin");

		if (plugin != NULL) {
			LocationPluginInterface *iface =
				(LocationPluginInterface *)plugin->GetInterface(1);

			if (iface != NULL)
				ret = iface->Abort();
		}
	} else if (m_pluginState == 0) {
		ret = locationPluginUnload();
		m_pluginState = -1;
	}
	m_entries[i].status = SCE_LOCATION_DIALOG_STATUS_IDLE;
	m_entries[i].result = SCE_LOCATION_DIALOG_RESULT_NONE;
	return ret;
}

/* (0x81188D26) */
int LocationConfirmMgr::RemoveEntry(int appId, SceLocationHandle handle)
{
	for (signed char i = 0; i < 8; i++) {
		if (m_entries[i].appId == appId && m_entries[i].handle == handle) {
			sce_paf_memset(&m_entries[i], 0, sizeof(m_entries[i]));
			m_entries[i].appId = -1;
			m_entryCount--;
			break;
		}
	}

	for (signed char i = 0; i < 8; i++) {
		if (m_apps[i].appId == appId) {
			signed char j = 0;

			for (; j < 8; j++) {
				if (m_apps[i].handles[j] == handle) {
					m_apps[i].handles[j] = (SceLocationHandle)-1;
					m_apps[i].handleCount--;
					if (m_apps[i].activeHandle == handle)
						m_apps[i].activeHandle = (SceLocationHandle)-1;
					if (m_apps[i].handleCount == 0)
						m_apps[i].appId = -1;
					break;
				}
			}
			if (j < 8)
				break;
		}
	}
	return -1;
}

/* (0x81188DF6) */
int LocationConfirmMgr::CloseApp(int appId, SceLocationHandle handle)
{
	int ret = 0;

	m_lastResult = 0;
	if (appId < 0)
		return ret;

	signed char i = 0;

	for (; i < 8; i++) {
		if (m_entries[i].appId == appId && m_entries[i].handle == handle)
			break;
	}
	if (i >= 8)
		return ret;

	if (m_entryCount == 1) {
		m_activeAppId = -1;

		Plugin *plugin = Plugin::Find("location_plugin");
		LocationPluginInterface *iface =
			(plugin != NULL) ? (LocationPluginInterface *)plugin->GetInterface(1)
					 : NULL;

		if (iface != NULL) {
			ret = iface->Close();
		} else if (m_pluginState == 0) {
			ret = locationPluginUnload();
			m_pluginState = -1;
		}
	}
	RemoveEntry(appId, handle);
	return ret;
}

/* (0x81188E90) */
static void locationConfirmOnDialogNotify(int appId, int handle, int dialogType, int result,
					  int canceled)
{
	s_confirmMgr->m_task.m_handle = handle;
	s_confirmMgr->m_task.m_dialogType = dialogType;
	s_confirmMgr->m_task.m_appId = appId;
	s_confirmMgr->m_task.m_result = result;
	s_confirmMgr->m_task.m_canceled = canceled;
	locationTaskSchedule(&s_confirmMgr->m_task, 1);
}
