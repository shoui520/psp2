/*
 * SceDisplay: syscalls taking structures (copied through the kernel stack).
 * File name unknown; the module links its parts in this order.
 */
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/intrmgr.h>
#include <systimer/systimer.h>
#include <lowio/iftu.h>
#include <display/display.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include "display_internal.h"

/*
 * Syscalls. A kernel error returned to user mode gets bit 30 set; the user's
 * structures are copied through the kernel stack (up to 0x1c bytes).
 */
#define SYSCALL_COPY_MAX	0x1c

static __attribute__((noinline)) int syscallSetFrameBufInternal(SceUInt32 head, SceUInt32 plane,
		const SceDisplayFrameBuf *pParam, SceUInt32 sync) /* name unknown */
{
	int ret = sceDisplaySetFrameBufInternal(head, plane, pParam, sync);

	if (ret < 0)
		ret |= 0x40000000;
	return ret;
}

static __attribute__((noinline)) int syscallGetFrameBufInternal(SceUInt32 head, SceUInt32 plane,
		SceDisplayFrameBuf *pParam, SceUInt32 sync) /* name unknown */
{
	int ret = sceDisplayGetFrameBufInternal(head, plane, pParam, sync);

	if (ret < 0)
		ret |= 0x40000000;
	return ret;
}

static __attribute__((noinline)) int syscallSetFrameBuf(const SceDisplayFrameBuf *pParam,
							SceUInt32 sync) /* name unknown */
{
	int ret = sceDisplaySetFrameBuf(pParam, sync);

	if (ret < 0)
		ret |= 0x40000000;
	return ret;
}

static __attribute__((noinline)) int syscallGetFrameBuf(SceDisplayFrameBuf *pParam,
							SceUInt32 sync) /* name unknown */
{
	int ret = sceDisplayGetFrameBuf(pParam, sync);

	if (ret < 0)
		ret |= 0x40000000;
	return ret;
}

static __attribute__((noinline)) int syscallGetResolutionInfoInternal(SceUInt32 head,
		SceDisplayResolutionInfo *info) /* name unknown */
{
	int ret = sceDisplayGetResolutionInfoInternal(head, info);

	if (ret < 0)
		ret |= 0x40000000;
	return ret;
}

static __attribute__((noinline)) int syscallGetMaximumFrameBufResolution(int *width,
		int *height) /* name unknown */
{
	int ret = SceDisplayForDriver_5AFE6CD3(width, height);

	if (ret < 0)
		ret |= 0x40000000;
	return ret;
}

int _sceDisplayGetMaximumFrameBufResolution(int *pWidth, int *pHeight)
{
	int *width = NULL, *height = NULL;
	int ret, res;

	if (pWidth != NULL) {
		width = __builtin_alloca(sizeof(*width));
		*width = 0;
	}
	if (pHeight != NULL) {
		height = __builtin_alloca(sizeof(*height));
		*height = 0;
	}
	ret = syscallGetMaximumFrameBufResolution(width, height);
	if (pWidth != NULL) {
		res = sceKernelCopyToUser(pWidth, width, sizeof(*width));
		if (res < 0)
			return res;
	}
	if (pHeight != NULL) {
		res = sceKernelCopyToUser(pHeight, height, sizeof(*height));
		if (res < 0)
			return res;
	}
	return ret;
}

typedef struct DisplaySetFrameBufInternalArgs {
	SceUInt32 sync;
	SceSize size;			/* of the frame buffer structure */
	SceUInt32 reserved[2];
} DisplaySetFrameBufInternalArgs;

int _sceDisplaySetFrameBufInternal(SceUInt32 head, SceUInt32 plane, const SceDisplayFrameBuf *pParam,
				   const DisplaySetFrameBufInternalArgs *pArgs)
{
	DisplaySetFrameBufInternalArgs args;
	SceDisplayFrameBuf *frameBuf;
	int ret;

	ret = sceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	frameBuf = NULL;
	if (pParam != NULL) {
		if (args.size > SYSCALL_COPY_MAX)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		frameBuf = __builtin_alloca(SYSCALL_COPY_MAX);
		ret = sceKernelCopyFromUser(frameBuf, pParam, args.size);
		if (ret < 0)
			return ret;
	}
	return syscallSetFrameBufInternal(head, plane, frameBuf, args.sync);
}

typedef struct DisplaySetFrameBufForCompatArgs {
	int cropHeight;
	const SceDisplayFrameBuf *pParam;
	const SceDisplayFrameBuf *convDst;
	SceSize paramSize;
	SceSize convDstSize;
	SceUInt32 reserved;
} DisplaySetFrameBufForCompatArgs;

int _sceDisplaySetFrameBufForCompat(int x, int y, int cropWidth,
				    const DisplaySetFrameBufForCompatArgs *pArgs)
{
	DisplaySetFrameBufForCompatArgs args;
	SceDisplayFrameBuf *frameBuf, *convDst;
	int ret;

	ret = sceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	frameBuf = NULL;
	if (args.pParam != NULL) {
		if (args.paramSize > SYSCALL_COPY_MAX)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		frameBuf = __builtin_alloca(SYSCALL_COPY_MAX);
		ret = sceKernelCopyFromUser(frameBuf, args.pParam, args.paramSize);
		if (ret < 0)
			return ret;
	}
	convDst = NULL;
	if (args.convDst != NULL) {
		if (args.convDstSize > sizeof(SceDisplayFrameBuf))
			return SCE_KERNEL_ERROR_NO_MEMORY;
		convDst = __builtin_alloca(sizeof(SceDisplayFrameBuf));
		ret = sceKernelCopyFromUser(convDst, args.convDst, args.convDstSize);
		if (ret < 0)
			return ret;
	}
	return setFrameBufForCompat(x, y, cropWidth, args.cropHeight, frameBuf, convDst);
}

typedef struct DisplayGetFrameBufInternalArgs {
	SceUInt32 sync;
	SceSize size;			/* of the frame buffer structure */
	SceUInt32 reserved[2];
} DisplayGetFrameBufInternalArgs;

int _sceDisplayGetFrameBufInternal(SceUInt32 head, SceUInt32 plane, SceDisplayFrameBuf *pParam,
				   const DisplayGetFrameBufInternalArgs *pArgs)
{
	DisplayGetFrameBufInternalArgs args;
	SceDisplayFrameBuf *frameBuf;
	int ret, res;

	ret = sceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (pParam == NULL)
		return syscallGetFrameBufInternal(head, plane, NULL, args.sync);
	if (args.size > SYSCALL_COPY_MAX)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	frameBuf = __builtin_alloca(SYSCALL_COPY_MAX);
	memset(frameBuf, 0, SYSCALL_COPY_MAX);
	ret = sceKernelCopyFromUser(frameBuf, pParam, args.size);
	if (ret < 0)
		return ret;
	ret = syscallGetFrameBufInternal(head, plane, frameBuf, args.sync);
	res = sceKernelCopyToUser(pParam, frameBuf, args.size);
	return res < 0 ? res : ret;
}

typedef struct DisplaySizeArgs {
	SceSize size;			/* of the structure passed */
	SceUInt32 reserved;
} DisplaySizeArgs;

int _sceDisplaySetFrameBuf(const SceDisplayFrameBuf *pParam, SceUInt32 sync, const DisplaySizeArgs *pArgs)
{
	DisplaySizeArgs args;
	SceDisplayFrameBuf *frameBuf;
	int ret;

	ret = sceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	frameBuf = NULL;
	if (pParam != NULL) {
		if (args.size > SYSCALL_COPY_MAX)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		frameBuf = __builtin_alloca(SYSCALL_COPY_MAX);
		ret = sceKernelCopyFromUser(frameBuf, pParam, args.size);
		if (ret < 0)
			return ret;
	}
	return syscallSetFrameBuf(frameBuf, sync);
}

int _sceDisplayGetFrameBuf(SceDisplayFrameBuf *pParam, SceUInt32 sync, const DisplaySizeArgs *pArgs)
{
	DisplaySizeArgs args;
	SceDisplayFrameBuf *frameBuf;
	int ret, res;

	ret = sceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (pParam == NULL)
		return syscallGetFrameBuf(NULL, sync);
	if (args.size > SYSCALL_COPY_MAX)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	frameBuf = __builtin_alloca(SYSCALL_COPY_MAX);
	memset(frameBuf, 0, SYSCALL_COPY_MAX);
	ret = sceKernelCopyFromUser(frameBuf, pParam, args.size);
	if (ret < 0)
		return ret;
	ret = syscallGetFrameBuf(frameBuf, sync);
	res = sceKernelCopyToUser(pParam, frameBuf, args.size);
	return res < 0 ? res : ret;
}

int _sceDisplayGetResolutionInfoInternal(SceUInt32 head, SceDisplayResolutionInfo *pInfo,
					 const DisplaySizeArgs *pArgs)
{
	DisplaySizeArgs args;
	SceDisplayResolutionInfo *info;
	int ret, res;

	ret = sceKernelCopyFromUser(&args, pArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (pInfo == NULL)
		return syscallGetResolutionInfoInternal(head, NULL);
	if (args.size > SYSCALL_COPY_MAX)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(SYSCALL_COPY_MAX);
	memset(info, 0, SYSCALL_COPY_MAX);
	ret = sceKernelCopyFromUser(info, pInfo, args.size);
	if (ret < 0)
		return ret;
	ret = syscallGetResolutionInfoInternal(head, info);
	res = sceKernelCopyToUser(pInfo, info, args.size);
	return res < 0 ? res : ret;
}
