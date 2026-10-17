# SceShutterSound (vs0:sys/external/libSceShutterSound.suprx), built by Sony
# with SNC; unwind_pr0.c is the SNC runtime's EHABI personality routine.
psp2_module(vs0_sys_external_libSceShutterSound KIND user SOURCES
	shutter_sound.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
