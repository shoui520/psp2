# SceLibKernel: one implementation with firmware-selected interface tables.
include(../../../../../toolchains/libgcc/unwind.cmake)
set(libkernel_unwind_source "${CMAKE_CURRENT_BINARY_DIR}/gcc48-unwind.c")
gcc48_prepare_unwind(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05/libgcc/config/arm/unwind-arm.c"
	"${libkernel_unwind_source}")
include(heap.cmake)
set(libkernel_heap_source "${CMAKE_CURRENT_BINARY_DIR}/dlmalloc-sce.c")
libkernel_prepare_heap(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/dlmalloc-2.8.3/malloc.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/heap_sce.patch"
	"${libkernel_heap_source}")

psp2_module(os0_us_libkernel KIND user SOURCES
	"${libkernel_unwind_source}"
	"${libkernel_heap_source}"
	format.c
	format_wrappers.c
	thread_context.c
	cpu_context.S
	change_stack.S
	stack_fail.S
	thread_objects.c
	thread_status.c
	events.c
	timers.c
	msgpipe.c
	msgpipe_vectors.c
	lifecycle.c
	atomics.c
	atomics64.c
	atomics_small.c
	process_clock.c
	backtrace.c
	backtrace_print.c
	format_checked.c
	backtrace_exidx.c
	backtrace_walk.c
	backtrace_capture.S
	thread_create.c
	thread_waits.c
	thread_misc.c
	vm.c
	system_identity.c
	save_context.c
	auth_requests.c
	adhoc_cipher.c
	lw_create.c
	lw_sync.c
	lw_mutex.c
	lw_mutex_arm.c
	modules.c
	library_lookup.c
	io_transfer.c
	io_paths.c
	clib_memory.c
	clib_strings.c
	clib_case.c
	clib_abort.c
	heap.c
	heap_destroy.c
	heap_inspect.c
	format_io.c
	../../../kernel/src/sysmem/fill_core.S
	../../../kernel/src/sysmem/fill.c
	../../../kernel/src/sysmem/compare_core.S
	../../../kernel/src/sysmem/compare.c
	../../../kernel/src/sysmem/copy_forward.S
	../../../kernel/src/sysmem/copy.c
	../../../kernel/src/sysmem/move.S
	../../../kernel/src/sysmem/ctype.c
	../../../kernel/src/sysmem/strings.c
	../../../kernel/src/sysmem/bsd_strings.c
	../../../kernel/src/sysmem/bsd_strtoll.c
	../../../kernel/src/sysmem/search.c
	../../../kernel/src/sysmem/checked_strings.c
	../../../../../contrib/gcc-4.8-2014.05/libgcc/config/arm/lib1funcs.S
	../../../../../contrib/gcc-4.8-2014.05/libgcc/config/arm/libunwind.S
	../../../../../toolchains/libgcc/mul.c
	../../../../../toolchains/libgcc/f2lz.c
	../../../../../toolchains/libgcc/f2ulz.c
	../../../../../toolchains/libgcc/d2lz.c
	../../../../../toolchains/libgcc/d2ulz.c
	../../../../../toolchains/libgcc/udiv.S
	../../../../../toolchains/libgcc/idiv.S
	../../../../../toolchains/libgcc/div64.c
	../../../../../toolchains/libgcc/udiv64.c
	../../../../../toolchains/libgcc/unwind_pr.c
	../../../../../toolchains/libgcc/ldivmod.S
	../../../../../toolchains/libgcc/uldivmod.S
	../../../../../contrib/gcc-4.8-2014.05/libgcc/config/arm/bpabi.c
)

# User backtraces and the local C++ ABI unwinder consume these generated tables.
target_compile_options(os0_us_libkernel PRIVATE
	"$<$<COMPILE_LANGUAGE:C>:-funwind-tables>")
add_custom_command(OUTPUT os0_us_libkernel.velf APPEND
	COMMAND python3 "${PSP2_SRC}/tools/fixup_unwind.py"
		$<TARGET_FILE:os0_us_libkernel> os0_us_libkernel.velf
	DEPENDS "${PSP2_SRC}/tools/fixup_unwind.py")

# Structure copies emitted by GCC use LibKernel's own native memory engine.
target_link_options(os0_us_libkernel PRIVATE
	"-Wl,--defsym=memcpy=sceClibMemcpy")

target_include_directories(os0_us_libkernel PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	../../../../../toolchains/libgcc
	../../../../../toolchains/libgcc/target
	../../../../../contrib/gcc-4.8-2014.05/gcc
	../../../../../contrib/gcc-4.8-2014.05/gcc/ginclude
	../../../../../contrib/gcc-4.8-2014.05/libgcc
	../../../../../contrib/gcc-4.8-2014.05/include)

set_source_files_properties(../../../../../toolchains/libgcc/mul.c
	../../../../../toolchains/libgcc/f2lz.c ../../../../../toolchains/libgcc/f2ulz.c
	../../../../../toolchains/libgcc/d2lz.c ../../../../../toolchains/libgcc/d2ulz.c
	../../../../../toolchains/libgcc/div64.c ../../../../../toolchains/libgcc/udiv64.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc"
	COMPILE_DEFINITIONS "IN_LIBGCC2")

set_source_files_properties(../../../../../toolchains/libgcc/unwind_pr.c PROPERTIES
	COMPILE_OPTIONS "-fno-inline;-fno-stack-protector"
	COMPILE_DEFINITIONS
	"_Unwind_GetRegionStart=SceLibGcc_DA5097CE;_Unwind_GetLanguageSpecificData=SceLibGcc_BAC00FF7;_Unwind_GetDataRelBase=SceLibGcc_AC15DBA5;_Unwind_GetTextRelBase=SceLibGcc_8D4953C7;_Unwind_VRS_Get=SceLibGcc_0DFF2B2C;_Unwind_VRS_Set=SceLibGcc_DAB28374;_Unwind_VRS_Pop=SceLibGcc_F16E32FC")

set_source_files_properties("${libkernel_unwind_source}" PROPERTIES
	COMPILE_OPTIONS "-std=gnu99;-fno-inline;-fno-stack-protector"
	COMPILE_DEFINITIONS
	"_Unwind_VRS_Get=SceLibGcc_0DFF2B2C;_Unwind_VRS_Set=SceLibGcc_DAB28374;_Unwind_VRS_Pop=SceLibGcc_F16E32FC;_Unwind_Complete=SceLibGcc_8A5F29D8;_Unwind_DeleteException=SceLibGcc_4BB45B70;_Unwind_GetCFA=SceLibGcc_DBE840D6")

# These syscall marshalling wrappers have no strong stack guards in the original.
set_source_files_properties(lw_mutex_arm.c PROPERTIES
	COMPILE_OPTIONS "-marm;-fno-inline;-fno-stack-protector")

set_source_files_properties(backtrace_exidx.c PROPERTIES
	COMPILE_OPTIONS "-fno-inline")

set_source_files_properties(thread_objects.c thread_status.c
	events.c timers.c msgpipe_vectors.c backtrace.c
	backtrace_print.c
	thread_waits.c lw_sync.c modules.c io_transfer.c
	io_paths.c thread_misc.c
	PROPERTIES
	COMPILE_OPTIONS "-fstack-protector")

# This core is itself exported by LibKernel, rather than reached via a wrapper.
set_source_files_properties(../../../kernel/src/sysmem/copy_forward.S
	../../../kernel/src/sysmem/copy.c
	../../../kernel/src/sysmem/move.S PROPERTIES
	COMPILE_DEFINITIONS "CopyMemoryForward=sceClibMemcpy")

set_source_files_properties(../../../kernel/src/sysmem/ctype.c PROPERTIES
	COMPILE_DEFINITIONS
	"look_ctype_table=sceClibLookCtypeTable;tolower=sceClibTolower;toupper=sceClibToupper")

set_source_files_properties(../../../kernel/src/sysmem/compare.c PROPERTIES
	COMPILE_DEFINITIONS "timingsafe_memcmp=sceClibMemcmpConstTime")

set_property(SOURCE ../../../kernel/src/sysmem/strings.c PROPERTY
	COMPILE_DEFINITIONS
	SYSCLIB_SELECT_STRINGS
	SYSCLIB_WANT_MEMCHR memchr=sceClibMemchr
	SYSCLIB_WANT_STRCHR strchr=sceClibStrchr
	SYSCLIB_WANT_STRNLEN strnlen=sceClibStrnlen
	SYSCLIB_WANT_STRCMP strcmp=sceClibStrcmp
	SYSCLIB_WANT_STRNCMP strncmp=sceClibStrncmp
	SYSCLIB_WANT_STRRCHR strrchr=sceClibStrrchr
	SYSCLIB_WANT_STRNCPY strncpy=sceClibStrncpy memset=sceClibMemset
	SYSCLIB_WANT_STRLEN
	SYSCLIB_WANT_STRLCAT strlcat=sceClibStrlcat)

set_source_files_properties(../../../kernel/src/sysmem/bsd_strings.c PROPERTIES
	COMPILE_DEFINITIONS "strlcpy=sceClibStrlcpy")

set_source_files_properties(../../../kernel/src/sysmem/bsd_strtoll.c PROPERTIES
	COMPILE_OPTIONS "-fsigned-char"
	COMPILE_DEFINITIONS "strtoll=sceClibStrtoll")

set_source_files_properties(../../../kernel/src/sysmem/search.c PROPERTIES
	COMPILE_DEFINITIONS
	"strstr=sceClibStrstr;strchr=sceClibStrchr;memcmp=sceClibMemcmp")

set_property(SOURCE ../../../kernel/src/sysmem/checked_strings.c PROPERTY
	COMPILE_DEFINITIONS
	strncat=sceClibStrncat strnlen=sceClibStrnlen
	strncpy=sceClibStrncpy memcpy=sceClibMemcpy_safe
	__strncpy_chk2=sceClibStrcpyChk __strncpy_chk=sceClibStrncpyChk
	__strncat_chk2=sceClibStrcatChk __strncat_chk=sceClibStrncatChk)

# Pristine compiler ABI helpers; select only the native shifts and comparisons.
set_source_files_properties(
	../../../../../contrib/gcc-4.8-2014.05/libgcc/config/arm/lib1funcs.S PROPERTIES
	COMPILE_DEFINITIONS
	"L_lshrdi3;L_ashrdi3;L_ashldi3;L_aeabi_lcmp;L_aeabi_ulcmp;L_dvmd_tls")
