/*
 * livearea_updater.cpp: livearea_plugin updater implementation
 * (0x811A2A78-0x811A2CBC, sti 0x811A2C4A).
 */
#include "livearea_internal.h"

struct LiveAreaUpdaterImpl {
	LiveAreaTaskMgr *m_taskMgr;				/* +0x00 */
	LiveAreaTaskMgr *m_dailyTask;				/* +0x04 */
	bool m_started;						/* +0x08 */
	unsigned char statePadding[3];
};

/* 0x811A2A78 */
LiveAreaUpdaterImpl *liveareaUpdaterImplCtor(LiveAreaUpdaterImpl *self)
{
	self->m_started = false;
	void *taskMgrMem = operator new(4);
	self->m_taskMgr = (taskMgrMem != NULL) ? liveareaTaskMgrCtor(taskMgrMem) : NULL;
	void *dailyTaskMem = operator new(4);
	self->m_dailyTask = (dailyTaskMem != NULL) ? liveareaDailyTaskCtor(dailyTaskMem, 0) : NULL;
	return self;
}

/* 0x811A2AB0 */
int liveareaUpdaterImplStop(LiveAreaUpdaterImpl *self)
{
	if (self->m_started) {
		liveareaDailyTaskStop(self->m_dailyTask);
		self->m_started = false;
		return 0;
	}
	return 0;
}

/* 0x811A2AC4 */
LiveAreaUpdaterImpl *liveareaUpdaterImplDtor(LiveAreaUpdaterImpl *self)
{
	if (self->m_started) {
		liveareaDailyTaskStop(self->m_dailyTask);
		self->m_started = false;
	}
	if (self->m_dailyTask != NULL) {
		liveareaDailyTaskDtor(self->m_dailyTask);
		operator delete(self->m_dailyTask);
	}
	if (self->m_taskMgr != NULL) {
		liveareaTaskMgrDtor(self->m_taskMgr);
		operator delete(self->m_taskMgr);
	}
	return self;
}

/* 0x811A2AF4 */
int liveareaUpdaterImplStart(LiveAreaUpdaterImpl *self)
{
	if (self->m_started)
		return (int)0x80107901;
	int ret = liveareaDailyTaskStart(self->m_dailyTask);
	if (ret == 0) {
		self->m_started = true;
		return 0;
	}
	return ret;
}

/* 0x811A2B1A */
int liveareaUpdaterImplUpdateContent(LiveAreaUpdaterImpl *self, SceUInt64 *outId,
				     const char *titleId, const SceUInt64 *timestamp,
				     int mode, void *callback, int pageKey)
{
	if (!self->m_started)
		return (int)0x80107901;
	if (titleId != NULL)
		return liveareaTaskMgrRequestContentUpdate(self->m_taskMgr, outId, titleId, timestamp,
							   mode == 1, 1, 0, callback,
							   (void *)pageKey, NULL);
	return (int)0x80107902;
}

/* 0x811A2B72 */
int liveareaUpdaterImplCheckPatch(LiveAreaUpdaterImpl *self, SceUInt64 *outId,
				  const char *titleId, int mode, int appType,
				  void *callback, int pageKey)
{
	if (!self->m_started)
		return (int)0x80107901;
	if (titleId == NULL)
		return (int)0x80107902;
	if (appType == 1 || appType == 2)
		return liveareaTaskMgrRequestPatchCheck(self->m_taskMgr, outId, titleId,
							mode == 1, appType, callback,
							(void *)pageKey);
	return (int)0x80107903;
}

/* 0x811A2BC8 */
int liveareaUpdaterImplUpdateFeed(LiveAreaUpdaterImpl *self, SceUInt64 *outId,
				  const char *titleId, int mode, void *callback, int pageKey)
{
	if (!self->m_started)
		return (int)0x80107901;
	if (titleId == NULL)
		return (int)0x80107902;
	if (mode < 2)
		return liveareaTaskMgrRequestActivityUpdate(self->m_taskMgr, outId, titleId,
							    mode, mode == 1, callback,
							    (void *)pageKey);
	return (int)0x80107902;
}

/* 0x811A2C18 */
int liveareaUpdaterImplCleanCache(LiveAreaUpdaterImpl *self, SceUInt64 *outId,
				  const char *titleId, int mode, void *callback, int pageKey)
{
	if (!self->m_started)
		return (int)0x80107901;
	return liveareaTaskMgrRequestClean(self->m_taskMgr, outId, titleId, mode == 1,
					   callback, (void *)pageKey);
}
