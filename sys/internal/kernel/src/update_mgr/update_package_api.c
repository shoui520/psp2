#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include "update_internal.h"
#include <acmgr/acmgr.h>
#include <intrmgr/syscall.h>

int sceSblUsUpdateSpackageForUser(int packageType, const SceKernelSpackageArgs *args,
			 uint32_t *requestId)
{
	SceKernelSpackageArgs request;
	void *buffer = 0;
	uint32_t id = 0;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	if (!args || !requestId) {
		result = (int)0x800f0216;
	} else {
		result = sceKernelCopyFromUser(&request, args, sizeof(request));
		if (result == 0) {
			result = UpdateUtilUA2KA(request.addr, sceKernelGetProcessId(), &buffer);
			if (result == 0) {
				result = SpkgUtilUpdateSpackage(packageType, buffer,
					request.size, request.flags, &id);
				if (result == 0)
					result = sceKernelCopyToUser(requestId, &id, sizeof(id));
			}
		}
	}
	sceSyscallLeave(state);
	return result;
}

int sceSblUsInspectSpackageForUser(int packageType, const SceKernelSpackageArgs *args,
			  uint32_t *requestId)
{
	SceKernelSpackageArgs request;
	void *buffer = 0;
	uint32_t id = 0;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	if (!args || !requestId) {
		result = (int)0x800f0216;
	} else {
		result = sceKernelCopyFromUser(&request, args, sizeof(request));
		if (result == 0) {
			result = UpdateUtilUA2KA(request.addr, sceKernelGetProcessId(), &buffer);
			if (result == 0) {
				result = SpkgUtilInspectSpackage(packageType, buffer,
					request.size, request.flags, &id);
				if (result == 0)
					result = sceKernelCopyToUser(requestId, &id, sizeof(id));
			}
		}
	}
	sceSyscallLeave(state);
	return result;
}

int sceSblUsExtractSpackageForUser(int packageType, const SceKernelSpackageArgs *args,
			  uint32_t *requestId)
{
	SceKernelSpackageArgs request;
	void *buffer = 0;
	uint32_t id = 0;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	if (!args || !requestId) {
		result = (int)0x800f0216;
	} else {
		result = sceKernelCopyFromUser(&request, args, sizeof(request));
		if (result == 0) {
			result = UpdateUtilUA2KA(request.addr, sceKernelGetProcessId(), &buffer);
			if (result == 0) {
				result = SpkgUtilExtractSpackage(packageType, buffer,
					request.size, request.flags, &id);
				if (result == 0)
					result = sceKernelCopyToUser(requestId, &id, sizeof(id));
			}
		}
	}
	sceSyscallLeave(state);
	return result;
}

int sceSblUsGetExtractSpackageForUser(int nodeType, uint32_t requestId,
			     SceKernelSpackageArgs *args)
{
	SceKernelSpackageArgs request;
	UpdateUtilNodeStatus status;
	void *userBuffer = 0;
	void *kernelBuffer;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	result = sceKernelCopyFromUser(&request, args, sizeof(request));
	if (result == 0) {
		result = UpdateUtilGetNodeStatusLocked(nodeType, &status);
		if (result == 0) {
			if (status.requestId == requestId) {
				kernelBuffer = status.buffer;
				result = UpdateUtilKA2UA(kernelBuffer, sceKernelGetProcessId(),
							&userBuffer);
				if (result == 0) {
					request.addr = userBuffer;
					result = sceKernelCopyToUser(args, &request, sizeof(request));
				}
			} else {
				result = (int)0x800f0202;
			}
		}
	}
	sceSyscallLeave(state);
	return result;
}

int sceSblUsAllocateBufferForUser(SceSize size, void **output)
{
	void *userBuffer = 0;
	void *kernelBuffer = 0;
	void *buffer;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	result = UpdateUtilAllocateBuffer(size, &kernelBuffer, sceKernelGetProcessId());
	if (result == 0) {
		buffer = kernelBuffer;
		result = UpdateUtilKA2UA(buffer, sceKernelGetProcessId(), &userBuffer);
		if (result == 0)
			result = sceKernelCopyToUser(output, &userBuffer, sizeof(userBuffer));
	}
	sceSyscallLeave(state);
	return result;
}

int sceSblUsReleaseBufferForUser(void *userBuffer)
{
	void *kernelBuffer = 0;
	void *buffer;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	result = UpdateUtilUA2KA(userBuffer, sceKernelGetProcessId(), &kernelBuffer);
	if (result == 0) {
		buffer = kernelBuffer;
		result = UpdateUtilReleaseBuffer(buffer, sceKernelGetProcessId());
	}
	sceSyscallLeave(state);
	return result;
}

int sceSblUsGetStatusForUser(int nodeType, uint32_t requestId,
			   const SceKernelSpackageArgs *args)
{
	SceKernelSpackageArgs request;
	uint32_t seq_no = 0;
	uint32_t status = 0;
	uint32_t progress = 0;
	uint32_t written_rates = 0;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	if (!args)
		return (int)0x800f0216;
	state = sceSyscallEnter();
	result = sceKernelCopyFromUser(&request, args, sizeof(request));
	if (result >= 0) {
		result = SpkgUtilGetStatus(nodeType, requestId, &seq_no, &status,
					 &progress, &written_rates);
		if (result == 0) {
			result = sceKernelCopyToUser(request.seq_no, &seq_no, sizeof(seq_no));
			if (result >= 0) {
				result = sceKernelCopyToUser(request.result, &status, sizeof(status));
				if (result >= 0) {
					result = sceKernelCopyToUser(request.progress, &progress,
								    sizeof(progress));
					if (result >= 0)
						result = sceKernelCopyToUser(request.written_rates,
							&written_rates, sizeof(written_rates));
				}
			}
		}
	}
	sceSyscallLeave(state);
	return result;
}

int sceSblUsGetSpkgInfoForUser(int packageType, SceSblUsSpkgInfo *output)
{
	SceSblUsSpkgInfo info;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	if (output) {
		result = SpkgInfoUtilGetSpkgInfo(packageType, &info);
		if (result == 0)
			result = sceKernelCopyToUser(output, &info, sizeof(info));
	} else {
		result = (int)0x800f0216;
	}
	sceSyscallLeave(state);
	return result;
}
