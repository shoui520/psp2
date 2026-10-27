# NSKBL, the non-secure kernel boot loader: segment 3 of kernel_boot_loader.self
# (ARZL-compressed there), a flat ARM image that runs at 0x51000000.
# Sony built it from os0 sources: its SceSysmem, SceExcpmgr, SceKernelIntrMgr, SceSblSmschedProxy, SceSblAuthMgr, SceSdif and
# SceLowio parts are those modules' own files compiled with SYSMEM_NSKBL, INTRMGR_NSKBL,
# EXCPMGR_NSKBL, SMSC_PROXY_NSKBL, AUTHMGR_NSKBL, SDIF_NSKBL and LOWIO_NSKBL, which
# leave out what the boot loader does not have (the inlining in Sony's image shows its
# translation units held only those functions); the sources are listed in NSKBL's link
# order. The reset vector and exception entries are the original's; boot.c is SKBL's
# (BOOT_NSKBL): the same exception handlers and ASLR helpers, and NSKBL's own boot();
# kbl_main.c, archive_ns.c and iofilemgr_if.c are NSKBL's own, on its FAT driver (sfat.h);
# memset and memcpy are SceSysmem's memset_chk.c and memcpy_chk.c with sysclib's
# assembly cores.
set(sysmem_source ../../sysmem)
set(lowio_source ../../../../driver/src/lowio)
set(sdif_source ../../../../driver/src/sdif)
set(intrmgr_source ../../intrmgr)
set(smsc_proxy_source ../../../../driver/src/smsc_proxy)
set(modulemgr_source ../../modulemgr)
set(excpmgr_source ../../excpmgr)
# SceKernelModulemgr's files in NSKBL's link order
set(modulemgr_c_sources
	${modulemgr_source}/modulemgr.c
	${modulemgr_source}/modulecb.c
	${modulemgr_source}/loadelf.c
	${modulemgr_source}/self_decrypter_kbl.c
	${modulemgr_source}/stub_linker.c
	${modulemgr_source}/loadcore.c
	${modulemgr_source}/relocator.c
	${modulemgr_source}/librarydb.c
	${modulemgr_source}/libstubcb.c
	${modulemgr_source}/libentcb.c
)
# the kernel heap's dlmalloc/malloc.c, prepared as SceSysmem's (sysmem/sources.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/${sysmem_source}/vendor_patch.cmake)
set(sysmem_dlmalloc_source "${CMAKE_CURRENT_BINARY_DIR}/dlmalloc-sce.c")
sysmem_prepare_patched(
	"${PSP2_SRC}/contrib/dlmalloc-2.8.4/malloc.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/${sysmem_source}/dlmalloc_sce.patch"
	"${sysmem_dlmalloc_source}")
set(sysmem_c_sources
	${sysmem_source}/uid.c
	${sysmem_source}/memblock_obj.c
	${sysmem_source}/uid_heap.c
	${sysmem_source}/uid_class.c
	${sysmem_source}/smb.c
	${sysmem_source}/sysmem.c
	${sysmem_source}/sysmem_heap.c
	${sysmem_source}/nameheap2.c
	${sysmem_source}/heaplib.c
	${sysmem_source}/l2pagetable.c
	${sysmem_source}/memblock.c
	${sysmem_source}/address_space.c
	${sysmem_source}/partition.c
	${sysmem_source}/fixed_heap.c
	${sysmem_source}/phymem.c
	${sysmem_source}/phymem_low.c
	${sysmem_source}/dlmalloc/heap.c
	${sysmem_source}/memory_set32.S
	${sysmem_source}/oheap.c
	${sysmem_source}/memblock_db.c
	${sysmem_source}/memblock_vpl.c
	${sysmem_source}/memblock_partial.c
	${sysmem_source}/virpage.c
	${sysmem_source}/uid_dlink.c
	${sysmem_source}/murmurhash3.c
	${sysmem_source}/l1pagetable.c
	${sysmem_dlmalloc_source}
	${sysmem_source}/sysroot.c
	${sysmem_source}/led.c
	${sysmem_source}/debug_state.c
	${sysmem_source}/debug_dump.c
	${sysmem_source}/debug_asm.S
	${sysmem_source}/string_compare.S
	${sysmem_source}/string_scan.S
	${sysmem_source}/ctype.c
	${sysmem_source}/bsd_prnt.c
	${sysmem_source}/formatter_buffer.c
	${sysmem_source}/strncpy.S
	${sysmem_source}/debug_output.c
	${sysmem_source}/memset_chk.c
	${sysmem_source}/memcpy_chk.c
	${sysmem_source}/stack_chk.c
	${sysmem_source}/cpu_cache.c
	${sysmem_source}/atomics_small.c
	${sysmem_source}/atomics32.c
	${sysmem_source}/atomic_counters.c
	${sysmem_source}/cpu.c
	${sysmem_source}/cpu_spinlock.S
	${sysmem_source}/corelock.c
	${sysmem_source}/cache.c
	${sysmem_source}/mmu.c
	${sysmem_source}/l2cc.c
	${sysmem_source}/l2cc_maintenance.S
	${sysmem_source}/qafmgr.c
	${sysmem_source}/aimgr.c
	${sysmem_source}/serial.c
	${sysmem_source}/dipsw.c
)
# SceSysmem's files Sony linked after the drivers: memmove, and the zlib inflater of
# SceKernelUtils (zlib streams only)
set(sysmem_tail_sources
	${sysmem_source}/memmove_chk.c
	${sysmem_source}/move.S
	${sysmem_source}/gzip.c
	${sysmem_source}/deflate.S
	${sysmem_source}/deflate_huffman.S
)
# the compiler runtime, last: the EABI divisions and of GCC's bpabi.c only __gnu_uldivmod_helper,
# the one NSKBL's 64-bit division calls; in ARM instructions, the toolchain's own libgcc, where
# sysmem builds LibKernel's Thumb one (sysmem/sources.cmake)
set(libgcc_source ${PSP2_SRC}/toolchains/libgcc)
set(gcc_source ${PSP2_SRC}/contrib/gcc-4.8-2014.05)
include(${libgcc_source}/select.cmake)
set(nskbl_bpabi_source "${CMAKE_CURRENT_BINARY_DIR}/bpabi-uldivmod.c")
gcc48_select_tail("${gcc_source}/libgcc/config/arm/bpabi.c" "${nskbl_bpabi_source}"
	"long long\n__gnu_ldivmod_helper (" "unsigned long long\n__gnu_uldivmod_helper (")
# and the EHABI unwinder the runtime's unwind tables bring in, LibKernel's adaptation of it
# (toolchains/libgcc/unwind.cmake: no WMMX state)
include(${libgcc_source}/unwind.cmake)
set(nskbl_unwind_source "${CMAKE_CURRENT_BINARY_DIR}/gcc48-unwind.c")
gcc48_prepare_unwind("${gcc_source}/libgcc/config/arm/unwind-arm.c" "${nskbl_unwind_source}")
set(nskbl_libunwind_source "${CMAKE_CURRENT_BINARY_DIR}/libunwind.S")
gcc48_prepare_libunwind("${gcc_source}/libgcc/config/arm/libunwind.S" "${nskbl_libunwind_source}")
psp2_image(slb2_nskbl ADDRESS 0x51000000 PARTIAL SOURCES
	reset.S
	../skbl/boot.c
	kbl_main.c
	archive_ns.c
	iofilemgr_if.c
	${excpmgr_source}/excpmgr.c
	${excpmgr_source}/excpmgr_vector.S
	${excpmgr_source}/excpmgr_initial.S
	${excpmgr_source}/excpmgr_context.S
	${intrmgr_source}/intrmgr.c
	${sysmem_c_sources}
	${smsc_proxy_source}/smsc_proxy.c
	${smsc_proxy_source}/tzs_bridge.c
	${smsc_proxy_source}/smc.S
	../../authmgr/authmgr.c
	${modulemgr_c_sources}
	${sysmem_source}/copy_forward.S
	${sysmem_source}/fill_core.S
	${sdif_source}/sdif.c
	${sdif_source}/cmd_queue.c
	${sdif_source}/sdio_intr.c
	${sdif_source}/mmc.c
	${sdif_source}/sd.c
	${sdif_source}/adma.c
	${sdif_source}/card_cmd.c
	sfat.c
	${lowio_source}/lowio_state.c
	${lowio_source}/pervasive.c
	${lowio_source}/pervasive_clock.c
	threadmgr_if.c
	${sysmem_tail_sources}
	abort.c
	${libgcc_source}/udiv.S
	${libgcc_source}/idiv.S
	${libgcc_source}/dvmd_tls.S
	${libgcc_source}/uldivmod.S
	${nskbl_bpabi_source}
	${libgcc_source}/udiv64.c
	${nskbl_unwind_source}
	${nskbl_libunwind_source}
	${libgcc_source}/unwind_pr.c
)
set_source_files_properties(${intrmgr_source}/intrmgr.c PROPERTIES COMPILE_DEFINITIONS INTRMGR_NSKBL)
set_source_files_properties(${excpmgr_source}/excpmgr.c PROPERTIES COMPILE_DEFINITIONS EXCPMGR_NSKBL)
# SceSysmem's flags (sysmem/sources.cmake): NULL tests after a dereference dropped,
# caller-saved registers kept across leaf calls, signed characters in serial.c and uid.c
set_source_files_properties(${sysmem_c_sources} PROPERTIES
	COMPILE_DEFINITIONS SYSMEM_NSKBL
	COMPILE_OPTIONS "-fdelete-null-pointer-checks;-fuse-caller-save")
set_source_files_properties(${sysmem_source}/l2cc_maintenance.S PROPERTIES COMPILE_OPTIONS "")
set_source_files_properties(${sysmem_source}/serial.c ${sysmem_source}/uid.c PROPERTIES
	COMPILE_OPTIONS "-fdelete-null-pointer-checks;-fuse-caller-save;-fsigned-char")
# the atomic entry points in ARM instructions, as os0's (sysmem/sources.cmake)
set_source_files_properties(${sysmem_source}/atomics_small.c ${sysmem_source}/atomics32.c
	${sysmem_source}/atomic_counters.c PROPERTIES
	COMPILE_OPTIONS "-fdelete-null-pointer-checks;-fuse-caller-save;-marm")
set_source_files_properties(${smsc_proxy_source}/smsc_proxy.c PROPERTIES
	COMPILE_DEFINITIONS SMSC_PROXY_NSKBL)
set_source_files_properties(../../authmgr/authmgr.c PROPERTIES
	COMPILE_DEFINITIONS AUTHMGR_NSKBL)
# modulemgr's per-firmware definitions (MODULE_OBJECT_VERSION), from its own fw/<FW>.local.cmake
set(nskbl_module_defines "${MODULE_DEFINES}")
include(${CMAKE_CURRENT_SOURCE_DIR}/${modulemgr_source}/fw/${FW}.local.cmake)
set(modulemgr_defines ${MODULE_DEFINES})
set(MODULE_DEFINES "${nskbl_module_defines}")
set_source_files_properties(${modulemgr_c_sources} PROPERTIES
	COMPILE_DEFINITIONS "MODULEMGR_NSKBL;${modulemgr_defines}")
set_source_files_properties(${sdif_source}/sdif.c ${sdif_source}/cmd_queue.c
	${sdif_source}/sdio_intr.c ${sdif_source}/mmc.c ${sdif_source}/sd.c
	${sdif_source}/adma.c ${sdif_source}/card_cmd.c PROPERTIES COMPILE_DEFINITIONS SDIF_NSKBL)
set_source_files_properties(../skbl/boot.c PROPERTIES COMPILE_DEFINITIONS "BOOT_NSKBL;SYSMEM_NSKBL;LOWIO_KBL")
# NSKBL's own files see sysmem's and authmgr's interfaces as NSKBL builds them
set_source_files_properties(kbl_main.c archive_ns.c iofilemgr_if.c PROPERTIES
	COMPILE_DEFINITIONS "SYSMEM_NSKBL;AUTHMGR_NSKBL")
set_source_files_properties(${sysmem_tail_sources} PROPERTIES
	COMPILE_DEFINITIONS SYSMEM_NSKBL
	COMPILE_OPTIONS "-fdelete-null-pointer-checks;-fuse-caller-save")
set_source_files_properties(${libgcc_source}/udiv.S ${libgcc_source}/idiv.S
	${libgcc_source}/dvmd_tls.S ${libgcc_source}/uldivmod.S PROPERTIES COMPILE_OPTIONS "-marm")
set_source_files_properties(${libgcc_source}/udiv64.c ${nskbl_bpabi_source} PROPERTIES
	COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc;-marm"
	COMPILE_DEFINITIONS "IN_LIBGCC2"
	INCLUDE_DIRECTORIES "${libgcc_source};${libgcc_source}/target;${gcc_source}/gcc;${gcc_source}/gcc/ginclude;${gcc_source}/libgcc;${gcc_source}/include")
set_source_files_properties(${nskbl_unwind_source} ${libgcc_source}/unwind_pr.c PROPERTIES
	COMPILE_OPTIONS "-std=gnu99;-fno-inline;-fno-stack-protector;-marm"
	INCLUDE_DIRECTORIES "${libgcc_source};${libgcc_source}/target;${gcc_source}/gcc;${gcc_source}/gcc/ginclude;${gcc_source}/libgcc;${gcc_source}/include")
set_property(SOURCE ${nskbl_unwind_source} ${libgcc_source}/unwind_pr.c APPEND PROPERTY
	COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/unwind_unused.h")
set_property(SOURCE ${libgcc_source}/unwind_pr.c APPEND PROPERTY COMPILE_DEFINITIONS NSKBL_UNWIND_PR)
set_property(SOURCE ${nskbl_libunwind_source} APPEND PROPERTY COMPILE_OPTIONS "-marm")
include(${libgcc_source}/lib1funcs.cmake)
gcc48_use_lib1funcs(slb2_nskbl)
# SceSfat, NSKBL's FAT driver, at -O3 (its loops over the open files unrolled, its directory
# scan prefetched) with signed characters
set_source_files_properties(sfat.c PROPERTIES COMPILE_DEFINITIONS SYSMEM_NSKBL
	COMPILE_OPTIONS "-O3;-fsigned-char")
# SceLowio's Pervasive driver, its own files built for the boot loader (LOWIO_KBL: physical
# registers, no lock; LOWIO_NSKBL: what this image keeps), at the boot loader's -O2: the images keep
# ResetSet and GateSet whole, where os0's -O3 clones them
set_source_files_properties(${lowio_source}/lowio_state.c ${lowio_source}/pervasive.c
	${lowio_source}/pervasive_clock.c PROPERTIES
	COMPILE_DEFINITIONS "LOWIO_KBL;LOWIO_NSKBL")
