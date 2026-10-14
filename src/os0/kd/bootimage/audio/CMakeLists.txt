# Audio implementation; remaining exports are compiled as placeholders.
psp2_module(os0_kd_bootimage_audio KIND kernel PARTIAL SOURCES
	source/audio_work.c
	source/biquad.c
	source/audio.c
	source/mixer.c
	source/i2s.c
	source/spdif.c
	source/aclkgen.c
	source/srcmix.c
	source/srcmix_coefficients.c
	source/alc.c
	source/alc_core.c
	source/equalizer.c
	source/equalizer_core.c
)
