/*
 * SceShell livearea_plugin: LiveArea updator task queue & mutex
 * 3.65-CEX: 0x811B216C - 0x811B307C (sti 0x811B300C)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <std/list>
#include <std/string>
#include <thread/thread.h>

#include "../../livespace_db/lsdb_keys.h"

extern "C" {
extern SceUInt64 ScePafCommon_AC2F3710(void);
}

extern int liveareaCronEnqueueJob(paf::common::SharedPtr<int> *job); /* 0x81199EC0 */
extern int *liveareaRequestUpdateJobCtor(void *mem, paf::common::SharedPtr<int> *task,
	SceUInt64 id);						/* 0x811B3094 */
extern int *liveareaUpdateTaskCtor(void *mem, const char *titleId, const SceUInt64 *ts,
	SceUInt8 a4, SceUInt8 a5, int a6, int a7, int a8);	/* 0x811B31D2 */
extern int *liveareaPromoteTaskCtor(void *mem, const char *titleId, int a3, int a4, int a5); /* 0x811B5474 */
extern int *liveareaDeleteTaskCtor(void *mem, const char *titleId, int a3, int a4, int a5);  /* 0x811B577A */
extern int *liveareaCleanTaskCtor(void *mem, const char *titleId); /* 0x811B6554 */

struct LiveAreaUpdatorEntry {
	SceUInt64 id;
	paf::string titleId;
	int kind;
	paf::common::SharedPtr<int> job;
};

class LiveAreaUpdatorImpl {
public:
	LiveAreaUpdatorImpl();
	~LiveAreaUpdatorImpl();

	int RequestUpdate(SceUInt64 *outId, const char *titleId, const SceUInt64 *ts,
		char a5, SceUInt8 a6, SceUInt8 a7, int a8, int a9, int a10);
	int RequestPromote(SceUInt64 *outId, const char *titleId, int a4, int a5, int a6, int a7);
	int RequestDelete(SceUInt64 *outId, const char *titleId, int a4, int a5, int a6, int a7);
	int RequestClean(SceUInt64 *outId, const char *titleId);
	void RemoveEntry(const LiveAreaUpdatorEntry &entry);
	int CancelById(const SceUInt64 &id);

private:
	paf::list<LiveAreaUpdatorEntry> m_entries;		/* +0 (12 bytes) */
	int m_pad;						/* +12 */
	paf::thread::RMutex m_mutex;				/* +16 (32 bytes) */
	int m_state;						/* +48 */
	int m_pad2;						/* +52 */
};

/* 0x811B216C */
LiveAreaUpdatorImpl::LiveAreaUpdatorImpl()
	: m_mutex("SceShellLiveAreaUpdatorMutex", 0), m_state(-1)
{}

/* 0x811B21A8 */
LiveAreaUpdatorImpl::~LiveAreaUpdatorImpl()
{}

/* 0x811B21C0 */
int LiveAreaUpdatorImpl::RequestUpdate(SceUInt64 *outId, const char *titleId, const SceUInt64 *ts,
	char, SceUInt8 a6, SceUInt8 a7, int a8, int a9, int a10)
{
	if (!titleId)
		return (int)0x80107C02;

	SceUInt64 tsCopy = *ts;
	*outId = ScePafCommon_AC2F3710();

	void *tmem = operator new(40);
	int *task = tmem ? liveareaUpdateTaskCtor(tmem, titleId, &tsCopy, a6, a7, a8, a9, a10) : NULL;
	paf::common::SharedPtr<int> spTask(task);

	void *jmem = operator new(56);
	int *job = jmem ? liveareaRequestUpdateJobCtor(jmem, &spTask, *outId) : NULL;
	paf::common::SharedPtr<int> spJob(job);

	int res = liveareaCronEnqueueJob(&spJob);
	if (res == 0) {
		LiveAreaUpdatorEntry ent;
		ent.id = *outId;
		ent.titleId = titleId;
		ent.kind = 0;
		ent.job = spJob;

		m_mutex.Lock();
		m_entries.push_back(ent);
		m_mutex.Unlock();
	}
	return res;
}

/* 0x811B2532 */
int LiveAreaUpdatorImpl::RequestPromote(SceUInt64 *outId, const char *titleId, int,
	int a5, int a6, int a7)
{
	if (!titleId)
		return (int)0x80107C02;

	*outId = ScePafCommon_AC2F3710();

	void *tmem = operator new(32);
	int *task = tmem ? liveareaPromoteTaskCtor(tmem, titleId, a5, a6, a7) : NULL;
	paf::common::SharedPtr<int> spTask(task);

	void *jmem = operator new(56);
	int *job = jmem ? liveareaRequestUpdateJobCtor(jmem, &spTask, *outId) : NULL;
	paf::common::SharedPtr<int> spJob(job);

	int res = liveareaCronEnqueueJob(&spJob);
	if (res == 0) {
		LiveAreaUpdatorEntry ent;
		ent.id = *outId;
		ent.titleId = titleId;
		ent.kind = 1;
		ent.job = spJob;

		m_mutex.Lock();
		m_entries.push_back(ent);
		m_mutex.Unlock();
	}
	return res;
}

/* 0x811B287E */
int LiveAreaUpdatorImpl::RequestDelete(SceUInt64 *outId, const char *titleId, int a4,
	int, int a6, int a7)
{
	if (!titleId)
		return (int)0x80107C02;

	*outId = ScePafCommon_AC2F3710();

	void *tmem = operator new(56);
	int *task = tmem ? liveareaDeleteTaskCtor(tmem, titleId, a4, a6, a7) : NULL;
	paf::common::SharedPtr<int> spTask(task);

	void *jmem = operator new(56);
	int *job = jmem ? liveareaRequestUpdateJobCtor(jmem, &spTask, *outId) : NULL;
	paf::common::SharedPtr<int> spJob(job);

	int res = liveareaCronEnqueueJob(&spJob);
	if (res == 0) {
		LiveAreaUpdatorEntry ent;
		ent.id = *outId;
		ent.titleId = titleId;
		ent.kind = 2;
		ent.job = spJob;

		m_mutex.Lock();
		m_entries.push_back(ent);
		m_mutex.Unlock();
	}
	return res;
}

/* 0x811B2BCE */
int LiveAreaUpdatorImpl::RequestClean(SceUInt64 *outId, const char *titleId)
{
	*outId = ScePafCommon_AC2F3710();

	void *tmem = operator new(28);
	int *task = tmem ? liveareaCleanTaskCtor(tmem, titleId) : NULL;
	paf::common::SharedPtr<int> spTask(task);

	void *jmem = operator new(56);
	int *job = jmem ? liveareaRequestUpdateJobCtor(jmem, &spTask, *outId) : NULL;
	paf::common::SharedPtr<int> spJob(job);

	int res = liveareaCronEnqueueJob(&spJob);
	if (res == 0) {
		LiveAreaUpdatorEntry ent;
		ent.id = *outId;
		ent.titleId = titleId ? titleId : "";
		ent.kind = 3;
		ent.job = spJob;

		m_mutex.Lock();
		m_entries.push_back(ent);
		m_mutex.Unlock();
	}
	return res;
}

/* 0x811B2F12 */
void LiveAreaUpdatorImpl::RemoveEntry(const LiveAreaUpdatorEntry &entry)
{
	m_mutex.Lock();
	for (paf::list<LiveAreaUpdatorEntry>::iterator it = m_entries.begin(); it != m_entries.end(); ) {
		if (it->id == entry.id && it->kind == entry.kind)
			it = m_entries.erase(it);
		else
			++it;
	}
	m_mutex.Unlock();
}

/* 0x811B2FA8 */
int LiveAreaUpdatorImpl::CancelById(const SceUInt64 &id)
{
	m_mutex.Lock();
	for (paf::list<LiveAreaUpdatorEntry>::iterator it = m_entries.begin(); it != m_entries.end(); ++it) {
		if (it->id == id) {
			RemoveEntry(*it);
			break;
		}
	}
	m_mutex.Unlock();
	return 0;
}
