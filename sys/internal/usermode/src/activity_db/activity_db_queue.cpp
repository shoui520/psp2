/*
 * The activity daemon's queue: one thread runs its jobs in order on the databases, and the
 * jobs the daemon pushes on it hand their work to the daemon's Run methods.
 */
/* Sony's build of this file inlines vector's fill constructor */
#define PAF_VECTOR_FILL_INLINE
#include "activity_db_internal.h"

/* Sony calls the queue's push_back */
namespace paf {
template<> inline __attribute__((noinline))
void list<ActivityDbQueue::Entry *, allocator<ActivityDbQueue::Entry *> >::push_back(
	ActivityDbQueue::Entry *const &val)
{
	if (head == NULL)
		return;

	_Node *node = (_Node *)operator new(sizeof(_Node));

	if (node == NULL)
		ScePafStdc_D608246A();
	if (node != NULL)
		new (&node->value) ActivityDbQueue::Entry *(val);
	else
		node = head;
	if (node != head) {
		node->prev = head->prev;
		node->next = head;
		head->prev->next = node;
		head->prev = node;
		++count;
	}
}
}

__attribute__((noinline)) ActivityDbQueue::ActivityDbQueue(int priority)
	: paf::thread::Thread(priority, 0x4000, "SceLsdbActQueue", NULL),
	  m_lock("SceLsdbActQueue", 0), m_emptyCond(m_lock, "SceLsdbActQueueEmpty", 0),
	  m_notEmptyCond(m_lock, "SceLsdbActQueueNotEmpty", 0)
{
}

ActivityDbQueue::~ActivityDbQueue()
{
}

__attribute__((noinline)) int ActivityDbQueue::Push(ActivityDbAccess *access, ActivityDbJob *job,
	bool notify)
{
	m_lock.Lock();

	Entry *entry = new Entry;

	if (entry != NULL) {
		entry->access = access;
		entry->job = job;
		entry->notify = notify;
	}
	m_entries.push_back(entry);
	m_emptyCond.Notify();
	m_lock.Unlock();
	return 0;
}

__attribute__((noinline)) void ActivityDbQueue::Stop()
{
	paf::thread::Thread::Cancel();
	m_lock.Lock();
	if (m_entries.size() != 0)
		m_notEmptyCond.Wait(0);
	else
		m_emptyCond.Notify();
	m_lock.Unlock();
	Join();
}

void ActivityDbQueue::EntryFunction()
{
	for (;;) {
		for (;;) {
			m_lock.Lock();
			if (m_entries.size() == 0)
				break;

			Entry *entry = m_entries.front();

			m_entries.pop_front();
			if (m_entries.size() == 0)
				m_notEmptyCond.Notify();
			m_lock.Unlock();

			int result = ACTIVITY_DB_ERROR_NOT_READY;

			if (entry->job != NULL) {
				result = entry->job->Run(entry->access);
				if (result == 0 && entry->notify)
					SceActivityDb_FBB82B37()->NotifyListeners();
			}
			if (entry->job != NULL)
				entry->job->Done(result);
			if (entry != NULL) {
				if (entry->job != NULL)
					delete entry->job;
				delete entry;
			}
		}
		if (IsCanceled())
			break;
		m_emptyCond.Wait(0);
		m_lock.Unlock();
	}
	m_lock.Unlock();
}

int ActivityDbSyncActivityJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunSyncActivity(access, &m_deleted, m_added, &m_updated,
		&m_titleId, &m_syncedTime);
}

void ActivityDbSyncActivityJob::Done(int result)
{
	if (m_wait != NULL) {
		*m_wait->syncedTime = m_syncedTime;
		m_wait->Done(result);
	}
}

int ActivityDbSyncStatusDetailJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunSyncStatusDetail(access, &m_status, &m_comments,
		&m_deletedComments, &m_goodJobs, &m_cancelledGoodJobs, m_isVoted);
}

void ActivityDbSyncStatusDetailJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbStoreStatusesJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunStoreStatuses(access, &m_statuses);
}

int ActivityDbDeleteStatusJob::Run(ActivityDbAccess *access)
{
	ActivityDbDaemon *daemon = SceActivityDb_FBB82B37();
	paf::vector<SceInt64> rowids(1, m_rowid);

	return daemon->RunDeleteStatuses(access, &rowids);
}

int ActivityDbUpdateStatusJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunUpdateStatuses(access, &m_statuses);
}

void ActivityDbUpdateStatusJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbPostCommentJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunPostComments(access, &m_mainRowid, &m_comments);
}

void ActivityDbPostCommentJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbDeleteCommentJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunDeleteComments(access, &m_keys);
}

void ActivityDbDeleteCommentJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbVoteGoodJobJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunVoteGoodJob(access, &m_key);
}

void ActivityDbVoteGoodJobJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbCancelGoodJobJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunCancelGoodJob(access, &m_key);
}

void ActivityDbCancelGoodJobJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbAddWaitStatusJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunAddWaitStatus(access, &m_status);
}

int ActivityDbClearWaitStatusJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunClearWaitStatus(access);
}

void ActivityDbClearWaitStatusJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbClearWaitStatusesJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunClearWaitStatuses(access, &m_rowids);
}

void ActivityDbClearWaitStatusesJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbDeleteTitlesJob::Run(ActivityDbAccess *access)
{
	return SceActivityDb_FBB82B37()->RunDeleteTitles(access, &m_titleIds);
}

void ActivityDbDeleteTitlesJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}
