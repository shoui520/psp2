/*
 * parental_control_mgr.cpp: SceShell daily play-time parental control manager
 * 3.65-CEX: 0x812327F8 - 0x81233304
 */

#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <plugin.h>
#include <job/job.h>
#include <kernel/threadmgr.h>
#include <appmgr/appmgr.h>
#include <regmgr/regmgr.h>
#include "shell.h"
#include "plugins.h"

extern "C" {
extern int sceKernelCreateCallback(const char *name, SceUInt32 attr,
				   SceKernelCallbackFunction func, void *userData);
extern int sceKernelDeleteCallback(SceUID cbid);
extern int sceKernelPowerLock(SceUInt32 type);
extern int sceKernelPowerUnlock(SceUInt32 type);
extern int sceRegMgrRegisterCallback(SceUID cbid);
extern int sceRegMgrUnregisterCallback(SceUID cbid);
extern int sceRegMgrAddRegistryCallback(SceUID cbid, const char *category);
extern int sceRegMgrStartCallback(SceUID cbid);
extern int sceAppMgrGetStatusByName(const char *appName, SceAppMgrAppStatus *status);
extern int sceAppMgrGetRunningAppIdListForShell(SceInt32 *appIds, int count);
extern int sceAppMgrQuitApp(int appId);
}

extern int musicCorePlayerControl(SceUInt8 command, int param);

namespace {

static const char *const kParentalCategory = "/CONFIG/SECURITY/PARENTAL";
static const char *const kSettingsTitleId = "NPXS10094";

static const int kParentalNotInitializedError = -2136014847;	/* 0x80AF0001 */
static const int kParentalSaveIntervalSec = 900;
static const int kParentalClockJumpThresholdSec = 60;
static const int kParentalWarningLevelCount = 4;
static const int kParentalAppIdActive = -4;
static const SceUInt32 kParentalExemptAttrMask = 0x04000000U;

enum {
	IMPOSE_FUNC_PARENTAL_OPEN		= 21,	/* +0x54: ImposeParentalControl::Open */
	IMPOSE_FUNC_PARENTAL_CLOSE		= 22,	/* +0x58: ImposeParentalControl::Close */
	IMPOSE_FUNC_PARENTAL_SHOW_NOTICE	= 23,	/* +0x5C: ImposeParentalControl::ShowNotice */
	TOPMENU_FUNC_DISMISS_APP_IF_ACTIVE	= 63,	/* +0xFC */
	TOPMENU_FUNC_GET_APP_PARENT_NAME	= 70,	/* +0x118 */
};

static const int kWarningThresholdSec[kParentalWarningLevelCount] = {
	0, 300, 900, 1800
};

static const char *const kRestrictDurationKeys[8] = {
	"restrict_duration_one",
	"restrict_duration_sun",
	"restrict_duration_mon",
	"restrict_duration_tue",
	"restrict_duration_wed",
	"restrict_duration_thu",
	"restrict_duration_fri",
	"restrict_duration_sat"
};

static void imposeParentalClose(void)
{
	paf::Plugin *plugin = paf::Plugin::Find("impose_plugin");
	if (plugin == NULL)
		return;

	ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);
	if (iface != NULL)
		iface->func[IMPOSE_FUNC_PARENTAL_CLOSE]();
}

static void imposeParentalOpen(int limitMinutes)
{
	paf::Plugin *plugin = paf::Plugin::Find("impose_plugin");
	if (plugin == NULL)
		return;

	ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);
	if (iface != NULL)
		((void (*)(int))iface->func[IMPOSE_FUNC_PARENTAL_OPEN])(limitMinutes);
}

static void imposeParentalShowNotice(int remainingMinutes)
{
	paf::Plugin *plugin = paf::Plugin::Find("impose_plugin");
	if (plugin == NULL)
		return;

	ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);
	if (iface != NULL)
		((void (*)(int))iface->func[IMPOSE_FUNC_PARENTAL_SHOW_NOTICE])(remainingMinutes);
}

/* 0x8123320E..0x81233290 */
class ParentalControlSaveJob : public paf::job::JobItem {
public:
	ParentalControlSaveJob(int remainingDate, int remainingDuration)
		: paf::job::JobItem(NULL, NULL),
		  m_remainingDate(remainingDate),
		  m_remainingDuration(remainingDuration)
	{
	}

	virtual ~ParentalControlSaveJob()
	{
	}

	virtual int Run()
	{
		sceKernelPowerLock(0);
		int ret = sceRegMgrSetKeyInt(kParentalCategory, "remaining_date", m_remainingDate);
		if (ret == 0)
			ret = sceRegMgrSetKeyInt(kParentalCategory, "remaining_duration", m_remainingDuration);
		sceKernelPowerUnlock(0);
		return ret;
	}

	virtual void Finish(int result)
	{
		(void)result;
	}

private:
	int m_remainingDate;
	int m_remainingDuration;
};

} /* namespace */

/* 0x81232868..0x81233178 */
class ParentalControlMgr {
public:
	ParentalControlMgr();
	virtual ~ParentalControlMgr();

	int GetRemainingSec() const { return m_remainingSec; }
	int Update(bool isActive, bool forceSave);
	int Pause();
	int Resume();

private:
	int EnqueueSaveJob(int remainingDate, int elapsedSec);
	void QuitRestrictedApps();
	int CalculateElapsedSec(int *clockSec);
	int LoadDailyLimit();
	void ResetDailyLimitState();

	static int OnRegistryChanged(int notifyId, int notifyCount, int notifyArg, void *common);
	static void OnAppEvent(int code, const void *eventData, SceSize eventSize, void *userArg);

	SceUID m_regCallbackId;
	int m_currentDate;
	int m_lastClockSec;
	int m_sessionStartSec;
	int m_dailyLimitSec;
	int m_accumulatedSec;
	int m_lastSavedSec;
	SceUInt8 m_pauseCount;
	SceUInt8 m_nextWarningLevel;
	int m_remainingSec;
};

static ParentalControlMgr *s_parentalControlMgr = NULL;

/* 0x81232868 */
int ParentalControlMgr::EnqueueSaveJob(int remainingDate, int elapsedSec)
{
	ParentalControlSaveJob *rawJob = new ParentalControlSaveJob(remainingDate, elapsedSec);
	if (rawJob == NULL)
		return SHELL_ERROR_NO_MEMORY;

	paf::common::SharedPtr<paf::job::JobItem> jobRef(rawJob);
	int ret = paf::job::JobQueue::default_queue->Enqueue(jobRef);
	if (ret == 0)
		m_lastSavedSec = elapsedSec;
	return ret;
}

/* 0x81232968 */
void ParentalControlMgr::QuitRestrictedApps()
{
	paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
	if (topmenuPlugin != NULL) {
		ShellPluginInterface *topmenu = (ShellPluginInterface *)topmenuPlugin->GetInterface(1);
		if (topmenu != NULL) {
			SceAppMgrAppStatus appStatus;
			if (sceAppMgrGetStatusByAppId(kParentalAppIdActive, &appStatus) == 0) {
				((void (*)(const char *))topmenu->func[TOPMENU_FUNC_DISMISS_APP_IF_ACTIVE])(
					appStatus.appName);
			}

			SceInt32 runningAppIds[20];
			int runningCount = sceAppMgrGetRunningAppIdListForShell(runningAppIds, 20);
			for (int index = 0; index < runningCount; ++index) {
				if (sceAppMgrGetStatusByAppId(runningAppIds[index], &appStatus) != 0)
					continue;

				if ((appStatus.processAttr & 1U) != 0) {
					SceUInt64 appAttr = 0;
					const char *rawParent =
						((const char *(*)(const char *, SceUInt64 *))
							topmenu->func[TOPMENU_FUNC_GET_APP_PARENT_NAME])(
							appStatus.appName, &appAttr);
					paf::string parentName(rawParent != NULL ? rawParent : "");
					if (!parentName.empty() && (appAttr & kParentalExemptAttrMask) == 0) {
						SceAppMgrAppStatus parentStatus;
						if (sceAppMgrGetStatusByName(parentName.c_str(), &parentStatus) == 0)
							sceAppMgrQuitApp(parentStatus.appId);
						sceAppMgrQuitApp(appStatus.appId);
					}
				} else if ((appStatus.processAttr & 2U) != 0) {
					sceAppMgrQuitApp(appStatus.appId);
				}
			}
		}
	}

	musicCorePlayerControl(1, 0);
}

/* 0x81232AA4 */
int ParentalControlMgr::CalculateElapsedSec(int *clockSec)
{
	int curSec = *clockSec;
	if (m_lastClockSec >= 0 &&
	    (curSec - m_lastClockSec <= -kParentalClockJumpThresholdSec ||
	     curSec - m_lastClockSec >= kParentalClockJumpThresholdSec)) {
		curSec = m_lastClockSec;
		*clockSec = curSec;
	}

	if (curSec < m_sessionStartSec)
		m_sessionStartSec = -1;

	int elapsedSec = m_accumulatedSec;
	if (m_sessionStartSec >= 0)
		elapsedSec += curSec - m_sessionStartSec;
	if (elapsedSec > m_dailyLimitSec)
		elapsedSec = m_dailyLimitSec;

	int prevRemainingSec = m_remainingSec;
	if (m_dailyLimitSec < 0)
		m_remainingSec = -1;
	else
		m_remainingSec = m_dailyLimitSec - elapsedSec;

	if (prevRemainingSec >= 0 && (m_remainingSec > prevRemainingSec || m_remainingSec == -1)) {
		imposeParentalClose();
		m_nextWarningLevel = kParentalWarningLevelCount;
	}
	return elapsedSec;
}

void ParentalControlMgr::ResetDailyLimitState()
{
	m_lastClockSec = -1;
	m_currentDate = 0;
	m_sessionStartSec = -1;
	m_accumulatedSec = 0;
	m_lastSavedSec = -kParentalSaveIntervalSec;
	m_dailyLimitSec = -1;
}

/* 0x81232B32 */
int ParentalControlMgr::LoadDailyLimit()
{
	int regValue = 0;
	int ret = sceRegMgrGetKeyInt(kParentalCategory, "restrict_mode", &regValue);
	if (ret != 0 || (regValue != 1 && regValue != 2)) {
		ResetDailyLimitState();
		return ret;
	}

	int keyIndex = 0;
	if (regValue == 2) {
		int year = (m_currentDate / 10000) % 10000;
		int month = (m_currentDate / 100) % 100;
		int day = m_currentDate % 100;
		keyIndex = sceRtcGetDayOfWeek(year, month, day) + 1;
	}

	ret = sceRegMgrGetKeyInt(kParentalCategory, kRestrictDurationKeys[keyIndex], &regValue);
	if (ret != 0 || regValue < 0) {
		ResetDailyLimitState();
		return ret;
	}

	int durationMinutes = regValue;
	ret = sceRegMgrGetKeyInt(kParentalCategory, "change_date", &regValue);
	if (ret != 0) {
		ResetDailyLimitState();
		return ret;
	}

	if (regValue == m_currentDate) {
		ret = sceRegMgrGetKeyInt(kParentalCategory, "change_duration", &regValue);
		if (ret != 0) {
			ResetDailyLimitState();
			return ret;
		}
		if (regValue >= -durationMinutes)
			durationMinutes += regValue;
	}

	if (durationMinutes < 0) {
		ResetDailyLimitState();
		return ret;
	}

	m_dailyLimitSec = 60 * durationMinutes;
	return ret;
}

/* 0x81232C82 */
int ParentalControlMgr::Update(bool isActive, bool forceSave)
{
	SceDateTime now;
	if (m_dailyLimitSec < 0 || sceRtcGetCurrentClockLocalTime(&now) != 0)
		return 0;

	bool isFirstDate = (m_currentDate == 0);
	int todayDate = 10000 * now.year + 100 * now.month + now.day;
	if (todayDate != m_currentDate) {
		m_currentDate = todayDate;
		m_lastClockSec = -1;
		m_sessionStartSec = -1;
		m_lastSavedSec = -kParentalSaveIntervalSec;
		m_accumulatedSec = 0;
		imposeParentalClose();
		m_nextWarningLevel = kParentalWarningLevelCount;

		int savedVal = 0;
		if (isFirstDate &&
		    sceRegMgrGetKeyInt(kParentalCategory, "remaining_date", &savedVal) == 0 &&
		    savedVal == m_currentDate &&
		    sceRegMgrGetKeyInt(kParentalCategory, "remaining_duration", &savedVal) == 0 &&
		    savedVal >= 0) {
			m_accumulatedSec = savedVal;
		}
		LoadDailyLimit();
	}

	int nowSec = now.second + 60 * (now.minute + 60 * now.hour);
	int clampedSec = nowSec;
	int elapsedSec = CalculateElapsedSec(&clampedSec);
	if (m_dailyLimitSec < 0)
		return 0;

	m_lastClockSec = nowSec;
	if (m_pauseCount != 0) {
		isActive = false;
	} else if (isActive && m_nextWarningLevel != 0) {
		for (SceUInt8 level = 0; level < m_nextWarningLevel; ++level) {
			if (m_remainingSec <= kWarningThresholdSec[level]) {
				m_nextWarningLevel = level;
				if (m_remainingSec > kWarningThresholdSec[level] - 60) {
					if (level != 0) {
						imposeParentalShowNotice(kWarningThresholdSec[level] / 60);
					} else {
						QuitRestrictedApps();
						imposeParentalOpen(m_dailyLimitSec / 60);
					}
				}
				break;
			}
		}
	}

	if (m_sessionStartSec >= 0) {
		if (!isActive || clampedSec != m_lastClockSec) {
			m_accumulatedSec = elapsedSec;
			m_sessionStartSec = -1;
			if (isActive)
				m_sessionStartSec = m_lastClockSec;
		}
	} else if (isActive) {
		m_sessionStartSec = m_lastClockSec;
	}

	if (elapsedSec - m_lastSavedSec >= kParentalSaveIntervalSec || forceSave)
		EnqueueSaveJob(todayDate, elapsedSec);

	return 0;
}

/* 0x81232ECC */
ParentalControlMgr::ParentalControlMgr()
	: m_regCallbackId(-1),
	  m_currentDate(0),
	  m_lastClockSec(0),
	  m_sessionStartSec(0),
	  m_dailyLimitSec(0),
	  m_accumulatedSec(0),
	  m_lastSavedSec(0),
	  m_pauseCount(0),
	  m_nextWarningLevel(0),
	  m_remainingSec(0)
{
	m_regCallbackId = sceKernelCreateCallback("SceShellParentalControlMgrReg", 0,
						  OnRegistryChanged, NULL);
	if (m_regCallbackId >= 0 &&
	    sceRegMgrRegisterCallback(m_regCallbackId) >= 0 &&
	    sceRegMgrAddRegistryCallback(m_regCallbackId, kParentalCategory) >= 0) {
		sceRegMgrStartCallback(m_regCallbackId);
	}
	SceShell_83D16CA8(SHELL_APP_EVENT_ACTIVATED, OnAppEvent, NULL);
	SceShell_83D16CA8(SHELL_APP_EVENT_DEACTIVATED, OnAppEvent, NULL);
	Update(true, false);
}

/* 0x81232FB4 / 0x81232FF6 */
ParentalControlMgr::~ParentalControlMgr()
{
	SceShell_E3D3A162(SHELL_APP_EVENT_DEACTIVATED, OnAppEvent, NULL);
	SceShell_E3D3A162(SHELL_APP_EVENT_ACTIVATED, OnAppEvent, NULL);
	if (m_regCallbackId >= 0) {
		sceRegMgrUnregisterCallback(m_regCallbackId);
		sceKernelDeleteCallback(m_regCallbackId);
	}
}

/* 0x8123303C */
int ParentalControlMgr::Pause()
{
	if (m_pauseCount == 0) {
		imposeParentalClose();
		if (m_nextWarningLevel == 0)
			m_nextWarningLevel = 1;
	}
	++m_pauseCount;
	return 0;
}

/* 0x8123307E */
int ParentalControlMgr::Resume()
{
	if (m_pauseCount == 0)
		return SHELL_ERROR_ALREADY_CREATED;

	--m_pauseCount;
	if (m_pauseCount == 0)
		Update(true, false);
	return 0;
}

/* 0x812330A6 */
int ParentalControlMgr::OnRegistryChanged(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)common;

	SceDateTime now;
	if (sceRtcGetCurrentClockLocalTime(&now) != 0)
		return 0;

	ParentalControlMgr *mgr = s_parentalControlMgr;
	int todayDate = 10000 * now.year + 100 * now.month + now.day;
	if (todayDate != mgr->m_currentDate) {
		mgr->m_currentDate = todayDate;
		mgr->m_lastClockSec = -1;
		mgr->m_sessionStartSec = -1;
		mgr->m_lastSavedSec = -kParentalSaveIntervalSec;
		mgr->m_accumulatedSec = 0;
		imposeParentalClose();
		mgr->m_nextWarningLevel = kParentalWarningLevelCount;
	}

	mgr->LoadDailyLimit();
	int nowSec = now.second + 60 * (now.minute + 60 * now.hour);
	mgr->CalculateElapsedSec(&nowSec);
	return 0;
}

/* 0x81233178 */
void ParentalControlMgr::OnAppEvent(int code, const void *eventData, SceSize eventSize, void *userArg)
{
	(void)eventSize;
	(void)userArg;

	const ShellAppEvent *appEvent = (const ShellAppEvent *)eventData;
	if (sce_paf_strcmp(appEvent->titleId, kSettingsTitleId) != 0)
		return;

	if (code == SHELL_APP_EVENT_ACTIVATED)
		s_parentalControlMgr->Pause();
	else if (code == SHELL_APP_EVENT_DEACTIVATED)
		s_parentalControlMgr->Resume();
}

/* 0x81232F64 */
int shellParentalControlMgrCreate(void)
{
	if (s_parentalControlMgr != NULL)
		return SHELL_ERROR_ALREADY_CREATED;

	s_parentalControlMgr = new ParentalControlMgr();
	if (s_parentalControlMgr == NULL)
		return SHELL_ERROR_NO_MEMORY;
	return 0;
}

/* 0x81232FA6 */
ParentalControlMgr *shellParentalControlMgrGetInstance(void)
{
	return s_parentalControlMgr;
}

/* 0x812327F8 */
int parentalGetRemainingSec(void)
{
	if (s_parentalControlMgr != NULL)
		return s_parentalControlMgr->GetRemainingSec();
	return kParentalNotInitializedError;
}

/* 0x81232812 */
int shellParentalControlUpdate(bool isActive, bool forceSave)
{
	if (s_parentalControlMgr != NULL)
		return s_parentalControlMgr->Update(isActive, forceSave);
	return kParentalNotInitializedError;
}

/* 0x81232834 */
int shellParentalControlPause(void)
{
	if (s_parentalControlMgr != NULL)
		return s_parentalControlMgr->Pause();
	return kParentalNotInitializedError;
}

/* 0x8123284E */
int shellParentalControlResume(void)
{
	if (s_parentalControlMgr != NULL)
		return s_parentalControlMgr->Resume();
	return kParentalNotInitializedError;
}
