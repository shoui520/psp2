psp2_module(os0_kd_bootimage_audio KIND kernel COMPILER gcc48 SOURCES
	audio_work.c
	biquad.c
	audio.c
	mixer.c
	i2s.c
	spdif.c
	aclkgen.c
	srcmix.c
	srcmix_program.S
	srcmix_coefficients.c
	alc.c
	alc_core.c
	equalizer.c
	equalizer_core.c
)

# The SrcMix processors' program, assembled from source (tools/srcmixasm.py); srcmix_program.S
# takes the binary with .incbin.
set(srcmix "${CMAKE_CURRENT_BINARY_DIR}/srcmix")
file(MAKE_DIRECTORY "${srcmix}")
add_custom_command(OUTPUT "${srcmix}/srcmix.bin"
	COMMAND python3 -I "${PSP2_SRC}/tools/srcmixasm.py" "${srcmix}/srcmix.bin"
		"${CMAKE_CURRENT_SOURCE_DIR}/srcmix.asm"
	DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/srcmix.asm" "${PSP2_SRC}/tools/srcmixasm.py" VERBATIM)
set_source_files_properties(srcmix_program.S PROPERTIES
	COMPILE_OPTIONS "-Wa,-I${srcmix}"
	OBJECT_DEPENDS "${srcmix}/srcmix.bin")
