# SceKernelThreadMgr. Sony's kernel build: no tail calls (callers' lr reach panics/asserts).
set(MODULE_COMPILE_OPTIONS -fno-optimize-sibling-calls)
psp2_module(os0_kd_threadmgr KIND kernel SOURCES
	vfp_poison.S
	kernel_stack_space.c
	excp_handlers.S
	debug_thread_event.c
	uid_waitable.c
	threadmgr_boot.c
	threadmgr.c
	context.c
	thread_context.c
	scheduler.c
	thread_start.c
	exit_thread.c
	trap.c
	hooks.c
	uid_context.c
	vfp_context.c
	uid_class.c
	timer.c
	timer_syscalls.c
	wait_common.c
	semaphore.c
	process_thread_scan.c
	dispatch.c
	user_stack.c
	context_runtime.c
	event_flag.c
	event_flag_queries.c
	event_flag_lifecycle.c
	mutex.c
	cond.c
	rw_lock.c
	pmon.c
	lw_mutex.c
	lw_cond.c
	thread_event.c
	dtrace.c
	fast_mutex.c
	priority_change.c
	thread_priority.c
	priority_public.c
	affinity_runtime.c
	thread_affinity.c
	affinity_public.c
	affinity_queries.c
	intr_dispatch.c
	intr_context.c
	extended_stack_cache.c
	thread_state.c
	callback.c
	debugger.c
	vfp_exception.c
	gpu_exception.c
	work_queue.c
	virtual_machine.c
	debug_suspend.c
	syslibtrace.c
	active_cpu.c
	stop_thread.c
	threadmgr_for_user.c
	delay_thread.c
	thread_wait.c
	thread_info.c
	signal.c
	thread_lifecycle.c
	thread_start_kernel.c
	kernel_stack.c
	thread_create.c
	thread_exception.c
	threadmgr_syscall_noidl.c
	process_hooks.c
	callback_queries.c
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
