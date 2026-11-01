/*
 * Textures paf makes without an image: noise, one colour, a checkerboard and a radial
 * gradient (the file's name and the function names ours, but for the export's NID).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <std/vector>

using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace graph {
namespace arl {

/* the next number of a minimal standard generator as a float in [-1, 1) */
static inline float NextRandom(uint32_t &seed)
{
	union {
		uint32_t bits;
		float value;
	} number;

	seed *= 16807;
	number.bits = (seed >> 9) | 0x40000000;
	return number.value - 3.0f;
}

common::SharedPtr<Texture2D> CreateNoiseTexture(common::SharedPtr<VramAllocator> allocator,
						SurfaceSize size, SurfaceFlags flags)
{
	vector<uint32_t> pixels(size.m_width * size.m_height);
	uint32_t seed = 1;

	for (size_t i = 0; i < pixels.size(); ++i) {
		float r = NextRandom(seed);
		float g = NextRandom(seed);
		float b = NextRandom(seed);
		float a = NextRandom(seed);
		Vector4 color(r, g, b, a);

		color = mulPerElem(color + Vector4(1.0f), Vector4(0.5f));
		color = maxPerElem(minPerElem(color, Vector4(1.0f)), Vector4(0.0f));
		color = mulPerElem(color, Vector4(255.0f));
		pixels[i] = (uint32_t)vgetq_lane_f32(color.get128(), 0) |
			    (uint32_t)vgetq_lane_f32(color.get128(), 1) << 8 |
			    (uint32_t)vgetq_lane_f32(color.get128(), 2) << 16 |
			    (uint32_t)vgetq_lane_f32(color.get128(), 3) << 24;
	}

	common::SharedPtr<Texture2D> texture =
		CreateTexture<ImageMode_RGBA8888>(allocator, size, &pixels[0], flags, 0);

	texture->setWrapS(TEXTURE_WRAP_REPEAT);
	texture->setWrapT(TEXTURE_WRAP_REPEAT);
	return texture;
}

common::SharedPtr<Texture2D> CreateSolidTexture(common::SharedPtr<VramAllocator> allocator,
						uint32_t color, SurfaceSize size,
						SurfaceFlags flags)
{
	vector<uint32_t> pixels(size.m_width * size.m_height);

	for (size_t i = 0; i < pixels.size(); ++i)
		pixels[i] = color;

	common::SharedPtr<Texture2D> texture =
		CreateTexture<ImageMode_RGBA8888>(allocator, size, &pixels[0], flags, 0);

	texture->setWrapS(TEXTURE_WRAP_REPEAT);
	texture->setWrapT(TEXTURE_WRAP_REPEAT);
	texture->setMinFilter(SCE_GXM_TEXTURE_FILTER_POINT, false);
	texture->setMagFilter(SCE_GXM_TEXTURE_FILTER_POINT);
	return texture;
}

/* a checkerboard's texel: white on the odd cells */
static inline uint8_t CheckerTexel(uint32_t x, uint32_t y, SurfaceSize cell)
{
	return ((x / cell.m_width + (y / cell.m_height & 1)) & 1) ? 0xFF : 0;
}

common::SharedPtr<Texture2D> CreateCheckerTexture(common::SharedPtr<VramAllocator> allocator,
						  SurfaceSize size, SurfaceSize cell,
						  SurfaceFlags flags)
{
	uint32_t width = size.m_width;
	uint32_t height = size.m_height;
	vector<uint8_t> pixels(width * height);

	for (uint32_t y = 0; y != height; ++y) {
		for (uint32_t x = 0; x != width; ++x)
			pixels[y * width + x] = CheckerTexel(x, y, cell);
	}

	common::SharedPtr<Texture2D> texture =
		CreateTexture<ImageMode_L8>(allocator, size, &pixels[0], flags, 0);

	texture->setWrapS(TEXTURE_WRAP_REPEAT);
	texture->setWrapT(TEXTURE_WRAP_REPEAT);
	texture->setMinFilter(SCE_GXM_TEXTURE_FILTER_POINT, false);
	texture->setMagFilter(SCE_GXM_TEXTURE_FILTER_POINT);
	return texture;
}

/* the index of the highest bit set */
static inline uint32_t Log2(uint32_t value)
{
	uint32_t shift;
	uint32_t result;

	result = (value > 0xFFFF) << 4;
	value >>= result;
	shift = (value > 0xFF) << 3;
	value >>= shift;
	result |= shift;
	shift = (value > 0xF) << 2;
	value >>= shift;
	result |= shift;
	shift = (value > 0x3) << 1;
	value >>= shift;
	result |= shift;
	return result | (value >> 1);
}

}
}
}

using namespace paf;
using namespace paf::graph::arl;

common::SharedPtr<Texture2D> ScePafGraphics_27C5476A(common::SharedPtr<VramAllocator> allocator,
						     uint32_t size, SurfaceFlags flags, bool mipmap,
						     bool generate_mipmaps)
{
	Gl();

	uint32_t levels = Log2(size);

	if (!mipmap)
		levels = 1;

	common::SharedPtr<Texture2D> texture =
		CreateTexture<ImageMode_L8>(allocator, SurfaceSize(size, size), NULL, flags, levels);
	int32_t width = texture->m_desc.m_width;
	int32_t height = texture->m_desc.m_height;

	for (uint32_t level = 0; level < levels; ++level) {
		vector<uint8_t> pixels;

		for (int32_t y = 0; y != height; ++y) {
			for (int32_t x = 0; x != width; ++x) {
				Vector2 position((float)x / (float)(width - 1),
						 (float)y / (float)(height - 1));
				float distance = length(mulPerElem(position - Vector2(0.5f),
								   Vector2(2.0f)));

				if (distance < 0.0f)
					distance = 0.0f;

				float value = 1.0f;

				if (distance < 1.0f)
					value = distance;
				pixels.push_back((uint8_t)(value * 255.0f));
			}
		}
		ScePafGraphics_309D2902(texture.get(), level, width, height,
					texture->m_desc.m_format, texture->m_desc.m_levels, &pixels[0]);
		width >>= 1;
		height >>= 1;
	}
	texture->setWrapS(TEXTURE_WRAP_CLAMP_FULL_BORDER);
	texture->setWrapT(TEXTURE_WRAP_CLAMP_FULL_BORDER);
	if (generate_mipmaps)
		ScePafGraphics_FF97623F(texture.get(), 1.0f);
	return texture;
}
