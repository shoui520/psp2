# SceLibKernel: one implementation with firmware-selected interface tables.
include(../../../common/runtime/gcc48/unwind.cmake)
set(libkernel_unwind_source "${CMAKE_CURRENT_BINARY_DIR}/gcc48-unwind.c")
gcc48_prepare_unwind(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor/gcc-linaro-4.8-2014.04/libgcc/config/arm/unwind-arm.c"
	"${libkernel_unwind_source}")
include(source/heap.cmake)
set(libkernel_heap_source "${CMAKE_CURRENT_BINARY_DIR}/dlmalloc-sce.c")
libkernel_prepare_heap(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor/dlmalloc-2.8.3/malloc.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/source/heap_sce.patch"
	"${libkernel_heap_source}")

psp2_module(os0_us_libkernel KIND user SOURCES
	"${libkernel_unwind_source}"
	"${libkernel_heap_source}"
	source/format.c
	source/format_wrappers.c
	source/thread_context.c
	source/cpu_context.S
	source/change_stack.S
	source/stack_fail.S
	source/thread_objects.c
	source/thread_status.c
	source/events.c
	source/timers.c
	source/msgpipe.c
	source/msgpipe_vectors.c
	source/lifecycle.c
	source/atomics.c
	source/atomics64.c
	source/atomics_small.c
	source/process_clock.c
	source/backtrace.c
	source/backtrace_print.c
	source/format_checked.c
	source/backtrace_exidx.c
	source/backtrace_walk.c
	source/backtrace_capture.S
	source/thread_create.c
	source/thread_waits.c
	source/thread_misc.c
	source/vm.c
	source/system_identity.c
	source/save_context.c
	source/auth_requests.c
	source/adhoc_cipher.c
	source/lw_create.c
	source/lw_sync.c
	source/lw_mutex.c
	source/lw_mutex_arm.c
	source/modules.c
	source/library_lookup.c
	source/io_transfer.c
	source/io_paths.c
	source/clib_memory.c
	source/clib_strings.c
	source/clib_case.c
	source/clib_abort.c
	source/heap.c
	source/heap_destroy.c
	source/heap_inspect.c
	source/format_io.c
	../../../common/sysclib/fill.c
	../../../common/sysclib/compare.c
	../../../common/sysclib/copy_forward.S
	../../../common/sysclib/copy.c
	../../../common/sysclib/move.c
	../../../common/sysclib/ctype.c
	../../../common/sysclib/strings.c
	../../../common/sysclib/bsd_strings.c
	../../../common/sysclib/bsd_strtoll.c
	../../../common/sysclib/search.c
	../../../common/sysclib/checked_strings.c
	../../../vendor/gcc-linaro-4.8-2014.04/libgcc/config/arm/lib1funcs.S
	../../../vendor/gcc-linaro-4.8-2014.04/libgcc/config/arm/libunwind.S
	../../../common/runtime/gcc48/mul.c
	../../../common/runtime/gcc48/f2lz.c
	../../../common/runtime/gcc48/f2ulz.c
	../../../common/runtime/gcc48/d2lz.c
	../../../common/runtime/gcc48/d2ulz.c
	../../../common/runtime/gcc48/udiv.S
	../../../common/runtime/gcc48/idiv.S
	../../../common/runtime/gcc48/div64.c
	../../../common/runtime/gcc48/udiv64.c
	../../../common/runtime/gcc48/unwind_pr.c
	../../../common/runtime/gcc48/ldivmod.S
	../../../common/runtime/gcc48/uldivmod.S
	../../../vendor/gcc-linaro-4.8-2014.04/libgcc/config/arm/bpabi.c
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
	"${CMAKE_CURRENT_SOURCE_DIR}/source"
	../../../common/runtime/gcc48
	../../../common/runtime/gcc48/target
	../../../vendor/gcc-linaro-4.8-2014.04/gcc
	../../../vendor/gcc-linaro-4.8-2014.04/gcc/ginclude
	../../../vendor/gcc-linaro-4.8-2014.04/libgcc
	../../../vendor/gcc-linaro-4.8-2014.04/include)

set_source_files_properties(../../../common/runtime/gcc48/mul.c
	../../../common/runtime/gcc48/f2lz.c ../../../common/runtime/gcc48/f2ulz.c
	../../../common/runtime/gcc48/d2lz.c ../../../common/runtime/gcc48/d2ulz.c
	../../../common/runtime/gcc48/div64.c ../../../common/runtime/gcc48/udiv64.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc"
	COMPILE_DEFINITIONS "IN_LIBGCC2")

set_source_files_properties(../../../common/runtime/gcc48/unwind_pr.c PROPERTIES
	COMPILE_OPTIONS "-fno-inline;-fno-stack-protector"
	COMPILE_DEFINITIONS
	"_Unwind_GetRegionStart=SceLibGcc_DA5097CE;_Unwind_GetLanguageSpecificData=SceLibGcc_BAC00FF7;_Unwind_GetDataRelBase=SceLibGcc_AC15DBA5;_Unwind_GetTextRelBase=SceLibGcc_8D4953C7;_Unwind_VRS_Get=SceLibGcc_0DFF2B2C;_Unwind_VRS_Set=SceLibGcc_DAB28374;_Unwind_VRS_Pop=SceLibGcc_F16E32FC")

set_source_files_properties("${libkernel_unwind_source}" PROPERTIES
	COMPILE_OPTIONS "-std=gnu99;-fno-inline;-fno-stack-protector"
	COMPILE_DEFINITIONS
	"_Unwind_VRS_Get=SceLibGcc_0DFF2B2C;_Unwind_VRS_Set=SceLibGcc_DAB28374;_Unwind_VRS_Pop=SceLibGcc_F16E32FC;_Unwind_Complete=SceLibGcc_8A5F29D8;_Unwind_DeleteException=SceLibGcc_4BB45B70;_Unwind_GetCFA=SceLibGcc_DBE840D6")

# These syscall marshalling wrappers have no strong stack guards in the original.
set_source_files_properties(source/lw_mutex_arm.c PROPERTIES
	COMPILE_OPTIONS "-marm;-fno-inline;-fno-stack-protector")

set_source_files_properties(source/backtrace_exidx.c PROPERTIES
	COMPILE_OPTIONS "-fno-inline")

set_source_files_properties(source/thread_objects.c source/thread_status.c
	source/events.c source/timers.c source/msgpipe_vectors.c source/backtrace.c
	source/backtrace_print.c
	source/thread_waits.c source/lw_sync.c source/modules.c source/io_transfer.c
	source/io_paths.c source/thread_misc.c
	PROPERTIES
	COMPILE_OPTIONS "-fstack-protector")

# This core is itself exported by LibKernel, rather than reached via a wrapper.
set_source_files_properties(../../../common/sysclib/copy_forward.S
	../../../common/sysclib/copy.c
	../../../common/sysclib/move.c PROPERTIES
	COMPILE_DEFINITIONS "CopyMemoryForward=sceClibMemcpy")

set_source_files_properties(../../../common/sysclib/ctype.c PROPERTIES
	COMPILE_DEFINITIONS
	"look_ctype_table=sceClibLookCtypeTable;tolower=sceClibTolower;toupper=sceClibToupper")

set_source_files_properties(../../../common/sysclib/compare.c PROPERTIES
	COMPILE_DEFINITIONS "timingsafe_memcmp=sceClibMemcmpConstTime")

set_property(SOURCE ../../../common/sysclib/strings.c PROPERTY
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

set_source_files_properties(../../../common/sysclib/bsd_strings.c PROPERTIES
	COMPILE_DEFINITIONS "strlcpy=sceClibStrlcpy")

set_source_files_properties(../../../common/sysclib/bsd_strtoll.c PROPERTIES
	COMPILE_OPTIONS "-fsigned-char"
	COMPILE_DEFINITIONS "strtoll=sceClibStrtoll")

set_source_files_properties(../../../common/sysclib/search.c PROPERTIES
	COMPILE_DEFINITIONS
	"strstr=sceClibStrstr;strchr=sceClibStrchr;memcmp=sceClibMemcmp")

set_property(SOURCE ../../../common/sysclib/checked_strings.c PROPERTY
	COMPILE_DEFINITIONS
	strncat=sceClibStrncat strnlen=sceClibStrnlen
	strncpy=sceClibStrncpy memcpy=sceClibMemcpy_safe
	__strncpy_chk2=sceClibStrcpyChk __strncpy_chk=sceClibStrncpyChk
	__strncat_chk2=sceClibStrcatChk __strncat_chk=sceClibStrncatChk)

# Pristine compiler ABI helpers; select only the native shifts and comparisons.
set_source_files_properties(
	../../../vendor/gcc-linaro-4.8-2014.04/libgcc/config/arm/lib1funcs.S PROPERTIES
	COMPILE_DEFINITIONS
	"L_lshrdi3;L_ashrdi3;L_ashldi3;L_aeabi_lcmp;L_aeabi_ulcmp;L_dvmd_tls")
