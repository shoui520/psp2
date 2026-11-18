/*
 * The game plugin's alarm: the secure RTC alarm of the applications whose license expires, one
 * entry each, armed for the application in front (0x81063BD0..0x8106471C). The file's name and
 * the functions' are ours (no record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <kernel/threadmgr.h>
#include <vshbridge/vshbridge.h>
#include <std/string.h>
#include <std/string>
#include <std/map>

#include "../../shell/shell.h"
#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "alarm.h"

using namespace paf;

/*
 * the shell events the alarm follows: an application stopped and resumed (PLACEHOLDER names: the
 * alarm is disarmed, and armed again), and the application event server's (values only)
 */
#define GAME_ALARM_EVENT_APP_STOPPED	2
#define GAME_ALARM_EVENT_APP_RESUMED	3
#define GAME_ALARM_APP_EVENT		1

/* the system's own title, whose events the alarm leaves alone but for closing the notices */
#define GAME_ALARM_SYSTEM_TITLE_ID	"NPXS19999"

/* an application being installed: its title ID and when its license expires */
static struct {
	char titleId[12];
	SceUInt32 reserved;
	SceUInt64 expireTime;
} s_pending;

GameAlarm::GameAlarm() : m_callback(-1), m_error(0)
{
	SceShell_7FE4E732(GAME_ALARM_EVENT_APP_STOPPED, (ShellEventCallback)OnAppStopped, this);
	SceShell_7FE4E732(GAME_ALARM_EVENT_APP_RESUMED, (ShellEventCallback)OnAppResumed, this);
	shellAppEventServerAddListener(GAME_ALARM_APP_EVENT, (ShellEventCallback)OnAppEvent, this);

	SceUID cb = sceKernelCreateCallback("SceGameAlarm", 0, OnAlarm, this);

	if (cb < 0) {
		m_error = cb;
		m_callback = -1;
		return;
	}

	m_callback = cb;

	int ret = SceShell_83D16CA8(SHELL_APP_EVENT_ENDED, OnAppEnded, this);

	if (ret < 0) {
		m_error = ret;
		return;
	}

	m_error = 0;
}

GameAlarm::~GameAlarm()
{
	SceShell_E3D3A162(SHELL_APP_EVENT_ENDED, OnAppEnded, this);

	if (m_callback != -1) {
		vshRtcUnregisterSecureAlarmCallback(m_callback);
		sceKernelDeleteCallback(m_callback);
		m_callback = -1;
	}

	shellAppEventServerRemoveListener(GAME_ALARM_APP_EVENT, (ShellEventCallback)OnAppEvent, this);
	SceShell_D4C7D2B7(GAME_ALARM_EVENT_APP_RESUMED, (ShellEventCallback)OnAppResumed, this);
	SceShell_D4C7D2B7(GAME_ALARM_EVENT_APP_STOPPED, (ShellEventCallback)OnAppStopped, this);
}

inline int GameAlarm::StopAlarm(const string &titleId)
{
	if (m_callback != -1) {
		int ret = vshRtcUnregisterSecureAlarmCallback(m_callback);

		if (ret < 0 && ret != SCE_RTC_ERROR_NOT_FOUND)
			return ret;

		m_alarmTitleId.clear();
	}

	return 0;
}

inline int GameAlarm::StartAlarm(const string &titleId)
{
	if (m_callback != -1) {
		int ret = vshRtcRegisterSecureAlarmCallback(m_callback);

		if (ret < 0)
			return ret;

		m_alarmTitleId = titleId;
	}

	return 0;
}

/*
 * The application at path, being installed, kept as the next to get an alarm, with when its
 * license expires: status's, or its own license's when status is NULL; none for a license that
 * does not expire (0x81063D4A).
 */
int GameAlarm::SetPending(const char *path, const GameLicenseStatus *status)
{
	sce_paf_memset(&s_pending, 0, sizeof(s_pending));

	if (m_error < 0)
		return GAME_ERROR_NOT_STARTED;

	if (status == NULL) {
		GameLicenseStatus own;

		sce_paf_memset(&own, 0, sizeof(own));
		sce_paf_strncpy(own.path, path, sizeof(own.path) - 1);
		own.path[sizeof(own.path) - 1] = '\0';

		int ret = gameLicenseGetStatus(&own);

		if (ret < 0)
			return ret;

		if (own.valid == false)
			return 0;

		if (own.valid == true && own.expireTime == 0x7FFFFFFFFFFFFFFFull)
			return 0;

		status = &own;

		if (own.result < 0)
			return own.result;
	}

	const char *name = sce_paf_strrchr(path, '/') + 1;
	string titleId(name);

	sce_paf_strncpy(s_pending.titleId, name, sizeof(s_pending.titleId) - 1);
	s_pending.titleId[sizeof(s_pending.titleId) - 1] = '\0';
	sce_paf_memcpy(&s_pending.expireTime, &status->expireTime, sizeof(s_pending.expireTime));

	return 0;
}

/* titleId's entry deleted, and the alarm disarmed when it is armed for titleId (0x81063E58) */
void GameAlarm::Remove(const string &titleId)
{
	EntryMap::iterator it = m_entries.find(titleId);

	if (it == m_entries.end())
		return;

	if (m_alarmTitleId == titleId)
		StopAlarm(titleId);

	if (it->second != NULL)
		delete it->second;

	m_entries.erase(it->first);
}

/*
 * The application of info, when it is the pending one, given an entry, the alarm armed for it
 * and started (0x81063EEC).
 */
int GameAlarm::Add(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));

	if (!(titleId == s_pending.titleId)) {
		sce_paf_memset(&s_pending, 0, sizeof(s_pending));
		return 0;
	}

	if (m_entries.find(titleId) != m_entries.end())
		return 0;

	GameAlarmEntry *entry = new GameAlarmEntry(titleId, &s_pending.expireTime);

	if (entry == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	m_entries[titleId] = entry;

	int ret = StopAlarm(titleId);

	if (ret < 0)
		return ret;

	ret = StartAlarm(titleId);
	if (ret < 0)
		return ret;

	ret = entry->Start();
	if (ret < 0) {
		Remove(titleId);
		return ret;
	}

	return 0;
}

/*
 * The alarm armed again for the application of info and its entry started again; an entry gone
 * is removed (0x81064154).
 */
int GameAlarm::Refresh(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	EntryMap::iterator entry = m_entries.find(titleId);

	if (entry == m_entries.end() || entry->second == NULL) {
		Remove(titleId);
		return 0;
	}

	GameAlarmEntry *alarm = entry->second;
	int ret = StopAlarm(titleId);

	if (ret < 0)
		return ret;

	ret = StartAlarm(titleId);
	if (ret < 0)
		return ret;

	ret = alarm->Restart();
	if (ret < 0) {
		Remove(titleId);
		return ret;
	}

	return 0;
}

/*
 * The secure alarm rang (the "SceGameAlarm" callback, 0x810642F2): the entry it is armed for
 * checked, and the alarm disarmed when the check stopped.
 */
int GameAlarm::OnAlarm(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg, void *common)
{
	GameAlarm *alarm = (GameAlarm *)common;

	if (notifyArg == 0)
		return 0;

	if (alarm->m_alarmTitleId.size() == 0)
		return 0;

	EntryMap::iterator it = alarm->m_entries.find(alarm->m_alarmTitleId);

	if (it == alarm->m_entries.end())
		return 0;

	GameAlarmEntry *entry = it->second;

	if (entry == NULL)
		return 0;

	int ret = entry->Check();

	if (ret >= 0)
		return 0;

	if (ret == GAME_ERROR_ABORTED) {
		alarm->StopAlarm(alarm->m_alarmTitleId);
		return 0;
	}

	return ret;
}

/*
 * An application ended (0x8106437E): its entry removed, and the alarm disarmed when it was armed
 * for it.
 */
void GameAlarm::OnAppEnded(int code, const void *data, SceSize size, void *arg)
{
	GameAlarm *alarm = (GameAlarm *)arg;
	const ShellAppEvent *event = (const ShellAppEvent *)data;

	alarm->Remove(string(event->titleId));

	if (alarm->m_alarmTitleId.size() != 0 && alarm->m_alarmTitleId == event->titleId) {
		alarm->StopAlarm(string(event->titleId));
		alarm->m_alarmTitleId.clear();
	}
}

/* An application other than the system's stopped: the alarm disarmed (0x81064480). */
void GameAlarm::OnAppStopped(int code, int value, const char *titleId, void *arg)
{
	GameAlarm *alarm = (GameAlarm *)arg;

	if (sce_paf_strncmp(titleId, GAME_ALARM_SYSTEM_TITLE_ID,
			    sizeof(GAME_ALARM_SYSTEM_TITLE_ID)) == 0)
		return;

	alarm->StopAlarm(string(titleId));
}

/*
 * An application other than the system's resumed: the alarm armed for it again and its entry
 * started again, when it has one (0x81064514).
 */
void GameAlarm::OnAppResumed(int code, int value, const char *titleId, void *arg)
{
	GameAlarm *alarm = (GameAlarm *)arg;

	if (sce_paf_strncmp(titleId, GAME_ALARM_SYSTEM_TITLE_ID,
			    sizeof(GAME_ALARM_SYSTEM_TITLE_ID)) == 0)
		return;

	EntryMap::iterator end = alarm->m_entries.end();

	for (EntryMap::iterator it = alarm->m_entries.begin(); it != end; ++it) {
		GameAlarmEntry *entry = it->second;

		if (entry != NULL && entry->titleId == titleId) {
			alarm->StartAlarm(string(titleId));
			entry->Restart();
			return;
		}
	}
}

/* The system's title came up (0x81064632): every entry's notice closed. */
void GameAlarm::OnAppEvent(int code, const char *titleId, SceSize size, void *arg)
{
	GameAlarm *alarm = (GameAlarm *)arg;

	if (sce_paf_strncmp(titleId, GAME_ALARM_SYSTEM_TITLE_ID,
			    sizeof(GAME_ALARM_SYSTEM_TITLE_ID)) != 0 || alarm == NULL)
		return;

	EntryMap::iterator end = alarm->m_entries.end();

	for (EntryMap::iterator it = alarm->m_entries.begin(); it != end; ++it) {
		if (it->second != NULL)
			it->second->Close();
	}
}
