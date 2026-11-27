# SceAudiocodec (vs0:sys/external/audiocodec.suprx)
psp2_module(vs0_sys_external_audiocodec KIND user COMPILER gcc48 SOURCES
	audiocodec.c
	audiocodec_tables.c
	atrac3.c
	atrac3plus.c
	mp3.c
	aac.c
	atrac9.c
	audiocodec_math.c
	audiocodec_qsort.c
)
target_compile_options(vs0_sys_external_audiocodec PRIVATE -mfloat-abi=hard)
set_source_files_properties(mp3.c PROPERTIES COMPILE_OPTIONS -marm)
set_source_files_properties(audiocodec_math.c audiocodec_qsort.c PROPERTIES COMPILE_OPTIONS -Os)
