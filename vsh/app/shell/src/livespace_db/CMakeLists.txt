enable_language(CXX)
# SceLsdb (vs0:vsh/shell/livespace_db.suprx), built by Sony with SNC 3.57 in C++ on paf. No debug
# record names its files; they are split where each file's static initializer of the shared
# parameter keys ends it (ours).
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml vsh/app/common/src/dbutil/exports.yml
	vsh/app/common/src/sqlite/exports.yml sys/internal/usermode/src/bXCe/exports.yml
	sys/internal/usermode/src/ini_file_processor/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	appinfo_sclient.cpp
	appinfo_sclient_impl.cpp
	appinfo_daemon.cpp
	appdb.cpp
	appinfo_listener.cpp
	appinfo_qclient.cpp
	appinfo_qclient_impl.cpp
	appinfo_daemon_api.cpp
	appinfo_accessor.cpp
	appinfo_util.cpp
	lsdb_database.cpp
	dbblob.cpp
	newevent.cpp
	livearea_template.cpp
	livearea_parser.cpp
	livearea_filter.cpp
	livearea_api.cpp
	lidaemon.cpp
	livearea_model.cpp
	title_listener.cpp
	xml_element.cpp
	xml_tree.cpp
	iconlayout.cpp
	module.c
	theme_param.cpp
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
	"${PSP2_SRC}/contrib/sqlite-3.7.6.3" "${PSP2_SRC}/vsh/app/common/src"
	"${PSP2_SRC}/sys/internal/usermode/src")
# Sony linked with psp2ld --strip-unused
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
# Sony's build inlines paf::vector's fill constructor in appinfo_daemon.cpp (CreateFolder's four
# one-int vectors), and calls it everywhere else
set_source_files_properties(appinfo_daemon.cpp PROPERTIES COMPILE_DEFINITIONS PAF_VECTOR_FILL_INLINE)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
