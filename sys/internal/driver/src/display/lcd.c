/*
 * SceDisplay: the main LCD panel, brought to the requested state at vblank.
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

static DisplayLcdState lcd;

int lcdInit(void) /* name unknown */
{
	void *volatile imported;

	lcd.present = 0;
	lcd.suspended = 0;
	lcd.wantOn = 0;
	lcd.isOn = 0;
	lcd.brightness = 0;
	lcd.curBrightness = 0;
	lcd.mode = 0;
	lcd.colorSpace = 0;
	lcd.curColorSpace = 0;
	imported = (void *)SceLcdForDriver_E03E120B;
	if (imported == NULL)
		return -1;
	lcd.present = 1;
	lcd.brightness = 0x10000;
	return 0;
}

int lcdSuspend(void) /* name unknown */
{
	if (lcd.present) {
		lcd.suspended = 1;
		lcd.isOn = -1;
		lcd.curBrightness = -1;
	}
	return 0;
}

void lcdVblank(void)
{
	int ret;

	if (!lcd.present || lcd.suspended)
		return;
	if (lcd.isOn != lcd.wantOn) {
		if (lcd.wantOn == 0)
			ret = SceLcdForDriver_1A0A7519();
		else
			ret = SceLcdForDriver_5F4124AB();
		if (ret >= 0)
			lcd.isOn = lcd.wantOn;
	}
	if (!bootLogoShown && lcd.isOn != 0) {
		if (lcd.curColorSpace != lcd.colorSpace
		    && SceLcdForDriver_D40968FB(lcd.colorSpace) >= 0)
			lcd.curColorSpace = lcd.colorSpace;
		if (lcd.curBrightness != lcd.brightness && SceLcdForDriver_581D3A87(lcd.brightness) >= 0)
			lcd.curBrightness = lcd.brightness;
	}
	SceLcdForDriver_59CCDC97();
}

int lcdStart(void)
{
	if (!lcd.present)
		return 0;
	if (SceLcdForDriver_6A08623C() != 0) {
		lcd.suspended = 1;
		lcd.isOn = 0;
		SceLcdForDriver_4CC1E8C1(1);
		SceLcdForDriver_3E7C2CCA();
		lcd.suspended = 0;
	}
	SceLcdForDriver_4CC1E8C1(0);
	return 0;
}

int lcdSetPower(int on) /* name unknown */
{
	SceKernelIntrStatus intr;
	int old;

	intr = sceKernelCpuSuspendIntr();
	old = lcd.wantOn;
	lcd.wantOn = on;
	sceKernelCpuResumeIntr(intr);
	return old;
}

int lcdBusy(void) /* name unknown */
{
	if (!lcd.present)
		return 0;
	if (lcd.isOn != lcd.wantOn)
		return 1;
	return SceLcdForDriver_4735059C(1) != 0;
}

int lcdSetBrightness(SceUInt32 brightness)
{
	if (brightness == 0 || brightness == 1 || brightness <= 0x10000) {
		lcd.brightness = brightness;
		return 0;
	}
	return SCE_DISPLAY_ERROR_INVALID_VALUE;
}

int lcdSetMode(int mode)
{
	if (mode == 0 || mode == 1) {
		SceLcdForDriver_1D73D7F3(mode);
		lcd.mode = mode;
		return 0;
	}
	return SCE_DISPLAY_ERROR_INVALID_VALUE;
}

int lcdSetColorSpaceMode(int mode)
{
	int colorSpace;

	if ((SceUInt32)mode < 2)
		colorSpace = 0;
	else if (mode == 2)
		colorSpace = 1;
	else
		return SCE_DISPLAY_ERROR_INVALID_VALUE;
	lcd.colorSpace = colorSpace;
	return 0;
}
