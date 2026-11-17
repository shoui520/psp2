enable_language(CXX)
# SceLsdb (vs0:vsh/shell/livespace_db.suprx), built by Sony with SNC 3.57 in C++ on paf. No debug
# record names its files; they are split where each file's static initializer of the shared
# parameter keys ends it (ours).
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml vsh/app/common/src/dbutil/exports.yml
	vsh/app/common/src/sqlite/exports.yml sys/internal/usermode/src/bXCe/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
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
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
	"${PSP2_SRC}/contrib/sqlite-3.7.6.3" "${PSP2_SRC}/vsh/app/common/src"
	"${PSP2_SRC}/sys/internal/usermode/src")
# Sony linked with psp2ld --strip-unused
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
