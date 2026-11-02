# PARTIAL until the SrcMix DSP programs (srcmix.c audioSrcMixProgram, audioSrcMixSecondaryProgram) are rebuilt from source.
psp2_module(os0_kd_bootimage_audio KIND kernel COMPILER gcc48 PARTIAL SOURCES
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
