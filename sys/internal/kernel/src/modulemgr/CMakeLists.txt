psp2_module(os0_kd_modulemgr KIND kernel SOURCES
	source/backtrace_entry.S
	source/loadelf.c
	source/relocator.c
	source/modulemgr.c
	source/bootimage_fs.c
	source/modulecb.c
	source/libentcb.c
	source/libstubcb.c
	source/librarydb.c
	source/self_decrypter.c
	source/syscall_stub.c
	source/syscall.c
	source/loadcore.c
	source/stub_linker.c
	source/syscallcb.c
	source/exidx.c
	source/backtrace.c
)
# Sony built the backtracer for size
set_source_files_properties(source/backtrace.c PROPERTIES COMPILE_OPTIONS -Os)
# keeps the modern compiler from calling __aeabi_uidivmod for the table size
# checks; Sony's build divides inline and does not import it
set_source_files_properties(source/modulecb.c PROPERTIES COMPILE_OPTIONS -fno-guess-branch-probability)
# Sony's flags: reads through null pointers are kept as they are written, and
# calls stay calls (the debug asserts pass the caller's return address)
target_compile_options(os0_kd_modulemgr PRIVATE -fno-delete-null-pointer-checks
	-fno-optimize-sibling-calls)
