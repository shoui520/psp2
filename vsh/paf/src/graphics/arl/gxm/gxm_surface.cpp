/*
 * paf::graph::arl's surfaces on GXM: the render target pool, and a surface's storage, its
 * memory laid out in mip levels and the GXM surfaces and textures made over it (the file's
 * name ours).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <kernel/sysmem/memblock.h>
#include <sysmem/memblock.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_render_target_pool.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <src/common/frame_count.h>
#include <src/module.h>

extern "C" {
/* SceGxmInternal: a render target in the memory block its parameters name */
int sceGxmGetRenderTargetMemSizeInternal(const SceGxmRenderTargetParams *params,
					 SceSize *driverMemSize);
int sceGxmCreateRenderTargetInternal(const SceGxmRenderTargetParams *params,
				     SceGxmRenderTarget **renderTarget);
}

namespace paf {
namespace graph {
namespace arl {

RenderTargetPool::RenderTargetPool() : m_reserved(0), m_mem_size(0)
{
}

void RenderTargetPool::Clear()
{
	for (map<Key, TargetList *>::iterator it = m_targets.begin(); it != m_targets.end();
	     ++it) {
		TargetList *list = it->second;

		for (uint32_t i = 0; i != list->size(); i++) {
			sceGxmDestroyRenderTarget((*list)[i]->m_target);
			delete (*list)[i];
		}
		delete it->second;
	}
	m_targets._Tidy();
	m_scenes._Tidy();
}

RenderTargetPool::~RenderTargetPool()
{
	Clear();
}

/* a target for the size that is free in this frame, else a new one */
SceGxmRenderTarget *RenderTargetPool::GetRenderTarget(Key size)
{
	Key key = size;

	if (m_targets.find(key) == m_targets.end())
		m_targets[key] = new TargetList();

	TargetList *list = m_targets.find(key)->second;
	uint32_t i;

	for (i = 0; i != list->size(); i++) {
		if ((*list)[i]->m_frame < common::FrameCount())
			break;
		if ((*list)[i]->m_frame == (uint32_t)-1 ||
		    ((*list)[i]->m_frame == common::FrameCount() &&
		     (*list)[i]->m_uses < (*list)[i]->m_params.scenesPerFrame))
			break;
	}
	if (i == list->size()) {
		Target *target = new Target();
		SceSize size;

		target->m_params.flags = 0;
		target->m_params.width = key.m_width;
		target->m_params.height = key.m_height;
		target->m_params.scenesPerFrame = 1;
		target->m_params.multisampleMode = key.m_multisample_mode;
		target->m_params.multisampleLocations = 0;
		target->m_params.driverMemBlock = -1;

		map<Key, int32_t>::iterator scenes = m_scenes.find(key);

		if (scenes != m_scenes.end())
			target->m_params.scenesPerFrame = scenes->second;
		if (sce_paf_is_common_dialog_mode)
			target->m_params.flags |= 4;
		sceGxmGetRenderTargetMemSizeInternal(&target->m_params, &size);
		m_mem_size += size;
		target->m_params.driverMemBlock =
			sceKernelAllocMemBlock("ScePafQglRenderTargetPool",
					       sce_paf_is_common_dialog_mode ?
					       SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_NC_RW :
					       SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, size, NULL);
		sceGxmCreateRenderTargetInternal(&target->m_params, &target->m_target);
		list->push_back(target);
		i = list->size() - 1;
	}
	if ((*list)[i]->m_frame != common::FrameCount())
		(*list)[i]->m_uses = 1;
	(*list)[i]->m_frame = common::FrameCount();
	return (*list)[i]->m_target;
}

/* how many scenes a frame draws into the targets of a size */
void RenderTargetPool::AddScenes(Key size, int32_t scenes)
{
	Key key = size;
	map<Key, int32_t>::iterator it = m_scenes.find(key);

	if (it == m_scenes.end())
		m_scenes.insert(map<Key, int32_t>::value_type(key, scenes));
	else
		it->second += scenes;
}

/* the bytes of a pixel of each format a surface takes */
static inline uint32_t BytesPerPixel(ImageMode format)
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
	case ImageMode_A16FP:
		return 2;
	case ImageMode_depth32:
	case ImageMode_A32FP:
		return 4;
	case ImageMode_WZYX16FP:
		return 8;
	case ImageMode_WZYX32FP:
		return 16;
	default:
		return 0;
	}
}

SurfaceStorage::SurfaceStorage(common::SharedPtr<VramAllocator> allocator)
	: m_width(0), m_pitch(0), m_height(0), m_flags(0), m_levels(0),
	  m_memory(new SurfaceMemory(allocator))
{
}

SurfaceStorage::~SurfaceStorage()
{
}

void SurfaceStorage::Alloc(ImageMode format, uint32_t width, uint32_t height, SurfaceFlags flags,
			   uint32_t levels)
{
	SurfaceMemory *memory = m_memory.get();
	void *addr = memory->m_addr;
	uint32_t align = 0;
	uint32_t offset = 0;

	if (flags.IsSet(COLOR_SURFACE))
		align = 32;
	if (flags.IsSet(STENCILDEPTH_SURFACE)) {
		if (align < 16)
			align = 16;
		if (align < 32)
			align = 32;
	}
	if (flags.IsSet(TEXTURE) && align < 16)
		align = 16;
	if (flags.IsSet(PALETTE) && align < 64)
		align = 64;
	m_width = width;
	m_pitch = align != 0 ? (width + align - 1) & ~(align - 1) : width;
	m_height = height;
	m_flags = flags;
	m_levels = levels;
	Level *level = m_level;

	for (uint32_t i = 0; i != m_levels; i++, level++) {
		level->m_width = m_pitch >> i;
		level->m_height = m_height >> i;
		level->m_offset = offset;
		level->m_size = level->m_width * level->m_height * BytesPerPixel(format);
		offset += level->m_size;
	}
	if (addr == NULL) {
		memory->m_allocator->Alloc(memory, offset);
		memory->m_size = offset;
	}
}

static inline SceGxmColorFormat GetColorFormat(ImageMode format)
{
	switch (format) {
	case ImageMode_A8:
		return SCE_GXM_COLOR_FORMAT_A8;
	case ImageMode_BGRA8888:
		return SCE_GXM_COLOR_FORMAT_A8R8G8B8;
	case ImageMode_RGBA8888:
		return SCE_GXM_COLOR_FORMAT_A8B8G8R8;
	case ImageMode_WZYX16FP:
		return SCE_GXM_COLOR_FORMAT_F16F16F16F16_RGBA;
	default:
		return SCE_GXM_COLOR_FORMAT_A8B8G8R8;
	}
}

void SurfaceStorage::InitColorSurface(SceGxmColorSurface *surface, ImageMode format,
				      const SurfaceFlags &flags, uint32_t level)
{
	if (format != ImageMode_RGBA8888 && format != ImageMode_A8 &&
	    format != ImageMode_WZYX16FP)
		return;

	uint8_t *data = (uint8_t *)m_memory->m_addr;
	SurfaceFlags surface_flags = flags;

	if (level != 0)
		data += m_level[level].m_offset;

	SceGxmColorFormat color_format = GetColorFormat(format);
	SceGxmColorSurfaceType type;
	SceGxmColorSurfaceScaleMode scale;
	SceGxmOutputRegisterSize size;

	if (surface_flags.IsSet(COLOR_SURFACE_LINEAR))
		type = SCE_GXM_COLOR_SURFACE_LINEAR;
	else if (surface_flags.IsSet(COLOR_SURFACE_TILED))
		type = SCE_GXM_COLOR_SURFACE_TILED;
	else if (surface_flags.IsSet(COLOR_SURFACE_SWIZZLED))
		type = SCE_GXM_COLOR_SURFACE_SWIZZLED;
	else
		type = SCE_GXM_COLOR_SURFACE_LINEAR;
	if (surface_flags.IsSet(COLOR_SURFACE_SCALE_NONE))
		scale = SCE_GXM_COLOR_SURFACE_SCALE_NONE;
	else if (surface_flags.IsSet(COLOR_SURFACE_SCALE_MSAA_DOWNSCALE))
		scale = SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE;
	else
		scale = SCE_GXM_COLOR_SURFACE_SCALE_NONE;
	if (surface_flags.IsSet(COLOR_SURFACE_OUTPUT_REGISTER_SIZE_32BIT))
		size = SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT;
	else if (surface_flags.IsSet(COLOR_SURFACE_OUTPUT_REGISTER_SIZE_64BIT))
		size = SCE_GXM_OUTPUT_REGISTER_SIZE_64BIT;
	else
		size = SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT;
	sceGxmColorSurfaceInit(surface, color_format, type, scale, size,
			       m_level[level].m_width, m_level[level].m_height,
			       m_level[level].m_width, data);
}

void SurfaceStorage::InitDepthStencilSurface(SceGxmDepthStencilSurface *surface,
					     ImageMode format, const SurfaceFlags &flags)
{
	if (format != ImageMode_depth16 && format != ImageMode_depth32)
		return;

	SurfaceFlags surface_flags = flags;
	SceGxmDepthStencilFormat depth_format = format == ImageMode_depth32 ?
						SCE_GXM_DEPTH_STENCIL_FORMAT_DF32 :
						SCE_GXM_DEPTH_STENCIL_FORMAT_D16;
	SceGxmDepthStencilSurfaceType type;

	if (surface_flags.IsSet(STENCILDEPTH_SURFACE_LINEAR))
		type = SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR;
	else if (surface_flags.IsSet(STENCILDEPTH_SURFACE_TILED))
		type = SCE_GXM_DEPTH_STENCIL_SURFACE_TILED;
	else
		type = SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR;
	sceGxmDepthStencilSurfaceInit(surface, depth_format, type, m_pitch, m_memory->m_addr, NULL);
	sceGxmDepthStencilSurfaceSetForceStoreMode(surface,
		flags.IsSet(STENCILDEPTH_SURFACE_FORCE_STORE) ?
		SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED : SCE_GXM_DEPTH_STENCIL_FORCE_STORE_DISABLED);
	sceGxmDepthStencilSurfaceSetForceLoadMode(surface,
		flags.IsSet(STENCILDEPTH_SURFACE_FORCE_LOAD) ?
		SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED : SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_DISABLED);
}

static SceGxmTextureFormat GetTextureFormat(ImageMode format)
{
	switch (format) {
	case ImageMode_L8:
		return SCE_GXM_TEXTURE_FORMAT_L8;
	case ImageMode_A8:
		return SCE_GXM_TEXTURE_FORMAT_A8;
	case ImageMode_A16FP:
		return SCE_GXM_TEXTURE_FORMAT_F16_R000;
	case ImageMode_BGRA8888:
		return SCE_GXM_TEXTURE_FORMAT_A8R8G8B8;
	case ImageMode_RGBA8888:
		return SCE_GXM_TEXTURE_FORMAT_A8B8G8R8;
	case ImageMode_WZYX16FP:
		return SCE_GXM_TEXTURE_FORMAT_F16F16F16F16_RGBA;
	case ImageMode_A32FP:
		return SCE_GXM_TEXTURE_FORMAT_F32M_R000;
	case ImageMode_depth32:
		return SCE_GXM_TEXTURE_FORMAT_F32M_000R;
	default:
		return (SceGxmTextureFormat)0;
	}
}

int SurfaceStorage::InitTexture(SceGxmTexture *texture, ImageMode format,
				const SurfaceFlags &flags)
{
	if (flags.IsSet(TEXTURE_SWIZZLED))
		return sceGxmTextureInitSwizzled(texture, m_memory->m_addr, GetTextureFormat(format),
						 m_width, m_height, m_levels);
	if (flags.IsSet(TEXTURE_LINEAR))
		return sceGxmTextureInitLinear(texture, m_memory->m_addr, GetTextureFormat(format),
					       m_width, m_height, m_levels);
	if (flags.IsSet(TEXTURE_LINEAR_STRIDED))
		return sceGxmTextureInitLinearStrided(texture, m_memory->m_addr,
						      GetTextureFormat(format), m_width, m_height,
						      m_width * BytesPerPixel(format));
	if (flags.IsSet(TEXTURE_TILED))
		return sceGxmTextureInitTiled(texture, m_memory->m_addr, GetTextureFormat(format),
					      m_width, m_height, m_levels);
	return 0;
}

}
}
}
