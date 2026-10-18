#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "update_internal.h"

int PupUtilVerifyPupWatermarkMain(SceSize arglen, PupVerifyThreadArgs *args);

int PupUtilVerifyPupWatermark(SceUID fd, int need_header_verify,
		int load_sm, int resident_sm)
{
	PupVerifyThreadArgs args;
	SceUID thread;
	int status = 0;
	int permission, result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	args.segment_index = 0;
	args.reserved = 0;
	args.fd = fd;
	args.need_header_verify = need_header_verify;
	args.load_sm = load_sm;
	args.resident_sm = resident_sm;
	if (need_header_verify == 1) {
		result = PupUtilVerifyPupHeader(fd, load_sm, 1);
		if (result)
			return result;
		args.load_sm = 0;
	}
	thread = UpdateUtilCreateThread("SceSblUsVerifyPupWatermark",
		(SceKernelThreadEntry)PupUtilVerifyPupWatermarkMain,
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
