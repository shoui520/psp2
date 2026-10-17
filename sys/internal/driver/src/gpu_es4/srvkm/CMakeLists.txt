# Native partial implementation (T2): GPU startup and remaining services pending.
psp2_module(os0_kd_bootimage_gpu_es4 KIND kernel SKELETON PARTIAL SOURCES
	bridged/sgx/bridged_sgx_bridge_sce.c
	bridged/bridged_pvr_bridge.c
	common/buffer_manager_sce.c
	handle.c
	hash.c
	lists.c
	mem.c
	common/memorymanager.c
	common/perproc.c
	common/power.c
	common/pvrsrv.c
	common/resman.c
	common/sharedsync.c
	pb_sce.c
	sgxinit_sce.c
	sgxkick_sce.c
	sgxpower_sce.c
	sgxreset_sce.c
	sgxtransfer_sce.c
	sgxutils_sce.c
	devices/sgx/mmu_sce.c
	env/playstation/mutex.c
	env/playstation/osfunc.c
	osperproc.c
	env/playstation/process.c
	pvr_bridge_k.c
	env/playstation/pvr_debug.c
	gpu_es4.c
	../../gpu/lockup/signal-lockup.c
	../../gpu/memory/hostmem.c
	../../gpu/memory/memorycontext.c
	register-map.c
	lockup-history.c
	../../gpu/memory/heap.c
	../../gpu/memory/mmu.c
	../../gpu/memory/blockmgr.c
	cache_sce.c
	../../gpu/misc/waitmodobj.c
	device_sce.c
	suspend_sce.c
	../../gpu/lockup/registerdump.c
	../../gpu/lockup/registerdump_analysis.c
	client_pb.c
	client_render.c
	client_ccb.c
	sysconfig_sce.c
	handle-base.c
)

# Recovered bodies use the original explicit guard frames in their wrappers.
# Adding a second compiler guard changes observable native caller-frame words.
set_source_files_properties(
	handle-base.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector"
)
