/*
 * livearea_updater.cpp: livearea_plugin updater implementation
 * (0x811A2A78-0x811A2CBC, sti 0x811A2C4A). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include "../../livespace_db/lsdb_keys.h"

extern void *liveareaTaskMgrCtor(void *mem);			/* 0x811B204C */
extern void *liveareaTaskMgrDtor(void *obj);			/* 0x811B2068 */
extern int liveareaTaskMgrSub07C(void *obj);			/* 0x811B207C */
extern int liveareaTaskMgrSub0AC(void *obj);			/* 0x811B20AC */
extern int liveareaTaskMgrSub0C8(void *obj);			/* 0x811B20C8 */
extern int liveareaTaskMgrSub0E4(void *obj, void *out, const char *title, int flag, void *cb, int appId); /* 0x811B20E4 */

extern void *liveareaDailyTaskCtor(void *mem, int arg);		/* 0x811A2CBC */
extern int liveareaDailyTaskStop(void *obj);			/* 0x811A2CC8 */
extern void *liveareaDailyTaskDtor(void *obj);			/* 0x811A2CEC */
extern int liveareaDailyTaskStart(void *obj);			/* 0x811A2D1C */

struct LiveAreaUpdaterImpl {
	void *m_taskMgr;					/* +0x00 */
	void *m_dailyTask;					/* +0x04 */
	bool m_started;						/* +0x08 */
	unsigned char m_pad[3];
};

/* 0x811A2A78 */
LiveAreaUpdaterImpl *liveareaUpdaterImplCtor(LiveAreaUpdaterImpl *self)
{
	self->m_started = false;
	void *m1 = operator new(4);
	self->m_taskMgr = (m1 != NULL) ? liveareaTaskMgrCtor(m1) : NULL;
	void *m2 = operator new(4);
	self->m_dailyTask = (m2 != NULL) ? liveareaDailyTaskCtor(m2, 0) : NULL;
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
int liveareaUpdaterImplUpdateInfo(LiveAreaUpdaterImpl *self, void *out, const char *title)
{
	(void)out;
	if (!self->m_started)
		return (int)0x80107901;
	if (title != NULL)
		return liveareaTaskMgrSub07C(self->m_taskMgr);
	return (int)0x80107902;
}

/* 0x811A2B72 */
int liveareaUpdaterImplUpdateExt(LiveAreaUpdaterImpl *self, void *out, const char *title,
				 int unusedArg, int mode)
{
	(void)out;
	(void)unusedArg;
	if (!self->m_started)
		return (int)0x80107901;
	if (title == NULL)
		return (int)0x80107902;
	if (mode == 1 || mode == 2)
		return liveareaTaskMgrSub0AC(self->m_taskMgr);
	return (int)0x80107903;
}

/* 0x811A2BC8 */
int liveareaUpdaterImplUpdateFeed(LiveAreaUpdaterImpl *self, void *out, const char *title,
				  int mode)
{
	(void)out;
	if (!self->m_started)
		return (int)0x80107901;
	if (title == NULL)
		return (int)0x80107902;
	if (mode < 2)
		return liveareaTaskMgrSub0C8(self->m_taskMgr);
	return (int)0x80107902;
}

/* 0x811A2C18 */
int liveareaUpdaterImplUpdateDolce(LiveAreaUpdaterImpl *self, void *out, const char *title,
				   int mode, void *cb, int appId)
{
	if (!self->m_started)
		return (int)0x80107901;
	return liveareaTaskMgrSub0E4(self->m_taskMgr, out, title, (mode == 1) ? 1 : 0, cb, appId);
}
