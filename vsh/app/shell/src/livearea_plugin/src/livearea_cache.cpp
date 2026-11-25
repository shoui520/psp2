/*
 * livearea_cache.cpp: livearea_plugin common CacheManager ("CmnCacheManager";
 * 0x8119A408-0x8119A698, sti 0x8119A626).
 */
#include "livearea_internal.h"

LiveAreaCacheMgr *g_liveareaCache;				/* 0x81567080 */

/* 0x8119A408 */
LiveAreaCacheMgr::LiveAreaCacheMgr(paf::Plugin *plugin)
{
	g_liveareaCache = this;

	paf::cache::CacheManager::Params param;
	param.name = "CmnCacheManager";
	param.longlife_heap = paf::Framework::Instance()->GetDecodeHeap();
	param.instant_heap = paf::Framework::Instance()->GetDecodeHeap();
	param.surface_pool = plugin->m_surface_pool;
	param.cache_longlife_heap = false;
	param.image_cache_divide_thread = true;
	m_manager = new paf::cache::CacheManager(&param);
}

/* 0x8119A51E */
int LiveAreaCacheMgr::CreateCache(const paf::string &path,
				  paf::intrusive_ptr<paf::cache::CacheImageInterface> &outRef,
				  const paf::cache::CacheImage::Option *option,
				  paf::job::JobQueue *queue)
{
	paf::cache::CacheImage::Option localOption;
	if (option != NULL)
		localOption = *option;
	localOption.read_offset = 10;

	int ret = m_manager->CreateCache(path, outRef, &localOption, queue, NULL);
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
	if (g_liveareaCache != NULL) {
		g_liveareaCache->m_manager->RequestShutdown();
		delete g_liveareaCache->m_manager;
		LiveAreaCacheMgr *mgr = g_liveareaCache;
		g_liveareaCache = NULL;
		delete mgr;
	}
}
