# SceCodecEnginePerf (vs0:sys/external/libcodecengine_perf.suprx): forwards to
# the SceCodecEngineWrapper syscalls. Sony built it without a stack protector.
set(MODULE_COMPILE_OPTIONS -fno-stack-protector -fno-optimize-sibling-calls)
psp2_module(vs0_sys_external_libcodecengine_perf KIND user SOURCES
	libcodecengine_perf.c
)
