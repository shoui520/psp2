/*
 * SceFios2KernelForDriver: the kernel API. Each call runs with TPIDRURO
 * shifted, as an exported syscall does.
 */

#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <intrmgr/syscall.h>
#include "fios2_internal.h"

extern "C" int SceIofilemgrForDriver_43170575(SceUID fd, int flag);	/* name unknown */

/* the calling thread's state, NULL when it has none (and create is 0) */
static inline SceFiosKernelThreadState *fiosThreadState(int create)	/* name unknown */
{
	int slot = g_fiosTlsSlot;

	if (slot == -1)
		return NULL;
	return (SceFiosKernelThreadState *)SceThreadmgrForDriver_89CA5698(slot,
		sceKernelGetThreadId(), create);
}

int SceFios2KernelForDriver_2607EE4C(SceFiosOverlay *overlay, SceFiosOverlayID *outId)
{
	SceUID pid = overlay != NULL ? overlay->pid : SceThreadmgrForDriver_FA54D49A();
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (overlay == NULL || outId == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else
		ret = sceFiosKernelOverlayAddInternal(pid, overlay, outId);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_17E65A1C(SceUID pid, SceFiosOverlay *overlay, SceFiosOverlayID *outId)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (overlay == NULL || outId == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else
		ret = sceFiosKernelOverlayAddInternal(pid, overlay, outId);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_2368FEB5(SceFiosOverlayID id)
{
	SceUID pid = SceThreadmgrForDriver_FA54D49A();
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = sceFiosKernelOverlayRemoveInternal(pid, id);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_23247EFB(SceUID pid, SceFiosOverlayID id)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = sceFiosKernelOverlayRemoveInternal(pid, id);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_725E6817(SceFiosOverlayID id, SceFiosOverlay *outOverlay)
{
	SceUID pid = SceThreadmgrForDriver_FA54D49A();
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (outOverlay == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else
		ret = sceFiosKernelOverlayGetInfoInternal(pid, id, outOverlay);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_F1762BC2(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *outOverlay)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (outOverlay == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else
		ret = sceFiosKernelOverlayGetInfoInternal(pid, id, outOverlay);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_7F8B960C(SceFiosOverlayID id, SceFiosOverlay *overlay)
{
	SceUID pid = SceThreadmgrForDriver_FA54D49A();
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (overlay == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else
		ret = sceFiosKernelOverlayModifyInternal(pid, id, overlay);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_853EA82A(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *overlay)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (overlay == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else
		ret = sceFiosKernelOverlayModifyInternal(pid, id, overlay);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_FF42AAF0(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
				 SceFiosOverlayID *outIds, SceSize maxIds, SceSize *outCount)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = sceFiosKernelOverlayGetListInternal(pid, minOrder, maxOrder, outIds, maxIds, outCount);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_E71192C5(void);

int SceFios2KernelForDriver_0F456345(SceUID pid, int resolveFlag, const char *inPath, char *outPath,
				     SceSize maxPath)
{
	SceFiosKernelResolveParam param;
	SceFiosKernelResolveArgs args;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	param.minOrder = SceFios2KernelForDriver_E71192C5() ? 0x80 : 0;
	args.param = &param;
	param.maxPath = maxPath;
	param.maxOrder = 0xff;
	args.inPath = inPath;
	args.outPath = outPath;
	param.pid = pid;
	param.resolveFlag = resolveFlag;
	if (inPath == NULL || outPath == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else
		ret = (int)sceKernelExtendKernelStackWide(0x2000,
			(ThreadmgrStackFunction)sceFiosKernelOverlayResolveCallback, &args);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_D3D968FC(SceUID pid, int resolveFlag, const char *inPath,
					      char *outPath, SceSize maxPath, uint8_t minOrder,
					      uint8_t maxOrder)
{
	SceFiosKernelResolveParam param;
	SceFiosKernelResolveArgs args;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (SceFios2KernelForDriver_E71192C5())
		minOrder = 0x80;
	args.param = &param;
	param.maxPath = maxPath;
	param.minOrder = minOrder;
	param.maxOrder = maxOrder;
	args.inPath = inPath;
	args.outPath = outPath;
	param.pid = pid;
	param.resolveFlag = resolveFlag;
	if (inPath == NULL || outPath == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else
		ret = (int)sceKernelExtendKernelStackWide(0x2000,
			(ThreadmgrStackFunction)sceFiosKernelOverlayResolveCallback, &args);
	sceSyscallLeave(state);
	return ret;
}

/* opens a directory handle in the caller's process heap */
int SceFios2KernelForDriver_3B329E86(SceFiosKernelDH **outDH, const char *path, unsigned int minOrder,
				     unsigned int maxOrder)
{
	SceFiosKernelProcessStorage *storage = NULL;
	SceUInt32 state = sceSyscallEnter();
	void *buffer;
	int size, ret, i;

	if (path == NULL || outDH == NULL) {
		ret = SCE_FIOS_ERROR_BAD_PTR;
		goto out;
	}
	*outDH = NULL;
	ret = SceProcessmgrForDriver_AF80F39C(SceThreadmgrForDriver_FA54D49A(), g_fiosPlsKey,
							 (void **)&storage, 1);
	if (ret != 0)
		goto out;
	if (storage == NULL) {
		ret = SCE_FIOS_ERROR_CANT_ALLOCATE_DH;
		goto out;
	}
	for (i = 0; i < FIOS_DH_MAX; i++) {
		if (SceCpuForDriver_CDA96E81((SceInt32 *)&storage->dh[i], 0, -1) != 0)
			continue;
		/* the first pass measures, the second builds */
		size = sceFiosKernelDHOpenInternal(SceThreadmgrForDriver_FA54D49A(), outDH, path,
						   minOrder, maxOrder, NULL, 0, NULL);
		ret = size;
		if (size >= 0) {
			buffer = sceKernelAllocOwnProcessHeap(size, NULL);
			if (buffer == NULL) {
				ret = SCE_FIOS_ERROR_CANT_ALLOCATE_DH;
			} else {
				ret = sceFiosKernelDHOpenInternal(SceThreadmgrForDriver_FA54D49A(), outDH,
								  path, minOrder, maxOrder, buffer,
								  size, NULL);
				if (ret >= 0) {
					storage->dh[i] = *outDH;
					ret = 0;
					goto out;
				}
				SceProcessmgrForDriver_FC2A424E(buffer);
				*outDH = NULL;
			}
		}
		if (storage != NULL)
			storage->dh[i] = NULL;
		goto out;
	}
	ret = SCE_FIOS_ERROR_CANT_ALLOCATE_DH;
out:
	sceSyscallLeave(state);
	return ret;
}

/* reads the next entry of a directory handle */
int SceFios2KernelForDriver_28E28A58(SceFiosKernelDH *dh, SceIoDirent *dirent)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (dirent == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else
		ret = sceFiosKernelDHRead(dh, dirent, NULL, NULL);
	sceSyscallLeave(state);
	return ret;
}

/* the handle leaves the process's table */
static inline int fiosDHForget(SceFiosKernelDH *dh)	/* name unknown */
{
	SceFiosKernelProcessStorage *storage = NULL;
	int i;

	if (SceProcessmgrForDriver_AF80F39C(SceThreadmgrForDriver_FA54D49A(), g_fiosPlsKey,
						       (void **)&storage, 1) == 0) {
		if (storage == NULL)
			return 0;
		for (i = 0; i < FIOS_DH_MAX; i++) {
			if (storage->dh[i] == dh) {
				storage->dh[i] = NULL;
				return 0;
			}
		}
	}
	return SCE_FIOS_ERROR_BAD_DH;
}

/* closes a directory handle of the caller's */
int SceFios2KernelForDriver_DD7627EC(SceFiosKernelDH *dh)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = sceFiosKernelDHClose(dh);
	if (dh != NULL && ret == 0)
		sceKernelFreeRemoteProcessHeap(0, dh);
	else if (ret != 0)
		goto out;
	ret = fiosDHForget(dh);
out:
	sceSyscallLeave(state);
	return ret;
}

/* closes a directory handle of pid's */
int sceFiosKernelDHFree(SceUID pid, SceFiosKernelDH *dh)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = sceFiosKernelDHClose(dh);
	if (dh != NULL && ret == 0)
		sceKernelFreeRemoteProcessHeap(pid, dh);
	else if (ret != 0)
		goto out;
	ret = fiosDHForget(dh);
out:
	sceSyscallLeave(state);
	return ret;
}

/* the stat of the directory a handle lists */
int SceFios2KernelForDriver_2649408B(SceFiosKernelDH *dh, SceIoStat *stat)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (stat == NULL) {
		ret = SCE_FIOS_ERROR_BAD_PTR;
	} else if (dh == NULL || dh->magic != SCE_FIOS_KERNEL_DH_MAGIC) {
		ret = SCE_FIOS_ERROR_BAD_DH;
	} else if (dh->fd != -1 && SceIofilemgrForDriver_462F059B(dh->fd, stat) == 0) {
		ret = 0;
	} else {
		/* only overlays name it */
		memset(stat, 0, sizeof(*stat));
		ret = 0;
		stat->st_mode = SCE_STM_FDIR | SCE_STM_RWU | IO_STM_RWS;
	}
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_35E7E75C(SceFiosKernelDH *dh, SceIoStat *stat, int bits)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (stat == NULL)
		ret = SCE_FIOS_ERROR_BAD_PTR;
	else if (dh == NULL || dh->magic != SCE_FIOS_KERNEL_DH_MAGIC)
		ret = SCE_FIOS_ERROR_BAD_DH;
	else if (dh->fd == -1)
		ret = SCE_FIOS_ERROR_READ_ONLY;
	else
		ret = SceIofilemgrForDriver_DF57A75F(dh->fd, stat, bits);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_990F46A5(SceFiosKernelDH *dh, int flag)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (dh == NULL || dh->magic != SCE_FIOS_KERNEL_DH_MAGIC)
		ret = SCE_FIOS_ERROR_BAD_DH;
	else if (dh->fd == -1)
		ret = SCE_FIOS_ERROR_READ_ONLY;
	else
		ret = SceIofilemgrForDriver_43170575(dh->fd, flag);
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_03727E5E(int disabled)
{
	SceFiosKernelThreadState *thread;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	thread = fiosThreadState(1);
	if (thread == NULL) {
		ret = -1;
	} else {
		thread->overlayDisabled = disabled != 0;
		ret = 0;
	}
	sceSyscallLeave(state);
	return ret;
}

int SceFios2KernelForDriver_E71192C5(void)
{
	SceFiosKernelThreadState *thread;
	SceUInt32 state = sceSyscallEnter();

	thread = fiosThreadState(0);
	if (thread != NULL) {
		sceSyscallLeave(state);
		return thread->overlayDisabled;
	}
	sceSyscallLeave(state);
	return 0;
}

/* a host0..host9 path is served best by the host scheduler */
int SceFios2KernelForDriver_241BF0D6(int avail, const char *path)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (avail <= 1 || path == NULL || strlen(path) <= 5) {
		ret = 0;
	} else {
		ret = path[0] == 'h' && path[1] == 'o' && path[2] == 's' && path[3] == 't' &&
		      path[5] == ':' && (unsigned int)(path[4] - '0') <= 9;
	}
	sceSyscallLeave(state);
	return ret;
}
