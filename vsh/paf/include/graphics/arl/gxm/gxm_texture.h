/*
 * paf::graph::arl's textures on GXM: a texture object holds its surface storage, the memory
 * a VramAllocator gave it laid out in mip levels, and the SceGxmTexture made over it. Class
 * names from the mangled export names; the file's name, the storage's classes and every
 * member name ours.
 */
#ifndef PAF_GRAPHICS_ARL_GXM_GXM_TEXTURE_H
#define PAF_GRAPHICS_ARL_GXM_GXM_TEXTURE_H

#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <common/shared_ptr.h>
#include <std/string>
#include <graphics/arl/arl_defines.h>
#include <graphics/graphmem/graphmem.h>
#include <image/image_types.h>

namespace paf {
namespace graph {
namespace arl {

class SurfaceMemory;

/* the bytes of a pixel of each format a surface takes */
inline uint32_t BytesPerPixel(ImageMode format)
{
	switch (format) {
	case ImageMode_ABGR8888:
	case ImageMode_RGBA8888:
	case ImageMode_ARGB8888:
	case ImageMode_BGRA8888:
		return 4;
	case ImageMode_A8:
	case ImageMode_L8:
		return 1;
	case ImageMode_depth16:
		return 2;
	case ImageMode_depth32:
		return 4;
	case ImageMode_WZYX16FP:
		return 8;
	case ImageMode_WZYX32FP:
		return 16;
	case ImageMode_A16FP:
		return 2;
	case ImageMode_A32FP:
		return 4;
	default:
		return 0;
	}
}

/* gives surfaces their video memory from one of GraphMem's heaps, counting what it gave
   (Sony's class name; its members' names ours) */
class VramAllocator {
public:
	VramAllocator(const char *name,
			  const common::WeakPtr<GraphMem::GraphHeapAllocator> &heap)
		: m_used(0), m_name(name), m_heap(heap)
	{
	}

	virtual ~VramAllocator()
	{
	}

	virtual inline void Alloc(SurfaceMemory *memory, uint32_t size);

	virtual inline void Free(SurfaceMemory *memory);

	uint32_t m_used;
	string m_name;
	common::WeakPtr<GraphMem::GraphHeapAllocator> m_heap;
};

/* a block of video memory, given back to its allocator when the last owner lets it go */
class SurfaceMemory {
public:
	SurfaceMemory(const common::SharedPtr<VramAllocator> &allocator)
		: m_addr(NULL), m_alloc_size(0), m_size(0), m_allocator(allocator)
	{
	}

	~SurfaceMemory()
	{
		m_allocator->Free(this);
		m_size = 0;
	}

	void *m_addr;
	/* what the allocator gave, which it takes back on Free */
	uint32_t m_alloc_size;
	uint32_t m_size;
	common::SharedPtr<VramAllocator> m_allocator;
};

inline void VramAllocator::Alloc(SurfaceMemory *memory, uint32_t size)
{
	common::SharedPtr<GraphMem::GraphHeapAllocator> heap(m_heap);

	if (heap.get() != NULL) {
		GraphMem::GraphHeapAllocator *allocator =
			common::SharedPtr<GraphMem::GraphHeapAllocator>(m_heap).get();

		memory->m_addr = allocator->sce_paf_malloc(size);
	}
	memory->m_alloc_size = size;
	m_used += size;
}

inline void VramAllocator::Free(SurfaceMemory *memory)
{
	if (memory->m_addr != NULL) {
		m_used -= memory->m_alloc_size;

		common::SharedPtr<GraphMem::GraphHeapAllocator> heap(m_heap);

		if (heap.get() != NULL) {
			GraphMem::GraphHeapAllocator *allocator =
				common::SharedPtr<GraphMem::GraphHeapAllocator>(m_heap).get();

			allocator->sce_paf_free(memory->m_addr);
		}
		memory->m_addr = NULL;
		memory->m_alloc_size = 0;
	}
}

/* a surface's memory and how its mip levels are laid out in it */
class SurfaceStorage {
public:
	enum {
		MAX_LEVELS = 16
	};

	class Level {
	public:
		Level() : m_width(0), m_height(0), m_offset(0), m_size(0)
		{
		}

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_offset;
		uint32_t m_size;
	};

	SurfaceStorage(common::SharedPtr<VramAllocator> allocator);
	~SurfaceStorage();

	void Alloc(ImageMode format, uint32_t width, uint32_t height, SurfaceFlags flags,
		   uint32_t levels);
	void InitColorSurface(SceGxmColorSurface *surface, ImageMode format,
			      const SurfaceFlags &flags, uint32_t level);
	void InitDepthStencilSurface(SceGxmDepthStencilSurface *surface, ImageMode format,
				     const SurfaceFlags &flags);
	int InitTexture(SceGxmTexture *texture, ImageMode format, const SurfaceFlags &flags);

	uint32_t m_width;
	uint32_t m_pitch;
	uint32_t m_height;
	SurfaceFlags m_flags;
	uint32_t m_levels;
	common::SharedPtr<SurfaceMemory> m_memory;
	Level m_level[MAX_LEVELS];
};

/* a pointer that deletes what it holds when it owns it */
template<typename T>
class OwnedPtr {
public:
	OwnedPtr() : m_ptr(NULL), m_owned(true)
	{
	}

	~OwnedPtr()
	{
		reset(NULL);
	}

	void reset(T *ptr)
	{
		if (m_ptr != NULL && m_owned)
			delete m_ptr;
		m_ptr = ptr;
		m_owned = true;
	}

	T *get() const
	{
		return m_ptr;
	}

	T *m_ptr;
	bool m_owned;
};

/* what a texture's data pointer holds, which no converted code reads yet (name ours) */
class TextureData;

/* a texture's size, format and mip level count, which a copy takes whole */
class TextureDesc {
public:
	TextureDesc() : m_width(0), m_height(0), m_format(ImageMode_NONE), m_levels(0)
	{
	}

	TextureDesc(int32_t width, int32_t height, ImageMode format, uint32_t levels)
		: m_width(width), m_height(height), m_format(format), m_levels(levels)
	{
	}

	int32_t m_width;
	int32_t m_height;
	ImageMode m_format;
	uint32_t m_levels;
};

class TextureObject {
public:
	enum {
		MAX_LEVELS = 12
	};

	TextureObject(common::SharedPtr<VramAllocator> allocator, int32_t type);
	virtual ~TextureObject();
	virtual void OnUpdate();

	int setLodBias(float bias);
	int setMinFilter(SceGxmTextureFilter filter, bool mipmap);
	int setMagFilter(SceGxmTextureFilter filter);
	void Unused2();
	int setWrapS(TextureWrap wrap);
	int setWrapT(TextureWrap wrap);
	bool alloc(TextureDesc desc, int32_t depth, int32_t samples, SurfaceFlags flags);

	/* makes the colour surface over the level the first time a scene draws into it */
	void PrepareColorSurface(uint32_t level)
	{
		OwnedPtr<SceGxmColorSurface> &surface = m_color_surfaces[level];

		if (surface.get() == NULL) {
			SurfaceFlags flags = m_storage->m_flags;

			if (m_storage->m_flags.IsSet(TEXTURE_LINEAR))
				flags.Set(COLOR_SURFACE_LINEAR);
			if (m_storage->m_flags.IsSet(TEXTURE_TILED))
				flags.Set(COLOR_SURFACE_TILED);
			if (m_storage->m_flags.IsSet(TEXTURE_SWIZZLED))
				flags.Set(COLOR_SURFACE_SWIZZLED);

			common::SharedPtr<SurfaceStorage> storage = m_storage;
			ImageMode format = m_desc.m_format;

			surface.reset(new SceGxmColorSurface());
			storage->InitColorSurface(surface.get(), format, flags, level);
		}
	}

	/* makes the depth and stencil surface over the texture the first time a scene uses it */
	void PrepareDepthStencilSurface()
	{
		if (m_depth_stencil_surface.get() == NULL) {
			SurfaceFlags flags = m_storage->m_flags;

			if (m_storage->m_flags.IsSet(TEXTURE_LINEAR))
				flags.Set(STENCILDEPTH_SURFACE_LINEAR);
			if (m_storage->m_flags.IsSet(TEXTURE_TILED))
				flags.Set(STENCILDEPTH_SURFACE_TILED);

			common::SharedPtr<SurfaceStorage> storage = m_storage;
			ImageMode format = m_desc.m_format;

			m_depth_stencil_surface.reset(new SceGxmDepthStencilSurface());
			storage->InitDepthStencilSurface(m_depth_stencil_surface.get(), format, flags);
		}
	}

	common::SharedPtr<SurfaceStorage> m_storage;
	common::SharedPtr<TextureData> m_data;
	OwnedPtr<SceGxmColorSurface> m_color_surfaces[MAX_LEVELS];
	OwnedPtr<SceGxmDepthStencilSurface> m_depth_stencil_surface;
	OwnedPtr<SceGxmTexture> m_texture;
	int32_t m_type;
	TextureDesc m_desc;
};

class Texture2D : public TextureObject {
public:
	Texture2D(common::SharedPtr<VramAllocator> allocator);
	virtual ~Texture2D();

	/* sets up a surface like another texture's */
	bool init(const common::SharedPtr<TextureObject> &other);
};

/* a 2D texture of one format over the memory an allocator gives, filled from data when given */
template<ImageMode FORMAT>
common::SharedPtr<Texture2D> CreateTexture(common::SharedPtr<VramAllocator> allocator,
					   SurfaceSize size, const void *data, SurfaceFlags flags,
					   uint32_t levels);

/* textures made from nothing: noise, one colour, a checkerboard (names ours) */
common::SharedPtr<Texture2D> CreateNoiseTexture(common::SharedPtr<VramAllocator> allocator,
						SurfaceSize size, SurfaceFlags flags);
common::SharedPtr<Texture2D> CreateSolidTexture(common::SharedPtr<VramAllocator> allocator,
						uint32_t color, SurfaceSize size,
						SurfaceFlags flags);
common::SharedPtr<Texture2D> CreateCheckerTexture(common::SharedPtr<VramAllocator> allocator,
						  SurfaceSize size, SurfaceSize cell,
						  SurfaceFlags flags);

}
}
}

extern "C" {
/* requests the texture's mip levels be generated: nothing to do on GXM */
void ScePafGraphics_FF97623F(paf::graph::arl::TextureObject *texture, float quality);
/* sets up a 2D texture's surface for the size, format and levels given */
bool ScePafGraphics_C4BD3558(paf::graph::arl::Texture2D *texture, int32_t width,
			     int32_t height, ImageMode format, uint32_t levels,
			     paf::graph::arl::SurfaceFlags flags);
/* copies an image into a mip level of a texture whose surface is set up */
int ScePafGraphics_309D2902(paf::graph::arl::Texture2D *texture, int32_t level, int32_t width,
			    int32_t height, ImageMode format, uint32_t levels, const void *data);
/* loads an image file into a 2D texture, its levels from the image's array */
bool ScePafGraphics_7C53C58A(paf::common::SharedPtr<paf::graph::arl::Texture2D> texture,
			     const paf::string &filename, paf::graph::arl::SurfaceFlags flags);
/* a square texture of one channel, its value the distance from the centre (1 at the edges) */
paf::common::SharedPtr<paf::graph::arl::Texture2D>
ScePafGraphics_27C5476A(paf::common::SharedPtr<paf::graph::arl::VramAllocator> allocator,
			uint32_t size, paf::graph::arl::SurfaceFlags flags, bool mipmap,
			bool generate_mipmaps);
}

namespace paf {
namespace graph {
namespace arl {

template<ImageMode FORMAT>
__attribute__((noinline)) common::SharedPtr<Texture2D>
CreateTexture(common::SharedPtr<VramAllocator> allocator, SurfaceSize size, const void *data,
	      SurfaceFlags flags, uint32_t levels)
{
	common::SharedPtr<Texture2D> texture(new Texture2D(allocator));

	ScePafGraphics_C4BD3558(texture.get(), size.m_width, size.m_height, FORMAT, levels, flags);
	if (data != NULL)
		ScePafGraphics_309D2902(texture.get(), 0, size.m_width, size.m_height, FORMAT, levels,
					data);
	return texture;
}

}
}
}

#endif
