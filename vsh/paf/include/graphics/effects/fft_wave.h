/*
 * The parameters of the waves computed by FFT (the parameters' set is named "FFTWave"; its
 * members gravity, A, logA, patch_size, wind_speed, wind_dir and time_step). Every file that
 * includes this has its own copy of the defaults. The class name and the member names are ours;
 * which value is which parameter is read from the values, the constructor (0x8109fec0) is not
 * converted yet.
 */
#ifndef PAF_GRAPHICS_EFFECTS_FFT_WAVE_H
#define PAF_GRAPHICS_EFFECTS_FFT_WAVE_H

namespace paf {
namespace graph {

class FFTWaveParam {
public:
	FFTWaveParam();

	float m_gravity;
	float m_a;
	/* logf(m_a) */
	float m_log_a;
	float m_wind_speed;
	float m_wind_dir;
	float m_patch_size;
	float m_time_step;
	float m_reserved;
};

static FFTWaveParam s_fft_wave_param;

}
}

#endif
