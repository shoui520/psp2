# SceKernelThreadMgr. Sony's kernel build: no tail calls (callers' lr reach panics/asserts).
set(MODULE_COMPILE_OPTIONS -fno-optimize-sibling-calls)
psp2_module(os0_kd_threadmgr KIND kernel SOURCES
	vfp_poison.S
	kernel_stack_space.c
	excp_handlers.S
	debug_thread_event.c
	threadmgr.c
	hooks.c
	timer.c
	timer_syscalls.c
	wait_common.c
	semaphore.c
	dispatch.c
	event_flag.c
	event_flag_queries.c
	mutex.c
	cond.c
	rw_lock.c
	pmon.c
	lw_mutex.c
	lw_cond.c
	thread_event.c
	dtrace.c
	fast_mutex.c
	callback.c
	debugger.c
	vfp_exception.c
	gpu_exception.c
	work_queue.c
	virtual_machine.c
	syslibtrace.c
	threadmgr_for_user.c
	signal.c
	threadmgr_syscall_noidl.c
	sgi.c
	event_notification.c
	event.c
	simple_event.c
	message_pipe.c
	thread_tls.c
)
# The data segment sits where the original's does.
set(THREADMGR_DATA_BASE_4.00-DEBUGGER 0x81054000)
set(THREADMGR_DATA_BASE_3.650.011-TOOL 0x81039000)
set(THREADMGR_DATA_BASE_3.65-CEX 0x81039000)
set(THREADMGR_DATA_BASE_3.630.011-DEBUGGER 0x81039000)
fw_value(THREADMGR_DATA_BASE THREADMGR_DATA_BASE)
target_link_options(os0_kd_threadmgr PRIVATE -Wl,-Tdata,${THREADMGR_DATA_BASE})
