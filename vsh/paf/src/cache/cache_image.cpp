#include <cache/cache_image.h>
#include <cache/cache_manager.h>
#include <cache/cache_manager_impl.h>
#include <common/string_util.h>
#include <graphics/surface/surface.h>
#include <image/image.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <plugin.h>
#include <std/new>
#include <std/stdlib.h>
#include <versatileparam/idparam.h>

namespace paf {
namespace cache {

extern const char s_empty_id[];

inline void CacheImage::ResetJobQueue()
{
	m_jobq[0] = CacheManager::GetJobQueue(CacheManager::IsHttpFile(m_path) ?
					      CacheManager::JobQueueType_Net :
					      CacheManager::JobQueueType_Local);
	if (m_load_step == LoadStep_Decode)
		m_load_step = LoadStep_ReadFile;
}

CacheImage::CacheImage(CacheManagerImpl *manager, const Option *option)
	: CacheBase(manager, option, 1), m_resize_type(image::ImageResizeType_NearestNeighbor),
	  m_direct_load(false), m_image_order(ImageOrder_NONE), m_image_order_param(0),
	  m_check_image(NULL), m_check_image_userdata(NULL), m_surface_flag(false),
	  m_load_step(LoadStep_Direct)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (option != NULL) {
		m_sub_rect = option->sub_rect;
		m_scale = option->scale;
		m_resize_type = option->resize_type;
		m_direct_load = option->direct_load;
		m_image_order = option->image_order;
		m_image_order_param = option->image_order_param;
		m_check_image = option->check_image;
		m_check_image_userdata = option->check_image_userdata;
		m_surface_flag = option->surface_flag;
		if (manager->m_image_cache_divide_thread)
			m_load_step = LoadStep_ReadFile;
		if (option->direct_load == true)
			m_load_step = LoadStep_Direct;
	}
	CacheManagerImpl::m_rmutex.Unlock();
}

/* From the manager's instance heap when it has one. */
CacheImage *CacheImage::CreateCacheInstance(CacheManagerImpl *manager, const Option *option)
{
	void *buf;

	if (manager->m_cache_instance_heap != NULL)
		buf = manager->m_cache_instance_heap->sce_paf_malloc(sizeof(CacheImage));
	else
		buf = sce_paf_malloc(sizeof(CacheImage));
	return new (buf) CacheImage(manager, option);
}

inline void CacheImage::DestroyCacheInstance(CacheImage *cache)
{
	if (cache == NULL)
		return;
	if (cache->m_instance_heap) {
		if (cache->m_manager != NULL) {
			memory::HeapAllocator *heap = cache->m_manager->m_cache_instance_heap;

			cache->~CacheImage();
			heap->sce_paf_free(cache);
		}
	} else {
		cache->~CacheImage();
		sce_paf_free(cache);
	}
}

/* The surface, once the cache is registered and its job (re)started. */
intrusive_ptr<graph::Surface> CacheImage::GetImage()
{
	thread::RMutexLock lock(&CacheManagerImpl::m_rmutex);

	if (m_manager == NULL)
		return intrusive_ptr<graph::Surface>();
	if (!m_regist_manager)
		return intrusive_ptr<graph::Surface>();
	if (RestartJob() < 0)
		return intrusive_ptr<graph::Surface>();
	Touch();
	return m_surface;
}

CacheImage::~CacheImage()
{
	m_surface.clear();
}

/* Whether option asks for another image of the same path. */
bool CacheImage::DiffOption(const CacheBase::Option *base_option)
{
	const Option *option = (const Option *)base_option;

	if (option == NULL)
		return false;
	if (((option->sub_rect.extent.w > 0 && option->sub_rect.extent.h > 0) ||
	     (m_sub_rect.extent.w > 0 && m_sub_rect.extent.h > 0)) &&
	    (option->sub_rect.origin.x != m_sub_rect.origin.x ||
	     option->sub_rect.origin.y != m_sub_rect.origin.y ||
	     option->sub_rect.extent.w != m_sub_rect.extent.w ||
	     option->sub_rect.extent.h != m_sub_rect.extent.h))
		return true;
	if (option->scale.w != m_scale.w || option->scale.h != m_scale.h ||
	    option->resize_type != m_resize_type || option->read_size != m_read_size ||
	    option->read_offset != m_read_offset || option->direct_load != m_direct_load ||
	    option->image_order != m_image_order ||
	    option->image_order_param != m_image_order_param)
		return true;
	return false;
}

/* Decodes again what a failed job left without a surface, while retries are left. */
int32_t CacheImage::RestartJob()
{
	uint32_t status = GetJobStatus();

	if (m_surface.get() != NULL || status != 2 || m_retry_max == 0 || m_retry_count == 0)
		return 0;

	int32_t ret = StartJob(0);

	if (ret >= 0)
		return 0;
	return ret;
}

void CacheImage::DestroyInstance()
{
	CacheManagerImpl::m_rmutex.Lock();
	DestroyCacheInstance(this);
	CacheManagerImpl::m_rmutex.Unlock();
}

/* The decoder frees what AllocHeap gave it, unless the manager keeps the data. */
void CacheImage::FreeHeap(void *buf, void *userdata)
{
	CacheImage *cache = (CacheImage *)userdata;

	CacheManagerImpl::m_rmutex.Lock();

	CacheManagerImpl *manager = cache->m_manager;

	if (manager == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return;
	}
	if (!manager->m_cache_longlife_heap)
		manager->FreeLongLifeHeap(buf);
	CacheManagerImpl::m_rmutex.Unlock();
}

/*
 * One step of loading: decode straight from the file, decode the data an
 * earlier step read, or read the file (CacheBase::LoadFile).
 */
int32_t CacheImage::JobRun()
{
	int32_t ret = 0;

	if (m_job->IsCanceled() == true)
		return SCE_PAF_ERROR_CACHE_CANCELED;

	CacheManagerImpl *manager = m_manager;

	if (manager == NULL)
		return SCE_PAF_ERROR_CACHE_NO_MANAGER;
	if (manager->m_is_shutdown)
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;

	if (m_load_step == LoadStep_Direct) {
		ret = OpenFile();
		if (ret < 0)
			return ret;
		if (m_job->IsCanceled() == true) {
			CloseFile();
			return SCE_PAF_ERROR_CACHE_CANCELED;
		}

		intrusive_ptr<graph::Surface> surface;
		graph::Surface::LoadOption option;

		option.decode_heap = m_manager->m_instant_heap;
		option.alloc_heap = AllocHeap;
		option.alloc_heap_userdata = this;
		option.free_heap = FreeHeap;
		option.free_heap_userdata = this;
		option.alloc_surface = AllocSurface;
		option.alloc_surface_userdata = this;
		option.check_image = CheckImage;
		option.check_image_userdata = this;
		option.max_size_kb = m_max_size_kb;
		option.cancel = IsCancel;
		option.cancel_userdata = this;
		option.sub_rect = m_sub_rect;
		option.scale = m_scale;
		option.resize_type = m_resize_type;
		option.read_size = m_read_size;
		option.read_offset = m_read_offset;
		option.direct_load = m_direct_load;
		if (m_image_order != ImageOrder_NONE) {
			option.image_order = m_image_order;
			option.image_order_param = m_image_order_param;
			surface = ScePafGraphics_AD93FF1B(m_manager->m_surface_pool, m_file, &option);
		} else {
			surface = graph::Surface::Load(m_manager->m_surface_pool, m_file, &option);
		}
		ret = option.error;
		if (surface.get() == NULL && ret == 0 && m_retry_count > 0) {
			ret = SCE_PAF_ERROR_CACHE_NO_SURFACE;
			option.error = ret;
		}
		if (ret < 0) {
			m_surface.clear();
			CloseFile();
			return ret;
		} else {
			CloseFile();
			CacheManagerImpl::m_rmutex.Lock();
			m_surface = surface;
			m_data = option.data_buff;
			m_size = option.size;
			if (!m_manager->m_cache_longlife_heap)
				m_data = NULL;
			CacheManagerImpl::m_rmutex.Unlock();
			return ret;
		}
	} else if (m_load_step == LoadStep_ReadFile) {
		if (m_data == NULL && m_size == 0)
			ret = LoadFile();
	} else {
		if (m_load_step == LoadStep_Decode) {
			intrusive_ptr<graph::Surface> surface;
			graph::Surface::LoadOption option;

			option.decode_heap = manager->m_instant_heap;
			option.alloc_surface = AllocSurface;
			option.alloc_surface_userdata = this;
			option.cancel = IsCancel;
			option.cancel_userdata = this;
			option.sub_rect = m_sub_rect;
			option.scale = m_scale;
			option.resize_type = m_resize_type;
			option.read_size = m_read_size;
			option.read_offset = m_read_offset;
			option.direct_load = m_direct_load;
			if (m_image_order != ImageOrder_NONE) {
				option.image_order = m_image_order;
				option.image_order_param = m_image_order_param;
				surface = ScePafGraphics_476AB004(manager->m_surface_pool, m_data, m_size,
								  &option);
			} else {
				surface = graph::Surface::Load(manager->m_surface_pool, m_data, m_size,
							       &option);
			}
			ret = option.error;
			if (surface.get() == NULL && ret == 0 && m_retry_count > 0) {
				ret = SCE_PAF_ERROR_CACHE_NO_SURFACE;
				option.error = ret;
			}
			if (ret < 0) {
				CacheManagerImpl::m_rmutex.Lock();
				if (!m_manager->m_cache_longlife_heap) {
					FreeHeap(m_data, this);
					m_data = NULL;
					m_size = 0;
				}
				m_surface.clear();
				CacheManagerImpl::m_rmutex.Unlock();
				return ret;
			} else {
				CacheManagerImpl::m_rmutex.Lock();
				m_surface = surface;
				if (!m_manager->m_cache_longlife_heap) {
					FreeHeap(m_data, this);
					m_data = NULL;
					m_size = 0;
				}
				CacheManagerImpl::m_rmutex.Unlock();
				return ret;
			}
		}
	}
	return ret;
}

extern "C" void ScePafMisc_925B7948(CacheImage *cache)
{
	cache->ResetJobQueue();
}

/*
 * A failed step frees the data and the surface and counts a retry (none for a
 * missing file or device, or an image too large or bad); a read is followed by
 * a decode on the decoding queue.
 */
int32_t CacheImage::JobFinish(int32_t result, bool &is_finished)
{
	int32_t ret;

	CacheManagerImpl::m_rmutex.Lock();
	if (m_manager == NULL) {
		ret = SCE_PAF_ERROR_CACHE_NO_MANAGER;
		goto out;
	}
	if (result < 0) {
		Clear();
		m_surface.clear();
	}
	m_error = result;
	if (result < 0) {
		is_finished = true;
		if (m_error == SCE_PAF_ERROR_FILE_NO_ENTRY || m_error == SCE_PAF_ERROR_FILE_NO_DEVICE ||
		    m_error == SCE_PAF_ERROR_CACHE_TOO_LARGE || m_error == SCE_PAF_ERROR_CACHE_BAD_IMAGE) {
			m_retry_count = 0;
		} else {
			int32_t count = m_retry_count;

			if (count > 0)
				m_retry_count = --count;
			if (m_retry_max != 0 && count != 0)
				ResetJobQueue();
			else
				m_error = SCE_PAF_ERROR_CACHE_LOAD_FAILED;
		}
		Touch();
	} else if (m_load_step == LoadStep_Direct || m_load_step == LoadStep_Decode) {
		is_finished = true;
		ResetJobQueue();
		Touch();
	} else if (m_load_step == LoadStep_ReadFile) {
		if (m_manager->m_is_shutdown) {
			is_finished = true;
			m_load_step = LoadStep_ReadFile;
			ret = SCE_PAF_ERROR_CACHE_SHUTDOWN;
			goto out;
		}
		is_finished = false;
		m_jobq[0] = CacheManager::GetJobQueue(CacheManager::JobQueueType_Operating);
		m_load_step = LoadStep_Decode;
	}
	ret = 0;
out:
	m_job = NULL;
	CacheManagerImpl::m_rmutex.Unlock();
	return ret;
}

/* The decoder's buffer for the file: the data already read, else from the long-life heap. */
void *CacheImage::AllocHeap(size_t size, bool *load, void *userdata)
{
	CacheImage *cache = (CacheImage *)userdata;

	CacheManagerImpl::m_rmutex.Lock();

	CacheManagerImpl *manager = cache->m_manager;

	if (manager == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return NULL;
	}

	void *buf = cache->m_data;

	if (buf != NULL && cache->m_size != 0) {
		*load = false;
		CacheManagerImpl::m_rmutex.Unlock();
		return buf;
	}
	buf = manager->AllocLongLifeHeap(size, cache->m_retry_max != 0 && cache->m_retry_count != 0,
					 cache);
	CacheManagerImpl::m_rmutex.Unlock();
	return buf;
}

/*
 * The decoder's surface for image. While the pool has no memory for it and
 * retries are left, the manager drops another image cache's surface.
 */
intrusive_ptr<graph::Surface> CacheImage::AllocSurface(graph::SurfacePool *pool,
						       const common::SharedPtr<image::Image> &image,
						       int32_t num_page, int32_t num_level,
						       void *userdata)
{
	CacheImage *cache = (CacheImage *)userdata;
	thread::RMutexLock lock(&CacheManagerImpl::m_rmutex);
	intrusive_ptr<graph::Surface> surface;

	if (cache == NULL)
		return surface;
	for (;;) {
		if (IsCancel(cache) == true)
			return intrusive_ptr<graph::Surface>();
		{
			intrusive_ptr<graph::Surface> created(
				new graph::Surface(pool, image->GetExtent().w, image->GetExtent().h,
						   image->GetMode(), image->GetOrder(), num_page,
						   num_level, cache->m_surface_flag));

			surface = created;
		}
		if (surface.get() != NULL) {
			if (surface->GetMemory() != NULL)
				return surface;
			surface.clear();
		}
		if (cache->m_retry_max == 0 || cache->m_retry_count == 0)
			return surface;

		CacheBase *found = NULL;

		if (!cache->m_manager->CheckCacheAll(CheckSurfacePool, &found))
			break;

		CacheBase *release = found;

		if (release != NULL && release->IsEnableReleaseInstance()) {
			CacheManagerImpl *manager = release->m_manager;

			if (manager != NULL)
				manager->RemoveCache(release);
		}
	}
	return surface;
}

/* The option's check of the decoded image, under the managers' lock. */
int32_t CacheImage::CheckImage(const common::SharedPtr<image::Image> &image, void *userdata)
{
	CacheImage *cache = (CacheImage *)userdata;

	CacheManagerImpl::m_rmutex.Lock();
	if (cache->m_check_image != NULL) {
		int32_t ret = cache->m_check_image(cache->m_path, image, cache->m_check_image_userdata);

		if (ret < 0) {
			CacheManagerImpl::m_rmutex.Unlock();
			return ret;
		}
	}
	CacheManagerImpl::m_rmutex.Unlock();
	return 0;
}

/* An image cache only its manager holds gives up its surface when no one else holds it. */
bool CacheImage::CheckSurfacePool(CacheBase *base)
{
	if (base == NULL)
		return false;
	if (base->m_cache_type != 1)
		return false;

	CacheImage *cache = static_cast<CacheImage *>(base);

	if (cache->GetRefCount() > 1)
		return false;

	graph::Surface *surface = cache->m_surface.get();

	if (surface == NULL)
		return true;
	if (surface->GetRefCount() > 1)
		return false;
	cache->m_surface.clear();
	return true;
}

CacheResourceImage *CacheResourceImage::CreateCacheInstance(CacheManagerImpl *manager,
							    const CacheBase::Option *option)
{
	void *buf;

	if (manager->m_cache_instance_heap != NULL)
		buf = manager->m_cache_instance_heap->sce_paf_malloc(sizeof(CacheResourceImage));
	else
		buf = sce_paf_malloc(sizeof(CacheResourceImage));
	return new (buf) CacheResourceImage(manager, option);
}

string CacheResourceImage::MakePath(const Plugin *plugin, const IDParam &id)
{
	static int32_t count;

	++count;

	string path("resource:");

	path += plugin->GetName();
	path += "/%08x";
	path = common::FormatString(path.c_str(), id.GetIDHash());
	return path;
}

CacheResourceImage::~CacheResourceImage()
{
}

static bool IsResourcePath(const string &path)
{
	return path.substr(0, 9) == "resource:";
}

/* The id hash of a "resource:" path: after its last '/' or '\\'; else "". */
string CacheResourceImage::GetIDHashString(const string &path)
{
	int32_t start = -1;

	if (IsResourcePath(path)) {
		for (int32_t pos = path.size(); pos > 0; --pos) {
			char ch = path.c_str()[pos - 1];

			if (ch != ':' && (ch == '/' || ch == '\\')) {
				start = pos;
				break;
			}
		}
	}
	if (start != -1)
		return path.substr(start, path.size() - start);
	return string("");
}

/* The plugin name of a "resource:" path: between its ':' and the last '/' or '\\'; else "". */
string CacheResourceImage::GetPluginName(const string &path)
{
	int32_t start = -1;
	int32_t end = -1;

	if (IsResourcePath(path)) {
		for (int32_t pos = path.size(); pos > 0; --pos) {
			char ch = path.c_str()[pos - 1];

			if (end == -1) {
				if (ch != ':' && (ch == '/' || ch == '\\'))
					end = pos;
			} else if (ch == ':') {
				start = pos;
				break;
			}
		}
	}
	if (start != -1)
		return path.substr(start, end - start - 1);
	return string("");
}

/* Takes the texture from the plugin's resource; the plugin must be loaded. */
int32_t CacheResourceImage::JobRun()
{
	int32_t ret = 0;

	if (m_job->IsCanceled() == true)
		return SCE_PAF_ERROR_CACHE_CANCELED;
	if (m_manager == NULL)
		return SCE_PAF_ERROR_CACHE_NO_MANAGER;
	if (m_manager->m_is_shutdown)
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	if (!IsResourcePath(m_path))
		return ret;

	string name = GetPluginName(m_path);
	Plugin *plugin = Plugin::Find(name.c_str());

	if (plugin == NULL)
		return SCE_PAF_ERROR_CACHE_PLUGIN_NOT_FOUND;

	uint32_t idhash = sce_paf_strtoul(GetIDHashString(m_path).c_str(), NULL, 16);

	CacheManagerImpl::m_rmutex.Lock();
	{
		IDParam id(s_empty_id, idhash);
		intrusive_ptr<graph::Surface> surface = plugin->GetTexture(id,
									   m_manager->m_surface_pool);

		m_surface = surface;
	}
	CacheManagerImpl::m_rmutex.Unlock();
	if (m_surface.get() == NULL || m_surface->GetMemory() == NULL)
		ret = SCE_PAF_ERROR_CACHE_NO_TEXTURE;
	m_load_step = LoadStep_Decode;
	return ret;
}

/* As CacheImage's, but a failed texture frees other surfaces before the retry. */
int32_t CacheResourceImage::JobFinish(int32_t result, bool &is_finished)
{
	int32_t ret;

	CacheManagerImpl::m_rmutex.Lock();
	if (m_manager == NULL) {
		ret = SCE_PAF_ERROR_CACHE_NO_MANAGER;
		goto out;
	}
	if (result < 0) {
		Clear();
		m_surface.clear();
	}
	m_error = result;
	if (result < 0) {
		int32_t count = m_retry_count;
		int32_t max = m_retry_max;

		is_finished = false;
		if (count > 0)
			m_retry_count = --count;
		if (max != 0 && count != 0) {
			ResetJobQueue();

			CacheBase *found = NULL;

			m_manager->CheckCacheAll(CheckSurfacePool, &found);
			if (found != NULL && found->IsEnableReleaseInstance()) {
				CacheManagerImpl *manager = found->m_manager;

				if (manager != NULL)
					manager->RemoveCache(found);
			}
		} else {
			m_error = SCE_PAF_ERROR_CACHE_LOAD_FAILED;
		}
		Touch();
	} else if (m_load_step == LoadStep_Decode) {
		is_finished = true;
		ResetJobQueue();
		Touch();
	}
	ret = 0;
out:
	m_job = NULL;
	CacheManagerImpl::m_rmutex.Unlock();
	return ret;
}

const char s_empty_id[] = "";

}
}
