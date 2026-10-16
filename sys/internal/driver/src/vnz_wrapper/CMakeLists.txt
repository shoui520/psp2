if(FW_DEBUG)
	set(VENEZIA_RPC_SOURCES dsp0_rpc.c dsp0_rpc_debug.c)
else()
	set(VENEZIA_RPC_SOURCES dsp0_rpc_debug.c dsp0_rpc.c)
endif()

# SceCodecEngineWrapper C implementation.
psp2_module(os0_kd_bootimage_vnz_wrapper KIND kernel SOURCES
	entry.c
	entry_start.c
	venezia/venezia.c
	venezia/booter.c
	${VENEZIA_RPC_SOURCES}
	dsp0_rpc_init.c
	dsp0_rpc_history.c
	dsp0_mailbox.c
	dsp0_communication.c
	venezia/heapmgr.c
	venezia/heap.c
	venezia/simple_heap.c
	vnzmem.c
	venezia/vapa.c
	venezia/system.c
	dsp0_debug.c
)
