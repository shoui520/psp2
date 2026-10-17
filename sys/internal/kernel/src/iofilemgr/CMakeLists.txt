include("${CMAKE_CURRENT_SOURCE_DIR}/fw/native_layout.cmake")
fw_value(IOFILEMGR_NATIVE_LINKER_SCRIPT IOFILEMGR_NATIVE_LINKER_SCRIPT)
psp2_module(os0_kd_iofilemgr KIND kernel COMPILER gcc48 SCE_LAYOUT SOURCES
	iofilemgr.c
	iofilemgr_for_kernel.c
	flock.c
	vfs.c
	vfs_subr.c
	rfs_table.c
	vfs_acl.c
	vfs_vnops.c
	vfs_buff.c
	vfs_path.c
	buffer_pool.c
	io_scheduler.c
	sched_dispatcher.c
	sched_io_request.c
	io_scheduler_async.c
	ioerror.c
	daemon.c
	dummy_stdio.c
	iofilemgr_for_user.c
	mount.c
	sched_scheduler.c
	sched_io_chunk.c
	sched_io_request_queue.c
	import_bindings.S
)
# Sony built the I/O scheduler sources with a signed char
set_source_files_properties(io_scheduler.c sched_dispatcher.c
	sched_io_request.c dummy_stdio.c
	PROPERTIES COMPILE_OPTIONS "-fsigned-char")
get_target_property(IOFILEMGR_LINK_OPTIONS os0_kd_iofilemgr LINK_OPTIONS)
list(FILTER IOFILEMGR_LINK_OPTIONS EXCLUDE REGEX "prx\\.ld$")
set_property(TARGET os0_kd_iofilemgr PROPERTY LINK_OPTIONS "${IOFILEMGR_LINK_OPTIONS}")
target_link_options(os0_kd_iofilemgr PRIVATE
	"-Wl,--no-fix-cortex-a8")
target_link_options(os0_kd_iofilemgr BEFORE PRIVATE
	"-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/fw/${IOFILEMGR_NATIVE_LINKER_SCRIPT}")
