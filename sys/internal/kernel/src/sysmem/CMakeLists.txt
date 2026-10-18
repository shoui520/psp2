# Partial Sysmem foundations; remaining bodies are explicit stubs.
include(vendor_patch.cmake)
# Sony built SceSysmem without -fno-delete-null-pointer-checks: a NULL test after a
# dereference is dropped (dlmalloc's malloc.c, partition.c's free-list helpers)
set(MODULE_COMPILE_OPTIONS -fdelete-null-pointer-checks)
set(sysmem_sfmt_source "${CMAKE_CURRENT_BINARY_DIR}/sfmt-sce.c")
sysmem_prepare_patched(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/sfmt-1.3.3/SFMT.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/sfmt_sce.patch"
	"${sysmem_sfmt_source}")
# SceSysclib's strtoll (FreeBSD's)
set(sysmem_strtoll_source "${CMAKE_CURRENT_BINARY_DIR}/strtoll-sce.c")
sysmem_prepare_patched(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/bsd-stdlib-7.0.0/strtoll.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/strtoll_sce.patch"
	"${sysmem_strtoll_source}")
# SceSysclib's strlcpy (FreeBSD's), which strings.c includes
sysmem_prepare_patched(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/bsd-string-1998/strlcpy.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/strlcpy_sce.patch"
	"${CMAKE_CURRENT_BINARY_DIR}/strlcpy-sce.c")
# the kernel heap's dlmalloc/malloc.c
set(sysmem_dlmalloc_source "${CMAKE_CURRENT_BINARY_DIR}/dlmalloc-sce.c")
sysmem_prepare_patched(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/dlmalloc-2.8.4/malloc.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/dlmalloc_sce.patch"
	"${sysmem_dlmalloc_source}")

if(FW STREQUAL "4.00-DEBUGGER")
	set(sysmem_des_key_data des_key_data_400.c)
	set(sysmem_hmac_data hmac_data_400.c)
else()
	set(sysmem_des_key_data des_key_data_36x.c)
	set(sysmem_hmac_data hmac_data_36x.c)
endif()

psp2_module(os0_kd_sysmem KIND kernel COMPILER gcc48 SOURCES
	memory_set32.c
	sha1_transform.c
	oheap.c
	uid.c
	murmurhash3.c
	memblock_obj.c
	uid_class.c
	vslot.c
	sysmem.c
	sysmem_heap.c
	memblock.c
	partition.c
	phymem.c
	smb.c
	nameheap2.c
	l2pagetable.c
	address_space.c
	fixed_heap.c
	phymem_low.c
	dlmalloc/heap.c
	memblock_db.c
	memblock_vpl.c
	memblock_partial.c
	virpage.c
	l1pagetable.c
	bitheap.c
	suspend.c
	sysevent.c
	qafmgr.c
	pmmgr.c
	aimgr.c
	procevent.c
	led.c
	serial.c
	deflate.c
	strings.c
	search.c
	checked_strings.c
	numeric.c
	"${sysmem_strtoll_source}"
	ctype.c
	memcpy_chk.c
	copy_forward.S
	copy.c
	debug_state.c
	dipsw.c
	sysroot.c
	sysroot_time_low.c
	sysroot_capability.c
	aeabi.S
	unsigned_division.S
	signed_division.S
	signed_division64.S
	atomics32.c
	atomics64.c
	cpu_spinlock.S
	atomics_small.c
	atomic_counters.c
	corelock.c
	cpu.c
	cpu_cache.c
	l2cc.c
	l2cc_maintenance.S
	cache.c
	vmaccess.S
	mmu.c
	gzip.c
	mt19937.c
	des_permutation.c
	des_round.c
	${sysmem_des_key_data}
	des_key.c
	des_block.c
	kernel_utils_keys.c
	xorshift128.c
	aes_block.c
	kernel_utils_crypto.c
	sha256_transform.c
	sha2_data.c
	aes_key_data.c
	${sysmem_hmac_data}
	"${sysmem_sfmt_source}"
	"${sysmem_dlmalloc_source}"
	aes_cmac.c
	debug_dump.c
	debug_asm.S
	bsd_prnt.c
	formatter_buffer.c
	debug_output.c
	fill_core.S
	fill.c
	compare_core.S
	compare.c
	memset_chk.c
	move.S
	memcmp_chk.c
	memmove_chk.c
	strlcat_chk.c
	strlcpy_chk.c
	sprintf_chk.c
)
# strings.c includes the adapted strlcpy-sce.c from the build directory
target_include_directories(os0_kd_sysmem PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

set_source_files_properties("${sysmem_strtoll_source}"
	PROPERTIES COMPILE_OPTIONS "-fsigned-char")

# Sony built the DIP switch file for size: literal pools, not movw/movt.
set_source_files_properties(dipsw.c PROPERTIES COMPILE_OPTIONS "-Os")

# Sony's UART file sign-extends its characters.
set_source_files_properties(serial.c PROPERTIES COMPILE_OPTIONS "-fsigned-char")

# Sony's exported atomic entry points use ARM instructions in every firmware.
set_source_files_properties(atomics32.c atomics64.c
	atomics_small.c atomic_counters.c
	PROPERTIES COMPILE_OPTIONS "-marm")

# The data segment where the original has it, with room for the VELF tables after the text.
set(SYSMEM_DATA_BASE_4.00-DEBUGGER 0x81049000)
set(SYSMEM_DATA_BASE_3.650.011-TOOL 0x81030000)
set(SYSMEM_DATA_BASE_3.65-CEX 0x81030000)
set(SYSMEM_DATA_BASE_3.630.011-DEBUGGER 0x81030000)
fw_value(SYSMEM_DATA_BASE SYSMEM_DATA_BASE)
target_link_options(os0_kd_sysmem PRIVATE -Wl,-Tdata,${SYSMEM_DATA_BASE})

target_include_directories(os0_kd_sysmem PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/sfmt-1.3.3")
