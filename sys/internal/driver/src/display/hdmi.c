/*
 * SceDisplay: the HDMI transmitter.
 * File name unknown; the module links its parts in this order.
 */
#include <psp2kern/types.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/intrmgr.h>
#include <systimer/systimer.h>
#include <lowio/iftu.h>
#include <display/display.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include "display_internal.h"
#include "display_imports.h"

static DisplayHdmiState hdmi;

int hdmiInit(void) /* name unknown */
{
	void *volatile imported;

	imported = (void *)SceHdmiForDriver_7AD0F6C9;
	hdmi.present = 0;
	hdmi.probed = 0;
	if (imported != NULL)
		hdmi.present = 1;
	return 0;
}

/* The first use waits for the HDMI transmitter to answer. */
static inline __attribute__((always_inline)) int hdmiProbe(void) /* name unknown */
{
	SceUInt8 info[8];
	SceUInt8 count;
	int ret;

	if (!hdmi.present)
		return 0;
	if (!hdmi.probed) {
		while ((ret = SceHdmiForDriver_7AD0F6C9(info, 5, &count)) == 0x80370002)
			sceKernelDelayThread(1000);
		hdmi.probed = 1;
		if (ret < 0) {
			hdmi.present = 0;
			return 0;
		}
		if (!hdmi.present)
			return 0;
	}
	return 1;
}

int _sceDisplaySetupHdmiTx(SceUInt32 vic, SceUInt32 pixelformat, SceUInt32 pixelSize) /* Sony's name (0.990 messages) */
{
	DisplayHdmiMode mode;
	int ret;

	if (!hdmiProbe())
		return 0;
	switch (vic) {
	case 0x8300:
		vic = 0x8100;
		break;
	case 0x8100:
		vic = 0x8300;
		break;
	case 0x8280:
		vic = 0x8480;
		break;
	case 0x8480:
		vic = 0x8280;
		break;
	case 0x8170: case 0x82f0: case 0x8370: case 0x84f0: case 0x8500: case 0x8580:
	case 0x8600: case 0x8680: case 0x8710: case 0x8730: case 0x8790:
	case 0x8b00: case 0x8c00: case 0x8d00: case 0x8e00: case 0x8f00:
		break;
	default:
		vic = 0xffff;
		break;
	}
	mode.vic = vic;
	mode.reserved[0] = 0;
	mode.reserved[2] = 0;
	mode.reserved[1] = 0;
	switch (pixelformat) {
	case 0x8000:
	case 0x108000:
		mode.format[0] = 1;
		mode.format[1] = 1;
		mode.format[2] = 0;
		mode.format[3] = 1;
		break;
	case 0x60808000:
	case 0x60908000:
		mode.format[0] = 1;
		mode.format[1] = 1;
		mode.format[2] = 1;
		mode.format[3] = 1;
		break;
	case 0x60800000:
	case 0x60900000:
		mode.format[0] = 0;
		mode.format[1] = 0;
		mode.format[2] = 1;
		mode.format[3] = 1;
		break;
	case 0x100001:
	case 0x60800001:
	case 0x60900001:
		mode.format[0] = 0;
		mode.format[1] = 0;
		mode.format[2] = 1;
		mode.format[3] = 0;
		break;
	case 0:
	case 0x100000:
		mode.format[0] = 0;
		mode.format[1] = 0;
		mode.format[2] = 0;
		mode.format[3] = 1;
		break;
	case 1:
		mode.format[0] = 0;
		mode.format[1] = 0;
		mode.format[2] = 0;
		mode.format[3] = 0;
		break;
	default:
		return SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT;
	}
	if (pixelSize == 30)
		mode.format[2] = 1;
	else if (pixelSize == 36)
		mode.format[2] = 2;
	else if (pixelSize == 24)
		mode.format[2] = 0;
	else
		return SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT;
	ret = SceHdmiForDriver_5A212BB3(&mode);
	return ret < 0 ? ret : 0;
}

int _sceDisplaySetHdmiTxMute(int mute) /* Sony's name (0.990 messages) */
{
	int ret;

	if (!hdmiProbe())
		return 0;
	ret = SceHdmiForDriver_08AFFB03(mute != 0);
	if (ret < 0)
		return ret;
	ret = SceHdmiForDriver_99A3CABE(mute != 0);
	return ret < 0 ? ret : 0;
}
