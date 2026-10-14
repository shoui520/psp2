/* SceCsi camera interface. Private helper and context names are unknown. */
#include "lowio_internal.h"
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <sce/motion_dummy.h>
#include <sce/sysevent.h>
#include <sce/csi.h>

#define CSI_ERROR_HEAD ((uint32_t)0x803f0900)
#define CSI_ERROR_MODE ((uint32_t)0x803f0901)
#define CSI_REG(base, offset) (*(volatile uint32_t *)((base) + (offset)))

uint32_t ScePervasiveForDriver_5B4EF352(void);
uint32_t ScePervasiveForDriver_4B223AF5(unsigned head);
uint32_t ScePervasiveForDriver_D3B59C33(unsigned head);
uint32_t ScePervasiveForDriver_1068C067(unsigned head);
int sceKernelDelayThread(unsigned usec);

typedef struct CsiContext {
	uint8_t *base;
	uint8_t *cifBase;
	uint32_t width;
	uint32_t height;
	CsiTransferConfig transfer;
	SceKernelSpinlock lock;
	int intrCode;
	int cifIntrCode;
	int uid;
	int cifUid;
	uint32_t subIntrMask;
} CsiContext;

_Static_assert(sizeof(CsiContext) == 0x54, "CSI context layout");
static CsiContext s_csi[2];

/* ARM register shifts use the low byte and clear the result above bit 31. */
static uint32_t CsiSubIntrBit(unsigned subIntr)
{
	subIntr &= 0xff;
	return subIntr < 32 ? 1U << subIntr : 0;
}

/* name unknown: interrupt option callback at original offset 0x69b8. */
static int CsiIntrPending(int intrCode, int subIntr)
{
	CsiContext *ctx = &s_csi[intrCode == 0xa6 ? 0 : 1];

	(void)subIntr;
	return CSI_REG(ctx->cifBase, 0xbc) >> 24 & 1;
}

/* name unknown: first camera interface interrupt is acknowledged elsewhere. */
static int CsiIntrHandler(int intrCode, void *arg)
{
	(void)intrCode;
	(void)arg;
	return -1;
}

/* name unknown */
static int CifIntrHandler(int intrCode, void *arg)
{
	CsiContext *ctx = arg;
	uint32_t status = CSI_REG(ctx->cifBase, 0xbc);
	uint32_t value = CSI_REG(ctx->cifBase, 0xb4);
	uint32_t mask;
	unsigned i;

	CSI_REG(ctx->cifBase, 0xbc) = status;
	if (status & 0x1000000) {
		mask = ctx->subIntrMask;
		for (i = 0; i < 4; i++) {
			if (mask & (1U << i))
				ksceKernelTriggerSubIntr(intrCode, i, (void *)value);
		}
	}
	(void)CSI_REG(ctx->cifBase, 0xbc);
	LowioDsb();
	return -1;
}

/* name unknown: option table callback at original offset 0x6a50. */
static int CsiSetSubIntrHandler(int intrCode, unsigned subIntr, void *handler)
{
	CsiContext *ctx = &s_csi[intrCode == 0xa6 ? 0 : 1];
	uint32_t state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	uint32_t bit = CsiSubIntrBit(subIntr);

	ctx->subIntrMask = (ctx->subIntrMask & ~bit) | (handler ? bit : 0);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

/* name unknown: option table callback at original offset 0x6a98. */
static int CsiSuspendSubIntr(int intrCode, unsigned subIntr, uint32_t *enabled)
{
	CsiContext *ctx = &s_csi[intrCode == 0xa6 ? 0 : 1];
	uint32_t state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);

	if (enabled)
		*enabled = !!(ctx->subIntrMask & CsiSubIntrBit(subIntr));
	ctx->subIntrMask &= ~CsiSubIntrBit(subIntr);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

/* name unknown */
static int CsiDisableSubIntr(int intrCode, int subIntr)
{
	CsiContext *ctx = &s_csi[intrCode == 0xa6 ? 0 : 1];
	uint32_t state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);

	ctx->subIntrMask &= ~CsiSubIntrBit(subIntr);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

/* name unknown */
static int CsiEnableSubIntr(int intrCode, int subIntr)
{
	CsiContext *ctx = &s_csi[intrCode == 0xa6 ? 0 : 1];
	uint32_t state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);

	ctx->subIntrMask |= CsiSubIntrBit(subIntr);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

/* SDK callback typedefs have an extra pointer indirection; retain the raw ABI. */
static const struct {
	uint32_t size;
	void *callbacks[9];
} s_csiIntrHandlers = {
    0x28,
    {
	NULL,
	NULL,
	NULL,
	CsiDisableSubIntr,
	CsiEnableSubIntr,
	CsiDisableSubIntr,
	CsiSuspendSubIntr,
	CsiSetSubIntrHandler,
	CsiIntrPending,
    },
};

static SceKernelIntrOptParam s_csiIntrOpt = {
    0x14,
    4,
    (SceKernelIntrOptHandlers *)&s_csiIntrHandlers,
    0,
    0,
};

/* name unknown */
static void CsiReset(void)
{
	ScePervasiveForDriver_5B4EF352();
	ScePervasiveForDriver_4B223AF5(0);
	ScePervasiveForDriver_4B223AF5(1);
	ScePervasiveForDriver_D3B59C33(0);
	ScePervasiveForDriver_D3B59C33(1);
}

/* name unknown */
static int CsiSysEvent(int resume, int eventid, void *args, void *opt)
{
	unsigned i;

	(void)args;
	(void)opt;
	if (!resume && eventid == 0x4004) {
		for (i = 0; i < 2; i++) {
			ksceKernelDisableIntr(s_csi[i].intrCode);
			ksceKernelDisableIntr(s_csi[i].cifIntrCode);
		}
	} else if (resume == 1 && eventid == 0x10004) {
		CsiReset();
		for (i = 0; i < 2; i++) {
			ksceKernelEnableIntr(s_csi[i].intrCode);
			ksceKernelEnableIntr(s_csi[i].cifIntrCode);
		}
	}
	return 0;
}

/* name unknown: device descriptors at original offset 0x9164. */
static const struct {
	int intrCode;
	const char *intrName;
	const char *name;
	uint32_t paddr;
	uint32_t size;
	int cifIntrCode;
	const char *cifIntrName;
	const char *cifName;
	uint32_t cifPaddr;
	uint32_t cifSize;
} s_csiDevices[2] = {
    {0xa0, "SceCsi0", "SceCsi0Reg", 0xe3050000, 0x1000, 0xa6, "SceCif0", "SceCif0Reg", 0xe3020000,
	0x1000},
    {0xa2, "SceCsi1", "SceCsi1Reg", 0xe3060000, 0x1000, 0xa8, "SceCif1", "SceCif1Reg", 0xe3030000,
	0x1000},
};

/* name unknown: initializer at original offset 0x6bdc. */
int LowioCsiInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	CsiContext *ctx;
	void *base;
	unsigned i;
	int ret;

	memset(s_csi, 0, sizeof(s_csi));
	for (i = 0; i < 2; i++) {
		ctx = &s_csi[i];
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_csiDevices[i].paddr;
		ret = ksceKernelAllocMemBlock(
		    s_csiDevices[i].name, 0x20100806, s_csiDevices[i].size, &opt);
		if (ret < 0)
			return ret;
		ctx->uid = ret;
		ret = ksceKernelGetMemBlockBase(ret, &base);
		if (ret < 0)
			return ret;
		ctx->base = base;
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_csiDevices[i].cifPaddr;
		ret = ksceKernelAllocMemBlock(
		    s_csiDevices[i].cifName, 0x20100806, s_csiDevices[i].cifSize, &opt);
		if (ret < 0)
			return ret;
		ctx->cifUid = ret;
		ret = ksceKernelGetMemBlockBase(ret, &base);
		if (ret < 0)
			return ret;
		ctx->cifBase = base;
		ctx->intrCode = s_csiDevices[i].intrCode;
		ctx->cifIntrCode = s_csiDevices[i].cifIntrCode;
		ksceKernelRegisterIntrHandler(ctx->intrCode, s_csiDevices[i].intrName, 0,
		    CsiIntrHandler, ctx, 0xa0, 0xf, &s_csiIntrOpt);
		ksceKernelRegisterIntrHandler(ctx->cifIntrCode, s_csiDevices[i].cifIntrName, 0,
		    CifIntrHandler, ctx, 0xa0, 0xf, &s_csiIntrOpt);
		ksceKernelEnableIntr(ctx->intrCode);
		ksceKernelEnableIntr(ctx->cifIntrCode);
	}
	ksceKernelRegisterSysEventHandler("SceCsi", CsiSysEvent, NULL);
	CsiReset();
	return 0;
}

uint32_t SceCsiForDriver_B508822B(unsigned head, uint32_t a1, uint32_t a2)
{
	if (head >= 2)
		return CSI_ERROR_HEAD;
	s_csi[head].width = a1;
	s_csi[head].height = a2;
	return 0;
}

uint32_t SceCsiForDriver_10545393(unsigned head, const CsiTransferConfig *data)
{
	if (head >= 2)
		return CSI_ERROR_HEAD;
	s_csi[head].transfer = *data;
	return 0;
}

uint32_t SceCsiForDriver_2AFEA1B0(unsigned head, uint32_t mode)
{
	CsiContext *ctx;
	uint8_t *base;
	unsigned format = mode & 0xff;
	unsigned i;

	if (head >= 2)
		return CSI_ERROR_HEAD;
	if (format != 0 && format != 8 && format != 0x10 && format != 0x11 && format != 0x12 &&
	    format != 0x30 && format != 0x40)
		return CSI_ERROR_MODE;
	ScePervasiveForDriver_1068C067(head);
	ctx = &s_csi[head];
	base = ctx->cifBase;
	CSI_REG(base, 0xc) = (mode >> 4 & 0xf) | 0x301100;
	CSI_REG(base, 0x24) = (mode & 0xf) | 0xff0000;
	CSI_REG(base, 0x14) = mode & 0x1000 ? 0x100 : mode & 0x2000 ? 0x200 : 0;
	CSI_REG(base, 0x28) = 2;
	CSI_REG(base, 0x4c) = ctx->transfer.iAddress[0];
	CSI_REG(base, 0x78) = ctx->transfer.iAddress[1];
	CSI_REG(base, 0x84) = ctx->transfer.iAddress[2];
	CSI_REG(base, 0x50) = ctx->transfer.iPitch;
	for (i = 0; i < 3; i++) {
		static const uint8_t chromaOffsets[3] = { 0x54, 0x7c, 0x88 };

		CSI_REG(base, chromaOffsets[i]) = ctx->transfer.chromaAddress[i][0];
		CSI_REG(base, chromaOffsets[i] + 4) = ctx->transfer.chromaAddress[i][1];
	}
	CSI_REG(base, 0x5c) = ctx->transfer.chromaPitch;
	CSI_REG(base, 0x34) = ctx->width;
	CSI_REG(base, 0x38) = 0;
	CSI_REG(base, 0x3c) = ctx->height;
	CSI_REG(base, 0x40) = 0;
	CSI_REG(base, 0x30) = 0;
	CSI_REG(base, 0x90) = 0;
	CSI_REG(base, 0xc0) = 0x100000;
	CSI_REG(base, 0x18) = 1;
	CSI_REG(base, 4) = 1;
	LowioDmb();
	ksceKernelEnableIntr(ctx->cifIntrCode);
	return 0;
}

uint32_t SceCsiForDriver_A1D1805D(unsigned head)
{
	CsiContext *ctx;
	uint8_t *base;
	uint64_t start;
	uint32_t status;
	uint32_t control;

	if (head >= 2)
		return CSI_ERROR_HEAD;
	ctx = &s_csi[head];
	base = ctx->cifBase;
	ksceKernelDisableIntr(ctx->cifIntrCode);
	start = sceKernelGetSystemTimeWide();
	while (sceKernelGetSystemTimeWide() - start < 300000 && (CSI_REG(base, 8) & 1)) {
		status = CSI_REG(base, 0xbc);
		if (status) {
			CSI_REG(base, 0xbc) = status;
			(void)CSI_REG(base, 0xbc);
			LowioDsb();
			if (status & 0x1000000)
				break;
		}
		sceKernelDelayThread(1000);
	}
	CSI_REG(base, 0) = 1;
	(void)CSI_REG(base, 0xbc);
	LowioDmb();
	start = sceKernelGetSystemTimeWide();
	while (sceKernelGetSystemTimeWide() - start < 300000) {
		control = CSI_REG(base, 8);
		status = CSI_REG(base, 0xbc);
		if (status) {
			CSI_REG(base, 0xbc) = status;
			(void)CSI_REG(base, 0xbc);
			LowioDsb();
			if (status & 0x1000000)
				return 0;
		}
		if (!(control & 1))
			break;
		sceKernelDelayThread(1000);
	}
	return 0;
}

uint32_t SceCsiForDriver_FC165297(unsigned head, uint32_t a1)
{
	uint8_t *base;

	if (head >= 2)
		return CSI_ERROR_HEAD;
	base = s_csi[head].base;
	CSI_REG(base, 0x8c) = 0;
	CSI_REG(base, 0x28) = 0x1000;
	CSI_REG(base, 0x100) = a1;
	CSI_REG(base, 0) = 1;
	CSI_REG(base, 8) = 1;
	(void)CSI_REG(base, 0x14);
	LowioDmb();
	return 0;
}

uint32_t SceCsiForDriver_4AE39F26(unsigned head)
{
	uint8_t *base;

	if (head >= 2)
		return CSI_ERROR_HEAD;
	base = s_csi[head].base;
	CSI_REG(base, 8) = 0;
	CSI_REG(base, 0) = 0;
	LowioDmb();
	ScePervasiveForDriver_4B223AF5(head);
	return 0;
}
