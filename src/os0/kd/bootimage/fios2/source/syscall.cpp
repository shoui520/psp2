/*
 * The syscalls (SceFios2Kernel, and SceFios2Kernel02 for the calls taking a
 * process). Each copies its arguments from user memory to the kernel stack,
 * makes the call and copies the results back. Arguments past the fourth come
 * in a structure in user memory.
 */

#include <alloca.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <psp2kern/kernel/sysclib.h>
#include "fios2_internal.h"

extern "C" {

#define FIOS_SYSCALL	static inline __attribute__((always_inline))

typedef struct SceFiosGetListSyscallArgs {
	SceFiosOverlayID *outIds;
	SceSize maxIds;
	SceSize *outCount;
	SceSize outIdsSize;
	int reserved[2];
} SceFiosGetListSyscallArgs;

typedef struct SceFiosResolveSyncSyscallArgs {
	char *outPath;
	SceSize maxPath;
	int reserved;
	SceSize outPathSize;
	int reserved2[2];
} SceFiosResolveSyncSyscallArgs;

typedef struct SceFiosResolveWithRangeSyncSyscallArgs {
	char *outPath;
	SceSize maxPath;
	uint8_t minOrder;
	uint8_t maxOrder;
	int reserved;
	SceSize outPathSize;
	int reserved2[2];
} SceFiosResolveWithRangeSyncSyscallArgs;

typedef struct SceFiosDHOpenSyncSyscallArgs {
	uint8_t maxOrder;
	int reserved[2];
} SceFiosDHOpenSyncSyscallArgs;

typedef struct SceFiosGetRecommendedSchedulerSyscallArgs {
	int reserved[2];
} SceFiosGetRecommendedSchedulerSyscallArgs;

#define FIOS_GET_LIST_MAX	0x200	/* bytes of overlay IDs a list returns */

/* the result of the call, unless copying a result back failed */
#define FIOS_COPY_OUT(dst, src, size, ret)	({			\
	int __copy = ksceKernelCopyToUser(dst, src, size);		\
	__copy < 0 ? __copy : (ret);					\
})

FIOS_SYSCALL int fiosStrncpyPathFromUser(char *dst, const char *src)	/* name unknown */
{
	int ret = ksceKernelStrncpyFromUser(dst, src, FIOS_PATH_MAX);

	if (ret < 0)
		return ret;
	if (ret == FIOS_PATH_MAX && (signed char)dst[FIOS_PATH_MAX - 1] != '\0')
		return SCE_KERNEL_ERROR_NO_MEMORY;
	return 0;
}

int _sceFiosKernelOverlayAdd(SceFiosOverlay *pOverlay, SceFiosOverlayID *pOutId)
{
	SceFiosOverlay *overlay = pOverlay;
	SceFiosOverlayID *outId;
	int ret;

	if (pOverlay != NULL) {
		overlay = (SceFiosOverlay *)alloca(sizeof(*overlay));
		ret = ksceKernelCopyFromUser(overlay, pOverlay, sizeof(*overlay));
		if (ret < 0)
			return ret;
	}
	if (pOutId == NULL)
		return sceFiosKernelOverlayAdd(overlay, NULL);
	outId = (SceFiosOverlayID *)alloca(sizeof(*outId));
	*outId = 0;
	ret = sceFiosKernelOverlayAdd(overlay, outId);
	return FIOS_COPY_OUT(pOutId, outId, sizeof(*outId), ret);
}

int _sceFiosKernelOverlayRemove(SceFiosOverlayID id)
{
	return sceFiosKernelOverlayRemove(id);
}

int _sceFiosKernelOverlayGetInfo(SceFiosOverlayID id, SceFiosOverlay *pOutOverlay)
{
	SceFiosOverlay *outOverlay;
	int ret;

	if (pOutOverlay == NULL)
		return sceFiosKernelOverlayGetInfo(id, NULL);
	outOverlay = (SceFiosOverlay *)alloca(sizeof(*outOverlay));
	memset(outOverlay, 0, sizeof(*outOverlay));
	ret = sceFiosKernelOverlayGetInfo(id, outOverlay);
	return FIOS_COPY_OUT(pOutOverlay, outOverlay, sizeof(*outOverlay), ret);
}

int _sceFiosKernelOverlayModify(SceFiosOverlayID id, SceFiosOverlay *pOverlay)
{
	SceFiosOverlay *overlay = pOverlay;
	int ret;

	if (pOverlay != NULL) {
		overlay = (SceFiosOverlay *)alloca(sizeof(*overlay));
		ret = ksceKernelCopyFromUser(overlay, pOverlay, sizeof(*overlay));
		if (ret < 0)
			return ret;
	}
	return sceFiosKernelOverlayModify(id, overlay);
}

FIOS_SYSCALL int fiosSyscallAddForProcess(SceUID pid, SceFiosOverlay *pOverlay,
					  SceFiosOverlayID *pOutId)	/* name unknown */
{
	SceFiosOverlay *overlay = pOverlay;
	SceFiosOverlayID *outId;
	int ret;

	if (pOverlay != NULL) {
		overlay = (SceFiosOverlay *)alloca(sizeof(*overlay));
		ret = ksceKernelCopyFromUser(overlay, pOverlay, sizeof(*overlay));
		if (ret < 0)
			return ret;
	}
	if (pOutId == NULL)
		return sceFiosKernelOverlayAddForProcess(pid, overlay, NULL);
	outId = (SceFiosOverlayID *)alloca(sizeof(*outId));
	*outId = 0;
	ret = sceFiosKernelOverlayAddForProcess(pid, overlay, outId);
	return FIOS_COPY_OUT(pOutId, outId, sizeof(*outId), ret);
}

int _sceFiosKernelOverlayAddForProcess(SceUID pid, SceFiosOverlay *pOverlay, SceFiosOverlayID *pOutId)
{
	return fiosSyscallAddForProcess(pid, pOverlay, pOutId);
}

int _sceFiosKernelOverlayRemoveForProcess(SceUID pid, SceFiosOverlayID id)
{
	return sceFiosKernelOverlayRemoveForProcess(pid, id);
}

FIOS_SYSCALL int fiosSyscallGetInfoForProcess(SceUID pid, SceFiosOverlayID id,
					      SceFiosOverlay *pOutOverlay)	/* name unknown */
{
	SceFiosOverlay *outOverlay;
	int ret;

	if (pOutOverlay == NULL)
		return sceFiosKernelOverlayGetInfoForProcess(pid, id, NULL);
	outOverlay = (SceFiosOverlay *)alloca(sizeof(*outOverlay));
	memset(outOverlay, 0, sizeof(*outOverlay));
	ret = sceFiosKernelOverlayGetInfoForProcess(pid, id, outOverlay);
	return FIOS_COPY_OUT(pOutOverlay, outOverlay, sizeof(*outOverlay), ret);
}

int _sceFiosKernelOverlayGetInfoForProcess(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *pOutOverlay)
{
	return fiosSyscallGetInfoForProcess(pid, id, pOutOverlay);
}

FIOS_SYSCALL int fiosSyscallModifyForProcess(SceUID pid, SceFiosOverlayID id,
					     SceFiosOverlay *pOverlay)	/* name unknown */
{
	SceFiosOverlay *overlay = pOverlay;
	int ret;

	if (pOverlay != NULL) {
		overlay = (SceFiosOverlay *)alloca(sizeof(*overlay));
		ret = ksceKernelCopyFromUser(overlay, pOverlay, sizeof(*overlay));
		if (ret < 0)
			return ret;
	}
	return sceFiosKernelOverlayModifyForProcess(pid, id, overlay);
}

int _sceFiosKernelOverlayModifyForProcess(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *pOverlay)
{
	return fiosSyscallModifyForProcess(pid, id, pOverlay);
}

FIOS_SYSCALL int fiosSyscallGetList(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
				    SceFiosGetListSyscallArgs *pArgs)	/* name unknown */
{
	SceFiosGetListSyscallArgs args;
	SceFiosOverlayID *outIds;
	SceSize *outCount;
	int ret;

	ret = ksceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	outIds = args.outIds;
	if (outIds != NULL) {
		if (args.outIdsSize > FIOS_GET_LIST_MAX)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		outIds = (SceFiosOverlayID *)alloca(FIOS_GET_LIST_MAX);
		memset(outIds, 0, FIOS_GET_LIST_MAX);
	}
	outCount = args.outCount;
	if (outCount != NULL) {
		outCount = (SceSize *)alloca(sizeof(*outCount));
		*outCount = 0;
	}
	ret = sceFiosKernelOverlayGetList(pid, minOrder, maxOrder, outIds, args.maxIds, outCount);
	if (args.outIds != NULL) {
		int copy = ksceKernelCopyToUser(args.outIds, outIds, args.outIdsSize);
		if (copy < 0)
			return copy;
	}
	if (args.outCount == NULL)
		return ret;
	return FIOS_COPY_OUT(args.outCount, outCount, sizeof(*outCount), ret);
}

int _sceFiosKernelOverlayGetList(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
				 SceFiosGetListSyscallArgs *pArgs)
{
	return fiosSyscallGetList(pid, minOrder, maxOrder, pArgs);
}

FIOS_SYSCALL int fiosSyscallResolveSync(SceUID pid, int resolveFlag, const char *pInPath,
					SceFiosResolveSyncSyscallArgs *pArgs)	/* name unknown */
{
	SceFiosResolveSyncSyscallArgs args;
	const char *inPath = pInPath;
	char *outPath;
	int ret;

	ret = ksceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (pInPath != NULL) {
		char *path = (char *)alloca(FIOS_PATH_MAX);
		ret = fiosStrncpyPathFromUser(path, pInPath);
		if (ret < 0)
			return ret;
		inPath = path;
	}
	if (args.outPath == NULL)
		return sceFiosKernelOverlayResolveSync(pid, resolveFlag, inPath, NULL, args.maxPath);
	if (args.outPathSize > FIOS_PATH_MAX)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	outPath = (char *)alloca(FIOS_PATH_MAX);
	memset(outPath, 0, FIOS_PATH_MAX);
	ret = sceFiosKernelOverlayResolveSync(pid, resolveFlag, inPath, outPath, args.maxPath);
	return FIOS_COPY_OUT(args.outPath, outPath, args.outPathSize, ret);
}

int _sceFiosKernelOverlayResolveSync(SceUID pid, int resolveFlag, const char *pInPath,
				     SceFiosResolveSyncSyscallArgs *pArgs)
{
	return fiosSyscallResolveSync(pid, resolveFlag, pInPath, pArgs);
}

FIOS_SYSCALL int fiosSyscallResolveWithRangeSync(SceUID pid, int resolveFlag, const char *pInPath,
		SceFiosResolveWithRangeSyncSyscallArgs *pArgs)	/* name unknown */
{
	SceFiosResolveWithRangeSyncSyscallArgs args;
	const char *inPath = pInPath;
	char *outPath;
	int ret;

	ret = ksceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (pInPath != NULL) {
		char *path = (char *)alloca(FIOS_PATH_MAX);
		ret = fiosStrncpyPathFromUser(path, pInPath);
		if (ret < 0)
			return ret;
		inPath = path;
	}
	if (args.outPath == NULL)
		return sceFiosKernelOverlayResolveWithRangeSync(pid, resolveFlag, inPath, NULL,
							       args.maxPath, args.minOrder,
							       args.maxOrder);
	if (args.outPathSize > FIOS_PATH_MAX)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	outPath = (char *)alloca(FIOS_PATH_MAX);
	memset(outPath, 0, FIOS_PATH_MAX);
	ret = sceFiosKernelOverlayResolveWithRangeSync(pid, resolveFlag, inPath, outPath, args.maxPath,
						       args.minOrder, args.maxOrder);
	return FIOS_COPY_OUT(args.outPath, outPath, args.outPathSize, ret);
}

int _sceFiosKernelOverlayResolveWithRangeSync(SceUID pid, int resolveFlag, const char *pInPath,
					      SceFiosResolveWithRangeSyncSyscallArgs *pArgs)
{
	return fiosSyscallResolveWithRangeSync(pid, resolveFlag, pInPath, pArgs);
}

int _sceFiosKernelOverlayDHOpenSync(SceFiosKernelDH **pOutDH, const char *pPath, uint8_t minOrder,
				    SceFiosDHOpenSyncSyscallArgs *pArgs)
{
	SceFiosDHOpenSyncSyscallArgs args;
	SceFiosKernelDH **outDH = pOutDH;
	const char *path = pPath;
	int ret;

	ret = ksceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (pOutDH != NULL) {
		outDH = (SceFiosKernelDH **)alloca(sizeof(*outDH));
		*outDH = NULL;
	}
	if (pPath != NULL) {
		char *buffer = (char *)alloca(FIOS_PATH_MAX);
		ret = fiosStrncpyPathFromUser(buffer, pPath);
		if (ret < 0)
			return ret;
		path = buffer;
	}
	ret = sceFiosKernelOverlayDHOpenSync(outDH, path, minOrder, args.maxOrder);
	if (pOutDH == NULL)
		return ret;
	return FIOS_COPY_OUT(pOutDH, outDH, sizeof(*outDH), ret);
}

int _sceFiosKernelOverlayDHReadSync(SceFiosKernelDH *dh, SceIoDirent *pOutDirent)
{
	SceIoDirent *outDirent;
	int ret;

	if (pOutDirent == NULL)
		return sceFiosKernelOverlayDHReadSync(dh, NULL);
	outDirent = (SceIoDirent *)alloca(sizeof(*outDirent));
	memset(outDirent, 0, sizeof(*outDirent));
	ret = sceFiosKernelOverlayDHReadSync(dh, outDirent);
	return FIOS_COPY_OUT(pOutDirent, outDirent, sizeof(*outDirent), ret);
}

int _sceFiosKernelOverlayDHCloseSync(SceFiosKernelDH *dh)
{
	return sceFiosKernelOverlayDHCloseSync(dh);
}

int _sceFiosKernelOverlayDHStatSync(SceFiosKernelDH *dh, SceIoStat *pOutStat)
{
	SceIoStat *outStat;
	int ret;

	if (pOutStat == NULL)
		return sceFiosKernelOverlayDHStatSync(dh, NULL);
	outStat = (SceIoStat *)alloca(sizeof(*outStat));
	memset(outStat, 0, sizeof(*outStat));
	ret = sceFiosKernelOverlayDHStatSync(dh, outStat);
	return FIOS_COPY_OUT(pOutStat, outStat, sizeof(*outStat), ret);
}

int _sceFiosKernelOverlayDHChstatSync(SceFiosKernelDH *dh, SceIoStat *pStat, int bits)
{
	SceIoStat *stat = pStat;
	int ret;

	if (pStat != NULL) {
		stat = (SceIoStat *)alloca(sizeof(*stat));
		ret = ksceKernelCopyFromUser(stat, pStat, sizeof(*stat));
		if (ret < 0)
			return ret;
	}
	return sceFiosKernelOverlayDHChstatSync(dh, stat, bits);
}

int _sceFiosKernelOverlayDHSyncSync(SceFiosKernelDH *dh, int flag)
{
	return sceFiosKernelOverlayDHSyncSync(dh, flag);
}

FIOS_SYSCALL int fiosSyscallThreadSetDisabled(int disabled)	/* name unknown */
{
	return sceFiosKernelOverlayThreadSetDisabled(disabled);
}

int _sceFiosKernelOverlayThreadSetDisabled(int disabled)
{
	return fiosSyscallThreadSetDisabled(disabled);
}

int _sceFiosKernelOverlayThreadIsDisabled(void)
{
	return sceFiosKernelOverlayThreadIsDisabled();
}

FIOS_SYSCALL int fiosSyscallGetRecommendedScheduler(int avail, const char *pPath,
		SceFiosGetRecommendedSchedulerSyscallArgs *pArgs)	/* name unknown */
{
	SceFiosGetRecommendedSchedulerSyscallArgs args;
	const char *path = pPath;
	int ret;

	ret = ksceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (pPath != NULL) {
		char *buffer = (char *)alloca(FIOS_PATH_MAX);
		ret = fiosStrncpyPathFromUser(buffer, pPath);
		if (ret < 0)
			return ret;
		path = buffer;
	}
	return sceFiosKernelOverlayGetRecommendedScheduler(avail, path);
}

int _sceFiosKernelOverlayGetRecommendedScheduler(int avail, const char *pPath,
		SceFiosGetRecommendedSchedulerSyscallArgs *pArgs)
{
	return fiosSyscallGetRecommendedScheduler(avail, pPath, pArgs);
}

/* SceFios2Kernel02: the same calls taking a process */

int sceFiosKernelOverlayAddForProcess02(SceUID pid, SceFiosOverlay *pOverlay, SceFiosOverlayID *pOutId)
{
	return fiosSyscallAddForProcess(pid, pOverlay, pOutId);
}

int sceFiosKernelOverlayRemoveForProcess02(SceUID pid, SceFiosOverlayID id)
{
	return sceFiosKernelOverlayRemoveForProcess(pid, id);
}

int sceFiosKernelOverlayGetInfoForProcess02(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *pOutOverlay)
{
	return fiosSyscallGetInfoForProcess(pid, id, pOutOverlay);
}

int sceFiosKernelOverlayModifyForProcess02(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *pOverlay)
{
	return fiosSyscallModifyForProcess(pid, id, pOverlay);
}

int sceFiosKernelOverlayGetList02(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
				  SceFiosGetListSyscallArgs *pArgs)
{
	return fiosSyscallGetList(pid, minOrder, maxOrder, pArgs);
}

int sceFiosKernelOverlayResolveSync02(SceUID pid, int resolveFlag, const char *pInPath,
				      SceFiosResolveSyncSyscallArgs *pArgs)
{
	return fiosSyscallResolveSync(pid, resolveFlag, pInPath, pArgs);
}

int sceFiosKernelOverlayResolveWithRangeSync02(SceUID pid, int resolveFlag, const char *pInPath,
					       SceFiosResolveWithRangeSyncSyscallArgs *pArgs)
{
	return fiosSyscallResolveWithRangeSync(pid, resolveFlag, pInPath, pArgs);
}

int sceFiosKernelOverlayThreadSetDisabled02(int disabled)
{
	return fiosSyscallThreadSetDisabled(disabled);
}

int sceFiosKernelOverlayThreadIsDisabled02(void)
{
	return sceFiosKernelOverlayThreadIsDisabled();
}

int sceFiosKernelOverlayGetRecommendedScheduler02(int avail, const char *pPath,
		SceFiosGetRecommendedSchedulerSyscallArgs *pArgs)
{
	return fiosSyscallGetRecommendedScheduler(avail, pPath, pArgs);
}

} /* extern "C" */
