#include <sysmem/sysroot.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "update_internal.h"
#include "spkg.h"

int ksceSblAimgrIsCEX(void);
int UpdateUtilLockPupHeader(void);
int UpdateUtilUnlockPupHeader(void);
int UpdateUtilCheckBuffer(SceSize size, void *buffer);
int UpdateUtilSetBufferState(unsigned int state, void *buffer);
int UpdateUtilInitializeNodeStatus(unsigned int nodeType,
		const UpdateUtilNodeStatus *status);
int sceSblUsUpdateSpackageBH(SceSize arglen, SpkgThreadArgs *args);
extern uint32_t updateRequestIdCounter;

enum SpkgPackageOperation {
	SPKG_OPERATION_INSPECT,
	SPKG_OPERATION_UPDATE,
	SPKG_OPERATION_EXTRACT,
	SPKG_OPERATION_COUNT
};

typedef struct SpkgPackagePolicy {
	uint32_t allowed[SPKG_OPERATION_COUNT];
} SpkgPackagePolicy;

static const SpkgPackagePolicy spkgPackagePolicies[29] = {
	[1] = { .allowed = { 1, 1, 0 } },
	[3] = { .allowed = { 0, 0, 1 } },
	[4] = { .allowed = { 0, 0, 1 } },
	[8] = { .allowed = { 1, 1, 0 } },
	[9] = { .allowed = { 1, 1, 0 } },
	[10] = { .allowed = { 1, 1, 1 } },
	[11] = { .allowed = { 1, 1, 0 } },
	[12] = { .allowed = { 1, 1, 0 } },
	[13] = { .allowed = { 1, 1, 0 } },
	[15] = { .allowed = { 1, 1, 0 } },
	[16] = { .allowed = { 1, 1, 0 } },
	[17] = { .allowed = { 1, 1, 0 } },
	[18] = { .allowed = { 1, 1, 0 } },
	[19] = { .allowed = { 1, 1, 0 } },
	[20] = { .allowed = { 1, 1, 0 } },
	[21] = { .allowed = { 1, 1, 1 } },
	[22] = { .allowed = { 1, 1, 1 } },
	[23] = { .allowed = { 1, 1, 1 } },
	[24] = { .allowed = { 1, 1, 1 } },
	[25] = { .allowed = { 0, 1, 1 } },
	[27] = { .allowed = { 0, 0, 1 } },
	[28] = { .allowed = { 1, 1, 1 } }
};

_Static_assert(sizeof(SpkgPackagePolicy) == 12, "s-package operation policy size");

int SpkgUtilSpackageTopHalf(unsigned int nodeType, int packageType,
		void *buffer, SceSize size, int flags, uint32_t *requestId)
{
	SpkgThreadArgs args;
	UpdateUtilNodeStatus status;
	enum SpkgPackageOperation operation;
	SceUID thread;
	uint32_t id;
	int result, permission;

	permission = ksceKernelSetPermission(128);
	if (permission < 0)
		return permission;
	if (!buffer || !size || !requestId)
		goto invalid;
	if (SceSysrootForDriver_ksceSysrootIsManufacturingMode()) {
		if ((flags & 5) == 5)
			goto authorized;
	} else if (!ksceSblAimgrIsCEX() && (flags & 5) == 5) {
		goto authorized;
	}
	if ((flags & 9) != 9)
		goto invalid;
authorized:
	if (nodeType - 1 > 2)
		goto invalid;
	if (nodeType == 2)
		operation = SPKG_OPERATION_INSPECT;
	else if (nodeType == 3)
		operation = SPKG_OPERATION_EXTRACT;
	else
		operation = SPKG_OPERATION_UPDATE;
	if ((unsigned int)packageType - 1 > 27 ||
			!spkgPackagePolicies[packageType].allowed[operation]) {
		result = (int)0x800f0202;
		goto restorePermission;
	}
	UpdateUtilLockPupHeader();
	if (UpdateUtilCheckBuffer(size, buffer)) {
		result = (int)0x800f0216;
		goto unlock;
	}
	result = UpdateUtilSetBufferState(1, buffer);
	if (result)
		goto unlock;
	id = ++updateRequestIdCounter;
	if (!id)
		id = 1;
	result = UpdateUtilGetNodeStatusLocked(nodeType, &status);
	if (result != (int)0x800f0229 && result)
		goto unlock;
	if (!result && status.progress != 5) {
		result = (int)0x800f0210;
		goto unlock;
	}
	status.buffer = buffer;
	status.requestId = id;
	status.seq_no = 1;
	status.result = 0;
	status.progress = 1;
	status.written_rates = 0;
	result = UpdateUtilInitializeNodeStatus(nodeType, &status);
	if (result)
		goto unlock;
	thread = UpdateUtilCreateThread("SceSblUsUpdateSpackageBH",
			(SceKernelThreadEntry)sceSblUsUpdateSpackageBH,
			96, 8192, 0x50000, 0x80000, NULL);
	result = thread;
	if (thread < 0)
		goto unlock;
	args.nodeType = nodeType;
	args.packageType = packageType;
	args.requestId = id;
	args.buffer = buffer;
	args.size = size;
	args.flags = flags;
	args.pid = sceKernelGetProcessId();
	result = ksceKernelStartThread(thread, sizeof(args), &args);
	if (result)
		ksceKernelDeleteThread(thread);
	else
		*requestId = id;
unlock:
	UpdateUtilUnlockPupHeader();
restorePermission:
	ksceKernelSetPermission(permission);
	return result;
invalid:
	ksceKernelSetPermission(permission);
	return (int)0x800f0216;
}
