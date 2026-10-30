/*
 * DTrace's machine-dependent stack walks (Sony's dtrace/sce/dtrace_backtrace.c):
 * the kernel and user call stacks come from the module manager's backtrace, the
 * barriers and compare-and-swaps DTrace's probe context needs from the CPU.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/utility.h>
#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/sysroot_modulemgr.h>
#include <threadmgr/threadmgr.h>

typedef struct SceKernelBacktraceFrameForKernel dtrace_frame_t;

void dtrace_membar_producer(void)
{
	__asm__ volatile("dsb" ::: "memory");
}

void dtrace_membar_consumer(void)
{
	__asm__ volatile("dmb" ::: "memory");
}

uint32_t dtrace_cas32(uint32_t *target, uint32_t cmp, uint32_t new)
{
	return atomic_cas_32(target, cmp, new);
}

void *dtrace_casptr(void *target, void *cmp, void *new)
{
	return atomic_cas_ptr(target, cmp, new);
}

void dtrace_getpcstack(pc_t *pcstack, int pcstack_limit, int aframes, uint32_t *intrpc)
{
	dtrace_frame_t *frames = NULL;
	SceSize nframes = 0;
	SceSize i;
	int depth = 0;

	if (pcstack_limit > 0) {
		frames = dtrace_heap_alloc(pcstack_limit * sizeof(*frames));
		if (frames == NULL)
			DTRACE_LOG_ERROR_AT(49, "%s - failed to allocate buffer for kernel backtrace", __FUNCTION__)
		else
			SceBacktraceForDriver_888E99B8(0, frames, pcstack_limit * sizeof(*frames),
						       &nframes, 1);
	}

	if (intrpc != NULL)
		pcstack[depth++] = (pc_t)intrpc;

	for (i = aframes; i < nframes; i++)
		pcstack[depth++] = frames[i].pc;

	while (depth < pcstack_limit)
		pcstack[depth++] = 0;

	if (frames != NULL)
		dtrace_heap_free(frames);
}

int dtrace_getstackdepth(int aframes)
{
	int depth;

	depth = SceBacktraceForDriver_888E99B8(0, NULL, 0, NULL, 1);
	if (depth < 0)
		return 0;
	if (aframes > 0) {
		if (depth > aframes)
			return depth - aframes;
		return 0;
	}
	return depth;
}

void dtrace_getupcstack(uint64_t *pcstack, int pcstack_limit)
{
	dtrace_frame_t *frames = NULL;
	SceSize nframes = 0;
	ThreadCBInfo info;
	SceUID pid, thid;
	SceSize i;
	int res;

	pid = dtrace_get_current_process_id();
	if (!(cpu_core[CPU->cpu_id].cpuc_dtrace_flags & CPU_DTRACE_FAULT) && pcstack_limit > 0) {
		thid = 0;
		if (SceExcpmgrForKernel_B615A7DA() != 0 || sceKernelIsIntrContext() != 0) {
			res = sceKernelGetThreadContextInfo(&info);
			if (res < 0) {
				DTRACE_LOG_DEBUG_AT(130, 0, "%s - getThreadContextInfo failed: %d", __FUNCTION__, res);
				return;
			}
			pid = info.processId;
			thid = info.threadId;
		}
		if (pid == -1) {
			DTRACE_LOG_DEBUG_AT(153, 0, "%s - user backtrace failed: invalid pid", __FUNCTION__);
		} else {
			frames = dtrace_heap_alloc(pcstack_limit * sizeof(*frames));
			if (frames == NULL) {
				DTRACE_LOG_ERROR_AT(159, "%s - failed to allocate buffer for user backtrace", __FUNCTION__);
			} else if (SceBacktraceForDriver_888E99B8(thid, frames, pcstack_limit * sizeof(*frames),
								  &nframes, 0) < 0) {
				nframes = 0;
			}
		}
	}

	pcstack[0] = pid;
	for (i = 1; i < pcstack_limit; i++) {
		if (i > nframes)
			break;
		if (&frames[i - 1] != NULL)
			pcstack[i] = frames[i - 1].pc;
	}
	for (; i < pcstack_limit; i++)
		pcstack[i] = 0;

	if (frames != NULL)
		dtrace_heap_free(frames);
}

int dtrace_getustackdepth(void)
{
	int depth;

	depth = SceBacktraceForDriver_888E99B8(0, NULL, 0, NULL, 0);
	if (depth < 0)
		return 0;
	return depth;
}

void dtrace_getufpstack(uint64_t *pcstack, uint64_t *fpstack, int pcstack_limit)
{
	dtrace_frame_t *frames = NULL;
	SceSize nframes = 0;
	ThreadCBInfo info;
	SceUID pid, thid;
	SceSize i;
	int res;

	if (!(cpu_core[CPU->cpu_id].cpuc_dtrace_flags & CPU_DTRACE_FAULT) && pcstack_limit > 0) {
		pid = dtrace_get_current_process_id();
		thid = 0;
		if (SceExcpmgrForKernel_B615A7DA() != 0 || sceKernelIsIntrContext() != 0) {
			res = sceKernelGetThreadContextInfo(&info);
			if (res < 0) {
				DTRACE_LOG_DEBUG_AT(249, 0, "%s - getThreadContextInfo failed: %d", __FUNCTION__, res);
				return;
			}
			res = sceKernelDebugSuspendThread(info.threadId, 1);
			if (res < 0) {
				DTRACE_LOG_DEBUG_AT(255, 0, "%s - suspend thread failed: %d pid = %x tid = %x",
						 __FUNCTION__, res, info.processId, info.threadId);
			} else {
				pid = info.processId;
				thid = info.threadId;
			}
		}
		if (pid == -1) {
			DTRACE_LOG_DEBUG_AT(266, 0, "%s - user backtrace failed: invalid pid", __FUNCTION__);
		} else {
			frames = dtrace_heap_alloc(pcstack_limit * sizeof(*frames));
			if (frames == NULL) {
				DTRACE_LOG_ERROR_AT(272, "%s - failed to allocate buffer for user backtrace", __FUNCTION__);
			} else {
				res = SceBacktraceForDriver_888E99B8(thid, frames,
								     pcstack_limit * sizeof(*frames),
								     &nframes, 0);
				if (res < 0) {
					DTRACE_LOG_DEBUG_AT(278, 0, "%s - user backtrace failed: %d pid = %x tid = %x",
							 __FUNCTION__, res, pid, thid);
					nframes = 0;
				}
			}
		}
	}

	for (i = 0; i < nframes; i++) {
		if (&frames[i] != NULL) {
			pcstack[i] = frames[i].pc;
			fpstack[i] = frames[i].sp;
		}
	}
	for (; i < pcstack_limit; i++) {
		pcstack[i] = 0;
		fpstack[i] = 0;
	}

	if (frames != NULL)
		dtrace_heap_free(frames);
}

uint64_t dtrace_getarg(int arg, int aframes)
{
	return 0;
}

uintptr_t dtrace_caller(int ignore)
{
	return -1;
}
