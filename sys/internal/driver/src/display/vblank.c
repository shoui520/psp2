/*
 * SceDisplay: vblank interrupts, vcounts and the waits on them.
 * File name unknown; the module links its parts in this order.
 */
#include <scetypes.h>
#include <sysmem/spinlock.h>
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
#include "display_imports.h"
#include <threadmgr/uid_waitable.h>

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

static int sgiSet(int intr_code, int subintr_code, unsigned int enable) /* name unknown */
{
	DisplaySgiState *state = &sgiState[intr_code - 8];
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&state->lock);
	state->enabled = (state->enabled & ~(1 << subintr_code)) | ((enable != 0) << subintr_code);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

static int sgiTake(int intr_code, int subintr_code, unsigned int *enabled) /* name unknown */
{
	DisplaySgiState *state = &sgiState[intr_code - 8];
	SceKernelIntrStatus intr;
	SceUInt32 mask;

	intr = SceCpuForDriver_D32ACE9E(&state->lock);
	mask = state->enabled;
	if (enabled != NULL)
		*enabled = (mask >> subintr_code) & 1;
	state->enabled = mask & ~(1 << subintr_code);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
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
			sceKernelCallSubIntrHandler(intr_code, i, (void *)i);
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
	pls = SceProcessmgrForDriver_EE694840(plsKey);
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
			ret = SceThreadmgrForDriver_C529EA32(heads[head].vblankEvent, bits, &outBits, NULL, NULL);
			if (ret < 0)
				goto error;
			vcount = heads[head].vcount;
		} while (((vcount - start) & 0xffff) < count);
	} else {
		do {
			ret = SceThreadmgrForDriver_360C655C(heads[head].vblankEvent, bits, &outBits, NULL, NULL);
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
	sceKernelPulseEventWithNotifyCallback(h->vblankEvent, bits, 0, 0);
	if (devStatus != NULL && bits == 1)
		*devStatus = (h->vcount & ~0xff000000) | 0x11000000;
	if ((1 << startupHead) & bits)
		sceKernelGenerateSoftIntr(8, 2, 0);
	return -1;
}

/* System timer standing in for the vblank of heads without a running DSI link. */
static void vblankTimerHandler(SceSysTimerId timer, void *pUserData) /* name unknown */
{
	SceUInt32 bits = 8;

	if (!heads[0].dsiEnabled) {
		SceUInt32 b = heads[0].eventBits;

		heads[0].vcount++;
		sceKernelPulseEventWithNotifyCallback(heads[0].vblankEvent, b, 0, 0);
		bits = b | 8;
	}
	if (devStatus != NULL && primaryHead == 0)
		*devStatus = (heads[0].vcount & ~0xff000000) | 0x11000000;
	if (!heads[1].dsiEnabled) {
		SceUInt32 b = heads[1].eventBits;

		heads[1].vcount++;
		sceKernelPulseEventWithNotifyCallback(heads[1].vblankEvent, b, 0, 0);
		bits |= b;
	}
	if (devStatus != NULL && primaryHead == 1)
		*devStatus = (heads[1].vcount & ~0xff000000) | 0x11000000;
	if (bits & (1 << startupHead))
		sceKernelGenerateSoftIntr(8, 2, 0);
}

static int sgiDisable(int intr_code, int subintr_code) /* name unknown */
{
	DisplaySgiState *state = &sgiState[intr_code - 8];
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&state->lock);
	state->enabled &= ~(1 << subintr_code);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

static int sgiEnable(int intr_code, int subintr_code) /* name unknown */
{
	DisplaySgiState *state = &sgiState[intr_code - 8];
	SceKernelIntrStatus intr;

	intr = SceCpuForDriver_D32ACE9E(&state->lock);
	state->enabled |= 1 << subintr_code;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

static const SceKernelIntrCallbacks sgiOptHandlers = {
	.size = sizeof(SceKernelIntrCallbacks),
	.postRelease = sgiDisable,
	.enable = sgiEnable,
	.disable = sgiDisable,
	.suspend = sgiTake,
	.resume = sgiSet,
	.isOccurred = sgiNop,
};

static const SceKernelIntrRegisterOpt sgiOptParam = {
	.size = sizeof(SceKernelIntrRegisterOpt),
	.num = 0x20,
	.handlers = (void *)&sgiOptHandlers,
};

static const char *const vblankEventNames[2] = {
	"SceDisplayMainLCDVblank",
	"SceDisplayHDMIVblank",
};

static inline __attribute__((always_inline)) void startVblankTimer(void) /* name unknown */
{
	SceSysTimerId timer;

	timer = SceSystimerForDriver_E2B9D8E9(2);
	if (timer < 0 || SceSystimerForDriver_CCCD1EE3(timer, vblankTimerHandler, 0xf, NULL) < 0
	    || SceSystimerForDriver_0450933B(timer, 0x10, 0) < 0
	    || SceSystimerForDriver_A73351C1(timer, 0x6df92) < 0
	    || SceSystimerForDriver_59EBD22D(timer) < 0)
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
		ret = sceGUIDKernelCreate(&vblankEventClass.base, vblankEventNames[i],
					   (SceObjectBase **)&heads[i].vblankEventObject);
		if (ret < 0)
			return ret;
		heads[i].vblankEvent = ret;
		ret = sceKernelEventObjectInit(ret, 0x8000, 0, 0);
		if (ret < 0)
			return ret;
	}
	sceKernelRegisterIntrHandler(8, "SceDisplayVbStartSGI", 0, sgiHandler, sgiState, 0xa0, 0xf,
				     &sgiOptParam);
	if (hdmiOnly == 0) {
		sceKernelRegisterSubIntrHandler(0xd5, 1, "SceDisplayVbStartMainLCD", vblankSubIntrHandler,
						 &heads[0]);
		sceKernelRegisterSubIntrHandler(0xd2, 1, "SceDisplayVbStartHDMI", vblankSubIntrHandler,
						 &heads[1]);
		sceKernelEnableSubIntr(0xd5, 1);
		sceKernelEnableSubIntr(0xd2, 1);
	} else {
		sceKernelRegisterSubIntrHandler(0xd2, 1, "SceDisplayVbStartMainLCD", vblankSubIntrHandler,
						 &heads[0]);
		sceKernelEnableSubIntr(0xd2, 1);
	}
	startVblankTimer();
	return 0;
}

int vblankTimerStop(void) /* name unknown */
{
	SceSystimerForDriver_8358611F(vblankTimer);
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

int SceDisplayForDriver_8B5DA27B(int head)
{
	DisplayProcessLocal *pls;
	SceUInt32 vcount;

	if ((SceUInt32)head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	vcount = heads[head].vcount;
	if (sceKernelIsUserModeThread() != 0 && (pls = SceProcessmgrForDriver_EE694840(plsKey)) != NULL)
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
		    && (pls = SceProcessmgrForDriver_EE694840(plsKey)) != NULL)
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
		    && (pls = SceProcessmgrForDriver_EE694840(plsKey)) != NULL)
			vcount -= pls->vcountBase[head];
		ret = (SceUInt16)vcount;
	} else {
		ret = SCE_DISPLAY_ERROR_INVALID_HEAD;
	}
	sceSyscallLeave(t);
	return ret;
}

int SceDisplayForDriver_B80CA224(int head)
{
	return waitVblank(head, 1, 0);
}

int SceDisplayForDriver_8F3C0E19(int head)
{
	return waitVblank(head, 1, 1);
}

int SceDisplayForDriver_32287576(int head, unsigned int vcount)
{
	return waitVblank(head, vcount, 0);
}

int SceDisplayForDriver_61421AAF(int head, unsigned int vcount)
{
	return waitVblank(head, vcount, 1);
}

int SceDisplayForDriver_984C27E7(void)
{
	return waitVblank(primaryHead, 1, 0);
}

int SceDisplayForDriver_46F186C3(void)
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

int SceDisplayForDriver_40F1469C(unsigned int vcount)
{
	return waitVblank(primaryHead, vcount, 0);
}

int SceDisplayForDriver_77ED8B3A(unsigned int vcount)
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

int SceDisplayForDriver_12A77662(SceUInt32 head, SceUInt32 plane)
{
	return waitVblank(head, 1, (plane & 1) << 2 | 2);
}

int SceDisplayForDriver_35466D63(SceUInt32 head, SceUInt32 plane)
{
	return waitVblank(head, 1, (plane & 1) << 2 | 3);
}

int SceDisplayForDriver_F83C95B1(SceUInt32 head, SceUInt32 plane, SceUInt32 vcount)
{
	return waitVblank(head, vcount, (plane & 1) << 2 | 2);
}

int SceDisplayForDriver_6DC8F0F5(SceUInt32 head, SceUInt32 plane, SceUInt32 vcount)
{
	return waitVblank(head, vcount, (plane & 1) << 2 | 3);
}

int SceDisplayForDriver_1C0C9C4A(void)
{
	return waitVblank(primaryHead, 1, 2);
}

int SceDisplayForDriver_9D7F203C(void)
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

int SceDisplayForDriver_E6D27E0A(unsigned int vcount)
{
	return waitVblank(primaryHead, vcount, 2);
}

int SceDisplayForDriver_863EACBE(unsigned int vcount)
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

int SceDisplayForDriver_4AE2A2B1(int head, SceUID uid)
{
	if ((SceUInt32)head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	return sceKernelRegisterCallbackToEventForDriver(heads[head].vblankEvent, uid, heads[head].vblankEvent);
}

int SceDisplayForDriver_7FB0BD28(SceUID uid)
{
	SceUInt32 head = primaryHead;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	return sceKernelRegisterCallbackToEventForDriver(heads[head].vblankEvent, uid, heads[head].vblankEvent);
}

int sceDisplayRegisterVblankStartCallback(SceUID uid)
{
	SceUInt32 t;
	int ret;

	t = sceSyscallEnter();
	uid = SceSysmemForDriver_45D22597(0, uid);
	if (uid < 0) {
		sceSyscallLeave(t);
		return 0xc00280a1;
	}
	ret = sceKernelRegisterCallbackToEventForDriver(heads[primaryHead].vblankEvent, uid, -1);
	sceSyscallLeave(t);
	return ret;
}

int SceDisplayForDriver_B027433E(int head, SceUID uid)
{
	if ((SceUInt32)head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	return sceKernelUnregisterCallbackFromEvent(heads[head].vblankEvent, uid);
}

int SceDisplayForDriver_4B27191F(SceUID uid)
{
	SceUInt32 head = primaryHead;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	return sceKernelUnregisterCallbackFromEvent(heads[head].vblankEvent, uid);
}

int sceDisplayUnregisterVblankStartCallback(SceUID uid)
{
	SceUInt32 head;
	SceUInt32 t;
	int ret;

	t = sceSyscallEnter();
	uid = SceSysmemForDriver_45D22597(0, uid);
	if (uid < 0) {
		sceSyscallLeave(t);
		return 0xc00280a1;
	}
	head = primaryHead;
	if (head > 2) {
		sceSyscallLeave(t);
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	}
	ret = sceKernelUnregisterCallbackFromEvent(heads[head].vblankEvent, uid);
	sceSyscallLeave(t);
	return ret;
}
