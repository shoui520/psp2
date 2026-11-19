enable_language(CXX)
# SceLibMtp (vs0:vsh/common/libmtp.suprx), built by Sony with SNC 3.57: the MTP responder the
# shell runs over USB and over the network (its content manager connection), in C and C++, with
# the libraries it links statically (libSceCes, libSceMd5, libsnc, fs_util, console_info, the
# host collabo Wake-on-LAN sender and SceSblGcAuthMgr's stub).
set(MODULE_IMPORT_NAMES vsh/paf/exports.yml vsh/app/common/src/sqlite/exports.yml
	sys/internal/usermode/src/bXCe/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	module.c
	mtp_core.cpp
	mtp_dev.c
	mtp_driver.c
	mtp_if.c
	mtp_init.cpp
	mtp_mini.cpp
	mtp_queue.cpp
	mtp_task.cpp
	mtp_spec.c
	mtp_usb_charge.c
	mtp_usb_monitor.c
	mtp_util.cpp
	mtp_xfer.c
	mtp_xml_allocator.cpp
	mtp_netmtp_device.c
	mtp_netmtp_responder.c
	mtp_netmtp_util.c
	mtp_netmtp_thread.c
	mtp_callback.c
	mtp_cmd.cpp
	mtp_context.cpp
	mtp_context_manager.cpp
	mtp_file.c
	mtp_http.cpp
	mtp_netmtp_if.c
	mtp_prop.c
	mtp_registry.c
	mtp_http_parser.c
	mtp_ip.c
	mtp_base.cpp
	mtp_bgcopy.cpp
	mtp_bgcopy_if.cpp
	bgcopy_mtp_launcher.cpp
	mtp_capability.cpp
	mtp_operation.cpp
	mtp_cmd_ext.cpp
	mtp_marlin.cpp
	mtp_npdrm.cpp
	mtp_obj.cpp
	mtp_system_setting.cpp
	mtp_storage.cpp
	mtp_net_operation.cpp
	bgcopy_service_manager.cpp
	mtp_cache.cpp
	mtp_db.cpp
	mtp_db_sqlite.c
	mtp_host.c
	mtp_objmap.cpp
	mtp_sql.cpp
	../../../../../../sys/internal/usermode/src/fsutil/fs_util.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/ces_ucs_context.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/Str_UTF16_UTF8.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/Str_UTF8_Copy.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/Str_UTF8_UTF16.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF16_UTF8.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF16BE_UTF8.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF8.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/ces_context.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/ces_ucs_callback.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF16.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF16BE.c
	../../console_info/console_info.cpp
	../../console_info/console_info_file.cpp
	../../host_collabo/util/wake_on_lan.cpp
	../../../../../../sys/sdk/target/src/libsnc/i64ops.c
	../../../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
	../../../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../../../sys/sdk/target/src/md5/libmd5.c
	../../../../../../sys/sdk/target/src/md5/md5.S
	../../../../../../sys/sdk/target/src/libSceSblGcAuthMgr_stub/gc_auth_mgr_stub.c
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/sys/internal/usermode/src" "${PSP2_SRC}/contrib/sqlite-3.7.6.3"
	"${PSP2_SRC}/vsh/paf" "${PSP2_SRC}/vsh/app/common/src" "${PSP2_SRC}/sys/sdk/target/src")
set_source_files_properties(mtp_queue.cpp mtp_task.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=0)
# libSceCes's files, with libpaf's inlining budget for them
set_source_files_properties(
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/ces_ucs_context.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/Str_UTF16_UTF8.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/Str_UTF8_Copy.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/Str_UTF8_UTF16.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF16_UTF8.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF16BE_UTF8.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF8.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/ces_context.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/ces_ucs_callback.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF16.c
	../../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF16BE.c
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=40)
# Sony linked with psp2ld --strip-unused, which drops what nothing calls (libces's and libsnc's
# unused functions, the inline members SNC also emits out of line): each SNC function in its own
# section, and the link collects the unreferenced ones.
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
# libSceMd5, which Sony built with GCC 4.8 and without stack guards
set_source_files_properties(../../../../../../sys/sdk/target/src/md5/libmd5.c PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48 COMPILE_OPTIONS -fno-stack-protector)
