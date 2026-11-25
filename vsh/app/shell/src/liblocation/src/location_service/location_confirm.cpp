/*
 * location_confirm.cpp: location permission confirmation dialog manager ("location_plugin"
 * integration, per-app dialog queue, and app focus suspend/resume; 0x81187F14-0x81188F3E).
 * The file's and internal names here are ours (PLACEHOLDER).
 */
#include <std/string.h>
#include <std/new>
#include <plugin.h>
#include <system/system.h>
#include <datetime/datetime.h>
#include "../../../../shell.h"
#include "../../../../plugins.h"
#include "location_internal.h"

using namespace paf;

/* shell location_plugin loader (0x813C4C68, 0x813C4E96) */
int locationPluginLoad(LocationPluginLoadedFn loadedCb, LocationPluginDoneFn doneCb,
		       void *arg);				/* (0x813C4C68) */
int locationPluginUnload(void);					/* (0x813C4E96) */

struct LocationPluginInterface {
	int (*Open)(void);
	int (*Dismiss)(void);
	int (*ShowDialog)(void);
	int (*Show)(LocationPluginDoneFn doneCb, void *entry);
	int (*GetResult)(void);
	int (*GetStatus)(void);
	int (*Abort)(void);
	int (*Cancel)(void);
	int (*Suspend)(void);
	int (*Close)(void);
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
	s_confirmMgr->m_callback(m_entry.appId, m_entry.handle, m_entry.dialogType,
				 m_entry.result, m_canceled);
	return 0;
}

/* (0x81187F4E) */
__attribute__((noinline))
LocationConfirmMgr::LocationConfirmMgr(LocationConfirmCallback cb)
{
	m_entryCount = 0;
	m_task.m_mgr = NULL;
	m_task.m_slotIndex = 0;
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
	g_locationManager->RegisterTask(&m_task);
}

/* (0x81187FEE) */
LocationConfirmMgr::~LocationConfirmMgr()
{
	if (m_task.m_mgr != NULL) {
		m_task.Cancel();
		g_locationManager->UnregisterTask(&m_task);
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
				s_confirmMgr->m_task.m_entry.result = entry->result;
				s_confirmMgr->m_task.m_entry.appId = entry->appId;
				s_confirmMgr->m_task.m_entry.handle = entry->handle;
				s_confirmMgr->m_task.m_entry.dialogType = entry->dialogType;
				s_confirmMgr->m_task.m_canceled = 0;
				s_confirmMgr->m_task.Schedule(1);
			}
		}
	} else {
		if (entry->step == 11) {
			s_confirmMgr->m_task.m_entry.result = entry->result;
			s_confirmMgr->m_task.m_entry.appId = entry->appId;
			s_confirmMgr->m_task.m_entry.handle = entry->handle;
			s_confirmMgr->m_task.m_entry.dialogType = entry->dialogType;
			s_confirmMgr->m_task.m_canceled = 0;
			s_confirmMgr->m_task.Schedule(1);
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

		s_confirmMgr->m_context->GetPermission(entry->appId, entry->handle, &permInfo);
		locationSvcGetGlobalPerm(&globalPerm);
		entry->globalPerm = globalPerm;
		locationSvcGetAppPermSetting(&appPerm, false);
		entry->appPerm = appPerm;
		entry->permInfo = permInfo;

		if (system::Model::GetType() == 1) {
			step = 1;
			entry->step = 1;
		} else if (permInfo.applicationstatus == SCE_LOCATION_PERMISSION_APPLICATION_INIT &&
			   entry->skipInitialPrompt == 0) {
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

			m_context->GetPermission(appId, entry->handle, &permInfo);
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

	m_context->GetPermission(appId, entry->handle, &permInfo);
	entry->permInfo = permInfo;
	locationSvcGetGlobalPerm(&globalPerm);
	locationSvcGetAppPermSetting(&appPerm, false);

	int step;

	if (system::Model::GetType() == 1) {
		step = 1;
		entry->step = 1;
	} else if (permInfo.applicationstatus == SCE_LOCATION_PERMISSION_APPLICATION_INIT &&
		   entry->skipInitialPrompt == 0) {
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
		s_confirmMgr->m_task.m_entry.result = SCE_LOCATION_DIALOG_STATUS_ENABLE;
		s_confirmMgr->m_task.m_entry.appId = entry->appId;
		s_confirmMgr->m_task.m_entry.handle = entry->handle;
		s_confirmMgr->m_task.m_entry.dialogType = 0;
		s_confirmMgr->m_task.m_canceled = 0;
		s_confirmMgr->m_task.Schedule(1);
	} else if (locationPluginLoad(locationConfirmOnPluginLoaded, locationConfirmOnPluginDone,
				      entry) != 0) {
		entry->step = 0;
		entry->status = SCE_LOCATION_DIALOG_STATUS_IDLE;
		entry->result = SCE_LOCATION_DIALOG_RESULT_NONE;
		entry->dialogType = 0;
		s_confirmMgr->m_task.m_entry.result = SCE_LOCATION_DIALOG_RESULT_NONE;
		s_confirmMgr->m_task.m_entry.appId = entry->appId;
		s_confirmMgr->m_task.m_entry.handle = entry->handle;
		s_confirmMgr->m_task.m_entry.dialogType = 0;
		s_confirmMgr->m_task.m_canceled = 0;
		s_confirmMgr->m_task.Schedule(1);
	} else {
		m_pluginState = 0;
	}
}

/* (0x811887B6) */
static void locationConfirmOnAppEvent(int event, const void *data, SceSize size, void *arg)
{
	int appId = ((const ShellAppEvent *)data)->appId;

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
		entry->skipInitialPrompt = 0;
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
		unsigned char pending = 0;

		m_context->IsHandlePending(handle, &pending);
		if (pending == 0 && !allowBusy)
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

		ret = locationSvcGetGlobalPerm(&globalPerm);
		if (ret == 0) {
			ret = locationSvcGetAppPermSetting(&appPerm, false);
			if (ret == 0) {
				ret = m_context->GetPermission(appId, handle, &permInfo);
				if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
					LocationConfirmEntry *entry = &m_entries[idx];

					entry->status = SCE_LOCATION_DIALOG_STATUS_RUNNING;
					entry->result = SCE_LOCATION_DIALOG_RESULT_NONE;
					entry->step = 0;
					entry->abortFlag = 0;
					entry->shownPrompt = 0;
					entry->skipInitialPrompt = 0;
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
	s_confirmMgr->m_task.m_entry.handle = handle;
	s_confirmMgr->m_task.m_entry.dialogType = dialogType;
	s_confirmMgr->m_task.m_entry.appId = appId;
	s_confirmMgr->m_task.m_entry.result = (SceLocationDialogResult)result;
	s_confirmMgr->m_task.m_canceled = canceled;
	s_confirmMgr->m_task.Schedule(1);
}
