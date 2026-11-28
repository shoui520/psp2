/*
 * periodic_timer.cpp: SceTriggerMgr periodic timer management
 * (0x812A60F0-0x812A7561).
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

static const SceUInt32 s_periodicSlotMask[TRIGGER_MGR_MAX_PERIODIC_SLOTS] = {
	0x00000001u, 0x00000002u, 0x00000004u, 0x00000008u
};

static SceUID s_periodicCronHandles[TRIGGER_MGR_MAX_APPS][TRIGGER_MGR_MAX_PERIODIC_SLOTS];

static int triggerPeriodicCronCallback(SceUID cronHandle, void *userArg);

namespace {

/* Job executed when a periodic timer fires (0x812A60F0) */
class PeriodicTimerFireJob : public TriggerFireJobBase {
public:
	PeriodicTimerFireJob(const char *appTitleId, SceUInt32 timerSlot);
	virtual ~PeriodicTimerFireJob();
	virtual int Run();
};

PeriodicTimerFireJob::PeriodicTimerFireJob(const char *appTitleId, SceUInt32 timerSlot)
	: TriggerFireJobBase(appTitleId, timerSlot)
{
}

PeriodicTimerFireJob::~PeriodicTimerFireJob()
{
}

int PeriodicTimerFireJob::Run()
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
	if (appRecord == NULL || (appRecord->flags & s_periodicSlotMask[m_timerSlot]) == 0) {
		g_triggerMgrTableLock.WriterUnlock();
		return 0;
	}

	TriggerPeriodicTimerRecord *timerRecord =
		&g_triggerDb.periodicTimers[appIndex][m_timerSlot];
	SceUInt64 intervalUsecs =
		(SceUInt64)(60000u * (SceUInt32)timerRecord->intervalMinutes) * 1000ull;
	timerRecord->nextFireTick = currentTick.tick + intervalUsecs;

	if ((appRecord->flags & TRIGGER_APP_FLAG_ENABLED) != 0) {
		char targetTitleId[12];
		char launchArg[128];
		sce_paf_strncpy(targetTitleId, timerRecord->targetTitleId,
				TRIGGER_MGR_TITLE_ID_LEN);
		targetTitleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';

		bool waitLaunch = (timerRecord->flags & TRIGGER_TIMER_FLAG_WAIT_LAUNCH) != 0;
		bool resumeOnly = (timerRecord->flags & TRIGGER_TIMER_FLAG_RESUME_ONLY) != 0;
		sce_paf_snprintf(launchArg, sizeof(launchArg),
				 "trigger_util:type=PeriodicTimer&slot=%u", m_timerSlot);

		g_triggerMgrTableLock.WriterUnlock();
		int launchResult = triggerMgrLaunchOrResumeApp(
			this, targetTitleId, launchArg, waitLaunch, resumeOnly);
		g_triggerMgrTableLock.WriterLock();

		int refreshedIndex = triggerMgrFindAppIndex(m_appTitleId);
		if (refreshedIndex < 0) {
			g_triggerMgrTableLock.WriterUnlock();
			return 0;
		}
		g_triggerDb.periodicTimers[refreshedIndex][m_timerSlot].lastError = launchResult;
	} else {
		timerRecord->lastError = TRIGGER_MGR_ERROR_DISABLED;
	}

	g_triggerMgrTableLock.WriterUnlock();
	g_triggerMgrTableLock.ReaderLock();
	triggerMgrSaveDatabase();
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

/* Job to register or replace a periodic timer (0x812A62B6) */
class RegisterPeriodicTimerJob : public TriggerIpcJobBase {
public:
	RegisterPeriodicTimerJob(IPMI::Session *session, SceUInt32 requestId,
				 const char *targetTitleId,
				 const TriggerPeriodicTimerParam *timerParam,
				 SceUInt32 timerSlot, bool useCallback);
	virtual ~RegisterPeriodicTimerJob();
	virtual int Run();

	char m_targetTitleId[12];
	TriggerPeriodicTimerParam m_timerParam;
	SceUInt32 m_timerSlot;
};

RegisterPeriodicTimerJob::RegisterPeriodicTimerJob(IPMI::Session *session, SceUInt32 requestId,
						   const char *targetTitleId,
						   const TriggerPeriodicTimerParam *timerParam,
						   SceUInt32 timerSlot, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrJob", session, requestId, useCallback),
	  m_timerParam(*timerParam),
	  m_timerSlot(timerSlot)
{
	sce_paf_strncpy(m_targetTitleId, targetTitleId, TRIGGER_MGR_TITLE_ID_LEN);
	m_targetTitleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';
}

RegisterPeriodicTimerJob::~RegisterPeriodicTimerJob()
{
}

int RegisterPeriodicTimerJob::Run()
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

	SceUInt32 slotMask = s_periodicSlotMask[m_timerSlot];
	if ((appRecord->flags & slotMask) != 0) {
		ShellCron *cron = shellCronGetInstance();
		int removeErr = cron->Remove(s_periodicCronHandles[appIndex][m_timerSlot]);
		if (removeErr != 0)
			triggerMgrAssertFail(NULL, removeErr);
		s_periodicCronHandles[appIndex][m_timerSlot] = 0;
		appRecord->flags &= ~slotMask;
		sce_paf_memset(&g_triggerDb.periodicTimers[appIndex][m_timerSlot], 0,
			       sizeof(TriggerPeriodicTimerRecord));
	}

	ShellCronOption cronOpt;
	cronOpt.wakeUp = ((appRecord->flags & TRIGGER_APP_FLAG_ENABLED) != 0) &&
			 m_timerParam.wakeUp;
	cronOpt.persist = false;

	SceUID newCronHandle = 0;
	ShellCron *cron = shellCronGetInstance();
	int addErr = cron->AddInterval(&newCronHandle, triggerPeriodicCronCallback, NULL,
				       60u * m_timerParam.initialDelayMinutes,
				       60u * m_timerParam.intervalMinutes, &cronOpt);
	int finalResult;
	if (addErr != 0) {
		sceClibPrintf(
			"[SceTriggerMgr] ::shell::CronCallbackMgr::registerCallback() failed. 0x%x\n",
			addErr);
		finalResult = TRIGGER_MGR_ERROR_INTERNAL;
		if (createdAppRecord)
			sce_paf_memset(appRecord, 0, sizeof(*appRecord));
		g_triggerMgrTableLock.WriterUnlock();
	} else {
		SceRtcTick nowTick;
		sceRtcGetCurrentTick(&nowTick);

		appRecord->flags |= slotMask;
		TriggerPeriodicTimerRecord *timerRecord =
			&g_triggerDb.periodicTimers[appIndex][m_timerSlot];
		sce_paf_memset(timerRecord, 0, sizeof(*timerRecord));
		sce_paf_strncpy(timerRecord->targetTitleId, m_targetTitleId,
				TRIGGER_MGR_TITLE_ID_LEN);
		timerRecord->initialDelayMinutes = (SceUInt16)m_timerParam.initialDelayMinutes;
		timerRecord->intervalMinutes = (SceUInt16)m_timerParam.intervalMinutes;
		timerRecord->nextFireTick =
			nowTick.tick +
			(SceUInt64)(60000u * m_timerParam.initialDelayMinutes) * 1000ull;

		if (m_timerParam.persistOnSleep)
			timerRecord->flags |= TRIGGER_TIMER_FLAG_PERSIST_ON_SLEEP;
		if (m_timerParam.wakeUp)
			timerRecord->flags |= TRIGGER_TIMER_FLAG_WAKE_UP;
		if (m_timerParam.waitLaunchResult)
			timerRecord->flags |= TRIGGER_TIMER_FLAG_WAIT_LAUNCH;
		if (m_timerParam.resumeOnly)
			timerRecord->flags |= TRIGGER_TIMER_FLAG_RESUME_ONLY;

		s_periodicCronHandles[appIndex][m_timerSlot] = newCronHandle;
		g_triggerMgrTableLock.WriterUnlock();

		g_triggerMgrTableLock.ReaderLock();
		triggerMgrSaveDatabase();
		g_triggerMgrTableLock.ReaderUnlock();
		finalResult = 0;
	}

	triggerMgrCompleteSimpleAsyncJob(this, finalResult);
	return 0;
}

/* Job to unregister an active periodic timer (0x812A65E4) */
class UnregisterPeriodicTimerJob : public TriggerIpcJobBase {
public:
	UnregisterPeriodicTimerJob(IPMI::Session *session, SceUInt32 requestId,
				   SceUInt32 timerSlot, bool useCallback);
	virtual ~UnregisterPeriodicTimerJob();
	virtual int Run();

	SceUInt32 m_timerSlot;
};

UnregisterPeriodicTimerJob::UnregisterPeriodicTimerJob(IPMI::Session *session,
						       SceUInt32 requestId, SceUInt32 timerSlot,
						       bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrJob", session, requestId, useCallback),
	  m_timerSlot(timerSlot)
{
}

UnregisterPeriodicTimerJob::~UnregisterPeriodicTimerJob()
{
}

int UnregisterPeriodicTimerJob::Run()
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
		SceUInt32 slotMask = s_periodicSlotMask[m_timerSlot];
		if ((appRecord->flags & slotMask) != 0) {
			ShellCron *cron = shellCronGetInstance();
			int removeErr =
				cron->Remove(s_periodicCronHandles[appIndex][m_timerSlot]);
			if (removeErr != 0)
				triggerMgrAssertFail(NULL, removeErr);
			s_periodicCronHandles[appIndex][m_timerSlot] = 0;
			appRecord->flags &= ~slotMask;
			sce_paf_memset(&g_triggerDb.periodicTimers[appIndex][m_timerSlot],
				       0, sizeof(TriggerPeriodicTimerRecord));
			g_triggerMgrTableLock.WriterUnlock();

			g_triggerMgrTableLock.ReaderLock();
			triggerMgrSaveDatabase();
			g_triggerMgrTableLock.ReaderUnlock();
			finalResult = 0;
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

/* Job to query remaining minutes until next periodic timer fire (0x812A6738, 0x812A6806) */
class GetPeriodicTimerRemainingTimeJob : public TriggerIpcJobBase {
public:
	GetPeriodicTimerRemainingTimeJob(IPMI::Session *session, SceUInt32 requestId,
					 SceUInt32 timerSlot, bool useCallback);
	virtual ~GetPeriodicTimerRemainingTimeJob();
	void Complete(int result, SceUInt32 remainingMinutes);
	virtual int Run();

	SceUInt32 m_timerSlot;
};

GetPeriodicTimerRemainingTimeJob::GetPeriodicTimerRemainingTimeJob(IPMI::Session *session,
								   SceUInt32 requestId,
								   SceUInt32 timerSlot,
								   bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrGetPeriodicTimerRemainingTimeJob",
			    session, requestId, useCallback),
	  m_timerSlot(timerSlot)
{
}

GetPeriodicTimerRemainingTimeJob::~GetPeriodicTimerRemainingTimeJob()
{
}

/* 0x812A6738 */
void GetPeriodicTimerRemainingTimeJob::Complete(int result, SceUInt32 remainingMinutes)
{
	g_triggerMgrIpcLock.Lock();
	if (!IsCanceled()) {
		int respErr = triggerMgrRespondRemainingTime(
			m_session, m_requestId, result, &remainingMinutes);
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

/* 0x812A6806 */
int GetPeriodicTimerRemainingTimeJob::Run()
{
	char callerTitleId[12];
	SceUID clientPid = m_session->getProcessId();
	int resolveErr = triggerMgrResolveProcessTitleId(clientPid, callerTitleId);
	if (resolveErr != 0) {
		Complete(resolveErr, 0);
		return 0;
	}

	g_triggerMgrTableLock.ReaderLock();
	int appIndex = triggerMgrFindAppIndex(callerTitleId);
	int finalResult = TRIGGER_MGR_ERROR_TIMER_NOT_FOUND;
	SceUInt32 remainingMinutes = 0;

	if (appIndex >= 0) {
		TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
		if ((appRecord->flags & s_periodicSlotMask[m_timerSlot]) != 0) {
			int remainingSeconds = 0;
			ShellCron *cron = shellCronGetInstance();
			int queryErr = cron->GetRemaining(
				s_periodicCronHandles[appIndex][m_timerSlot],
				&remainingSeconds);
			if (queryErr != 0)
				triggerMgrAssertFail(NULL, queryErr);
			finalResult = 0;
			remainingMinutes = (SceUInt32)remainingSeconds / 60u;
		}
	}

	Complete(finalResult, remainingMinutes);
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

/* Job to query the last launch error for a periodic timer slot (0x812A6904) */
class GetPeriodicTimerLastErrorJob : public TriggerIpcJobBase {
public:
	GetPeriodicTimerLastErrorJob(IPMI::Session *session, SceUInt32 requestId,
				     SceUInt32 timerSlot, bool useCallback);
	virtual ~GetPeriodicTimerLastErrorJob();
	virtual int Run();

	SceUInt32 m_timerSlot;
};

GetPeriodicTimerLastErrorJob::GetPeriodicTimerLastErrorJob(IPMI::Session *session,
							   SceUInt32 requestId,
							   SceUInt32 timerSlot, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrJob", session, requestId, useCallback),
	  m_timerSlot(timerSlot)
{
}

GetPeriodicTimerLastErrorJob::~GetPeriodicTimerLastErrorJob()
{
}

int GetPeriodicTimerLastErrorJob::Run()
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
		if ((appRecord->flags & s_periodicSlotMask[m_timerSlot]) != 0)
			finalResult =
				g_triggerDb.periodicTimers[appIndex][m_timerSlot].lastError;
	}
	triggerMgrCompleteSimpleAsyncJob(this, finalResult);
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

} /* anonymous namespace */

/* Cron callback invoked on the ShellCron thread when a periodic entry fires (0x812A69BA) */
static int triggerPeriodicCronCallback(SceUID cronHandle, void *userArg)
{
	(void)userArg;
	g_triggerMgrTableLock.WriterLock();

	for (int appIndex = 0; appIndex < TRIGGER_MGR_MAX_APPS; ++appIndex) {
		TriggerAppRecord *appRecord = &g_triggerDb.apps[appIndex];
		if (appRecord->titleId[0] == '\0')
			continue;

		for (SceUInt32 slot = 0; slot < TRIGGER_MGR_MAX_PERIODIC_SLOTS; ++slot) {
			if ((appRecord->flags & s_periodicSlotMask[slot]) == 0 ||
			    s_periodicCronHandles[appIndex][slot] != cronHandle)
				continue;

			void *mem = triggerMgrHeapAlloc(sizeof(PeriodicTimerFireJob));
			PeriodicTimerFireJob *fireJob = NULL;
			if (mem != NULL)
				fireJob = new (mem) PeriodicTimerFireJob(appRecord->titleId, slot);

			common::SharedPtr<job::JobItem> jobRef(fireJob);
			if (fireJob != NULL && jobRef.get() != NULL) {
				g_triggerMgrJobQueue->Enqueue(jobRef);
			} else {
				g_triggerDb.periodicTimers[appIndex][slot].lastError =
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

/* Re-registers all persisted periodic timers on first service resume (0x812A6B90) */
int triggerPeriodicTimerRegisterAll(void)
{
	sce_paf_memset(s_periodicCronHandles, 0, sizeof(s_periodicCronHandles));

	for (int appIndex = 0; appIndex < TRIGGER_MGR_MAX_APPS; ++appIndex) {
		TriggerAppRecord *appRecord = &g_triggerDb.apps[appIndex];
		if (appRecord->titleId[0] == '\0')
			continue;

		for (SceUInt32 slot = 0; slot < TRIGGER_MGR_MAX_PERIODIC_SLOTS; ++slot) {
			if ((appRecord->flags & s_periodicSlotMask[slot]) == 0)
				continue;

			TriggerPeriodicTimerRecord *timerRecord =
				&g_triggerDb.periodicTimers[appIndex][slot];
			ShellCronOption cronOpt;
			cronOpt.wakeUp = ((appRecord->flags & TRIGGER_APP_FLAG_ENABLED) != 0) &&
					 ((timerRecord->flags & TRIGGER_TIMER_FLAG_WAKE_UP) != 0);
			cronOpt.persist = false;

			SceRtcTick currentTick;
			sceRtcGetCurrentTick(&currentTick);

			SceUInt32 diffMinutes;
			if (timerRecord->nextFireTick < currentTick.tick) {
				timerRecord->lastError = TRIGGER_MGR_ERROR_MISSED;
				diffMinutes = (SceUInt32)((currentTick.tick - timerRecord->nextFireTick) /
							  60000000ull);
			} else {
				diffMinutes = (SceUInt32)((timerRecord->nextFireTick - currentTick.tick) /
							  60000000ull);
			}

			SceUInt32 intervalMinutes = (SceUInt32)timerRecord->intervalMinutes;
			SceUInt32 delaySeconds = (diffMinutes % intervalMinutes) * 60u;
			SceUInt32 intervalSeconds = intervalMinutes * 60u;

			ShellCron *cron = shellCronGetInstance();
			int addErr = cron->AddInterval(&s_periodicCronHandles[appIndex][slot],
						       triggerPeriodicCronCallback, NULL,
						       delaySeconds, intervalSeconds, &cronOpt);
			if (addErr != 0)
				triggerMgrAssertFail(NULL, addErr);
		}
	}

	return 0;
}

/* Cancels all periodic cron callbacks for an uninstalled application (0x812A6D36) */
void triggerPeriodicTimerCancelAllForApp(int appIndex)
{
	TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
	for (SceUInt32 slot = 0; slot < TRIGGER_MGR_MAX_PERIODIC_SLOTS; ++slot) {
		if ((appRecord->flags & s_periodicSlotMask[slot]) != 0 &&
		    s_periodicCronHandles[appIndex][slot] != 0) {
			ShellCron *cron = shellCronGetInstance();
			cron->Remove(s_periodicCronHandles[appIndex][slot]);
			s_periodicCronHandles[appIndex][slot] = 0;
		}
	}
}

/* Updates RTC wake-up state on all periodic timers of an application (0x812A6D86) */
int triggerPeriodicTimerSetWakeUpForApp(int appIndex, bool wakeUp)
{
	TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIndex);
	for (SceUInt32 slot = 0; slot < TRIGGER_MGR_MAX_PERIODIC_SLOTS; ++slot) {
		if ((appRecord->flags & s_periodicSlotMask[slot]) != 0 &&
		    (g_triggerDb.periodicTimers[appIndex][slot].flags & TRIGGER_TIMER_FLAG_WAKE_UP) != 0) {
			ShellCron *cron = shellCronGetInstance();
			int setErr = cron->SetWakeUp(s_periodicCronHandles[appIndex][slot], wakeUp);
			if (setErr != 0)
				triggerMgrAssertFail(NULL, setErr);
		}
	}
	return 0;
}

/* IPC enqueue entry points (0x812A6E12, 0x812A7026, 0x812A71E4, 0x812A73A2) */
/* 0x812A6E12 */
int triggerPeriodicTimerEnqueueRegister(IPMI::Session *session, SceUInt32 requestId,
					const char *targetTitleId,
					const TriggerPeriodicTimerParam *timerParam,
					SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(RegisterPeriodicTimerJob));
	RegisterPeriodicTimerJob *job = NULL;
	if (mem != NULL)
		job = new (mem) RegisterPeriodicTimerJob(
			session, requestId, targetTitleId, timerParam, timerSlot, useCallback);
	common::SharedPtr<job::JobItem> jobRef(job);
	if (job == NULL || jobRef.get() == NULL)
		return TRIGGER_MGR_ERROR_NO_MEMORY;

	g_triggerMgrIpcLock.Lock();
	TriggerIpcJobSlot *slot = triggerMgrAllocIpcSlot(session, requestId, jobRef);
	g_triggerMgrIpcLock.Unlock();

	if (slot == NULL)
		return TRIGGER_MGR_ERROR_BUSY;

	g_triggerMgrJobQueue->Enqueue(jobRef);
	return 0;
}

/* 0x812A7026 */
int triggerPeriodicTimerEnqueueUnregister(IPMI::Session *session, SceUInt32 requestId,
					  SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(UnregisterPeriodicTimerJob));
	UnregisterPeriodicTimerJob *job = NULL;
	if (mem != NULL)
		job = new (mem) UnregisterPeriodicTimerJob(
			session, requestId, timerSlot, useCallback);
	common::SharedPtr<job::JobItem> jobRef(job);
	if (job == NULL || jobRef.get() == NULL)
		return TRIGGER_MGR_ERROR_NO_MEMORY;

	g_triggerMgrIpcLock.Lock();
	TriggerIpcJobSlot *slot = triggerMgrAllocIpcSlot(session, requestId, jobRef);
	g_triggerMgrIpcLock.Unlock();

	if (slot == NULL)
		return TRIGGER_MGR_ERROR_BUSY;

	g_triggerMgrJobQueue->Enqueue(jobRef);
	return 0;
}

/* 0x812A71E4 */
int triggerPeriodicTimerEnqueueGetRemainingTime(IPMI::Session *session, SceUInt32 requestId,
						SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(GetPeriodicTimerRemainingTimeJob));
	GetPeriodicTimerRemainingTimeJob *job = NULL;
	if (mem != NULL)
		job = new (mem) GetPeriodicTimerRemainingTimeJob(
			session, requestId, timerSlot, useCallback);
	common::SharedPtr<job::JobItem> jobRef(job);
	if (job == NULL || jobRef.get() == NULL)
		return TRIGGER_MGR_ERROR_NO_MEMORY;

	g_triggerMgrIpcLock.Lock();
	TriggerIpcJobSlot *slot = triggerMgrAllocIpcSlot(session, requestId, jobRef);
	g_triggerMgrIpcLock.Unlock();

	if (slot == NULL)
		return TRIGGER_MGR_ERROR_BUSY;

	g_triggerMgrJobQueue->Enqueue(jobRef);
	return 0;
}

/* 0x812A73A2 */
int triggerPeriodicTimerEnqueueGetLastError(IPMI::Session *session, SceUInt32 requestId,
					    SceUInt32 timerSlot, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(GetPeriodicTimerLastErrorJob));
	GetPeriodicTimerLastErrorJob *job = NULL;
	if (mem != NULL)
		job = new (mem) GetPeriodicTimerLastErrorJob(
			session, requestId, timerSlot, useCallback);
	common::SharedPtr<job::JobItem> jobRef(job);
	if (job == NULL || jobRef.get() == NULL)
		return TRIGGER_MGR_ERROR_NO_MEMORY;

	g_triggerMgrIpcLock.Lock();
	TriggerIpcJobSlot *slot = triggerMgrAllocIpcSlot(session, requestId, jobRef);
	g_triggerMgrIpcLock.Unlock();

	if (slot == NULL)
		return TRIGGER_MGR_ERROR_BUSY;

	g_triggerMgrJobQueue->Enqueue(jobRef);
	return 0;
}
