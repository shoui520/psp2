# SceShutterSound (vs0:sys/external/libSceShutterSound.suprx), built by Sony
# with SNC; unwind_pr0.c is the SNC runtime's EHABI personality routine.
psp2_module(vs0_sys_external_libSceShutterSound KIND user SOURCES
	source/shutter_sound.c
	../../../../common/runtime/unwind_pr0.c
)
