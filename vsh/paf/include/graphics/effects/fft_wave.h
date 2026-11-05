/*
 * The parameters of the waves computed by FFT that a file including this keeps: every file that
 * includes it has its own copy of the defaults (the name ours).
 */
#ifndef PAF_GRAPHICS_EFFECTS_FFT_WAVE_H
#define PAF_GRAPHICS_EFFECTS_FFT_WAVE_H

#include <graphics/effects/fft_wave_surface.h>

namespace paf {
namespace graph {

static FFTWaveParam s_fft_wave_param;

}
}

/* after the defaults, so a file that includes this first constructs them first */
#include <graphics/effects/wave_param.h>

namespace paf {
namespace graph {

/* the FFT waves' set takes its defaults from this file's copy */
inline FFTWaveParamSet::Values::Values(ParamSet *set)
	: m_gravity(set, "gravity", s_fft_wave_param.m_gravity, 0.1f, 30.0f, 0),
	  m_a(set, "A", s_fft_wave_param.m_a, 0.0f, s_fft_wave_param.m_a * 10.0f,
	      PARAM_FLAG_RAW),
	  m_log_a(set, "logA", s_fft_wave_param.m_log_a, -20.0f, -1.0f, PARAM_FLAG_FINE),
	  m_patch_size(set, "patch_size", s_fft_wave_param.m_patch_size, 0.0f, 500.0f, 0),
	  m_wind_speed(set, "wind_speed", s_fft_wave_param.m_wind_speed, 6.0f, 20.0f, 0),
	  m_wind_dir(set, "wind_dir", s_fft_wave_param.m_wind_dir, -WAVE_PARAM_PI,
		     WAVE_PARAM_PI, 0),
	  m_time_step(set, "time_step", s_fft_wave_param.m_time_step, 0.0f, 0.1f,
		      PARAM_FLAG_FINE)
{
}

}
}

#endif
