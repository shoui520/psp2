/*
 * scheduler.cpp: the schedulers that run the chunks. Every scheduler keeps a queue of the
 * chunks given to it, ordered by the cost the chunks' types and seeks are estimated at, and
 * threads that take the first eligible chunk off it and execute it. The callback scheduler
 * here runs the roots and callbacks; the platform's I/O schedulers (platform.cpp) run the
 * chunks of the files their path prefixes own.
 */
#include <alloca.h>
#include "fios2_internal.h"

static Scheduler *s_pSchedulers = NULL;

static void ThreadEntry(uintptr_t arg);

Scheduler g_fiosScheduler = {
	0, NULL, { {}, -1, 0 }, {}, {}, { { -1 } }, "", 0, {}, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	false, true, {}, fiosSchedulerOwnsPrefix, s_callbackExecutors
};

/* a scheduler's profiling event (SCE_FIOS_PROFILE_EVENT_*) */
struct SchedulerProfile {
	SceFiosSchedulerProfileData data;
	SceFiosProfilingEvent event;
};

static inline void schedulerProfileBegin(SchedulerProfile *pProfile, uint16_t chunkID,
					 uint8_t eventType)
{
	if (g_FiosParams.pProfileCallback != NULL) {
		pProfile->data.chunkID = chunkID;
		pProfile->data.chunkType = fiosChunkAt(chunkID)->type;
		pProfile->data.threadId = Wws::Platform::GetCurrentThreadId();
	}
	if (g_FiosParams.pProfileCallback != NULL) {
		pProfile->event.startTime = sceKernelGetProcessTimeWide();
		pProfile->event.endTime = 0;
		pProfile->event.eventType = eventType;
		pProfile->event.data = &pProfile->data;
	}
}

static inline void schedulerProfileEnd(SchedulerProfile *pProfile)
{
	if (g_FiosParams.pProfileCallback != NULL) {
		pProfile->event.endTime = sceKernelGetProcessTimeWide();
		g_FiosParams.pProfileCallback(&pProfile->event);
	}
}

/* a chunk's deadline, five minutes from now when it has none */
static inline SceFiosTime chunkDeadline(FiosChunk *pChunk)
{
	if (pChunk->deadline == 0) {
		SceFiosTimeInterval timeout = sceFiosTimeIntervalFromNanoseconds(300000000000LL);

		pChunk->deadline = sceKernelGetProcessTimeWide() + timeout;
	}
	return pChunk->deadline;
}

/* the chunk types that work on a path: their thread resolves overlays */
static inline bool chunkUsesOverlays(uint8_t type)
{
	return type != CHUNK_CALLBACK && type != CHUNK_FH_READ && type != CHUNK_FH_CLOSE &&
	       type != CHUNK_ARCHIVE_UNMOUNT && type != CHUNK_FH_WRITE && type != CHUNK_FH_SYNC &&
	       type != CHUNK_DH_READ && type != CHUNK_DH_READ_BULK && type != CHUNK_DH_CLOSE &&
	       type != CHUNK_FH_BARRIER && type != CHUNK_NULL && type != CHUNK_ROOT &&
	       type != CHUNK_BUFFER && type != CHUNK_IOCTL && type != CHUNK_PARENT &&
	       type != CHUNK_CHILD;
}

void Scheduler::Terminate()
{
	if (!m_initialized)
		return;
	if (!m_shutdown) {
		m_mutex.Lock();
		m_shutdown = true;
		m_wakeCond.Broadcast();
		m_mutex.Unlock();
		for (int i = 0; i < m_threadCount; i++) {
			m_threads[i].Join();
			m_threads[i].Init();
		}
	}
	m_queueFullCond.Term();
	m_wakeCond.Term();
	m_mutex.Term();
	m_initialized = false;
	s_pSchedulers = m_pNext;
}

int Scheduler::Initialize()
{
	if (m_initialized)
		return SCE_OK;

	bool isCallback = this == &g_fiosScheduler;

	m_threadCount = isCallback ? 1 : g_FiosParams.threadsPerScheduler;
	m_initialized = true;
	m_queuePosition = 0;
	m_queueCount = 0;
	m_queueHighWater = 0;
	m_busySkip = 0;
	m_executing = 0;
	m_kickCount = 0;
	m_bufferBusyCount = 0;
	m_lastPosition = 0;
	sceClibMemset(m_queue, 0, sizeof(m_queue));
	m_location.h = 0;
	m_executeStart = 0;
	m_location.offset = 0;
	m_nameLength = fiosStrlen(m_pName);

	SceFiosTime slow = sceFiosTimeIntervalFromNanoseconds(6250000);
	SceFiosTime fast = sceFiosTimeIntervalFromNanoseconds(100000);

	for (int type = 0; type < 36; type++) {
		if (type > CHUNK_ROOT + 5 && type < CHUNK_FH_CLOSE)
			m_typeTimes[type] = slow;
		else
			m_typeTimes[type] = fast;
	}
	m_seekTimes[0] = sceFiosTimeIntervalFromNanoseconds(1000000);
	m_seekTimes[1] = sceFiosTimeIntervalFromNanoseconds(50000);
	m_mutex.Init(0, "SceFiosScheduler");
	m_wakeCond.Init(&m_mutex, "SceFiosSchedulerWakeup");
	m_queueFullCond.Init(&m_mutex, "SceFiosSchedulerQueueFull");

	Wws::Platform::ThreadParams params = { 0, 0, 0, 0, "" };

	if (isCallback) {
		params.stackSize = g_FiosParams.threadStackSize[SCE_FIOS_CALLBACK_THREAD];
		params.cpuAffinityMask = g_FiosParams.threadAffinity[SCE_FIOS_CALLBACK_THREAD];
		params.priority = g_FiosParams.threadPriority[SCE_FIOS_CALLBACK_THREAD];
		fiosStrlcpy(params.name, "SceFiosCallback", sizeof(params.name));
	} else {
		params.stackSize = g_FiosParams.threadStackSize[SCE_FIOS_IO_THREAD];
		params.cpuAffinityMask = g_FiosParams.threadAffinity[SCE_FIOS_IO_THREAD];
		params.priority = g_FiosParams.threadPriority[SCE_FIOS_IO_THREAD];
		if (m_pName != NULL && m_pName[0] != 0)
			fiosSnprintf(params.name, sizeof(params.name), "SceFiosIO %s", m_pName);
		else
			fiosStrlcpy(params.name, "SceFiosIO", sizeof(params.name));
	}
	params.attr = 0;
	params.name[sizeof(params.name) - 1] = 0;

	int err = SCE_OK;
	int i;

	m_shutdown = false;
	for (i = 0; i < m_threadCount; i++) {
		err = m_threads[i].Create(&params, ThreadEntry, (uintptr_t)this);
		if (err != SCE_OK)
			break;
	}
	if (err == SCE_OK) {
		m_pNext = s_pSchedulers;
		s_pSchedulers = this;
		return SCE_OK;
	}
	m_shutdown = true;
	for (int j = 0; j < i; j++) {
		Wws::Platform::Thread *pThread = &m_threads[j];

		if (pThread != NULL) {
			pThread->Join();
			pThread->Init();
		}
	}
	m_queueFullCond.Term();
	m_wakeCond.Term();
	m_mutex.Term();
	m_initialized = false;
	return err;
}

int fiosSchedulersInit()
{
	int err = g_fiosScheduler.Initialize();

	if (err == SCE_OK) {
		err = fiosPlatformSchedulersInit();
		if (err != SCE_OK)
			g_fiosScheduler.Terminate();
	}
	return err;
}

bool fiosSchedulersIsIdle()
{
	uint32_t queued = 0;

	for (Scheduler *pScheduler = s_pSchedulers; pScheduler != NULL;
	     pScheduler = pScheduler->m_pNext)
		queued += pScheduler->m_queueCount;
	return queued == 0;
}

void Scheduler::Shutdown()
{
	int ms = 1;

	while (!fiosSchedulersIsIdle()) {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s waiting %dms for idle",
			   "Shutdown", ms);
		Wws::Platform::SleepMilliseconds(ms);
		ms = ms * 2 < 1000 ? ms * 2 : 1000;
	}
	fiosPlatformSchedulersTerm();
	g_fiosScheduler.Terminate();
}

void fiosSchedulersGetStatistics(FiosStatistics *pStats)
{
	Scheduler *pScheduler = s_pSchedulers;
	uint32_t i;

	for (i = 0; i < g_FiosParams.ioThreadCount && pScheduler != NULL; i++) {
		pStats->scheduler[i].maxAvailable = QUEUE_SIZE;
		pStats->scheduler[i].bytesPerUnit = sizeof(uint16_t);
		pStats->scheduler[i].current = pScheduler->m_queueCount;
		pStats->scheduler[i].highWater = pScheduler->m_queueHighWater;
		pScheduler = pScheduler->m_pNext;
	}
	pStats->scheduler[2].bytesPerUnit = sizeof(uint16_t);
	pStats->scheduler[2].current = pScheduler->m_queueCount;
	pStats->scheduler[2].highWater = pScheduler->m_queueHighWater;
	pStats->scheduler[2].maxAvailable = QUEUE_SIZE;
}

void fiosSchedulersResetStatistics()
{
	Scheduler *pScheduler = s_pSchedulers;

	for (uint32_t i = 0; i < g_FiosParams.ioThreadCount && pScheduler != NULL; i++) {
		pScheduler->m_queueHighWater = pScheduler->m_queueCount;
		pScheduler = pScheduler->m_pNext;
	}
	pScheduler->m_queueHighWater = pScheduler->m_queueCount;
}

/* wake the threads, if there is something queued */
void Scheduler::Kick()
{
	if (m_shutdown)
		return;
	m_mutex.Lock();
	if (m_queueCount != 0) {
		m_busySkip = 0;
		m_kickCount++;
		m_bufferBusyCount = 0;
		m_wakeCond.Broadcast();
	}
	m_mutex.Unlock();
}

void fiosSchedulersWake()
{
	for (Scheduler *pScheduler = s_pSchedulers; pScheduler != NULL;
	     pScheduler = pScheduler->m_pNext)
		pScheduler->Kick();
}

__attribute__((noinline)) void Scheduler::KickAll(Scheduler *pExcept)
{
	for (Scheduler *pScheduler = s_pSchedulers; pScheduler != NULL;
	     pScheduler = pScheduler->m_pNext)
		if (pScheduler != pExcept && pScheduler != &g_fiosScheduler)
			pScheduler->Kick();
}

/* where a chunk leaves the medium */
void schedulerChunkLocation(FiosChunk *pChunk, SchedulerLocation *pLocation)
{
	pLocation->h = pChunk->h;
	pLocation->offset = pChunk->offset;
}

/* two locations on one medium: the same handle, and the offsets in one device's range */
bool schedulerSameMedium(SchedulerLocation *pA, SchedulerLocation *pB, bool checkOffset)
{
	if (pA->h != pB->h)
		return false;
	if (!checkOffset)
		return true;
	if ((pA->offset & 0x8000000000000000ULL) == 0)
		return true;
	return ((pA->offset ^ pB->offset) & 0xEFFFFFF000000000ULL) == 0;
}

bool Scheduler::OwnsPath(FiosPath *pPath, SceFiosOffset offset, SceFiosSize length)
{
	if (m_pName[0] == 0)
		return true;
	if (pPath == NULL)
		return false;
	return m_pOwnsPath(this, pPath, offset, length);
}

bool Scheduler::OwnsHandle(SceFiosHandle h, SceFiosOffset offset, SceFiosSize length)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(h);

	if (pFH != NULL)
		return OwnsPath(&pFH->path, offset, length);

	FiosDH *pDH = (FiosDH *)s_pDHTable->Lookup(h);

	if (pDH != NULL)
		return OwnsPath(&pDH->path, offset, length);
	return false;
}

/* the scheduler a chunk runs on, found once and kept in the chunk */
Scheduler *Scheduler::ForChunk(uint16_t chunkID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	Scheduler *pScheduler = &g_fiosScheduler;
	uint32_t index = pChunk->scheduler;

	if (index != 0) {
		pScheduler = s_pSchedulers;
		for (uint32_t i = 1; i < index && pScheduler != NULL; i++)
			pScheduler = pScheduler->m_pNext;
		if (pScheduler != NULL)
			return pScheduler;
	}
	if (chunkID == CHUNKID_INVALID) {
		pChunk->scheduler = g_FiosParams.ioThreadCount + 1;
		return pScheduler;
	}

	uint8_t type = pChunk->type;

	if (type == CHUNK_CALLBACK || type == CHUNK_ROOT || type == CHUNK_NULL)
		return pScheduler;

	SceFiosOffset offset = 0;
	SceFiosSize length = 0;

	if (pChunk->split) {
		offset = pChunk->offset;
		length = pChunk->reqCount;
	}

	SceFiosHandle h = fiosChunkGetHandleLocked(chunkID);
	int n = 0;

	if (h != 0) {
		for (pScheduler = s_pSchedulers; pScheduler != NULL; pScheduler = pScheduler->m_pNext) {
			n++;
			if (pScheduler->OwnsHandle(h, offset, length)) {
				pChunk->scheduler = n;
				break;
			}
		}
		if (pScheduler == NULL) {
			n = 1;
			for (pScheduler = s_pSchedulers; pScheduler != &g_fiosIOSchedulers[0];
			     pScheduler = pScheduler->m_pNext)
				n++;
			pChunk->scheduler = n;
		}
		return pScheduler;
	}

	FiosPath *pPath = fiosChunkGetPathLocked(chunkID);

	for (pScheduler = s_pSchedulers; pScheduler != NULL; pScheduler = pScheduler->m_pNext) {
		n++;
		if (pScheduler->OwnsPath(pPath, offset, length)) {
			pChunk->scheduler = n;
			break;
		}
	}
	return pScheduler;
}

void Scheduler::PrintQueue()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
		   "%-15s queueCount=%d, queuePosition=%d, QUEUE_SIZE=%d", "PrintQueue",
		   m_queueCount, m_queuePosition, QUEUE_SIZE);
	for (uint32_t i = 0; i < QUEUE_SIZE; i += 16) {
		bool empty = true;

		for (uint32_t j = 0; j < 16; j++)
			if (m_queue[i + j] != 0)
				empty = false;
		if (!empty)
			FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
				   "%-15s [%d-%d]  %d %d %d %d  %d %d %d %d  %d %d %d %d  %d %d %d %d",
				   "PrintQueue", i, i + 15, m_queue[i + 0], m_queue[i + 1],
				   m_queue[i + 2], m_queue[i + 3], m_queue[i + 4], m_queue[i + 5],
				   m_queue[i + 6], m_queue[i + 7], m_queue[i + 8], m_queue[i + 9],
				   m_queue[i + 10], m_queue[i + 11], m_queue[i + 12], m_queue[i + 13],
				   m_queue[i + 14], m_queue[i + 15]);
	}
}

/* move the queue entries [start, start + length) by shift places */
void Scheduler::Shift(uint32_t start, uint32_t length, int shift)
{
	if (shift == 0 || length == 0)
		return;

	uint32_t last = start + length - 1;
	uint32_t src = m_queuePosition + start;
	uint32_t srcLast = m_queuePosition + last;
	uint32_t dst = m_queuePosition + (start + shift);
	uint32_t dstLast = m_queuePosition + (last + shift);

	if (src >= QUEUE_SIZE)
		src -= QUEUE_SIZE;
	if (srcLast >= QUEUE_SIZE)
		srcLast -= QUEUE_SIZE;
	if (dst >= QUEUE_SIZE)
		dst -= QUEUE_SIZE;
	if (dstLast >= QUEUE_SIZE)
		dstLast -= QUEUE_SIZE;

	bool srcWraps = src > srcLast;
	bool dstWraps = dst > dstLast;

	if (length == 1) {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
			   "%-15s start=%d, len=%d, shift=%d (%s)", "Shift", start, length, shift,
			   "single");
		m_queue[dst] = m_queue[src];
	} else if (!srcWraps && !dstWraps) {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
			   "%-15s start=%d, len=%d, shift=%d (%s)", "Shift", start, length, shift,
			   "memmove");
		sceClibMemmove(&m_queue[dst], &m_queue[src], length * sizeof(uint16_t));
	} else if (shift > 0) {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
			   "%-15s start=%d, len=%d, shift=%d (%s)", "Shift", start, length, shift,
			   "item-by-item right shift");
		uint32_t s = srcLast + 1;
		uint32_t d = dstLast + 1;

		for (uint32_t i = 0; i < length; i++) {
			s--;
			d--;
			m_queue[d] = m_queue[s];
			if (s == 0)
				s = QUEUE_SIZE;
			if (d == 0)
				d = QUEUE_SIZE;
		}
	} else {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
			   "%-15s start=%d, len=%d, shift=%d (%s)", "Shift", start, length, shift,
			   "item-by-item left shift");
		uint32_t s = src;
		uint32_t d = dst;

		for (uint32_t i = 0; i < length; i++) {
			m_queue[d++] = m_queue[s++];
			if (s == QUEUE_SIZE)
				s = 0;
			if (d == QUEUE_SIZE)
				d = 0;
		}
	}
	if (g_FiosParams.profiling & SCE_FIOS_PROFILE_SCHEDULER)
		PrintQueue();
}

/* a chunk into the queue at a position */
void Scheduler::Insert(uint16_t chunkID, uint32_t position)
{
	uint32_t count = m_queueCount;

	if (position == 0 && count != 0) {
		m_queueCount = count + 1;
		m_queuePosition = m_queuePosition != 0 ? m_queuePosition - 1 : QUEUE_SIZE - 1;
	} else {
		m_queueCount = count + 1;
		if (count - position != 0)
			Shift(position, count - position, 1);
	}
	if (m_queueCount > m_queueHighWater)
		m_queueHighWater = m_queueCount;

	uint32_t index = m_queuePosition + position;

	if (index >= QUEUE_SIZE)
		index -= QUEUE_SIZE;
	m_queue[index] = chunkID;
	if (index < m_lastPosition)
		m_lastPosition++;
}

/*
 * A chunk's cost, added to *pTime from *pLocation: its type's average time, scaled by its
 * length for a read or write. It is late when it ends after its deadline, and then counts
 * with its priority (*pPriority, else below every priority) against priority.
 */
void Scheduler::EstimateCost(uint16_t chunkID, int *pPriority, int priority, int *pHigher,
			     int *pEqual, int *pLate, SceFiosTime *pTime,
			     SchedulerLocation *pLocation)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	SceFiosTime cost = m_typeTimes[pChunk->type];

	if (pChunk->split) {
		pLocation->h = pChunk->h;
		*pTime += cost * pChunk->reqCount / 65536;
		pLocation->offset = pChunk->offset + pChunk->reqCount;
	} else {
		*pTime += cost;
		pLocation->h = -1;
	}

	int late = SCE_FIOS_PRIO_MIN - 1;

	if (*pTime > pChunk->deadline) {
		(*pLate)++;
		late = pChunk->priority;
	}
	if (priority < late)
		(*pHigher)++;
	else if (priority == late)
		(*pEqual)++;
	*pPriority = late;
}

/*
 * Run a chunk; m_mutex is held, and is again on return. A chunk whose buffer chunk is busy,
 * or that comes back busy, goes back where it was taken from, unless it runs on the
 * caller's thread (fastPath). The time it took goes into its type's average, and a seek's
 * into the seek averages.
 */
int Scheduler::ExecuteChunk(uint16_t chunkID, bool fastPath)
{
	SchedulerLocation location = m_location;
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	bool no = false;
	int ret = SCE_OK;
	int err = SCE_OK;
	uint16_t bufferID = pChunk->bufferChunk;

	if (bufferID != CHUNKID_INVALID) {
		FiosChunk *pBuffer = fiosChunkAt(bufferID);

		err = pBuffer->callback.pCallback(pBuffer, 1);
		if (err == SCE_OK && m_lastPosition != 0 && m_bufferBusyCount != 0) {
			pBuffer->callback.pCallback(pBuffer, 2);
			if (!fastPath)
				Insert(chunkID, m_lastPosition);
			m_busySkip++;
			m_bufferBusyCount++;
			m_executeStart = 0;
			s_chunkMutex.Lock();
			pChunk->executing = no;
			s_chunkMutex.Unlock();
			return FIOS_CALLBACK_AGAIN;
		}
		if (err == FIOS_CALLBACK_AGAIN)
			m_bufferBusyCount++;
	}

	SceFiosTime start = sceKernelGetProcessTimeWide();
	int priority, higher, equal, late;

	m_executeStart = start;
	EstimateCost(chunkID, &priority, 0, &higher, &equal, &late, &m_executeStart, &m_location);

	uint32_t kickCount = m_kickCount;
	bool unlocked = true;

	m_mutex.Unlock();
	if (err == SCE_OK || (err == SCE_FIOS_ERROR_CANCELLED && pChunk->cancelled))
		err = Execute(chunkID, m_pExecutors);
	if (m_executing < 2)
		m_executeStart = 0;
	if (err == FIOS_CALLBACK_AGAIN) {
		if (!fastPath) {
			m_mutex.Lock();
			Insert(chunkID, m_lastPosition);
			unlocked = false;
			if (kickCount == m_kickCount)
				m_busySkip++;
		}
		s_chunkMutex.Lock();
		fiosChunkAt(chunkID)->executing = no;
		s_chunkMutex.Unlock();
		if (unlocked)
			m_mutex.Lock();
		return FIOS_CALLBACK_AGAIN;
	}

	bool done = fiosChunkComplete(chunkID, err);
	SceFiosTime elapsed = sceKernelGetProcessTimeWide() - start;
	uint8_t type = pChunk->type;

	if (pChunk->pieces != 0)
		elapsed /= (int)pChunk->pieces;
	if (type < 36) {
		SceFiosTime average = m_typeTimes[type];
		bool update = true;

		if (pChunk->split) {
			if (location.h == pChunk->h || location.offset != pChunk->offset) {
				SceFiosTime seek = elapsed - average;

				update = false;
				if (seek > 0) {
					uint32_t index = no;

					if (location.h == pChunk->h &&
					    ((pChunk->offset & 0x8000000000000000ULL) == 0 ||
					     ((pChunk->offset ^ location.offset) &
					      0xEFFFFFF000000000ULL) == 0)) {
						SceFiosOffset distance = pChunk->offset - location.offset;

						if (distance < 0)
							distance = -distance;
						distance /= 0x8000;
						if (distance > 0) {
							SceFiosOffset blocks = distance / 2;

							if (blocks == 0)
								blocks = 1;
							seek /= blocks;
							index = 1;
						} else {
							seek = 0;
						}
					}
					if ((sceFiosTimeIntervalToNanoseconds(seek) + 999) / 1000 > 10)
						m_seekTimes[index] = (19 * m_seekTimes[index] + seek) / 20;
				}
			} else if ((sceFiosTimeIntervalToNanoseconds(elapsed) + 999) / 1000 <= 100) {
				update = false;
			}
		}
		if (update) {
			SceFiosTime next = (19 * average + elapsed) / 20;

			if (next == average) {
				if (elapsed > next)
					next++;
				else if (elapsed < next)
					next--;
			}
			m_typeTimes[pChunk->type] = next;
		}
	}

	m_mutex.Lock();
	m_busySkip = no;
	m_bufferBusyCount = no;

	bool kick = no;

	if (m_queueCount != 0)
		kick = true;
	m_mutex.Unlock();
	if (fastPath) {
		if (kick)
			Kick();
	} else {
		uint16_t parentID;

		for (int i = 0; (parentID = fiosChunkGetParentLocked(chunkID, i)) != CHUNKID_INVALID;
		     i++) {
			Scheduler *pScheduler = ForChunk(parentID);

			if (pScheduler != this && pScheduler != NULL) {
				FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s kick",
					   "ExecuteChunk");
				pScheduler->Kick();
			}
			KickAll(this);
		}
	}
	if (done)
		fiosChunkDeleteLocked(chunkID);
	m_mutex.Lock();
	return ret;
}

void Scheduler::RemoveFromSchedule(uint16_t chunkID)
{
	SchedulerProfile profile;

	schedulerProfileBegin(&profile, chunkID, SCE_FIOS_PROFILE_EVENT_REMOVEFROMSCHEDULE);
	m_mutex.Lock();

	uint32_t i = m_searchPosition;
	uint32_t count = m_queueCount;
	uint32_t n;

	if (i >= count) {
		i = 0;
		m_searchPosition = 0;
	}
	for (n = 0; n < count;) {
		if (QueueEntry(i) == chunkID)
			break;
		if (++i >= count)
			i = 0;
		n++;
	}
	if (n >= count) {
		sceFiosPrintf("Scheduler::RemoveFromSchedule could not find chunk! chunk = %d\n",
			      chunkID);
		sceFiosPrintf("%s\n", fiosDebugDumpChunk(chunkID, NULL, 0));
		m_mutex.Unlock();
		schedulerProfileEnd(&profile);
		return;
	}

	FiosChunk *pChunk = fiosChunkAt(chunkID);

	if (!pChunk->cancelled || fiosChunkIsReadyLocked(chunkID, true)) {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s removing chunk %d: %s",
			   "RemoveFromSchedule", chunkID, fiosDebugDumpChunk(chunkID, NULL, 0));
		if (m_busySkip != 0 && i <= m_busySkip)
			m_busySkip--;
		Shift(0, i, 1);
		QueueEntry(0) = 0;
		m_queuePosition++;
		if (m_queuePosition >= QUEUE_SIZE)
			m_queuePosition -= QUEUE_SIZE;
		if (m_queueCount-- == QUEUE_SIZE - 1)
			m_queueFullCond.Signal();
		m_searchPosition = i;
		if (pChunk->cancelled) {
			int err = ExecuteChunk(chunkID, false);

			if (err != SCE_OK && err != SCE_FIOS_ERROR_CANCELLED)
				sceFiosPrintf("FIOS2 WARNING: Unexpected return from Execute during cancel: %ld\n",
					      err);
		}
	} else {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
			   "%-15s chunk %d not removed, cancelled but not eligible: %s",
			   "RemoveFromSchedule", chunkID, fiosDebugDumpChunk(chunkID, NULL, 0));
		fiosChunkAt(chunkID)->scheduled = 1;
		m_busySkip = 0;
	}
	m_mutex.Unlock();
	schedulerProfileEnd(&profile);
}

void fiosChunkRemoveFromSchedule(uint16_t chunkID)
{
	Scheduler::ForChunk(chunkID)->RemoveFromSchedule(chunkID);
}

/*
 * Take a chunk and the chunks under it off their schedulers, cancelling them when cancel is
 * set. A chunk that a parent not cancelled still needs stays; the result says a chunk was
 * left, to try again.
 */
static int unscheduleTree(uint16_t chunkID, bool cancel)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	uint16_t removeID = CHUNKID_INVALID;

	FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s called for chunk %d",
		   "UnscheduleTree", chunkID);
	s_chunkMutex.Lock();
	if (cancel) {
		uint16_t parentID;

		for (int i = 0; (parentID = fiosChunkGetParent(chunkID, i)) != CHUNKID_INVALID; i++) {
			if (parentID != s_nullChunk && !fiosChunkAt(parentID)->cancelled) {
				s_chunkMutex.Unlock();
				return 1;
			}
		}
	}

	uint32_t count = 0;
	uint32_t n;
	uint16_t childID;

	for (n = 0; (childID = fiosChunkGetChild(chunkID, n)) != CHUNKID_INVALID; n++)
		if (!fiosChunkAt(childID)->completed)
			count++;

	uint16_t *pChildren = NULL;

	if (count != 0) {
		uint16_t *pOut = pChildren = (uint16_t *)alloca(count * sizeof(uint16_t));

		for (uint32_t i = 0; i < n; i++) {
			childID = fiosChunkGetChild(chunkID, i);
			if (!fiosChunkAt(childID)->completed)
				*pOut++ = childID;
		}
	}
	if (pChunk->completed) {
		if (fiosChunkChildrenComplete(chunkID)) {
			s_chunkMutex.Unlock();
			return 0;
		}
	} else if (!pChunk->executing) {
		if (cancel) {
			pChunk->cancelled = 1;
			if (pChunk->bufferChunk != CHUNKID_INVALID)
				fiosChunkAt(pChunk->bufferChunk)->cancelled = 1;
		}
		if (pChunk->scheduled && Scheduler::ForChunk(chunkID) != &g_fiosScheduler) {
			pChunk->scheduled = 0;
			removeID = chunkID;
		}
	}
	s_chunkMutex.Unlock();

	int again = 0;

	for (uint32_t i = 0; i < count; i++)
		if (pChildren[i] != CHUNKID_INVALID)
			again |= unscheduleTree(pChildren[i], cancel);
	if (removeID != CHUNKID_INVALID)
		Scheduler::ForChunk(removeID)->RemoveFromSchedule(removeID);
	return again;
}

void fiosChunkCancel(uint16_t chunkID, bool cancel)
{
	SchedulerProfile profile;

	schedulerProfileBegin(&profile, chunkID, SCE_FIOS_PROFILE_EVENT_UNSCHEDULETREE);
	if (unscheduleTree(chunkID, cancel))
		unscheduleTree(chunkID, cancel);
	schedulerProfileEnd(&profile);
}

/*
 * Where a chunk goes in the queue: behind a read or write it continues, else at the
 * position whose estimate makes the fewest chunks late, then ends soonest.
 */
uint32_t Scheduler::FindPosition(uint16_t chunkID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	SceFiosTime deadline = chunkDeadline(pChunk);
	uint32_t count = m_queueCount;

	if (count == 0)
		return 0;

	uint8_t type = pChunk->type;

	if (type == CHUNK_FH_READ || type == CHUNK_FH_WRITE) {
		FiosChunk *pLast = fiosChunkAt(QueueEntry(count - 1));
		uint8_t lastType = pLast->type;

		if ((lastType == CHUNK_FH_READ || lastType == CHUNK_FH_WRITE) &&
		    (type == CHUNK_FH_READ || type == CHUNK_FH_WRITE) && pLast->h == pChunk->h &&
		    pLast->priority == pChunk->priority && pLast->deadline <= deadline) {
			SceFiosOffset gap = pChunk->offset - (pLast->offset + pLast->reqCount);

			if (gap < 0)
				gap = -gap;
			if (gap < 0x8000)
				return count;
		}
	}

	SceFiosTime now = m_executeStart;
	int late = 0;
	SchedulerLocation location = m_location;

	if (now == 0) {
		SceFiosTimeInterval soon = sceFiosTimeIntervalFromNanoseconds(10000);

		now = sceKernelGetProcessTimeWide() + soon;
		count = m_queueCount;
	}

	int newPriority = 128;
	int queuedPriority = 128;
	int newLate = 0;
	int higher = 0;
	int equal = 0;
	SceFiosTime time = now;
	SchedulerLocation newLocation = location;
	SceFiosTime best = 0x7FFFFFFFFFFFFFFFLL;
	uint32_t bestLate = 0xFFFFFFFF;
	uint32_t bestPosition = count;
	bool split = pChunk->split;

	for (uint32_t i = 0; i <= count; i++) {
		int previous = queuedPriority;
		SceFiosTime before;
		SceFiosTime after;
		int priority;

		newLate = late;
		time = now;
		newLocation = location;
		before = now;
		if (i < count) {
			EstimateCost(QueueEntry(i), &queuedPriority, 0, &higher, &equal, &late, &now,
				     &location);
			before = time;
		}
		EstimateCost(chunkID, &newPriority, 0, &higher, &equal, &newLate, &time,
			     &newLocation);
		higher = 0;
		equal = 0;
		after = time;
		count = m_queueCount;
		for (uint32_t j = i; j < count; j++) {
			EstimateCost(QueueEntry(j), &priority, newPriority, &higher, &equal, &newLate,
				     &time, &newLocation);
			count = m_queueCount;
			if (higher != 0)
				break;
		}
		if (higher != 0)
			continue;
		if (equal == 0 && newPriority > previous && i != 0)
			continue;
		if ((uint32_t)newLate < bestLate ||
		    ((uint32_t)newLate == bestLate && time < best) ||
		    ((uint32_t)newLate == bestLate && time == best && (split || after > before))) {
			bestPosition = i;
			bestLate = newLate;
			best = time;
		}
	}
	return bestPosition;
}

/* the end of the queue */
uint32_t Scheduler::FindPositionFIFO(uint16_t chunkID)
{
	chunkDeadline(fiosChunkAt(chunkID));
	return m_queueCount;
}

/*
 * A chunk to the queue, or run at once on the caller's thread (fastPath) when it may: an
 * open, stat or close asked for SCE_FIOS_OPFLAG_IMMED, or nothing is queued and a thread is
 * free. The result says it ran.
 */
bool Scheduler::AddToSchedule(uint16_t chunkID, int opflags, bool fastPath)
{
	SchedulerProfile profile;

	schedulerProfileBegin(&profile, chunkID, SCE_FIOS_PROFILE_EVENT_ADDTOSCHEDULE);

	FiosChunk *pChunk = fiosChunkAt(chunkID);
	bool no = false;

	m_mutex.Lock();

	uint32_t count = m_queueCount;

	if (count >= QUEUE_SIZE - 1) {
		do {
			sceFiosPrintf("FIOS2 scheduler queue is full, waiting...\n");
			m_queueFullCond.Wait();
			if (m_queueCount < QUEUE_SIZE - 1)
				sceFiosPrintf("FIOS2 scheduler queue is available again\n");
		} while (m_queueCount >= QUEUE_SIZE - 1);
		count = m_queueCount;
	}

	int err = FIOS_CALLBACK_AGAIN;

	if (fastPath) {
		uint8_t type = pChunk->type;

		if (((type == CHUNK_FH_OPEN || type == CHUNK_STAT || type == CHUNK_FH_CLOSE) &&
		     (opflags & SCE_FIOS_OPFLAG_IMMED)) ||
		    (count == 0 && m_executing < m_threadCount)) {
			if (s_suspendCount <= 0 && fiosChunkIsReadyLocked(chunkID, false)) {
				if (chunkUsesOverlays(pChunk->type))
					sceFiosOverlayThreadSetDisabled02(1);
				m_executing++;
				FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s fast-path chunk %d",
					   "AddToSchedule", chunkID);
				err = ExecuteChunk(chunkID, fastPath);
				if (chunkUsesOverlays(pChunk->type))
					sceFiosOverlayThreadSetDisabled02(0);
				m_executing--;
			}
		}
	}
	if (err != SCE_OK) {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s inserting chunk %d",
			   "AddToSchedule", chunkID);

		uint32_t position;

		if (this == &g_fiosScheduler)
			position = FindPositionFIFO(chunkID);
		else
			position = FindPosition(chunkID);
		Insert(chunkID, position);
		if (m_busySkip > position)
			m_busySkip = position;
	}
	if (m_queueCount != 0 && m_executing <= 0) {
		bool wake = true;

		if (this == &g_fiosScheduler || m_queueCount != 1) {
			s_chunkMutex.Lock();
			wake = fiosChunkIsReady(chunkID, false);
			if (wake) {
				FiosChunk *pReady = fiosChunkAt(chunkID);

				if (pReady->type == CHUNK_CALLBACK &&
				    pReady->callback.pCallback(pReady, 0) == FIOS_CALLBACK_AGAIN) {
					wake = no;
				} else if (pReady->bufferChunk != CHUNKID_INVALID &&
					   (m_bufferBusyCount != 0 ||
					    fiosChunkAt(pReady->bufferChunk)->callback.pCallback(
						    fiosChunkAt(pReady->bufferChunk), 0) ==
						    FIOS_CALLBACK_AGAIN)) {
					wake = no;
				}
			}
			s_chunkMutex.Unlock();
		}
		if (wake)
			m_wakeCond.Broadcast();
	}

	bool ran = no;

	if (err == SCE_OK)
		ran = true;
	m_mutex.Unlock();
	schedulerProfileEnd(&profile);
	return ran;
}

bool fiosChunkAddToSchedule(uint16_t chunkID, int opflags, bool fastPath)
{
	fiosChunkAt(chunkID)->scheduler = 0;
	return Scheduler::ForChunk(chunkID)->AddToSchedule(chunkID, opflags, fastPath);
}

/*
 * Put a chunk and the chunks under it on their schedulers, children first. They run on the
 * caller's thread (fastPath) while each one can; the result says they all did.
 */
bool fiosScheduleTreeRecurse(uint16_t chunkID, int opflags, bool fastPath)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	uint32_t count = 0;
	uint16_t *pChildren = NULL;
	uint16_t addID = CHUNKID_INVALID;

	FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s called for chunk %d",
		   "ScheduleTreeRecurse", chunkID);
	s_chunkMutex.Lock();

	uint32_t n;
	uint16_t childID;

	for (n = 0; (childID = fiosChunkGetChild(chunkID, n)) != CHUNKID_INVALID; n++)
		if (!fiosChunkAt(childID)->completed)
			count++;
	if (count != 0) {
		uint16_t *pOut = pChildren = (uint16_t *)alloca(count * sizeof(uint16_t));

		for (uint32_t i = 0; i < n; i++) {
			childID = fiosChunkGetChild(chunkID, i);
			if (!fiosChunkAt(childID)->completed)
				*pOut++ = childID;
		}
	}
	if (fastPath && count > 1) {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
			   "%-15s parallel chunks caused switch to queueing", "ScheduleTreeRecurse");
		fastPath = false;
	}
	if (!pChunk->scheduled) {
		addID = chunkID;
		pChunk->scheduled = 1;
	}
	s_chunkMutex.Unlock();
	for (uint32_t i = 0; i < count; i++) {
		if (pChildren[i] != CHUNKID_INVALID &&
		    !fiosScheduleTreeRecurse(pChildren[i], opflags, fastPath) && fastPath)
			fastPath = false;
	}
	if (addID != CHUNKID_INVALID) {
		fiosChunkAt(addID)->scheduler = 0;
		if (!Scheduler::ForChunk(addID)->AddToSchedule(addID, opflags, fastPath) && fastPath) {
			FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
				   "%-15s chunk %d caused switch to queueing", "ScheduleTreeRecurse",
				   addID);
			fastPath = false;
		}
	}
	return fastPath;
}

bool fiosScheduleTree(uint16_t chunkID, int opflags, bool fastPath)
{
	SchedulerProfile profile;

	schedulerProfileBegin(&profile, chunkID, SCE_FIOS_PROFILE_EVENT_SCHEDULETREE);

	bool ran = fiosScheduleTreeRecurse(chunkID, opflags, fastPath);

	schedulerProfileEnd(&profile);
	return ran;
}

/* the first chunk of the queue that can run, taken off it, or CHUNKID_INVALID */
uint16_t Scheduler::NextEligible()
{
	static bool s_busyWarned;

	if (g_FiosParams.profiling & SCE_FIOS_PROFILE_SCHEDULER)
		PrintQueue();
	while (m_busySkip != 0 && m_busySkip == m_queueCount) {
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched",
			   "%-15s sleeping up to 50ms after getting BUSY from all %d chunks",
			   "NextEligible", m_queueCount);

		SceRtcTick tick;

		sceRtcGetCurrentTick(&tick);
		if (!m_wakeCond.WaitUntil(tick.tick + Wws::Platform::NanosecondsToTime(50000000))) {
			if (!s_busyWarned) {
				sceFiosPrintf("FIOS2 WARNING: An unexpected busy state was hit while scheduling!\n");
				s_busyWarned = true;
			}
			m_busySkip = 0;
			m_bufferBusyCount = 0;
		}
	}
	if (m_busySkip != 0 && m_busySkip >= m_queueCount)
		sceFiosPrintf("Warning m_busySkip %lu, m_queueCount %lu\n", m_busySkip, m_queueCount);
	s_chunkMutex.Lock();

	uint16_t chunkID = CHUNKID_INVALID;

	for (uint32_t i = m_busySkip; i < m_queueCount; i++) {
		chunkID = QueueEntry(i);
		if (!fiosChunkIsReady(chunkID, false)) {
			m_busySkip++;
		} else {
			FiosChunk *pChunk = fiosChunkAt(chunkID);

			if (pChunk->type == CHUNK_CALLBACK &&
			    pChunk->callback.pCallback(pChunk, 0) == FIOS_CALLBACK_AGAIN) {
				m_busySkip++;
			} else if (pChunk->bufferChunk == CHUNKID_INVALID ||
				   (m_bufferBusyCount == 0 &&
				    fiosChunkAt(pChunk->bufferChunk)->callback.pCallback(
					    fiosChunkAt(pChunk->bufferChunk), 0) != FIOS_CALLBACK_AGAIN)) {
				if (i != 0)
					Shift(0, i, 1);
				QueueEntry(0) = 0;
				m_queuePosition++;
				if (m_queuePosition >= QUEUE_SIZE)
					m_queuePosition -= QUEUE_SIZE;
				if (m_queueCount-- == QUEUE_SIZE - 1)
					m_queueFullCond.Signal();
				m_lastPosition = i;
				goto found;
			} else {
				m_bufferBusyCount++;
				m_busySkip++;
			}
		}
	}
	chunkID = CHUNKID_INVALID;
found:
	FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s %s", "NextEligible",
		   fiosDebugDumpChunk(chunkID, NULL, 0));
	if (chunkID != CHUNKID_INVALID) {
		fiosChunkAt(chunkID)->executing = 1;
		if (g_FiosParams.profiling & SCE_FIOS_PROFILE_SCHEDULER)
			PrintQueue();
	}
	s_chunkMutex.Unlock();
	return chunkID;
}

/* a scheduler thread: the queue's chunks, one after another, until it is shut down */
void Scheduler::Thread()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s starting", "Thread");
	sceFiosOverlayThreadSetDisabled02(1);
	m_mutex.Lock();
	for (;;) {
		if (m_shutdown && m_queueCount == 0)
			break;

		uint16_t chunkID;

		if (m_executing < m_threadCount && s_suspendCount <= 0 &&
		    (chunkID = NextEligible()) != CHUNKID_INVALID) {
			m_executing++;
			ExecuteChunk(chunkID, false);
			m_executing--;
			continue;
		}
		if (m_shutdown && m_queueCount == 0)
			break;
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s waiting", "Thread");
		m_wakeCond.Wait();
		FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s woke up", "Thread");
	}
	FIOS_TRACE(SCE_FIOS_PROFILE_SCHEDULER, "sched", "%-15s shutting down", "Thread");
	m_mutex.Unlock();
}

static void ThreadEntry(uintptr_t arg)
{
	((Scheduler *)arg)->Thread();
}

/* the front of the queue */
uint32_t Scheduler::FindPositionLIFO(uint16_t chunkID)
{
	chunkDeadline(fiosChunkAt(chunkID));
	return 0;
}

/* behind the chunks of its priority and higher */
uint32_t Scheduler::FindPositionPriority(uint16_t chunkID)
{
	FiosChunk *pChunk = fiosChunkAt(chunkID);
	int8_t priority = pChunk->priority;
	uint32_t position = 0;

	chunkDeadline(pChunk);
	for (; position < m_queueCount; position++)
		if (priority > fiosChunkAt(QueueEntry(position))->priority)
			break;
	return position;
}

/* a scheduler owns the paths its name prefixes */
bool fiosSchedulerOwnsPrefix(Scheduler *pScheduler, FiosPath *pPath, SceFiosOffset offset,
			     SceFiosSize length)
{
	return sceFiosPathncmp(fiosPathGet(pPath), pScheduler->m_pName,
			       pScheduler->m_nameLength) == 0;
}
