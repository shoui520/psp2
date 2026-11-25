/*
 * SceShell livearea_plugin: LiveArea task manager facade
 * 3.65-CEX: 0x811B204C - 0x811B216C (sti 0x811B20FC)
 */

#include <stddef.h>
#include <scetypes.h>

#include "../../livespace_db/lsdb_keys.h"

class LiveAreaUpdatorImpl;

extern LiveAreaUpdatorImpl *liveareaUpdatorImplCtor(void *mem);		/* 0x811B216C */
extern LiveAreaUpdatorImpl *liveareaUpdatorImplDtor(LiveAreaUpdatorImpl *self); /* 0x811B21A8 */
extern int liveareaUpdatorImplRequestUpdate(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
	const char *titleId, const SceUInt64 *ts, char a5, SceUInt8 a6, SceUInt8 a7,
	int a8, int a9, int a10);					/* 0x811B21C0 */
extern int liveareaUpdatorImplRequestPromote(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
	const char *titleId, SceUInt8 a4, int a5, int a6, int a7);	/* 0x811B2532 */
extern int liveareaUpdatorImplRequestDelete(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
	const char *titleId, int a4, int a5, int a6, int a7);		/* 0x811B287E */
extern int liveareaUpdatorImplRequestClean(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
	const char *titleId, SceUInt8 a4, int a5, int a6);		/* 0x811B2BCE */

class LiveAreaTaskMgr {
public:
	LiveAreaTaskMgr();
	~LiveAreaTaskMgr();

	int RequestUpdate(SceUInt64 *outId, const char *titleId, const SceUInt64 *ts,
		char a5, SceUInt8 a6, SceUInt8 a7, int a8, int a9, int a10);
	int RequestPromote(SceUInt64 *outId, const char *titleId, SceUInt8 a4, int a5, int a6, int a7);
	int RequestDelete(SceUInt64 *outId, const char *titleId, int a4, int a5, int a6, int a7);
	int RequestClean(SceUInt64 *outId, const char *titleId, SceUInt8 a4, int a5, int a6);

private:
	LiveAreaUpdatorImpl *m_impl;
};

/* 0x811B204C */
LiveAreaTaskMgr::LiveAreaTaskMgr()
{
	void *mem = operator new(56);
	m_impl = mem ? liveareaUpdatorImplCtor(mem) : NULL;
}

/* 0x811B2068 */
LiveAreaTaskMgr::~LiveAreaTaskMgr()
{
	if (m_impl) {
		liveareaUpdatorImplDtor(m_impl);
		operator delete(m_impl);
	}
}

/* 0x811B207C */
int LiveAreaTaskMgr::RequestUpdate(SceUInt64 *outId, const char *titleId, const SceUInt64 *ts,
	char a5, SceUInt8 a6, SceUInt8 a7, int a8, int a9, int a10)
{
	return liveareaUpdatorImplRequestUpdate(m_impl, outId, titleId, ts, a5, a6, a7, a8, a9, a10);
}

/* 0x811B20AC */
int LiveAreaTaskMgr::RequestPromote(SceUInt64 *outId, const char *titleId, SceUInt8 a4,
	int a5, int a6, int a7)
{
	return liveareaUpdatorImplRequestPromote(m_impl, outId, titleId, a4, a5, a6, a7);
}

/* 0x811B20C8 */
int LiveAreaTaskMgr::RequestDelete(SceUInt64 *outId, const char *titleId, int a4,
	int a5, int a6, int a7)
{
	return liveareaUpdatorImplRequestDelete(m_impl, outId, titleId, a4, a5, a6, a7);
}

/* 0x811B20E4 */
int LiveAreaTaskMgr::RequestClean(SceUInt64 *outId, const char *titleId, SceUInt8 a4,
	int a5, int a6)
{
	return liveareaUpdatorImplRequestClean(m_impl, outId, titleId, a4, a5, a6);
}
