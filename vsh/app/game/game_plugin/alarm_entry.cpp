/*
 * The game plugin's alarm of one application whose license expires: the secure alarm set 30,
 * 15 and 5 minutes before the license ends, a notice of the minutes left each time it rings,
 * and the application ended once the license has (0x8107FBF0..0x81080444). The file's name and
 * the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <common/char_encoding.h>

#include "../../shell/plugins.h"
#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "alarm.h"

using namespace paf;

/* how long before the license ends the alarm rings: 30, 15 and 5 minutes, in microseconds */
static const SceUInt64 s_alarmLeadTimes[] = { 1800000000, 900000000, 300000000 };

#define GAME_ALARM_STEPS	3

/* the time left a license ended already is given to end its application, in microseconds */
#define GAME_ALARM_GRACE_TIME	120000000

/* a minute, in microseconds */
#define GAME_ALARM_MINUTE	60000000

/* the applauncher_plugin text of the notice: the minutes left of a license, "%s" */
#define GAME_ALARM_MSG_MINUTES_LEFT	0x6F6BB579

GameAlarmEntry::GameAlarmEntry(const string &titleId, const SceUInt64 *expireTime)
	: m_expireTime(), m_titleId(titleId)
{
	sce_paf_memcpy(&m_expireTime, expireTime, sizeof(m_expireTime));
	m_name = string("SceGameAlarm") + titleId;
}

GameAlarmEntry::~GameAlarmEntry()
{
}

/* the notice of the minutes left until the license ends, rounded up (0x8107FE1C) */
__attribute__((noinline)) void GameAlarmEntry::Notify(int step, SceUInt64 remaining)
{
	Plugin *impose = Plugin::Find("impose_plugin");

	if (impose == NULL)
		return;

	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (applauncher == NULL)
		return;

	ImposePluginInterface *iface = (ImposePluginInterface *)impose->GetInterface(1);

	if (iface == NULL)
		return;

	char minutes[8];

	sce_paf_snprintf(minutes, sizeof(minutes), "%u",
			 (unsigned int)((remaining + GAME_ALARM_MINUTE - 1) / GAME_ALARM_MINUTE));

	wstring text;

	common::Utf8ToUtf16(string(minutes), &text, 0);

	wstring message = shellFormatText(
		applauncher->GetString(IDParam(GAME_ALARM_MSG_MINUTES_LEFT)), text.c_str());
	ImposeNotice notice;

	notice.name = m_name.c_str();
	notice.message = message.c_str();
	iface->showNotice(&notice);
}

/*
 * The next time the alarm rings: the earliest of its times still to come, with the number of
 * times left in step; once all have passed, the license's end, and once that has passed too the
 * application ended (0x81080028).
 */
__attribute__((noinline)) int GameAlarmEntry::GetNextTick(SceRtcTick *tick, int *step)
{
	SceRtcTick now;
	int i;
	int ret;

	for (i = GAME_ALARM_STEPS; i != 0; i--) {
		ret = vshRtcGetCurrentSecureTick(&now);
		if (ret < 0)
			return ret;

		ret = sceRtcTickAddTicks(tick, &m_expireTime, -s_alarmLeadTimes[i - 1]);
		if (ret < 0)
			return ret;

		if (tick->tick < now.tick)
			break;
	}

	if (step != NULL)
		*step = i;

	if (i == GAME_ALARM_STEPS) {
		if (m_expireTime.tick < now.tick) {
			SceAppMgrAppStatus status;

			sce_paf_memset(&status, 0, sizeof(status));
			if (sceAppMgrGetStatusByName(m_titleId.c_str(), &status) != 0)
				return 0;

			if (status.activated) {
				ret = sceAppMgrQuitApp(status.appId);
				if (ret < 0)
					return ret;
			}

			return GAME_ERROR_ABORTED;
		}

		ret = vshRtcGetCurrentSecureTick(&now);
		if (ret < 0)
			return ret;

		if (m_expireTime.tick <= now.tick)
			return GAME_ERROR_LICENSE_PERIOD;

		sce_paf_memcpy(tick, &m_expireTime, sizeof(*tick));
		return 0;
	}

	ret = sceRtcTickAddTicks(tick, &m_expireTime, -s_alarmLeadTimes[i]);
	if (ret < 0)
		return ret;

	return 0;
}

/* the alarm set for the license's next time, with a notice when it ends within 30 minutes */
int GameAlarmEntry::Start(void)
{
	int ret = vshRtcSetSecureAlarmTick(&m_expireTime);

	if (ret < 0)
		return ret;

	if (vshRtcIsSecureAlarmed())
		return GAME_ERROR_LICENSE_PERIOD;

	SceRtcTick tick;

	ret = GetNextTick(&tick, NULL);
	if (ret < 0)
		return ret;

	ret = vshRtcSetSecureAlarmTick(&tick);
	if (ret < 0)
		return ret;

	SceRtcTick now;

	ret = vshRtcGetCurrentSecureTick(&now);
	if (ret < 0)
		return ret;

	if (m_expireTime.tick < now.tick)
		return GAME_ERROR_LICENSE_PERIOD;

	SceUInt64 remaining = m_expireTime.tick - now.tick;

	if (remaining < s_alarmLeadTimes[0])
		Notify(0, remaining);

	return 0;
}

/*
 * The alarm set again (the application back in front): within 30 minutes of the license's end
 * its next time, with a notice; a license ended already gives the application two more minutes
 * (0x81080210).
 */
int GameAlarmEntry::Restart(void)
{
	SceRtcTick now;
	SceUInt64 remaining;
	int ret = vshRtcGetCurrentSecureTick(&now);

	if (ret < 0)
		return ret;

	if (m_expireTime.tick < now.tick) {
		SceRtcTick tick;

		remaining = GAME_ALARM_GRACE_TIME;

		ret = vshRtcGetCurrentSecureTick(&tick);
		if (ret < 0)
			return ret;

		ret = sceRtcTickAddTicks(&tick, &tick, GAME_ALARM_GRACE_TIME);
		if (ret < 0)
			return ret;

		ret = vshRtcSetSecureAlarmTick(&tick);
		if (ret < 0)
			return ret;
	} else {
		SceRtcTick limit;

		ret = sceRtcTickAddTicks(&limit, &now, s_alarmLeadTimes[0]);
		if (ret < 0)
			return ret;

		if (limit.tick < m_expireTime.tick)
			return 0;

		int step = GAME_ALARM_STEPS;
		SceRtcTick tick;

		ret = GetNextTick(&tick, &step);
		if (ret < 0)
			return ret;

		if (step > 0) {
			ret = vshRtcSetSecureAlarmTick(&tick);
			if (ret < 0)
				return ret;
		}

		remaining = m_expireTime.tick - now.tick;
		if (remaining < GAME_ALARM_GRACE_TIME)
			return 0;
	}

	if (remaining < s_alarmLeadTimes[0])
		Notify(0, remaining);

	return 0;
}

/*
 * The alarm rang: its next time set, and with the application running a notice of the minutes
 * of the time that rang (0x81080316).
 */
int GameAlarmEntry::Check(void)
{
	if (!vshRtcIsSecureAlarmed())
		return 0;

	SceRtcTick now;
	int ret = vshRtcGetCurrentSecureTick(&now);

	if (ret < 0)
		return ret;

	SceAppMgrAppStatus status;

	sce_paf_memset(&status, 0, sizeof(status));
	if (sceAppMgrGetStatusByName(m_titleId.c_str(), &status) == 0) {
		int step = GAME_ALARM_STEPS;
		SceRtcTick tick;

		ret = GetNextTick(&tick, &step);
		if (ret < 0)
			return ret;

		if (step > 0) {
			ret = vshRtcSetSecureAlarmTick(&tick);
			if (ret < 0)
				return ret;
		}

		if (status.activated)
			Notify(step, s_alarmLeadTimes[step - 1]);
	}

	return 0;
}

void GameAlarmEntry::Close(void)
{
	Plugin *impose = Plugin::Find("impose_plugin");

	if (impose == NULL)
		return;

	ImposePluginInterface *iface = (ImposePluginInterface *)impose->GetInterface(1);

	if (iface != NULL)
		iface->removeNotice(m_name.c_str());
}
