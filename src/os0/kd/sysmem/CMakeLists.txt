# Partial Sysmem foundations; remaining bodies are explicit stubs.
include(source/vendor_patch.cmake)
# Sony built SceSysmem without -fno-delete-null-pointer-checks: a NULL test after a
# dereference is dropped (dlmalloc's malloc.c, partition.c's free-list helpers)
set(MODULE_COMPILE_OPTIONS -fdelete-null-pointer-checks)
set(sysmem_sfmt_source "${CMAKE_CURRENT_BINARY_DIR}/sfmt-sce.c")
sysmem_prepare_patched(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor/sfmt-1.3.3/SFMT.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/source/sfmt_sce.patch"
	"${sysmem_sfmt_source}")
# the kernel heap's dlmalloc/malloc.c
set(sysmem_dlmalloc_source "${CMAKE_CURRENT_BINARY_DIR}/dlmalloc-sce.c")
sysmem_prepare_patched(
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor/dlmalloc-2.8.4/malloc.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/source/dlmalloc_sce.patch"
	"${sysmem_dlmalloc_source}")

if(FW STREQUAL "4.00-DEBUGGER")
	set(sysmem_des_key_data source/des_key_data_400.c)
	set(sysmem_hmac_data source/hmac_data_400.c)
else()
	set(sysmem_des_key_data source/des_key_data_36x.c)
	set(sysmem_hmac_data source/hmac_data_36x.c)
endif()

psp2_module(os0_kd_sysmem KIND kernel SOURCES
	source/memory_set32.c
	source/sha1_transform.c
	source/oheap.c
	source/uid.c
	source/murmurhash3.c
	source/memblock_obj.c
	source/uid_class.c
	source/vslot.c
	source/sysmem_misc.c
	source/sysmem_heap.c
	source/user_copy.c
	source/memblock.c
	source/partition.c
	source/phymem.c
	source/smb.c
	source/nameheap2.c
	source/l2pagetable.c
	source/address_space.c
	source/fixed_heap.c
	source/phymem_low.c
	source/heap.c
	source/memblock_db.c
	source/memblock_vpl.c
	source/memblock_partial.c
	source/virpage.c
	source/l1pagetable.c
	source/bitheap.c
	source/suspend.c
	source/sysevent.c
	source/qafmgr.c
	source/pmmgr.c
	source/aimgr.c
	source/procevent.c
	source/led.c
	source/serial.c
	source/deflate.c
	../../../common/sysclib/strings.c
	../../../common/sysclib/search.c
	../../../common/sysclib/checked_strings.c
	../../../common/sysclib/numeric.c
	../../../common/sysclib/bsd_strtoll_kernel.c
	../../../common/sysclib/ctype.c
	source/memory_copy.c
	../../../common/sysclib/copy_forward.S
	../../../common/sysclib/copy.c
	source/debug_state.c
	source/dipsw.c
	source/sysroot_time.c
	source/sysroot_time_low.c
	source/sysroot_capability.c
	source/aeabi.S
	source/unsigned_division.S
	source/signed_division.S
	source/signed_division64.S
	source/atomics32.c
	source/atomics64.c
	source/cpu_spinlock.S
	source/atomics_small.c
	source/atomic_counters.c
	source/corelock.c
	source/cpu.c
	source/cpu_cache.c
	source/l2cc.c
	source/cache.c
	source/vmaccess.S
	source/mmu.c
	source/gzip.c
	source/mt19937.c
	source/des_permutation.c
	source/des_round.c
	${sysmem_des_key_data}
	source/des_key.c
	source/des_block.c
	source/sysmem.c
	source/xorshift128.c
	source/aes_block.c
	source/kernel_utils_crypto.c
	source/sha256_transform.c
	source/sha2_data.c
	source/aes_key_data.c
	${sysmem_hmac_data}
	"${sysmem_sfmt_source}"
	"${sysmem_dlmalloc_source}"
	source/aes_cmac.c
	source/debug_dump.c
	source/debug_asm.S
	../../../common/sysclib/bsd_prnt.c
	source/formatter_buffer.c
	source/debug_output.c
	../../../common/sysclib/fill.c
	../../../common/sysclib/compare.c
	source/memory_set.c
	../../../common/sysclib/move.S
	source/memory_move.c
	source/strlcat_chk.c
	source/strlcpy_chk.c
	source/sprintf_chk.c
)

set_source_files_properties(../../../common/sysclib/bsd_strtoll_kernel.c
	PROPERTIES COMPILE_OPTIONS "-fsigned-char")

# Sony built the DIP switch file for size: literal pools, not movw/movt.
set_source_files_properties(source/dipsw.c PROPERTIES COMPILE_OPTIONS "-Os")

# Sony's UART file sign-extends its characters.
set_source_files_properties(source/serial.c PROPERTIES COMPILE_OPTIONS "-fsigned-char")

# Sony's exported atomic entry points use ARM instructions in every firmware.
set_source_files_properties(source/atomics32.c source/atomics64.c
	source/atomics_small.c source/atomic_counters.c
	PROPERTIES COMPILE_OPTIONS "-marm")

# The data segment where the original has it, with room for the VELF tables after the text.
set(SYSMEM_DATA_BASE_4.00-DEBUGGER 0x81049000)
set(SYSMEM_DATA_BASE_3.650.011-TOOL 0x81030000)
set(SYSMEM_DATA_BASE_3.65-CEX 0x81030000)
set(SYSMEM_DATA_BASE_3.630.011-DEBUGGER 0x81030000)
fw_value(SYSMEM_DATA_BASE SYSMEM_DATA_BASE)
target_link_options(os0_kd_sysmem PRIVATE -Wl,-Tdata,${SYSMEM_DATA_BASE})

target_include_directories(os0_kd_sysmem PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor/sfmt-1.3.3")
