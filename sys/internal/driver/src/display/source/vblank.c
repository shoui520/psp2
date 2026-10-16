/*
 * SceDisplay: vblank interrupts, vcounts and the waits on them.
 * File name unknown; the module links its parts in this order.
 */
#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include "sce/sysmem.h"
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/systimer.h>
#include <psp2kern/kernel/sysmem/uid_guid.h>
#include <psp2kern/lowio/iftu.h>
#include "sce/display.h"
#include "sce/syscall.h"
#include "sce/error.h"
#include "display_internal.h"
#include "display_imports.h"
#include "../../../../kernel/src/threadmgr/source/uid_waitable.h"

static DisplaySgiState sgiState[2];	/* SGI 8, 9 */
static SceSysTimerId vblankTimer;
static int vblankWaitKey;		/* thread local DisplayVblankWait */
static struct ThreadmgrWaitableClass vblankEventClass;

static int vblankEventCreate(void *object) /* name unknown */
{
	return 0;
}

static int vblankEventDestroy(void *object) /* name unknown */
{
	return 0;
}

static int sgiNop(int intr_code, int subintr_code) /* name unknown */
{
	return 0;
}

static int sgiSet(int intr_code, int subintr_code, int enable) /* name unknown */
{
	DisplaySgiState *state = &sgiState[intr_code - 8];
	SceKernelIntrStatus intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	state->enabled = (state->enabled & ~(1 << subintr_code)) | ((enable != 0) << subintr_code);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

static int sgiTake(int intr_code, int subintr_code, int *enabled) /* name unknown */
{
	DisplaySgiState *state = &sgiState[intr_code - 8];
	SceKernelIntrStatus intr;
	SceUInt32 mask;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	mask = state->enabled;
	if (enabled != NULL)
		*enabled = (mask >> subintr_code) & 1;
	state->enabled = mask & ~(1 << subintr_code);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

/* SceDisplayVbStartSGI: run the enabled subinterrupts. */
static int sgiHandler(int intr_code, void *userCtx) /* name unknown */
{
	DisplaySgiState *state = userCtx;
	SceUInt32 mask = state->enabled;
	int i;

	for (i = 0; i < 32; i++) {
		if ((1 << i) & mask)
			ksceKernelTriggerSubIntr(intr_code, i, (void *)i);
	}
	return -1;
}

/* Wait until count vblanks (or frame buffer changes) have passed on a head. */
static __attribute__((noinline)) int waitVblank(SceUInt32 head, SceUInt32 count, SceUInt32 flags) /* name unknown */
{
	DisplayVblankWait *wait;
	DisplayProcessLocal *pls;
	SceUInt32 status, vcount, base, start, depth, bits, outBits;
	SceUInt32 t;
	int ret;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (count - 1 > 0xfffe)
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	t = sceSyscallEnter();
	ret = sceKernelCheckWaitableStatus();
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	if (devStatus != NULL && head == primaryHead && sceKernelGetProcessId() == devOwnerPid) {
		status = (flags & 3) << 16 | count;
		*devStatus = status | 0x12000000;
	} else {
		status = 0;
	}
	pls = ksceKernelGetProcessLocalStorageAddr(plsKey);
	vcount = heads[head].vcount;
	if (!(flags & 2)) {
		wait = SceThreadmgrForDriver_3A72C6D8(vblankWaitKey);
		base = pls->vcountBase[head];
		start = wait->vcount[wait->depth][head] + base;
		if (SceThreadmgrForDriver_20C228E4() == 0) {
			depth = wait->depth++;
			if (depth + 1 < 7) {
				wait->vcount[depth + 1][0] = wait->vcount[depth][0];
				wait->vcount[depth + 1][1] = wait->vcount[depth][1];
				wait->vcount[depth + 1][2] = wait->vcount[depth][2];
			}
		}
	} else {
		start = pls->frameBuf[head][(flags & 4) ? 1 : 0].vcountShown;
		count--;
		if (vcount + 1 != start && ((vcount - start) & 0xffff) >= count) {
			if (devStatus != NULL && head == primaryHead && sceKernelGetProcessId() == devOwnerPid)
				*devStatus = status | 0x13000000;
			sceSyscallLeave(t);
			return 0;
		}
		wait = NULL;
		base = 0;
	}

	bits = (1 << head) | 8;
	if (!(flags & 1)) {
		do {
			ret = ksceKernelWaitEvent(heads[head].vblankEvent, bits, &outBits, NULL, NULL);
			if (ret < 0)
				goto error;
			vcount = heads[head].vcount;
		} while (((vcount - start) & 0xffff) < count);
	} else {
		do {
			ret = ksceKernelWaitEventCB(heads[head].vblankEvent, bits, &outBits, NULL, NULL);
			if (ret < 0)
				goto error;
			vcount = heads[head].vcount;
		} while (((vcount - start) & 0xffff) < count);
	}
	if (!(flags & 2))
		goto pop;
	goto done;

error:
	if (!(flags & 2) && ret != SCE_KERNEL_ERROR_THREAD_SUSPENDED) {
pop:
		depth = --wait->depth;
		if (depth < 7)
			wait->vcount[depth][head] = vcount - base;
	}
done:
	if (devStatus != NULL && head == primaryHead && sceKernelGetProcessId() == devOwnerPid)
		*devStatus = status | 0x13000000;
	sceSyscallLeave(t);
	return ret;
}

/* Vblank subinterrupt of a DSI head. */
static int vblankSubIntrHandler(void *subintrArg, void *userCtx, unsigned char priority) /* name unknown */
{
	DisplayHead *h = userCtx;
	SceUInt32 bits = h->eventBits;

	h->vcount++;
	ksceKernelPulseEventWithNotifyCallback(h->vblankEvent, bits, 0, 0);
	if (devStatus != NULL && bits == 1)
		*devStatus = (h->vcount & ~0xff000000) | 0x11000000;
	if ((1 << startupHead) & bits)
		ksceKernelTriggerSGI(8, 2, 0);
	return -1;
}

/* System timer standing in for the vblank of heads without a running DSI link. */
static void vblankTimerHandler(SceSysTimerId timer, void *pUserData) /* name unknown */
{
	SceUInt32 bits = 8;

	if (!heads[0].dsiEnabled) {
		SceUInt32 b = heads[0].eventBits;

		heads[0].vcount++;
		ksceKernelPulseEventWithNotifyCallback(heads[0].vblankEvent, b, 0, 0);
		bits = b | 8;
	}
	if (devStatus != NULL && primaryHead == 0)
		*devStatus = (heads[0].vcount & ~0xff000000) | 0x11000000;
	if (!heads[1].dsiEnabled) {
		SceUInt32 b = heads[1].eventBits;

		heads[1].vcount++;
		ksceKernelPulseEventWithNotifyCallback(heads[1].vblankEvent, b, 0, 0);
		bits |= b;
	}
	if (devStatus != NULL && primaryHead == 1)
		*devStatus = (heads[1].vcount & ~0xff000000) | 0x11000000;
	if (bits & (1 << startupHead))
		ksceKernelTriggerSGI(8, 2, 0);
}

static int sgiDisable(int intr_code, int subintr_code) /* name unknown */
{
	DisplaySgiState *state = &sgiState[intr_code - 8];
	SceKernelIntrStatus intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	state->enabled &= ~(1 << subintr_code);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

static int sgiEnable(int intr_code, int subintr_code) /* name unknown */
{
	DisplaySgiState *state = &sgiState[intr_code - 8];
	SceKernelIntrStatus intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	state->enabled |= 1 << subintr_code;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

static const SceKernelIntrOptHandlers sgiOptHandlers = {
	.size = sizeof(SceKernelIntrOptHandlers),
	.fptr0 = (void *)sgiDisable,
	.enable_subintr_cb = (void *)sgiEnable,
	.disable_subintr_cb = (void *)sgiDisable,
	.fptr3 = (void *)sgiTake,
	.fptr4 = (void *)sgiSet,
	.fptr5 = (void *)sgiNop,
};

static const SceKernelIntrOptParam sgiOptParam = {
	.size = sizeof(SceKernelIntrOptParam),
	.num = 0x20,
	.handlers = (SceKernelIntrOptHandlers *)&sgiOptHandlers,
};

static const char *const vblankEventNames[2] = {
	"SceDisplayMainLCDVblank",
	"SceDisplayHDMIVblank",
};

static inline __attribute__((always_inline)) void startVblankTimer(void) /* name unknown */
{
	SceSysTimerId timer;

	timer = ksceKernelSysTimerAlloc(2);
	if (timer < 0 || ksceKernelSysTimerSetHandler(timer, vblankTimerHandler, 0xf, NULL) < 0
	    || ksceKernelSysTimerSetClockSource(timer, 0x10, 0) < 0
	    || ksceKernelSysTimerSetInterval(timer, 0x6df92) < 0
	    || ksceKernelSysTimerStartCount(timer) < 0)
		return;
	vblankTimer = timer;
}

int vblankInit(void) /* name unknown */
{
	int i, ret;

	heads[2].eventBits = 4;
	sgiState[0].lock = 0;
	sgiState[0].enabled = 0;
	sgiState[1].lock = 0;
	sgiState[1].enabled = 0;
	heads[0].eventBits = 1;
	heads[1].eventBits = 2;
	ret = SceThreadmgrForDriver_B645C7EF(sizeof(DisplayVblankWait), 0, 0);
	if (ret < 0)
		return ret;
	vblankWaitKey = ret;
	ret = sceKernelUIDInheritedEventClassInit(&vblankEventClass, "SceUIDVblankEventClass", 0x38,
						  vblankEventCreate, vblankEventDestroy);
	if (ret < 0)
		return ret;
	for (i = 0; i < 2; i++) {
		ret = ksceGUIDKernelCreate(&vblankEventClass.base, vblankEventNames[i],
					   (SceObjectBase **)&heads[i].vblankEventObject);
		if (ret < 0)
			return ret;
		heads[i].vblankEvent = ret;
		ret = sceKernelEventObjectInit(ret, 0x8000, 0, 0);
		if (ret < 0)
			return ret;
	}
	ksceKernelRegisterIntrHandler(8, "SceDisplayVbStartSGI", 0, sgiHandler, sgiState, 0xa0, 0xf,
				      (SceKernelIntrOptParam *)&sgiOptParam);
	if (hdmiOnly == 0) {
		ksceKernelRegisterSubIntrHandler(0xd5, 1, "SceDisplayVbStartMainLCD", vblankSubIntrHandler,
						 &heads[0]);
		ksceKernelRegisterSubIntrHandler(0xd2, 1, "SceDisplayVbStartHDMI", vblankSubIntrHandler,
						 &heads[1]);
		ksceKernelEnableSubIntr(0xd5, 1);
		ksceKernelEnableSubIntr(0xd2, 1);
	} else {
		ksceKernelRegisterSubIntrHandler(0xd2, 1, "SceDisplayVbStartMainLCD", vblankSubIntrHandler,
						 &heads[0]);
		ksceKernelEnableSubIntr(0xd2, 1);
	}
	startVblankTimer();
	return 0;
}

int vblankTimerStop(void) /* name unknown */
{
	ksceKernelSysTimerFree(vblankTimer);
	vblankTimer = -1;
	return 0;
}

int vblankTimerStart(void) /* name unknown */
{
	startVblankTimer();
	return 0;
}

int vblankResume(void) /* name unknown */
{
	return 0;
}

int ksceDisplayGetVcountInternal(int head)
{
	DisplayProcessLocal *pls;
	SceUInt32 vcount;

	if ((SceUInt32)head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	vcount = heads[head].vcount;
	if (sceKernelIsUserModeThread() != 0 && (pls = ksceKernelGetProcessLocalStorageAddr(plsKey)) != NULL)
		return (vcount - pls->vcountBase[head]) & 0xffff;
	return vcount & 0xffff;
}

int sceDisplayGetVcount(void)
{
	SceUInt32 head = primaryHead;
	DisplayProcessLocal *pls;
	SceUInt32 vcount;
	SceUInt32 t;
	int ret;

	t = sceSyscallEnter();
	if (head < 3) {
		vcount = heads[head].vcount;
		if (sceKernelIsUserModeThread() != 0
		    && (pls = ksceKernelGetProcessLocalStorageAddr(plsKey)) != NULL)
			vcount -= pls->vcountBase[head];
		ret = (SceUInt16)vcount;
	} else {
		ret = SCE_DISPLAY_ERROR_INVALID_HEAD;
	}
	sceSyscallLeave(t);
	return ret;
}

int sceDisplayGetVcountInternal(SceUInt32 head)
{
	DisplayProcessLocal *pls;
	SceUInt32 vcount;
	SceUInt32 t;
	int ret;

	t = sceSyscallEnter();
	if (head < 3) {
		vcount = heads[head].vcount;
		if (sceKernelIsUserModeThread() != 0
		    && (pls = ksceKernelGetProcessLocalStorageAddr(plsKey)) != NULL)
			vcount -= pls->vcountBase[head];
		ret = (SceUInt16)vcount;
	} else {
		ret = SCE_DISPLAY_ERROR_INVALID_HEAD;
	}
	sceSyscallLeave(t);
	return ret;
}

int ksceDisplayWaitVblankStartInternal(int head)
{
	return waitVblank(head, 1, 0);
}

int ksceDisplayWaitVblankStartCBInternal(int head)
{
	return waitVblank(head, 1, 1);
}

int ksceDisplayWaitVblankStartMultiInternal(int head, unsigned int vcount)
{
	return waitVblank(head, vcount, 0);
}

int ksceDisplayWaitVblankStartMultiCBInternal(int head, unsigned int vcount)
{
	return waitVblank(head, vcount, 1);
}

int ksceDisplayWaitVblankStart(void)
{
	return waitVblank(primaryHead, 1, 0);
}

int ksceDisplayWaitVblankStartCB(void)
{
	return waitVblank(primaryHead, 1, 1);
}

int sceDisplayWaitVblankStart(void)
{
	return waitVblank(primaryHead, 1, 0);
}

int sceDisplayWaitVblankStartCB(void)
{
	return waitVblank(primaryHead, 1, 1);
}

int ksceDisplayWaitVblankStartMulti(unsigned int vcount)
{
	return waitVblank(primaryHead, vcount, 0);
}

int ksceDisplayWaitVblankStartMultiCB(unsigned int vcount)
{
	return waitVblank(primaryHead, vcount, 1);
}

int sceDisplayWaitVblankStartMulti(SceUInt32 vcount)
{
	return waitVblank(primaryHead, vcount, 0);
}

int sceDisplayWaitVblankStartMultiCB(SceUInt32 vcount)
{
	return waitVblank(primaryHead, vcount, 1);
}

int ksceDisplayWaitSetFrameBufInternal(SceUInt32 head, SceUInt32 plane)
{
	return waitVblank(head, 1, (plane & 1) << 2 | 2);
}

int ksceDisplayWaitSetFrameBufCBInternal(SceUInt32 head, SceUInt32 plane)
{
	return waitVblank(head, 1, (plane & 1) << 2 | 3);
}

int ksceDisplayWaitSetFrameBufMultiInternal(SceUInt32 head, SceUInt32 plane, SceUInt32 vcount)
{
	return waitVblank(head, vcount, (plane & 1) << 2 | 2);
}

int ksceDisplayWaitSetFrameBufMultiCBInternal(SceUInt32 head, SceUInt32 plane, SceUInt32 vcount)
{
	return waitVblank(head, vcount, (plane & 1) << 2 | 3);
}

int ksceDisplayWaitSetFrameBuf(void)
{
	return waitVblank(primaryHead, 1, 2);
}

int ksceDisplayWaitSetFrameBufCB(void)
{
	return waitVblank(primaryHead, 1, 3);
}

int sceDisplayWaitSetFrameBuf(void)
{
	return waitVblank(primaryHead, 1, 2);
}

int sceDisplayWaitSetFrameBufCB(void)
{
	return waitVblank(primaryHead, 1, 3);
}

int ksceDisplayWaitSetFrameBufMulti(unsigned int vcount)
{
	return waitVblank(primaryHead, vcount, 2);
}

int ksceDisplayWaitSetFrameBufMultiCB(unsigned int vcount)
{
	return waitVblank(primaryHead, vcount, 3);
}

int sceDisplayWaitSetFrameBufMulti(SceUInt32 vcount)
{
	return waitVblank(primaryHead, vcount, 2);
}

int sceDisplayWaitSetFrameBufMultiCB(SceUInt32 vcount)
{
	return waitVblank(primaryHead, vcount, 3);
}

int ksceDisplayRegisterVblankStartCallbackInternal(int head, SceUID uid)
{
	if ((SceUInt32)head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	return ksceKernelRegisterCallbackToEvent(heads[head].vblankEvent, uid, heads[head].vblankEvent);
}

int ksceDisplayRegisterVblankStartCallback(SceUID uid)
{
	SceUInt32 head = primaryHead;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	return ksceKernelRegisterCallbackToEvent(heads[head].vblankEvent, uid, heads[head].vblankEvent);
}

int sceDisplayRegisterVblankStartCallback(SceUID uid)
{
	SceUInt32 t;
	int ret;

	t = sceSyscallEnter();
	uid = kscePUIDtoGUID(0, uid);
	if (uid < 0) {
		sceSyscallLeave(t);
		return 0xc00280a1;
	}
	ret = ksceKernelRegisterCallbackToEvent(heads[primaryHead].vblankEvent, uid, -1);
	sceSyscallLeave(t);
	return ret;
}

int ksceDisplayUnregisterVblankStartCallbackInternal(int head, SceUID uid)
{
	if ((SceUInt32)head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	return ksceKernelUnregisterCallbackFromEvent(heads[head].vblankEvent, uid);
}

int ksceDisplayUnregisterVblankStartCallback(SceUID uid)
{
	SceUInt32 head = primaryHead;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	return ksceKernelUnregisterCallbackFromEvent(heads[head].vblankEvent, uid);
}

int sceDisplayUnregisterVblankStartCallback(SceUID uid)
{
	SceUInt32 head;
	SceUInt32 t;
	int ret;

	t = sceSyscallEnter();
	uid = kscePUIDtoGUID(0, uid);
	if (uid < 0) {
		sceSyscallLeave(t);
		return 0xc00280a1;
	}
	head = primaryHead;
	if (head > 2) {
		sceSyscallLeave(t);
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	}
	ret = ksceKernelUnregisterCallbackFromEvent(heads[head].vblankEvent, uid);
	sceSyscallLeave(t);
	return ret;
}
