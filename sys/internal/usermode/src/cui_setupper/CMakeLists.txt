enable_language(CXX)
# os0:ue/cui_setupper.self (SceCuiSetUpper), built by Sony with SNC; one build, shipped as
# os0:ue/cui_setupper and in the PUP (pup:cui_setupper): the target is named from the rel.
# PAF's heap is dlmalloc 2.8.4 (pristine upstream plus vsh/paf's 0.996 SCE patch).
set(cui_dlmalloc_source "${CMAKE_CURRENT_BINARY_DIR}/dlmalloc.c")
configure_file("${PSP2_SRC}/contrib/dlmalloc-2.8.4/malloc.c" "${cui_dlmalloc_source}" COPYONLY)
execute_process(COMMAND patch -s "${cui_dlmalloc_source}"
	"${PSP2_SRC}/vsh/paf/src/memory/dlmalloc_sce_0996.patch"
	RESULT_VARIABLE cui_patch_result ERROR_VARIABLE cui_patch_error)
if(NOT cui_patch_result EQUAL 0)
	message(FATAL_ERROR "cui_setupper dlmalloc patch failed: ${cui_patch_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${PSP2_SRC}/contrib/dlmalloc-2.8.4/malloc.c"
	"${PSP2_SRC}/vsh/paf/src/memory/dlmalloc_sce_0996.patch")

# Sony's build kept every method out of line and wrote both constructor and destructor copies
# (C1/C2, D1/D2) in full: budgets 12 to 16 do both, 0 to 8 call C1 from C2, 24 inlines
# CuiSetUpper::Initialize (2026-11-06)
set(MODULE_COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:C,CXX>:-Xautoinlinesize=12>)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	cui_setupper.cpp
	cui_view.c
	${PSP2_SRC}/sys/sdk/target/src/crt0/crt0_main.c
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/updater_core.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_copy_file.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_operation_base.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/partition_manager.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/producer_consumer_channel.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/pup_controller.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/segment_analysis.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group_manager.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/update_utility.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/updater_array.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group_diff.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group_full.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/tar_archive.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/memory_fat_fs.cpp
	${PSP2_SRC}/sys/internal/usermode/src/fsutil/fs_util.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/i64ops.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/i64shr.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/i32ops.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr0.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr1.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr2.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/aeabi_div0.c
	${PSP2_SRC}/vsh/paf/src/std/static_runtime.c
	${PSP2_SRC}/vsh/paf/src/std/global_heap.c
	${PSP2_SRC}/vsh/paf/src/std/memory.c
	${PSP2_SRC}/vsh/paf/src/std/printf.c
	${PSP2_SRC}/vsh/paf/src/std/string.c
	${PSP2_SRC}/vsh/paf/src/std/stdlib.c
	${PSP2_SRC}/vsh/paf/src/std/fill.c
	${PSP2_SRC}/vsh/paf/src/std/mem_neon.S
	${PSP2_SRC}/vsh/paf/src/std/stdc++/atexit.c
	${PSP2_SRC}/vsh/paf/src/std/stdc++/paf_crt_mgr.c
	${PSP2_SRC}/vsh/paf/src/memory/heap_allocator.cpp
	${cui_dlmalloc_source}
	${PSP2_SRC}/vsh/paf/src/std/ctype.c
	${PSP2_SRC}/vsh/paf/src/std/format.c
	${PSP2_SRC}/vsh/paf/src/std/stdio.c
	${PSP2_SRC}/vsh/paf/src/std/new.c
	${PSP2_SRC}/vsh/paf/src/thread/mutex.cpp
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_io_stub.c
)
target_include_directories(${PSP2_TARGET} PRIVATE
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
	"${PSP2_SRC}/vsh/paf/src")
target_compile_definitions(${PSP2_TARGET} PRIVATE PAF_STATIC_BUILD PSP2_STRIP_UNUSED)
# Sony's 0.996 SNC emitted no stack protector (cui_setupper imports no __stack_chk_guard or
# __stack_chk_fail); SNC 3.57 always guards frames with locals, so bind its guard word to a
# read-only word and its failure handler to __cxa_pure_virtual (which executes bkpt #0).
# Also export crt0's __sce_process_param as NID 0 variable sceProcessParam (0x70FBA1E7).
target_link_options(${PSP2_TARGET} PRIVATE
	-Wl,--gc-sections
	-Wl,--defsym=sceProcessParam=__sce_process_param
	-Wl,--defsym=__stack_chk_guard=__sce_process_param
	-Wl,--defsym=__stack_chk_fail=__cxa_pure_virtual)
# updater_core, a library of its own in Sony's build: budget 24 writes its classes' C2 and D2
# copies in full and inlines ParallelCopyFileBuffer's constructor into CreateElement, as Sony's
# build did (12 and 16 call C1 from C2), 2026-11-06
set_source_files_properties(
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/updater_core.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_copy_file.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/parallel_operation_base.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/partition_manager.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/producer_consumer_channel.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/pup_controller.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/segment_analysis.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group_manager.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/update_utility.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/updater_array.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group_diff.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/spkg_group_full.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/tar_archive.cpp
	${PSP2_SRC}/sys/internal/usermode/src/updater_core/memory_fat_fs.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=24)
# 0.996 libsnc masks 64-bit shift counts with 0x3f rather than returning 0 for > 63
set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/libsnc/i64ops.c
	PROPERTIES COMPILE_DEFINITIONS LIBSNC_0996)
# libsnc unwind personality routines call through ip
set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr0.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr1.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/unwind_pr2.c
	PROPERTIES COMPILE_OPTIONS -Xblxcall=1)
# PAF C++ runtime units and dlmalloc use the same inlining budgets as vsh/paf
set_source_files_properties(
	${PSP2_SRC}/vsh/paf/src/memory/heap_allocator.cpp
	${PSP2_SRC}/vsh/paf/src/thread/mutex.cpp
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)
set_source_files_properties("${cui_dlmalloc_source}"
	PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=21)
# the SceSblSsUpdateMgr and SceVshBridge stub libraries' wrappers, which Sony built with GCC 4.8
# and without a stack guard, as libpaf's (2026-11-06)
set_source_files_properties(
	${PSP2_SRC}/sys/sdk/target/src/libSceSblSsUpdateMgr_stub/update_mgr_stub.c
	${PSP2_SRC}/sys/sdk/target/src/libSceVshBridge_stub/vshbridge_io_stub.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48 COMPILE_OPTIONS -fno-stack-protector)
