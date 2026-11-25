/*
 * SceShell livearea_plugin: LiveArea updator task queue & mutex
 * 3.65-CEX: 0x811B216C - 0x811B307C (sti 0x811B300C)
 */
#include <std/string.h>
#include "livearea_internal.h"

extern "C" {
extern SceUInt64 ScePafCommon_AC2F3710(void);
}

struct LiveAreaUpdatorEntry {
	SceUInt64 id;
	paf::string titleId;
	int kind;
	paf::common::SharedPtr<paf::job::JobItem> job;
};

class LiveAreaUpdatorImpl {
public:
	LiveAreaUpdatorImpl();
	~LiveAreaUpdatorImpl();

	int RequestContentUpdate(SceUInt64 *outId, const char *titleId, const SceUInt64 *timestamp,
				 bool requireNetwork, SceUInt8 checkDevDir, SceUInt8 postNotice,
				 void *callback, void *userdata, const char *localPkg);
	int RequestPatchCheck(SceUInt64 *outId, const char *titleId, bool requireNetwork,
			      int appType, void *callback, void *userdata);
	int RequestActivityUpdate(SceUInt64 *outId, const char *titleId, int forceFlag,
				  bool requireNetwork, void *callback, void *userdata);
	int RequestClean(SceUInt64 *outId, const char *titleId, bool requireNetwork,
			 void *callback, void *userdata);
	void RemoveEntry(const LiveAreaUpdatorEntry &entry);
	int CancelById(const SceUInt64 &id);

private:
	paf::list<LiveAreaUpdatorEntry> m_entries;		/* +0x00 (12 bytes) */
	int listAlignPadding;					/* +0x0C */
	paf::thread::RMutex m_mutex;				/* +0x10 (32 bytes) */
	int m_state;						/* +0x30 */
	int tailAlignPadding;					/* +0x34 */
};

/* 0x811B216C */
LiveAreaUpdatorImpl::LiveAreaUpdatorImpl()
	: listAlignPadding(0),
	  m_mutex("SceShellLiveAreaUpdatorMutex", 0),
	  m_state(-1),
	  tailAlignPadding(0)
{}

/* 0x811B21A8 */
LiveAreaUpdatorImpl::~LiveAreaUpdatorImpl()
{}

/* 0x811B21C0 */
int LiveAreaUpdatorImpl::RequestContentUpdate(SceUInt64 *outId, const char *titleId,
					      const SceUInt64 *timestamp, bool requireNetwork,
					      SceUInt8 checkDevDir, SceUInt8 postNotice,
					      void *callback, void *userdata, const char *localPkg)
{
	if (titleId == NULL)
		return (int)0x80107C02;

	SceUInt64 tsCopy = *timestamp;
	*outId = ScePafCommon_AC2F3710();

	void *taskMem = operator new(40);
	LiveAreaUpdateTaskBase *task = taskMem
		? liveareaContentUpdateTaskCtor(taskMem, titleId, &tsCopy, checkDevDir,
						postNotice, callback, userdata, localPkg)
		: NULL;
	paf::common::SharedPtr<LiveAreaUpdateTaskBase> taskRef(task);

	void *jobMem = operator new(56);
	paf::job::JobItem *job = jobMem
		? liveareaRequestUpdateJobCtor(jobMem, *outId, this, requireNetwork ? 1 : 0, taskRef)
		: NULL;
	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);

	int res = liveareaCronEnqueueJob(jobRef);
	if (res == 0) {
		LiveAreaUpdatorEntry entry;
		entry.id = *outId;
		entry.titleId = titleId;
		entry.kind = 0;
		entry.job = jobRef;

		m_mutex.Lock();
		m_entries.push_back(entry);
		m_mutex.Unlock();
	}
	return res;
}

/* 0x811B2532 */
int LiveAreaUpdatorImpl::RequestPatchCheck(SceUInt64 *outId, const char *titleId,
					   bool requireNetwork, int appType,
					   void *callback, void *userdata)
{
	if (titleId == NULL)
		return (int)0x80107C02;

	*outId = ScePafCommon_AC2F3710();

	void *taskMem = operator new(32);
	LiveAreaUpdateTaskBase *task = taskMem
		? liveareaPatchCheckTaskCtor(taskMem, titleId, appType, callback, userdata)
		: NULL;
	paf::common::SharedPtr<LiveAreaUpdateTaskBase> taskRef(task);

	void *jobMem = operator new(56);
	paf::job::JobItem *job = jobMem
		? liveareaRequestUpdateJobCtor(jobMem, *outId, this, requireNetwork ? 1 : 0, taskRef)
		: NULL;
	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);

	int res = liveareaCronEnqueueJob(jobRef);
	if (res == 0) {
		LiveAreaUpdatorEntry entry;
		entry.id = *outId;
		entry.titleId = titleId;
		entry.kind = 1;
		entry.job = jobRef;

		m_mutex.Lock();
		m_entries.push_back(entry);
		m_mutex.Unlock();
	}
	return res;
}

/* 0x811B287E */
int LiveAreaUpdatorImpl::RequestActivityUpdate(SceUInt64 *outId, const char *titleId,
					       int forceFlag, bool requireNetwork,
					       void *callback, void *userdata)
{
	if (titleId == NULL)
		return (int)0x80107C02;

	*outId = ScePafCommon_AC2F3710();

	void *taskMem = operator new(56);
	LiveAreaUpdateTaskBase *task = taskMem
		? liveareaActivityUpdateTaskCtor(taskMem, titleId, forceFlag, callback, userdata)
		: NULL;
	paf::common::SharedPtr<LiveAreaUpdateTaskBase> taskRef(task);

	void *jobMem = operator new(56);
	paf::job::JobItem *job = jobMem
		? liveareaRequestUpdateJobCtor(jobMem, *outId, this, requireNetwork ? 1 : 0, taskRef)
		: NULL;
	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);

	int res = liveareaCronEnqueueJob(jobRef);
	if (res == 0) {
		LiveAreaUpdatorEntry entry;
		entry.id = *outId;
		entry.titleId = titleId;
		entry.kind = 2;
		entry.job = jobRef;

		m_mutex.Lock();
		m_entries.push_back(entry);
		m_mutex.Unlock();
	}
	return res;
}

/* 0x811B2BCE */
int LiveAreaUpdatorImpl::RequestClean(SceUInt64 *outId, const char *titleId, bool requireNetwork,
				      void *callback, void *userdata)
{
	*outId = ScePafCommon_AC2F3710();

	void *taskMem = operator new(28);
	LiveAreaUpdateTaskBase *task = taskMem
		? liveareaCleanTaskCtor(taskMem, titleId, callback, userdata)
		: NULL;
	paf::common::SharedPtr<LiveAreaUpdateTaskBase> taskRef(task);

	void *jobMem = operator new(56);
	paf::job::JobItem *job = jobMem
		? liveareaRequestUpdateJobCtor(jobMem, *outId, this, requireNetwork ? 1 : 0, taskRef)
		: NULL;
	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);

	int res = liveareaCronEnqueueJob(jobRef);
	if (res == 0) {
		LiveAreaUpdatorEntry entry;
		entry.id = *outId;
		entry.titleId = titleId ? titleId : "";
		entry.kind = 3;
		entry.job = jobRef;

		m_mutex.Lock();
		m_entries.push_back(entry);
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

LiveAreaUpdatorImpl *liveareaUpdatorImplCtor(void *mem)
{
	return new (mem) LiveAreaUpdatorImpl();
}

LiveAreaUpdatorImpl *liveareaUpdatorImplDtor(LiveAreaUpdatorImpl *self)
{
	self->~LiveAreaUpdatorImpl();
	return self;
}

int liveareaUpdatorImplRequestContentUpdate(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
					    const char *titleId, const SceUInt64 *timestamp,
					    bool requireNetwork, SceUInt8 checkDevDir,
					    SceUInt8 postNotice, void *callback,
					    void *userdata, const char *localPkg)
{
	return self->RequestContentUpdate(outId, titleId, timestamp, requireNetwork,
					  checkDevDir, postNotice, callback, userdata, localPkg);
}

int liveareaUpdatorImplRequestPatchCheck(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
					 const char *titleId, bool requireNetwork,
					 int appType, void *callback, void *userdata)
{
	return self->RequestPatchCheck(outId, titleId, requireNetwork, appType, callback, userdata);
}

int liveareaUpdatorImplRequestActivityUpdate(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
					     const char *titleId, int forceFlag,
					     bool requireNetwork, void *callback, void *userdata)
{
	return self->RequestActivityUpdate(outId, titleId, forceFlag, requireNetwork,
					   callback, userdata);
}

int liveareaUpdatorImplRequestClean(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
				    const char *titleId, bool requireNetwork,
				    void *callback, void *userdata)
{
	return self->RequestClean(outId, titleId, requireNetwork, callback, userdata);
}

int liveareaUpdatorCancelById(LiveAreaUpdatorImpl *updator, const SceUInt64 *id)
{
	return updator->CancelById(*id);
}
