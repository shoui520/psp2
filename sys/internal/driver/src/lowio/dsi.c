/* SceDsi: the two MIPI DSI transmitters, head 0 for the built-in panel and
 * head 1 for HDMI. Private helper and context names are unknown.
 */
#include "dsi_data.h"
#include "lowio_internal.h"
#include <psp2kern/kernel/intrmgr.h>
#include <sysmem/sysmem.h>
#include "dsi_regs.h"
#include <sysmem/sysevent.h>

#define DSI_ERROR_HEAD		((int)0x803f0600)
#define DSI_ERROR_VIC		((int)0x803f0601)
#define DSI_ERROR_PARAM		((int)0x803f0602)
#define DSI_ERROR_DISABLED	((int)0x803f0603)

#define DSI_INTR_HEAD0		0xd5
#define DSI_INTR_HEAD1		0xd2

/* DSI data types; head 1 sends interlaced video on virtual channel 2, the
 * second field's vertical sync on channel 3. */
#define DSI_VSYNC_START		0x01
#define DSI_VSYNC_END		0x11
#define DSI_HSYNC_START		0x21
#define DSI_HSYNC_END		0x31
#define DSI_BLANKING		0x19
#define DSI_PIXELS_24		0x3e
#define DSI_PIXELS_30		0x29
#define DSI_CHANNEL(n)		((n) << 6)

/* Program setup words, by head and lane count; meaning of the bits unknown. */
#define DSI_SETUP_PANEL		0xa3000082
#define DSI_SETUP_HDMI		0xa30000a2
#define DSI_SETUP_HDMI_3LANE	0xa30000a4
#define DSI_SETUP_COMMAND	0x83003c23
#define DSI_SETUP_COMMAND_3LANE	0x83003c25

uint32_t kscePervasiveGetSoCRevision(void);
uint32_t ScePervasiveForDriver_350BF570(unsigned head);
uint32_t ScePervasiveForDriver_F1908D3C(unsigned head);
uint32_t __aeabi_uidiv(uint32_t numerator, uint32_t denominator);

typedef struct DsiContext {
	volatile SceDsiRegisters *registers;
	uint8_t continuousClock;
	uint8_t pixelSize;		/* 24 or 30 bits */
	uint8_t lanes;
	uint8_t output;			/* SceDsiForDriver_F2921E29: head 0 1 = LCD */
	uint32_t reserved0;
	uint32_t vic;
	const DsiVicMode *mode;
	int enabled;
	uint32_t framePixels;		/* hTotal * vTotal */
	uint32_t targetFramePixels;	/* framePixels scaled by SceDsiForDriver_8610B795 */
	uint64_t outputPixels;
	uint64_t targetPixels;
	uint32_t reserved1[2];
	SceKernelSpinlock lock;
	SceUID uid;
	int intrCode;
	uint32_t subIntrMask;
} DsiContext;

_Static_assert(sizeof(DsiContext) == 0x48, "DSI context layout");
static DsiContext s_dsi[2];

static int DsiSysEvent(int resume, int eventId, void *args, void *opt);

/* name unknown: interrupt option callback */
static int DsiIntrPending(int intrCode, int subIntr)
{
	DsiContext *ctx = intrCode == DSI_INTR_HEAD1 ? &s_dsi[1] : &s_dsi[0];

	return ctx->registers->interruptStatus >> (subIntr + 1) & 1;
}

/* name unknown: interrupt option callback */
static int DsiSetSubIntrHandler(int intrCode, int subIntr, void *handler)
{
	DsiContext *ctx = intrCode == DSI_INTR_HEAD1 ? &s_dsi[1] : &s_dsi[0];
	uint32_t intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	uint32_t mask;

	mask = (ctx->subIntrMask & ~(1U << subIntr)) | ((handler != NULL) << subIntr);
	ctx->subIntrMask = mask;
	if (ctx->enabled) {
		ctx->registers->interruptEnable = mask;
		(void)ctx->registers->interruptEnable;
		LowioDmb();
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* name unknown: interrupt option callback */
static int DsiSuspendSubIntr(int intrCode, int subIntr, uint32_t *enabled)
{
	DsiContext *ctx = intrCode == DSI_INTR_HEAD1 ? &s_dsi[1] : &s_dsi[0];
	uint32_t intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	uint32_t mask = ctx->subIntrMask;

	if (enabled != NULL)
		*enabled = (mask >> subIntr) & 1;
	mask &= ~(1U << subIntr);
	ctx->subIntrMask = mask;
	if (ctx->enabled) {
		ctx->registers->interruptEnable = mask;
		(void)ctx->registers->interruptEnable;
		LowioDmb();
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* Hand a command program to the transmitter and wait until it has run. */
static inline __attribute__((always_inline)) void DsiRunCommand(
	volatile SceDsiRegisters *regs, const uint32_t *program, unsigned count)
{
	unsigned i;

	regs->commandMode = 1;
	while (!(regs->status & SCE_DSI_STATUS_READY))
		;
	regs->programControl = 8;
	(void)regs->programControl;
	LowioDsb();
	for (i = 0; i < count; i++)
		regs->commandProgram = program[i];
	regs->commandStatusClear = regs->commandStatus;
	while (regs->commandStatus & SCE_DSI_COMMAND_BUSY)
		;
	regs->commandMode = 0;
	while (!(regs->commandStatus & SCE_DSI_COMMAND_BUSY))
		;
}

/* name unknown: sends one short packet to the panel */
static int DsiShortWrite(unsigned head, uint32_t packet)
{
	DsiContext *ctx;
	volatile SceDsiRegisters *regs;
	uint32_t intr;

	if (head > 1)
		return DSI_ERROR_HEAD;
	ctx = &s_dsi[head];
	intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	if (!ctx->enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
		return DSI_ERROR_DISABLED;
	}
	regs = s_dsi[head].registers;
	{
		/* meaning of the first four words unknown */
		uint32_t program[] = {
			0x8e00bf7f, 0x911affff, 0x940001ff, 0x86043f0f,
			ctx->lanes == 3 ? DSI_SETUP_COMMAND_3LANE : DSI_SETUP_COMMAND,
			SCE_DSI_PROGRAM_SHORT(packet),
			0x30000003,
			SCE_DSI_PROGRAM_END,
		};

		DsiRunCommand(regs, program, 8);
	}
	regs->commandStatusClear = SCE_DSI_COMMAND_DONE;
	while (regs->commandStatus & SCE_DSI_COMMAND_DONE)
		;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* name unknown: sends a read request and collects up to 32 response bytes */
static int DsiRead(unsigned head, uint32_t packet, uint8_t *data, int size)
{
	DsiContext *ctx;
	volatile SceDsiRegisters *regs;
	uint32_t program[63];
	uint32_t intr, word;
	int i;

	if (head > 1)
		return DSI_ERROR_HEAD;
	if ((unsigned)size > 32)
		return DSI_ERROR_PARAM;
	ctx = &s_dsi[head];
	intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	if (!ctx->enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
		return DSI_ERROR_DISABLED;
	}
	regs = s_dsi[head].registers;
	program[0] = ctx->lanes == 3 ? DSI_SETUP_COMMAND_3LANE : DSI_SETUP_COMMAND;
	program[1] = SCE_DSI_PROGRAM_SHORT(size << 8 | 0x37);	/* maximum return size */
	program[2] = SCE_DSI_PROGRAM_SHORT(packet);
	program[3] = SCE_DSI_PROGRAM_END;
	DsiRunCommand(regs, program, 4);
	for (i = 0; i < size && !(regs->status & SCE_DSI_STATUS_RX_EMPTY); i += 4) {
		word = regs->rxFifo;
		data[i] = word;
		data[i + 1] = word >> 8;
		data[i + 2] = word >> 16;
		data[i + 3] = word >> 24;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* name unknown: paces HDMI frames to the target rate, one line at a time */
static int DsiIntrHandler(int intrCode, void *arg)
{
	DsiContext *ctx = arg;
	volatile SceDsiRegisters *regs = ctx->registers;
	const DsiVicMode *mode;
	uint64_t target, output;
	uint32_t intr, status, mask;

	intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	if (!ctx->enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
		return -2;
	}
	status = regs->interruptStatus;
	regs->interruptStatus = status;
	LowioDmb();
	(void)regs->interruptStatus;
	LowioDsb();
	if (status & 2) {
		mode = ctx->mode;
		target = ctx->targetPixels + ctx->targetFramePixels;
		ctx->targetPixels = target;
		output = ctx->outputPixels + ctx->framePixels;
		ctx->outputPixels = output;
		if (!mode->interlaced) {
			if (target - output >= mode->hTotal) {
				ctx->outputPixels = output + mode->hTotal;
				regs->vTotal = mode->vTotal + 1;
			} else {
				regs->vTotal = mode->vTotal;
			}
		}
	}
	LowioDmb();
	mask = ctx->subIntrMask;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	if ((status & 1) && (mask & 1))
		ksceKernelTriggerSubIntr(intrCode, 0, (void *)0);
	if ((status & 2) && (mask & 2))
		ksceKernelTriggerSubIntr(intrCode, 1, (void *)1);
	if ((status & 4) && (mask & 4))
		ksceKernelTriggerSubIntr(intrCode, 2, (void *)2);
	if ((status & 8) && (mask & 8))
		ksceKernelTriggerSubIntr(intrCode, 3, (void *)3);
	if ((status & 0x10) && (mask & 0x10))
		ksceKernelTriggerSubIntr(intrCode, 4, (void *)4);
	return -1;
}

/* name unknown: interrupt option callback */
static int DsiDisableSubIntr(int intrCode, int subIntr)
{
	DsiContext *ctx = intrCode == DSI_INTR_HEAD1 ? &s_dsi[1] : &s_dsi[0];
	uint32_t intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	uint32_t mask;

	mask = ctx->subIntrMask & ~(1U << subIntr);
	ctx->subIntrMask = mask;
	if (ctx->enabled) {
		ctx->registers->interruptEnable = mask;
		(void)ctx->registers->interruptEnable;
		LowioDmb();
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* name unknown: interrupt option callback */
static int DsiEnableSubIntr(int intrCode, int subIntr)
{
	DsiContext *ctx = intrCode == DSI_INTR_HEAD1 ? &s_dsi[1] : &s_dsi[0];
	uint32_t intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	uint32_t mask;

	mask = ctx->subIntrMask | 1U << subIntr;
	ctx->subIntrMask = mask;
	if (ctx->enabled) {
		ctx->registers->interruptEnable = mask;
		(void)ctx->registers->interruptEnable;
		LowioDmb();
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* SDK callback typedefs have an extra pointer indirection; retain the raw ABI. */
static const struct {
	uint32_t size;
	void *callbacks[9];
} s_dsiIntrHandlers = {
	0x28,
	{
		NULL,
		NULL,
		NULL,
		DsiDisableSubIntr,
		DsiEnableSubIntr,
		DsiDisableSubIntr,
		DsiSuspendSubIntr,
		DsiSetSubIntrHandler,
		DsiIntrPending,
	},
};

static SceKernelIntrOptParam s_dsiIntrOpt = {
	0x14,
	5,
	(SceKernelIntrOptHandlers *)&s_dsiIntrHandlers,
	0,
	0,
};

static inline __attribute__((always_inline)) const DsiVicMode *DsiFindVic(unsigned vic)
{
	unsigned i;

	for (i = 0; i < 15; i++) {
		if (s_dsiVicTable[i].vic == vic)
			return s_dsiVicTable[i].mode;
	}
	return NULL;
}

static inline __attribute__((always_inline)) void DsiSetVic(unsigned head, unsigned vic)
{
	const DsiVicMode *mode = DsiFindVic(vic);

	if (mode != NULL) {
		s_dsi[head].vic = vic;
		s_dsi[head].mode = mode;
	}
}

/* name unknown */
static __attribute__((noinline)) int DsiInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	unsigned i;
	int ret;

	memset(s_dsi, 0, sizeof(s_dsi));
	for (i = 0; i < 2; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		if (i) {
			opt.paddr = DSI_REG_BASE(1);
			ret = SceSysmemForDriver_C94850C9("SceDsi1Reg", 0x20100806, 0x1000, &opt);
		} else {
			opt.paddr = DSI_REG_BASE(0);
			ret = SceSysmemForDriver_C94850C9("SceDsi0Reg", 0x20100806, 0x1000, &opt);
		}
		if (ret < 0)
			return ret;
		s_dsi[i].uid = ret;
		SceSysmemForDriver_A841EDDA(ret, &base);
		s_dsi[i].registers = base;
	}
	s_dsi[0].intrCode = DSI_INTR_HEAD0;
	s_dsi[1].intrCode = DSI_INTR_HEAD1;
	ksceKernelRegisterIntrHandler(DSI_INTR_HEAD0, "SceDsi0", 0, DsiIntrHandler,
		&s_dsi[0], 0xa0, 0xf, &s_dsiIntrOpt);
	ksceKernelRegisterIntrHandler(DSI_INTR_HEAD1, "SceDsi1", 0, DsiIntrHandler,
		&s_dsi[1], 0xa0, 0xf, &s_dsiIntrOpt);
	sceKernelRegisterSysEventHandler("SceDsi", DsiSysEvent, NULL);
	s_dsi[0].continuousClock = 0;
	s_dsi[1].continuousClock = 1;
	ret = kscePervasiveGetSoCRevision();
	s_dsi[0].output = 0;
	s_dsi[1].lanes = 3;
	s_dsiPhyOledCurrent = (ret & 0x1ff00) > 0xff ? s_dsiPhyOled : s_dsiPhyOledEs;
	s_dsiVicTable[0].mode = &s_dsiModeOled60;
	s_dsiVicTable[1].mode = &s_dsiModeOled50;
	s_dsiVicTable[2].mode = &s_dsiModeOled48;
	s_dsi[0].pixelSize = 24;
	s_dsi[1].pixelSize = 24;
	s_dsi[0].lanes = 2;
	s_dsi[1].output = 2;
	DsiSetVic(0, 0);
	DsiSetVic(1, 0x8300);
	return 0;
}

int LowioDsiInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (DsiInit() < 0)
		return 2;
	return 0;
}

int SceDsiForDriver_F2921E29(unsigned head, unsigned output)
{
	if (head > 1)
		return DSI_ERROR_HEAD;
	s_dsi[head].output = output;
	if (head == 0 && output == 0) {
		s_dsi[0].continuousClock = 0;
		s_dsiVicTable[0].mode = &s_dsiModeOled60;
		s_dsiVicTable[1].mode = &s_dsiModeOled50;
		s_dsiVicTable[2].mode = &s_dsiModeOled48;
	} else if (head == 0 && output == 1) {
		s_dsi[0].continuousClock = 0;
		s_dsiVicTable[0].mode = &s_dsiModeLcd60;
		s_dsiVicTable[1].mode = NULL;
		s_dsiVicTable[2].mode = NULL;
	} else if (head == 1 && output == 1) {
		s_dsi[1].continuousClock = 0;
		s_dsiVicTable[0].mode = NULL;
		s_dsiVicTable[1].mode = NULL;
		s_dsiVicTable[2].mode = NULL;
	}
	return 0;
}

int ksceDsiSetLanesAndPixelSize(unsigned head, unsigned lanes, unsigned pixelSize)
{
	if (head > 1)
		return DSI_ERROR_HEAD;
	if (head == 0) {
		if (lanes != 2 || pixelSize != 24)
			return DSI_ERROR_PARAM;
	} else {
		if ((pixelSize != 24 && pixelSize != 30) || lanes - 2 > 1)
			return DSI_ERROR_PARAM;
	}
	s_dsi[head].pixelSize = pixelSize;
	s_dsi[head].lanes = lanes;
	return 0;
}

int ksceDsiGetVicResolution(unsigned vic, int *width, int *height)
{
	const DsiVicMode *mode = DsiFindVic(vic);
	int w;

	if (mode == NULL)
		return DSI_ERROR_VIC;
	if (width != NULL) {
		w = mode->hTotal - (mode->hBackPorch + mode->hFrontPorch) - mode->hSyncWidth;
		if (mode->flags & DSI_MODE_NARROW)
			w -= 2;
		*width = w;
	}
	if (height == NULL)
		return 0;
	if (mode->interlaced == 1)
		*height = mode->vTotal + 1 -
			2 * (mode->vFrontPorch + mode->vSyncWidth + mode->vBackPorch);
	else
		*height = mode->vTotal - (mode->vBackPorch + mode->vSyncWidth) -
			mode->vFrontPorch;
	return 0;
}

int ksceDsiSetVic(unsigned head, unsigned vic)
{
	const DsiVicMode *mode;

	if (head > 1)
		return DSI_ERROR_HEAD;
	mode = DsiFindVic(vic);
	if (mode == NULL)
		return DSI_ERROR_VIC;
	s_dsi[head].vic = vic;
	s_dsi[head].mode = mode;
	return 0;
}

int ksceDsiGetPixelClock(unsigned head)
{
	const DsiVicMode *mode;

	if (head > 1)
		return DSI_ERROR_HEAD;
	mode = s_dsi[head].mode;
	if (mode == NULL)
		return DSI_ERROR_VIC;
	return s_dsi[head].pixelSize == 24 ? mode->pixelClock24 : mode->pixelClock30;
}

int SceDsiForDriver_8610B795(unsigned head, uint32_t numerator, uint32_t denominator)
{
	if (head > 1)
		return DSI_ERROR_HEAD;
	s_dsi[head].targetFramePixels =
		__aeabi_uidiv(numerator * s_dsi[head].framePixels, denominator);
	return 0;
}

/* The pixel stream packet for one line, by pixel size. */
static inline __attribute__((always_inline)) uint32_t DsiPixels(
	unsigned width, unsigned pixelSize, uint32_t channel)
{
	if (pixelSize == 24)
		return SCE_DSI_PROGRAM_LONG(DSI_PIXELS_24 | channel, width * 3);
	return SCE_DSI_PROGRAM_LONG(DSI_PIXELS_30 | channel, width * 30 / 8);
}

/* A horizontal blanking period: a blanking packet when the mode sends one
 * (bytes has bit 31 set), otherwise a wait for the given event. */
static inline __attribute__((always_inline)) uint32_t DsiBlank(
	uint32_t bytes, uint32_t events, uint32_t channel)
{
	if (bytes)
		return SCE_DSI_PROGRAM_LONG(DSI_BLANKING | channel, bytes);
	return SCE_DSI_PROGRAM_WAIT(events);
}

/* Head 0: the panel's video program. */
static inline __attribute__((always_inline)) unsigned DsiPanelProgram(
	uint32_t *program, const DsiContext *ctx, const DsiVicMode *mode)
{
	uint32_t blank, width;

	width = mode->hTotal - (mode->hFrontPorch + mode->hSyncWidth);
	blank = ctx->output ? SCE_DSI_PROGRAM_LONG(DSI_BLANKING, 0xb78) :
		SCE_DSI_PROGRAM_LONG(DSI_BLANKING, 0xb40);
	program[0] = SCE_DSI_PROGRAM_REPEAT(7, 0xffff);
	program[1] = DSI_SETUP_PANEL;
	program[2] = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_VSYNC);
	program[3] = SCE_DSI_PROGRAM_SHORT(DSI_VSYNC_START);
	program[4] = blank;
	program[5] = SCE_DSI_PROGRAM_REPEAT(2, mode->vSyncWidth + mode->vBackPorch - 2);
	program[6] = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	program[7] = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	program[8] = blank;
	program[9] = SCE_DSI_PROGRAM_REPEAT(3, mode->vTotal - 1 - mode->vSyncWidth -
		mode->vBackPorch - mode->vFrontPorch);
	program[10] = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	program[11] = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	program[12] = SCE_DSI_PROGRAM_LONG(DSI_BLANKING, mode->hBackPorch * 3 - 10);
	program[13] = SCE_DSI_PROGRAM_LONG(DSI_PIXELS_24, (width - mode->hBackPorch) * 3);
	program[14] = SCE_DSI_PROGRAM_REPEAT(2, mode->vFrontPorch - 1);
	program[15] = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	program[16] = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	program[17] = blank;
	program[18] = SCE_DSI_PROGRAM_END;
	return 19;
}

/* Head 1, output 1: a fixed-blanking program. */
static inline __attribute__((always_inline)) unsigned DsiFixedProgram(
	uint32_t *program, const DsiContext *ctx, const DsiVicMode *mode)
{
	uint32_t width, pixels;

	width = mode->hTotal - (mode->hBackPorch + mode->hSyncWidth) - mode->hFrontPorch;
	if (mode->flags & DSI_MODE_NARROW)
		width -= 2;
	pixels = SCE_DSI_PROGRAM_LONG(DSI_PIXELS_24, width * 3);
	program[0] = SCE_DSI_PROGRAM_REPEAT(14, 0xffff);
	program[1] = ctx->lanes == 3 ? DSI_SETUP_HDMI_3LANE : DSI_SETUP_HDMI;
	program[2] = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_VSYNC);
	program[3] = SCE_DSI_PROGRAM_SHORT(DSI_VSYNC_START);
	program[4] = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	program[5] = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	program[6] = SCE_DSI_PROGRAM_REPEAT(1, mode->vBackPorch + mode->vSyncWidth - 3);
	program[7] = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	program[8] = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	program[9] = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	program[10] = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	program[11] = SCE_DSI_PROGRAM_LONG(DSI_BLANKING, 0x56);
	program[12] = pixels;
	program[13] = SCE_DSI_PROGRAM_LONG(DSI_BLANKING, 0x150);
	program[14] = SCE_DSI_PROGRAM_REPEAT(3, mode->vTotal - 2 - mode->vSyncWidth -
		mode->vBackPorch - mode->vFrontPorch);
	program[15] = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	program[16] = SCE_DSI_PROGRAM_LONG(DSI_BLANKING, 0x56);
	program[17] = pixels;
	program[18] = SCE_DSI_PROGRAM_LONG(DSI_BLANKING, 0x150);
	program[19] = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	program[20] = SCE_DSI_PROGRAM_REPEAT(1, mode->vFrontPorch - 2);
	program[21] = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	program[22] = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	program[23] = SCE_DSI_PROGRAM_END;
	return 24;
}

/* Head 1: a progressive HDMI video program. */
static inline __attribute__((always_inline)) unsigned DsiProgressiveProgram(
	uint32_t *program, const DsiContext *ctx, const DsiVicMode *mode)
{
	uint32_t *p = program;
	uint32_t hfp, hsa, hbp, sync, back, pixels, width, lines;
	unsigned pixelSize = ctx->pixelSize;

	width = mode->hTotal - (mode->hBackPorch + mode->hFrontPorch) - mode->hSyncWidth;
	lines = mode->vTotal - (mode->vFrontPorch + mode->vBackPorch) - mode->vSyncWidth;
	hfp = 0;
	if (mode->flags & DSI_MODE_HFP_BLANKING)
		hfp = ((mode->hFrontPorch * pixelSize >> 3) - 12) | 0x80000000;
	hsa = 0;
	if (mode->flags & DSI_MODE_HSA_BLANKING)
		hsa = ((mode->hSyncWidth * pixelSize >> 3) - 10) | 0x80000000;
	hbp = 0;
	if (mode->flags & DSI_MODE_HBP_BLANKING)
		hbp = ((mode->hBackPorch * pixelSize >> 3) - 10) | 0x80000000;
	sync = DsiBlank(hsa, SCE_DSI_WAIT_HSYNC_END, 0);
	back = DsiBlank(hbp, SCE_DSI_WAIT_ACTIVE_END, 0);
	pixels = DsiPixels(width, pixelSize, 0);

	*p++ = hfp ? SCE_DSI_PROGRAM_REPEAT(0x17, 0xffff) : SCE_DSI_PROGRAM_REPEAT(0xd, 0xffff);
	*p++ = ctx->lanes == 3 ? DSI_SETUP_HDMI_3LANE : DSI_SETUP_HDMI;
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_VSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_VSYNC_START);
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END);
	*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vSyncWidth - 2);
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END);
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_VSYNC_END);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_VSYNC_END);
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END);
	*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vBackPorch - 2);
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END);
	if (hfp) {
		*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
		*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
		*p++ = sync;
		*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END);
		*p++ = back;
		*p++ = pixels;
		*p++ = SCE_DSI_PROGRAM_LONG(DSI_BLANKING, hfp);
		*p++ = SCE_DSI_PROGRAM_REPEAT(5, lines - 2);
		*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
		*p++ = sync;
		*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END);
		*p++ = back;
		*p++ = pixels;
		*p++ = SCE_DSI_PROGRAM_LONG(DSI_BLANKING, hfp);
		*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
		*p++ = sync;
		*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END);
		*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vFrontPorch - 2);
	} else {
		*p++ = SCE_DSI_PROGRAM_REPEAT(5, lines - 1);
		*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
		*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
		*p++ = sync;
		*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END);
		*p++ = back;
		*p++ = pixels;
		*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vFrontPorch - 1);
	}
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START);
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END);
	*p++ = SCE_DSI_PROGRAM_END;
	return p - program;
}

/* Head 1: an interlaced HDMI video program, both fields. */
static inline __attribute__((always_inline)) unsigned DsiInterlacedProgram(
	uint32_t *program, const DsiContext *ctx, const DsiVicMode *mode)
{
	uint32_t *p = program;
	uint32_t hsa, hbp, sync, back, pixels, width, lines, active;
	unsigned pixelSize = ctx->pixelSize;

	lines = (mode->vTotal + 1 - 2 * (mode->vSyncWidth + mode->vFrontPorch +
		mode->vBackPorch)) >> 1;
	width = mode->hTotal - (mode->hSyncWidth + mode->hBackPorch) - mode->hFrontPorch;
	hsa = 0;
	if (mode->flags & DSI_MODE_HSA_BLANKING)
		hsa = ((mode->hSyncWidth * pixelSize >> 3) - 10) | 0x80000000;
	hbp = 0;
	if (mode->flags & DSI_MODE_HBP_BLANKING)
		hbp = ((mode->hBackPorch * pixelSize >> 3) - 10) | 0x80000000;
	sync = DsiBlank(hsa, SCE_DSI_WAIT_HSYNC_END, DSI_CHANNEL(2));
	back = DsiBlank(hbp, SCE_DSI_WAIT_ACTIVE_END, DSI_CHANNEL(2));
	pixels = DsiPixels(width, pixelSize, DSI_CHANNEL(2));
	active = SCE_DSI_PROGRAM_REPEAT(5, lines - 1);

	*p++ = SCE_DSI_PROGRAM_REPEAT(0x15, 0xffff);
	*p++ = ctx->lanes == 3 ? DSI_SETUP_HDMI_3LANE : DSI_SETUP_HDMI;
	/* first field */
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_SECOND_FIELD | SCE_DSI_WAIT_VSYNC |
		SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_VSYNC_START | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vSyncWidth - 2);
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_VSYNC_END);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_VSYNC_END | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vBackPorch - 3);
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	*p++ = active;
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	*p++ = back;
	*p++ = pixels;
	*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vFrontPorch);
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	/* second field */
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_VSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_VSYNC_START | DSI_CHANNEL(3));
	*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vSyncWidth - 1);
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_VSYNC_END);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_VSYNC_END | DSI_CHANNEL(3));
	*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vBackPorch - 2);
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	*p++ = active;
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	*p++ = back;
	*p++ = pixels;
	*p++ = SCE_DSI_PROGRAM_REPEAT(3, mode->vFrontPorch - 1);
	*p++ = SCE_DSI_PROGRAM_WAIT(SCE_DSI_WAIT_HSYNC);
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_START | DSI_CHANNEL(2));
	*p++ = sync;
	*p++ = SCE_DSI_PROGRAM_SHORT(DSI_HSYNC_END | DSI_CHANNEL(2));
	*p++ = SCE_DSI_PROGRAM_END;
	return p - program;
}

int ksceDsiEnableHead(unsigned head)
{
	DsiContext *ctx;
	volatile SceDsiRegisters *regs;
	const DsiVicMode *mode;
	const DsiPhyTiming *phy;
	uint32_t program[63];
	uint32_t intr;
	unsigned count, i;

	if (head > 1)
		return DSI_ERROR_HEAD;
	ctx = &s_dsi[head];
	ScePervasiveForDriver_350BF570(head);
	intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	if (ctx->enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
		return 0;
	}
	regs = s_dsi[head].registers;
	mode = ctx->mode;
	regs->interruptEnable = 0;
	regs->videoProgramEnable = 1;
	if ((kscePervasiveGetSoCRevision() & 0x1ff00) > 0xff) {
		regs->laneConfig[0] = (regs->laneConfig[0] & ~3U) | 2;
		regs->laneConfig[1] = (regs->laneConfig[1] & ~3U) | 2;
		regs->laneConfig[2] = (regs->laneConfig[2] & ~3U) | 2;
		if (ctx->lanes == 3)
			regs->laneConfig[3] = (regs->laneConfig[3] & ~3U) | 2;
	}
	regs->lanePower[0] = 0x200;
	regs->lanePower[1] = 0x200;
	regs->lanePower[2] = 0x200;
	if (ctx->lanes == 3)
		regs->lanePower[3] = 0x200;
	phy = ctx->pixelSize == 24 ? mode->phy24 : mode->phy30;
	if (phy != NULL) {
		for (i = 0; i < 9; i++)
			regs->phyTiming[i] = phy->timing[i];
		regs->clockLaneTiming = phy->timing[1] | phy->timing[1] << 16;
	}
	if (head == 1) {
		regs->laneEnable = 0xf;
		regs->continuousClock = 0;
		regs->phyReset = 1;
		regs->programReset = 0xffffffff;
		if (ctx->output == 1)
			count = DsiFixedProgram(program, &s_dsi[1], mode);
		else if (mode->interlaced)
			count = DsiInterlacedProgram(program, &s_dsi[1], mode);
		else
			count = DsiProgressiveProgram(program, &s_dsi[1], mode);
	} else {
		regs->laneEnable = 7;
		regs->continuousClock = 0;
		regs->phyReset = 1;
		regs->programReset = 0xffffffff;
		count = DsiPanelProgram(program, &s_dsi[0], mode);
	}
	for (i = 0; i < count; i++)
		regs->videoProgram = program[i];
	regs->commandMode = 1;
	ctx->enabled = 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	ksceKernelEnableIntr(ctx->intrCode);
	return 0;
}

int ksceDsiDisableHead(unsigned head)
{
	DsiContext *ctx;
	volatile SceDsiRegisters *regs;
	uint32_t intr;

	if (head > 1)
		return DSI_ERROR_HEAD;
	ctx = &s_dsi[head];
	intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	if (!ctx->enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
		return 0;
	}
	ctx->enabled = 0;
	ksceKernelDisableIntr(ctx->intrCode);
	regs = s_dsi[head].registers;
	while (regs->pending & 0xf)
		;
	regs->programReset = 0xffffffff;
	regs->lanePower[0] = 1;
	regs->lanePower[1] = 1;
	regs->lanePower[2] = 1;
	if (ctx->lanes == 3)
		regs->lanePower[3] = 1;
	regs->laneEnable = 0;
	regs->phyReset = 1;
	(void)regs->status;
	LowioDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	ScePervasiveForDriver_F1908D3C(head);
	return 0;
}

static const uint32_t s_dsiStartModes[4] = {2, 2, 3, 4};

int ksceDsiStartDisplay(unsigned head, unsigned start)
{
	DsiContext *ctx;
	volatile SceDsiRegisters *regs;
	const DsiVicMode *mode;
	uint32_t intr, hTotal, vTotal, interlaced, vfp, vsw, vbp, clocks;
	uint32_t hBlank, width, field, hHalf, vBlank, control;

	if (head > 1)
		return DSI_ERROR_HEAD;
	if (start > 4)
		return DSI_ERROR_PARAM;
	ctx = &s_dsi[head];
	intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	if (!ctx->enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
		return DSI_ERROR_VIC;
	}
	mode = ctx->mode;
	regs = s_dsi[head].registers;
	hTotal = mode->hTotal;
	hBlank = mode->hSyncWidth + mode->hBackPorch;
	vTotal = mode->vTotal;
	width = hTotal - hBlank - mode->hFrontPorch;
	if (mode->flags & DSI_MODE_NARROW)
		width -= 2;
	vfp = mode->vFrontPorch;
	vsw = mode->vSyncWidth;
	vbp = mode->vBackPorch;
	interlaced = mode->interlaced;
	regs->videoMode = interlaced == 1 ? 3 : 0;
	if (s_dsi[head].lanes == 2)
		clocks = 6;
	else if (s_dsi[head].pixelSize == 24)
		clocks = 4;
	else
		clocks = 5;
	regs->hTotal = hTotal * clocks >> 1;
	if (interlaced == 1) {
		field = (vTotal + 1) >> 1;
		hHalf = hTotal * clocks >> 2;
		regs->vTotal = (vTotal << 16 & 0x1fff0000) | (field & 0x1fff);
		regs->hStart = 0x10001;
		regs->hActive = ((((width + hBlank) * clocks >> 1) + 1) & 0xffff) |
			(hBlank * clocks >> 1) << 16;
		regs->vActive = ((vsw + vbp - 1) << 16 & 0x1fff0000) | ((field - vfp) & 0x1fff);
		regs->vActiveEven = ((vsw + vbp + ((vTotal - 1) >> 1)) << 16 & 0x1fff0000) |
			((vTotal + 1 - vfp) & 0x1fff);
		regs->hSync = (mode->hSyncWidth * clocks >> 1) << 16 | 1;
		regs->vStart = 0x10001;
		regs->vSync = (vsw << 16 & 0x1fff0000) | ((hTotal * clocks >> 1) & 0xffff);
		regs->vSyncEven = (field << 16 & 0x1fff0000) | ((hHalf + 1) & 0xffff);
		regs->vSyncEvenEnd = ((vsw + field) << 16 & 0x1fff0000) | (hHalf & 0xffff);
	} else {
		regs->vTotal = vTotal;
		field = (vTotal + 1) >> 1;
		regs->hStart = 0x10001;
		regs->hActive = ((((width + hBlank) * clocks >> 1) + 1) & 0xffff) |
			(hBlank * clocks >> 1) << 16;
		regs->vActive = ((vTotal + 1 - vfp) & 0x1fff) |
			((vsw + vbp) << 16 & 0x1fff0000);
		regs->hSync = (mode->hSyncWidth * clocks >> 1) << 16 | 1;
		regs->vStart = 0x10001;
		regs->vSync = ((hTotal * clocks >> 1) & 0xffff) | (vsw << 16 & 0x1fff0000);
	}
	if (s_dsi[head].lanes == 3 && s_dsi[head].pixelSize == 30) {
		if (vTotal == 525) {
			regs->hActive = 0x130083a;
			regs->hActiveAdjust = 0x131083a;
			regs->hSync = 0x9a0001;
			regs->hSyncAdjust = 0x9b0861;
			regs->phaseAdjust[0] = 0x440;
			regs->phaseAdjust[1] = 4;
			regs->videoMode |= 0x100;
		} else if (vTotal == 750) {
			regs->hActive = 0x28a0f0b;
			regs->hActiveAdjust = 0x2890f0b;
			regs->hSync = 0x640001;
			regs->hSyncAdjust = 0x63101d;
			regs->phaseAdjust[0] = 0;
			regs->phaseAdjust[1] = 0x444;
			regs->videoMode |= 0x100;
		}
	}
	regs->timingEnable = 1;
	vBlank = vsw + vbp;
	regs->lineInterrupt = ((field + vBlank - 8) << 16 & 0x1fff0000) |
		((vBlank - 8) & 0x1fff);
	if (interlaced == 1) {
		regs->fieldStart = (field << 16 & 0x1fff0000) | 1;
		regs->activeStart = ((vBlank + ((vTotal - 1) >> 1)) << 16 & 0x1fff0000) | 1;
		regs->fieldControl = ((vTotal + 1) << 14 & 0x1fff0000) | 0x40000001;
		regs->fieldStartEven = (vTotal << 16 & 0x1fff0000) | 1;
		regs->activeStartEven = ((vBlank - 1) << 16 & 0x1fff0000) | 1;
		regs->fieldControlEven = ((vTotal + 1) * 0xc000 & 0x1fff0000) | 0x40000001;
	} else {
		regs->fieldStart = 0x10001;
		regs->activeStart = (vBlank << 16 & 0x1fff0000) | 1;
		regs->fieldControl = ((vBlank + (vTotal >> 1)) << 16 & 0x1fff0000) | 0x40000001;
		regs->fieldControlEven = 0;
	}
	regs->interruptStatus = regs->interruptStatus;
	regs->interruptEnable = ctx->subIntrMask;
	if (ctx->continuousClock)
		regs->continuousClock = 1;
	else
		regs->continuousClock = 0;
	regs->videoProgramStart = 0xffffffff;
	ctx->targetPixels = 0;
	ctx->framePixels = hTotal * vTotal;
	ctx->targetFramePixels = hTotal * vTotal;
	ctx->outputPixels = 0;
	if ((start == 1 && head == 0) || (start == 2 && head == 1) || start - 1 > 3)
		control = 1;
	else
		control = s_dsiStartModes[start - 1];
	regs->control = control;
	LowioDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

int ksceDsiSendBlankingPacket(unsigned head)
{
	DsiContext *ctx;
	volatile SceDsiRegisters *regs;
	uint32_t intr;

	if (head > 1)
		return DSI_ERROR_HEAD;
	ctx = &s_dsi[head];
	intr = SceCpuForDriver_D32ACE9E(&ctx->lock);
	if (!ctx->enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
		return 0;
	}
	regs = s_dsi[head].registers;
	regs->programControl = 12;
	(void)regs->programControl;
	LowioDmb();
	regs->commandStatusClear = regs->commandStatus;
	while (regs->commandStatus & SCE_DSI_COMMAND_BLANKED)
		;
	regs->videoProgram = ctx->lanes == 3 ? DSI_SETUP_HDMI_3LANE : DSI_SETUP_HDMI;
	regs->videoProgram = SCE_DSI_PROGRAM_LONG(DSI_BLANKING, 0);
	/* meaning unknown */
	regs->videoProgram = ctx->lanes == 3 ? 0x30000079 : 0x30000039;
	regs->videoProgram = 0xa6002000;
	regs->videoProgram = 0x18000001;
	regs->videoProgramStart = 0xffffffff;
	LowioDmb();
	while (!(regs->commandStatus & SCE_DSI_COMMAND_BLANKED))
		;
	regs->programReset = 0xffffffff;
	regs->commandStatusClear = SCE_DSI_COMMAND_BLANKED;
	regs->programControl = 12;
	(void)regs->programControl;
	LowioDmb();
	regs->control = 0;
	(void)regs->control;
	LowioDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* name unknown */
static int DsiSysEvent(int resume, int eventId, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (resume == 0 && eventId == 0x4004) {
		ksceDsiSendBlankingPacket(0);
		ksceDsiSendBlankingPacket(1);
		ksceDsiDisableHead(0);
		ksceDsiDisableHead(1);
	}
	return 0;
}

int SceDsiForDriver_6F8029A1(unsigned head)
{
	if (head > 1)
		return DSI_ERROR_HEAD;
	if (!s_dsi[head].enabled)
		return DSI_ERROR_DISABLED;
	return (s_dsi[head].registers->position >> 16 & 0x1fff) - 1;
}

int ksceDsiGenericShortWrite(unsigned head, int parameter0, int parameter1)
{
	uint32_t packet;

	if (parameter0 < 0)
		packet = 0x03;
	else if (parameter1 < 0)
		packet = (parameter0 << 8 & 0xffff) | 0x13;
	else
		packet = (parameter0 << 8 & 0xffff) | (parameter1 << 16 & 0xff0000) | 0x23;
	return DsiShortWrite(head, packet);
}

int ksceDsiDcsShortWrite(unsigned head, int command, int parameter)
{
	uint32_t packet;

	if (parameter < 0)
		packet = (command << 8 & 0xffff) | 0x05;
	else
		packet = (command << 8 & 0xffff) | 0x15 | (parameter << 16 & 0xff0000);
	return DsiShortWrite(head, packet);
}

int ksceDsiGenericReadRequest(unsigned head, int parameter, void *data, unsigned size)
{
	uint32_t packet;

	if (parameter < 0)
		packet = 0x04;
	else
		packet = (parameter << 8 & 0xffff) | 0x14;
	return DsiRead(head, packet, data, size);
}

int ksceDsiDcsRead(unsigned head, int command, void *data, unsigned size)
{
	return DsiRead(head, (command << 8 & 0xffff) | 0x06, data, size);
}
