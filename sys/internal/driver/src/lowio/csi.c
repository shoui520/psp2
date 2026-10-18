/* SceCsi camera interface. Private helper and context names are unknown. */
#include "lowio_internal.h"
#include <intrmgr/intrmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysevent.h>
#include <lowio/csi.h>
#include "csi_regs.h"
#include <threadmgr/threadmgr.h>
#include <lowio/pervasive.h>

#define CSI_ERROR_HEAD ((uint32_t)0x803f0900)
#define CSI_ERROR_MODE ((uint32_t)0x803f0901)

typedef struct CsiContext {
	volatile SceCsiRegisters *registers;
	volatile SceCifRegisters *cifRegisters;
	uint32_t width;
	uint32_t height;
	CsiTransferConfig transfer;
	SceKernelSpinlock lock;
	int intrCode;
	int cifIntrCode;
	SceUID uid;
	SceUID cifUid;
	uint32_t subIntrMask;
} CsiContext;

_Static_assert(sizeof(CsiContext) == 0x54, "CSI context layout");
static CsiContext s_csi[2];

/* name unknown */
static int CsiIntrHandler(int intrCode, void *arg)
{
	(void)intrCode;
	(void)arg;
	return -1;
}

/* name unknown: interrupt option callback */
static int CsiIntrPending(int intrCode, int subIntr)
{
	CsiContext *ctx = intrCode == 0xa6 ? &s_csi[0] : &s_csi[1];

	(void)subIntr;
	return (ctx->cifRegisters->interruptStatus & SCE_CIF_INTR_FRAME) != 0;
}

/* name unknown */
static int CifIntrHandler(int intrCode, void *arg)
{
	CsiContext *ctx = arg;
	volatile SceCifRegisters *cif = ctx->cifRegisters;
	uint32_t status, frameInfo, mask;

	status = cif->interruptStatus;
	frameInfo = cif->frameInfo;
	cif->interruptStatus = status;
	if (status & SCE_CIF_INTR_FRAME) {
		mask = ctx->subIntrMask;
		if (mask & 1)
			sceKernelCallSubIntrHandler(intrCode, 0, (void *)frameInfo);
		if (mask & 2)
			sceKernelCallSubIntrHandler(intrCode, 1, (void *)frameInfo);
		if (mask & 4)
			sceKernelCallSubIntrHandler(intrCode, 2, (void *)frameInfo);
		if (mask & 8)
			sceKernelCallSubIntrHandler(intrCode, 3, (void *)frameInfo);
	}
	(void)cif->interruptStatus;
	LowioDsb();
	return -1;
}

/* name unknown: interrupt option callback */
static int CsiSetSubIntrHandler(int intrCode, int subIntr, void *handler)
{
	CsiContext *ctx = intrCode == 0xa6 ? &s_csi[0] : &s_csi[1];
	uint32_t intr = SceCpuForDriver_D32ACE9E(&ctx->lock);

	ctx->subIntrMask = (ctx->subIntrMask & ~(1U << subIntr)) |
		((handler != NULL) << subIntr);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* name unknown: interrupt option callback */
static int CsiSuspendSubIntr(int intrCode, int subIntr, uint32_t *enabled)
{
	CsiContext *ctx = intrCode == 0xa6 ? &s_csi[0] : &s_csi[1];
	uint32_t intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	uint32_t mask = ctx->subIntrMask;

	if (enabled != NULL)
		*enabled = (mask >> subIntr) & 1;
	ctx->subIntrMask = mask & ~(1U << subIntr);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

static inline __attribute__((always_inline)) void CsiReset(void)
{
	ScePervasiveForDriver_5B4EF352();
	ScePervasiveForDriver_4B223AF5(0);
	ScePervasiveForDriver_4B223AF5(1);
	ScePervasiveForDriver_D3B59C33(0);
	ScePervasiveForDriver_D3B59C33(1);
}

/* name unknown */
static int CsiSysEvent(int resume, int eventId, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (resume == 0) {
		if (eventId == 0x4004) {
			sceKernelDisableIntr(s_csi[0].intrCode);
			sceKernelDisableIntr(s_csi[0].cifIntrCode);
			sceKernelDisableIntr(s_csi[1].intrCode);
			sceKernelDisableIntr(s_csi[1].cifIntrCode);
		}
	} else if (resume == 1 && eventId == 0x10004) {
		CsiReset();
		sceKernelEnableIntr(s_csi[0].intrCode);
		sceKernelEnableIntr(s_csi[0].cifIntrCode);
		sceKernelEnableIntr(s_csi[1].intrCode);
		sceKernelEnableIntr(s_csi[1].cifIntrCode);
	}
	return 0;
}

/* name unknown */
static int CsiDisableSubIntr(int intrCode, int subIntr)
{
	CsiContext *ctx = intrCode == 0xa6 ? &s_csi[0] : &s_csi[1];
	uint32_t intr = SceCpuForDriver_D32ACE9E(&ctx->lock);

	ctx->subIntrMask &= ~(1U << subIntr);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* name unknown */
static int CsiEnableSubIntr(int intrCode, int subIntr)
{
	CsiContext *ctx = intrCode == 0xa6 ? &s_csi[0] : &s_csi[1];
	uint32_t intr = SceCpuForDriver_D32ACE9E(&ctx->lock);

	ctx->subIntrMask |= 1U << subIntr;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
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

static SceKernelIntrRegisterOpt s_csiIntrOpt = {
    0x14,
    4,
    (void *)&s_csiIntrHandlers,
    0,
    0,
};

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
    {0xa0, "SceCsi0", "SceCsi0Reg", CSI_REG_BASE(0), CSI_REG_SIZE,
	0xa6, "SceCif0", "SceCif0Reg", CIF_REG_BASE(0), CIF_REG_SIZE},
    {0xa2, "SceCsi1", "SceCsi1Reg", CSI_REG_BASE(1), CSI_REG_SIZE,
	0xa8, "SceCif1", "SceCif1Reg", CIF_REG_BASE(1), CIF_REG_SIZE},
};

/* name unknown */
static __attribute__((noinline)) int CsiInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	CsiContext *ctx;
	void *base;
	unsigned int i;
	int ret;

	memset(s_csi, 0, sizeof(s_csi));
	for (i = 0; i < 2; i++) {
		ctx = &s_csi[i];
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_csiDevices[i].paddr;
		ret = SceSysmemForDriver_C94850C9(s_csiDevices[i].name, 0x20100806,
			s_csiDevices[i].size, &opt);
		if (ret < 0)
			return ret;
		ctx->uid = ret;
		ret = SceSysmemForDriver_A841EDDA(ret, &base);
		if (ret < 0)
			return ret;
		ctx->registers = base;
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_csiDevices[i].cifPaddr;
		ret = SceSysmemForDriver_C94850C9(s_csiDevices[i].cifName, 0x20100806,
			s_csiDevices[i].cifSize, &opt);
		if (ret < 0)
			return ret;
		ctx->cifUid = ret;
		ret = SceSysmemForDriver_A841EDDA(ret, &base);
		if (ret < 0)
			return ret;
		ctx->cifRegisters = base;
		ctx->intrCode = s_csiDevices[i].intrCode;
		ctx->cifIntrCode = s_csiDevices[i].cifIntrCode;
		sceKernelRegisterIntrHandler(ctx->intrCode, s_csiDevices[i].intrName, 0,
			CsiIntrHandler, ctx, 0xa0, 0xf, &s_csiIntrOpt);
		sceKernelRegisterIntrHandler(ctx->cifIntrCode, s_csiDevices[i].cifIntrName, 0,
			CifIntrHandler, ctx, 0xa0, 0xf, &s_csiIntrOpt);
		sceKernelEnableIntr(ctx->intrCode);
		sceKernelEnableIntr(ctx->cifIntrCode);
	}
	sceKernelRegisterSysEventHandler("SceCsi", CsiSysEvent, NULL);
	CsiReset();
	return 0;
}

int LowioCsiInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (CsiInit() < 0)
		return 2;
	return 0;
}

uint32_t SceCsiForDriver_B508822B(unsigned head, uint32_t width, uint32_t height)
{
	uint32_t ret;

	if (head <= 1) {
		s_csi[head].width = width;
		s_csi[head].height = height;
		ret = 0;
	} else {
		ret = CSI_ERROR_HEAD;
	}
	return ret;
}

uint32_t SceCsiForDriver_10545393(unsigned head, const CsiTransferConfig *data)
{
	if (head > 1)
		return CSI_ERROR_HEAD;
	__builtin_memcpy(&s_csi[head].transfer, data, sizeof(*data));
	return 0;
}

uint32_t SceCsiForDriver_2AFEA1B0(unsigned head, uint32_t mode)
{
	CsiContext *ctx;
	volatile SceCifRegisters *cif;

	if (head > 1)
		return CSI_ERROR_HEAD;
	switch (mode & 0xff) {
	case 0x00:
	case 0x08:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x30:
	case 0x40:
		break;
	default:
		return CSI_ERROR_MODE;
	}
	ScePervasiveForDriver_1068C067(head);
	ctx = &s_csi[head];
	cif = ctx->cifRegisters;
	cif->inputFormat = (mode >> 4 & 0xf) | 0x301100;
	cif->outputFormat = (mode & 0xf) | 0xff0000;
	if (mode & 0x1000)
		cif->dataOrder = 0x100;
	else if (mode & 0x2000)
		cif->dataOrder = 0x200;
	else
		cif->dataOrder = 0;
	cif->lastFrame = 2;
	cif->iAddress = ctx->transfer.iAddress[0];
	cif->frame[0].iAddress = ctx->transfer.iAddress[1];
	cif->frame[1].iAddress = ctx->transfer.iAddress[2];
	cif->iPitch = ctx->transfer.iPitch;
	cif->chromaAddress[0] = ctx->transfer.chromaAddress[0][0];
	cif->chromaAddress[1] = ctx->transfer.chromaAddress[0][1];
	cif->frame[0].chromaAddress[0] = ctx->transfer.chromaAddress[1][0];
	cif->frame[0].chromaAddress[1] = ctx->transfer.chromaAddress[1][1];
	cif->frame[1].chromaAddress[0] = ctx->transfer.chromaAddress[2][0];
	cif->frame[1].chromaAddress[1] = ctx->transfer.chromaAddress[2][1];
	cif->chromaPitch = ctx->transfer.chromaPitch;
	cif->width = ctx->width;
	cif->left = 0;
	cif->height = ctx->height;
	cif->top = 0;
	cif->windowControl = 0;
	cif->currentFrame = 0;
	cif->interruptEnable = 0x100000;
	cif->outputEnable = 1;
	cif->start = 1;
	LowioDmb();
	sceKernelEnableIntr(ctx->cifIntrCode);
	return 0;
}

uint32_t SceCsiForDriver_A1D1805D(unsigned head)
{
	CsiContext *ctx;
	volatile SceCifRegisters *cif;
	SceUInt64 start;
	uint32_t status, busy;

	if (head > 1)
		return CSI_ERROR_HEAD;
	ctx = &s_csi[head];
	cif = ctx->cifRegisters;
	sceKernelDisableIntr(ctx->cifIntrCode);
	/* Let the frame in progress finish, for up to 300 ms. */
	start = sceKernelGetSystemTimeWide();
	while (sceKernelGetSystemTimeWide() - start < 300000) {
		if ((cif->status & SCE_CIF_STATUS_BUSY) == 0)
			break;
		status = cif->interruptStatus;
		if (status != 0) {
			cif->interruptStatus = status;
			(void)cif->interruptStatus;
			LowioDsb();
			if (status & SCE_CIF_INTR_FRAME)
				break;
		}
		sceKernelDelayThread(1000);
	}
	cif->stop = 1;
	(void)cif->interruptStatus;
	LowioDmb();
	/* Then wait for the interface to stop, for up to 300 ms. */
	start = sceKernelGetSystemTimeWide();
	while (sceKernelGetSystemTimeWide() - start < 300000) {
		busy = cif->status;
		status = cif->interruptStatus;
		if (status != 0) {
			cif->interruptStatus = status;
			(void)cif->interruptStatus;
			LowioDsb();
			if (status & SCE_CIF_INTR_FRAME)
				break;
		}
		if ((busy & SCE_CIF_STATUS_BUSY) == 0)
			break;
		sceKernelDelayThread(1000);
	}
	return 0;
}

uint32_t SceCsiForDriver_FC165297(unsigned head, uint32_t format)
{
	volatile SceCsiRegisters *csi;

	if (head > 1)
		return CSI_ERROR_HEAD;
	csi = s_csi[head].registers;
	csi->interruptEnable = 0;
	csi->mode = 0x1000;
	csi->format = format;
	csi->enable = 1;
	csi->receive = 1;
	(void)csi->status;
	LowioDmb();
	return 0;
}

uint32_t SceCsiForDriver_4AE39F26(unsigned head)
{
	volatile SceCsiRegisters *csi;

	if (head > 1)
		return CSI_ERROR_HEAD;
	csi = s_csi[head].registers;
	csi->receive = 0;
	csi->enable = 0;
	LowioDmb();
	ScePervasiveForDriver_4B223AF5(head);
	return 0;
}
