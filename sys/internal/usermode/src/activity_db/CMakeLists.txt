enable_language(CXX)
# SceActivityDb (vs0:sys/external/activity_db.suprx), built by Sony with SNC 3.57 in C++ on paf
# and dbutil. No debug record names its files; they are ours, split by what each part does.
set(MODULE_IMPORT_NAMES vsh/paf/exports.yml vsh/app/common/src/dbutil/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	module.c
	activity_db_access.cpp activity_db_query.cpp activity_db_open.cpp
	activity_db_daemon.cpp
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf" "${PSP2_SRC}/contrib/sqlite-3.7.6.3" "${PSP2_SRC}/vsh/app/common/src"
	"${PSP2_SRC}/sys/internal/usermode/src")
# Sony linked with psp2ld --strip-unused
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
