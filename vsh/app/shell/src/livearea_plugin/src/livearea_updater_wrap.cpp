/*
 * livearea_updater_wrap.cpp: livearea_plugin updater facade
 * (0x811A2910-0x811A2A78, sti 0x811A2A08).
 */
#include "livearea_internal.h"

/* 0x811A2910 */
LiveAreaUpdater *liveareaUpdaterCtor(LiveAreaUpdater *updater)
{
	LiveAreaUpdaterImpl *mem = (LiveAreaUpdaterImpl *)operator new(12);
	updater->m_impl = (mem != NULL) ? liveareaUpdaterImplCtor(mem) : NULL;
	return updater;
}

/* 0x811A292C */
LiveAreaUpdater *liveareaUpdaterDtor(LiveAreaUpdater *updater)
{
	if (updater->m_impl != NULL) {
		liveareaUpdaterImplDtor(updater->m_impl);
		operator delete(updater->m_impl);
	}
	return updater;
}

/* 0x811A2940 */
int liveareaUpdaterStart(LiveAreaUpdater *updater)
{
	return liveareaUpdaterImplStart(updater->m_impl);
}

/* 0x811A294A */
int liveareaUpdaterStop(LiveAreaUpdater *updater)
{
	return liveareaUpdaterImplStop(updater->m_impl);
}

/* 0x811A2954 */
int liveareaUpdaterRequestContentUpdate(LiveAreaUpdater *updater, SceUInt64 *outId,
					const char *titleId, const SceUInt64 *timestamp,
					int mode, void *callback, int pageKey)
{
	return liveareaUpdaterImplUpdateContent(updater->m_impl, outId, titleId, timestamp,
						mode, callback, pageKey);
}

/* 0x811A2972 */
int liveareaUpdaterRequestActivityRevert(LiveAreaUpdater *updater, SceUInt64 *outId,
					 int titleId, int flags, int mode, int option)
{
	return liveareaUpdaterImplUpdateContent(updater->m_impl, outId, (const char *)titleId,
						(const SceUInt64 *)flags, mode, (void *)option, 0);
}

/* 0x811A29C0 */
int liveareaUpdaterRequestPatchCheck(LiveAreaUpdater *updater, SceUInt64 *outId,
				     const char *titleId, int mode, int appType,
				     void *callback, int pageKey)
{
	return liveareaUpdaterImplCheckPatch(updater->m_impl, outId, titleId, mode, appType,
					     callback, pageKey);
}

/* 0x811A29DA */
int liveareaUpdaterRequestFeedUpdate(LiveAreaUpdater *updater, SceUInt64 *outId,
				     const char *titleId, int mode, void *callback, int pageKey)
{
	return liveareaUpdaterImplUpdateFeed(updater->m_impl, outId, titleId, mode, callback, pageKey);
}

/* 0x811A29F0 */
int liveareaUpdaterRequestClean(LiveAreaUpdater *updater, SceUInt64 *outId,
				const char *titleId, int mode, void *callback, int pageKey)
{
	return liveareaUpdaterImplCleanCache(updater->m_impl, outId, titleId, mode, callback, pageKey);
}
