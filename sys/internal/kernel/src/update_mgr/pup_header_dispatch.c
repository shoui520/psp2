#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include "update_internal.h"

int PupUtilVerifyPupHeaderAsync(SceSize arglen, PupVerifyThreadArgs *args);

int PupUtilVerifyPupHeader(SceUID fd, int load_sm, int resident_sm)
{
	PupVerifyThreadArgs args;
	SceUID thread;
	int status = 0;
	int permission, result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	args.need_header_verify = 0;
	args.segment_index = 0;
	args.reserved = 0;
	args.fd = fd;
	args.load_sm = load_sm;
	args.resident_sm = resident_sm;
	thread = UpdateUtilCreateThread("SceSblUsVerifyPupHeader",
		(SceKernelThreadEntry)PupUtilVerifyPupHeaderAsync,
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
