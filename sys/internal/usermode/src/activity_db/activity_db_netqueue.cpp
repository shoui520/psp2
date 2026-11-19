/*
 * The network daemon's queue: one thread runs its requests in order, and the requests
 * (jobs) the daemon pushes on it.
 */
#include "activity_db_internal.h"
#include <framework.h>

/* Sony calls the queue's push_back */
namespace paf {
template<> inline __attribute__((noinline))
void list<ActivityDbNetQueue::Entry *, allocator<ActivityDbNetQueue::Entry *> >::push_back(
	ActivityDbNetQueue::Entry *const &val)
{
	if (head == NULL)
		return;

	_Node *node = (_Node *)operator new(sizeof(_Node));

	if (node == NULL)
		ScePafStdc_D608246A();
	if (node != NULL)
		new (&node->value) ActivityDbNetQueue::Entry *(val);
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

__attribute__((noinline)) ActivityDbNetQueue::ActivityDbNetQueue(int priority)
	: paf::thread::Thread(priority, 0x4000, "SceLsdbActNetworkQueue", NULL),
	  m_current(ACTIVITY_DB_NET_ID_NONE), m_lock("SceLsdbActNetQueue", 0),
	  m_emptyCond(m_lock, "SceLsdbActNetQueueEmpty", 0),
	  m_notEmptyCond(m_lock, "SceLsdbActNetQueueNotEmpty", 0)
{
}

__attribute__((noinline)) int ActivityDbNetQueue::Push(ActivityDbNetJob *job, SceInt64 id)
{
	if (job != NULL) {
		m_lock.Lock();
		m_entries.push_back(new Entry(job, id));
		m_emptyCond.Notify();
		m_lock.Unlock();
	}
	return 0;
}

__attribute__((noinline)) void ActivityDbNetQueue::Stop()
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

/* cancels the requests of an ID: the running one is aborted, the waiting ones dropped */
__attribute__((noinline)) int ActivityDbNetQueue::Cancel(SceInt64 id)
{
	bool found;

	if (id == ACTIVITY_DB_NET_ID_NONE)
		return ACTIVITY_DB_ERROR_NOT_READY;
	m_lock.Lock();
	found = false;
	for (paf::list<Entry *>::iterator it = m_entries.begin(); it != m_entries.end();) {
		Entry *entry = *it;

		if (entry->id != id) {
			++it;
		} else if (m_current == id) {
			found = true;
			entry->cancelled = true;
			SceActivityDb_7B68F69D()->Abort();
			++it;
		} else {
			delete entry;
			it = m_entries.erase(it);
			found = true;
		}
	}

	int result = ACTIVITY_DB_ERROR_NOT_READY;

	if (found)
		result = 0;
	m_lock.Unlock();
	return result;
}

void ActivityDbNetQueue::EntryFunction()
{
	for (;;) {
		m_lock.Lock();
		while (m_entries.size() != 0) {
			Entry *entry = m_entries.front();
			bool cancelled;
			int result;

			m_current = entry->id;
			cancelled = entry->cancelled == true;
			m_lock.Unlock();
			if (!cancelled)
				result = entry->job->Run();
			m_lock.Lock();
			if (m_entries.size() != 0)
				m_entries.pop_front();
			m_current = ACTIVITY_DB_NET_ID_NONE;
			if (!cancelled)
				cancelled = entry->cancelled == true;
			m_lock.Unlock();
			entry->job->Done(cancelled ? ACTIVITY_DB_ERROR_CANCELLED : result);
			delete entry;
			m_lock.Lock();
		}
		m_notEmptyCond.Notify();
		if (IsCanceled())
			break;
		m_emptyCond.Wait(0);
		m_lock.Unlock();
	}
	m_lock.Unlock();
}

/*
 * The news a sync brought: the title of the newest of the user's own statuses with new
 * comments or good jobs (else the newest other user's status with new comments), that
 * title's newest such status, and the comments and good jobs added to the user's statuses.
 */
__attribute__((noinline))
static void ActivityDbSummarizeSync(paf::vector<ActivityDbNetStatus> *statuses,
	ActivityDbSyncSummary *summary)
{
	if (statuses->size() == 0)
		return;

	SceUInt64 selfTime = 0;
	SceUInt64 userTime = 0;

	for (paf::vector<ActivityDbNetStatus>::iterator it = statuses->begin();
	     it != statuses->end(); ++it) {
		if (it->status.ownerType == ACTIVITY_DB_OWNER_SELF &&
		    (it->status.numOfNewComments != 0 || it->addedVotes != 0)) {
			if (selfTime < (SceUInt64)it->status.updateTime) {
				selfTime = it->status.updateTime;
				summary->titleId = it->status.titleId;
			}
		} else if (it->status.ownerType == ACTIVITY_DB_OWNER_USER &&
			   it->status.numOfNewComments != 0 && selfTime == 0 &&
			   userTime < (SceUInt64)it->status.updateTime) {
			userTime = it->status.updateTime;
			summary->titleId = it->status.titleId;
		}
	}

	SceUInt64 newest = 0;

	for (paf::vector<ActivityDbNetStatus>::iterator it = statuses->begin();
	     it != statuses->end(); ++it) {
		if (!ActivityDbStrEqual(it->status.titleId.c_str(), summary->titleId.c_str()))
			continue;
		if (it->status.ownerType == ACTIVITY_DB_OWNER_SELF &&
		    (it->status.numOfNewComments != 0 || it->addedVotes != 0)) {
			if (summary->ownerType == ACTIVITY_DB_OWNER_USER) {
				summary->hasNewComments = false;
				summary->hasNewVotes = false;
				summary->rowid = 0;
				newest = 0;
			}
			summary->ownerType = it->status.ownerType;

			SceUInt64 time = it->status.updateTime;

			if (it->status.numOfNewComments != 0)
				summary->hasNewComments = true;
			if (it->addedVotes != 0)
				summary->hasNewVotes = true;
			if (newest < time) {
				summary->rowid = it->status.rowid;
				newest = time;
			}
		} else if (it->status.ownerType == ACTIVITY_DB_OWNER_USER &&
			   summary->ownerType != ACTIVITY_DB_OWNER_SELF &&
			   newest < (SceUInt64)it->status.updateTime &&
			   it->status.numOfNewComments != 0) {
			newest = it->status.updateTime;
			summary->ownerId = it->status.ownerId;
			summary->ownerType = it->status.ownerType;
			summary->hasNewComments = true;
			summary->rowid = it->status.rowid;
		}
	}
	for (paf::vector<ActivityDbNetStatus>::iterator it = statuses->begin();
	     it != statuses->end(); ++it) {
		if (it->status.ownerType == ACTIVITY_DB_OWNER_SELF) {
			summary->addedComments += it->addedComments;
			summary->addedVotes += it->addedVotes;
		}
	}
}

/*
 * Reads the statuses the network has for the title since the last sync (the database's base
 * time when it never synced) and stores them; a local title's are never read.
 */
int ActivityDbNetSyncActivityJob::Run()
{
	SceInt64 baseTime = 0;
	int database = ActivityDbGetQueryDatabase(m_param.condition.type);
	SceInt64 waitTime = 0;
	int result;

	if (m_param.reload == true) {
		m_param.since = 0;
	} else {
		ActivityDbQuery query;
		ActivityDbQueryCondition condition = m_param.condition;

		result = query.Setup(&condition, &database);
		if (result != 0)
			goto fail;
		if (m_param.since == 0) {
			result = query.GetLastSyncedTime(&m_titleId, (SceInt64 *)&m_param.since);
			if (result != 0)
				goto fail;
		}
		result = query.GetBaseTime(&m_titleId, &baseTime);
		if (result != 0)
			goto fail;
	}
	m_syncedTime = baseTime;
	if (ActivityDbIsLocalTitle(&m_titleId)) {
		m_changed = false;
		return 0;
	}
	result = SceActivityDb_7B68F69D()->FetchActivity(&m_param, &m_titleId, &m_added,
		&m_updated, &m_deleted, (SceUInt64 *)&m_netTime);
	if (result == 0) {
		result = SceActivityDb_FBB82B37()->SyncActivity(m_param.condition.type, &m_titleId,
			&m_netTime, &m_added, &m_updated, &m_deleted, &waitTime, database);
		if (result == 0) {
			ActivityDbSummarizeSync(&m_added, &m_summary);
			ActivityDbSummarizeSync(&m_updated, &m_summary);
			m_syncedTime = waitTime;
			m_changed = m_deleted.size() != 0 || m_added.size() != 0 ||
				    m_updated.size() != 0;
			return result;
		}
	}
fail:
	m_changed = false;
	return result;
}

void ActivityDbNetSyncActivityJob::Done(int result)
{
	if (result != ACTIVITY_DB_ERROR_CANCELLED && m_param.callback != NULL)
		m_param.callback(result, m_changed, &m_titleId, &m_syncedTime, &m_summary,
			m_param.arg);
}

/* a status the network no longer has is deleted from the database */
int ActivityDbNetSyncDetailJob::Run()
{
	if (ActivityDbIsLocalTitle(&m_titleId)) {
		m_changed = false;
		return 0;
	}

	int result = SceActivityDb_7B68F69D()->RunSyncDetail(m_mainRowid, &m_callback,
		&m_changed);

	if (result == NP_ACTIVITY_ERROR_STATUS_NOT_FOUND)
		SceActivityDb_FBB82B37()->DeleteStatus(&m_mainRowid);
	return result;
}

void ActivityDbNetSyncDetailJob::Done(int result)
{
	if (result != ACTIVITY_DB_ERROR_CANCELLED && m_callback.func != NULL)
		m_callback.func(result, m_changed, m_callback.arg);
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbNetGetAvatarInfoJob::Run()
{
	paf::string titleId = m_condition.titleId;

	if (ActivityDbIsLocalTitle(&titleId)) {
		m_found = false;
		return 0;
	}
	return SceActivityDb_7B68F69D()->RunGetAvatarInfo(&m_condition, &m_found, &m_callback);
}

void ActivityDbNetGetAvatarInfoJob::Done(int result)
{
	if (result != ACTIVITY_DB_ERROR_CANCELLED && m_callback.func != NULL)
		m_callback.func(result, m_found, m_callback.arg);
}

int ActivityDbNetPostStatusJob::Run()
{
	return SceActivityDb_7B68F69D()->RunPostStatus(&m_status);
}

int ActivityDbNetSendWaitStatusesJob::Run()
{
	return SceActivityDb_7B68F69D()->RunSendWaitStatuses();
}

int ActivityDbNetDeleteStatusJob::Run()
{
	return SceActivityDb_7B68F69D()->RunDeleteStatus(&m_statusId);
}

void ActivityDbNetDeleteStatusJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbNetPostCommentJob::Run()
{
	return SceActivityDb_7B68F69D()->RunPostComment(&m_mainRowid, &m_comment);
}

void ActivityDbNetPostCommentJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbNetDeleteCommentJob::Run()
{
	return SceActivityDb_7B68F69D()->RunDeleteComment(&m_key);
}

void ActivityDbNetDeleteCommentJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbNetVoteGoodJobJob::Run()
{
	return SceActivityDb_7B68F69D()->RunVoteGoodJob(&m_key);
}

void ActivityDbNetVoteGoodJobJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbNetCancelGoodJobJob::Run()
{
	return SceActivityDb_7B68F69D()->RunCancelGoodJob(&m_key);
}

void ActivityDbNetCancelGoodJobJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbNetRemoveAllMyStatusesJob::Run()
{
	return SceActivityDb_7B68F69D()->RunRemoveAllMyStatuses(&m_user);
}

void ActivityDbNetRemoveAllMyStatusesJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}

int ActivityDbNetGetAnnounceInfoJob::Run()
{
	return SceActivityDb_7B68F69D()->RunGetAnnounceInfo(&m_owner, m_info);
}

void ActivityDbNetGetAnnounceInfoJob::Done(int result)
{
	if (m_wait != NULL)
		m_wait->Done(result);
}
