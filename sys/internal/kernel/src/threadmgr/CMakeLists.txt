# SceKernelThreadMgr. Sony's kernel build: no tail calls (callers' lr reach panics/asserts).
set(MODULE_COMPILE_OPTIONS -fno-optimize-sibling-calls)
psp2_module(os0_kd_threadmgr KIND kernel SOURCES
	source/vfp_poison.S
	source/kernel_stack_space.c
	source/excp_handlers.S
	source/debug_thread_event.c
	source/uid_waitable.c
	source/threadmgr_boot.c
	source/threadmgr.c
	source/context.c
	source/thread_context.c
	source/scheduler.c
	source/thread_start.c
	source/exit_thread.c
	source/trap.c
	source/hooks.c
	source/uid_context.c
	source/vfp_context.c
	source/uid_class.c
	source/timer.c
	source/timer_syscalls.c
	source/wait_common.c
	source/semaphore.c
	source/process_thread_scan.c
	source/dispatch.c
	source/user_stack.c
	source/context_runtime.c
	source/event_flag.c
	source/event_flag_queries.c
	source/event_flag_lifecycle.c
	source/mutex.c
	source/cond.c
	source/rw_lock.c
	source/pmon.c
	source/lw_mutex.c
	source/lw_cond.c
	source/thread_event.c
	source/dtrace.c
	source/fast_mutex.c
	source/priority_change.c
	source/thread_priority.c
	source/priority_public.c
	source/affinity_runtime.c
	source/thread_affinity.c
	source/affinity_public.c
	source/affinity_queries.c
	source/intr_dispatch.c
	source/intr_context.c
	source/extended_stack_cache.c
	source/thread_state.c
	source/callback.c
	source/debugger.c
	source/vfp_exception.c
	source/gpu_exception.c
	source/work_queue.c
	source/virtual_machine.c
	source/debug_suspend.c
	source/syslibtrace.c
	source/active_cpu.c
	source/stop_thread.c
	source/threadmgr_for_user.c
	source/delay_thread.c
	source/thread_wait.c
	source/thread_info.c
	source/signal.c
	source/thread_lifecycle.c
	source/thread_start_kernel.c
	source/kernel_stack.c
	source/thread_create.c
	source/thread_exception.c
	source/threadmgr_syscall_noidl.c
	source/process_hooks.c
	source/callback_queries.c
	source/intr_requests.c
	source/event_notification.c
	source/event.c
	source/simple_event.c
	source/message_pipe.c
	source/thread_tls.c
)
# The data segment sits where the original's does.
set(THREADMGR_DATA_BASE_4.00-DEBUGGER 0x81054000)
set(THREADMGR_DATA_BASE_3.650.011-TOOL 0x81039000)
set(THREADMGR_DATA_BASE_3.65-CEX 0x81039000)
set(THREADMGR_DATA_BASE_3.630.011-DEBUGGER 0x81039000)
fw_value(THREADMGR_DATA_BASE THREADMGR_DATA_BASE)
target_link_options(os0_kd_threadmgr PRIVATE -Wl,-Tdata,${THREADMGR_DATA_BASE})
