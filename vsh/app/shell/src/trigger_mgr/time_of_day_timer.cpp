/*
 * time_of_day_timer.cpp: SceTriggerMgr time-of-day and daily timer management
 * (0x812A7562-0x812A9707).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <std/stdio.h>
#include <std/string.h>
#include "../../shell_cron.h"
#include "trigger_mgr_internal.h"

using namespace paf;

static const SceUInt32 s_timeOfDaySlotMask[TRIGGER_MGR_MAX_TIMEOFDAY_SLOTS] = {
	0x00000010u, 0x00000020u, 0x00000040u, 0x00000080u,
	0x00000100u, 0x00000200u, 0x00000400u, 0x00000800u
};

static SceUID s_timeOfDayCronHandles[TRIGGER_MGR_MAX_APPS][TRIGGER_MGR_MAX_TIMEOFDAY_SLOTS];

static int triggerTimeOfDayCronCallback(SceUID cronHandle, void *userArg);

namespace {

static void refreshExpiredStatusIfUnfired(int appIndex, SceUInt32 timerSlot)
{
	TriggerTimeOfDayTimerRecord *timerRecord =
		&g_triggerDb.timeOfDayTimers[appIndex][timerSlot];
	if ((timerRecord->flags & TRIGGER_TIMER_FLAG_FIRED) == 0) {
		int remainingSeconds = 0;
		ShellCron *cron = shellCronGetInstance();
		if (cron->GetRemaining(s_timeOfDayCronHandles[appIndex][timerSlot],
				       &remainingSeconds) != 0) {
			timerRecord->flags |= TRIGGER_TIMER_FLAG_FIRED;
			timerRecord->lastError = TRIGGER_MGR_ERROR_MISSED;
		}
	}
}

/* Job executed when a time-of-day or daily timer fires (0x812A7562) */
class TimeOfDayTimerFireJob : public TriggerFireJobBase {
public:
	TimeOfDayTimerFireJob(const char *appTitleId, SceUInt32 timerSlot);
	virtual ~TimeOfDayTimerFireJob();
	virtual int Run();
};

TimeOfDayTimerFireJob::TimeOfDayTimerFireJob(const char *appTitleId, SceUInt32 timerSlot)
	: TriggerFireJobBase(appTitleId, timerSlot)
{
}

TimeOfDayTimerFireJob::~TimeOfDayTimerFireJob()
{
}

int TimeOfDayTimerFireJob::Run()
{
	SceRtcTick currentTick;
	sceRtcGetCurrentTick(&currentTick);

	g_triggerMgrTableLock.WriterLock();
	int appIndex = triggerMgrFindAppIndex(m_appTitleId);
	if (appIndex < 0) {
		g_triggerMgrTableLock.WriterUnlock();
		return 0;
	}

	TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
	if (appRecord == NULL || (appRecord->flags & s_timeOfDaySlotMask[m_timerSlot]) == 0) {
		g_triggerMgrTableLock.WriterUnlock();
		return 0;
	}

	TriggerTimeOfDayTimerRecord *timerRecord =
		&g_triggerDb.timeOfDayTimers[appIndex][m_timerSlot];
	timerRecord->flags |= TRIGGER_TIMER_FLAG_FIRED;

	if ((appRecord->flags & TRIGGER_APP_FLAG_ENABLED) != 0) {
		char targetTitleId[12];
		char launchArg[128];
		sce_paf_strncpy(targetTitleId, timerRecord->targetTitleId, TRIGGER_MGR_TITLE_ID_LEN);
		targetTitleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';

		bool waitLaunch = (timerRecord->flags & TRIGGER_TIMER_FLAG_WAIT_LAUNCH) != 0;
		bool resumeOnly = (timerRecord->flags & TRIGGER_TIMER_FLAG_RESUME_ONLY) != 0;
		const char *timerKindName =
			(timerRecord->flags & TRIGGER_TIMER_FLAG_IS_DAILY) != 0
				? "DailyTimer"
				: "TimeOfDayTimer";
		sce_paf_snprintf(launchArg, sizeof(launchArg),
				 "trigger_util:type=%s&slot=%u", timerKindName, m_timerSlot);

		g_triggerMgrTableLock.WriterUnlock();
		int launchResult = triggerMgrLaunchOrResumeApp(
			this, targetTitleId, launchArg, waitLaunch, resumeOnly);
		g_triggerMgrTableLock.WriterLock();

		int refreshedIndex = triggerMgrFindAppIndex(m_appTitleId);
		if (refreshedIndex < 0) {
			g_triggerMgrTableLock.WriterUnlock();
			return 0;
		}
		g_triggerDb.timeOfDayTimers[refreshedIndex][m_timerSlot].lastError = launchResult;
	} else {
		timerRecord->lastError = TRIGGER_MGR_ERROR_DISABLED;
	}

	g_triggerMgrTableLock.WriterUnlock();
	g_triggerMgrTableLock.ReaderLock();
	triggerMgrSaveDatabase();
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

/* Job to register a one-shot time-of-day timer (0x812A76FA) */
class RegisterTimeOfDayTimerJob : public TriggerIpcJobBase {
public:
	RegisterTimeOfDayTimerJob(IPMI::Session *session, SceUInt32 requestId,
				  const char *targetTitleId,
				  const TriggerTimeOfDayTimerParam *timerParam,
				  SceUInt32 timerSlot, bool useCallback);
	virtual ~RegisterTimeOfDayTimerJob();
	virtual int Run();

	char m_targetTitleId[12];
	TriggerTimeOfDayTimerParam m_timerParam;
	SceUInt32 m_timerSlot;
};

RegisterTimeOfDayTimerJob::RegisterTimeOfDayTimerJob(IPMI::Session *session, SceUInt32 requestId,
						     const char *targetTitleId,
						     const TriggerTimeOfDayTimerParam *timerParam,
						     SceUInt32 timerSlot, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrJob", session, requestId, useCallback),
	  m_timerParam(*timerParam),
	  m_timerSlot(timerSlot)
{
	sce_paf_strncpy(m_targetTitleId, targetTitleId, TRIGGER_MGR_TITLE_ID_LEN);
	m_targetTitleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';
}

RegisterTimeOfDayTimerJob::~RegisterTimeOfDayTimerJob()
{
}

int RegisterTimeOfDayTimerJob::Run()
{
	char callerTitleId[12];
	SceUID clientPid = m_session->getProcessId();
	int resolveErr = triggerMgrResolveProcessTitleId(clientPid, callerTitleId);
	if (resolveErr != 0) {
		triggerMgrCompleteSimpleAsyncJob(this, resolveErr);
		return 0;
	}

	if (!triggerMgrIsTitleInstalled(m_targetTitleId)) {
		triggerMgrCompleteSimpleAsyncJob(this, TRIGGER_MGR_ERROR_APP_NOT_FOUND);
		return 0;
	}

	g_triggerMgrTableLock.WriterLock();
	int appIndex = triggerMgrFindAppIndex(callerTitleId);
	bool createdAppRecord = false;
	TriggerAppRecord *appRecord = NULL;
	if (appIndex >= 0) {
		appRecord = triggerMgrGetAppRecord(appIndex);
	} else {
		appIndex = triggerMgrFindFreeAppIndex();
		if (appIndex < 0) {
			triggerMgrCompleteSimpleAsyncJob(this, TRIGGER_MGR_ERROR_APP_TABLE_FULL);
			g_triggerMgrTableLock.WriterUnlock();
			return 0;
		}
		appRecord = triggerMgrGetAppRecord(appIndex);
		SceUInt32 regOrder = triggerMgrGetSequenceCounter();
		sce_paf_memset(appRecord, 0, sizeof(*appRecord));
		sce_paf_strncpy(appRecord->titleId, callerTitleId, TRIGGER_MGR_TITLE_ID_LEN);
		appRecord->registrationOrder = regOrder;
		appRecord->flags = TRIGGER_APP_FLAG_ENABLED;
		createdAppRecord = true;
	}

	SceUInt32 slotMask = s_timeOfDaySlotMask[m_timerSlot];
	if ((appRecord->flags & slotMask) != 0) {
		ShellCron *cron = shellCronGetInstance();
		cron->Remove(s_timeOfDayCronHandles[appIndex][m_timerSlot]);
		s_timeOfDayCronHandles[appIndex][m_timerSlot] = 0;
		appRecord->flags &= ~slotMask;
		sce_paf_memset(&g_triggerDb.timeOfDayTimers[appIndex][m_timerSlot], 0,
			       sizeof(TriggerTimeOfDayTimerRecord));
	}

	ShellCronOption cronOpt;
	cronOpt.wakeUp = ((appRecord->flags & TRIGGER_APP_FLAG_ENABLED) != 0) &&
			 m_timerParam.wakeUp;
	cronOpt.persist = m_timerParam.persistIfLate;

	SceUID newCronHandle = 0;
	ShellCron *cron = shellCronGetInstance();
	int addErr = cron->AddAt(&newCronHandle, triggerTimeOfDayCronCallback, NULL,
				 m_timerParam.targetTick.tick, &cronOpt);
	int finalResult;
	if (addErr != 0) {
		sceClibPrintf(
			"[SceTriggerMgr] ::shell::CronCallbackMgr::registerTimeOfDayTimerCallback() failed. 0x%x\n",
			addErr);
		finalResult = TRIGGER_MGR_ERROR_INTERNAL;
		if (createdAppRecord)
			sce_paf_memset(appRecord, 0, sizeof(*appRecord));
		g_triggerMgrTableLock.WriterUnlock();
	} else {
		appRecord->flags |= slotMask;
		TriggerTimeOfDayTimerRecord *timerRecord =
			&g_triggerDb.timeOfDayTimers[appIndex][m_timerSlot];
		sce_paf_memset(timerRecord, 0, sizeof(*timerRecord));
		sce_paf_strncpy(timerRecord->targetTitleId, m_targetTitleId,
				TRIGGER_MGR_TITLE_ID_LEN);
		timerRecord->scheduledTime = m_timerParam.targetTick.tick;

		if (m_timerParam.wakeUp)
			timerRecord->flags |= TRIGGER_TIMER_FLAG_WAKE_UP;
		if (m_timerParam.waitLaunchResult)
			timerRecord->flags |= TRIGGER_TIMER_FLAG_WAIT_LAUNCH;
		if (m_timerParam.resumeOnly)
			timerRecord->flags |= TRIGGER_TIMER_FLAG_RESUME_ONLY;
		if (m_timerParam.persistIfLate)
			timerRecord->flags |= TRIGGER_TIMER_FLAG_PERSIST_IF_LATE;

		s_timeOfDayCronHandles[appIndex][m_timerSlot] = newCronHandle;
		g_triggerMgrTableLock.WriterUnlock();

		g_triggerMgrTableLock.ReaderLock();
		triggerMgrSaveDatabase();
		g_triggerMgrTableLock.ReaderUnlock();
		finalResult = 0;
	}

	triggerMgrCompleteSimpleAsyncJob(this, finalResult);
	return 0;
}

/* Job to unregister a time-of-day or daily timer (0x812A79D0) */
class UnregisterTimeOfDayTimerJob : public TriggerIpcJobBase {
public:
	UnregisterTimeOfDayTimerJob(IPMI::Session *session, SceUInt32 requestId,
				    SceUInt32 timerSlot, bool isDaily, bool useCallback);
	virtual ~UnregisterTimeOfDayTimerJob();
	virtual int Run();

	bool m_isDaily;
	SceUInt32 m_timerSlot;
};

UnregisterTimeOfDayTimerJob::UnregisterTimeOfDayTimerJob(IPMI::Session *session,
							 SceUInt32 requestId, SceUInt32 timerSlot,
							 bool isDaily, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrJob", session, requestId, useCallback),
	  m_isDaily(isDaily),
	  m_timerSlot(timerSlot)
{
}

UnregisterTimeOfDayTimerJob::~UnregisterTimeOfDayTimerJob()
{
}

int UnregisterTimeOfDayTimerJob::Run()
{
	char callerTitleId[12];
	SceUID clientPid = m_session->getProcessId();
	int resolveErr = triggerMgrResolveProcessTitleId(clientPid, callerTitleId);
	if (resolveErr != 0) {
		triggerMgrCompleteSimpleAsyncJob(this, resolveErr);
		return 0;
	}

	g_triggerMgrTableLock.WriterLock();
	int appIndex = triggerMgrFindAppIndex(callerTitleId);
	int finalResult;
	if (appIndex >= 0) {
		TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
		SceUInt32 slotMask = s_timeOfDaySlotMask[m_timerSlot];
		TriggerTimeOfDayTimerRecord *timerRecord =
			&g_triggerDb.timeOfDayTimers[appIndex][m_timerSlot];
		if ((appRecord->flags & slotMask) != 0) {
			bool recordIsDaily = (timerRecord->flags & TRIGGER_TIMER_FLAG_IS_DAILY) != 0;
			if (m_isDaily != recordIsDaily) {
				g_triggerMgrTableLock.WriterUnlock();
				finalResult = TRIGGER_MGR_ERROR_TIMER_TYPE_MISMATCH;
			} else {
				ShellCron *cron = shellCronGetInstance();
				cron->Remove(s_timeOfDayCronHandles[appIndex][m_timerSlot]);
				s_timeOfDayCronHandles[appIndex][m_timerSlot] = 0;
				appRecord->flags &= ~slotMask;
				sce_paf_memset(timerRecord, 0, sizeof(*timerRecord));
				g_triggerMgrTableLock.WriterUnlock();

				g_triggerMgrTableLock.ReaderLock();
				triggerMgrSaveDatabase();
				g_triggerMgrTableLock.ReaderUnlock();
				finalResult = 0;
			}
		} else {
			g_triggerMgrTableLock.WriterUnlock();
			finalResult = TRIGGER_MGR_ERROR_TIMER_NOT_FOUND;
		}
	} else {
		g_triggerMgrTableLock.WriterUnlock();
		finalResult = TRIGGER_MGR_ERROR_TIMER_NOT_FOUND;
	}

	triggerMgrCompleteSimpleAsyncJob(this, finalResult);
	return 0;
}

/* Job to query the scheduled tick and fired flag of a time-of-day timer (0x812A7B30, 0x812A7C0E) */
class GetTimeOfDayTimerTimeJob : public TriggerIpcJobBase {
public:
	GetTimeOfDayTimerTimeJob(IPMI::Session *session, SceUInt32 requestId,
				 SceUInt32 timerSlot, bool useCallback);
	virtual ~GetTimeOfDayTimerTimeJob();
	void Complete(int result, SceUInt64 scheduledTick, bool expired);
	virtual int Run();

	SceUInt32 m_timerSlot;
};

GetTimeOfDayTimerTimeJob::GetTimeOfDayTimerTimeJob(IPMI::Session *session, SceUInt32 requestId,
						   SceUInt32 timerSlot, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrGetTimeOfDayTimerTimeJob", session, requestId,
			    useCallback),
	  m_timerSlot(timerSlot)
{
}

GetTimeOfDayTimerTimeJob::~GetTimeOfDayTimerTimeJob()
{
}

void GetTimeOfDayTimerTimeJob::Complete(int result, SceUInt64 scheduledTick, bool expired)
{
	SceRtcTick tickValue;
	tickValue.tick = scheduledTick;
	g_triggerMgrIpcLock.Lock();
	if (!IsCanceled()) {
		int respErr = triggerMgrRespondTimeOfDayTime(
			m_session, m_requestId, result, &tickValue, &expired);
		if (respErr != 0)
			triggerMgrAssertFail(NULL, respErr);
		if (m_useCallback) {
			int notifyErr = shellSvcNotifyEvent(
				m_session, TRIGGER_MGR_NOTIFY_EVENT_FLAG);
			if (notifyErr != 0)
				triggerMgrAssertFail(NULL, notifyErr);
		}
		TriggerIpcJobSlot *slot = triggerMgrFindIpcSlot(m_session, m_requestId);
		if (slot == NULL)
			triggerMgrAssertFailNoCode();
		triggerMgrClearIpcSlot(slot);
	}
	g_triggerMgrIpcLock.Unlock();
}

int GetTimeOfDayTimerTimeJob::Run()
{
	char callerTitleId[12];
	SceUID clientPid = m_session->getProcessId();
	int resolveErr = triggerMgrResolveProcessTitleId(clientPid, callerTitleId);
	if (resolveErr != 0) {
		Complete(resolveErr, 0, false);
		return 0;
	}

	g_triggerMgrTableLock.ReaderLock();
	int appIndex = triggerMgrFindAppIndex(callerTitleId);
	int finalResult = TRIGGER_MGR_ERROR_TIMER_NOT_FOUND;
	SceUInt64 scheduledTick = 0;
	bool expired = false;

	if (appIndex >= 0) {
		TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
		refreshExpiredStatusIfUnfired(appIndex, m_timerSlot);
		TriggerTimeOfDayTimerRecord *timerRecord =
			&g_triggerDb.timeOfDayTimers[appIndex][m_timerSlot];
		if ((appRecord->flags & s_timeOfDaySlotMask[m_timerSlot]) != 0) {
			if ((timerRecord->flags & TRIGGER_TIMER_FLAG_IS_DAILY) != 0) {
				finalResult = TRIGGER_MGR_ERROR_TIMER_TYPE_MISMATCH;
			} else {
				scheduledTick = timerRecord->scheduledTime;
				expired = (timerRecord->flags & TRIGGER_TIMER_FLAG_FIRED) != 0;
				finalResult = 0;
			}
		}
	}

	Complete(finalResult, scheduledTick, expired);
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

/* Job to query the last launch error for a time-of-day or daily timer (0x812A7D6A) */
class GetTimeOfDayTimerLastErrorJob : public TriggerIpcJobBase {
public:
	GetTimeOfDayTimerLastErrorJob(IPMI::Session *session, SceUInt32 requestId,
				      SceUInt32 timerSlot, bool isDaily, bool useCallback);
	virtual ~GetTimeOfDayTimerLastErrorJob();
	virtual int Run();

	SceUInt32 m_timerSlot;
	bool m_isDaily;
};

GetTimeOfDayTimerLastErrorJob::GetTimeOfDayTimerLastErrorJob(IPMI::Session *session,
							     SceUInt32 requestId,
							     SceUInt32 timerSlot, bool isDaily,
							     bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrJob", session, requestId, useCallback),
	  m_timerSlot(timerSlot),
	  m_isDaily(isDaily)
{
}

GetTimeOfDayTimerLastErrorJob::~GetTimeOfDayTimerLastErrorJob()
{
}

int GetTimeOfDayTimerLastErrorJob::Run()
{
	char callerTitleId[12];
	SceUID clientPid = m_session->getProcessId();
	int resolveErr = triggerMgrResolveProcessTitleId(clientPid, callerTitleId);
	if (resolveErr != 0) {
		triggerMgrCompleteSimpleAsyncJob(this, resolveErr);
		return 0;
	}

	g_triggerMgrTableLock.ReaderLock();
	int appIndex = triggerMgrFindAppIndex(callerTitleId);
	int finalResult = TRIGGER_MGR_ERROR_TIMER_NOT_FOUND;
	if (appIndex >= 0) {
		TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
		if ((appRecord->flags & s_timeOfDaySlotMask[m_timerSlot]) != 0) {
			refreshExpiredStatusIfUnfired(appIndex, m_timerSlot);
			TriggerTimeOfDayTimerRecord *timerRecord =
				&g_triggerDb.timeOfDayTimers[appIndex][m_timerSlot];
			bool recordIsDaily =
				(timerRecord->flags & TRIGGER_TIMER_FLAG_IS_DAILY) != 0;
			if (m_isDaily == recordIsDaily)
				finalResult = timerRecord->lastError;
			else
				finalResult = TRIGGER_MGR_ERROR_TIMER_TYPE_MISMATCH;
		}
	}

	triggerMgrCompleteSimpleAsyncJob(this, finalResult);
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

/* Job to register a repeating daily timer (0x812A7E94) */
class RegisterDailyTimerJob : public TriggerIpcJobBase {
public:
	RegisterDailyTimerJob(IPMI::Session *session, SceUInt32 requestId,
			      const char *targetTitleId,
			      const TriggerDailyTimerParam *timerParam,
			      SceUInt32 timerSlot, bool useCallback);
	virtual ~RegisterDailyTimerJob();
	virtual int Run();

	char m_targetTitleId[12];
	TriggerDailyTimerParam m_timerParam;
	SceUInt32 m_timerSlot;
};

RegisterDailyTimerJob::RegisterDailyTimerJob(IPMI::Session *session, SceUInt32 requestId,
					     const char *targetTitleId,
					     const TriggerDailyTimerParam *timerParam,
					     SceUInt32 timerSlot, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrJob", session, requestId, useCallback),
	  m_timerParam(*timerParam),
	  m_timerSlot(timerSlot)
{
	sce_paf_strncpy(m_targetTitleId, targetTitleId, TRIGGER_MGR_TITLE_ID_LEN);
	m_targetTitleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';
}

RegisterDailyTimerJob::~RegisterDailyTimerJob()
{
}

int RegisterDailyTimerJob::Run()
{
	char callerTitleId[12];
	SceUID clientPid = m_session->getProcessId();
	int resolveErr = triggerMgrResolveProcessTitleId(clientPid, callerTitleId);
	if (resolveErr != 0) {
		triggerMgrCompleteSimpleAsyncJob(this, resolveErr);
		return 0;
	}

	if (!triggerMgrIsTitleInstalled(m_targetTitleId)) {
		triggerMgrCompleteSimpleAsyncJob(this, TRIGGER_MGR_ERROR_APP_NOT_FOUND);
		return 0;
	}

	g_triggerMgrTableLock.WriterLock();
	int appIndex = triggerMgrFindAppIndex(callerTitleId);
	bool createdAppRecord = false;
	TriggerAppRecord *appRecord = NULL;
	if (appIndex >= 0) {
		appRecord = triggerMgrGetAppRecord(appIndex);
	} else {
		appIndex = triggerMgrFindFreeAppIndex();
		if (appIndex < 0) {
			triggerMgrCompleteSimpleAsyncJob(this, TRIGGER_MGR_ERROR_APP_TABLE_FULL);
			g_triggerMgrTableLock.WriterUnlock();
			return 0;
		}
		appRecord = triggerMgrGetAppRecord(appIndex);
		SceUInt32 regOrder = triggerMgrGetSequenceCounter();
		sce_paf_memset(appRecord, 0, sizeof(*appRecord));
		sce_paf_strncpy(appRecord->titleId, callerTitleId, TRIGGER_MGR_TITLE_ID_LEN);
		appRecord->registrationOrder = regOrder;
		appRecord->flags = TRIGGER_APP_FLAG_ENABLED;
		createdAppRecord = true;
	}

	SceUInt32 slotMask = s_timeOfDaySlotMask[m_timerSlot];
	if ((appRecord->flags & slotMask) != 0) {
		ShellCron *cron = shellCronGetInstance();
		cron->Remove(s_timeOfDayCronHandles[appIndex][m_timerSlot]);
		s_timeOfDayCronHandles[appIndex][m_timerSlot] = 0;
		appRecord->flags &= ~slotMask;
		sce_paf_memset(&g_triggerDb.timeOfDayTimers[appIndex][m_timerSlot], 0,
			       sizeof(TriggerTimeOfDayTimerRecord));
	}

	ShellCronOption cronOpt;
	cronOpt.wakeUp = ((appRecord->flags & TRIGGER_APP_FLAG_ENABLED) != 0) &&
			 m_timerParam.wakeUp;
	cronOpt.persist = false;

	SceUID newCronHandle = 0;
	ShellCron *cron = shellCronGetInstance();
	int addErr = cron->AddDaily(&newCronHandle, triggerTimeOfDayCronCallback, NULL,
				    (int)m_timerParam.secondOfDay,
				    (int)m_timerParam.dayOfWeekMask, &cronOpt);
	int finalResult;
	if (addErr != 0) {
		sceClibPrintf(
			"[SceTriggerMgr] ::shell::CronCallbackMgr::registerDailyTimerCallback() failed. 0x%x\n",
			addErr);
		finalResult = TRIGGER_MGR_ERROR_INTERNAL;
		if (createdAppRecord)
			sce_paf_memset(appRecord, 0, sizeof(*appRecord));
		g_triggerMgrTableLock.WriterUnlock();
	} else {
		appRecord->flags |= slotMask;
		TriggerTimeOfDayTimerRecord *timerRecord =
			&g_triggerDb.timeOfDayTimers[appIndex][m_timerSlot];
		sce_paf_memset(timerRecord, 0, sizeof(*timerRecord));
		sce_paf_strncpy(timerRecord->targetTitleId, m_targetTitleId,
				TRIGGER_MGR_TITLE_ID_LEN);
		timerRecord->scheduledTime = (SceUInt64)m_timerParam.secondOfDay;

		SceUInt32 entryFlags = TRIGGER_TIMER_FLAG_IS_DAILY |
				       (((SceUInt32)m_timerParam.dayOfWeekMask & 0x7Fu) << 16);
		if (m_timerParam.wakeUp)
			entryFlags |= TRIGGER_TIMER_FLAG_WAKE_UP;
		if (m_timerParam.waitLaunchResult)
			entryFlags |= TRIGGER_TIMER_FLAG_WAIT_LAUNCH;
		if (m_timerParam.resumeOnly)
			entryFlags |= TRIGGER_TIMER_FLAG_RESUME_ONLY;
		timerRecord->flags = entryFlags;

		s_timeOfDayCronHandles[appIndex][m_timerSlot] = newCronHandle;
		g_triggerMgrTableLock.WriterUnlock();

		g_triggerMgrTableLock.ReaderLock();
		triggerMgrSaveDatabase();
		g_triggerMgrTableLock.ReaderUnlock();
		finalResult = 0;
	}

	triggerMgrCompleteSimpleAsyncJob(this, finalResult);
	return 0;
}

/* Job to query configuration and remaining seconds of a daily timer (0x812A8144, 0x812A8226) */
class GetDailyTimerInfoJob : public TriggerIpcJobBase {
public:
	GetDailyTimerInfoJob(IPMI::Session *session, SceUInt32 requestId,
			     SceUInt32 timerSlot, bool useCallback);
	virtual ~GetDailyTimerInfoJob();
	void Complete(int result, const char *targetTitleId,
		      const TriggerDailyTimerParam *timerParam, SceInt32 remainingSeconds);
	virtual int Run();

	SceUInt32 m_timerSlot;
};

GetDailyTimerInfoJob::GetDailyTimerInfoJob(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 timerSlot, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrGetDailyTimerInfoJob", session, requestId, useCallback),
	  m_timerSlot(timerSlot)
{
}

GetDailyTimerInfoJob::~GetDailyTimerInfoJob()
{
}

void GetDailyTimerInfoJob::Complete(int result, const char *targetTitleId,
				    const TriggerDailyTimerParam *timerParam,
				    SceInt32 remainingSeconds)
{
	g_triggerMgrIpcLock.Lock();
	if (!IsCanceled()) {
		int respErr = triggerMgrRespondDailyTimerInfo(
			m_session, m_requestId, result, targetTitleId,
			timerParam, &remainingSeconds);
		if (respErr != 0)
			triggerMgrAssertFail(NULL, respErr);
		if (m_useCallback) {
			int notifyErr = shellSvcNotifyEvent(
				m_session, TRIGGER_MGR_NOTIFY_EVENT_FLAG);
			if (notifyErr != 0)
				triggerMgrAssertFail(NULL, notifyErr);
		}
		TriggerIpcJobSlot *slot = triggerMgrFindIpcSlot(m_session, m_requestId);
		if (slot == NULL)
			triggerMgrAssertFailNoCode();
		triggerMgrClearIpcSlot(slot);
	}
	g_triggerMgrIpcLock.Unlock();
}

int GetDailyTimerInfoJob::Run()
{
	char targetTitleId[16];
	TriggerDailyTimerParam timerParam;
	SceInt32 remainingSeconds = -1;
	sce_paf_memset(targetTitleId, 0, sizeof(targetTitleId));
	sce_paf_memset(&timerParam, 0, sizeof(timerParam));

	char callerTitleId[12];
	SceUID clientPid = m_session->getProcessId();
	int resolveErr = triggerMgrResolveProcessTitleId(clientPid, callerTitleId);
	if (resolveErr != 0) {
		Complete(resolveErr, targetTitleId, &timerParam, remainingSeconds);
		return 0;
	}

	g_triggerMgrTableLock.ReaderLock();
	int appIndex = triggerMgrFindAppIndex(callerTitleId);
	int finalResult = TRIGGER_MGR_ERROR_TIMER_NOT_FOUND;

	if (appIndex >= 0) {
		TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
		refreshExpiredStatusIfUnfired(appIndex, m_timerSlot);
		TriggerTimeOfDayTimerRecord *timerRecord =
			&g_triggerDb.timeOfDayTimers[appIndex][m_timerSlot];
		if ((appRecord->flags & s_timeOfDaySlotMask[m_timerSlot]) != 0) {
			if ((timerRecord->flags & TRIGGER_TIMER_FLAG_IS_DAILY) != 0) {
				ShellCron *cron = shellCronGetInstance();
				int querySeconds = -1;
				if (cron->GetRemaining(
					    s_timeOfDayCronHandles[appIndex][m_timerSlot],
					    &querySeconds) != 0) {
					finalResult = TRIGGER_MGR_ERROR_TIMER_NOT_FOUND;
				} else {
					remainingSeconds = querySeconds;
					sce_paf_strlcpy(targetTitleId, timerRecord->targetTitleId,
							TRIGGER_MGR_TITLE_ID_LEN);
					timerParam.secondOfDay =
						(SceUInt32)timerRecord->scheduledTime;
					timerParam.dayOfWeekMask =
						(SceUInt16)((timerRecord->flags >> 16) & 0x7Fu);
					timerParam.wakeUp =
						(timerRecord->flags & TRIGGER_TIMER_FLAG_WAKE_UP) != 0;
					timerParam.waitLaunchResult =
						(timerRecord->flags & TRIGGER_TIMER_FLAG_WAIT_LAUNCH) != 0;
					finalResult = 0;
				}
			} else {
				finalResult = TRIGGER_MGR_ERROR_TIMER_TYPE_MISMATCH;
			}
		}
	}

	Complete(finalResult, targetTitleId, &timerParam, remainingSeconds);
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

static int enqueueTimeOfDayIpcJob(IPMI::Session *session, SceUInt32 requestId,
				  job::JobItem *rawJob)
{
	common::SharedPtr<job::JobItem> jobRef(rawJob);
	if (rawJob == NULL || jobRef.get() == NULL)
		return TRIGGER_MGR_ERROR_NO_MEMORY;

	g_triggerMgrIpcLock.Lock();
	TriggerIpcJobSlot *slot = triggerMgrAllocIpcSlot(session, requestId, jobRef);
	g_triggerMgrIpcLock.Unlock();

	if (slot == NULL)
		return TRIGGER_MGR_ERROR_BUSY;

	g_triggerMgrJobQueue->Enqueue(jobRef);
	return 0;
}

} /* anonymous namespace */

/* Cron callback invoked on the ShellCron thread for time-of-day and daily timers (0x812A8414) */
static int triggerTimeOfDayCronCallback(SceUID cronHandle, void *userArg)
{
	(void)userArg;
	g_triggerMgrTableLock.WriterLock();

	for (int appIndex = 0; appIndex < TRIGGER_MGR_MAX_APPS; ++appIndex) {
		TriggerAppRecord *appRecord = &g_triggerDb.apps[appIndex];
		if (appRecord->titleId[0] == '\0')
			continue;

		for (SceUInt32 slot = 0; slot < TRIGGER_MGR_MAX_TIMEOFDAY_SLOTS; ++slot) {
			if ((appRecord->flags & s_timeOfDaySlotMask[slot]) == 0 ||
			    s_timeOfDayCronHandles[appIndex][slot] != cronHandle)
				continue;

			void *mem = triggerMgrHeapAlloc(sizeof(TimeOfDayTimerFireJob));
			TimeOfDayTimerFireJob *fireJob = NULL;
			if (mem != NULL)
				fireJob = new (mem) TimeOfDayTimerFireJob(appRecord->titleId, slot);

			common::SharedPtr<job::JobItem> jobRef(fireJob);
			if (fireJob != NULL && jobRef.get() != NULL) {
				g_triggerMgrJobQueue->Enqueue(jobRef);
			} else {
				g_triggerDb.timeOfDayTimers[appIndex][slot].lastError =
					TRIGGER_MGR_ERROR_NO_MEMORY;
				triggerMgrSaveDatabase();
			}
			g_triggerMgrTableLock.WriterUnlock();
			return 0;
		}
	}

	g_triggerMgrTableLock.WriterUnlock();
	return 0;
}

/* Re-registers all persisted time-of-day and daily timers on first service resume (0x812A85DA) */
int triggerTimeOfDayTimerRegisterAll(void)
{
	sce_paf_memset(s_timeOfDayCronHandles, 0, sizeof(s_timeOfDayCronHandles));

	for (int appIndex = 0; appIndex < TRIGGER_MGR_MAX_APPS; ++appIndex) {
		TriggerAppRecord *appRecord = &g_triggerDb.apps[appIndex];
		if (appRecord->titleId[0] == '\0')
			continue;

		for (SceUInt32 slot = 0; slot < TRIGGER_MGR_MAX_TIMEOFDAY_SLOTS; ++slot) {
			if ((appRecord->flags & s_timeOfDaySlotMask[slot]) == 0)
				continue;

			TriggerTimeOfDayTimerRecord *timerRecord =
				&g_triggerDb.timeOfDayTimers[appIndex][slot];
			ShellCronOption cronOpt;
			cronOpt.wakeUp = ((appRecord->flags & TRIGGER_APP_FLAG_ENABLED) != 0) &&
					 ((timerRecord->flags & TRIGGER_TIMER_FLAG_WAKE_UP) != 0);
			cronOpt.persist = false;

			ShellCron *cron = shellCronGetInstance();
			int addErr = 0;
			if ((timerRecord->flags & TRIGGER_TIMER_FLAG_IS_DAILY) != 0) {
				int dayMask = (int)((timerRecord->flags >> 16) & 0x7Fu);
				addErr = cron->AddDaily(
					&s_timeOfDayCronHandles[appIndex][slot],
					triggerTimeOfDayCronCallback, NULL,
					(int)timerRecord->scheduledTime, dayMask, &cronOpt);
			} else {
				if ((timerRecord->flags & TRIGGER_TIMER_FLAG_FIRED) != 0)
					continue;
				if ((timerRecord->flags & TRIGGER_TIMER_FLAG_PERSIST_IF_LATE) != 0)
					cronOpt.persist = true;
				addErr = cron->AddAt(
					&s_timeOfDayCronHandles[appIndex][slot],
					triggerTimeOfDayCronCallback, NULL,
					timerRecord->scheduledTime, &cronOpt);
			}

			if (addErr != 0)
				triggerMgrAssertFail(NULL, addErr);
		}
	}

	return 0;
}

/* Cancels all time-of-day and daily cron callbacks for an uninstalled application (0x812A876E) */
void triggerTimeOfDayTimerCancelAllForApp(int appIndex)
{
	TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
	for (SceUInt32 slot = 0; slot < TRIGGER_MGR_MAX_TIMEOFDAY_SLOTS; ++slot) {
		if ((appRecord->flags & s_timeOfDaySlotMask[slot]) != 0 &&
		    s_timeOfDayCronHandles[appIndex][slot] != 0) {
			ShellCron *cron = shellCronGetInstance();
			cron->Remove(s_timeOfDayCronHandles[appIndex][slot]);
			s_timeOfDayCronHandles[appIndex][slot] = 0;
		}
	}
}

/* Updates RTC wake-up state on time-of-day/daily timers of an application (0x812A87BE) */
int triggerTimeOfDayTimerSetWakeUpForApp(int appIndex, bool wakeUp)
{
	TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
	/* Matches Sony's 0x812A87BE loop bound (first 4 slots) */
	for (SceUInt32 slot = 0; slot < TRIGGER_MGR_MAX_PERIODIC_SLOTS; ++slot) {
		if ((appRecord->flags & s_timeOfDaySlotMask[slot]) != 0 &&
		    (g_triggerDb.timeOfDayTimers[appIndex][slot].flags & TRIGGER_TIMER_FLAG_WAKE_UP) != 0) {
			ShellCron *cron = shellCronGetInstance();
			cron->SetWakeUp(s_timeOfDayCronHandles[appIndex][slot], wakeUp);
		}
	}
	return 0;
}

/* Time-of-day IPC enqueue entry points (0x812A8828-0x812A8DEC) */
int triggerTimeOfDayTimerEnqueueRegister(IPMI::Session *session, SceUInt32 requestId,
					 const char *targetTitleId,
					 const TriggerTimeOfDayTimerParam *timerParam,
					 SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(RegisterTimeOfDayTimerJob));
	RegisterTimeOfDayTimerJob *job = NULL;
	if (mem != NULL)
		job = new (mem) RegisterTimeOfDayTimerJob(
			session, requestId, targetTitleId, timerParam, timerSlot, useCallback);
	return enqueueTimeOfDayIpcJob(session, requestId, job);
}

int triggerTimeOfDayTimerEnqueueUnregister(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(UnregisterTimeOfDayTimerJob));
	UnregisterTimeOfDayTimerJob *job = NULL;
	if (mem != NULL)
		job = new (mem) UnregisterTimeOfDayTimerJob(
			session, requestId, timerSlot, false, useCallback);
	return enqueueTimeOfDayIpcJob(session, requestId, job);
}

int triggerTimeOfDayTimerEnqueueGetTime(IPMI::Session *session, SceUInt32 requestId,
					SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(GetTimeOfDayTimerTimeJob));
	GetTimeOfDayTimerTimeJob *job = NULL;
	if (mem != NULL)
		job = new (mem) GetTimeOfDayTimerTimeJob(
			session, requestId, timerSlot, useCallback);
	return enqueueTimeOfDayIpcJob(session, requestId, job);
}

int triggerTimeOfDayTimerEnqueueGetLastError(IPMI::Session *session, SceUInt32 requestId,
					     SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(GetTimeOfDayTimerLastErrorJob));
	GetTimeOfDayTimerLastErrorJob *job = NULL;
	if (mem != NULL)
		job = new (mem) GetTimeOfDayTimerLastErrorJob(
			session, requestId, timerSlot, false, useCallback);
	return enqueueTimeOfDayIpcJob(session, requestId, job);
}

/* Daily timer IPC enqueue entry points (0x812A8FAE-0x812A9544) */
int triggerDailyTimerEnqueueRegister(IPMI::Session *session, SceUInt32 requestId,
				     const char *targetTitleId,
				     const TriggerDailyTimerParam *timerParam,
				     SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(RegisterDailyTimerJob));
	RegisterDailyTimerJob *job = NULL;
	if (mem != NULL)
		job = new (mem) RegisterDailyTimerJob(
			session, requestId, targetTitleId, timerParam, timerSlot, useCallback);
	return enqueueTimeOfDayIpcJob(session, requestId, job);
}

int triggerDailyTimerEnqueueUnregister(IPMI::Session *session, SceUInt32 requestId,
				       SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(UnregisterTimeOfDayTimerJob));
	UnregisterTimeOfDayTimerJob *job = NULL;
	if (mem != NULL)
		job = new (mem) UnregisterTimeOfDayTimerJob(
			session, requestId, timerSlot, true, useCallback);
	return enqueueTimeOfDayIpcJob(session, requestId, job);
}

int triggerDailyTimerEnqueueGetInfo(IPMI::Session *session, SceUInt32 requestId,
				    SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(GetDailyTimerInfoJob));
	GetDailyTimerInfoJob *job = NULL;
	if (mem != NULL)
		job = new (mem) GetDailyTimerInfoJob(
			session, requestId, timerSlot, useCallback);
	return enqueueTimeOfDayIpcJob(session, requestId, job);
}

int triggerDailyTimerEnqueueGetLastError(IPMI::Session *session, SceUInt32 requestId,
					 SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(GetTimeOfDayTimerLastErrorJob));
	GetTimeOfDayTimerLastErrorJob *job = NULL;
	if (mem != NULL)
		job = new (mem) GetTimeOfDayTimerLastErrorJob(
			session, requestId, timerSlot, true, useCallback);
	return enqueueTimeOfDayIpcJob(session, requestId, job);
}
