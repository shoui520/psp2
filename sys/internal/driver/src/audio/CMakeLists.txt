# Audio implementation; remaining exports are compiled as placeholders.
psp2_module(os0_kd_bootimage_audio KIND kernel PARTIAL SOURCES
	audio_work.c
	biquad.c
	audio.c
	mixer.c
	i2s.c
	spdif.c
	aclkgen.c
	srcmix.c
	srcmix_coefficients.c
	alc.c
	alc_core.c
	equalizer.c
	equalizer_core.c
)
