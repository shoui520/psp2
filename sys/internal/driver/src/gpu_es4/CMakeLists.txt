# Native partial implementation (T2): GPU startup and remaining services pending.
psp2_module(os0_kd_bootimage_gpu_es4 KIND kernel SKELETON PARTIAL SOURCES
	bridged_sgx_bridge_sce.c
	bridged_pvr_bridge.c
	buffer_manager_sce.c
	handle.c
	hash.c
	lists.c
	mem.c
	memorymanager.c
	perproc.c
	power.c
	pvrsrv_sce.c
	resman_sce.c
	sharedsync.c
	pb_sce.c
	sgxinit_sce.c
	sgxkick_sce.c
	sgxpower_sce.c
	sgxreset_sce.c
	sgxtransfer_sce.c
	sgxutils_sce.c
	mmu_sce.c
	mutex.c
	osfunc.c
	osperproc.c
	process.c
	pvr_bridge_k.c
	pvr_debug.c
	gpu_es4.c
	signal-lockup.c
	hostmem.c
	memorycontext.c
	register-map.c
	lockup-history.c
	gpu-heap.c
	mmu.c
	gpu-blockheap.c
	gpu-spans.c
	cache_sce.c
	waitmodobj.c
	device_sce.c
	suspend_sce.c
	registerdump.c
	registerdump_analysis.c
	client_pb.c
	client_render.c
	client_ccb.c
	sysconfig_sce.c
	gpu-blocks.c
	handle-base.c
)

# Recovered bodies use the original explicit guard frames in their wrappers.
# Adding a second compiler guard changes observable native caller-frame words.
set_source_files_properties(
	handle-base.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector"
)
