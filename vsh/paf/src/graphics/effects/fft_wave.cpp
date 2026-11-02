/*
 * The waves computed by FFT, moved on every frame (the file's name ours). Only this file has the
 * (40, 30) size at its end; nothing reads it.
 */
#include <stdint.h>
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/drawobj/draw_env.h>
#include <src/common/frame_count.h>

namespace paf {
namespace graph {

static const arl::SurfaceSize s_reserved_size(40, 30);

void FFTWave::Update()
{
	arl::ArlInstance *gl = arl::Gl();

	m_param_keys.Update(gl->m_frame_time);
	m_frame = common::FrameCount();
	if (!m_paused)
		m_time += m_speed * gl->m_frame_time;
	if (m_time > m_period / (2.0f * 3.14159265f))
		m_time = 0.0f;
	m_surface.Update(m_paused, false);
}

}
}
