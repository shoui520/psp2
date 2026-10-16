# SceSqliteVsh: configured pristine SQLite plus Sony's platform layer.
set(sqlite_upstream "${PSP2_SRC}/contrib/sqlite-3.7.6.3/upstream/sqlite-src-3070603")
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
		"${CMAKE_CURRENT_SOURCE_DIR}/source/mutex-appdef.patch"
	COMMAND "${sqlite_native_patch}" sqlite3.c
		"${CMAKE_CURRENT_SOURCE_DIR}/source/journal-alignment.patch"
	WORKING_DIRECTORY "${sqlite_generated}"
	DEPENDS "${sqlite_upstream}/src/parse.y" "${sqlite_upstream}/tool/mkkeywordhash.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
		"${CMAKE_CURRENT_SOURCE_DIR}/source/mutex-appdef.patch"
		"${CMAKE_CURRENT_SOURCE_DIR}/source/journal-alignment.patch"
	VERBATIM)
psp2_module(vs0_sys_external_sqlite KIND user SOURCES
	source/module.c
	source/sqlite3.c
	source/os_psp2.c
	../../../../../common/sqlite/localtime.c
	../../../../../common/sqlite/vfs_time.c
	../../../../../common/runtime/i32ops.c
	../../../../../common/runtime/i64ops.c
	../../../../../common/runtime/aeabi_ldivmod.S
	../../../../../common/runtime/unwind_pr0.c
	../../../../../common/runtime/unwind_pr1.c
)
add_custom_target(vs0_sys_external_sqlite_core
	DEPENDS "${sqlite_generated}/sqlite3.c")
add_dependencies(vs0_sys_external_sqlite vs0_sys_external_sqlite_core)
target_include_directories(vs0_sys_external_sqlite PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
set_source_files_properties("../../../../../common/sqlite/vfs_time.c" PROPERTIES
	COMPILE_DEFINITIONS "memset=sce_paf_memset;memcpy=sce_paf_memcpy")
