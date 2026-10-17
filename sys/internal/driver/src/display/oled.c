/*
 * SceDisplay: the main OLED panel, brought to the requested state at vblank.
 * File name unknown; the module links its parts in this order.
 */
#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/systimer.h>
#include <psp2kern/kernel/sysmem/uid_guid.h>
#include <psp2kern/lowio/iftu.h>
#include <display/display.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include "display_internal.h"
#include "display_imports.h"

static DisplayOledState oled;

int oledInit(void) /* name unknown */
{
	void *volatile imported;

	oled.present = 0;
	oled.suspended = 0;
	oled.wantOn = 0;
	oled.isOn = 0;
	oled.brightness = 0;
	oled.curBrightness = 0;
	oled.colorSpace = 0;
	oled.curColorSpace = 0;
	imported = (void *)ksceOledGetDDB;
	if (imported == NULL)
		return -1;
	oled.present = 1;
	oled.brightness = 0x10000;
	return 0;
}

int oledSuspend(void) /* name unknown */
{
	if (oled.present) {
		oled.suspended = 1;
		oled.isOn = -1;
		oled.curBrightness = -1;
	}
	return 0;
}

/* At vblank: bring the panel to what was asked. */
void oledVblank(void)
{
	if (!oled.present || oled.suspended)
		return;
	if (oled.isOn != oled.wantOn) {
		if (oled.wantOn == 0) {
			if (ksceOledDisplayOff() < 0)
				goto set;
		} else {
			if (ksceOledDisplayOn() < 0)
				goto set;
			oled.curBrightness = -1;
		}
		oled.isOn = oled.wantOn;
	}
set:
	if (!bootLogoShown && oled.isOn == 1) {
		if (oled.curColorSpace != oled.colorSpace
		    && ksceOledSetDisplayColorSpaceMode(oled.colorSpace) >= 0)
			oled.curColorSpace = oled.colorSpace;
		if (oled.curBrightness != oled.brightness && ksceOledSetBrightness(oled.brightness) >= 0)
			oled.curBrightness = oled.brightness;
	}
	SceOledForDriver_E30604CC();
}

int oledStart(void)
{
	int ret;

	if (!oled.present)
		return 0;
	ret = 0;
	if (SceOledForDriver_9F4ABDDC() != 0) {
		oled.suspended = 1;
		oled.isOn = 0;
		SceOledForDriver_ED2D6F19(1);
		ret = SceOledForDriver_DDB1412B();
		oled.suspended = 0;
	}
	SceOledForDriver_ED2D6F19(0);
	return ret;
}

int _sceDisplayOledSetEnable(int on) /* Sony's name (0.990 messages) */
{
	SceKernelIntrStatus intr;
	int old;

	intr = ksceKernelCpuSuspendIntr();
	old = oled.wantOn;
	oled.wantOn = on;
	ksceKernelCpuResumeIntr(intr);
	return old;
}

int oledBusy(void) /* name unknown */
{
	if (!oled.present)
		return 0;
	if (oled.isOn != oled.wantOn)
		return 1;
	return SceOledForDriver_BF5C5624(1) != 0;
}

int oledSetBrightness(SceUInt32 brightness)
{
	if (brightness == 0 || brightness == 1 || brightness <= 0x10000) {
		oled.brightness = brightness;
		return 0;
	}
	return SCE_DISPLAY_ERROR_INVALID_VALUE;
}

int oledSetColorSpaceMode(int mode)
{
	int ret;

	if (mode == 0 || mode == 1) {
		ret = 0;
		oled.colorSpace = mode;
	} else {
		ret = SCE_DISPLAY_ERROR_INVALID_VALUE;
	}
	return ret;
}
