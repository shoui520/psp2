# os0:kd/bootimage/ulobjmgr - kernel support for libult's user-level objects
psp2_module(os0_kd_bootimage_ulobjmgr KIND kernel SOURCES
	source/ulobjmgr.c
	source/instance.c
	source/info_array.c
	source/api.c
	source/ult_list.c
	source/ult_sync_obj.c
	source/driver_api.c
	source/driver_api_for_coredump.c
	source/driver_api_for_ult_sync_obj.c
)
# Sony built this module with -O3 (vectorised loops, loop->memset, unswitching)
target_compile_options(os0_kd_bootimage_ulobjmgr PRIVATE -O3 -fno-optimize-sibling-calls
	-fno-delete-null-pointer-checks)
