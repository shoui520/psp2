enable_language(CXX)
# SceActivityDb (vs0:sys/external/activity_db.suprx), built by Sony with SNC 3.57 in C++ on paf
# and dbutil. No debug record names its files; they are ours, split by what each part does.
set(MODULE_IMPORT_NAMES vsh/paf/exports.yml vsh/app/common/src/dbutil/exports.yml
	vsh/app/common/src/np/activity/exports.yml vsh/app/common/src/np/np_manager/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	module.c
	activity_db_access.cpp activity_db_query.cpp activity_db_open.cpp
	activity_db_daemon.cpp activity_db_net.cpp activity_db_netqueue.cpp
	activity_db_sync.cpp activity_db_queue.cpp activity_db_npaccess.cpp
	activity_db_databases.cpp
	../../../../../vsh/app/common/src/np/np_activity_sdk/np_activity_utf8.cpp
	activity_db_post.cpp
	../../../../../vsh/app/common/src/np/np_activity_sdk/np_activity_ipc_client.cpp
	../../../../../vsh/app/common/src/np/np_community/np_community.c
	activity_db_init.c
	../../../../sdk/target/src/hmac/sha1.c
	../../../../sdk/target/src/hmac/libhmac.c
	../../../../sdk/target/src/sha1/libsha1.c
	../../../../sdk/target/src/libsnc/weak_calls.c
	../../../kernel/src/sysmem/sha1_transform.S
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf" "${PSP2_SRC}/contrib/sqlite-3.7.6.3" "${PSP2_SRC}/vsh/app/common/src"
	"${PSP2_SRC}/sys/internal/usermode/src")
# Sony linked with psp2ld --strip-unused
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
# The SDK's libSceHmac and libSceSha1, which Sony built with GCC and without stack guards
set_source_files_properties(
	../../../../sdk/target/src/hmac/sha1.c
	../../../../sdk/target/src/hmac/libhmac.c
	../../../../sdk/target/src/sha1/libsha1.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
# both of SNC's forwarding stubs
set_source_files_properties(../../../../sdk/target/src/libsnc/weak_calls.c
	PROPERTIES COMPILE_DEFINITIONS SNC_WEAK_CALLS_ON)
set_source_files_properties(../../../kernel/src/sysmem/sha1_transform.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
