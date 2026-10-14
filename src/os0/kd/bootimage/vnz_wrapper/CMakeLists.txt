if(FW_DEBUG)
	set(VENEZIA_RPC_SOURCES source/dsp0_rpc.c source/dsp0_rpc_debug.c)
else()
	set(VENEZIA_RPC_SOURCES source/dsp0_rpc_debug.c source/dsp0_rpc.c)
endif()

# SceCodecEngineWrapper C implementation.
psp2_module(os0_kd_bootimage_vnz_wrapper KIND kernel SOURCES
	source/entry.c
	source/entry_start.c
	source/venezia.c
	source/booter.c
	${VENEZIA_RPC_SOURCES}
	source/dsp0_rpc_init.c
	source/dsp0_rpc_history.c
	source/dsp0_mailbox.c
	source/dsp0_communication.c
	source/heapmgr.c
	source/heap.c
	source/simple_heap.c
	source/vnzmem.c
	source/dsp0_dma.c
	source/system.c
	source/dsp0_debug.c
)
