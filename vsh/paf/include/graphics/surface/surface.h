/*
 * paf::graph::Surface: a texture of one or more pages, its memory taken from a
 * SurfacePool, its reference count kept by the surface itself. Laid out from
 * Surface::Initialize, InitObj and the base constructor (0x94 bytes); fields no
 * converted code reads yet are named when paf::graph is converted.
 */
#ifndef PAF_GRAPHICS_SURFACE_SURFACE_H
#define PAF_GRAPHICS_SURFACE_SURFACE_H

#include <stddef.h>
#include <stdint.h>
#include <std/memory>
#include <common/shared_ptr.h>
#include <image/image.h>

namespace paf {

class File;

namespace memory {
class HeapAllocator;
}

namespace graph {

class SurfacePool;
class SurfaceCLUT;

class SurfaceBase {
public:
	SurfaceBase(ImageMode format, bool reserved);
	virtual ~SurfaceBase();

	ImageMode m_format;
	bool m_reserved;
	int32_t m_pixel_bits;
};

class SurfaceListNode {
public:
	SurfaceListNode *m_prev;
	SurfaceListNode *m_next;
};

class Surface : public SurfaceBase, protected SurfaceListNode {
public:
	enum SurfaceType {
		TypeTexture = 0
	};

	enum MemClass {
		Fixed = 0,
		Permanent = 1,
		Unallocated = 2
	};

	class Texture;

	/* How Load decodes an image into a surface; the callbacks get their userdata. */
	class LoadOption {
	public:
		typedef void *(*AllocHeapFunc)(size_t size, bool *load, void *userdata);
		typedef void (*FreeHeapFunc)(void *buf, void *userdata);
		typedef intrusive_ptr<Surface> (*AllocSurfaceFunc)(SurfacePool *pool,
								   const common::SharedPtr<image::Image> &image,
								   int32_t num_page, int32_t num_level,
								   void *userdata);
		typedef int32_t (*CheckImageFunc)(const common::SharedPtr<image::Image> &image,
						  void *userdata);
		typedef bool (*CancelFunc)(void *userdata);

		LoadOption()
			: decode_heap(NULL), data_load_heap(NULL), reserved0(0), error(0),
			  data_buff(NULL), size(0), reserved1(0), alloc_heap(NULL),
			  alloc_heap_userdata(NULL), free_heap(NULL), free_heap_userdata(NULL),
			  alloc_surface(NULL), alloc_surface_userdata(NULL), check_image(NULL),
			  check_image_userdata(NULL), max_size_kb(0), cancel(NULL),
			  cancel_userdata(NULL), reserved2(true),
			  resize_type(image::ImageResizeType_NearestNeighbor), read_size(0),
			  read_offset(0), direct_load(false), rotate_type(0), flip_type(0),
			  image_order(ImageOrder_NONE), image_order_param(0), reserved3(0), reserved4(0),
			  reserved5(false)
		{
		}

		memory::HeapAllocator *decode_heap;
		memory::HeapAllocator *data_load_heap;
		int32_t reserved0;
		int32_t error;
		void *data_buff;
		int64_t size;
		int32_t reserved1;
		AllocHeapFunc alloc_heap;
		void *alloc_heap_userdata;
		FreeHeapFunc free_heap;
		void *free_heap_userdata;
		AllocSurfaceFunc alloc_surface;
		void *alloc_surface_userdata;
		CheckImageFunc check_image;
		void *check_image_userdata;
		uint16_t max_size_kb;
		CancelFunc cancel;
		void *cancel_userdata;
		image::ImageRect sub_rect;
		image::ImageExtent scale;
		bool reserved2;
		image::ImageResizeType resize_type;
		size_t read_size;
		size_t read_offset;
		bool direct_load;
		int32_t rotate_type;
		int32_t flip_type;
		ImageOrder image_order;
		int32_t image_order_param;
		int32_t reserved3;
		int32_t reserved4;
		bool reserved5;
	};

	Surface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
		ImageOrder order, int32_t num_page, int32_t num_level, bool reserved);
	virtual ~Surface();

	static void *operator new(size_t size);
	static void operator delete(void *ptr);

	static intrusive_ptr<Surface> Load(SurfacePool *pool, common::SharedPtr<File> fp,
					   LoadOption *option);
	static intrusive_ptr<Surface> Load(SurfacePool *pool, void *buf, int64_t size,
					   LoadOption *option);

	/* SCE_PAF_ERROR_GRAPHICS_NO_SPACE while pool cannot hold a width x height surface. */
	static int32_t IsAllocatableSurfacePool(SurfacePool *pool, uint32_t width, uint32_t height);

	int32_t AddRef();
	int32_t Release();

	int32_t GetRefCount() const
	{
		return m_ref_count;
	}

	/* The pages' memory from the pool; NULL when the pool had none. */
	void *GetMemory() const
	{
		return m_mem.bp;
	}

protected:
	class TexParam {
	public:
		uint32_t mag_filter;
		uint32_t min_filter;
		uint32_t mip_filter;
		uint32_t wrap_s;
		uint32_t wrap_t;
		uint32_t lod_bias;
	};

	class SysMemory {
	public:
		void *bp;
		int16_t align;
		int16_t attr;
	};

	int32_t m_ref_count;
	int16_t m_width;
	int16_t m_height;
	ImageOrder m_order;
	SurfaceType m_surf_type;
	MemClass m_mem_class;
	int16_t m_tex_width;
	int16_t m_tex_height;
	float m_factor_u;
	float m_factor_v;
	int32_t m_page_size;
	int32_t m_stride;
	uint8_t m_locked : 1;
	uint8_t m_texturized : 1;
	uint8_t m_symmetry_u : 1;
	uint8_t m_symmetry_v : 1;
	uint8_t m_reserved_flag : 1;
	Texture *m_tex_pages;
	TexParam m_texture_param;
	bool m_texture_param_changed;
	int16_t m_num_level;
	int16_t m_num_page;
	int16_t m_page_counter;
	int16_t m_reserved0;
	SysMemory m_mem;
	int32_t m_score;
	intrusive_ptr<Surface> m_depth;
	intrusive_ptr<SurfaceCLUT> m_clut;
	SurfacePool *m_pool;
	int16_t m_reserved1;
	uint8_t m_reserved2;
	uint8_t m_reserved3;
	int32_t m_reserved4;
	int16_t m_reserved5;
	int16_t m_reserved6;
	int16_t m_reserved7;
	int16_t m_reserved8;
};

}
}

/* A surface pool export Sony's exports do not name yet: the pool's virtual 7 with flag. */
extern "C" int ScePafGraphics_83ABDFF5(paf::graph::SurfacePool *pool, uint8_t flag);

/* Surface loads Sony's exports do not name yet: the file and memory Load for an image_order. */
extern "C" paf::intrusive_ptr<paf::graph::Surface>
ScePafGraphics_AD93FF1B(paf::graph::SurfacePool *pool, paf::common::SharedPtr<paf::File> fp,
			paf::graph::Surface::LoadOption *option);
extern "C" paf::intrusive_ptr<paf::graph::Surface>
ScePafGraphics_476AB004(paf::graph::SurfacePool *pool, void *buf, int64_t size,
			paf::graph::Surface::LoadOption *option);

#endif
