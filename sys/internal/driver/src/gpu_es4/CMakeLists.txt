# Native partial implementation (T2): GPU startup and remaining services pending.
psp2_module(os0_kd_bootimage_gpu_es4 KIND kernel SKELETON PARTIAL SOURCES
	source/bridged_sgx_bridge_sce.c
	source/bridged_pvr_bridge.c
	source/buffer_manager_sce.c
	source/handle.c
	source/hash.c
	source/lists.c
	source/mem.c
	source/memorymanager.c
	source/perproc.c
	source/power.c
	source/pvrsrv_sce.c
	source/resman_sce.c
	source/sharedsync.c
	source/pb_sce.c
	source/sgxinit_sce.c
	source/sgxkick_sce.c
	source/sgxpower_sce.c
	source/sgxreset_sce.c
	source/sgxtransfer_sce.c
	source/sgxutils_sce.c
	source/mmu_sce.c
	source/mutex.c
	source/osfunc.c
	source/osperproc.c
	source/process.c
	source/pvr_bridge_k.c
	source/pvr_debug.c
	source/gpu_es4.c
	source/signal-lockup.c
	source/hostmem.c
	source/memorycontext.c
	source/register-map.c
	source/lockup-history.c
	source/gpu-heap.c
	source/mmu.c
	source/gpu-blockheap.c
	source/gpu-spans.c
	source/cache_sce.c
	source/waitmodobj.c
	source/device_sce.c
	source/suspend_sce.c
	source/registerdump.c
	source/registerdump_analysis.c
	source/client_pb.c
	source/client_render.c
	source/client_ccb.c
	source/sysconfig_sce.c
	source/gpu-blocks.c
	source/handle-base.c
)

# Recovered bodies use the original explicit guard frames in their wrappers.
# Adding a second compiler guard changes observable native caller-frame words.
set_source_files_properties(
	source/handle-base.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector"
)
