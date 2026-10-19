psp2_module(os0_kd_modulemgr KIND kernel COMPILER gcc48 SOURCES
	backtrace_entry.S
	loadelf.c
	relocator.c
	modulemgr.c
	bootimage_fs.c
	modulecb.c
	libentcb.c
	libstubcb.c
	librarydb.c
	self_decrypter.c
	syscall_stub.c
	modulemgr_for_user.c
	loadcore.c
	stub_linker.c
	syscallcb.c
	exidx.c
	backtrace.c
)
# Sony built the backtracer for size
set_source_files_properties(backtrace.c PROPERTIES COMPILE_OPTIONS -Os)
# keeps the modern compiler from calling __aeabi_uidivmod for the table size
# checks; Sony's build divides inline and does not import it
set_source_files_properties(modulecb.c PROPERTIES COMPILE_OPTIONS -fno-guess-branch-probability)
# Sony's flags: reads through null pointers are kept as they are written, and
# calls stay calls (the debug asserts pass the caller's return address)
target_compile_options(os0_kd_modulemgr PRIVATE -fno-delete-null-pointer-checks
	-fno-optimize-sibling-calls)
