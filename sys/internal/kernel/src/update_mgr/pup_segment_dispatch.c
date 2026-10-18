#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "update_internal.h"

int PupUtilVerifyPupSegmentMain(SceSize arglen, PupVerifyThreadArgs *args);

int PupUtilVerifyPupSegment(SceUID fd, uint64_t segment_index,
		int need_header_verify, int load_sm, int resident_sm)
{
	PupVerifyThreadArgs args;
	SceUID thread;
	int status = 0;
	int permission, result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	args.reserved = 0;
	args.fd = fd;
	args.segment_index = segment_index;
	args.need_header_verify = need_header_verify;
	args.load_sm = load_sm;
	args.resident_sm = resident_sm;
	thread = UpdateUtilCreateThread("SceSblUsVerifyPupSegment",
		(SceKernelThreadEntry)PupUtilVerifyPupSegmentMain,
		96, 8192, 0x50000, 0x80000, NULL);
	if (thread < 0) {
		sceKernelSetThreadAccessLevel(permission);
		return thread;
	}
	result = sceKernelStartThread(thread, sizeof(args), &args);
	if (result) {
		sceKernelDeleteThread(thread);
		sceKernelSetThreadAccessLevel(permission);
		return result;
	}
	result = sceKernelWaitThreadEnd(thread, &status, NULL);
	if (result)
		status = result;
	sceKernelDeleteThread(thread);
	sceKernelSetThreadAccessLevel(permission);
	return status;
}
