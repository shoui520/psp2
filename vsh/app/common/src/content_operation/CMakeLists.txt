enable_language(CXX)
# SceContentOperation (vs0:vsh/common/content_operation.suprx), built by Sony with SNC 3.57 in C++
# on paf and dbutil (3.20's paths: vsh/app/common/src/content_operation/../../../../paf/include).
# No debug record names its file; the file is named by the module (ours). SNC's weak forwarder
# links after it, as in mail_api_for_local.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml vsh/app/common/src/dbutil/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	content_operation.cpp
	${PSP2_SRC}/sys/sdk/target/src/libsnc/weak_calls.c
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf" "${PSP2_SRC}/contrib/sqlite-3.7.6.3" "${PSP2_SRC}/vsh/app/common/src")
# Sony linked with psp2ld --strip-unused
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
