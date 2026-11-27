/*
 * The game plugin's alarm: the secure RTC alarm of the applications whose license expires, one
 * entry each, armed for the application in front (alarm.cpp). The names are ours (no record
 * names them).
 */
#ifndef _GAME_PLUGIN_ALARM_H
#define _GAME_PLUGIN_ALARM_H

#include <scetypes.h>
#include <rtc.h>
#include <std/string>
#include <std/map>

#include "game_appinfo.h"

/*
 * An application's alarm (alarm_entry.cpp, constructor 0x8107FBF0, vtable 0x8152EF3C), 56 bytes:
 * the application, when its license expires, and the secure alarm set 30, 15 and 5 minutes
 * before, each time with a notice of the minutes left. The names are ours.
 */
class GameAlarmEntry {
public:
	GameAlarmEntry(const paf::string &titleId, const SceUInt64 *expireTime);
	virtual ~GameAlarmEntry();

	/* PLACEHOLDER names: the alarm started (0x81080160), started again (0x81080210), its time
	   checked when the alarm rings (0x81080316), and its notice taken down (0x810803AA) */
	int Start(void);
	int Restart(void);
	int Check(void);
	void Close(void);

private:
	friend class GameAlarm;

	void Notify(int step, SceUInt64 remaining);
	int GetNextTick(SceRtcTick *tick, int *step);

	SceUInt32 headerPad;
	SceRtcTick m_expireTime;
	paf::string m_titleId;
	paf::wstring unusedLabel;	/* nothing reads or writes it */
	paf::string m_name;		/* the notice's: "SceGameAlarm" and the title ID */
	SceUInt32 tailPad;
};

typedef char GameAlarmEntrySize[sizeof(GameAlarmEntry) == 56 ? 1 : -1];

/*
 * The plugin's alarm (constructor 0x81063BD0): the entries by title ID, the kernel callback the
 * secure alarm calls and the application it is armed for.
 */
class GameAlarm {
public:
	GameAlarm();
	virtual ~GameAlarm();

	int SetPending(const char *path, const GameLicenseStatus *status);
	int Add(const LsdbAppInfoPtr *info);
	int Refresh(const LsdbAppInfoPtr *info);

private:
	typedef paf::map<paf::string, GameAlarmEntry *> EntryMap;

	void Remove(const paf::string &titleId);

	/* the secure alarm disarmed, and armed for titleId */
	int StopAlarm(const paf::string &titleId);
	int StartAlarm(const paf::string &titleId);

	static int OnAlarm(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg, void *common);
	static void OnAppEnded(int code, const void *data, SceSize size, void *arg);
	static void OnAppStopped(int code, int value, const char *titleId, void *arg);
	static void OnAppResumed(int code, int value, const char *titleId, void *arg);
	static void OnAppEvent(int code, const char *titleId, SceSize size, void *arg);

	EntryMap m_entries;
	SceUID m_callback;			/* the "SceGameAlarm" callback, -1 without */
	int m_error;				/* the constructor's */
	paf::string m_alarmTitleId;		/* the application the alarm is armed for */
};

typedef char GameAlarmSize[sizeof(GameAlarm) == 36 ? 1 : -1];

#endif /* _GAME_PLUGIN_ALARM_H */
