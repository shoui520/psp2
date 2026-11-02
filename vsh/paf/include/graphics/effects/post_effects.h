/*
 * The effects drawn over the scene once it is drawn: the blur of the screen behind the panels,
 * and the programs they share. Blur is Sony's name (its "Blur::draw_uniform_blur" marker); the
 * other classes and every member name are ours. Their code is not converted yet: Blur's is at
 * 0x8119d772.., the filter's and the programs' at 0x811a0f64.. and 0x811a16f2.
 */
#ifndef PAF_GRAPHICS_EFFECTS_POST_EFFECTS_H
#define PAF_GRAPHICS_EFFECTS_POST_EFFECTS_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <math/ngp/v2.h>

namespace paf {
namespace graph {

/* the fragment programs and textures of the post effects; 0x188 bytes */
class PostEffects {
public:
	/* 0x811a16f2 */
	PostEffects();
	/* 0x811fe8e4: implicit, emitted with the programs */
	~PostEffects();

	uint32_t reserved[0x188 / 4];
};

/* blurs a copy of the screen in steps and draws it back; 0x1f0 bytes */
class Blur {
public:
	/* 0x8119dd28 */
	Blur();
	/* 0x811f0efc: implicit, emitted with the renderer's destructor */
	~Blur();

	/* 0x8119de6c: the texture of the screen captured at a size, blurred from the first to
	   the second amount */
	void Capture(common::SharedPtr<arl::Texture2D> texture, arl::SurfaceSize size,
		     const math::v2 &amount);
	/* 0x8119eba2: the blurred copy drawn by the amount */
	void Draw(float amount);

	void SetPostEffects(common::SharedPtr<PostEffects> post_effects)
	{
		m_post_effects = post_effects;
	}

	uint8_t reserved[8];
	common::SharedPtr<PostEffects> m_post_effects;
	uint8_t reserved2[0x1e0];
};

/* a filter drawn over the screen; 0x2ec bytes */
class ScreenFilter {
public:
	/* 0x811a0f64 */
	ScreenFilter();
	/* 0x811f1198: implicit, emitted with the renderer's destructor */
	~ScreenFilter();

	uint32_t reserved[0x2e8 / 4];
	/* the flag ScePafGraphics_505CD74E sets; nothing in libpaf reads it by this offset */
	bool m_flag_505CD74E;
	uint8_t reserved2[3];
};

}
}

#endif
