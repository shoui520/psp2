enable_language(CXX)
# SceLibMtp (vs0:vsh/common/libmtp.suprx), built by Sony with SNC 3.57: the MTP responder the
# shell runs over USB and over the network (its content manager connection), in C, with one
# C++ file (bgcopy_service_manager.cpp, its only path Sony's strings give). The C is being
# written; until every function is C nothing links.
set(MODULE_IMPORT_NAMES vsh/paf/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	module.c
	mtp_core.cpp
	mtp_dev.c
	mtp_driver.c
	mtp_if.c
	mtp_init.cpp
	mtp_mini.cpp
	mtp_queue.cpp
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
	bgcopy_service_manager.cpp
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/sys/internal/usermode/src")
set_source_files_properties(mtp_queue.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=0)
