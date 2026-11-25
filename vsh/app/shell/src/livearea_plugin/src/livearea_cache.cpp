/*
 * livearea_cache.cpp: livearea_plugin common CacheManager ("CmnCacheManager";
 * 0x8119A408-0x8119A698, sti 0x8119A626). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <framework.h>
#include <plugin.h>
#include <thread/thread.h>
#include <cache/cache_manager.h>
#include <graphics/surface/surface.h>
#include "../../livespace_db/lsdb_keys.h"

class LiveAreaCacheMgr {
public:
	LiveAreaCacheMgr(paf::Plugin *plugin);		/* 0x8119A408 */
	int CreateCache(const paf::string &path,
			paf::intrusive_ptr<paf::cache::CacheImageInterface> &outRef,
			const paf::cache::CacheImage::Option *opt,
			paf::job::JobQueue *q);		/* 0x8119A51E */

	paf::cache::CacheManager *m_manager;
};

static LiveAreaCacheMgr *s_liveareaCache;		/* 0x81567080 */

/* 0x8119A408 */
LiveAreaCacheMgr::LiveAreaCacheMgr(paf::Plugin *plugin)
{
	s_liveareaCache = this;

	paf::cache::CacheManager::Params param;
	param.name = "CmnCacheManager";
	param.longlife_heap = *(paf::memory::HeapAllocator **)((char *)paf::Framework::Instance() + 52);
	param.instant_heap = *(paf::memory::HeapAllocator **)((char *)paf::Framework::Instance() + 52);
	param.surface_pool = *(paf::graph::SurfacePool **)((char *)plugin + 212);
	param.cache_longlife_heap = false;
	param.image_cache_divide_thread = true;
	m_manager = new paf::cache::CacheManager(&param);
}

/* 0x8119A51E */
int LiveAreaCacheMgr::CreateCache(const paf::string &path,
				  paf::intrusive_ptr<paf::cache::CacheImageInterface> &outRef,
				  const paf::cache::CacheImage::Option *opt,
				  paf::job::JobQueue *q)
{
	paf::cache::CacheImage::Option cp;
	if (opt != NULL)
		cp = *opt;
	*(int *)((char *)&cp + 0x10) = 10;

	int ret = m_manager->CreateCache(path, outRef, &cp, q, NULL);
	if (ret == 0) {
		paf::intrusive_ptr<paf::graph::Surface> surf = outRef->GetImage();
	}
	return ret;
}

/* 0x8119A5DE */
LiveAreaCacheMgr *liveareaCacheCreate(paf::Plugin *plugin)
{
	return new LiveAreaCacheMgr(plugin);
}

/* 0x8119A5F4 */
void liveareaCacheDestroy(void)
{
	if (s_liveareaCache != NULL) {
		s_liveareaCache->m_manager->RequestShutdown();
		delete s_liveareaCache->m_manager;
		LiveAreaCacheMgr *mgr = s_liveareaCache;
		s_liveareaCache = NULL;
		delete mgr;
	}
}
