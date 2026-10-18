/*
 * SceDisplay: display heads (main panel, HDMI), IFTU planes and frame buffers.
 *
 * The debug record of setIftuFrameBufAddr names forcompat.c, and its code and
 * strings lie inside display.c's, so display.c includes forcompat.c. The other
 * parts (vblank, boot logo, panels, HDMI, DTrace, syscalls) are linked after it,
 * each with its own data, as in the original. Internal names are unknown unless
 * a comment says otherwise.
 */
#include <psp2kern/types.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/processmgr.h>
#include <sysmem/procevent.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <psp2kern/kernel/intrmgr.h>
#include <sysmem/suspend.h>
#include <dmacmgr/dmacmgr.h>
#include <systimer/systimer.h>
#include <display/display.h>
#include <lowio/iftu.h>
#include <fw.h>
#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <excpmgr/excpmgr_cpu.h>
#include <kernel/error.h>
#include <sysmem/sysevent_ids.h>
#include "display_internal.h"
#include "display_imports.h"

DisplayHead heads[DISPLAY_HEAD_COUNT];
int hdmiOnly;				/* no main panel: head 0 drives HDMI */
SceUID devOwnerPid;
SceUInt32 *devStatus;
int primaryHead;
int startupHead;
int bootLogoShown;
int plsKey;
static SceKernelFastMutex mutex;
static int autoOwner;
static int hdmiPresent;
static int lcdPresent;
static SceUIntPtr dmaTagPaddr;
static int oledPresent;
static SceKernelDmaOpTag *dmaTag;

/* Colour conversion of a wide gamut (HDMI) head: YCbCr input. */
static const SceIftuCscParams cscWideInput = {
	0x40, 0x202, 0x3ff, 0, 0, 0,
	{ { 0x254, 0, 0x395 }, { 0x254, 0xf93, 0xef0 }, { 0x254, 0x439, 0 } },
};

static const SceUInt32 integerScale[5] = { 0x10000, 0x10000, 0x8000, 0x5555, 0x4000 };

static int initCallback(void) /* name unknown */
{
	return 0;
}

/* Show a frame buffer on an IFTU plane, scaled to the head's output. */
static int _sceDisplaySetFrameBufPhy(SceUInt32 head, SceUInt32 plane, SceUInt32 pitch,
				     SceUInt32 width, SceUInt32 height, SceUIntPtr paddr,
				     SceUInt32 pixelformat, SceUInt32 flags,
				     SceUInt32 sync) /* Sony's name (0.990 messages) */
{
	DisplayHead *h;
	SceIftuPlaneState state;
	SceUInt32 alignedWidth, alignedHeight;
	SceUInt32 outWidth, outHeight, planeFlags, vic;
	SceUInt32 dstWidth, dstHeight, srcHeight, stride;
	SceKernelIntrStatus intr;
	int ret, n;

	if (head > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (sync > 1)
		return SCE_DISPLAY_ERROR_INVALID_UPDATETIMING;

	h = &heads[head];
	alignedWidth = (width + 15) & ~15;
	alignedHeight = (height + 7) & ~7;
	state.fb.pixelformat = pixelformat;
	state.fb.width = alignedWidth;
	state.fb.height = alignedHeight;
	stride = pitch - alignedWidth;
	if (pixelformat < 9)
		state.fb.leftover_stride = stride * 2;
	else
		state.fb.leftover_stride = stride * 4;
	state.fb.leftover_align = 0;
	state.fb.paddr0 = paddr;
	state.fb.paddr1 = 0;
	state.fb.paddr2 = 0;
	state.sourceConfig[0] = 0;
	state.sourceConfig[1] = 0;
	state.sourceConfig[2] = 0;
	state.vtop_padding = 0;
	state.vbot_padding = alignedHeight - height;
	state.hleft_padding = 0;
	state.hright_padding = alignedWidth - width;

	if (width == 0 || height == 0) {
		state.scaleX = 0x10000;
		state.scaleY = 0x10000;
		state.dst_x = 0;
		state.dst_y = 0;
		goto set;
	}

	planeFlags = h->plane[plane].scaleFlags;
	switch (flags & 0xf0000) {
	case 0x10000:
		planeFlags |= 0x80;
		break;
	case 0x30000:
		planeFlags = 0x200;
		break;
	case 0x40000:
		if (h->width == width && h->height == height)
			planeFlags |= 0x80;
		else
			planeFlags = 0x400;
		break;
	}
	outWidth = h->width;
	outHeight = h->height;

	if (h->plane[plane].viewport[2] != 0 || h->plane[plane].viewport[3] != 0) {
		SceUInt32 cropWidth = (h->plane[plane].viewport[2] * width) >> 16;
		SceUInt32 cropHeight = (height * h->plane[plane].viewport[3]) >> 16;
		SceUInt32 cropX = (((width * h->plane[plane].viewport[0]) >> 16) + 63) & ~63;
		SceUInt32 cropY = (height * h->plane[plane].viewport[1]) >> 16;

		if (cropX + cropWidth >= width) {
			planeFlags |= 0x200;
			cropWidth = width - cropX;
		}
		width = cropWidth;
		if (cropHeight + cropY >= height) {
			cropHeight = height - cropY;
			planeFlags |= 0x200;
		}
		height = cropHeight;
		planeFlags &= ~0x80;
		if (pixelformat < 9)
			state.fb.paddr0 = paddr + (pitch * cropY + cropX) * 2;
		else
			state.fb.paddr0 = paddr + (pitch * cropY + cropX) * 4;
		state.vbot_padding = 0;
		state.hright_padding = 0;
	}

	if (!(planeFlags & 0x80) || outWidth < width || outHeight < height) {
		vic = h->vic;
		if (planeFlags & 1) {
			dstWidth = (SceUInt32)((float)(int)outWidth * h->plane[plane].scale);
			dstHeight = (SceUInt32)((float)(int)outHeight * h->plane[plane].scale);
		} else {
			dstWidth = outWidth;
			dstHeight = outHeight;
			if (planeFlags == 0x40) {
				dstWidth = 960;
				goto aspect;
			}
		}
		if (planeFlags & 0x200) {
			srcHeight = height << 16;
		} else if (planeFlags & 0x100) {
			if ((vic & ~0x200) == 0x8100 || (vic & ~0x200) == 0x8170)
				dstHeight = (dstWidth * 360) / 528;
			else if (vic == 0x82f0 || vic == 0x8280 || vic == 0x8480 || vic == 0x84f0)
				dstHeight = (dstWidth * 144) / 176;
			else
				dstHeight = (dstWidth * 9) >> 4;
			if (height == 272 && width == 480) {
				srcHeight = 270 << 16;
				state.vtop_padding = 1;
				state.vbot_padding = 1;
			} else if (height == 368 && width == 640) {
				srcHeight = 360 << 16;
				state.vtop_padding = 4;
				state.vbot_padding = 4;
			} else if (height == 488 && width == 704) {
				srcHeight = 480 << 16;
				state.vtop_padding = 4;
				state.vbot_padding = 4;
			} else if (height == 544 && width == 960) {
				srcHeight = 540 << 16;
				state.vtop_padding = 2;
				state.vbot_padding = 2;
			} else if (height == 725 && width == 1280) {
				state.vtop_padding = 2;
				srcHeight = 720 << 16;
				state.vbot_padding = 3;
			} else if (height == 736 && width == 1280) {
				srcHeight = 720 << 16;
				state.vtop_padding = 8;
				state.vbot_padding = 8;
			} else if ((height == 1088 && width == 1440) || (width == 1920 && height == 1088)) {
				srcHeight = 1080 << 16;
				state.vtop_padding = 4;
				state.vbot_padding = 4;
			} else {
				srcHeight = height << 16;
			}
		} else {
aspect:
			if (!(planeFlags & 0x400)) {
				if ((vic & ~0x200) == 0x8170 || (vic & ~0x200) == 0x8100)
					dstHeight = __aeabi_uidiv(dstWidth * height * 40, width) / 33;
				else if (vic == 0x8280 || vic == 0x82f0 || vic == 0x8480 || vic == 0x84f0)
					dstHeight = __aeabi_uidiv(dstWidth * height * 16, width) / 11;
				else
					dstHeight = __aeabi_uidiv(dstWidth * height, width);
				srcHeight = height << 16;
			} else {
				if ((vic & ~0x200) == 0x8100 || (vic & ~0x200) == 0x8170)
					dstHeight = (dstWidth * 360) / 528;
				else if (vic == 0x8280 || vic == 0x82f0 || vic == 0x8480 || vic == 0x84f0)
					dstHeight = (dstWidth * 144) / 176;
				else
					dstHeight = (dstWidth * 9) >> 4;
				if (height == 272 && width == 480) {
					srcHeight = 270 << 16;
					state.vbot_padding = 2;
				} else if (height == 368 && width == 640) {
					srcHeight = 360 << 16;
					state.vbot_padding = 8;
				} else if (height == 488 && width == 704) {
					srcHeight = 480 << 16;
					state.vbot_padding = 8;
				} else if (height == 544 && width == 960) {
					srcHeight = 540 << 16;
					state.vbot_padding = 4;
				} else if (height == 725 && width == 1280) {
					srcHeight = 720 << 16;
					state.vbot_padding = 5;
				} else if (height == 736 && width == 1280) {
					srcHeight = 720 << 16;
					state.vbot_padding = 16;
				} else if (height == 1080 && width == 1440) {
					srcHeight = 1080 << 16;
				} else if ((height == 1088 && width == 1440) || (width == 1920 && height == 1088)) {
					srcHeight = 1080 << 16;
					state.vbot_padding = 8;
				} else {
					srcHeight = height << 16;
				}
			}
		}
		state.scaleX = __aeabi_uidiv(dstWidth + width * 0x10000 - 1, dstWidth);
		state.scaleY = __aeabi_uidiv(srcHeight + dstHeight - 1, dstHeight);
		state.dst_x = outWidth < dstWidth ? 0 : (outWidth - dstWidth) >> 1;
		state.dst_y = outHeight < dstHeight ? 0 : (outHeight - dstHeight) >> 1;
	} else {
		if (outWidth >= width * 4 && outHeight >= height * 4)
			n = 4;
		else if (outWidth >= width * 3 && outHeight >= height * 3)
			n = 3;
		else if (outWidth >= width * 2 && outHeight >= height * 2)
			n = 2;
		else
			n = 1;
		state.scaleX = integerScale[n];
		state.scaleY = integerScale[n];
		flags = 0;
		state.dst_x = (outWidth - width * n) >> 1;
		state.dst_y = (outHeight - height * n) >> 1;
	}

set:
	state.sampleX = 0;
	state.sampleY = 0;
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	if (h->iftuPlane[plane] == -1) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	} else {
		ret = SceIftuForDriver_7CE0C4DA(h->iftuPlane[plane], &state,
						  (flags >> 20) & 1, sync);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int _sceDisplayModuleProcessCreate(SceUID pid, ProcEventCreateParam *param, int arg) /* Sony's name (0.990 messages) */
{
	DisplayProcessLocal *pls;
	SceUID event;
	int ret;

	ret = SceProcessmgrForDriver_AF80F39C(pid, plsKey, (void **)&pls, 1);
	if (ret < 0)
		return ret;
	memset(pls, 0, sizeof(*pls));
	if (SceProcessmgrForDriver_E1A67C86(pid, &pls->resolutionFlags) < 0)
		pls->resolutionFlags = 0;
	event = sceKernelCreateSimpleEvent("SceDispaly", 0x100, 0, NULL);
	pls->frameEvent = event | (event >> 31);
	pls->vcountBase[0] = heads[0].vcount;
	pls->vcountBase[1] = heads[1].vcount;
	return 0;
}

static const SceIftuCscParams cscDefaultOutput = {
	0, 0x202, 0x3ff, 0, 0, 0,
	{ { 0x200, 0, 0x326 }, { 0x200, 0xfa1, 0xf11 }, { 0x200, 0x3b6, 0 } },
};

static const SceIftuCscParams cscLimitedInput = {
	0x40, 0x40, 0x3ac, 0x40, 0x3ac, 0x40,
	{ { 0x1b7, 0, 0 }, { 0, 0x1b7, 0 }, { 0, 0, 0x1b7 } },
};

static const SceIftuCscParams cscLimitedOutput = {
	0x40, 0x202, 0x3ff, 0, 0, 0,
	{ { 0x254, 0, 0x395 }, { 0x254, 0xf93, 0xef0 }, { 0x254, 0x439, 0 } },
};

static const SceIftuCscParams cscWideOutput = {
	0, 0, 0x3ff, 0, 0x3ff, 0,
	{ { 0x200, 0, 0 }, { 0, 0x200, 0 }, { 0, 0, 0x200 } },
};

static const SceIftuCscParams cscDefaultInput = {
	0, 0, 0x3ff, 0, 0x3ff, 0,
	{ { 0x200, 0, 0 }, { 0, 0x200, 0 }, { 0, 0, 0x200 } },
};

/* Program the colour conversion of an IFTU plane for the head's gain and inversion. */
static int setPlaneCsc(DisplayHead *h, int plane) /* name unknown */
{
	const SceIftuCscParams *input, *output;
	SceIftuCscParams csc;
	SceUInt32 gain = h->gain;
	int wide;
	int i, j, ret;

	if (h->outputFlags & 0x8000) {
		output = &cscWideOutput;
		input = &cscWideInput;
		wide = 1;
	} else {
		wide = 0;
		input = &cscDefaultInput;
		output = &cscDefaultOutput;
		if (h->outputFlags & 1) {
			input = &cscLimitedInput;
			output = &cscLimitedOutput;
		}
	}

	csc = *input;
	if (h->invertColors == 1) {
		csc.ctm[0][0] = -csc.ctm[0][0];
		csc.ctm[1][1] = -csc.ctm[1][1];
		csc.ctm[2][2] = -csc.ctm[2][2];
		csc.post_add_0 = csc.post_clamp_max_0;
		csc.post_add_1_2 = csc.post_clamp_max_1_2;
		if (h->dsiBus == 1) {
			csc.post_add_0 = (csc.post_add_0 * gain) >> 16;
			csc.post_add_1_2 = (csc.post_add_1_2 * gain) >> 16;
			for (i = 0; i < 3; i++)
				for (j = 0; j < 3; j++)
					csc.ctm[i][j] = (csc.ctm[i][j] * gain) >> 16;
		}
	} else if (h->dsiBus == 1) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				csc.ctm[i][j] = (csc.ctm[i][j] * gain) >> 16;
	}
	if (plane == 2 && (h->mergeSetting & 3) == 2) {
		csc.post_add_0 = 0;
		csc.post_add_1_2 = 0;
		csc.post_clamp_min_0 = 0;
		csc.post_clamp_min_1_2 = 0;
	}
	ret = SceIftuForDriver_D64F4C6B(plane, &csc);
	if (ret < 0)
		return ret;

	csc = *output;
	if (wide) {
		csc.ctm[0][0] = (csc.ctm[0][0] * gain) >> 16;
		csc.ctm[1][0] = (csc.ctm[1][0] * gain) >> 16;
		csc.ctm[2][0] = (csc.ctm[2][0] * gain) >> 16;
	} else {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				csc.ctm[i][j] = (csc.ctm[i][j] * gain) >> 16;
	}
	ret = SceIftuForDriver_0FCBF457(plane, &csc);
	return ret < 0 ? ret : 0;
}

/* Start the DSI link and the IFTU planes of a head. */
static int _sceDisplayStartDsiIftu(SceUInt32 head) /* Sony's name (0.990 messages) */
{
	DisplayHead *h;
	int bus, ret, i;

	bus = heads[head].dsiBus;
	if (heads[head].dsiEnabled)
		return 0;
	if (bus == 1) {
		ret = SceDsiForDriver_78E6E3CF(1, 3, heads[head].pixelSize);
		if (ret < 0)
			return ret;
		ret = SceDsiForDriver_97BFEA76(1, heads[head].vic);
	} else {
		ret = SceDsiForDriver_97BFEA76(bus, heads[head].vic);
	}
	if (ret < 0)
		return ret;
	ret = ScePervasiveForDriver_91C80C41(bus, SceDsiForDriver_4DF9E924(bus));
	if (ret < 0)
		return ret;
	ret = ScePervasiveForDriver_BC42C72F(bus, 0xf);
	if (ret < 0)
		return ret;
	ret = ScePervasiveForDriver_FFB43AC2(bus, 7);
	if (ret < 0)
		return ret;
	ret = SceDsiForDriver_5BE5AA9B(bus);
	if (ret < 0)
		return ret;

	h = &heads[head];
	for (i = 0; i < DISPLAY_PLANE_COUNT; i++) {
		int plane = h->iftuPlane[i];

		ret = SceIftuForDriver_E6EE2C6B(plane, h->width, h->height, h->outputFormat, h->interlaced);
		if (ret < 0)
			return ret;
		setPlaneCsc(h, plane);
	}
	ret = SceIftuForDriver_0D7C02F7(h->iftuPlane[0]);
	if (ret < 0)
		return ret;
	ret = SceIftuForDriver_0D7C02F7(h->iftuPlane[1]);
	if (ret < 0)
		return ret;

	if (head == 0) {
		if (oledPresent && (ret = oledStart()) < 0)
			return ret;
		if (lcdPresent && (ret = lcdStart()) < 0)
			return ret;
		if (!oledPresent && !lcdPresent)
			goto done;
		if (bus != 1)
			goto start;
		if (startupHead == 1) {
			if (heads[1].dsiBus != 1)
				goto wait;
			ret = SceDsiForDriver_C2E85919(1, 2);
		} else if (startupHead == 2) {
			ret = SceDsiForDriver_C2E85919(bus, 3);
		} else {
wait:
			SceDisplayForDriver_984C27E7();
			ret = SceDsiForDriver_C2E85919(1, 0);
		}
	} else {
		if (!hdmiPresent)
			goto done;
start:
		if (startupHead == 0) {
			if (heads[0].dsiBus == 0) {
				ret = SceDsiForDriver_C2E85919(bus, 1);
			} else {
waitStart:
				SceDisplayForDriver_984C27E7();
				ret = SceDsiForDriver_C2E85919(bus, 0);
			}
		} else {
			if (startupHead != 2)
				goto waitStart;
			ret = SceDsiForDriver_C2E85919(bus, 3);
		}
	}
	if (ret < 0)
		return ret;
done:
	heads[head].dsiEnabled = 1;
	return 0;
}

static int vblankHandler(void *subintrArg, void *userCtx, unsigned char priority) /* name unknown */
{
	oledVblank();
	lcdVblank();
	return -1;
}

#include "forcompat.c"

/* Blank an IFTU plane (show nothing). */
static inline __attribute__((always_inline)) int clearPlane(SceUInt32 head, SceUInt32 plane) /* name unknown */
{
	DisplayHead *h = &heads[head];
	SceIftuPlaneState state;
	SceKernelIntrStatus intr;
	int iftu;

	if (head > 1 || plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	h->plane[plane].paddr = 0;
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	iftu = h->iftuPlane[plane];
	if (iftu == -1) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	} else {
		memset(&state, 0, sizeof(state));
		SceIftuForDriver_7CE0C4DA(iftu, &state, -1, 1);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	}
	return 0;
}

/* A process ends: take its frame buffers off every plane. */
static inline __attribute__((always_inline)) void releaseProcess(SceUID pid) /* name unknown */
{
	DisplayProcessLocal *pls = NULL;
	SceUInt32 head, plane;
	SceUID event;

	SceProcessmgrForDriver_AF80F39C(pid, plsKey, (void **)&pls, 1);
	for (head = 0; head < 2; head++) {
		for (plane = 0; plane < DISPLAY_PLANE_COUNT; plane++) {
			if (heads[head].plane[plane].owner == pid) {
				clearPlane(head, plane);
				heads[head].plane[plane].owner = -1;
			}
		}
	}
	if (devOwnerPid == pid) {
		devStatus = NULL;
		devOwnerPid = -1;
	}
	if (pls != NULL && pls->frameEvent > 0) {
		event = SceCpuForDriver_0EE04C03(&pls->frameEvent, 0);
		if (event > 0)
			sceKernelDeleteSimpleEvent(event);
	}
}

static int procKill(SceUID pid, ProcEventParam *param, int arg) /* name unknown */
{
	releaseProcess(pid);
	return 0;
}

static int procExit(SceUID pid, ProcEventParam *param, int arg) /* name unknown */
{
	releaseProcess(pid);
	return 0;
}

/* Stop showing anything on a head and, unless it is the startup head, power it down. */
static int disableHead(SceUInt32 head) /* name unknown */
{
	DisplayHead *h = &heads[head];
	SceUInt32 plane;
	int bus, ret;

	if (!h->enabled)
		return 0;
	if (h->dsiBus == 1) {
		if (hdmiPresent && (ret = _sceDisplaySetHdmiTxMute(1)) < 0)
			return ret;
	} else {
		if (oledPresent) {
			ret = _sceDisplayOledSetEnable(0);
			if (ret < 0)
				return ret;
			while (oledBusy())
				SceDisplayForDriver_B80CA224(head);
		}
		if (lcdPresent) {
			ret = lcdSetPower(0);
			if (ret < 0)
				return ret;
			while (lcdBusy())
				SceDisplayForDriver_B80CA224(head);
		}
	}
	ret = SceIftuForDriver_C11F30B3(h->iftuPlane[0]);
	if (ret < 0)
		return ret;
	ret = SceIftuForDriver_C11F30B3(h->iftuPlane[1]);
	if (ret < 0)
		return ret;
	if (startupHead != head) {
		bus = h->dsiBus;
		if (h->dsiEnabled) {
			for (plane = 0; plane < DISPLAY_PLANE_COUNT; plane++)
				clearPlane(head, plane);
			ret = SceDsiForDriver_7640F607(bus);
			if (ret >= 0) {
				sceKernelDelayThread(42000);
				ret = SceDsiForDriver_114D1413(bus);
				if (ret >= 0 && (ret = ScePervasiveForDriver_25AE181E(bus, 0xf)) >= 0
				    && (ret = ScePervasiveForDriver_E3FC1C8D(bus, 7)) >= 0) {
					ScePervasiveForDriver_91C80C41(bus, 0);
					h->dsiEnabled = 0;
				}
			}
		}
	}
	h->enabled = 0;
	return ret;
}

/* Power a head up and put the planes' owners' frame buffers back on it. */
static int enableHead(SceUInt32 head) /* name unknown */
{
	DisplayHead *h = &heads[head];
	SceUInt32 plane;
	SceUID owner;
	int ret;

	if (h->enabled)
		return 0;
	if (!h->dsiEnabled) {
		ret = _sceDisplayStartDsiIftu(head);
		if (ret < 0)
			return ret;
	}
	if (head == 0) {
		if (oledPresent && (ret = _sceDisplayOledSetEnable(1)) < 0)
			return ret;
		if (lcdPresent && (ret = lcdSetPower(1)) < 0)
			return ret;
	} else if (hdmiPresent) {
		ret = _sceDisplaySetupHdmiTx(h->vic, h->outputFlags, h->pixelSize);
		if (ret < 0)
			return ret;
	}
	for (plane = 0; plane < DISPLAY_PLANE_COUNT; plane++) {
		owner = h->plane[plane].owner;
		if (owner != -1) {
			h->plane[plane].owner = -1;
			SceDisplayForDriver_B54962A1(head, plane, owner);
		}
	}
	h->enabled = 1;
	return 0;
}

static inline __attribute__((always_inline)) void setHeadMode(DisplayHead *h, SceUInt32 vic,
	SceUInt32 width, SceUInt32 height, SceUInt32 flags, int interlaced) /* name unknown */
{
	SceKernelIntrStatus intr;

	if (lockFastMutex(&mutex) < 0)
		return;
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	h->vic = vic;
	h->outputFlags = flags;
	h->outputFormat = 0x2000;
	h->refreshRate = 60.0f / 1.001f;
	h->pixelSize = 0x18;
	h->width = width;
	h->height = height;
	h->interlaced = interlaced;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	sceKernelUnlockFastMutex(&mutex);
}

/* Work queue item: set the heads up once the panels are ready. */
static int initWork(void *arg) /* name unknown */
{
	void *volatile imported;
	SceUInt32 width, height;
	SceUInt32 head, plane;
	int genuineDolce;

	/* the panel drivers are optional: call them only when they were loaded */
	imported = NULL;
	(void)imported;
	imported = (void *)SceOledForDriver_0CC6BCB4;
	if (imported != NULL)
		SceOledForDriver_0CC6BCB4();
	imported = (void *)SceLcdForDriver_0C7E03D8;
	if (imported != NULL)
		SceLcdForDriver_0C7E03D8();

	if (hdmiOnly == 0) {
		if (SceDsiForDriver_B3A70C05(0, &width, &height) >= 0)
			setHeadMode(&heads[0], 0, width, height, 0, 0);
	} else {
		if (SceDsiForDriver_B3A70C05(0x8900, &width, &height) >= 0)
			setHeadMode(&heads[0], 0x8900, width, height, 0, 0);
	}
	if (hdmiOnly != 1) {
		if (SceSysrootForDriver_834439A7() == 0) {
			if (SceDsiForDriver_B3A70C05(0x8600, &width, &height) >= 0)
				setHeadMode(&heads[1], 0x8600, width, height, 1, 0);
		} else {
			if (SceDsiForDriver_B3A70C05(0x8300, &width, &height) >= 0)
				setHeadMode(&heads[1], 0x8300, width, height, 1, 0);
		}
		heads[1].gain = 0x10000;
		if (setPlaneCsc(&heads[1], heads[1].iftuPlane[0]) >= 0)
			setPlaneCsc(&heads[1], heads[1].iftuPlane[1]);
	}

	if (oledInit() >= 0) {
		oledPresent = 1;
		SceDsiForDriver_F2921E29(0, 0);
	}
	if (lcdInit() >= 0) {
		if (hdmiOnly == 0) {
			lcdPresent = 1;
			SceDsiForDriver_F2921E29(0, 1);
		} else {
			lcdPresent = 2;
			SceDsiForDriver_F2921E29(1, 1);
		}
	}
	if (hdmiInit() >= 0) {
		hdmiPresent = 1;
		if (lockFastMutex(&mutex) >= 0) {
			SceKernelIntrStatus intr = SceCpuForDriver_D32ACE9E(&heads[1].lock);

			if (SceIftuForDriver_AF19FD85(3, 4) >= 0) {
				heads[1].mergeSetting = 4;
				SceIftuForDriver_357EAE24(2, 0);
				SceIftuForDriver_357EAE24(3, 0xff);
			}
			sceKernelSpinlockLowUnlockCpuResumeIntr(&heads[1].lock, intr);
			sceKernelUnlockFastMutex(&mutex);
		}
	}

	genuineDolce = SceSblAIMgrForDriver_C6E83F34();
	if (SceSblAIMgrForDriver_71608CA3() != 0 && hdmiPresent) {
		startupHead = 3;
		primaryHead = 1;
	}
	sceKernelEnableSubIntr(8, 2);
	for (head = 0; head < DISPLAY_HEAD_COUNT; head++) {
		for (plane = 0; plane < DISPLAY_PLANE_COUNT; plane++) {
			if (head != 2) {
				clearPlane(head, plane);
				heads[head].plane[plane].owner = -1;
			}
		}
	}
	if (genuineDolce == 0 && lockFastMutex(&mutex) >= 0) {
		enableHead(0);
		sceKernelUnlockFastMutex(&mutex);
	}
	if (SceSysrootForDriver_B0E1FC67() == 0 && SceSysrootForDriver_89D19090() == 0
	    && SceSysrootForDriver_7918D44E() == 0 && SceSysrootForDriver_79C9AE10() == 0
	    && primaryHead == 0 && showBootLogo(0) >= 0)
		bootLogoShown = 1;
	return 1;
}

static int sysEventHandler(int resume, int eventid, void *args, void *opt) /* name unknown */
{
	SceUInt32 head, plane;

	if (resume == 0) {
		switch (eventid) {
		case SYSEVENT_SUSPEND_IDLE_ENTER:
			for (head = 0; head < DISPLAY_HEAD_COUNT; head++) {
				if (heads[head].enabled && head == 0) {
					if (oledPresent)
						_sceDisplayOledSetEnable(0);
					if (lcdPresent)
						lcdSetPower(0);
				}
			}
			break;
		case SYSEVENT_SUSPEND_PREPARE(0):
			for (head = 0; head < DISPLAY_HEAD_COUNT; head++) {
				if (heads[head].enabled && head == 1 && hdmiPresent) {
					_sceDisplaySetHdmiTxMute(1);
					if (lockFastMutex(&mutex) >= 0) {
						disableHead(1);
						sceKernelUnlockFastMutex(&mutex);
					}
				}
			}
			break;
		case SYSEVENT_SUSPEND_IDLE_QUERY:
			if (oledPresent && oledBusy())
				return -1;
			if (lcdPresent && lcdBusy())
				return -1;
			return 0;
		case SYSEVENT_SUSPEND_DEVICE(10):
			for (head = 0; head < DISPLAY_HEAD_COUNT; head++) {
				for (plane = 0; plane < DISPLAY_PLANE_COUNT; plane++) {
					if (head != 2 && heads[head].plane[plane].owner != SCE_KERNEL_PROCESS_ID) {
						clearPlane(head, plane);
						heads[head].plane[plane].owner = -1;
					}
				}
			}
			oledSuspend();
			lcdSuspend();
			vblankTimerStop();
			return 0;
		}
	} else if (resume == 1) {
		switch (eventid) {
		case SYSEVENT_RESUME_INTR:
			vblankResume();
			return 0;
		case SYSEVENT_RESUME_START:
			for (head = 0; head < DISPLAY_HEAD_COUNT; head++) {
				if (heads[head].enabled) {
					heads[head].enabled = 0;
					if (head != 1)
						SceDisplayForDriver_496032D6(head);
				}
			}
			break;
		case SYSEVENT_RESUME_DEVICE(10):
			heads[0].dsiEnabled = 0;
			heads[1].dsiEnabled = 0;
			heads[2].dsiEnabled = 0;
			if (startupHead == 0)
				_sceDisplayStartDsiIftu(0);
			else if (startupHead == 1)
				_sceDisplayStartDsiIftu(1);
			vblankTimerStart();
			return 0;
		}
	}
	return 0;
}

static const SceKernelProcEventHandler procEventHandler = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = _sceDisplayModuleProcessCreate,
	.exit = procExit,
	.kill = procKill,
	.suspend = procStop,
	.resume = procStart,
	.notify = NULL,
};

int displayInit(void) /* name unknown */
{
	SceUInt8 hardwareFlags[16];
	SceKernelAllocMemBlockKernelOpt opt;
	struct {
		SceSize size;
		int attr;
	} mutexOpt;
	void *base;
	SceUID block;
	int ret;

	memset(heads, 0, sizeof(heads));
	autoOwner = 1;
	devStatus = NULL;
	devOwnerPid = -1;
	SceSysrootForDriver_46E72428(hardwareFlags);
	hdmiOnly = 0;
	if ((hardwareFlags[0] & 8) == 0) {
		heads[0].dsiBus = 0;
		heads[0].iftuPlane[0] = 0;
		heads[0].iftuPlane[1] = 1;
		startupHead = 0;
		primaryHead = 0;
		heads[0].plane[0].scaleFlags = 1;
		heads[0].plane[1].scaleFlags = 1;
		heads[1].dsiBus = 1;
		heads[0].plane[0].scale = 1.0f;
		heads[0].plane[1].scale = 1.0f;
		heads[1].iftuPlane[0] = 2;
		heads[1].iftuPlane[1] = 3;
	} else {
		hdmiOnly = 1;
		heads[0].dsiBus = 1;
		heads[1].dsiBus = -1;
		heads[1].iftuPlane[0] = -1;
		heads[0].plane[0].scaleFlags = 1;
		startupHead = 0;
		primaryHead = 0;
		heads[0].plane[1].scaleFlags = 1;
		heads[1].iftuPlane[1] = -1;
		heads[0].plane[0].scale = 1.0f;
		heads[0].plane[1].scale = 1.0f;
		heads[0].iftuPlane[0] = 2;
		heads[0].iftuPlane[1] = 3;
	}
	heads[1].plane[0].scaleFlags = 0x80;
	heads[1].plane[0].scale = 0;
	heads[1].plane[1].scaleFlags = 0x80;
	heads[1].plane[1].scale = 0;
	plsKey = SceProcessmgrForDriver_3801D7D6("SceDisplay", sizeof(DisplayProcessLocal));
	mutexOpt.size = 8;
	mutexOpt.attr = 0x20;
	sceKernelInitializeFastMutex(&mutex, "SceDisplay", 6, &mutexOpt);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x200000;
	block = SceSysmemForDriver_C94850C9("SceDisplayDmaTag", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW, 0x8000, &opt);
	SceSysmemForDriver_A841EDDA(block, &base);
	dmaTag = base;
	SceSysmemForDriver_8D160E65(base, &dmaTagPaddr);
	vblankInit();
	sceKernelRegisterSubIntrHandler(8, 2, "SceDispalyVblank", vblankHandler, NULL);
	sceKernelRegisterSysEventHandler("SceDisplay", sysEventHandler, NULL);
	sceKernelRegisterProcEventHandler("SceDisplay", &procEventHandler, 0);
	sceKernelRegisterInitCallback(initCallback, 8, 0, 0);
	ret = sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceDisplayInit", initWork, NULL);
	return ret < 0 ? ret : 0;
}

int module_start(SceSize args, void *argp)
{
	displayInit();
	return 0;
}

/* Exported to SceDisplayForDriver and as syscall 0x7178FADA. */
int SceDisplayForDriver_C8E554C5(void)
{
	return primaryHead;
}

int SceDisplayForDriver_8D9A1CCE(SceUInt32 head, SceUInt32 *type)
{
	SceUInt32 device, kind;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (head == 0) {
		kind = 0;
		device = oledPresent;
		if (oledPresent) {
			kind = 0x103;
			device = 1;
		}
		if (lcdPresent) {
			device = 2;
			kind = 0x101;
		}
	} else if (head == 1) {
		device = hdmiPresent;
		kind = hdmiPresent;
		if (hdmiPresent) {
			kind = 0x101;
			device = 3;
		}
	} else {
		device = SCE_DISPLAY_ERROR_INVALID_HEAD;
		kind = 0;
	}
	if (type != NULL)
		*type = kind;
	return device;
}

int SceDisplayForDriver_AF5EE5BE(SceUInt32 head, SceUInt32 vic, SceUInt32 mode)
{
	SceUInt32 width, height, format, pixelSize;
	SceKernelIntrStatus intr;
	int interlaced, ret;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (head == 0) {
		switch (mode) {
		case 0x100000:
			format = 0x1000;
			break;
		case 0:
			format = 0x2000;
			break;
		case 0x60800000:
			format = 0x8000;
			break;
		case 0x60900000:
			format = 0x4000;
			break;
		default:
			return SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT;
		}
		ret = SceDsiForDriver_B3A70C05(vic, &width, &height);
		if (ret < 0)
			return ret;
		interlaced = 0;
		pixelSize = 0x18;
	} else if (head == 1) {
		switch (mode) {
		case 0x108000:
			pixelSize = 0x18;
			format = 0x20;
			break;
		case 0x8000:
			pixelSize = 0x18;
			format = 0x10;
			break;
		case 0:
		case 1:
			pixelSize = 0x18;
			format = 0x2000;
			break;
		case 0x100000:
		case 0x100001:
			pixelSize = 0x18;
			format = 0x1000;
			break;
		case 0x60808000:
			pixelSize = 0x1e;
			format = 0x40;
			break;
		case 0x60800000:
		case 0x60800001:
			pixelSize = 0x1e;
			format = 0x8000;
			break;
		case 0x60908000:
			pixelSize = 0x1e;
			format = 0x80;
			break;
		case 0x60900000:
		case 0x60900001:
			pixelSize = 0x1e;
			format = 0x4000;
			break;
		default:
			return SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT;
		}
		ret = SceDsiForDriver_B3A70C05(vic, &width, &height);
		if (ret < 0)
			return ret;
		interlaced = (vic & ~0x80) == 0x8500;
	} else {
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	}

	ret = lockFastMutex(&mutex);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_D32ACE9E(&heads[head].lock);
	heads[head].vic = vic;
	heads[head].outputFlags = mode;
	heads[head].outputFormat = format;
	heads[head].refreshRate = 60.0f / 1.001f;
	heads[head].pixelSize = pixelSize;
	heads[head].width = width;
	heads[head].height = height;
	heads[head].interlaced = interlaced;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&heads[head].lock, intr);
	sceKernelUnlockFastMutex(&mutex);
	return 0;
}

int SceDisplayForDriver_D2CED235(SceUInt32 head, SceUInt32 *vic, SceUInt32 *mode)
{
	DisplayHead *h;
	SceKernelIntrStatus intr;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	h = &heads[head];
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	if (vic != NULL)
		*vic = h->vic;
	if (mode != NULL)
		*mode = h->outputFlags;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	return 0;
}

int SceDisplayForDriver_B3C6D647(SceUInt32 head, SceDisplayResolutionInfo *info)
{
	DisplayHead *h;
	SceKernelIntrStatus intr;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (info == NULL || info->size != sizeof(*info))
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	h = &heads[head];
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	if (!h->enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
		return SCE_DISPLAY_ERROR_NO_OUTPUT_SIGNAL;
	}
	info->height = h->height;
	info->vic = h->vic;
	info->width = h->width;
	info->mode = h->outputFlags;
	info->interlaced = h->interlaced;
	info->refreshRate = h->refreshRate;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	return 0;
}

int SceDisplayForDriver_496032D6(SceUInt32 head)
{
	int ret;

	ret = lockFastMutex(&mutex);
	if (ret < 0)
		return ret;
	if (head == 0 || head == 1)
		ret = enableHead(head);
	else
		ret = SCE_DISPLAY_ERROR_INVALID_HEAD;
	sceKernelUnlockFastMutex(&mutex);
	return ret;
}

int SceDisplayForDriver_43347565(SceUInt32 head)
{
	int ret;

	ret = lockFastMutex(&mutex);
	if (ret < 0)
		return ret;
	if (head == 0 || head == 1)
		ret = disableHead(head);
	else
		ret = SCE_DISPLAY_ERROR_INVALID_HEAD;
	sceKernelUnlockFastMutex(&mutex);
	return ret;
}

/*
 * Physical address of a frame buffer: RAM inside [bound, bound + 256 MiB) must be
 * physically contiguous, anything else is resolved as a range.
 */
static inline __attribute__((always_inline)) int framebufPaddr(SceUIntPtr base, SceSize size,
	SceUIntPtr bound, SceUIntPtr *paddr) /* name unknown */
{
	SceKernelVARange va;
	SceKernelPARange pa;
	SceUIntPtr first, last;
	int ret;

	ret = SceSysmemForDriver_8D160E65((void *)base, &first);
	if (ret < 0)
		return ret;
	if (first - bound < 0x10000000) {
		ret = SceSysmemForDriver_8D160E65((void *)(base - 1 + size), &last);
		if (ret < 0)
			return ret;
		if (first + size - 1 != last)
			return SCE_DISPLAY_ERROR_INVALID_ADDR;
		*paddr = first;
		return 0;
	}
	va.addr = base;
	va.size = size;
	ret = SceSysmemForDriver_AE36C775(&va, &pa);
	if (ret < 0)
		return ret;
	*paddr = pa.addr;
	return 0;
}

/*
 * PSP emulation frame buffer (syscall _sceDisplaySetFrameBufForCompat). With
 * convDst, the frame is converted into that buffer instead of being shown.
 */
int setFrameBufForCompat(int x, int y, int cropWidth, int cropHeight,
				const SceDisplayFrameBuf *pParam, const SceDisplayFrameBuf *convDst) /* name unknown */
{
	DisplayProcessLocal *pls;
	DisplayFrameBufRecord *rec;
	SceIftuPlaneState state;
	SceIftuFrameBuf dst;
	SceIftuConvParams conv;
	SceUIntPtr paddr, dstPaddr, base;
	SceUInt32 pitch, pixelformat, width, height, resolution, size, iftuFormat;
	SceUInt32 vcount, flags;
	int head, shift, ret;
	SceUInt32 t;

	head = primaryHead;
	if (SceSblACMgrForDriver_FD00C72A(0) != 1)
		return SCE_ERROR_ERRNO_ENOSYS;
	t = sceSyscallEnter();
	ret = lockFastMutex(&mutex);
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	pls = SceProcessmgrForDriver_EE694840(plsKey);
	if (pParam == NULL) {
		shift = 1;
		resolution = 0x100000;
		pixelformat = 0;
		width = 0;
		height = 0;
		pitch = 0;
		iftuFormat = 0;
		paddr = 0;
	} else {
		if (pParam->size != DISPLAY_FRAMEBUF_PARAM_SIZE) {
			ret = SCE_DISPLAY_ERROR_INVALID_VALUE;
			goto unlock;
		}
		base = (SceUIntPtr)pParam->base;
		if (base == 0 || (base & 0xff) != 0) {
			ret = SCE_DISPLAY_ERROR_INVALID_ADDR;
			goto unlock;
		}
		pitch = pParam->pitch;
		pixelformat = pParam->pixelformat;
		width = pParam->width;
		height = pParam->height;
		resolution = ((const DisplayFrameBufParam *)pParam)->resolution;
		if (pitch & 0x3f) {
			ret = SCE_DISPLAY_ERROR_INVALID_PITCH;
			goto unlock;
		}
		if (pixelformat == 0) {
			shift = 2;
			iftuFormat = 0x10;
			if ((pitch - width) >> 11) {
				ret = SCE_DISPLAY_ERROR_INVALID_PITCH;
				goto unlock;
			}
		} else if (pixelformat == 0x50000000) {
			shift = 1;
			iftuFormat = 4;
			if ((pitch - width) >> 12) {
				ret = SCE_DISPLAY_ERROR_INVALID_PITCH;
				goto unlock;
			}
		} else {
			ret = SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT;
			goto unlock;
		}
		size = (pitch * ((height + 7) & ~7)) << shift;
		if (size == 0) {
			ret = SCE_DISPLAY_ERROR_INVALID_VALUE;
			goto unlock;
		}
		ret = framebufPaddr(base, size, 0x8000000, &paddr);
		if (ret < 0)
			goto unlock;
	}

	if (autoOwner && heads[head].plane[0].owner == -1)
		heads[head].plane[0].owner = sceKernelGetProcessId();
	flags = shift == 1 ? 6 : 4;
	if (convDst != NULL && pParam != NULL)
		flags = shift == 1 ? 2 : 0;
	setIftuFrameBufAddr(&state, pitch, width, height, x, y, cropWidth, cropHeight, paddr, flags);
	if (heads[head].plane[0].owner == sceKernelGetProcessId() && convDst == NULL) {
		ret = SceIftuForDriver_7CE0C4DA(heads[head].iftuPlane[0], &state, (resolution >> 20) & 1, 1);
		if (ret < 0)
			goto unlock;
		vcount = heads[head].vcount;
		rec = &pls->frameBuf[head][0];
		heads[head].plane[0].paddr = paddr;
		rec->vcountShown = vcount + 1;
		if (rec->frameBuf.pixelformat != pixelformat || pitch != rec->frameBuf.pitch
		    || width != rec->frameBuf.width || height != rec->frameBuf.height)
			pls->frameBuf[head][0].vcountChanged = vcount;
		if (pParam != NULL) {
			if (pParam->size == DISPLAY_FRAMEBUF_SIZE) {
				rec->frameBuf = *pParam;
				rec->resolution = resolution;
			} else {
				memcpy(rec, pParam, DISPLAY_FRAMEBUF_PARAM_SIZE);
			}
		}
		rec->cropHeight = cropHeight - 1;
		rec->cropWidth = cropWidth - 1;
		rec->paddr = paddr;
		rec->iftuPixelformat = iftuFormat;
		rec->flags = resolution;
		rec->cropY = y;
		rec->cropX = x;
	}
	if (convDst != NULL && pParam != NULL) {
		if (((SceUIntPtr)convDst->base & 0xff) != 0) {
			ret = SCE_DISPLAY_ERROR_INVALID_ADDR;
			goto unlock;
		}
		if (convDst->pixelformat != 0) {
			ret = SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT;
			goto unlock;
		}
		size = convDst->pitch * convDst->height * 4;
		ret = sceKernelLockRangeWithMode(0x30, convDst->base, size);
		if (ret < 0)
			goto unlock;
		if (size == 0) {
			ret = SCE_DISPLAY_ERROR_INVALID_VALUE;
			goto unlockRange;
		}
		ret = framebufPaddr((SceUIntPtr)convDst->base, size, 0x8000000, &dstPaddr);
		if (ret < 0)
			goto unlockRange;
		SceCpuForDriver_364E68A4(convDst->base, size);
		memset(&dst, 0, sizeof(dst));
		dst.pixelformat = 0x10;
		dst.width = convDst->width;
		dst.height = convDst->height;
		dst.leftover_stride = (convDst->pitch - convDst->width) * 4;
		dst.paddr0 = dstPaddr;
		memset(&conv, 0, sizeof(conv));
		conv.filterMode = (resolution >> 20) & 1;
		ret = SceIftuForDriver_67E37EFC(&dst, &state, &conv);
		SceCpuForDriver_02796361(convDst->base, size);
		SceSysmemForDriver_22CBE925(0x30, convDst->base, size);
		sceKernelUnlockFastMutex(&mutex);
		if (ret < 0) {
			sceSyscallLeave(t);
			return ret;
		}
	} else {
		sceKernelUnlockFastMutex(&mutex);
	}

	if (pls->frameEvent > 0)
		sceKernelPulseEvent(pls->frameEvent, heads[head].eventBits, 0);
	if (pParam != NULL && pls->frameBuf[head][0].notified == 0) {
		SceUID callback = heads[head].frameBufCallback;

		pls->frameBuf[head][0].notified = 1;
		if (callback != 0 && sceKernelNotifyCallback(callback, sceKernelGetProcessId()) < 0)
			heads[head].frameBufCallback = 0;
	}
	sceSyscallLeave(t);
	return 0;

unlockRange:
	SceSysmemForDriver_22CBE925(0x30, convDst->base, size);
unlock:
	sceKernelUnlockFastMutex(&mutex);
	sceSyscallLeave(t);
	return ret;
}

/* Blank an IFTU plane of a head, under the head's lock. */
static inline __attribute__((always_inline)) int blankPlane(DisplayHead *h, SceUInt32 plane) /* name unknown */
{
	SceIftuPlaneState state;
	SceKernelIntrStatus intr;
	int iftu, ret;

	h->plane[plane].paddr = 0;
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	iftu = h->iftuPlane[plane];
	if (iftu == -1) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
		return 0;
	}
	memset(&state, 0, sizeof(state));
	ret = SceIftuForDriver_7CE0C4DA(iftu, &state, -1, 1);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	return ret;
}

int SceDisplayForDriver_B54962A1(int head, int plane, SceUID pid)
{
	DisplayHead *h;
	DisplayProcessLocal *pls;
	DisplayFrameBufRecord *rec;
	SceIftuPlaneState state;
	SceUID owner;
	int ret;

	if ((SceUInt32)head > 1 || (SceUInt32)plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	ret = lockFastMutex(&mutex);
	if (ret < 0)
		return ret;
	if (bootLogoShown && head == primaryHead && pid != -1) {
		bootLogoShown = 0;
		freeBootLogo(head);
	}
	h = &heads[head];
	owner = h->plane[plane].owner;
	if (pid == -1) {
		h->plane[plane].owner = -1;
		if (owner != -1)
			blankPlane(h, plane);
		sceKernelUnlockFastMutex(&mutex);
		return 0;
	}
	if (owner != pid
	    && (ret = SceProcessmgrForDriver_AF80F39C(pid, plsKey, (void **)&pls, 0)) >= 0) {
		h->plane[plane].owner = pid;
		rec = &pls->frameBuf[head][plane];
		if (rec->paddr == 0) {
			ret = blankPlane(h, plane);
			if (ret >= 0) {
				sceKernelUnlockFastMutex(&mutex);
				return 0;
			}
		} else if (rec->cropWidth == 0) {
			ret = _sceDisplaySetFrameBufPhy(head, plane, rec->frameBuf.pitch, rec->frameBuf.width,
					       rec->frameBuf.height, rec->paddr, rec->iftuPixelformat,
					       rec->flags, 1);
		} else {
			setIftuFrameBufAddr(&state, rec->frameBuf.pitch, rec->frameBuf.width,
					    rec->frameBuf.height, rec->cropX, rec->cropY,
					    rec->cropWidth + 1, rec->cropHeight + 1, rec->paddr,
					    rec->iftuPixelformat == 0x10 ? 4 : 6);
			SceIftuForDriver_7CE0C4DA(h->iftuPlane[plane], &state, (rec->flags >> 20) & 1, 1);
		}
	}
	sceKernelUnlockFastMutex(&mutex);
	return ret;
}

/* Give a plane without an owner to the next process that sets a frame buffer. */
int SceDisplayForDriver_BC76296A(int enable)
{
	autoOwner = enable;
	return 0;
}

int __attribute__((pcs("aapcs-vfp"))) SceDisplayForDriver_EB390A76(float scale, SceUInt32 head,
							      SceUInt32 plane, SceUInt32 flags)
{
	DisplayHead *h;
	SceKernelIntrStatus intr;
	int ret;

	if (head > 1 || plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (flags & 1) {
		if ((flags & 0xc0) != 0 || scale < 0.8f || scale > 1.2f)
			return SCE_DISPLAY_ERROR_INVALID_VALUE;
	} else if (scale != scale) {
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	}
	ret = lockFastMutex(&mutex);
	if (ret < 0)
		return ret;
	h = &heads[head];
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	h->plane[plane].scaleFlags = flags;
	h->plane[plane].scale = scale;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	sceKernelUnlockFastMutex(&mutex);
	return 0;
}

int SceDisplayForDriver_6B198052(SceUInt32 head, SceUInt32 setting, SceUInt32 alpha)
{
	DisplayHead *h;
	SceKernelIntrStatus intr;
	int plane, ret;

	if (head > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (alpha > 0x100)
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	ret = lockFastMutex(&mutex);
	if (ret < 0)
		return ret;
	h = &heads[head];
	plane = head * 2 + 1;
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	ret = SceIftuForDriver_AF19FD85(plane, setting);
	if (ret < 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
		sceKernelUnlockFastMutex(&mutex);
		return ret;
	}
	h->mergeSetting = setting;
	SceIftuForDriver_357EAE24(head * 2, 0);
	SceIftuForDriver_357EAE24(plane, alpha);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	sceKernelUnlockFastMutex(&mutex);
	return 0;
}

int SceDisplayForDriver_9E3C6DC6(SceUInt32 head, SceUInt32 brightness)
{
	int i, ret;

	if (head == 0) {
		ret = 0;
		if (oledPresent)
			ret = oledSetBrightness(brightness);
		if (lcdPresent)
			ret = lcdSetBrightness(brightness);
		return ret;
	}
	if (head != 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (brightness == 0) {
		heads[1].gain = 0;
	} else if (brightness == 1) {
		heads[1].gain = 0x8000;
	} else {
		if (brightness > 0x10000)
			return SCE_DISPLAY_ERROR_INVALID_VALUE;
		heads[1].gain = (brightness >> 1) + 0x8000;
	}
	for (i = 0; i < DISPLAY_PLANE_COUNT; i++) {
		ret = setPlaneCsc(&heads[1], heads[1].iftuPlane[i]);
		if (ret < 0)
			return ret;
	}
	return 0;
}

int SceDisplayForDriver_3D95D478(SceUInt32 head, int mode)
{
	if (head != 0)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (!lcdPresent)
		return 0;
	return lcdSetMode(mode);
}

int SceDisplayForDriver_19140ACD(int head, int enable)
{
	int i;

	if (head == 0) {
		heads[0].invertColors = enable;
		for (i = 0; i < DISPLAY_PLANE_COUNT; i++)
			setPlaneCsc(&heads[0], heads[0].iftuPlane[i]);
	} else if (head == 1) {
		heads[1].invertColors = enable;
		for (i = 0; i < DISPLAY_PLANE_COUNT; i++)
			setPlaneCsc(&heads[1], heads[1].iftuPlane[i]);
	} else {
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	}
	return 0;
}

int SceDisplayForDriver_8D79D187(SceUInt32 head, int mode)
{
	int ret;

	if (head != 0)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	ret = 0;
	if (oledPresent)
		ret = oledSetColorSpaceMode(mode);
	if (lcdPresent)
		return lcdSetColorSpaceMode(mode);
	return ret;
}

int SceDisplayForDriver_EE5EB52D(SceUInt32 head, SceUInt32 plane, const SceDisplayViewportConf *conf)
{
	DisplayHead *h;
	DisplayProcessLocal *pls;
	DisplayFrameBufRecord *rec;
	SceKernelIntrStatus intr;
	SceUInt32 *viewport;
	int ret;

	if (head > 1 || plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (conf != NULL) {
		if (conf->size != sizeof(*conf))
			return SCE_DISPLAY_ERROR_INVALID_VALUE;
		if (conf->x == 0 && conf->y == 0) {
			if (conf->width == 960 && conf->height == 544)
				conf = NULL;
		} else if (conf->x > 640 || conf->y > 364) {
			return SCE_DISPLAY_ERROR_INVALID_VALUE;
		}
	}
	ret = lockFastMutex(&mutex);
	if (ret < 0)
		return ret;
	h = &heads[head];
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	viewport = h->plane[plane].viewport;
	if (conf == NULL) {
		viewport[0] = 0;
		viewport[1] = 0;
		viewport[2] = 0x10000;
		viewport[3] = 0x10000;
	} else {
		viewport[0] = (conf->x << 16) / 960;
		viewport[1] = (conf->y << 16) / 544;
		viewport[2] = (conf->width << 16) / 960;
		viewport[3] = (conf->height << 16) / 544;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	if (h->plane[plane].owner != -1) {
		if (SceProcessmgrForDriver_AF80F39C(h->plane[plane].owner, plsKey,
							       (void **)&pls, 0) < 0)
			pls = NULL;
		else if (pls != NULL) {
			rec = &pls->frameBuf[head][plane];
			if (rec->paddr != 0 && rec->cropWidth == 0)
				_sceDisplaySetFrameBufPhy(head, plane, rec->frameBuf.pitch, rec->frameBuf.width,
						 rec->frameBuf.height, rec->paddr, rec->iftuPixelformat,
						 rec->flags, 1);
		}
	}
	sceKernelUnlockFastMutex(&mutex);
	return 0;
}

int SceDisplayForDriver_40ACFE51(SceUInt32 head, SceUInt32 plane, SceDisplayViewportConf *conf)
{
	DisplayHead *h;
	DisplayProcessLocal *pls;
	DisplayFrameBufRecord *rec;
	SceKernelIntrStatus intr;
	SceUInt32 width, height, x, y, w, ht;
	int ret;

	if (head > 1 || plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (conf == NULL || conf->size != sizeof(*conf))
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	ret = lockFastMutex(&mutex);
	if (ret < 0)
		return ret;
	h = &heads[head];
	if (h->plane[plane].owner == -1) {
		conf->x = 0;
		conf->y = 0;
		conf->width = 0;
		conf->height = 0;
		sceKernelUnlockFastMutex(&mutex);
		return 0;
	}
	ret = SceProcessmgrForDriver_AF80F39C(h->plane[plane].owner, plsKey, (void **)&pls, 0);
	if (ret < 0) {
		sceKernelUnlockFastMutex(&mutex);
		return ret;
	}
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	rec = &pls->frameBuf[head][plane];
	width = rec->frameBuf.width;
	height = rec->frameBuf.height;
	if (rec->cropWidth == 0 && height != 0 && width != 0) {
		x = ((width * h->plane[plane].viewport[0] >> 16) + 0x3f) & ~0x3f;
		y = height * h->plane[plane].viewport[1] >> 16;
		w = h->plane[plane].viewport[2] * width >> 16;
		ht = height * h->plane[plane].viewport[3] >> 16;
		conf->x = x;
		if (x + w >= width)
			w = width - x;
		conf->y = y;
		if (ht + y >= height)
			ht = height - y;
		conf->width = width * w / 960;
		conf->height = ht * height / 544;
	} else {
		conf->x = 0;
		conf->y = 0;
		conf->width = 0;
		conf->height = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	sceKernelUnlockFastMutex(&mutex);
	return 0;
}

int SceDisplayForDriver_7911958E(SceUInt32 head, float *rate, int *interlaced)
{
	DisplayHead *h;
	SceKernelIntrStatus intr;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	h = &heads[head];
	intr = SceCpuForDriver_D32ACE9E(&h->lock);
	if (rate != NULL)
		*rate = h->refreshRate;
	if (interlaced != NULL)
		*interlaced = h->interlaced;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&h->lock, intr);
	return 0;
}

int sceDisplayGetRefreshRate(float *pFps)
{
	SceUInt32 head = primaryHead;
	SceKernelIntrStatus intr;
	float rate;
	int ret;

	if (pFps == NULL)
		return SCE_DISPLAY_ERROR_INVALID_ADDR;
	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	intr = SceCpuForDriver_D32ACE9E(&heads[head].lock);
	rate = heads[head].refreshRate;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&heads[head].lock, intr);
	ret = SceSysmemForDriver_6D88EF8A(pFps, &rate, sizeof(rate));
	return ret < 0 ? ret : 0;
}

int SceDisplayForDriver_332C5410(int head)
{
	if (head < 3) {
		if (!heads[head].enabled)
			return 0;
		if (head == 0)
			return SceDsiForDriver_6F8029A1(heads[0].dsiBus);
		if (head == 1)
			return SceDsiForDriver_6F8029A1(heads[1].dsiBus);
	}
	return SCE_DISPLAY_ERROR_INVALID_HEAD;
}

int SceDisplayForDriver_5AFE6CD3(int *width, int *height)
{
	DisplayProcessLocal *pls;
	SceUInt32 flags;
	int w, h;

	pls = SceProcessmgrForDriver_EE694840(plsKey);
	flags = 0;
	if (pls != NULL)
		flags = pls->resolutionFlags;
	if (SceSblAIMgrForDriver_71608CA3() == 0 || sceSblACMgrIsGameProgram(0) != 0) {
		if (isHighResolutionTitle())
			flags |= 1;
	} else {
		flags |= 3;
	}
	if (flags & 2) {
		h = 1088;
		w = 1920;
	} else if (flags & 1) {
		h = 725;
		w = 1280;
	} else {
		h = 544;
		w = 960;
	}
	if (width != NULL)
		*width = w;
	if (height != NULL)
		*height = h;
	return 0;
}

/* Development mode: report display state to a process's status word. */
void SceDisplayForDriver_086DEFB6(SceUInt32 *status, SceUID pid)
{
	if (SceSblACMgrForDriver_E87D1777() != 1)
		return;
	if (pid == 0)
		pid = sceKernelGetProcessId();
	devStatus = status;
	devOwnerPid = pid;
}

/* A title may use a frame buffer beyond the panel's resolution. */
static inline __attribute__((always_inline)) int highResolutionAllowed(void) /* name unknown */
{
	if (SceSblAIMgrForDriver_71608CA3() != 0 && sceSblACMgrIsGameProgram(0) == 0)
		return 1;
	return isHighResolutionTitle();
}

__attribute__((noinline))
int SceDisplayForDriver_16466675(int head, int plane, const SceDisplayFrameBuf *pParam,
				   int sync)
{
	DisplayHead *h;
	DisplayProcessLocal *pls;
	DisplayFrameBufRecord *rec;
	SceUIntPtr base, paddr;
	SceUInt32 pitch, pixelformat, width, height, resolution, iftuFormat, size, sdkVersion, vcount;
	SceUID callback;
	int ok, ret;
	SceUInt32 t;

	t = sceSyscallEnter();
	ret = lockFastMutex(&mutex);
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	pls = SceProcessmgrForDriver_EE694840(plsKey);
	if ((SceUInt32)head > 1 || (SceUInt32)plane > 1) {
		ret = SCE_DISPLAY_ERROR_INVALID_HEAD;
		goto unlock;
	}
	if ((SceUInt32)sync > 1) {
		ret = SCE_DISPLAY_ERROR_INVALID_UPDATETIMING;
		goto unlock;
	}
	rec = &pls->frameBuf[head][plane];
	h = &heads[head];

	if (pParam == NULL) {
		resolution = 0x100000;
		pixelformat = 0;
		pitch = 0;
		width = 0;
		height = 0;
		iftuFormat = 0;
		if (sync == 0) {
			if (rec->frameBuf.pitch != 0) {
				ret = SCE_DISPLAY_ERROR_INVALID_UPDATETIMING;
				goto unlock;
			}
		}
		paddr = 0;
		goto set;
	}

	if ((pParam->size & ~4) != DISPLAY_FRAMEBUF_SIZE) {
		ret = SCE_DISPLAY_ERROR_INVALID_VALUE;
		goto unlock;
	}
	base = (SceUIntPtr)pParam->base;
	if (base == 0 || (base & 0xff) != 0) {
		ret = SCE_DISPLAY_ERROR_INVALID_ADDR;
		goto unlock;
	}
	pitch = pParam->pitch;
	pixelformat = pParam->pixelformat;
	width = pParam->width;
	height = pParam->height;
	resolution = 0x100000;
	if (pParam->size == DISPLAY_FRAMEBUF_PARAM_SIZE) {
		resolution = ((const DisplayFrameBufParam *)pParam)->resolution;
		if (resolution & 0xffe0ffff) {
			if (SceProcessmgrForDriver_D141C076(0, &sdkVersion) >= 0 && sdkVersion > 0x24fffff) {
				ret = SCE_DISPLAY_ERROR_INVALID_VALUE;
				goto unlock;
			}
			resolution = 0x100000;
		}
	}
	if (pitch & 0x3f) {
		ret = SCE_DISPLAY_ERROR_INVALID_PITCH;
		goto unlock;
	}
	if (sceKernelIsUserModeThread() != 0 && pixelformat != 0 && pixelformat != 0x8000
	    && pixelformat != 0x60800000 && pixelformat != 0x60808000) {
		ret = SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT;
		goto unlock;
	}
	switch (pixelformat) {
	case 0:
		iftuFormat = 0x10;
		break;
	case 0x100000:
		iftuFormat = 0x20;
		break;
	case 0x8000:
		iftuFormat = 0x2000;
		break;
	case 0x108000:
		iftuFormat = 0x1000;
		break;
	case 0x60800000:
		iftuFormat = 0x40;
		break;
	case 0x60808000:
		iftuFormat = 0x8000;
		break;
	case 0x60900000:
		iftuFormat = 0x80;
		break;
	case 0x60908000:
		iftuFormat = 0x4000;
		break;
	default:
		ret = SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT;
		goto unlock;
	}

	if (head == 0 && !(height == 272 && width == 480) && !(height == 368 && width == 640)
	    && !(height == 408 && width == 720) && !(height == 488 && width == 704)
	    && !(height == 544 && width == 960)) {
		if (width == 1280 && (height == 725 || height == 736)) {
			if (!(pls->resolutionFlags & 1) && !highResolutionAllowed()) {
				ret = SCE_DISPLAY_ERROR_INVALID_RESOLUTION;
				goto unlock;
			}
		} else if (height == 1088 && (width == 1440 || width == 1920)) {
			if (!(pls->resolutionFlags & 2) && !highResolutionAllowed()) {
				ret = SCE_DISPLAY_ERROR_INVALID_RESOLUTION;
				goto unlock;
			}
		} else {
			if (height == 480 && width == 720)
				ok = 0;
			else if (height == 720 && width == 1280)
				ok = pls->resolutionFlags & 1;
			else if (height == 1080 && (width == 1440 || width == 1920))
				ok = pls->resolutionFlags & 2;
			else {
				ret = SCE_DISPLAY_ERROR_INVALID_RESOLUTION;
				goto unlock;
			}
			if ((!ok && !highResolutionAllowed())
			    || ((resolution & 0xd0000) != 0x10000 && (resolution & 0xf0000) != 0x40000)) {
				ret = SCE_DISPLAY_ERROR_INVALID_RESOLUTION;
				goto unlock;
			}
		}
	}
	if (width > pitch || (pitch - width) * 4 >= 0x800) {
		ret = SCE_DISPLAY_ERROR_INVALID_PITCH;
		goto unlock;
	}
	size = height * pitch * 4;
	if (size == 0) {
		ret = SCE_DISPLAY_ERROR_INVALID_VALUE;
		goto unlock;
	}
	ret = framebufPaddr(base, size, 0x20000000, &paddr);
	if (ret < 0)
		goto unlock;
	if (paddr - 0x20000000 >= 0x10000000 && sceSblACMgrIsGameProgram(0) != 0) {
		ret = SCE_DISPLAY_ERROR_INVALID_ADDR;
		goto unlock;
	}
	if (sync == 0) {
		if (paddr == 0) {
			if (rec->frameBuf.pitch != 0) {
				ret = SCE_DISPLAY_ERROR_INVALID_UPDATETIMING;
				goto unlock;
			}
		} else {
			if (rec->frameBuf.pitch == 0 || pixelformat != rec->frameBuf.pixelformat
			    || pitch != rec->frameBuf.pitch || width != rec->frameBuf.width
			    || height != rec->frameBuf.height) {
				ret = SCE_DISPLAY_ERROR_INVALID_UPDATETIMING;
				goto unlock;
			}
			if (rec->vcountChanged == h->vcount)
				goto notify;
		}
	}

set:
	if (autoOwner) {
		if (bootLogoShown && head == primaryHead) {
			bootLogoShown = 0;
			freeBootLogo(head);
		}
		if (h->plane[plane].owner == -1)
			h->plane[plane].owner = sceKernelGetProcessId();
	}
	if (h->plane[plane].owner == sceKernelGetProcessId()) {
		ret = _sceDisplaySetFrameBufPhy(head, plane, pitch, width, height, paddr, iftuFormat, resolution, sync);
		if (ret < 0)
			goto unlock;
		h->plane[plane].paddr = paddr;
	}
	if (sync == 0) {
		rec->vcountShown = h->vcount;
	} else {
		vcount = h->vcount;
		rec->vcountShown = vcount + 1;
		if (pixelformat != rec->frameBuf.pixelformat || pitch != rec->frameBuf.pitch
		    || width != rec->frameBuf.width || height != rec->frameBuf.height)
			pls->frameBuf[head][0].vcountChanged = vcount;
	}
	if (pParam != NULL) {
		if (pParam->size == DISPLAY_FRAMEBUF_SIZE) {
			rec->frameBuf = *pParam;
			rec->resolution = resolution;
		} else {
			rec->frameBuf = *pParam;
			rec->resolution = ((const DisplayFrameBufParam *)pParam)->resolution;
		}
	}
	rec->paddr = paddr;
	rec->cropWidth = 0;
	rec->iftuPixelformat = iftuFormat;
	rec->flags = resolution;

notify:
	sceKernelUnlockFastMutex(&mutex);
	if (pls->frameEvent > 0)
		sceKernelPulseEvent(pls->frameEvent, h->eventBits, 0);
	if (devStatus != NULL && head == primaryHead && sceKernelGetProcessId() == devOwnerPid)
		*devStatus = sync == 1 ? 0x10800000 : 0x10000000;
	if (pParam != NULL && rec->notified == 0) {
		callback = h->frameBufCallback;
		rec->notified = 1;
		if (callback != 0 && sceKernelNotifyCallback(callback, sceKernelGetProcessId()) < 0)
			h->frameBufCallback = 0;
	}
	sceSyscallLeave(t);
	return 0;

unlock:
	sceKernelUnlockFastMutex(&mutex);
	sceSyscallLeave(t);
	return ret;
}

int SceDisplayForDriver_19F94C63(int head, int plane, SceDisplayFrameBuf *pParam, int sync)
{
	DisplayProcessLocal *pls;
	SceSize size;
	SceUInt32 t;
	int ret;

	if ((SceUInt32)head > 1 || (SceUInt32)plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (pParam == NULL)
		return SCE_DISPLAY_ERROR_INVALID_ADDR;
	if ((pParam->size & ~4) != DISPLAY_FRAMEBUF_SIZE)
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	if ((SceUInt32)sync > 1)
		return SCE_DISPLAY_ERROR_INVALID_UPDATETIMING;
	t = sceSyscallEnter();
	ret = lockFastMutex(&mutex);
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	pls = SceProcessmgrForDriver_EE694840(plsKey);
	size = pParam->size;
	if (size > DISPLAY_FRAMEBUF_PARAM_SIZE)
		size = DISPLAY_FRAMEBUF_PARAM_SIZE;
	memcpy(pParam, &pls->frameBuf[head][plane], size);
	sceKernelUnlockFastMutex(&mutex);
	sceSyscallLeave(t);
	return 0;
}

/* Call a syscall-entry export from inside a syscall: TPIDRURO back to the user value. */
static inline __attribute__((always_inline)) SceUInt32 sceSyscallResume(void) /* name unknown */
{
	SceUInt32 t = readTpidruro();

	writeTpidruro(t >> 16);
	return t;
}

int SceDisplayForDriver_289D82FE(const SceDisplayFrameBuf *pParam, int sync)
{
	SceUInt32 t, u;
	int ret;

	t = sceSyscallEnter();
	ret = lockFastMutex(&mutex);
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	u = sceSyscallResume();
	ret = SceDisplayForDriver_16466675(0, 0, pParam, sync);
	sceSyscallLeave(u);
	if (ret < 0) {
		sceKernelUnlockFastMutex(&mutex);
		sceSyscallLeave(t);
		return ret;
	}
	u = sceSyscallResume();
	ret = SceDisplayForDriver_16466675(1, 0, pParam, sync);
	sceSyscallLeave(u);
	if (ret < 0) {
		sceKernelUnlockFastMutex(&mutex);
		sceSyscallLeave(t);
		return ret;
	}
	sceKernelUnlockFastMutex(&mutex);
	sceSyscallLeave(t);
	return 0;
}

int SceDisplayForDriver_EEDA2E54(SceDisplayFrameBuf *pParam, int sync)
{
	SceUInt32 head = primaryHead;
	DisplayProcessLocal *pls;
	SceSize size;
	SceUInt32 t;
	int ret;

	if ((SceUInt32)head > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (pParam == NULL)
		return SCE_DISPLAY_ERROR_INVALID_ADDR;
	if ((pParam->size & ~4) != DISPLAY_FRAMEBUF_SIZE)
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	if ((SceUInt32)sync > 1)
		return SCE_DISPLAY_ERROR_INVALID_UPDATETIMING;
	t = sceSyscallEnter();
	ret = lockFastMutex(&mutex);
	if (ret < 0) {
		sceSyscallLeave(t);
		return ret;
	}
	pls = SceProcessmgrForDriver_EE694840(plsKey);
	size = pParam->size;
	if (size > DISPLAY_FRAMEBUF_PARAM_SIZE)
		size = DISPLAY_FRAMEBUF_PARAM_SIZE;
	memcpy(pParam, &pls->frameBuf[head][0], size);
	sceKernelUnlockFastMutex(&mutex);
	sceSyscallLeave(t);
	return 0;
}

int SceDisplayForDriver_3BC165EF(SceUID pid, int head, int plane,
				       SceDisplayFrameBufInfo *info)
{
	DisplayProcessLocal *pls;
	DisplayFrameBufRecord *rec;
	SceUIntPtr paddr;
	int ret;

	if ((SceUInt32)head > 1 || (SceUInt32)plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (info == NULL)
		return SCE_DISPLAY_ERROR_INVALID_ADDR;
	if (info->size != sizeof(*info))
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	ret = lockFastMutex(&mutex);
	if (ret < 0)
		return ret;
	if (pid == -1)
		pid = heads[head].plane[plane].owner;
	else if (pid == 0)
		pid = sceKernelGetProcessId();
	ret = SceProcessmgrForDriver_AF80F39C(pid, plsKey, (void **)&pls, 0);
	if (ret < 0) {
		sceKernelUnlockFastMutex(&mutex);
		return ret;
	}
	info->pid = pid;
	rec = &pls->frameBuf[head][plane];
	paddr = rec->paddr;
	info->vblankcount = rec->vcountShown;
	if (paddr == 0)
		SceSysmemForDriver_61A67D32(pid, 0x10, rec->frameBuf.base, &paddr);
	info->paddr = paddr;
	info->framebuf = pls->frameBuf[head][plane].frameBuf;
	info->resolution = pls->frameBuf[head][plane].resolution;
	sceKernelUnlockFastMutex(&mutex);
	return 0;
}

/* TPIDRURO bit 16: called from a syscall, so the buffer is the caller's. */
static inline __attribute__((always_inline)) SceUInt32 lockRangeMode(void) /* name unknown */
{
	return (readTpidruro() & 0x10000) ? 0x30 : 3;
}

/* Wait for the next frame buffer of a process (or one vblank when it sets none). */
static inline __attribute__((always_inline)) int waitProcessFrame(DisplayProcessLocal *pls,
								  SceUInt32 head) /* name unknown */
{
	SceUInt32 timeout = 64000;
	int ret;

	ret = SceThreadmgrForDriver_C529EA32(pls->frameEvent, heads[head].eventBits, NULL, NULL, &timeout);
	if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return SceDisplayForDriver_B80CA224(head);
	return ret;
}

/* Copy the frame a process shows on a plane into fb, converted by the IFTU. */
static inline __attribute__((always_inline)) int captureFrameBufIftu(SceUID pid, SceUInt32 head,
		SceUInt32 plane, SceDisplayFrameBuf *fb) /* name unknown */
{
	DisplayProcessLocal *pls;
	DisplayFrameBufRecord *rec;
	SceIftuPlaneState src;
	SceIftuFrameBuf dst;
	SceUIntPtr base, paddr;
	SceUInt32 iftuFormat, size, mode, pitch, width, height, alignedWidth, alignedHeight;
	int ret;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	if (head > 1 || plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (fb == NULL)
		return SCE_DISPLAY_ERROR_INVALID_ADDR;
	if (fb->size != DISPLAY_FRAMEBUF_SIZE)
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	base = (SceUIntPtr)fb->base;
	if (base & 0xff)
		return SCE_DISPLAY_ERROR_INVALID_ADDR;
	switch (fb->pixelformat) {
	case 0:
		iftuFormat = 0x10;
		break;
	case 0x100000:
		iftuFormat = 0x20;
		break;
	case 0x8000:
		iftuFormat = 0x2000;
		break;
	case 0x108000:
		iftuFormat = 0x1000;
		break;
	case 0x60800000:
		iftuFormat = 0x40;
		break;
	case 0x60808000:
		iftuFormat = 0x8000;
		break;
	case 0x60900000:
		iftuFormat = 0x80;
		break;
	case 0x60908000:
		iftuFormat = 0x4000;
		break;
	default:
		return SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT;
	}
	size = fb->pitch * fb->height * 4;
	mode = lockRangeMode();
	ret = sceKernelLockRangeWithMode(mode, fb->base, size);
	if (ret < 0)
		return ret;
	if (size == 0) {
		ret = SCE_DISPLAY_ERROR_INVALID_VALUE;
		goto unlockRange;
	}
	ret = framebufPaddr(base, size, 0x20000000, &paddr);
	if (ret < 0)
		goto unlockRange;
	dst.pixelformat = iftuFormat;
	dst.width = fb->width;
	dst.height = fb->height;
	dst.leftover_stride = (fb->pitch - fb->width) * 4;
	dst.paddr0 = paddr;
	SceCpuForDriver_02796361(fb->base, size);
	ret = SceSysmemForDriver_0F5C84B7(pid, NULL);
	if (ret < 0)
		goto unlockRange;
	ret = SceProcessmgrForDriver_AF80F39C(pid, plsKey, (void **)&pls, 0);
	if (ret < 0)
		goto release;
	if (pls == NULL || pls->frameEvent <= 0) {
		ret = SCE_DISPLAY_ERROR_NO_FRAME_BUFFER;
		goto release;
	}
	ret = waitProcessFrame(pls, head);
	if (ret < 0)
		goto release;
	src.scaleX = 0;
	src.scaleY = 0;
	src.dst_x = 0;
	src.dst_y = 0;
	ret = lockFastMutex(&mutex);
	if (ret < 0)
		goto release;
	rec = &pls->frameBuf[head][plane];
	pitch = rec->frameBuf.pitch;
	paddr = rec->paddr;
	iftuFormat = rec->iftuPixelformat;
	width = rec->frameBuf.width;
	height = rec->frameBuf.height;
	if (rec->cropWidth != 0) {
		src.scaleX = rec->cropWidth + 1;
		src.scaleY = rec->cropHeight + 1;
		src.dst_x = rec->cropX;
		src.dst_y = rec->cropY;
	}
	sceKernelUnlockFastMutex(&mutex);
	if (iftuFormat == 0) {
		ret = SCE_DISPLAY_ERROR_NO_PIXEL_DATA;
		goto release;
	}
	alignedWidth = (width + 15) & ~15;
	alignedHeight = (height + 7) & ~7;
	src.fb.pixelformat = iftuFormat;
	src.fb.width = alignedWidth;
	src.fb.height = alignedHeight;
	src.vbot_padding = alignedHeight - height;
	src.hright_padding = alignedWidth - width;
	src.fb.leftover_stride = iftuFormat <= 8 ? (pitch - alignedWidth) * 2 : (pitch - alignedWidth) * 4;
	src.fb.paddr0 = paddr;
	src.sourceConfig[0] = 0;
	src.vtop_padding = 0;
	src.hleft_padding = 0;
	if (src.scaleX == 0) {
		src.scaleX = ((width << 16) + fb->width - 1) / fb->width;
		src.scaleY = ((height << 16) + fb->height - 1) / fb->height;
	} else {
		src.scaleX = ((src.scaleX + 1) * 960 - 1) / fb->width;
		src.scaleY = ((src.scaleY + 1) * 544 - 1) / fb->height;
	}
	src.sampleX = 0;
	src.sampleY = 0;
	ret = SceIftuForDriver_67E37EFC(&dst, &src, NULL);
	if (ret < 0)
		goto release;
	sceGUIDReleaseObject(pid);
	SceCpuForDriver_02796361(fb->base, size);
	SceSysmemForDriver_22CBE925(mode, fb->base, size);
	return 0;

release:
	sceGUIDReleaseObject(pid);
unlockRange:
	SceSysmemForDriver_22CBE925(mode, fb->base, size);
	return ret;
}

int SceDisplayForDriver_D4C812E5(SceUID pid, SceUInt32 head, SceUInt32 plane,
					   SceDisplayFrameBuf *fb)
{
	return captureFrameBufIftu(pid, head, plane, fb);
}

int SceDisplayForDriver_B0CED8BC(SceUID pid, SceDisplayFrameBuf *fb)
{
	return captureFrameBufIftu(pid, primaryHead, 0, fb);
}

/* Copy the frame a process shows on a plane into fb, line by line by DMA. */
static inline __attribute__((always_inline)) int captureFrameBufDmac(SceUID pid, SceUInt32 head,
		SceUInt32 plane, SceDisplayFrameBuf *fb) /* name unknown */
{
	DisplayProcessLocal *pls;
	DisplayFrameBufRecord *rec;
	SceKernelDmaOpTag tags[2];
	SceKernelDmaOpTag *tag;
	SceKernelDmaOpId op;
	SceUIntPtr base, paddr, srcPaddr;
	SceUInt32 pitch, width, height, size, mode, srcPitch, iftuFormat, pixelformat, lines, i;
	SceUInt32 srcStride, dstStride, timeout;
	int ret;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	if (head > 1 || plane > 1)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	if (fb == NULL)
		return SCE_DISPLAY_ERROR_INVALID_ADDR;
	if (fb->size != DISPLAY_FRAMEBUF_SIZE)
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	base = (SceUIntPtr)fb->base;
	if (base & 0xff)
		return SCE_DISPLAY_ERROR_INVALID_ADDR;
	pitch = fb->pitch;
	width = fb->width;
	height = fb->height;
	if ((pitch & 0x3f) != 0 || pitch < width)
		return SCE_DISPLAY_ERROR_INVALID_PITCH;
	size = height * pitch * 4;
	mode = lockRangeMode();
	ret = sceKernelLockRangeWithMode(mode, fb->base, size);
	if (ret < 0)
		return ret;
	if (size == 0) {
		ret = SCE_DISPLAY_ERROR_INVALID_VALUE;
		goto unlockRange;
	}
	ret = framebufPaddr(base, size, 0x20000000, &paddr);
	if (ret < 0)
		goto unlockRange;
	op = sceKernelDmaOpAlloc("SceDisplay");
	if (op < 0) {
		ret = op;
		goto unlockRange;
	}
	ret = sceKernelDmaOpAssign(op, SCE_KERNEL_DMAC_ID_DMAC23, 0);
	if (ret < 0)
		goto free;
	SceCpuForDriver_02796361(fb->base, size);
	ret = SceSysmemForDriver_0F5C84B7(pid, NULL);
	if (ret < 0)
		goto free;
	ret = SceProcessmgrForDriver_AF80F39C(pid, plsKey, (void **)&pls, 0);
	if (ret < 0)
		goto release;
	if (pls == NULL || pls->frameEvent <= 0) {
		ret = SCE_DISPLAY_ERROR_NO_FRAME_BUFFER;
		goto release;
	}
	timeout = 64000;
	ret = SceThreadmgrForDriver_C529EA32(pls->frameEvent, heads[head].eventBits, NULL, NULL, &timeout);
	if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		ret = SceDisplayForDriver_B80CA224(head);
	if (ret < 0)
		goto release;
	ret = lockFastMutex(&mutex);
	if (ret < 0)
		goto release;
	rec = &pls->frameBuf[head][plane];
	srcPitch = rec->frameBuf.pitch;
	iftuFormat = rec->iftuPixelformat;
	if (rec->frameBuf.width < width)
		width = rec->frameBuf.width;
	srcPaddr = rec->paddr;
	if (rec->frameBuf.height < height)
		height = rec->frameBuf.height;
	pixelformat = rec->frameBuf.pixelformat;
	sceKernelUnlockFastMutex(&mutex);
	if (iftuFormat == 0) {
		ret = SCE_DISPLAY_ERROR_NO_PIXEL_DATA;
		goto release;
	}
	fb->height = height;
	fb->pixelformat = pixelformat;
	fb->width = width;

	tags[0].src = (const void *)srcPaddr;
	tags[0].dst = (void *)paddr;
	tags[0].len = width * 4;
	tags[0].cmd = 0;
	tags[0].blockSize = 0;
	tags[0].pNext = (SceKernelDmaOpTag *)dmaTagPaddr;
	srcStride = srcPitch * 4;
	dstStride = pitch * 4;
	lines = height - 1;
	tag = dmaTag;
	if (lines < 2) {
		lines = 1;
	} else {
		for (i = 1; i != lines; i++, tag++) {
			tag->src = (const void *)(srcPaddr + i * srcStride);
			tag->dst = (void *)(paddr + i * dstStride);
			tag->len = width * 4;
			tag->blockSize = 0;
			tag->cmd = 0;
			tag->pNext = (SceKernelDmaOpTag *)(dmaTagPaddr + ((SceUIntPtr)(tag + 1) - (SceUIntPtr)dmaTag));
		}
		tag = (SceKernelDmaOpTag *)dmaTag + height - 2;
	}
	tags[1].src = (const void *)(srcPaddr + lines * srcStride);
	tags[1].dst = (void *)(paddr + lines * dstStride);
	tags[1].len = width * 4;
	tags[1].cmd = 0;
	tags[1].blockSize = 0;
	tags[1].pNext = tag - 1;
	ret = sceKernelDmaOpSetupChain(op, tags, NULL, SCE_KERNEL_DMA_OP_COMPLETE_CHAIN);
	if (ret >= 0 && (ret = sceKernelDmaOpEnQueue(op)) >= 0
	    && (ret = sceKernelDmaOpSync(op, SCE_KERNEL_DMA_OP_SYNC_WAIT, NULL, NULL)) >= 0)
		SceCpuForDriver_02796361(fb->base, size);
	sceGUIDReleaseObject(pid);
	SceSysmemForDriver_22CBE925(mode, fb->base, size);
	sceKernelDmaOpFree(op);
	return ret;

release:
	sceGUIDReleaseObject(pid);
free:
	SceSysmemForDriver_22CBE925(mode, fb->base, size);
	sceKernelDmaOpFree(op);
	return ret;

unlockRange:
	SceSysmemForDriver_22CBE925(mode, fb->base, size);
	return ret;
}

int SceDisplayForDriver_707EEE2E(SceUID pid, int head, int plane,
					   SceDisplayFrameBuf *fb)
{
	return captureFrameBufDmac(pid, head, plane, fb);
}

int SceDisplayForDriver_F116D0B4(SceUID pid, SceDisplayFrameBuf *fb)
{
	return captureFrameBufDmac(pid, primaryHead, 0, fb);
}

int SceDisplayForDriver_FA7CE579(int head, SceUID uid)
{
	int ret;

	if ((SceUInt32)head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	ret = sceKernelGetThreadmgrUIDClass(uid);
	if (ret < 0)
		return ret;
	if (ret != 8)
		return SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID;
	heads[head].frameBufCallback = uid;
	return 0;
}

int SceDisplayForDriver_6E22990E(SceUID uid)
{
	SceUInt32 head = primaryHead;
	int ret;

	if (head > 2)
		return SCE_DISPLAY_ERROR_INVALID_HEAD;
	ret = sceKernelGetThreadmgrUIDClass(uid);
	if (ret < 0)
		return ret;
	if (ret != 8)
		return SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID;
	heads[head].frameBufCallback = uid;
	return 0;
}
