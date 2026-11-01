/*
 * paf::graph::arl::TextureObject and Texture2D: a texture's surface storage and the
 * SceGxmTexture over it (the file's name ours).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/color.h>
#include <std/string.h>

namespace paf {
namespace graph {
namespace arl {

TextureObject::TextureObject(common::SharedPtr<VramAllocator> allocator, int32_t type)
	: m_storage(new SurfaceStorage(allocator)), m_type(type)
{
}

TextureObject::~TextureObject()
{
}

void TextureObject::OnUpdate()
{
}

int TextureObject::setLodBias(float bias)
{
	float value = bias * 8.0f + 31.0f;

	if (value < 0.0f)
		value = 0.0f;
	return sceGxmTextureSetLodBias(m_texture.get(), (uint32_t)(value < 63.0f ? value : 63.0f));
}

int TextureObject::setMinFilter(SceGxmTextureFilter filter, bool mipmap)
{
	sceGxmTextureSetMipFilter(m_texture.get(), mipmap ? SCE_GXM_TEXTURE_MIP_FILTER_ENABLED :
							    SCE_GXM_TEXTURE_MIP_FILTER_DISABLED);
	return sceGxmTextureSetMinFilter(m_texture.get(), filter);
}

int TextureObject::setMagFilter(SceGxmTextureFilter filter)
{
	return sceGxmTextureSetMagFilter(m_texture.get(), filter);
}

void TextureObject::Unused2()
{
}

/* the border modes need a swizzled texture: others clamp to the edge */
int TextureObject::setWrapS(TextureWrap wrap)
{
	bool swizzled = m_storage->m_flags.IsSet(TEXTURE_SWIZZLED);
	SceGxmTexture *texture = m_texture.get();

	if ((wrap == TEXTURE_WRAP_CLAMP_FULL_BORDER || wrap == TEXTURE_WRAP_CLAMP_HALF_BORDER) &&
	    !swizzled)
		wrap = TEXTURE_WRAP_CLAMP;
	return sceGxmTextureSetUAddrMode(texture, (SceGxmTextureAddrMode)wrap);
}

int TextureObject::setWrapT(TextureWrap wrap)
{
	bool swizzled = m_storage->m_flags.IsSet(TEXTURE_SWIZZLED);
	SceGxmTexture *texture = m_texture.get();

	if ((wrap == TEXTURE_WRAP_CLAMP_FULL_BORDER || wrap == TEXTURE_WRAP_CLAMP_HALF_BORDER) &&
	    !swizzled)
		wrap = TEXTURE_WRAP_CLAMP;
	return sceGxmTextureSetVAddrMode(texture, (SceGxmTextureAddrMode)wrap);
}

}
}
}

using namespace paf;
using namespace paf::graph::arl;

void ScePafGraphics_FF97623F(TextureObject *texture, float quality)
{
}

namespace paf {
namespace graph {
namespace arl {

bool TextureObject::alloc(TextureDesc desc, int32_t depth, int32_t samples, SurfaceFlags flags)
{
	int32_t levels = desc.m_levels;

	m_desc.m_width = desc.m_width;
	m_desc.m_height = desc.m_height;
	m_desc.m_format = desc.m_format;
	flags.Set(TEXTURE);
	if (!flags.IsSet(TEXTURE_LINEAR) && !flags.IsSet(TEXTURE_LINEAR_STRIDED) &&
	    !flags.IsSet(TEXTURE_TILED) && !flags.IsSet(TEXTURE_SWIZZLED))
		flags.Set(TEXTURE_LINEAR);
	if (flags.IsSet(TEXTURE_ALLOC_MIPMAPS)) {
		int32_t count;

		for (count = 0; (desc.m_width >> count) != 0 && (desc.m_height >> count) != 0; count++)
			;
		if (desc.m_levels != 0 && count >= (int32_t)desc.m_levels)
			count = desc.m_levels;
		levels = count;
	}
	m_desc.m_levels = levels > 1 ? levels : 1;
	m_storage->Alloc(desc.m_format, desc.m_width, desc.m_height, flags, m_desc.m_levels);

	{
		common::SharedPtr<SurfaceStorage> storage = m_storage;

		m_texture.reset(new SceGxmTexture());
		storage->InitTexture(m_texture.get(), desc.m_format, flags);
	}
	sceGxmTextureSetMipFilter(m_texture.get(), m_desc.m_levels > 1 ?
				  SCE_GXM_TEXTURE_MIP_FILTER_ENABLED :
				  SCE_GXM_TEXTURE_MIP_FILTER_DISABLED);
	sceGxmTextureSetMinFilter(m_texture.get(), SCE_GXM_TEXTURE_FILTER_LINEAR);
	sceGxmTextureSetMagFilter(m_texture.get(), SCE_GXM_TEXTURE_FILTER_LINEAR);
	return true;
}

Texture2D::Texture2D(common::SharedPtr<VramAllocator> allocator) : TextureObject(allocator, 0)
{
}

Texture2D::~Texture2D()
{
}

}
}
}

bool ScePafGraphics_C4BD3558(Texture2D *texture, int32_t width, int32_t height, ImageMode format,
			     uint32_t levels, SurfaceFlags flags)
{
	return texture->alloc(TextureDesc(width, height, format, levels), 1, 0, flags);
}

namespace paf {
namespace graph {
namespace arl {

bool Texture2D::init(const common::SharedPtr<TextureObject> &other)
{
	return alloc(other->m_desc, 1, 0, other->m_storage->m_flags);
}

}
}
}

int ScePafGraphics_309D2902(Texture2D *texture, int32_t level, int32_t width, int32_t height,
			    ImageMode format, uint32_t levels, const void *data)
{
	if (texture->m_desc.m_width == 0 || texture->m_desc.m_height == 0)
		return 0;

	SurfaceStorage *storage = texture->m_storage.get();

	sce_paf_memcpy((uint8_t *)storage->m_memory->m_addr + storage->m_level[level].m_offset,
		       data, storage->m_level[level].m_size);
	return 1;
}
