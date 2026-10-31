/*
 * paf::cache::CacheImage: a cached image, decoded into a graph::Surface of the
 * manager's surface pool. The image is either decoded straight from its file
 * (direct load) or read into the long-life heap by CacheBase first and decoded
 * from there by a second job on the decoding queue; a surface the pool cannot
 * hold frees other caches' surfaces and retries.
 *
 * CacheResourceImage (a name of ours) decodes a texture of a plugin's
 * resource instead, named "resource:<plugin>/<id hash>".
 */
#ifndef PAF_CACHE_CACHE_IMAGE_H
#define PAF_CACHE_CACHE_IMAGE_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <std/memory>
#include <common/shared_ptr.h>
#include <image/image.h>
#include <graphics/surface/surface.h>
#include <cache/cache_base.h>
#include <cache/cache_manager.h>

namespace paf {

class Plugin;
class IDParam;

namespace cache {

/*
 * What a cached image offers its users (a name of ours: no record or export
 * names the interface). CacheImage implements it beside CacheBase.
 */
class ImageCacheInterface {
public:
	virtual ~ImageCacheInterface()
	{
	}

	virtual intrusive_ptr<graph::Surface> GetImage() = 0;
	virtual int32_t GetError() = 0;
	virtual uint32_t GetJobStatus() = 0;
	virtual void CancelJob() = 0;
	virtual int32_t AddRef() = 0;
	virtual int32_t Release() = 0;
	virtual int32_t GetRefCount() = 0;

	virtual bool IsRetryEnabled()
	{
		return false;
	}
};

class CacheImage : public CacheBase, public ImageCacheInterface {
public:
	/* Checks a decoded image before its surface is made; path is the cache's. */
	typedef int32_t (*CheckImageFunc)(const string &path,
					  const common::SharedPtr<image::Image> &image,
					  void *userdata);

	class Option : public CacheBase::Option {
	public:
		image::ImageRect sub_rect;
		image::ImageExtent scale;
		image::ImageResizeType resize_type;
		bool direct_load;
		ImageOrder image_order;
		int32_t image_order_param;
		CheckImageFunc check_image;
		void *check_image_userdata;
		bool surface_flag;
	};

	/* What JobRun does next. */
	enum LoadStep {
		LoadStep_ReadFile = 0,	/* CacheBase::LoadFile into the long-life heap */
		LoadStep_Decode = 1,	/* decode what LoadFile read */
		LoadStep_Direct = 3	/* decode straight from the file */
	};

	static CacheImage *CreateCacheInstance(CacheManagerImpl *manager, const Option *option);
	__declspec(dllexport) static void DestroyCacheInstance(CacheImage *cache);

	CacheImage(CacheManagerImpl *manager, const Option *option);
	virtual ~CacheImage();

	virtual uint32_t GetJobStatus()
	{
		return m_job_state;
	}

	virtual void DestroyInstance();
	virtual bool DiffOption(const CacheBase::Option *option);
	virtual int32_t RestartJob();
	virtual int32_t JobRun();
	virtual int32_t JobFinish(int32_t result, bool &is_finished);

	virtual intrusive_ptr<graph::Surface> GetImage();

	virtual int32_t GetError()
	{
		return m_error;
	}

	virtual void CancelJob()
	{
		CacheBase::CancelJob();
	}

	virtual int32_t AddRef()
	{
		return CacheBase::AddRef();
	}

	virtual int32_t Release()
	{
		return CacheBase::Release();
	}

	virtual int32_t GetRefCount()
	{
		return m_ref_count;
	}

	virtual bool IsRetryEnabled()
	{
		return m_retry_max != 0 && m_retry_count != 0;
	}

	static void *AllocHeap(size_t size, bool *load, void *userdata);
	static void FreeHeap(void *buf, void *userdata);
	static intrusive_ptr<graph::Surface> AllocSurface(graph::SurfacePool *pool,
							  const common::SharedPtr<image::Image> &image,
							  int32_t num_page, int32_t num_level,
							  void *userdata);
	static int32_t CheckImage(const common::SharedPtr<image::Image> &image, void *userdata);
	static bool CheckSurfacePool(CacheBase *cache);

	/* The queue for the next job: by the path, a decode after a read goes back to a read. */
	void ResetJobQueue()
	{
		m_jobq[0] = CacheManager::GetJobQueue(CacheManager::IsHttpFile(m_path) ?
						      CacheManager::JobQueueType_Net :
						      CacheManager::JobQueueType_Local);
		if (m_load_step == LoadStep_Decode)
			m_load_step = LoadStep_ReadFile;
	}

protected:
	image::ImageRect m_sub_rect;
	image::ImageExtent m_scale;
	image::ImageResizeType m_resize_type;
	bool m_direct_load;
	ImageOrder m_image_order;
	int32_t m_image_order_param;
	CheckImageFunc m_check_image;
	void *m_check_image_userdata;
	bool m_surface_flag;
	intrusive_ptr<graph::Surface> m_surface;
	uint8_t m_load_step;
};

class CacheResourceImage : public CacheImage {
public:
	static CacheResourceImage *CreateCacheInstance(CacheManagerImpl *manager,
						       const CacheBase::Option *option);

	/* "resource:<plugin name>/<id hash in hex>" */
	static string MakePath(const Plugin *plugin, const IDParam &id);

	CacheResourceImage(CacheManagerImpl *manager, const CacheBase::Option *option)
		: CacheImage(manager, NULL)
	{
		CacheBase(manager, option, 0);
	}

	virtual ~CacheResourceImage();

	virtual int32_t JobRun();
	virtual int32_t JobFinish(int32_t result, bool &is_finished);

private:
	static string GetIDHashString(const string &path);
	static string GetPluginName(const string &path);
};

}
}

#endif
