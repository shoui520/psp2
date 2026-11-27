/*
 * location_setting.cpp: registry & LSDB settings manager ("SceLocationSetting") and
 * registry watcher ("SceLocationRegWatcher") (0x8118F574-0x81190FCE, sti 0x81190F5E).
 * The file and class names are ours (PLACEHOLDER).
 */
#include "location_internal.h"
#include "../../../../plugins.h"
#include <libnetctl.h>
#include <appmgr/appmgr.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include <std/string.h>
#include <system/system.h>
#include <plugin.h>

extern "C" {
int SceLsdb_C36501F7(const paf::string *titleId, unsigned int key, const LsdbValue *value);
int sceWlanGetConfiguration(void);
int sceNetCtlAdhocGetState(int *state);
int vshSblQafMgrIsAllowLiblocationChangeModel(void);
int sceRegMgrRegisterCallback(SceUID cbid);
int sceRegMgrUnregisterCallback(SceUID cbid);
int sceRegMgrAddRegistryCallback(SceUID cbid, const char *path);
int sceRegMgrStartCallback(SceUID cbid);
}

extern void shellRegMgrLogError(int err, int flag);		/* 0x81345038 */
extern int consoleInfoFileWriteInt(int index, SceUInt32 value, int mode); /* 0x81376B3C */

/* 0x8141F548 */
LocationMethodState *LocationSetting::GetMethodState()
{
	return &m_methodState;
}

/* 0x810D3B64 */
int LocationSetting::GetGpsTimeoutMs()
{
	return m_gpsTimeoutMs;
}

/* 0x8118F576 */
int LocationRegWatcher::Stop()
{
	int ret = 0;
	if (m_state == 1)
		m_state = 2;
	if (m_cbId >= 0) {
		ret = sceRegMgrUnregisterCallback(m_cbId);
		int dret = sceKernelDeleteCallback(m_cbId);
		if (dret < 0)
			return dret;
	}
	return ret;
}

LocationRegWatcher::~LocationRegWatcher()
{
	Stop();
}

/* 0x8118F5A8 */
int LocationRegWatcher::OnNotify()
{
	if (m_state == 0) {
		SceUID cbId = sceKernelCreateCallback("SceLocationRegWatcher", 0,
						      RegCallback, this);
		int ret = cbId;
		if (cbId >= 0) {
			m_cbId = cbId;
			ret = sceRegMgrRegisterCallback(cbId);
			if (ret >= 0) {
				ret = sceRegMgrAddRegistryCallback(
					m_cbId, "/CONFIG/LOCATION/debug_permission_status");
				if (ret >= 0) {
					ret = sceRegMgrStartCallback(m_cbId);
					if (ret >= 0) {
						if (m_state == 0) {
							m_state = 1;
							ret = 0;
						} else {
							ret = SCE_LOCATION_ERROR_FATAL_ERROR;
						}
					}
				}
				if (ret < 0) {
					int uret = sceRegMgrUnregisterCallback(m_cbId);
					if (uret < 0)
						ret = uret;
				}
			}
			if (ret < 0) {
				int dret = sceKernelDeleteCallback(m_cbId);
				if (dret >= 0)
					m_cbId = -1;
				else
					ret = dret;
			}
		}
		if (ret < 0)
			m_state = 3;
	} else if (m_state == 1) {
		sceKernelCheckCallback();
	}
	return 1;
}

/* 0x8118F65C */
int LocationSetting::GetDebugPermStatus()
{
	return m_debugPermStatus;
}

/* 0x8118F662 */
int LocationRegWatcher::HandleRegChange()
{
	int ret = 0;
	if (m_setting->m_debugPermEnabled != 0) {
		int prev = m_setting->m_debugPermStatus;
		int val = 0;
		ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "debug_permission_status", &val);
		if (ret != 0) {
			m_setting->m_debugPermStatus = 0;
			shellRegMgrLogError(ret, 1);
		} else {
			m_setting->m_debugPermStatus = val;
		}
		if (ret == 0) {
			int cur = m_setting->m_debugPermStatus;
			if (cur != prev) {
				int lastCnt = 0;
				ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION",
							 "last_update_counter", &lastCnt);
				if (ret == 0) {
					ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION",
								 "last_update_counter",
								 lastCnt & ~2);
				}
				if (ret != 0)
					shellRegMgrLogError(ret, 1);
				locationGetContext()->ResetAllPerms(cur);
			}
		}
	}
	return ret;
}

/* 0x8118F73A */
void LocationRegWatcher::OnResume()
{
	HandleRegChange();
}

/* 0x8118F742 */
int LocationRegWatcher::RegCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	if (common != NULL)
		((LocationRegWatcher *)common)->HandleRegChange();
	return 0;
}

/* 0x8118F750 */
LocationSetting::LocationSetting()
{
	SceLsdb_A6D8772C(&m_lsdbClient);
	m_hasTelephony = 0;
	m_debugPermEnabled = 0;
	m_isDevMode = 0;
	m_regWatcher.m_mgr = NULL;
	m_regWatcher.m_slotIndex = 0;
	m_regWatcher.m_setting = this;
	m_regWatcher.m_state = 0;
	m_regWatcher.m_cbId = -1;
	m_methodState.m_flags = 0;
	m_methodState.m_lmethod = 0;
	m_methodState.m_hmethod = 0;
	m_mutexResult = -1;
	m_enableService = 0;
	m_enableParental = 0;
	m_enableLocale = 0;
	m_updateCounter = 0;
	m_debugPermStatus = 0;
	m_agePermit = 1;
	m_gpsTimeoutMs = 120000;
	m_flyingStartInterval = 14400000000ULL;
}

/* 0x8118F7DC / 0x8118F86E */
LocationSetting::~LocationSetting()
{
	if (m_regWatcher.m_mgr != NULL)
		g_locationManager->UnregisterTask(&m_regWatcher);
	if (m_mutexResult == 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}
	SceLsdb_E2101B03(&m_lsdbClient);
}

/* 0x8118F880 */
int LocationSetting::CheckLocaleChanged()
{
	int val = 0;
	int prevLocale = 0;
	int ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "enable_locale_last", &val);
	if (ret != 0)
		shellRegMgrLogError(ret, 1);
	else
		prevLocale = (val != 0);

	if (ret == 0 && prevLocale != m_enableLocale) {
		ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION", "enable_locale_last",
					 m_enableLocale);
		if (ret != 0)
			shellRegMgrLogError(ret, 1);
		if (ret == 0) {
			m_updateCounter = (m_updateCounter + 1) & 0x3FFFFFFF;
			ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION", "update_counter",
						 m_updateCounter);
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
		}
	}
	return ret;
}

/* 0x8118F92E */
int LocationSetting::LoadRegistry()
{
	int ret = vshSblSsIsDevelopmentMode();
	if (ret >= 0) {
		m_isDevMode = (ret == 1) ? 1 : 0;
		ret = 0;
	}
	if (ret == 0) {
		int val = 0;
		ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "enable_service", &val);
		if (ret != 0)
			shellRegMgrLogError(ret, 1);
		else
			m_enableService = val;
		if (ret == 0) {
			ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "enable_parental", &val);
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
			else
				m_enableParental = val;
			if (ret == 0) {
				ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "update_counter",
							 &m_updateCounter);
				if (ret != 0) {
					m_updateCounter = 0;
					shellRegMgrLogError(ret, 1);
				}
				if (ret == 0) {
					ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION",
								 "enable_locale", &val);
					if (ret != 0)
						shellRegMgrLogError(ret, 1);
					else
						m_enableLocale = (val != 0);
				}
			}
		}
	}
	return ret;
}

/* 0x8118FA24 */
int LocationSetting::Init()
{
	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, "SceLocationSetting", 2, 0, NULL);
	if (m_mutexResult != 0)
		return m_mutexResult;

	int ret = SceLsdb_E90B81CD(&m_lsdbClient);
	if (ret == 0) {
		ret = LoadRegistry();
		if (ret == 0)
			ret = CheckLocaleChanged();
	}

	if (vshSblAimgrIsDEX() != 0 || vshSblAimgrIsTool() != 0) {
		ret = g_locationManager->RegisterTask(&m_regWatcher);
		if (ret == 0) {
			ret = m_regWatcher.Schedule(0);
			if (ret != 0)
				g_locationManager->UnregisterTask(&m_regWatcher);
		}
	}

	paf::Plugin *plugin = paf::Plugin::Find("telephony_plugin");
	TelephonyPluginInterface *iface = NULL;
	if (plugin != NULL)
		iface = (TelephonyPluginInterface *)plugin->GetInterface(1);
	bool hasTel = false;
	if (iface != NULL) {
		TelephonyModem *modem = iface->getModem();
		if (modem != NULL)
			hasTel = (modem->vtable->needsInitialCheck(modem) != 0);
	}
	m_hasTelephony = hasTel ? 1 : 0;

	if (ret < 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}
	return ret;
}

/* 0x8118FB08 */
int LocationSetting::GetAppTitleId(int appId, char *outTitleId)
{
	SceAppMgrAppStatus status;
	int ret = sceAppMgrGetStatusByAppId(appId, &status);
	if (ret == 0) {
		outTitleId[31] = '\0';
		if (status.launchOrigin == 1) {
			sce_paf_strncpy(outTitleId, "NPXS10998", 31);
		} else if (status.launchOrigin == 2) {
			sce_paf_strncpy(outTitleId, "NPXS10999", 31);
		} else {
			sce_paf_strncpy(outTitleId, status.appName, 31);
			LsdbAppInfoPtr info(new LsdbAppInfo());
			paf::string titleStr(outTitleId);
			ret = SceLsdb_D239031A(&m_lsdbClient, &titleStr, &info);
			if (ret != 0) {
				if (ret == (int)LSDB_ERROR_NOT_FOUND)
					ret = SCE_LOCATION_ERROR_INVALID_TITLE_ID;
			} else {
				LsdbAppInfo::iterator it = info->find(0xCEFB6EFDU);
				if (it != info->end()) {
					const char *s = "";
					if (it->second.type == 1 && it->second.text != NULL)
						s = it->second.text->c_str();
					sce_paf_strncpy(outTitleId, s, 31);
				}
			}
		}
	}
	return ret;
}

/* 0x8118FD0C */
int LocationSetting::GetParental()
{
	return m_enableParental;
}

/* 0x8118FD12 */
int LocationSetting::SetParental(int val)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		m_enableParental = val;
		ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION", "enable_parental", val);
		if (ret != 0)
			shellRegMgrLogError(ret, 1);
		if (ret == 0) {
			m_updateCounter = (m_updateCounter + 1) & 0x3FFFFFFF;
			ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION", "update_counter",
						 m_updateCounter);
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
		}
		sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return ret;
}

/* 0x8118FD8A */
bool LocationSetting::IsServiceEnabled()
{
	if (paf::system::Model::GetType() == 0 && m_enableService == 1)
		return m_enableLocale == 1;
	return false;
}

/* 0x8118FDAA */
int LocationSetting::SetServiceEnabled(int val)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		m_enableService = val;
		ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION", "enable_service", val);
		if (ret != 0)
			shellRegMgrLogError(ret, 1);
		if (ret == 0) {
			m_updateCounter = (m_updateCounter + 1) & 0x3FFFFFFF;
			ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION", "update_counter",
						 m_updateCounter);
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
		}
		sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return ret;
}

/* 0x8118FE22 */
int LocationSetting::IsDebugApp(const char *titleId)
{
	if (sce_paf_strncmp(titleId, "NPXS10999", 32) == 0 ||
	    sce_paf_strncmp(titleId, "NPXS10998", 32) == 0)
		return 1;
	return 0;
}

/* 0x8118FE56 */
int LocationSetting::GetAppPermission(const char *titleId, int *outPerm, unsigned char *outFlag)
{
	if (vshSblAimgrIsDEX() != 0 || vshSblAimgrIsTool() != 0) {
		if (sce_paf_strncmp(titleId, "NPXS10999", 32) == 0 ||
		    sce_paf_strncmp(titleId, "NPXS10998", 32) == 0 ||
		    m_debugPermEnabled != 0) {
			*outPerm = m_debugPermStatus;
			int lastCnt = 0;
			int ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION",
						     "last_update_counter", &lastCnt);
			int bitSet = 1;
			if (ret != 0) {
				shellRegMgrLogError(ret, 1);
			} else {
				bitSet = (lastCnt & 2) ? 1 : 0;
			}
			*outFlag = (bitSet == 0) ? 1 : 0;
			return ret;
		}
	} else {
		if (sce_paf_strncmp(titleId, "NPXS10999", 32) == 0 ||
		    sce_paf_strncmp(titleId, "NPXS10998", 32) == 0) {
			*outPerm = 0;
			return 0;
		}
	}

	LsdbAppInfoPtr info(new LsdbAppInfo());
	paf::string titleStr(titleId);
	int ret = SceLsdb_D239031A(&m_lsdbClient, &titleStr, &info);
	if (ret != 0) {
		if (ret == (int)LSDB_ERROR_NOT_FOUND)
			ret = SCE_LOCATION_ERROR_INVALID_TITLE_ID;
	} else {
		LsdbAppInfo::iterator it = info->find(0xA9492ECAU);
		if (it == info->end()) {
			*outPerm = 0;
		} else {
			SceInt64 v = 0;
			if (it->second.type == 0)
				v = it->second.integer;
			*outPerm = (int)(v & 3);
			*outFlag = ((v & 0x80000000ULL) == 0) ? 1 : 0;
		}
	}
	return ret;
}

/* 0x81190136 */
int LocationSetting::UpdateLsdbLiblocationAttr(const char *titleId, SceUInt64 mask, SceUInt64 value)
{
	LsdbAppInfoPtr info(new LsdbAppInfo());
	paf::string titleStr(titleId);
	int ret = SceLsdb_D239031A(&m_lsdbClient, &titleStr, &info);
	if (ret != 0) {
		if (ret == (int)LSDB_ERROR_NOT_FOUND)
			ret = SCE_LOCATION_ERROR_INVALID_TITLE_ID;
	} else {
		LsdbAppInfo::iterator it = info->find(0xA9492ECAU);
		if (it == info->end()) {
			ret = SCE_LOCATION_ERROR_INVALID_TITLE_ID;
		} else {
			SceUInt64 cur = (value & mask);
			if (it->second.type == 0)
				cur |= ((SceUInt64)it->second.integer & ~mask);
			paf::string keyTitle(titleId);
			LsdbValue val;
			val.type = 0;
			val.integer = (SceInt64)cur;
			ret = SceLsdb_C36501F7(&keyTitle, 0xA9492ECAU, &val);
		}
	}
	return ret;
}

/* 0x811903C4 */
int LocationSetting::SetAppPermission(const char *titleId, int perm)
{
	if (vshSblAimgrIsDEX() == 0 && vshSblAimgrIsTool() == 0) {
		if (sce_paf_strncmp(titleId, "NPXS10999", 32) == 0 ||
		    sce_paf_strncmp(titleId, "NPXS10998", 32) == 0)
			return SCE_LOCATION_ERROR_DISABLE_APPLICATION;
	} else if (sce_paf_strncmp(titleId, "NPXS10999", 32) == 0 ||
		   sce_paf_strncmp(titleId, "NPXS10998", 32) == 0 ||
		   m_debugPermEnabled != 0) {
		int ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION",
					     "debug_permission_status", perm);
		if (ret != 0)
			shellRegMgrLogError(ret, 1);
		if (ret == 0) {
			m_debugPermStatus = perm;
			int lastCnt = 0;
			ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION",
						 "last_update_counter", &lastCnt);
			if (ret == 0) {
				ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION",
							 "last_update_counter",
							 lastCnt & ~2);
			}
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
		}
		return ret;
	}
	return UpdateLsdbLiblocationAttr(titleId, 0xFFFFFFFFULL, (SceUInt64)(SceUInt32)perm);
}

/* 0x811904FA */
int LocationSetting::SetAppLastCounter(const char *titleId, int counter)
{
	if (vshSblAimgrIsDEX() == 0 && vshSblAimgrIsTool() == 0) {
		if (sce_paf_strncmp(titleId, "NPXS10999", 32) == 0 ||
		    sce_paf_strncmp(titleId, "NPXS10998", 32) == 0)
			return SCE_LOCATION_ERROR_DISABLE_APPLICATION;
	} else if (sce_paf_strncmp(titleId, "NPXS10999", 32) == 0 ||
		   sce_paf_strncmp(titleId, "NPXS10998", 32) == 0 ||
		   m_debugPermEnabled != 0) {
		int ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION", "last_update_counter",
					     counter * 4);
		if (ret != 0)
			shellRegMgrLogError(ret, 1);
		if (ret == 0) {
			int lastCnt = 0;
			ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION",
						 "last_update_counter", &lastCnt);
			if (ret == 0) {
				ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION",
							 "last_update_counter",
							 lastCnt | 2);
			}
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
		}
		return ret;
	}
	SceUInt64 val = 0x80000000ULL | ((SceUInt64)(SceUInt32)(counter * 4) << 32);
	return UpdateLsdbLiblocationAttr(titleId, 0xFFFFFFFC80000000ULL, val);
}

/* 0x8119062A */
int LocationSetting::GetAppLastCounter(const char *titleId, SceUInt32 *outCounter)
{
	if (vshSblAimgrIsDEX() != 0 || vshSblAimgrIsTool() != 0) {
		if (sce_paf_strncmp(titleId, "NPXS10999", 32) == 0 ||
		    sce_paf_strncmp(titleId, "NPXS10998", 32) == 0 ||
		    m_debugPermEnabled != 0) {
			SceUInt32 lastCnt = 0;
			int ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION",
						     "last_update_counter",
						     (int *)&lastCnt);
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
			else
				*outCounter = lastCnt >> 2;
			return ret;
		}
	} else {
		if (sce_paf_strncmp(titleId, "NPXS10999", 32) == 0 ||
		    sce_paf_strncmp(titleId, "NPXS10998", 32) == 0)
			return SCE_LOCATION_ERROR_INVALID_TITLE_ID;
	}

	LsdbAppInfoPtr info(new LsdbAppInfo());
	paf::string titleStr(titleId);
	int ret = SceLsdb_D239031A(&m_lsdbClient, &titleStr, &info);
	if (ret != 0) {
		if (ret == (int)LSDB_ERROR_NOT_FOUND)
			ret = SCE_LOCATION_ERROR_INVALID_TITLE_ID;
	} else {
		LsdbAppInfo::iterator it = info->find(0xA9492ECAU);
		if (it == info->end()) {
			ret = SCE_LOCATION_ERROR_INVALID_TITLE_ID;
		} else {
			SceUInt32 hi = 0;
			if (it->second.type == 0)
				hi = (SceUInt32)((SceUInt64)it->second.integer >> 32);
			*outCounter = hi >> 2;
		}
	}
	return ret;
}

/* 0x811908AA */
int LocationSetting::LoadDebugPermission()
{
	int ret = 0;
	if (vshSblAimgrIsDEX() != 0 || vshSblAimgrIsTool() != 0) {
		int val = 0;
		ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "debug_permission", &val);
		m_debugPermEnabled = (ret == 0 && val == 1) ? 1 : 0;
		if (ret != 0)
			shellRegMgrLogError(ret, 1);
	} else {
		m_debugPermEnabled = 0;
	}

	if (ret == 0) {
		int val = 0;
		ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "debug_permission_status", &val);
		if (ret != 0) {
			m_debugPermStatus = 0;
			shellRegMgrLogError(ret, 1);
		} else {
			m_debugPermStatus = val;
		}
	}
	return ret;
}

/* 0x81190950 */
int LocationSetting::GetUpdateCounter()
{
	return m_updateCounter;
}

static int getEmulationMode(int *outMode)
{
	int ret = 0;
	if (vshSblAimgrIsDEX() != 0 || vshSblAimgrIsTool() != 0) {
		ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "emulation_mode", outMode);
		if (ret != 0)
			return ret;
		if (vshSblQafMgrIsAllowLiblocationChangeModel() == 0 && *outMode == 2)
			*outMode = 0;
	} else {
		if (vshSblQafMgrIsAllowLiblocationChangeModel() == 0) {
			*outMode = 0;
			return 0;
		}
		ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "emulation_mode", outMode);
	}
	return ret;
}

/* 0x81190954 */
int LocationSetting::HasGps()
{
	int mode = 0;
	int ret = getEmulationMode(&mode);
	if (ret != 0) {
		shellRegMgrLogError(ret, 1);
		return 0;
	}
	if (mode != 2)
		return m_hasTelephony;
	return 0;
}

/* 0x811909F2 */
int LocationSetting::IsEmulationMode()
{
	int mode = 0;
	int ret = getEmulationMode(&mode);
	if (ret != 0) {
		shellRegMgrLogError(ret, 1);
		return 0;
	}
	return (mode == 1) ? 1 : 0;
}

/* 0x81190A8C */
int LocationSetting::IsDeviceEnabled()
{
	int mode = 0;
	int ret = getEmulationMode(&mode);
	if (ret != 0) {
		shellRegMgrLogError(ret, 1);
	} else if (mode == 1) {
		return 1;
	}
	if ((m_methodState.m_lmethod & 0xF) == 0xF)
		return 1;
	return 0;
}

/* 0x81190B32 */
int LocationSetting::IsWlanOrGpsAvailable()
{
	if (HasGps() == 0)
		return sceWlanGetConfiguration() & 1;
	return 1;
}

/* 0x81190B4C */
int LocationSetting::IsApInvisible()
{
	if (vshSblAimgrIsDEX() != 0 || vshSblAimgrIsTool() != 0) {
		int val = 0;
		int ret = sceRegMgrGetKeyInt("/CONFIG/LOCATION", "make_ap_invisible", &val);
		if (ret != 0)
			shellRegMgrLogError(ret, 1);
		else if (val == 1)
			return 1;
	}
	return 0;
}

/* 0x81190BAC */
int LocationSetting::GetValue(SceUInt32 *outVal)
{
	*outVal = 1;
	if ((sceWlanGetConfiguration() & 1) != 0)
		*outVal |= 2U;

	int mode = 0;
	int ret = getEmulationMode(&mode);
	if (ret != 0) {
		shellRegMgrLogError(ret, 1);
	} else if (mode == 1) {
		*outVal |= 0xCU;
		return 0;
	}

	if (HasGps() != 0)
		*outVal |= 0xCU;
	return 0;
}

/* 0x81190C76 */
int LocationSetting::GetUserAge(int *outAge)
{
	int yob = 0, mob = 0, dob = 0;
	int ret = sceRegMgrGetKeyInt("/CONFIG/NP", "yob", &yob);
	if (ret != 0)
		shellRegMgrLogError(ret, 1);
	if (ret == 0) {
		ret = sceRegMgrGetKeyInt("/CONFIG/NP", "mob", &mob);
		if (ret != 0)
			shellRegMgrLogError(ret, 1);
		if (ret == 0) {
			ret = sceRegMgrGetKeyInt("/CONFIG/NP", "dob", &dob);
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
			if (ret == 0) {
				SceRtcTick utc, local;
				SceDateTime dt;
				int age = locationGetCurrentTick(&utc);
				if (age >= 0) {
					age = sceRtcConvertUtcToLocalTime(&utc, &local);
					if (age == 0) {
						age = sceRtcSetTick(&dt, &local);
						if (age == 0) {
							int curDate = 10000 * dt.year + 100 * dt.month + dt.day;
							int birthDate = 10000 * yob + 100 * mob + dob;
							age = (birthDate <= curDate) ? (curDate - birthDate) / 10000 : 0;
						}
					}
				}
				*outAge = age;
				if (age < 0)
					return age;
			}
		}
	}
	return ret;
}

/* 0x81190D8A */
int LocationSetting::ReloadAgePermit()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		int prevVal = 0, permitAge = 0, userAge = 0;
		sceRegMgrGetKeyInt("/CONFIG/LOCATION", "age_permit_state", &prevVal);
		int prevPermit = (prevVal != 0);
		sceRegMgrGetKeyInt("/CONFIG/LOCATION", "permit_age", &permitAge);
		ret = GetUserAge(&userAge);
		if (ret == 0)
			m_agePermit = (userAge >= permitAge) ? 1 : 0;
		if (ret == 0 && prevPermit != m_agePermit) {
			sceRegMgrSetKeyInt("/CONFIG/LOCATION", "age_permit_state", m_agePermit);
			m_updateCounter = (m_updateCounter + 1) & 0x3FFFFFFF;
			ret = sceRegMgrSetKeyInt("/CONFIG/LOCATION", "update_counter",
						 m_updateCounter);
			if (ret != 0)
				shellRegMgrLogError(ret, 1);
		}
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x81190E70 */
bool LocationSetting::IsAdhocAllowed()
{
	int state = 0;
	if (sceNetCtlAdhocGetState(&state) == 0) {
		if (state == 0)
			return true;
		PositionManager *pm = locationGetContext();
		if (pm != NULL) {
			LocationPrivacySettings *permMgr = pm->GetPermMgr();
			if (permMgr != NULL && permMgr->m_count != 0)
				return pm->PollActive() == 0;
		}
	}
	return false;
}

/* 0x81190ED0 */
int LocationSetting::CountUp(int id, unsigned int delta)
{
	return shellUsageCounterAdd(id, delta, 0);
}

/* 0x81190EDE */
int LocationSetting::SetTelemetryValue(int id, int value)
{
	return consoleInfoFileWriteInt(id, (SceUInt32)value, 0);
}

/* 0x81190EEC */
void LocationSetting::GetMethod(SceUInt32 *lmethod, SceUInt32 *hmethod)
{
	if (lmethod != NULL)
		*lmethod = m_methodState.m_lmethod;
	if (hmethod != NULL)
		*hmethod = m_methodState.m_hmethod;
}

/* 0x81190EFA */
int LocationSetting::SetLMethod(const SceUInt32 *lmethod)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		m_methodState.m_lmethod = *lmethod;
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x81190F2C */
int LocationSetting::SetHMethod(const SceUInt32 *hmethod)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		m_methodState.m_hmethod = *hmethod;
		int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}
