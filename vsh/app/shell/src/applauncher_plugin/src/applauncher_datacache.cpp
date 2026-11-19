/*
 * applauncher_datacache.cpp: the "DataCacheManager", the cache manager the home loads its pages'
 * images with (0x810BA444-0x810BA6FC). The file's name and the names here are ours
 * (PLACEHOLDER): no record of the 3.6x shell names them; "DataCacheManager" is Sony's name of
 * the manager.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/string>
#include <thread/thread.h>
#include <cache/cache_manager.h>
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* how often a load is tried again, and the largest image read (in KiB) */
#define APPLAUNCHER_DATA_CACHE_RETRY_COUNT	2
#define APPLAUNCHER_DATA_CACHE_MAX_SIZE_KB	600

/* PLACEHOLDER: what holds the manager (4 bytes) */
class ApplauncherDataCache {
public:
	ApplauncherDataCache(Plugin *plugin);

	cache::CacheManager *m_manager;
};

/* PLACEHOLDER: the data cache (0x815552C0) */
ApplauncherDataCache *g_applauncherDataCache;

/* (0x810BA444) */
ApplauncherDataCache::ApplauncherDataCache(Plugin *plugin)
{
	g_applauncherDataCache = this;

	cache::CacheManager::Params param;

	param.name = string("DataCacheManager");
	param.longlife_heap = Framework::Instance()->GetDecodeHeap();
	param.instant_heap = Framework::Instance()->GetDecodeHeap();
	param.surface_pool = plugin->m_surface_pool;
	param.cache_longlife_heap = false;
	param.image_cache_divide_thread = true;

	m_manager = new cache::CacheManager(&param);
}

/* (0x810BA558) */
void applauncherDataCacheCreate(Plugin *plugin)
{
	new ApplauncherDataCache(plugin);
}

/* (0x810BA56E) */
void applauncherDataCacheDestroy(void)
{
	ApplauncherDataCache *cache = g_applauncherDataCache;

	if (cache == NULL)
		return;

	cache->m_manager->RequestShutdown();
	delete cache->m_manager;
	g_applauncherDataCache = NULL;
	delete cache;
}

/* (0x810BA5A2) */
int applauncherDataCacheLoad(const string &path,
			     intrusive_ptr<cache::CacheImageInterface> *image,
			     const cache::CacheImage::Option *option, job::JobQueue *queue,
			     bool directLoad)
{
	cache::CacheImage::Option opt;

	/* the option's defaults (inline in Sony's build) */
	opt.auto_delete = true;
	opt.retry = true;
	opt.retry_count = -1;
	opt.reserved = 1;
	opt.read_size = 0;
	opt.read_offset = 0;
	opt.max_size_kb = 0;
	opt.open_func = NULL;
	opt.open_userdata = NULL;
	memset(&opt.sub_rect, 0, sizeof(opt.sub_rect));
	memset(&opt.scale, 0, sizeof(opt.scale));
	opt.resize_type = image::ImageResizeType_NearestNeighbor;
	opt.direct_load = false;
	opt.image_order = ImageOrder_NONE;
	opt.image_order_param = 0;
	opt.check_image = NULL;
	opt.check_image_userdata = NULL;
	opt.surface_flag = false;
	if (option != NULL)
		memcpy(&opt, option, offsetof(cache::CacheImage::Option, surface_flag) + 1);
	opt.direct_load = directLoad;
	opt.retry_count = APPLAUNCHER_DATA_CACHE_RETRY_COUNT;
	opt.max_size_kb = APPLAUNCHER_DATA_CACHE_MAX_SIZE_KB;

	int ret = g_applauncherDataCache->m_manager->CreateCache(path, *image, &opt, queue, NULL);

	if (ret == 0)
		(*image)->GetImage();
	return ret;
}

/* (0x810BA678) */
void applauncherDataCacheMakeSpace(void)
{
	g_applauncherDataCache->m_manager->MakeSpaceSurfacePool();
}
