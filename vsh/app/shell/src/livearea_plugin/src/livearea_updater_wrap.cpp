/*
 * livearea_updater_wrap.cpp: livearea_plugin updater wrapper
 * (0x811A2910-0x811A2A78, sti 0x811A2A08). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include "../../livespace_db/lsdb_keys.h"

struct LiveAreaUpdaterImpl;
extern LiveAreaUpdaterImpl *liveareaUpdaterImplCtor(LiveAreaUpdaterImpl *self); /* 0x811A2A78 */
extern int liveareaUpdaterImplStop(LiveAreaUpdaterImpl *self);			/* 0x811A2AB0 */
extern LiveAreaUpdaterImpl *liveareaUpdaterImplDtor(LiveAreaUpdaterImpl *self); /* 0x811A2AC4 */
extern int liveareaUpdaterImplStart(LiveAreaUpdaterImpl *self);			/* 0x811A2AF4 */
extern int liveareaUpdaterImplUpdateInfo(LiveAreaUpdaterImpl *self, void *out, const char *title, SceInt64 *ts, int mode, void *cb, int appId); /* 0x811A2B1A */
extern int liveareaUpdaterImplUpdateInfo2(LiveAreaUpdaterImpl *self, void *out, int titleId, int flags, int mode, int opt); /* 0x811A2B1A */
extern int liveareaUpdaterImplUpdateExt(LiveAreaUpdaterImpl *self, void *out, const char *title, int mode, void *arg, void *cb, int appId); /* 0x811A2B72 */
extern int liveareaUpdaterImplUpdateFeed(LiveAreaUpdaterImpl *self, void *out, const char *title, int mode, void *cb, int appId); /* 0x811A2BC8 */
extern int liveareaUpdaterImplUpdateDolce(LiveAreaUpdaterImpl *self, void *out, const char *title, int mode, void *cb, int appId); /* 0x811A2C18 */

struct LiveAreaUpdater {
	LiveAreaUpdaterImpl *m_impl;
};

/* 0x811A2910 */
LiveAreaUpdater *liveareaUpdaterCtor(LiveAreaUpdater *u)
{
	LiveAreaUpdaterImpl *mem = (LiveAreaUpdaterImpl *)operator new(12);
	u->m_impl = (mem != NULL) ? liveareaUpdaterImplCtor(mem) : NULL;
	return u;
}

/* 0x811A292C */
LiveAreaUpdater *liveareaUpdaterDtor(LiveAreaUpdater *u)
{
	if (u->m_impl != NULL) {
		liveareaUpdaterImplDtor(u->m_impl);
		operator delete(u->m_impl);
	}
	return u;
}

/* 0x811A2940 */
int liveareaUpdaterStart(LiveAreaUpdater *u)
{
	return liveareaUpdaterImplStart(u->m_impl);
}

/* 0x811A294A */
int liveareaUpdaterStop(LiveAreaUpdater *u)
{
	return liveareaUpdaterImplStop(u->m_impl);
}

/* 0x811A2954 */
int liveareaUpdaterSub954(LiveAreaUpdater *u, void *out, const char *title, SceInt64 *ts,
			  int mode, void *cb, int appId)
{
	return liveareaUpdaterImplUpdateInfo(u->m_impl, out, title, ts, mode, cb, appId);
}

/* 0x811A2972 */
int liveareaUpdaterSub972(LiveAreaUpdater *u, void *out, int titleId, int flags, int mode, int opt)
{
	return liveareaUpdaterImplUpdateInfo2(u->m_impl, out, titleId, flags, mode, opt);
}

/* 0x811A29C0 */
int liveareaUpdaterSub9C0(LiveAreaUpdater *u, void *out, const char *title, int mode,
			  void *arg, void *cb, int appId)
{
	return liveareaUpdaterImplUpdateExt(u->m_impl, out, title, mode, arg, cb, appId);
}

/* 0x811A29DA */
int liveareaUpdaterSub9DA(LiveAreaUpdater *u, void *out, const char *title, int mode,
			  void *cb, int appId)
{
	return liveareaUpdaterImplUpdateFeed(u->m_impl, out, title, mode, cb, appId);
}

/* 0x811A29F0 */
int liveareaUpdaterSub9F0(LiveAreaUpdater *u, void *out, const char *title, int mode,
			  void *cb, int appId)
{
	return liveareaUpdaterImplUpdateDolce(u->m_impl, out, title, mode, cb, appId);
}
