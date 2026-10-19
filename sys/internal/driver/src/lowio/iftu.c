/*
 * SceIftu: display-plane scanout (IFTU0/1 planes A and B) and the off-screen
 * colour space converter and scaler (IFTU2). Context field names are by use;
 * the export parameters follow the henkaku wiki's SceLowio page.
 */
#include "lowio_internal.h"
#include <string.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <lowio/iftu.h>
#include <lowio/iftu_regs.h>
#include <sysmem/sysevent.h>
#include <lowio/pervasive.h>

#define IFTU_PLANE_COUNT	4	/* IFTU0 A/B, IFTU1 A/B */
#define IFTU_CONVERTER		4	/* IFTU2 */
#define IFTU_NO_BLEND		0x100	/* alpha: opaque, blending off */
#define IFTU_NO_MERGE		0x80

enum IftuState {
	IFTU_STATE_DISABLED,
	IFTU_STATE_ENABLED,
	IFTU_STATE_STARTING,		/* enabled; the first frame is pending */
};

#define IFTU_PENDING_FILTER	0x40000000

/* What a display plane scans out to (SceIftuForDriver_E6EE2C6B). */
typedef struct IftuOutput {
	uint32_t width;
	uint32_t height;
	uint32_t pixelFormat;
	uint32_t planeEnable;
} IftuOutput;

typedef struct IftuPlane {
	volatile SceIftuRegisters *registers;
	volatile SceIftucRegisters *control;		/* shared by A and B */
	volatile SceIftucPlaneRegisters *planeControl;
	uint32_t reserved0[64];
	SceIftuCscParams cscParams2;
	SceIftuCscParams cscParams1;
	SceIftuPlaneState planeState;
	IftuOutput output;
	uint32_t state;
	uint32_t pending;			/* bit 31: bank in bit 0 to load */
	uint32_t bank;
	int bilinear;
	uint32_t alpha;
	uint32_t mergeSetting;
	SceKernelSpinlock lock;
	int intrCode;
	SceUID controlUid;
	uint32_t reserved1;
	SceUID eventFlag;			/* IFTU2 conversion done */
} IftuPlane;

_Static_assert(offsetof(IftuPlane, cscParams2) == 0x10c, "IFTU CSC 2");
_Static_assert(offsetof(IftuPlane, planeState) == 0x184, "IFTU plane state");
_Static_assert(offsetof(IftuPlane, state) == 0x1e8, "IFTU state");
_Static_assert(offsetof(IftuPlane, lock) == 0x200, "IFTU lock");
_Static_assert(sizeof(IftuPlane) == 0x214, "IFTU plane context");

static IftuPlane s_iftuPlanes[5];
static SceKernelFastMutex s_iftuFastMutex;

static inline void IftuLoadCsc2(volatile SceIftuRegisters *regs,
	const SceIftuCscParams *csc)
{
	regs->cscParams2.add[0] = csc->post_add_0;
	regs->cscParams2.add[1] = csc->post_add_1_2;
	regs->cscParams2.matrix[0] = csc->ctm[0][0];
	regs->cscParams2.matrix[1] = csc->ctm[0][1];
	regs->cscParams2.matrix[2] = csc->ctm[0][2];
	regs->cscParams2.matrix[3] = csc->ctm[1][0];
	regs->cscParams2.matrix[4] = csc->ctm[1][1];
	regs->cscParams2.matrix[5] = csc->ctm[1][2];
	regs->cscParams2.matrix[6] = csc->ctm[2][0];
	regs->cscParams2.matrix[7] = csc->ctm[2][1];
	regs->cscParams2.matrix[8] = csc->ctm[2][2];
}

static inline void IftuLoadCsc1(volatile SceIftuRegisters *regs,
	const SceIftuCscParams *csc)
{
	regs->cscParams1.add[0] = csc->post_add_0;
	regs->cscParams1.add[1] = csc->post_add_1_2;
	regs->cscParams1.matrix[0] = csc->ctm[0][0];
	regs->cscParams1.matrix[1] = csc->ctm[0][1];
	regs->cscParams1.matrix[2] = csc->ctm[0][2];
	regs->cscParams1.matrix[3] = csc->ctm[1][0];
	regs->cscParams1.matrix[4] = csc->ctm[1][1];
	regs->cscParams1.matrix[5] = csc->ctm[1][2];
	regs->cscParams1.matrix[6] = csc->ctm[2][0];
	regs->cscParams1.matrix[7] = csc->ctm[2][1];
	regs->cscParams1.matrix[8] = csc->ctm[2][2];
	regs->clamp[0].max = csc->post_clamp_max_0;
	regs->clamp[0].min = csc->post_clamp_min_0;
	regs->clamp[1].max = csc->post_clamp_max_1_2;
	regs->clamp[1].min = csc->post_clamp_min_1_2;
}

/* Source half of a configuration bank, from a plane state. */
static inline void IftuLoadSource(volatile SceIftuBufferRegisters *buffer,
	const SceIftuPlaneState *state)
{
	buffer->format = state->fb.pixelformat;
	buffer->width = state->fb.width;
	buffer->height = state->fb.height;
	buffer->linePadding = state->fb.leftover_stride;
	buffer->leftoverAlign = state->fb.leftover_align;
	buffer->address[0] = state->fb.paddr0;
	buffer->address[1] = state->fb.paddr1;
	buffer->address[2] = state->fb.paddr2;
	buffer->sourceConfig[0] = state->sourceConfig[0];
	buffer->sourceConfig[1] = state->sourceConfig[1];
	buffer->sourceConfig[2] = state->sourceConfig[2];
	buffer->vtopPadding = state->vtop_padding;
	buffer->vbotPadding = state->vbot_padding;
	buffer->hleftPadding = state->hleft_padding;
	buffer->hrightPadding = state->hright_padding;
	buffer->scaleX = state->scaleX;
	buffer->scaleY = state->scaleY;
	buffer->dstX = state->dst_x;
	buffer->dstY = state->dst_y;
	buffer->sampleX = state->sampleX;
	buffer->sampleY = state->sampleY;
	buffer->disabled = 0;
}

/* A plane without a frame shows a blank output-sized frame. */
static inline void IftuLoadBlank(volatile SceIftuBufferRegisters *buffer,
	const IftuPlane *plane)
{
	buffer->disabled = 1;
	buffer->format = SCE_IFTU_PIXELFORMAT_BGRX8888;
	buffer->width = plane->output.width;
	buffer->height = plane->output.height;
	buffer->vtopPadding = 0;
	buffer->vbotPadding = 0;
	buffer->hleftPadding = 0;
	buffer->hrightPadding = 0;
	buffer->scaleX = 0x10000;
	buffer->scaleY = 0x10000;
}

static int Iftu2IntrHandler(int intrCode, void *arg)
{
	IftuPlane *plane = arg;
	volatile SceIftuRegisters *regs = plane->registers;

	(void)intrCode;
	(void)regs->interruptStatus;
	regs->interruptStatus = 0;
	(void)regs->interruptStatus;
	LowioDsb();
	sceKernelSetEventFlag(plane->eventFlag, 1);
	return -1;
}

static int IftuIntrHandler(int intrCode, void *arg)
{
	IftuPlane *plane = arg;
	volatile SceIftuRegisters *regs = plane->registers;
	uint32_t intr, pending;

	(void)intrCode;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
	if (plane->state == IFTU_STATE_DISABLED) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
		return -2;
	}
	pending = plane->pending;
	(void)regs->interruptStatus;
	regs->interruptEnable = 0;
	regs->interruptStatus = 0;
	(void)regs->interruptStatus;
	if (pending != 0) {
		regs->filter = plane->bilinear;
		if ((int)pending < 0) {
			volatile SceIftuBufferRegisters *buffer =
				&regs->buffer[pending & 1];

			if (plane->planeState.fb.pixelformat != 0)
				IftuLoadSource(buffer, &plane->planeState);
			else
				IftuLoadBlank(buffer, plane);
		}
		plane->pending = 0;
	}
	LowioDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
	return -1;
}

static inline __attribute__((always_inline)) void IftuStop(IftuPlane *plane)
{
	volatile SceIftuRegisters *regs = plane->registers;

	plane->state = IFTU_STATE_DISABLED;
	regs->buffer[0].disabled = 1;
	regs->buffer[1].disabled = 1;
	regs->interruptEnable = 0;
	regs->interruptStatus = 0;
	(void)regs->interruptStatus;
	LowioDsb();
	sceKernelDisableIntr(plane->intrCode);
}

static int IftuSysEvent(int resume, int eventId, void *args, void *opt)
{
	IftuPlane *plane, *converter = &s_iftuPlanes[IFTU_CONVERTER];
	uint32_t intr;

	(void)args;
	(void)opt;
	switch (resume) {
	case 0:
		if (eventId != 0x4005)
			break;
		for (plane = s_iftuPlanes; plane != converter; plane++) {
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
			if (plane->state != IFTU_STATE_DISABLED)
				IftuStop(plane);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
		}
		break;
	case 1:
		if (eventId != 0x10005)
			break;
		ScePervasiveForDriver_07F2A738();
		ScePervasiveForDriver_E92E28FF();
		(void)converter->registers->pathConfig;
		LowioDsb();
		converter->registers->pathConfig = 0x108;
		sceKernelEnableIntr(converter->intrCode);
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&converter->lock);
		if (converter->state != IFTU_STATE_DISABLED) {
			IftuLoadCsc1(converter->registers, &converter->cscParams1);
			LowioDmb();
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&converter->lock, intr);
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&converter->lock);
		if (converter->state != IFTU_STATE_DISABLED) {
			IftuLoadCsc2(converter->registers, &converter->cscParams2);
			LowioDmb();
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&converter->lock, intr);
		(void)converter->registers->pathConfig;
		LowioDsb();
		ScePervasiveForDriver_5AFE0AF0();
		break;
	}
	return 0;
}

int SceIftuForDriver_0FCBF457(unsigned int index, SceIftuCscParams *param)
{
	IftuPlane *plane;
	uint32_t intr;

	if (index > IFTU_CONVERTER)
		return SCE_IFTU_ERROR_INVALID_PLANE;
	plane = &s_iftuPlanes[index];
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
	if (param != NULL)
		__builtin_memcpy(&plane->cscParams2, param, sizeof(*param));
	else
		param = &plane->cscParams2;
	if (plane->state != IFTU_STATE_DISABLED) {
		IftuLoadCsc2(plane->registers, param);
		LowioDmb();
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
	return 0;
}

int SceIftuForDriver_D64F4C6B(unsigned int index, SceIftuCscParams *param)
{
	IftuPlane *plane;
	uint32_t intr;

	if (index > IFTU_CONVERTER)
		return SCE_IFTU_ERROR_INVALID_PLANE;
	plane = &s_iftuPlanes[index];
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
	if (param != NULL)
		__builtin_memcpy(&plane->cscParams1, param, sizeof(*param));
	else
		param = &plane->cscParams1;
	if (plane->state != IFTU_STATE_DISABLED) {
		IftuLoadCsc1(plane->registers, param);
		LowioDmb();
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
	return 0;
}

int SceIftuForDriver_E6EE2C6B(int index, unsigned int width,
	unsigned int height, unsigned int pixelFormat, unsigned int planeEnable)
{
	IftuPlane *plane;
	uint32_t intr, state;

	if ((unsigned int)index > 3)
		return SCE_IFTU_ERROR_INVALID_PLANE;
	if ((width & 0xf) != 0 || (height & 7) != 0)
		return SCE_IFTU_ERROR_INVALID_PARAM;
	switch (pixelFormat) {
	case SCE_IFTU_PIXELFORMAT_BGRX8888:
	case SCE_IFTU_PIXELFORMAT_RGBX8888:
	case SCE_IFTU_PIXELFORMAT_BGRA1010102:
	case SCE_IFTU_PIXELFORMAT_RGBA1010102:
	case SCE_IFTU_PIXELFORMAT_RGBX8888_MULT:
	case SCE_IFTU_PIXELFORMAT_BGRX8888_MULT:
	case SCE_IFTU_PIXELFORMAT_RGBA1010102_MULT:
	case SCE_IFTU_PIXELFORMAT_BGRA1010102_MULT:
		break;
	default:
		return SCE_IFTU_ERROR_INVALID_PIXELFORMAT;
	}
	plane = &s_iftuPlanes[index];
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
	state = plane->state;
	if (state != IFTU_STATE_DISABLED) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
		return SCE_IFTU_ERROR_PLANE_BUSY;
	}
	plane->output.width = width;
	plane->output.height = height;
	plane->output.pixelFormat = pixelFormat;
	plane->output.planeEnable = planeEnable;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
	return state;
}

static inline __attribute__((always_inline)) int IftuValidSourceFormat(uint32_t format)
{
	switch (format) {
	case 0:
	case SCE_IFTU_PIXELFORMAT_BGR565:
	case SCE_IFTU_PIXELFORMAT_RGB565:
	case SCE_IFTU_PIXELFORMAT_BGRA5551:
	case SCE_IFTU_PIXELFORMAT_RGBA5551:
	case SCE_IFTU_PIXELFORMAT_BGRX8888:
	case SCE_IFTU_PIXELFORMAT_RGBX8888:
	case SCE_IFTU_PIXELFORMAT_BGRA1010102:
	case SCE_IFTU_PIXELFORMAT_RGBA1010102:
	case SCE_IFTU_PIXELFORMAT_BGRP:
	case SCE_IFTU_PIXELFORMAT_RGBX8888_MULT:
	case SCE_IFTU_PIXELFORMAT_BGRX8888_MULT:
	case SCE_IFTU_PIXELFORMAT_RGBA1010102_MULT:
	case SCE_IFTU_PIXELFORMAT_BGRA1010102_MULT:
	case SCE_IFTU_PIXELFORMAT_NV12:
	case SCE_IFTU_PIXELFORMAT_YUV420:
	case SCE_IFTU_PIXELFORMAT_YUV422:
	case 0x80020000:
	case 0x80140000:
	case 0x80180000:
		return 1;
	}
	return 0;
}

int SceIftuForDriver_7CE0C4DA(unsigned int index, SceIftuPlaneState *state,
	int bilinear, int sync)
{
	IftuPlane *plane;
	volatile SceIftuRegisters *regs;
	volatile SceIftuBufferRegisters *buffer;
	volatile SceIftucPlaneRegisters *control;
	uint32_t intr, status, bank;

	if (index > IFTU_CONVERTER)
		return SCE_IFTU_ERROR_INVALID_PLANE;
	plane = &s_iftuPlanes[index];
	if (state == NULL) {
		state = &plane->planeState;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
	} else {
		if (!IftuValidSourceFormat(state->fb.pixelformat))
			return SCE_IFTU_ERROR_INVALID_PIXELFORMAT;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
		memcpy(&plane->planeState, state, sizeof(*state));
	}
	if (bilinear >= 0)
		plane->bilinear = bilinear;
	if (plane->state == IFTU_STATE_DISABLED)
		goto unlock;
	regs = plane->registers;
	if (plane->state == IFTU_STATE_STARTING) {
		plane->state = IFTU_STATE_ENABLED;
	} else if (!sync) {
		/* Move the frame in both banks at once. */
		bank = (regs->crtcMask >> 1) & 1;
		buffer = &regs->buffer[bank];
		buffer->address[0] = state->fb.paddr0;
		buffer->address[1] = state->fb.paddr1;
		buffer->address[2] = state->fb.paddr2;
		buffer->sourceConfig[0] = state->sourceConfig[0];
		buffer->sourceConfig[1] = state->sourceConfig[1];
		buffer->sourceConfig[2] = state->sourceConfig[2];
		buffer = &regs->buffer[bank ^ 1];
		buffer->address[0] = state->fb.paddr0;
		buffer->address[1] = state->fb.paddr1;
		buffer->address[2] = state->fb.paddr2;
		buffer->sourceConfig[0] = state->sourceConfig[0];
		buffer->sourceConfig[1] = state->sourceConfig[1];
		buffer->sourceConfig[2] = state->sourceConfig[2];
		goto done;
	}
	/* Load the bank not being scanned out, then switch to it. */
	status = regs->crtcMask;
	control = plane->planeControl;
	bank = (status >> 1) & 1;
	if (plane->bank == bank) {
		bank ^= 1;
	} else if (status & 1) {
		control->bankSelect = bank;
		plane->bank = bank;
		bank = ((regs->crtcMask >> 1) & 1) ^ 1;
	}
	buffer = &regs->buffer[bank];
	if (state->fb.pixelformat != 0)
		IftuLoadSource(buffer, state);
	else
		IftuLoadBlank(buffer, plane);
	control->bankSelect = bank;
	plane->bank = bank;
	if (regs->filter != (uint32_t)plane->bilinear) {
		plane->pending |= IFTU_PENDING_FILTER;
		regs->interruptEnable = 3;
	}
done:
	LowioDmb();
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
	return 0;
}

int SceIftuForDriver_357EAE24(unsigned int index, unsigned int alpha)
{
	IftuPlane *plane;
	volatile SceIftuRegisters *regs;
	uint32_t intr;

	if ((index & ~2) != 1)
		return SCE_IFTU_ERROR_INVALID_PLANE;
	if (alpha > IFTU_NO_BLEND)
		return SCE_IFTU_ERROR_INVALID_PARAM;
	plane = &s_iftuPlanes[index];
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
	plane->alpha = alpha;
	if (plane->state != IFTU_STATE_DISABLED) {
		regs = plane->registers;
		if (alpha == IFTU_NO_BLEND) {
			regs->alpha = 0;
			regs->alphaControl = 1;
		} else {
			regs->alpha = alpha;
			regs->alphaControl = 0;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
	return 0;
}

int SceIftuForDriver_AF19FD85(unsigned int index, int setting)
{
	IftuPlane *plane;
	volatile SceIftucRegisters *control;
	uint32_t intr;

	if ((index & ~2) != 1)
		return SCE_IFTU_ERROR_INVALID_PLANE;
	plane = &s_iftuPlanes[index];
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
	plane->mergeSetting = setting;
	if (plane->state != IFTU_STATE_DISABLED) {
		control = plane->control;
		if (setting == IFTU_NO_MERGE) {
			control->blendControl = 0;
			control->planeControl &= ~1;
		} else {
			control->blendControl = setting;
			control->planeControl |= 1;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
	return 0;
}

int SceIftuForDriver_0D7C02F7(unsigned int index)
{
	IftuPlane *plane;
	volatile SceIftuRegisters *regs;
	volatile SceIftucRegisters *control;
	uint32_t intr, planeControl;
	unsigned int i;

	if (index > 3)
		return SCE_IFTU_ERROR_INVALID_PLANE;
	plane = &s_iftuPlanes[index];
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
	if (plane->state != IFTU_STATE_DISABLED) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
		return 0;
	}
	regs = plane->registers;
	regs->pathConfig = 0x108;
	regs->filter = plane->bilinear;
	IftuLoadCsc2(regs, &plane->cscParams2);
	IftuLoadCsc1(regs, &plane->cscParams1);
	regs->backgroundColor[0] = 0;
	regs->backgroundColor[1] = 0;
	regs->backgroundColor[2] = 0;
	if (plane->alpha == IFTU_NO_BLEND) {
		regs->alpha = 0;
		regs->alphaControl = 1;
	} else {
		regs->alpha = plane->alpha;
		regs->alphaControl = 0;
	}
	for (i = 0; i < 2; i++) {
		volatile SceIftuBufferRegisters *buffer = &regs->buffer[i];

		buffer->dstFormat = plane->output.pixelFormat;
		buffer->dstWidth = plane->output.width;
		buffer->dstHeight = plane->output.height;
		if (plane->planeState.fb.pixelformat != 0)
			IftuLoadSource(buffer, &plane->planeState);
		else
			IftuLoadBlank(buffer, plane);
	}
	control = plane->control;
	planeControl = control->planeControl;
	if ((index & ~2) == 1) {
		uint32_t setting = plane->mergeSetting;

		if (setting == IFTU_NO_MERGE) {
			planeControl &= ~1;
			setting = 0;
		} else {
			planeControl |= 1;
		}
		control->blendControl = setting;
	}
	if (index & 1) {
		if (plane->output.planeEnable == 1) {
			planeControl |= 0x10;
			control->plane[1].companion = 0;
		} else {
			planeControl &= ~0x10;
		}
	} else if (plane->output.planeEnable == 1) {
		planeControl |= 4;
		control->plane[0].companion = 0;
	} else {
		planeControl &= ~4;
	}
	plane->bank = 0;
	plane->planeControl->bankSelect = 0;
	control->planeControl = planeControl;
	control->engineControl = 1;
	LowioDmb();
	plane->state = IFTU_STATE_STARTING;
	sceKernelEnableIntr(plane->intrCode);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
	return 0;
}

int SceIftuForDriver_C11F30B3(unsigned int index)
{
	IftuPlane *plane;
	uint32_t intr;

	if (index > 3)
		return SCE_IFTU_ERROR_INVALID_PLANE;
	plane = &s_iftuPlanes[index];
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&plane->lock);
	if (plane->state != IFTU_STATE_DISABLED)
		IftuStop(plane);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&plane->lock, intr);
	return 0;
}

int SceIftuForDriver_67E37EFC(SceIftuFrameBuf *dst, SceIftuPlaneState *src,
	SceIftuConvParams *params)
{
	IftuPlane *converter = &s_iftuPlanes[IFTU_CONVERTER];
	volatile SceIftuRegisters *regs;
	volatile SceIftuBufferRegisters *buffer;
	uint32_t intr, field;
	int ret;

	switch (dst->pixelformat & 0x7fffffff) {
	case SCE_IFTU_PIXELFORMAT_BGR565:
	case SCE_IFTU_PIXELFORMAT_RGB565:
	case SCE_IFTU_PIXELFORMAT_BGRA5551:
	case SCE_IFTU_PIXELFORMAT_RGBA5551:
	case SCE_IFTU_PIXELFORMAT_BGRX8888:
	case SCE_IFTU_PIXELFORMAT_RGBX8888:
	case SCE_IFTU_PIXELFORMAT_BGRA1010102:
	case SCE_IFTU_PIXELFORMAT_RGBA1010102:
	case SCE_IFTU_PIXELFORMAT_BGRP:
	case SCE_IFTU_PIXELFORMAT_RGBX8888_MULT:
	case SCE_IFTU_PIXELFORMAT_BGRX8888_MULT:
	case SCE_IFTU_PIXELFORMAT_RGBA1010102_MULT:
	case SCE_IFTU_PIXELFORMAT_BGRA1010102_MULT:
	case SCE_IFTU_PIXELFORMAT_NV12:
	case SCE_IFTU_PIXELFORMAT_YUV420:
	case SCE_IFTU_PIXELFORMAT_YUV422:
		break;
	default:
		return SCE_IFTU_ERROR_INVALID_PIXELFORMAT;
	}
	if (!IftuValidSourceFormat(src->fb.pixelformat))
		return SCE_IFTU_ERROR_INVALID_PIXELFORMAT;
	ret = lockFastMutex(&s_iftuFastMutex);
	if (ret < 0)
		return ret;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&converter->lock);
	ScePervasiveForDriver_07F2A738();
	regs = converter->registers;
	(void)regs->pathConfig;
	LowioDsb();
	if (params != NULL) {
		regs->filter = params->filterMode;
		if (params->csc_params1 != NULL) {
			__builtin_memcpy(&converter->cscParams1, params->csc_params1,
				sizeof(SceIftuCscParams));
			if (converter->state != IFTU_STATE_DISABLED) {
				IftuLoadCsc1(converter->registers, params->csc_params1);
				LowioDmb();
			}
		}
		if (params->csc_params2 != NULL) {
			__builtin_memcpy(&converter->cscParams2, params->csc_params2,
				sizeof(SceIftuCscParams));
			if (converter->state != IFTU_STATE_DISABLED) {
				IftuLoadCsc2(converter->registers, params->csc_params2);
				LowioDmb();
			}
		}
		regs->cscControl = params->csc_control;
		regs->backgroundColor[0] = params->backgroundColor[0];
		regs->backgroundColor[1] = params->backgroundColor[1];
		regs->backgroundColor[2] = params->backgroundColor[2];
		regs->alpha = params->alpha;
		field = params->fieldMode & 3;
	} else {
		regs->filter = 1;
		field = 0;
		regs->cscControl = 0;
		regs->backgroundColor[0] = 0;
		regs->backgroundColor[1] = 0;
		regs->backgroundColor[2] = 0;
		regs->alpha = 0xff;
	}
	/* The converter's bank: source, destination, then the scaling. */
	buffer = &regs->buffer[0];
	buffer->disabled = 0;
	buffer->format = src->fb.pixelformat;
	buffer->width = src->fb.width;
	buffer->height = src->fb.height;
	buffer->linePadding = src->fb.leftover_stride;
	buffer->leftoverAlign = src->fb.leftover_align;
	buffer->address[0] = src->fb.paddr0;
	buffer->address[1] = src->fb.paddr1;
	buffer->address[2] = src->fb.paddr2;
	buffer->sourceConfig[0] = src->sourceConfig[0];
	buffer->sourceConfig[1] = src->sourceConfig[1];
	buffer->sourceConfig[2] = src->sourceConfig[2];
	buffer->vtopPadding = src->vtop_padding;
	buffer->vbotPadding = src->vbot_padding;
	buffer->hleftPadding = src->hleft_padding;
	buffer->hrightPadding = src->hright_padding;
	buffer->dstFormat = dst->pixelformat;
	buffer->dstWidth = dst->width;
	buffer->dstHeight = dst->height;
	buffer->dstLinePadding = dst->leftover_stride;
	buffer->dstLeftoverAlign = dst->leftover_align;
	buffer->dstAddress[0] = dst->paddr0;
	buffer->dstAddress[1] = dst->paddr1;
	buffer->dstAddress[2] = dst->paddr2;
	buffer->scaleX = src->scaleX;
	buffer->scaleY = src->scaleY;
	buffer->dstX = src->dst_x;
	buffer->dstY = src->dst_y;
	buffer->sampleX = src->sampleX;
	buffer->sampleY = src->sampleY;
	regs->interruptEnable = 3;
	regs->pathConfig = 0x108;
	if (field == 2) {
		regs->fieldMode = 0;
		regs->control = 5;
	} else if (field == 3) {
		regs->fieldMode = 0;
		regs->control = 13;
	} else {
		regs->fieldMode = field == 1;
		regs->control = 1;
	}
	LowioDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&converter->lock, intr);
	sceKernelWaitEventFlag(converter->eventFlag, 1, 5, NULL, NULL);
	ScePervasiveForDriver_5AFE0AF0();
	ret = sceKernelUnlockFastMutex(&s_iftuFastMutex);
	return ret < 0 ? ret : 0;
}

/* Interrupt, register blocks and IFTUC control block of each unit. */
static const struct {
	int intrCode;
	const char *intrName;
	const char *name;
	uint32_t paddr;
	const char *controlName;
	uint32_t controlPaddr;
} s_iftuDevices[5] = {
	{ 0xcc, "SceIftu0a", "SceIftu0RegA", IFTU_PAGE_BASE(0, 0), "SceIftuc0Reg", IFTUC_REG_BASE(0) },
	{ 0xcd, "SceIftu0b", "SceIftu0RegB", IFTU_PAGE_BASE(0, 1), NULL, IFTUC_REG_BASE(0) },
	{ 0xce, "SceIftu1a", "SceIftu1RegA", IFTU_PAGE_BASE(1, 0), "SceIftuc1Reg", IFTUC_REG_BASE(1) },
	{ 0xcf, "SceIftu1b", "SceIftu1RegB", IFTU_PAGE_BASE(1, 1), NULL, IFTUC_REG_BASE(1) },
	{ 0xff, "SceIftu2", "SceIftu2Reg", IFTU2_REG_BASE, NULL, 0 },
};

static __attribute__((noinline)) int IftuInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	IftuPlane *plane;
	IftuPlane *converter = &s_iftuPlanes[IFTU_CONVERTER];
	void *base;
	unsigned int i;
	int ret;

	memset(s_iftuPlanes, 0, sizeof(s_iftuPlanes));
	for (i = 0; i < 5; i++) {
		plane = &s_iftuPlanes[i];
		plane->alpha = 0xff;
		plane->mergeSetting = IFTU_NO_MERGE;
		plane->bilinear = 1;
		if (i == IFTU_CONVERTER)
			plane->eventFlag = sceKernelCreateEventFlag("SceIftu2",
				0x1000, 0, NULL);
		else
			plane->eventFlag = -1;
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_iftuDevices[i].paddr;
		ret = sceKernelAllocMemBlockForKernel(s_iftuDevices[i].name, 0x20100806,
			0x1000, &opt);
		if (ret < 0)
			return ret;
		plane->controlUid = ret;
		sceKernelGetMemBlockBase(ret, &base);
		plane->registers = base;
		if (s_iftuDevices[i].controlPaddr != 0) {
			if (i & 1) {
				plane->controlUid = -1;
				plane->control = plane[-1].control;
				plane->planeControl = &plane->control->plane[1];
			} else {
				opt.size = 0x58;
				opt.attr = 2;
				opt.paddr = s_iftuDevices[i].controlPaddr;
				ret = sceKernelAllocMemBlockForKernel(
					s_iftuDevices[i].controlName, 0x20100806,
					0x1000, &opt);
				/* Sony does not check this allocation or mapping. */
				sceKernelGetMemBlockBase(ret, &base);
				plane->controlUid = ret;
				plane->control = base;
				plane->planeControl = &plane->control->plane[0];
			}
		} else {
			plane->control = NULL;
			plane->planeControl = NULL;
			plane->controlUid = -1;
		}
		plane->intrCode = s_iftuDevices[i].intrCode;
		if (plane->intrCode >= 0) {
			if (i == IFTU_CONVERTER) {
				sceKernelRegisterIntrHandler(plane->intrCode,
					"SceIftu2", 0, Iftu2IntrHandler, plane, 0x50,
					0xf, NULL);
				sceKernelEnableIntr(converter->intrCode);
				break;
			}
			sceKernelRegisterIntrHandler(plane->intrCode,
				s_iftuDevices[i].intrName, 0, IftuIntrHandler, plane,
				0x50, 0xf, NULL);
		}
	}
	ScePervasiveForDriver_07F2A738();
	ScePervasiveForDriver_E92E28FF();
	(void)converter->registers->pathConfig;
	LowioDsb();
	converter->registers->pathConfig = 0x108;
	converter->state = IFTU_STATE_ENABLED;
	(void)converter->registers->pathConfig;
	LowioDsb();
	ScePervasiveForDriver_5AFE0AF0();
	sceKernelInitializeFastMutex(&s_iftuFastMutex, "SceIftu2", 2, NULL);
	sceKernelRegisterSysEventHandler("SceIftu", IftuSysEvent, NULL);
	return 0;
}

int LowioIftuInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	IftuInit();
	return 0;
}
