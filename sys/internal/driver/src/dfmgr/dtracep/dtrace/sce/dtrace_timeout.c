/*
 * Solaris timeouts (Sony's dtrace/sce/dtrace_timeout.c): each timeout is a
 * thread of its own that sleeps, calls its function and deletes itself.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/sce/dtrace_timeout.h>
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>

typedef struct dtrace_timeout_args {
	void (*func)(void *);
	void *arg;
	hrtime_t nsec;
} dtrace_timeout_args_t;

static int _dtrace_timeout_thread(SceSize argSize, void *argp)
{
	dtrace_timeout_args_t *args = argp;
	SceUID thid;
	int res;

	thid = sceKernelGetThreadId();
	SCE_DBG_ASSERT(argSize == sizeof(*args), 64);
	sceKernelDelayThread((uint64_t)args->nsec / 1000);
	if (args->func != NULL)
		args->func(args->arg);
	res = sceKernelExitDeleteThread(0);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(80, "failed to exit/delete thread for timeout thread 0x%08x (0x%08x)",
				    thid, res);
	return 0;
}

timeout_id_t dtrace_timeout(void (*func)(void *), void *arg, hrtime_t nsec)
{
	SceKernelThreadOptParam opt = {
		.size = sizeof(opt),
		.attr = 1,
		.kStackMemType = 0x10f0d006,
	};
	dtrace_timeout_args_t args;
	SceUID thid;
	int res;

	thid = sceKernelCreateThread("SceDeci4pDtracepTimeout", _dtrace_timeout_thread, 0x1e,
				     0x4000, 0, 0x80000, &opt);
	if (thid < 0) {
		DTRACE_LOG_ERROR_AT(107, "failed to create timeout thread (0x%x)", thid);
		return thid;
	}
	args.func = func;
	args.arg = arg;
	args.nsec = nsec;
	res = sceKernelStartThread(thid, sizeof(args), &args);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(122, "failed to start timeout thread 0x%08x (0x%x)", thid, res);
		return res;
	}
	return thid;
}
