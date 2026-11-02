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
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <thread/thread.h>

struct SceGxmTexture;

namespace paf {

class File;
class Plugin;

namespace memory {
class HeapAllocator;
}

namespace graph {

class Surface;
class SurfacePool;
class SurfaceCLUT;

}
}

/* the division's ratios across and down, set (clamped to 0..1) and read (Sony's names not
   found) */
extern "C" {
int32_t ScePafGraphics_36960B24(paf::graph::Surface *surface, paf::math::v2 ratio_h,
				paf::math::v2 ratio_v);
void ScePafGraphics_AFAE7807(paf::graph::Surface *surface, paf::math::v2 *ratio_h,
			     paf::math::v2 *ratio_v);
}

namespace paf {
namespace graph {

class SurfaceBase {
public:
	SurfaceBase(ImageMode format, bool reserved);
	virtual ~SurfaceBase();

	/* log2 of a format's bits per pixel (name ours) */
	static int32_t GetPixelBits(ImageMode format);

	int32_t PixelsToBytes(int32_t pixels) const;

	ImageMode m_format;
	bool m_reserved;
	/* log2 of the bits per pixel */
	int32_t m_pixel_bits;
};

class SurfaceListNode {
public:
	/* alone, in no list */
	SurfaceListNode()
	{
		m_prev = this;
		m_next = this;
	}

	/* out of its list (names ours) */
	void Unlink()
	{
		m_prev->m_next = m_next;
		m_next->m_prev = m_prev;
	}

	/* into a list after prev */
	void InsertAfter(SurfaceListNode *prev)
	{
		m_prev = prev;

		SurfaceListNode *next = prev->m_next;

		m_next = next;
		next->m_prev = this;
		prev->m_next = this;
	}

	SurfaceListNode *m_prev;
	SurfaceListNode *m_next;
};

/* a colour table the indexed formats look up, in pool memory or the caller's */
class SurfaceCLUT : public SurfaceBase, protected SurfaceListNode {
public:
	static void DeactivateSurfacePool(SurfacePool *pool);

	SurfaceCLUT(SurfacePool *pool, ImageMode format, int32_t num_entry);
	SurfaceCLUT(ImageMode format, void *clut, int32_t num_entry);
	virtual ~SurfaceCLUT();

	int32_t ActivateCLUT();
	int32_t AddRef();
	int32_t Release();
	int32_t GetCLUTSize() const;

	void *GetCLUT() const
	{
		return m_clut;
	}

	static void *operator new(size_t size);
	static void operator delete(void *ptr);

	static SurfaceListNode m_clut_list;
	static thread::Mutex m_clut_mutex;

	int32_t m_ref_count;
	int16_t m_num_entry;
	int16_t m_clut_size;
	SurfacePool *m_pool;
	void *m_clut;
};

/* how a texture is filtered and addressed (names ScePaf-RE's) */
enum TextureFilter {
	NEAREST = 0,
	LINEAR = 1,
	NEAREST_MIPMAP_NEAREST = 2,
	LINEAR_MIPMAP_NEAREST = 3,
	NEAREST_MIPMAP_LINEAR = 4,
	LINEAR_MIPMAP_LINEAR = 5
};

enum TextureWrap {
	CLAMP = 0,
	REPEAT = 1,
	CLAMP_TO_EDGE = 2,
	MIRRORED_REPEAT = 3,
	MIRROR = 4,
	MIRROR_CLAMP = 5,
	REPEAT_IGNORE_BORDER = 6,
	CLAMP_IGNORE_BORDER = 7,
	CLAMP_FULL_BORDER = 8,
	CLAMP_HALF_BORDER = 9
};

/* which page GetPageIndex gives: the one shown, or the one drawn into (names ours) */
enum FrameBufferType {
	FrameBufferType_Front = 0,
	FrameBufferType_Back = 1
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

	/* a page's GXM texture, owned unless the page's memory is the caller's */
	class Texture {
	public:
		Texture();
		~Texture();

		SceGxmTexture *texture;
		/* the texture is deleted with the page (name ours) */
		bool m_owned;
	};

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
			: decode_heap(NULL), data_load_heap(NULL), align_bytes(0), error(0),
			  data_buff(NULL), size(0), clut_pool(NULL), alloc_heap(NULL),
			  alloc_heap_userdata(NULL), free_heap(NULL), free_heap_userdata(NULL),
			  alloc_surface(NULL), alloc_surface_userdata(NULL), check_image(NULL),
			  check_image_userdata(NULL), max_size_kb(0), cancel(NULL),
			  cancel_userdata(NULL), keep_aspect(true),
			  resize_type(image::ImageResizeType_NearestNeighbor), read_size(0),
			  read_offset(0), direct_load(false), rotate_type(0), flip_type(0),
			  image_order(ImageOrder_NONE), image_order_param(0), extlines(0), surface_order(0),
			  reserved5(false)
		{
		}

		memory::HeapAllocator *decode_heap;
		memory::HeapAllocator *data_load_heap;
		/* the pages' alignment (name ours) */
		int32_t align_bytes;
		int32_t error;
		void *data_buff;
		int64_t size;
		/* the pool of an indexed image's colour table, else the surface's (name ours) */
		SurfacePool *clut_pool;
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
		/* scale shrunk to the image's aspect (name ours) */
		bool keep_aspect;
		image::ImageResizeType resize_type;
		size_t read_size;
		size_t read_offset;
		bool direct_load;
		int32_t rotate_type;
		int32_t flip_type;
		ImageOrder image_order;
		int32_t image_order_param;
		/* the edge lines CopyWithEdge adds (name ours) */
		uint32_t extlines;
		/* ImageOrder_Tiled: a linear image tiled in the surface (name ours) */
		int32_t surface_order;
		bool reserved5;
	};

	Surface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
		ImageOrder order, int32_t num_page, int32_t num_level, bool reserved);
	Surface(int32_t width, int32_t height, ImageMode format, ImageOrder order, int32_t num_page,
		void *bp, int32_t line_bytes, int32_t num_level, bool reserved);
	Surface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
		ImageOrder order, int32_t num_page, int32_t align_bytes, int32_t line_bytes,
		int32_t num_level, bool reserved);
	Surface(int32_t width, int32_t height, ImageMode format, ImageOrder order,
		SurfaceType surf_type, int32_t num_page, void *bp, int32_t line_bytes,
		int32_t num_level, bool reserved);
	Surface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
		ImageOrder order, SurfaceType surf_type, int32_t num_page, int32_t num_level,
		bool reserved);
	/* a one-page surface over a GXM texture someone else owns (Canvas's) */
	Surface(SceGxmTexture *texture, int32_t width, int32_t height);
	virtual ~Surface();
	/* told of each draw into the surface, with the front end's third debug cap: nothing */
	virtual void SetDirty(bool dirty) {}
	/* whether the page drawn into has been drawn to: a plain surface never says so */
	virtual bool IsDirty() { return false; }
	/* clears the page drawn into */
	virtual void Clear(const math::v4 &color, uint32_t clear_mask);
	virtual void Swap();
	virtual int32_t BeginScene();
	virtual int32_t EndScene();
	/* the page's memory for the CPU, and back */
	virtual void *Lock(int32_t page);
	virtual void Unlock();
	virtual int32_t Resize(int32_t width, int32_t height);
	virtual int32_t GenTexture();

	static void *operator new(size_t size);
	static void operator delete(void *ptr);

	static intrusive_ptr<Surface> Load(SurfacePool *pool, common::SharedPtr<File> fp,
					   LoadOption *option);
	static intrusive_ptr<Surface> Load(SurfacePool *pool, void *buf, int64_t size,
					   LoadOption *option);

	/* SCE_PAF_ERROR_GRAPHICS_NO_SPACE while pool cannot hold a width x height surface. */
	/* SCE_PAF_ERROR_GRAPHICS_NO_SPACE while pool cannot give size bytes aligned to align */
	static int32_t IsAllocatableSurfacePool(SurfacePool *pool, uint32_t size, uint32_t align);
	/* the surfaces in pool forget it and their memory from it */
	static void DeactivateSurfacePool(SurfacePool *pool);
	/* value rounded down to a power of two; 1 for none above 0 */
	static int32_t FloorLog2(int32_t value);

	static int32_t DecodeImage(SurfacePool *pool, LoadOption *option, intrusive_ptr<Surface> &surf,
				   common::SharedPtr<image::Image> image);
	static int32_t DecodeImage(SurfacePool *pool, LoadOption *option, intrusive_ptr<Surface> &surf,
				   void *buf, size_t size);
	static int32_t SetDecodeImageOption(SurfacePool *pool, LoadOption *option,
					    intrusive_ptr<Surface> &surf,
					    common::SharedPtr<image::Image> image);
	static int16_t GetAdjustHeight(SurfacePool *pool, int32_t height);
	static int16_t GetAdjustWidth(SurfacePool *pool, int32_t width);
	static uint32_t GetN(uint32_t value);

	int32_t AddRef();
	int32_t Release();

	int32_t AttachCLUT(const intrusive_ptr<SurfaceCLUT> &clut);
	void DetachCLUT();
	int32_t FillPadding(int32_t page, const void *color, int32_t level);
	int32_t Copy(int32_t page, const void *src, ImageOrder src_order, int32_t line_bytes,
		     int32_t level);
	int32_t Copy_(int32_t page, const void *src, ImageOrder src_order, int32_t line_bytes,
		      int32_t level);
	int32_t Copy_YUV420PX(int32_t page, const void *src, ImageOrder src_order,
			      int32_t line_bytes, int32_t level);
	void GenTextureAll();
	int32_t GenTexture(Texture *tex, void *addr);
	int32_t SetPalette(Texture *tex, const void *palette);
	int32_t Initialize(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
			   ImageOrder order, SurfaceType surf_type, int32_t num_page, int32_t num_level);
	int32_t Initialize(int32_t width, int32_t height, ImageMode format, ImageOrder order,
			   SurfaceType surf_type, int32_t num_page, void *bp, int32_t line_bytes,
			   int32_t num_level);
	int32_t Initialize(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
			   ImageOrder order, SurfaceType surf_type, int32_t num_page,
			   int32_t align_bytes, int32_t size_byte, int32_t line_bytes,
			   int32_t num_level);
	void InitObj(int32_t line_bytes);
	/* every member back to nothing before Initialize (name ours) */
	void InitMember();
	void SetMemParam(int32_t line_bytes, int16_t adj_width, int16_t adj_height);
	void SetMemAttr();
	int32_t CreateMipmap();
	int32_t GetCopyHeight(int32_t pixel_align, int32_t level);
	int32_t GetStride(int32_t level) const;
	int32_t GetScore() const;
	uint8_t *GetPagePtr(int32_t page) const;
	uint8_t *GetPagePtr(int32_t page, int32_t level) const;
	void FastMemcpy(void *dst, const void *src, int32_t n);
	void ConvertTiledFormat(void *dst, const void *src, int32_t width, int32_t height);
	int32_t ConvertSwizzleFormat(void *dst, const void *src, int32_t width, int32_t height);

	static SurfaceListNode m_active_mark;
	static int32_t m_initialized;
	static SurfaceListNode m_surface_list;
	static thread::RMutex m_list_cs;
	static thread::Mutex m_lock_mutex;

	/* the surface made a GXM texture, mapped as given */
	int32_t Texturize(uint32_t map_type, float offset_u, float offset_v, float scale_u,
			  float scale_v);

	int32_t Texturize()
	{
		return Texturize(0, 0.0f, 0.0f, 1.0f, 1.0f);
	}

	int32_t ActivateTexture(uint32_t map_type, float offset_u, float offset_v, float scale_u,
				float scale_v);
	int32_t DeleteTexture();
	int32_t ActivatePage(int32_t page);
	int32_t Copy(int32_t page, const intrusive_ptr<Surface> &src, int32_t src_page);
	void FreeMemory();
	/* nothing */
	void DelayedRelease();
	/* the surface moved in the list ahead of the first with no higher score */
	void Reorder();
	uint32_t GetLodBias();
	int32_t SetLodBias(uint32_t lod_bias);
	/* the old setting */
	bool SetSymmetryU(bool symmetry);
	bool SetSymmetryV(bool symmetry);

	SceGxmTexture *GetRawTexture();

	/* how the surface is cut for drawing (3x3, 3x1, ...), and the stretching parts' ratio */
	void SetDivideMode(uint32_t mode);
	int32_t SetDivideStretchRatio(math::v2 ratio);
	void GetDivideStretchRatio(math::v2 &ratio) const;

	/* a division ratio's preset: none, 1/3, 0.49, 1/2, or m_div_ratio's (names ours) */
	enum DivideRatio {
		DivideRatio_None = 0,
		DivideRatio_Third = 1,
		DivideRatio_049 = 2,
		DivideRatio_Half = 3,
		DivideRatio_Other = 4
	};

	uint16_t GetDivideMode() const
	{
		return m_divide_mode;
	}

	/* the texture's sampling (src/graphics/state.c, still C) */
	int32_t SetMagFilter(TextureFilter value);
	int32_t SetMinFilter(TextureFilter value);
	int32_t SetWrapS(TextureWrap value);
	int32_t SetWrapT(TextureWrap value);

	int32_t GetMagFilter() const
	{
		return m_texture_param.mag_filter;
	}

	int32_t GetWrapS() const
	{
		return m_texture_param.wrap_s;
	}

	int32_t GetWrapT() const
	{
		return m_texture_param.wrap_t;
	}

	/* the margins ahead of the division (left, bottom, right, top) */
	uint16_t GetMarginLeft() const
	{
		return m_margin_left;
	}

	uint16_t GetMarginBottom() const
	{
		return m_margin_bottom;
	}

	uint16_t GetMarginRight() const
	{
		return m_margin_right;
	}

	uint16_t GetMarginTop() const
	{
		return m_margin_top;
	}

	int32_t GetRefCount() const
	{
		return m_ref_count;
	}

	/* 0x80AF2206 without somewhere to put the page */
	int32_t GetPageIndex(FrameBufferType type, int32_t *page);

	int16_t GetWidth() const
	{
		return m_width;
	}

	int16_t GetHeight() const
	{
		return m_height;
	}

	ImageOrder GetOrder() const
	{
		return m_order;
	}

	int32_t GetStride() const
	{
		return m_stride;
	}

	/* the page drawn into */
	int16_t GetPageCounter() const
	{
		return m_page_counter;
	}

	/* the pages (name ours) */
	int16_t GetNumPage() const
	{
		return m_num_page;
	}

	/* The pages' memory from the pool; NULL when the pool had none. */
	void *GetMemory() const
	{
		return m_mem.bp;
	}

	/* the texture loader sets the margins */
	friend class ::paf::Plugin;
	friend int32_t (::ScePafGraphics_36960B24)(Surface *surface, math::v2 ratio_h,
						   math::v2 ratio_v);
	friend void (::ScePafGraphics_AFAE7807)(Surface *surface, math::v2 *ratio_h,
						math::v2 *ratio_v);

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
	/* Clear cleared the page drawn into; Swap forgets it (name ours) */
	uint8_t m_cleared : 1;
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
	/* SetDivideMode's */
	uint16_t m_divide_mode;
	/* the division's ratios across and down, as DivideRatio presets; the texture element's
	   div_ratio_h and div_ratio_v (names ours) */
	uint8_t m_div_ratio_h;
	uint8_t m_div_ratio_v;
	/* x and y of the ratios across and down that are no preset, then of the stretching part's
	   (div_ratio_stretch) (name ours) */
	float *m_div_ratio;
	/* the texture element's margin_h and margin_v (Plugin's texture loader) */
	uint16_t m_margin_left;
	uint16_t m_margin_bottom;
	uint16_t m_margin_right;
	uint16_t m_margin_top;
};

}
}

/* Surface loads Sony's exports do not name yet: the file and memory Load for an image_order. */
extern "C" paf::intrusive_ptr<paf::graph::Surface>
ScePafGraphics_AD93FF1B(paf::graph::SurfacePool *pool, paf::common::SharedPtr<paf::File> fp,
			paf::graph::Surface::LoadOption *option);
extern "C" paf::intrusive_ptr<paf::graph::Surface>
ScePafGraphics_476AB004(paf::graph::SurfacePool *pool, void *buf, int64_t size,
			paf::graph::Surface::LoadOption *option);

/* the page before the one drawn into, the last after the first (Sony's name not found) */
extern "C" int32_t ScePafGraphics_B000FA14(paf::graph::Surface *surface);

/* the colour table's entries copied in (Sony's name not found) */
extern "C" int32_t ScePafGraphics_F4986A3A(paf::graph::SurfaceCLUT *clut, const void *src);

/* Copy for the DXT formats (Sony's name not found) */
extern "C" int32_t ScePafGraphics_D811125C(paf::graph::Surface *surface, int32_t page,
					   const void *src, ImageOrder src_order,
					   int32_t line_bytes, int32_t level);

#endif
