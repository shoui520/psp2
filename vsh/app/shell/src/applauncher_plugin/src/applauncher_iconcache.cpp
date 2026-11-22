/*
 * applauncher_iconcache.cpp: the "IconCacheManager", the cache manager the icons' images are
 * loaded with (0x810BD248-0x810BD534). The file's name and the names here are ours
 * (PLACEHOLDER): no record of the 3.6x shell names them; "IconCacheManager" is Sony's name of
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
#include "../../../shell.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* how often a load is tried again, and the largest image read (in KiB) */
#define APPLAUNCHER_ICON_CACHE_RETRY_COUNT	10
#define APPLAUNCHER_ICON_CACHE_MAX_SIZE_KB	32

/* PLACEHOLDER: the icon cache (0x815553B0) */
ApplauncherIconCache *g_applauncherIconCache;

/* (0x810BD248) */
ApplauncherIconCache::ApplauncherIconCache(Plugin *plugin)
{
	g_applauncherIconCache = this;

	cache::CacheManager::Params param;

	param.name = string("IconCacheManager");
	param.longlife_heap = Framework::Instance()->GetDecodeHeap();
	param.instant_heap = Framework::Instance()->GetDecodeHeap();
	param.surface_pool = shellIconCacheGetSurfacePool();
	param.cache_longlife_heap = false;
	param.image_cache_divide_thread = true;

	m_manager = new cache::CacheManager(&param);
}

/* (0x810BD35C) */
void applauncherIconCacheCreate(Plugin *plugin)
{
	new ApplauncherIconCache(plugin);
}

/* (0x810BD372) */
void applauncherIconCacheDestroy(void)
{
	ApplauncherIconCache *cache = g_applauncherIconCache;

	if (cache == NULL)
		return;

	cache->m_manager->RequestShutdown();
	delete cache->m_manager;
	g_applauncherIconCache = NULL;
	delete cache;
}

/* (0x810BD3A6) */
int applauncherIconCacheLoad(const string &path,
			     intrusive_ptr<cache::CacheImageInterface> *image,
			     const cache::CacheImage::Option *option, job::JobQueue *queue,
			     bool noSurface)
{
	cache::CacheImage::Option opt;

	/* the option's defaults (inline in Sony's build) */
	opt.auto_delete = true;
	opt.retry = true;
	opt.retry_count = -1;
	opt.reserved = true;
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
	opt.retry_count = APPLAUNCHER_ICON_CACHE_RETRY_COUNT;
	opt.max_size_kb = APPLAUNCHER_ICON_CACHE_MAX_SIZE_KB;
	if (opt.open_func == NULL)
		opt.surface_flag = true;
	if (noSurface)
		opt.surface_flag = false;

	int ret = g_applauncherIconCache->m_manager->CreateCache(path, *image, &opt, queue, NULL);

	if (ret == 0)
		(*image)->GetImage();
	return ret;
}

/* (0x810BD482) */
void applauncherIconCacheClear(intrusive_ptr<cache::CacheImageInterface> &image)
{
	g_applauncherIconCache->m_manager->ClearCache(image);
}

/* (0x810BD498) */
void applauncherIconCacheClearPath(const string &path)
{
	g_applauncherIconCache->m_manager->ClearCache(path);
}

/* (0x810BD4AE) */
void applauncherIconCacheMakeSpace(void)
{
	g_applauncherIconCache->m_manager->MakeSpaceSurfacePool();
}
