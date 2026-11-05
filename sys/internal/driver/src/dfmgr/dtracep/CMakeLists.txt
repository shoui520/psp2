# os0:kd/deci4p_dtracep - DTrace (Sony's port); being converted to C
# Sony's CDDL sources, as Sony published them (contrib/sce-dtrace-cddl-2015.01), at the
# paths Sony's build compiled them from
set(dtrace_cddl "${PSP2_SRC}/contrib/sce-dtrace-cddl-2015.01")
set(dtrace_cddl_sources
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtracep_syscalls_cddl.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/dtrace.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/dtrace_subr.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/fasttrap.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/fasttrap_isa.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/list.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/sdt.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/sdt_subr.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/profile.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/systrace.c"
	"${dtrace_cddl}/driver/src/dfmgr/dtracep/dtrace/cddl/taskq.c")
foreach(src IN LISTS dtrace_cddl_sources)
	string(REGEX REPLACE ".*/driver/src/dfmgr/dtracep/" "" name "${src}")
	set_property(SOURCE "${src}" APPEND PROPERTY COMPILE_DEFINITIONS "__FILE_NAME__=\"${name}\"")
endforeach()

# xnu's blist.c and qsort.c (contrib/xnu-1456.1.26), with Sony's changes as patches in
# dtrace/bsd/: DTrace's heap and the kernel's panic. The patched copies are made at
# configure time.
set(dtrace_xnu "${PSP2_SRC}/contrib/xnu-1456.1.26")
function(dtrace_prepare_patched upstream patch output)
	configure_file("${upstream}" "${output}" COPYONLY)
	execute_process(COMMAND patch -s "${output}" "${patch}"
		RESULT_VARIABLE result ERROR_VARIABLE error)
	if(NOT result EQUAL 0)
		message(FATAL_ERROR "DTrace adaptation of ${upstream} failed: ${error}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${upstream}" "${patch}")
endfunction()
set(dtrace_bsd_out "${CMAKE_CURRENT_BINARY_DIR}/dtrace/bsd")
dtrace_prepare_patched("${dtrace_xnu}/bsd/dev/dtrace/blist.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/dtrace/bsd/blist.patch" "${dtrace_bsd_out}/blist.c")
dtrace_prepare_patched("${dtrace_xnu}/bsd/kern/qsort.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/dtrace/bsd/qsort.patch" "${dtrace_bsd_out}/qsort.c")
set(dtrace_bsd_sources "${dtrace_bsd_out}/blist.c" "${dtrace_bsd_out}/qsort.c")
foreach(src IN LISTS dtrace_bsd_sources)
	get_filename_component(name "${src}" NAME)
	set_property(SOURCE "${src}" APPEND PROPERTY COMPILE_DEFINITIONS
		"__FILE_NAME__=\"dtrace/bsd/${name}\"")
endforeach()

psp2_module(os0_kd_deci4p_dtracep KIND kernel COMPILER gcc48 SOURCES
	dtrace_change_stack.S
	dtrace_client.c
	dtrace_client_list.c
	dtracep.c
	dtracep_command.c
	dtracep_module.c
	dtracep_packet_queue.c
	dtracep_syscalls.c
	dtracep_pmep.c
	${dtrace_bsd_sources}
	${dtrace_cddl_sources}
	dtrace/sce/dtrace_backtrace.c
	dtrace/sce/dtrace_bookmark.c
	dtrace/sce/dtrace_copy.c
	dtrace/sce/dtrace_cpu.c
	dtrace/sce/dtrace_hrtime.c
	dtrace/sce/dtrace_sce.c
	dtrace/sce/dtrace_security.c
	dtrace/sce/dtrace_process.c
	dtrace/sce/dtrace_thread.c
	dtrace/sce/dtrace_timeout.c
	dtrace/sce/dtrace_vtime.c
	dtrace/sce/sce_fasttrap.c
	dtrace/sce/sce_irqtrace.c
	dtrace/sce/sce_profile.c
	dtrace/sce/sce_systrace.c
	dtrace/sce/sce_taskq.c
	dtrace/sce/solaris_atomic.c
	dtrace/sce/solaris_bitmap.c
	dtrace/sce/solaris_condvar.c
	dtrace/sce/solaris_cpu.c
	dtrace/sce/solaris_cred.c
	dtrace/sce/solaris_cyclic.c
	dtrace/sce/solaris_cyclic_low.c
	dtrace/sce/solaris_cyclic_high.c
	dtrace/sce/solaris_ddi.c
	dtrace/sce/solaris_debug.c
	dtrace/sce/solaris_hrtime.c
	dtrace/sce/solaris_kmem.c
	dtrace/sce/solaris_kmemcache.c
	dtrace/sce/solaris_mutex.c
	dtrace/sce/solaris_process.c
	dtrace/sce/solaris_rwlock.c
	dtrace/sce/solaris_thread.c
	dtrace/sce/solaris_vmem.c
	dtrace/sce/dtrace_breakpoint.c
	global_timer.c
	heap.c
	utility.c
)
# Sony built DTrace with signed chars (its sign-extended char loads and compares) and at -O3
# (vectorized loops in the stack walks; dtrace.c keeps 104 functions, -O2 would keep 137), with
# the C library builtins (values live across strcmp, the checked copies fold) and GCC's
# default null pointer check deletion (no NULL test of a pointer already dereferenced)
target_compile_options(os0_kd_deci4p_dtracep PRIVATE -fsigned-char -O3 -fbuiltin -fdelete-null-pointer-checks)
target_compile_definitions(os0_kd_deci4p_dtracep PRIVATE __SCE__ KERNEL DTRACE_KERNEL)
# Sony's link leaves out the functions nothing calls (list_insert_*, list_move_tail,
# taskq_suspend/resume, ...) and their debug records: a section per function and per
# record, and a link with --gc-sections. Records Sony's link kept without their code
# are orphans (solaris_rwlock.c).
target_compile_options(os0_kd_deci4p_dtracep PRIVATE -ffunction-sections)
target_compile_definitions(os0_kd_deci4p_dtracep PRIVATE SCE_DBGINFO_GC)
get_target_property(dtracep_type os0_kd_deci4p_dtracep TYPE)
if(dtracep_type STREQUAL "EXECUTABLE")
	# the map gives tools/dbginfo.py the objects of records whose strings were discarded
	target_link_options(os0_kd_deci4p_dtracep PRIVATE -Wl,--gc-sections
		-Wl,-Map=$<TARGET_FILE:os0_kd_deci4p_dtracep>.map)
endif()
# the release firmwares are DTrace's release build: no DIF/DOF error reports, no command names
if(NOT FW_DEBUG)
	target_compile_definitions(os0_kd_deci4p_dtracep PRIVATE DTRACE_RELEASE_VERSION=1)
endif()
target_include_directories(os0_kd_deci4p_dtracep PRIVATE
	"${dtrace_cddl}/driver/src/dfmgr/dtracep"
	"${dtrace_cddl}/include"
	"${dtrace_cddl}/include/dtrace/cddl")
