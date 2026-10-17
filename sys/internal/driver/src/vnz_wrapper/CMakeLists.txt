# SceCodecEngineWrapper C implementation.
psp2_module(os0_kd_bootimage_vnz_wrapper KIND kernel SOURCES
	entry.c
	entry_start.c
	venezia/venezia.c
	venezia/booter.c
	venezia/dsp0_rpc.c
	dsp0_mailbox.c
	venezia/vnzmem.c
	venezia/heapmgr.c
	venezia/heap.c
	venezia/simple_heap.c
	venezia/vapa.c
	venezia/system.c
	dsp0_debug.c
)
