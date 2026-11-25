/*
 * location_timer.cpp: location service periodic task scheduler ("SceLocationTimer",
 * "SceLocationTimerComplete", paf::TimerListener integration; 0x8118A764-0x8118AE80).
 * The file's and internal names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <liblocation.h>
#include <rtc.h>
#include <kernel/threadmgr.h>
#include <std/string.h>
#include <timer/timer.h>
#include <timer/timerlistener.h>
#include <timer/timerlistenerlist.h>
#include "location_internal.h"

extern "C" SceUInt64 sceRtcGetAccumulativeTime(void);

using namespace paf;

/* (0x8118A764) */
int LocationTimerMgr::Unlock()
{
	return sceKernelUnlockLwMutex(&m_mutex, 1);
}

/* (0x8118A770) */
int LocationTimerMgr::Lock()
{
	return sceKernelLockLwMutex(&m_mutex, 1, NULL);
}

/* (0x8118A780) */
int LocationTask::Schedule(unsigned int delayMs)
{
	if (m_mgr == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	int ret = sceKernelLockLwMutex(&m_mgr->m_mutex, 1, NULL);

	if (ret == 0) {
		LocationTimerSlot *slot = &m_mgr->m_slots[m_slotIndex];

		if (slot->state == 1 || slot->state == 2) {
			slot->interval = (SceUInt64)delayMs * 1000ULL;
			slot->startTime = sceRtcGetAccumulativeTime();
			slot->state = 2;
		} else {
			ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		}
		int ur = sceKernelUnlockLwMutex(&m_mgr->m_mutex, 1);

		if (ur != 0)
			return ur;
	}
	return ret;
}

/* (0x8118A800) */
int LocationTask::Cancel()
{
	if (m_mgr == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	int ret = sceKernelLockLwMutex(&m_mgr->m_mutex, 1, NULL);

	if (ret == 0) {
		m_mgr->m_slots[m_slotIndex].state = 1;
		return sceKernelUnlockLwMutex(&m_mgr->m_mutex, 1);
	}
	return ret;
}

/* (0x8118A838) */
LocationTimerMgr::LocationTimerMgr(Timer *timer) : m_listener(timer, this)
{
	m_stopping = 0;
	m_mutexResult = -1;
	m_threadId = -1;
	m_eventFlag = -1;
	sce_paf_memset(m_slots, 0, sizeof(m_slots));
	m_slotCount = 0;
	m_idle = 1;
	m_activeCount = 0;
	sce_paf_memset(m_observers, 0, sizeof(m_observers));
}

/* (0x8118A894) */
__attribute__((noinline))
int LocationTimerMgr::Term()
{
	TimerListenerList::s_default_list->Unregister(&m_listener);
	if (m_threadId >= 0) {
		SceUInt32 numWait = 0;

		m_stopping = 1;
		sceKernelCancelEventFlag(m_eventFlag, 1, &numWait);
		sceKernelWaitThreadEnd(m_threadId, NULL, NULL);
		sceKernelDeleteThread(m_threadId);
		m_threadId = -1;
	}
	if (m_completeEvent >= 0) {
		sceKernelDeleteEventFlag(m_completeEvent);
		m_completeEvent = -1;
	}
	if (m_eventFlag >= 0) {
		sceKernelDeleteEventFlag(m_eventFlag);
		m_eventFlag = -1;
	}
	if (m_mutexResult == 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}
	return 0;
}

/* (0x8118A930) */
LocationTimerMgr::~LocationTimerMgr()
{
	Term();
}

/* (0x8118A954) */
int LocationTimerMgr::Init()
{
	m_mutexResult = -1;
	m_threadId = -1;
	m_eventFlag = -1;
	m_stopping = 0;
	sce_paf_memset(m_slots, 0, sizeof(m_slots));
	m_slotCount = 0;
	m_activeCount = 0;
	m_idle = 1;
	sce_paf_memset(m_observers, 0, sizeof(m_observers));

	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, "SceLocationTimer", 2, 0, NULL);
	if (m_mutexResult != 0)
		return m_mutexResult;

	m_eventFlag = sceKernelCreateEventFlag("SceLocationTimer", 0, 0, NULL);
	if (m_eventFlag < 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
		return m_eventFlag;
	}

	m_completeEvent = sceKernelCreateEventFlag("SceLocationTimerComplete", 0, 0, NULL);
	if (m_completeEvent < 0) {
		sceKernelDeleteEventFlag(m_eventFlag);
		sceKernelDeleteLwMutex(&m_mutex);
		m_eventFlag = -1;
		m_mutexResult = -1;
		return m_completeEvent;
	}

	m_threadId = sceKernelCreateThread("SceLocationTimer", ThreadEntry, 0x1000010A, 0x1000,
					   0, 0, NULL);
	if (m_threadId < 0) {
		sceKernelDeleteEventFlag(m_completeEvent);
		sceKernelDeleteEventFlag(m_eventFlag);
		sceKernelDeleteLwMutex(&m_mutex);
		m_eventFlag = -1;
		m_mutexResult = -1;
		return m_threadId;
	}

	LocationTimerMgr *self = this;
	int ret = sceKernelStartThread(m_threadId, sizeof(self), &self);

	if (ret >= 0) {
		TimerListenerList::ListenerParam param(&m_listener, true, NULL, NULL);

		TimerListenerList::s_default_list->Register(param);
	} else {
		sceKernelDeleteThread(m_threadId);
		sceKernelDeleteEventFlag(m_eventFlag);
		sceKernelDeleteLwMutex(&m_mutex);
		m_threadId = -1;
		m_eventFlag = -1;
		m_mutexResult = -1;
	}
	return ret;
}

/* (0x8118AACE) */
int LocationTimerMgr::RegisterTask(LocationTask *task)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);

	if (ret == 0) {
		if (m_slotCount == 16) {
			ret = SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
		} else {
			int found = 0;

			for (int i = 0; i < 16 && !found; i++) {
				if (m_slots[i].state == 0) {
					found = 1;
					m_slots[i].task = task;
					m_slots[i].interval = 0;
					m_slots[i].startTime = 0;
					m_slots[i].state = 1;
					m_slotCount++;
					task->m_mgr = this;
					task->m_slotIndex = i;
				}
			}
			if (!found)
				ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		}
		int ur = sceKernelUnlockLwMutex(&m_mutex, 1);

		if (ur < 0)
			return ur;
	}
	return ret;
}

/* (0x8118AB62) */
int LocationTimerMgr::UnregisterTask(LocationTask *task)
{
	int idx = task->m_slotIndex;
	unsigned int outBits = 0;
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);

	if (ret == 0) {
		sceKernelClearEventFlag(m_completeEvent, ~(1u << idx));
		m_slots[idx].state = 3;
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		if (ret == 0)
			return sceKernelWaitEventFlag(m_completeEvent, 1u << idx,
						      SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL,
						      &outBits, NULL);
	}
	return ret;
}

/* (0x8118ABDC) */
int LocationTimerMgr::ReleaseActive()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);

	if (ret == 0) {
		if (m_activeCount != 0) {
			m_activeCount--;
			if (m_activeCount == 0) {
				m_idle = 1;
				for (int i = 0; i < 1; i++) {
					if (m_observers[i] != NULL)
						m_observers[i]->OnSuspend();
				}
			}
		}
		int ur = sceKernelUnlockLwMutex(&m_mutex, 1);

		if (ur < 0)
			return ur;
	}
	return ret;
}

/* (0x8118AC32) */
int LocationTimerMgr::AcquireActive()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);

	if (ret == 0) {
		m_activeCount++;
		if (m_activeCount == 1) {
			m_idle = 0;
			for (int i = 0; i < 1; i++) {
				if (m_observers[i] != NULL)
					m_observers[i]->OnResume();
			}
		}
		int ur = sceKernelUnlockLwMutex(&m_mutex, 1);

		if (ur < 0)
			return ur;
	}
	return ret;
}

/* (0x8118AC86) */
int LocationTimerMgr::RegisterObserver(LocationTimerObserver *obs)
{
	int found = 0;
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;

	for (int i = 0; i < 1 && !found; i++) {
		if (m_observers[i] == NULL) {
			found = 1;
			m_observers[i] = obs;
			ret = 0;
		}
	}
	return ret;
}

/* (0x8118ACBA) */
int LocationTimerMgr::UnregisterObserver(LocationTimerObserver *obs)
{
	int found = 0;
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;

	for (int i = 0; i < 1 && !found; i++) {
		if (m_observers[i] == obs) {
			found = 1;
			m_observers[i] = NULL;
			ret = 0;
		}
	}
	return ret;
}

/* (0x8118ACF0) */
void LocationTimerMgr::CheckTasks()
{
	SceUInt64 now = sceRtcGetAccumulativeTime();

	for (int i = 0; i < 16; i++) {
		if (sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
			LocationTimerSlot *slot = &m_slots[i];

			if (slot->state == 2) {
				if (now > slot->startTime) {
					SceUInt64 elapsed = now - slot->startTime;

					if (elapsed >= slot->interval) {
						SceUInt64 nextStart = now - (elapsed % slot->interval);

						sceKernelUnlockLwMutex(&m_mutex, 1);
						int keep = slot->task->OnNotify();

						sceKernelLockLwMutex(&m_mutex, 1, NULL);
						if (slot->state == 2) {
							if (keep == 1)
								slot->startTime = nextStart;
							else
								slot->state = 1;
						}
					}
				}
			} else if (slot->state == 3) {
				slot->task->m_slotIndex = 0;
				slot->task->m_mgr = NULL;
				slot->task = NULL;
				m_slotCount--;
				slot->interval = 0;
				slot->startTime = 0;
				slot->state = 0;
				sceKernelSetEventFlag(m_completeEvent, 1u << i);
			}
			sceKernelUnlockLwMutex(&m_mutex, 1);
		}
	}
}

/* (0x8118AE0E) */
int LocationTimerMgr::ThreadEntry(SceSize args, void *argp)
{
	LocationTimerMgr *self = *(LocationTimerMgr **)argp;
	unsigned int outBits = 0;

	self->m_stopping = 0;
	do {
		if (sceKernelWaitEventFlagCB(self->m_eventFlag, 1,
					     SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL,
					     &outBits, NULL) != 0)
			break;
		self->CheckTasks();
	} while (self->m_stopping == 0);
	return 0;
}

/* (0x8118AE56) */
void LocationTimerListener::OnTick(void *userArg, void *extraArg)
{
	(void)userArg;
	(void)extraArg;
	if (m_mgr->m_idle == 0)
		sceKernelSetEventFlag(m_mgr->m_eventFlag, 1);
}

/* (0x8118AE6A) */
void LocationTimerListener::OnFinish(void *userArg, void *extraArg)
{
	(void)userArg;
	(void)extraArg;
	if (m_mgr->m_idle != 0)
		sceKernelSetEventFlag(m_mgr->m_eventFlag, 1);
}
