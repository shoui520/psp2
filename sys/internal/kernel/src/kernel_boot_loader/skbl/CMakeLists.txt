# SKBL, the secure kernel boot loader and secure monitor: segments 0-2 of
# kernel_boot_loader.self (the reset vector page at 0, the code at 0x40020000 with the
# TrustZone modules it loads, the data). It starts the TrustZone kernel (slb2/kbl/),
# then hands over to NSKBL (sys/internal/kernel/src/kernel_boot_loader/nskbl/) in Non-secure state.
# Sony built it from the TrustZone and os0 kernel sources: its SceKernelModulemgr,
# SceExcpmgr, SceKernelIntrMgr, SceSysmem and SceLowio parts are those modules' own
# files compiled with MODULEMGR_SKBL, EXCPMGR_SKBL, INTRMGR_SKBL, SYSMEM_SKBL and
# LOWIO_SKBL, listed in SKBL's link order.
set(sysmem_source ../../sysmem)
set(lowio_source ../../../../driver/src/lowio)
set(intrmgr_source ../../intrmgr/tzs)
set(modulemgr_source ../../modulemgr)
set(excpmgr_source ../../excpmgr)
# SceKernelModulemgr's files in SKBL's link order
set(modulemgr_c_sources
	${modulemgr_source}/modulemgr.c
	${modulemgr_source}/modulecb.c
	${modulemgr_source}/loadelf.c
	${modulemgr_source}/self_parser.c
	${modulemgr_source}/loadcore.c
	${modulemgr_source}/relocator.c
	${modulemgr_source}/librarydb.c
	${modulemgr_source}/libstubcb.c
	${modulemgr_source}/libentcb.c
	${modulemgr_source}/stub_linker.c
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
	${sysmem_source}/memmove_chk.c
	${sysmem_source}/stack_chk.c
	${sysmem_source}/copy_forward.S
	${sysmem_source}/move.S
	${sysmem_source}/fill_core.S
	${sysmem_source}/cpu_cache.c
	${sysmem_source}/cpu.c
	${sysmem_source}/cpu_spinlock.S
	${sysmem_source}/atomics_small.c
	${sysmem_source}/atomics32.c
	${sysmem_source}/atomic_counters.c
	${sysmem_source}/l2cc.c
	${sysmem_source}/l2cc_maintenance.S
	${sysmem_source}/corelock.c
	${sysmem_source}/cache.c
	${sysmem_source}/mmu.c
	${sysmem_source}/dipsw.c
	${sysmem_source}/qafmgr.c
	${sysmem_source}/aimgr.c
	${sysmem_source}/serial.c
)
# the compiler runtime, last: the EABI divisions and of GCC's bpabi.c only __gnu_uldivmod_helper,
# in ARM instructions, the toolchain's own libgcc
set(libgcc_source ${PSP2_SRC}/toolchains/libgcc)
set(gcc_source ${PSP2_SRC}/contrib/gcc-4.8-2014.05)
include(${libgcc_source}/select.cmake)
set(skbl_bpabi_source "${CMAKE_CURRENT_BINARY_DIR}/bpabi-uldivmod.c")
gcc48_select_tail("${gcc_source}/libgcc/config/arm/bpabi.c" "${skbl_bpabi_source}"
	"long long\n__gnu_ldivmod_helper (" "unsigned long long\n__gnu_uldivmod_helper (")
# and the EHABI unwinder the runtime's unwind tables bring in, LibKernel's adaptation of it
include(${libgcc_source}/unwind.cmake)
set(skbl_unwind_source "${CMAKE_CURRENT_BINARY_DIR}/gcc48-unwind.c")
gcc48_prepare_unwind("${gcc_source}/libgcc/config/arm/unwind-arm.c" "${skbl_unwind_source}")
set(skbl_libunwind_source "${CMAKE_CURRENT_BINARY_DIR}/libunwind.S")
gcc48_prepare_libunwind("${gcc_source}/libgcc/config/arm/libunwind.S" "${skbl_libunwind_source}")
psp2_image(slb2_skbl ADDRESS 0x40020000 LINKER_SCRIPT skbl.ld NO_BIN SOURCES
	reset.S
	version.c
	memory_test.c
	monitor.S
	boot.c
	kbl_main.c
	archive_tzs.c
	${intrmgr_source}/dmacmgr_tzs.c
	${modulemgr_c_sources}
	${excpmgr_source}/tzs/excpmgr.c
	${excpmgr_source}/tzs/excpmgr_vector.S
	${excpmgr_source}/excpmgr_initial.S
	${excpmgr_source}/excpmgr_context.S
	${intrmgr_source}/intrmgr.c
	${intrmgr_source}/usleep.c
	${intrmgr_source}/intrmgr_entry.S
	${sysmem_c_sources}
	${lowio_source}/lowio_state.c
	${lowio_source}/pervasive.c
	${lowio_source}/pervasive_clock.c
	arlz.c
	../nskbl/abort.c
	${libgcc_source}/udiv.S
	${libgcc_source}/dvmd_tls.S
	${libgcc_source}/uldivmod.S
	${skbl_bpabi_source}
	${libgcc_source}/udiv64.c
	${skbl_unwind_source}
	${skbl_libunwind_source}
	${libgcc_source}/unwind_pr.c
	compat_sram.S
)
# modulemgr's per-firmware definitions (MODULE_OBJECT_VERSION), from its own fw/<FW>.local.cmake
set(skbl_module_defines "${MODULE_DEFINES}")
include(${CMAKE_CURRENT_SOURCE_DIR}/${modulemgr_source}/fw/${FW}.local.cmake)
set(modulemgr_defines ${MODULE_DEFINES})
set(MODULE_DEFINES "${skbl_module_defines}")
set_source_files_properties(${modulemgr_c_sources} PROPERTIES
	COMPILE_DEFINITIONS "MODULEMGR_NSKBL;MODULEMGR_SKBL;${modulemgr_defines}")
set_source_files_properties(${excpmgr_source}/tzs/excpmgr.c
	${excpmgr_source}/tzs/excpmgr_vector.S PROPERTIES COMPILE_DEFINITIONS EXCPMGR_SKBL)
set_source_files_properties(${excpmgr_source}/excpmgr_initial.S
	${excpmgr_source}/excpmgr_context.S PROPERTIES COMPILE_DEFINITIONS EXCPMGR_TZS)
set_source_files_properties(${intrmgr_source}/intrmgr.c ${intrmgr_source}/usleep.c
	${intrmgr_source}/intrmgr_entry.S PROPERTIES
	COMPILE_DEFINITIONS INTRMGR_SKBL)
set_source_files_properties(${intrmgr_source}/dmacmgr_tzs.c PROPERTIES
	COMPILE_DEFINITIONS INTRMGR_SKBL COMPILE_OPTIONS "-Os")
set_source_files_properties(arlz.c PROPERTIES
	COMPILE_OPTIONS "-O3;-fno-stack-protector;-marm")
# SceSysmem's flags (sysmem/sources.cmake): NULL tests after a dereference dropped,
# caller-saved registers kept across leaf calls, signed characters in serial.c and uid.c
set_source_files_properties(${sysmem_c_sources} PROPERTIES
	COMPILE_DEFINITIONS "SYSMEM_NSKBL;SYSMEM_SKBL"
	COMPILE_OPTIONS "-fdelete-null-pointer-checks;-fuse-caller-save")
set_source_files_properties(${sysmem_source}/l2cc.c
	${sysmem_source}/l2cc_maintenance.S PROPERTIES
	COMPILE_DEFINITIONS SYSMEM_SKBL)
set_source_files_properties(${sysmem_source}/l2cc_maintenance.S
	${sysmem_source}/copy_forward.S ${sysmem_source}/move.S
	${sysmem_source}/fill_core.S PROPERTIES COMPILE_OPTIONS "")
set_source_files_properties(${sysmem_source}/serial.c ${sysmem_source}/uid.c PROPERTIES
	COMPILE_OPTIONS "-fdelete-null-pointer-checks;-fuse-caller-save;-fsigned-char")
# the atomic entry points in ARM instructions, as os0's (sysmem/sources.cmake)
set_source_files_properties(${sysmem_source}/atomics_small.c ${sysmem_source}/atomics32.c
	${sysmem_source}/atomic_counters.c PROPERTIES
	COMPILE_OPTIONS "-fdelete-null-pointer-checks;-fuse-caller-save;-marm")
# the boot loader's own files see sysmem's and lowio's interfaces as SKBL builds them
set_property(SOURCE boot.c kbl_main.c archive_tzs.c APPEND PROPERTY
	COMPILE_DEFINITIONS SYSMEM_NSKBL SYSMEM_SKBL LOWIO_KBL)
# SceLowio's Pervasive driver, its own files built for the boot loader (LOWIO_KBL: physical
# registers, no lock; LOWIO_SKBL: what this image keeps), at the boot loader's -O2: the images keep
# ResetSet and GateSet whole, where os0's -O3 clones them
set_source_files_properties(${lowio_source}/lowio_state.c ${lowio_source}/pervasive.c
	${lowio_source}/pervasive_clock.c PROPERTIES
	COMPILE_DEFINITIONS "LOWIO_KBL;LOWIO_SKBL")
set_source_files_properties(${libgcc_source}/udiv.S ${libgcc_source}/dvmd_tls.S
	${libgcc_source}/uldivmod.S PROPERTIES COMPILE_OPTIONS "-marm")
set_source_files_properties(${libgcc_source}/udiv64.c ${skbl_bpabi_source} PROPERTIES
	COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc;-marm"
	COMPILE_DEFINITIONS "IN_LIBGCC2"
	INCLUDE_DIRECTORIES "${libgcc_source};${libgcc_source}/target;${gcc_source}/gcc;${gcc_source}/gcc/ginclude;${gcc_source}/libgcc;${gcc_source}/include")
set_source_files_properties(${skbl_unwind_source} ${libgcc_source}/unwind_pr.c PROPERTIES
	COMPILE_OPTIONS "-std=gnu99;-fno-inline;-fno-stack-protector;-marm"
	INCLUDE_DIRECTORIES "${libgcc_source};${libgcc_source}/target;${gcc_source}/gcc;${gcc_source}/gcc/ginclude;${gcc_source}/libgcc;${gcc_source}/include")
set_property(SOURCE ${skbl_unwind_source} ${libgcc_source}/unwind_pr.c APPEND PROPERTY
	COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/../nskbl/unwind_unused.h")
set_property(SOURCE ${libgcc_source}/unwind_pr.c APPEND PROPERTY COMPILE_DEFINITIONS NSKBL_UNWIND_PR)
set_property(SOURCE ${skbl_libunwind_source} APPEND PROPERTY COMPILE_OPTIONS "-marm")
include(${libgcc_source}/lib1funcs.cmake)
gcc48_use_lib1funcs(slb2_skbl)
if(TARGET slb2_skbl)
	# the TrustZone modules SKBL carries, as their own folders build them: SceSysmem as an ARZL
	# archive, the others as they are (compat_sram.S)
	set(tzs_build "${CMAKE_BINARY_DIR}/slb2/kbl")
	set(tzs_archive "${CMAKE_CURRENT_BINARY_DIR}/SceSysmem.arzl")
	add_custom_command(OUTPUT "${tzs_archive}"
		COMMAND python3 "${PSP2_SRC}/tools/arzl.py" pack
			"${tzs_build}/SceSysmem/slb2_kbl_SceSysmem.velf" "${tzs_archive}"
		DEPENDS slb2_kbl_SceSysmem_velf "${tzs_build}/SceSysmem/slb2_kbl_SceSysmem.velf"
			"${PSP2_SRC}/tools/arzl.py"
		VERBATIM)
	add_custom_target(slb2_skbl_archive DEPENDS "${tzs_archive}")
	set(tzs_images "${tzs_archive}")
	set(tzs_defines "TZS_SYSMEM_ARCHIVE=\"${tzs_archive}\"")
	foreach(entry Excpmgr:SceExcpmgr IntrMgr:SceKernelIntrMgr BusError:SceKernelBusError
			Smsched:SceSblSmsched DriverTzs:SceDriverTzs)
		string(REPLACE ":" ";" entry "${entry}")
		list(GET entry 0 label)
		list(GET entry 1 module)
		string(TOUPPER "${label}" label)
		set(image "${tzs_build}/${module}/slb2_kbl_${module}.velf")
		list(APPEND tzs_images "${image}")
		list(APPEND tzs_defines "TZS_${label}_IMAGE=\"${image}\"")
		add_dependencies(slb2_skbl slb2_kbl_${module}_velf)
	endforeach()
	add_dependencies(slb2_skbl slb2_skbl_archive)
	set_property(SOURCE compat_sram.S APPEND PROPERTY COMPILE_DEFINITIONS ${tzs_defines})
	set_property(SOURCE compat_sram.S APPEND PROPERTY OBJECT_DEPENDS ${tzs_images})

	# kernel_boot_loader.self as the firmware ships it: SKBL's segments and NSKBL's ARZL
	# archive (tools/mkkbl.py) in an fSELF of type 9, a boot loader, with Sony's authority ID
	set(nskbl_bin "${CMAKE_BINARY_DIR}/slb2/nskbl/slb2_nskbl.bin")
	set(nskbl_archive "${CMAKE_CURRENT_BINARY_DIR}/nskbl.arzl")
	add_custom_command(OUTPUT "${nskbl_archive}"
		COMMAND python3 "${PSP2_SRC}/tools/arzl.py" pack "${nskbl_bin}" "${nskbl_archive}"
		DEPENDS slb2_nskbl_bin "${nskbl_bin}" "${PSP2_SRC}/tools/arzl.py"
		VERBATIM)
	add_custom_command(OUTPUT kernel_boot_loader.elf
		COMMAND python3 "${PSP2_SRC}/tools/mkkbl.py" $<TARGET_FILE:slb2_skbl> "${nskbl_archive}"
			kernel_boot_loader.elf
		DEPENDS slb2_skbl $<TARGET_FILE:slb2_skbl> "${nskbl_archive}" "${PSP2_SRC}/tools/mkkbl.py"
		VERBATIM)
	math(EXPR kbl_version "(${FW_VERSION} & 0xFFFF0000) << 16" OUTPUT_FORMAT HEXADECIMAL)
	add_custom_command(OUTPUT kernel_boot_loader.self
		COMMAND python3 "${PSP2_SRC}/tools/mkfself.py" kernel_boot_loader.elf kernel_boot_loader.self
			--auth-id 0x2808000000000001 --self-type 9 --app-version ${kbl_version} --no-compress
		DEPENDS kernel_boot_loader.elf "${PSP2_SRC}/tools/mkfself.py"
		VERBATIM)
	add_custom_target(slb2_kernel_boot_loader ALL DEPENDS kernel_boot_loader.self)
endif()
