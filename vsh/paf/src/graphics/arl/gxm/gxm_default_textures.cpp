/*
 * paf::graph::arl's DefaultTextures: the textures the front end makes at its set-up (the file's
 * name ours; Sony's object holds these two functions and arl_defines.h's static initializer).
 */
#include <stdint.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_texture.h>

namespace paf {
namespace graph {
namespace arl {

void DefaultTextures::Init(common::SharedPtr<VramAllocator> allocator)
{
	m_white = CreateSolidTexture(allocator, 0xFFFFFFFF, SurfaceSize(16, 16), SurfaceFlags());
	m_black = CreateSolidTexture(allocator, 0x000000FF, SurfaceSize(16, 16), SurfaceFlags());
	m_noise = CreateNoiseTexture(allocator, SurfaceSize(64, 64), SurfaceFlags());
}

void DefaultTextures::Term()
{
	m_black.reset();
	m_white.reset();
	m_noise.reset();
}

}
}
}
