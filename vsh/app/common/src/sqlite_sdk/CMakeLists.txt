# SceSqlite (vs0:sys/external/libSceSqlite.suprx): SQLite 3.6.23.1 from the
# vendored amalgamation (sqlite3.c sets Sony's options), Sony's OS
# layer (os_psp2.c) and the C library pieces it runs on (libc_psp2.c), plus the
# parts of the SNC runtime (libsnc) the original links in.
psp2_module(vs0_sys_external_libSceSqlite KIND user SOURCES
	os_psp2.c
	libc_psp2.c
	sqlite3.c
	../sqlite/localtime.c
	../sqlite/vfs_time.c
	../../../../../common/runtime/i32ops.c
	../../../../../common/runtime/i64ops.c
	../../../../../common/runtime/aeabi_ldivmod.S
	../../../../../common/runtime/unwind_pr0.c
	../../../../../common/runtime/unwind_pr1.c
)

if(TARGET vs0_sys_external_libSceSqlite)
	# the module supplies its own memset/strlen/printf/malloc/atoi
	target_compile_options(vs0_sys_external_libSceSqlite PRIVATE -fno-builtin)
endif()
