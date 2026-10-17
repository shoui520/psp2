# os0:kd/bootimage/ulobjmgr - kernel support for libult's user-level objects
psp2_module(os0_kd_bootimage_ulobjmgr KIND kernel COMPILER gcc48 SOURCES
	ulobjmgr.c
	instance.c
	info_array.c
	api.c
	ult_list.c
	ult_sync_obj.c
	driver_api.c
	driver_api_for_coredump.c
	driver_api_for_ult_sync_obj.c
)
# Sony built this module with -O3 (vectorised loops, loop->memset, unswitching)
target_compile_options(os0_kd_bootimage_ulobjmgr PRIVATE -O3 -fno-optimize-sibling-calls
	-fno-delete-null-pointer-checks)
