include("${CMAKE_CURRENT_SOURCE_DIR}/fw/native_layout.cmake")
fw_value(IOFILEMGR_NATIVE_LINKER_SCRIPT IOFILEMGR_NATIVE_LINKER_SCRIPT)
psp2_module(os0_kd_iofilemgr KIND kernel SCE_LAYOUT SOURCES
	source/iofilemgr.c
	source/iofilemgr_for_kernel.c
	source/flock.c
	source/vfs.c
	source/vfs_subr.c
	source/rfs_table.c
	source/vfs_acl.c
	source/vfs_vnops.c
	source/vfs_buff.c
	source/vfs_path.c
	source/buffer_pool.c
	source/io_scheduler.c
	source/sched_dispatcher.c
	source/sched_io_request.c
	source/io_scheduler_async.c
	source/ioerror.c
	source/daemon.c
	source/dummy_stdio.c
	source/iofilemgr_for_user.c
	source/mount.c
	source/sched_scheduler.c
	source/sched_io_chunk.c
	source/sched_io_request_queue.c
	source/import_bindings.S
)
# Sony built the I/O scheduler sources with a signed char
set_source_files_properties(source/io_scheduler.c source/sched_dispatcher.c
	source/sched_io_request.c source/dummy_stdio.c
	PROPERTIES COMPILE_OPTIONS "-fsigned-char")
get_target_property(IOFILEMGR_LINK_OPTIONS os0_kd_iofilemgr LINK_OPTIONS)
list(FILTER IOFILEMGR_LINK_OPTIONS EXCLUDE REGEX "prx\\.ld$")
set_property(TARGET os0_kd_iofilemgr PROPERTY LINK_OPTIONS "${IOFILEMGR_LINK_OPTIONS}")
target_link_options(os0_kd_iofilemgr PRIVATE
	"-Wl,--no-fix-cortex-a8")
target_link_options(os0_kd_iofilemgr BEFORE PRIVATE
	"-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/fw/${IOFILEMGR_NATIVE_LINKER_SCRIPT}")
