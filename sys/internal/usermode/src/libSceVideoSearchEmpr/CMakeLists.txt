# SceVideoSearchEmpr (vs0:sys/external/libSceVideoSearchEmpr.suprx):
# Video search wrapper over SQLite 3.7.6.3 (same configuration and PSP2 VFS as SceSqliteVsh)
# using sceClibMspace instead of libpaf, plus libsnc runtime helpers.
set(sqlite_upstream "${PSP2_SRC}/contrib/sqlite-3.7.6.3/upstream/sqlite-src-3070603")
set(sqlite_vsh_dir "${PSP2_SRC}/vsh/app/common/src/sqlite")
set(sqlite_generated "${CMAKE_CURRENT_BINARY_DIR}/sqlite-config")
file(MAKE_DIRECTORY "${sqlite_generated}")
find_program(sqlite_native_make NAMES gmake make REQUIRED)
find_program(sqlite_native_cc NAMES cc clang REQUIRED)
find_program(sqlite_native_patch NAMES patch REQUIRED)

set(sqlite_keyword_options "-DSQLITE_OMIT_ANALYZE -DSQLITE_OMIT_REINDEX -DSQLITE_OMIT_VACUUM -DSQLITE_OMIT_AUTOVACUUM -DSQLITE_OMIT_WAL -DSQLITE_OMIT_VIRTUALTABLE -DSQLITE_OMIT_EXPLAIN")
set(sqlite_parser_options "${sqlite_keyword_options} -DSQLITE_ENABLE_UPDATE_DELETE_LIMIT")
add_custom_command(OUTPUT "${sqlite_generated}/sqlite3.c"
	COMMAND "${CMAKE_COMMAND}" -E env "CC=${sqlite_native_cc}"
		"${sqlite_upstream}/configure" --disable-shared --disable-readline
	COMMAND "${CMAKE_COMMAND}" -E rm -f keywordhash.h parse.c parse.h
		opcodes.c opcodes.h sqlite3.c
	COMMAND "${sqlite_native_make}" keywordhash.h "OPTS=${sqlite_keyword_options}"
	COMMAND "${sqlite_native_make}" sqlite3.c "OPTS=${sqlite_parser_options}"
	COMMAND "${sqlite_native_patch}" sqlite3.c
		"${sqlite_vsh_dir}/mutex-appdef.patch"
	COMMAND "${sqlite_native_patch}" sqlite3.c
		"${sqlite_vsh_dir}/journal-alignment.patch"
	COMMAND "${sqlite_native_patch}" sqlite3.c
		"${sqlite_vsh_dir}/lookaside-default.patch"
	COMMAND "${sqlite_native_patch}" sqlite3.c
		"${sqlite_vsh_dir}/open-flags-mask.patch"
	COMMAND "${sqlite_native_patch}" sqlite3.c
		"${sqlite_vsh_dir}/line-offsets.patch"
	WORKING_DIRECTORY "${sqlite_generated}"
	DEPENDS "${sqlite_upstream}/src/parse.y" "${sqlite_upstream}/tool/mkkeywordhash.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
		"${sqlite_vsh_dir}/mutex-appdef.patch"
		"${sqlite_vsh_dir}/journal-alignment.patch"
		"${sqlite_vsh_dir}/lookaside-default.patch"
		"${sqlite_vsh_dir}/open-flags-mask.patch"
		"${sqlite_vsh_dir}/line-offsets.patch"
	VERBATIM)

psp2_module(vs0_sys_external_libSceVideoSearchEmpr KIND user COMPILER snc SOURCES
	video_search_empr.c
	vse_sqlite.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i64mod.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
)
add_custom_target(vs0_sys_external_libSceVideoSearchEmpr_core
	DEPENDS "${sqlite_generated}/sqlite3.c")
add_dependencies(vs0_sys_external_libSceVideoSearchEmpr vs0_sys_external_libSceVideoSearchEmpr_core)
target_include_directories(vs0_sys_external_libSceVideoSearchEmpr PRIVATE
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${PSP2_SRC}/contrib/sqlite-3.7.6.3"
	"${sqlite_vsh_dir}"
	"${PSP2_SRC}/vsh/paf/include")
target_compile_definitions(vs0_sys_external_libSceVideoSearchEmpr PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libSceVideoSearchEmpr PRIVATE -Wl,--gc-sections)
