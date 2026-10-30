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

psp2_module(os0_kd_deci4p_dtracep KIND kernel COMPILER gcc48 PARTIAL SOURCES
	dtrace_change_stack.S
	deci4p_dtracep.c
	dtrace_client.c
	dtrace_client_list.c
	dtracep.c
	dtracep_packet_queue.c
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
# (vectorized loops in the stack walks; dtrace.c keeps 104 functions, -O2 would keep 137)
target_compile_options(os0_kd_deci4p_dtracep PRIVATE -fsigned-char -O3)
target_compile_definitions(os0_kd_deci4p_dtracep PRIVATE __SCE__ KERNEL DTRACE_KERNEL)
target_include_directories(os0_kd_deci4p_dtracep PRIVATE
	"${dtrace_cddl}/driver/src/dfmgr/dtracep"
	"${dtrace_cddl}/include"
	"${dtrace_cddl}/include/dtrace/cddl")
