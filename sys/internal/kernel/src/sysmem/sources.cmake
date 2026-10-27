# SceSysmem's sources and their settings, shared by os0's module (this folder) and the
# secure world's (tzs/). The includer sets sysmem_dir to this folder; sysmem_sources
# lists the sources in link order, sysmem_settings(<target>) applies the per-file and
# per-target settings.
include(${sysmem_dir}/vendor_patch.cmake)
# Sony built SceSysmem without -fno-delete-null-pointer-checks: a NULL test after a
# dereference is dropped (dlmalloc's malloc.c, partition.c's free-list helpers).
# And with -fuse-caller-save (CodeSourcery's, which the toolchain turns off by default): the
# 3.6x ASLockRangeWithMode keeps its va in r2 across the call to ASLockUnlockModeCheck,
# a leaf that never writes r2 (3.650.011-TOOL 0x81007870 against 0x81007720)
set(MODULE_COMPILE_OPTIONS -fdelete-null-pointer-checks -fuse-caller-save)
# SceSysclib's strtoll (FreeBSD's)
set(sysmem_strtoll_source "${CMAKE_CURRENT_BINARY_DIR}/strtoll-sce.c")
sysmem_prepare_patched(
	"${sysmem_dir}/../../../../../contrib/bsd-stdlib-7.0.0/strtoll.c"
	"${sysmem_dir}/strtoll_sce.patch"
	"${sysmem_strtoll_source}")
# SceSysclib's strlcpy (FreeBSD's), which strings.c includes
sysmem_prepare_patched(
	"${sysmem_dir}/../../../../../contrib/bsd-string-1998/strlcpy.c"
	"${sysmem_dir}/strlcpy_sce.patch"
	"${CMAKE_CURRENT_BINARY_DIR}/strlcpy-sce.c")
# the kernel heap's dlmalloc/malloc.c
set(sysmem_dlmalloc_source "${CMAKE_CURRENT_BINARY_DIR}/dlmalloc-sce.c")
sysmem_prepare_patched(
	"${sysmem_dir}/../../../../../contrib/dlmalloc-2.8.4/malloc.c"
	"${sysmem_dir}/dlmalloc_sce.patch"
	"${sysmem_dlmalloc_source}")

if(FW STREQUAL "4.00-DEBUGGER")
	set(sysmem_des_key_data ${sysmem_dir}/des_key_data_400.c)
	set(sysmem_hmac_data ${sysmem_dir}/hmac_data_400.c)
else()
	set(sysmem_des_key_data ${sysmem_dir}/des_key_data_36x.c)
	set(sysmem_hmac_data ${sysmem_dir}/hmac_data_36x.c)
endif()

set(sysmem_sources
	${sysmem_dir}/memory_set32.S
	${sysmem_dir}/sha1_transform.S
	${sysmem_dir}/oheap.c
	${sysmem_dir}/uid.c
	${sysmem_dir}/murmurhash3.c
	${sysmem_dir}/memblock_obj.c
	${sysmem_dir}/uid_heap.c
	${sysmem_dir}/uid_class.c
	${sysmem_dir}/vslot.c
	${sysmem_dir}/sysmem.c
	${sysmem_dir}/sysmem_heap.c
	${sysmem_dir}/memblock.c
	${sysmem_dir}/partition.c
	${sysmem_dir}/phymem.c
	${sysmem_dir}/smb.c
	${sysmem_dir}/nameheap2.c
	${sysmem_dir}/l2pagetable.c
	${sysmem_dir}/address_space.c
	${sysmem_dir}/fixed_heap.c
	${sysmem_dir}/phymem_low.c
	${sysmem_dir}/dlmalloc/heap.c
	${sysmem_dir}/heaplib.c
	${sysmem_dir}/memblock_db.c
	${sysmem_dir}/memblock_vpl.c
	${sysmem_dir}/memblock_partial.c
	${sysmem_dir}/virpage.c
	${sysmem_dir}/l1pagetable.c
	${sysmem_dir}/bitheap.c
	${sysmem_dir}/suspend.c
	${sysmem_dir}/sysevent.c
	${sysmem_dir}/qafmgr.c
	${sysmem_dir}/pmmgr.c
	${sysmem_dir}/aimgr.c
	${sysmem_dir}/procevent.c
	${sysmem_dir}/acmgr.c
	${sysmem_dir}/led.c
	${sysmem_dir}/serial.c
	${sysmem_dir}/deflate.S
	${sysmem_dir}/string_compare.S
	${sysmem_dir}/string_scan.S
	${sysmem_dir}/strncpy.S
	${sysmem_dir}/strings.c
	${sysmem_dir}/search.c
	${sysmem_dir}/strncat.c
	${sysmem_dir}/strcpy_chk.c
	${sysmem_dir}/strncpy_chk.c
	${sysmem_dir}/strcat_chk.c
	${sysmem_dir}/strncat_chk.c
	${sysmem_dir}/numeric.c
	"${sysmem_strtoll_source}"
	${sysmem_dir}/ctype.c
	${sysmem_dir}/memcpy_chk.c
	${sysmem_dir}/copy_forward.S
	${sysmem_dir}/copy.c
	${sysmem_dir}/debug_state.c
	${sysmem_dir}/dipsw.c
	${sysmem_dir}/sysroot.c
	${sysmem_dir}/sysroot_time_low.c
	${sysmem_dir}/sysroot_capability.c
	${sysmem_dir}/../../../../../toolchains/libgcc/udiv.S
	${sysmem_dir}/../../../../../toolchains/libgcc/idiv.S
	${sysmem_dir}/../../../../../toolchains/libgcc/dvmd_tls.S
	${sysmem_dir}/../../../../../toolchains/libgcc/lshrdi3.S
	${sysmem_dir}/../../../../../toolchains/libgcc/ashrdi3.S
	${sysmem_dir}/../../../../../toolchains/libgcc/ashldi3.S
	${sysmem_dir}/../../../../../toolchains/libgcc/lcmp.S
	${sysmem_dir}/../../../../../toolchains/libgcc/ulcmp.S
	${sysmem_dir}/../../../../../toolchains/libgcc/ldivmod.S
	${sysmem_dir}/../../../../../toolchains/libgcc/uldivmod.S
	${sysmem_dir}/atomics32.c
	${sysmem_dir}/atomics64.c
	${sysmem_dir}/cpu_spinlock.S
	${sysmem_dir}/atomics_small.c
	${sysmem_dir}/atomic_counters.c
	${sysmem_dir}/corelock.c
	${sysmem_dir}/cpu.c
	${sysmem_dir}/cpu_cache.c
	${sysmem_dir}/l2cc.c
	${sysmem_dir}/l2cc_maintenance.S
	${sysmem_dir}/cache.c
	${sysmem_dir}/vmaccess.S
	${sysmem_dir}/mmu.c
	${sysmem_dir}/gzip.c
	${sysmem_dir}/mt19937_next.S
	${sysmem_dir}/mt19937.c
	${sysmem_dir}/des.S
	${sysmem_dir}/des_round.c
	${sysmem_des_key_data}
	${sysmem_dir}/des_block.c
	${sysmem_dir}/kernel_utils_keys.c
	${sysmem_dir}/aes_block.c
	${sysmem_dir}/aes.S
	${sysmem_dir}/sfmt.S
	${sysmem_dir}/deflate_huffman.S
	${sysmem_dir}/xorshift128.c
	${sysmem_dir}/kernel_utils_crypto.c
	${sysmem_dir}/sha256_transform.S
	${sysmem_dir}/sha2_data.c
	${sysmem_dir}/aes_key_data.c
	${sysmem_hmac_data}
	"${sysmem_dlmalloc_source}"
	${sysmem_dir}/aes_cmac.c
	${sysmem_dir}/debug_dump.c
	${sysmem_dir}/debug_asm.S
	${sysmem_dir}/bsd_prnt.c
	${sysmem_dir}/formatter_buffer.c
	${sysmem_dir}/debug_output.c
	${sysmem_dir}/fill_core.S
	${sysmem_dir}/fill.c
	${sysmem_dir}/compare_core.S
	${sysmem_dir}/memset_chk.c
	${sysmem_dir}/move.S
	${sysmem_dir}/memcmp_chk.c
	${sysmem_dir}/memmove_chk.c
	${sysmem_dir}/strlcat_chk.c
	${sysmem_dir}/strlcpy_chk.c
	${sysmem_dir}/sprintf_chk.c
	${sysmem_dir}/stack_chk.c
	${sysmem_dir}/../../../../../toolchains/libgcc/mul.c
	${sysmem_dir}/../../../../../contrib/gcc-4.8-2014.05/libgcc/config/arm/bpabi.c
	${sysmem_dir}/../../../../../toolchains/libgcc/div64.c
	${sysmem_dir}/../../../../../toolchains/libgcc/udiv64.c
)

macro(sysmem_settings target)
	# strings.c includes the adapted strlcpy-sce.c from the build directory
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

	set_source_files_properties("${sysmem_strtoll_source}"
		PROPERTIES COMPILE_OPTIONS "-fsigned-char")

	# Sony built the DIP switch file for size: literal pools, not movw/movt.
	# strncmp, timingsafe_memcmp, the scans and strncpy are Sony's assembly (string_compare.S,
	# string_scan.S, strncpy.S); strings.c gives the rest
	set_property(SOURCE ${sysmem_dir}/strings.c PROPERTY COMPILE_DEFINITIONS
		SYSCLIB_SELECT_STRINGS SYSCLIB_WANT_STRCMP SYSCLIB_WANT_STRLCPY SYSCLIB_WANT_STRLCAT
		SYSCLIB_WANT_STRNCPY_S)

	set_source_files_properties(${sysmem_dir}/dipsw.c PROPERTIES COMPILE_OPTIONS "-Os")

	# Sony's UART file, string functions and UID names sign-extend their characters.
	set_source_files_properties(${sysmem_dir}/serial.c ${sysmem_dir}/strings.c ${sysmem_dir}/uid.c PROPERTIES COMPILE_OPTIONS "-fsigned-char")

	# Sony's exported atomic entry points use ARM instructions in every firmware.
	set_source_files_properties(${sysmem_dir}/atomics32.c ${sysmem_dir}/atomics64.c
		${sysmem_dir}/atomics_small.c ${sysmem_dir}/atomic_counters.c
		PROPERTIES COMPILE_OPTIONS "-marm")

	# the compiler runtime (libgcc), built from GCC's source as LibKernel's: the EABI division and
	# 64-bit entry points in SceSysclib's place, the C helpers last, where Sony's link puts libgcc
	set(libgcc ${sysmem_dir}/../../../../../toolchains/libgcc)
	set(gcc ${sysmem_dir}/../../../../../contrib/gcc-4.8-2014.05)
	set_source_files_properties(${libgcc}/mul.c ${libgcc}/div64.c ${libgcc}/udiv64.c
		${gcc}/libgcc/config/arm/bpabi.c
		PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc"
		COMPILE_DEFINITIONS "IN_LIBGCC2"
		INCLUDE_DIRECTORIES "${libgcc};${libgcc}/target;${gcc}/gcc;${gcc}/gcc/ginclude;${gcc}/libgcc;${gcc}/include")
	include(${sysmem_dir}/../../../../../toolchains/libgcc/lib1funcs.cmake)
	gcc48_use_lib1funcs(${target})
endmacro()
