/*
 * top_tab_cache_manager.cpp: per-Liveboard-tab image cache manager wrapper
 * (0x812928A0-0x81292AF3).
 */
#include "topmenu_internal.h"

/* 0x812928A0 */
TopTabCacheManager::TopTabCacheManager(paf::graph::SurfacePool *surfacePool)
{
	paf::cache::CacheManager::Params params;
	params.name = paf::string("TopTabCacheManager");
	paf::memory::HeapAllocator *decodeHeap =
		paf::Framework::Instance()->GetDecodeHeap();
	params.longlife_heap = decodeHeap;
	params.instant_heap = decodeHeap;
	params.cache_instance_heap = NULL;
	params.surface_pool = surfacePool;
	params.cache_longlife_heap = false;
	params.image_cache_divide_thread = true;

	m_cacheManager = new paf::cache::CacheManager(&params);
}

/* 0x812929AA */
TopTabCacheManager::~TopTabCacheManager()
{
	m_cacheManager->RequestShutdown();
	delete m_cacheManager;
}

/* 0x812929C4 */
int TopTabCacheManager::CreateImageCache(
	const paf::string &path,
	paf::intrusive_ptr<paf::cache::CacheImageInterface> &outImage,
	const paf::cache::CacheImage::Option *option,
	paf::job::JobQueue *jobQueue)
{
	paf::cache::CacheImage::Option localOption;
	if (option != NULL)
		localOption = *option;
	localOption.retry_count = 10;

	int result = m_cacheManager->CreateCache(path, outImage, option, jobQueue, NULL);
	if (result == 0) {
		paf::intrusive_ptr<paf::graph::Surface> surface = outImage->GetImage();
		(void)surface;
	}
	return result;
}
