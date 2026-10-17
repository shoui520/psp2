#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <threadmgr/threadmgr.h>
#include "update_internal.h"
#include <acmgr/acmgr.h>

int sceSblUsUpdateSpackageForUser(int packageType, const SceKernelSpackageArgs *args,
			 uint32_t *requestId)
{
	SceKernelSpackageArgs request;
	void *buffer = 0;
	uint32_t id = 0;
	unsigned int state;
	int result;

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	if (!args || !requestId) {
		result = (int)0x800f0216;
	} else {
		result = ksceKernelCopyFromUser(&request, args, sizeof(request));
		if (result == 0) {
			result = UpdateUtilUA2KA(request.addr, sceKernelGetProcessId(), &buffer);
			if (result == 0) {
				result = SpkgUtilUpdateSpackage(packageType, buffer,
					request.size, request.flags, &id);
				if (result == 0)
					result = ksceKernelCopyToUser(requestId, &id, sizeof(id));
			}
		}
	}
	EXIT_SYSCALL(state);
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

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	if (!args || !requestId) {
		result = (int)0x800f0216;
	} else {
		result = ksceKernelCopyFromUser(&request, args, sizeof(request));
		if (result == 0) {
			result = UpdateUtilUA2KA(request.addr, sceKernelGetProcessId(), &buffer);
			if (result == 0) {
				result = SpkgUtilInspectSpackage(packageType, buffer,
					request.size, request.flags, &id);
				if (result == 0)
					result = ksceKernelCopyToUser(requestId, &id, sizeof(id));
			}
		}
	}
	EXIT_SYSCALL(state);
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

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	if (!args || !requestId) {
		result = (int)0x800f0216;
	} else {
		result = ksceKernelCopyFromUser(&request, args, sizeof(request));
		if (result == 0) {
			result = UpdateUtilUA2KA(request.addr, sceKernelGetProcessId(), &buffer);
			if (result == 0) {
				result = SpkgUtilExtractSpackage(packageType, buffer,
					request.size, request.flags, &id);
				if (result == 0)
					result = ksceKernelCopyToUser(requestId, &id, sizeof(id));
			}
		}
	}
	EXIT_SYSCALL(state);
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

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = ksceKernelCopyFromUser(&request, args, sizeof(request));
	if (result == 0) {
		result = UpdateUtilGetNodeStatusLocked(nodeType, &status);
		if (result == 0) {
			if (status.requestId == requestId) {
				kernelBuffer = status.buffer;
				result = UpdateUtilKA2UA(kernelBuffer, sceKernelGetProcessId(),
							&userBuffer);
				if (result == 0) {
					request.addr = userBuffer;
					result = ksceKernelCopyToUser(args, &request, sizeof(request));
				}
			} else {
				result = (int)0x800f0202;
			}
		}
	}
	EXIT_SYSCALL(state);
	return result;
}

int sceSblUsAllocateBufferForUser(SceSize size, void **output)
{
	void *userBuffer = 0;
	void *kernelBuffer = 0;
	void *buffer;
	unsigned int state;
	int result;

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = UpdateUtilAllocateBuffer(size, &kernelBuffer, sceKernelGetProcessId());
	if (result == 0) {
		buffer = kernelBuffer;
		result = UpdateUtilKA2UA(buffer, sceKernelGetProcessId(), &userBuffer);
		if (result == 0)
			result = ksceKernelCopyToUser(output, &userBuffer, sizeof(userBuffer));
	}
	EXIT_SYSCALL(state);
	return result;
}

int sceSblUsReleaseBufferForUser(void *userBuffer)
{
	void *kernelBuffer = 0;
	void *buffer;
	unsigned int state;
	int result;

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = UpdateUtilUA2KA(userBuffer, sceKernelGetProcessId(), &kernelBuffer);
	if (result == 0) {
		buffer = kernelBuffer;
		result = UpdateUtilReleaseBuffer(buffer, sceKernelGetProcessId());
	}
	EXIT_SYSCALL(state);
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

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	if (!args)
		return (int)0x800f0216;
	ENTER_SYSCALL(state);
	result = ksceKernelCopyFromUser(&request, args, sizeof(request));
	if (result >= 0) {
		result = SpkgUtilGetStatus(nodeType, requestId, &seq_no, &status,
					 &progress, &written_rates);
		if (result == 0) {
			result = ksceKernelCopyToUser(request.seq_no, &seq_no, sizeof(seq_no));
			if (result >= 0) {
				result = ksceKernelCopyToUser(request.result, &status, sizeof(status));
				if (result >= 0) {
					result = ksceKernelCopyToUser(request.progress, &progress,
								    sizeof(progress));
					if (result >= 0)
						result = ksceKernelCopyToUser(request.written_rates,
							&written_rates, sizeof(written_rates));
				}
			}
		}
	}
	EXIT_SYSCALL(state);
	return result;
}

int sceSblUsGetSpkgInfoForUser(int packageType, SceSblUsSpkgInfo *output)
{
	SceSblUsSpkgInfo info;
	unsigned int state;
	int result;

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	if (output) {
		result = SpkgInfoUtilGetSpkgInfo(packageType, &info);
		if (result == 0)
			result = ksceKernelCopyToUser(output, &info, sizeof(info));
	} else {
		result = (int)0x800f0216;
	}
	EXIT_SYSCALL(state);
	return result;
}
