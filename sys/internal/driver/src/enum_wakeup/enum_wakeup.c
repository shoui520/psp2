/*
 * SceEnumWakeUp: see enum_wakeup.h. The power button, the power-off thread,
 * the icon screen and module_start.
 */
#include "ew_internal.h"
#include <kernel/error.h>
#include <syscon/syscon_scratchpad.h>

EwGlobals ewg;

/* ===== power off ===== */

/* SceCtrlForDriver power button callback */
void ewPowerButton(uint32_t state, uint32_t previous, void *argument)
{
	int intr;

	(void)previous;
	(void)argument;
	intr = SceCpuForDriver_D32ACE9E(&ewg.ew.spinlock);
	ewg.ew.powerButton = state;
	if (state & EW_BUTTON_POWER) {
		sceKernelSetEventFlag(ewg.ew.evf, EW_EVENT_POWER_PRESSED);
		sceKernelClearEventFlag(ewg.ew.evf, ~EW_EVENT_POWER_RELEASED);
	} else {
		sceKernelSetEventFlag(ewg.ew.evf, EW_EVENT_POWER_RELEASED);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ewg.ew.spinlock, intr);
}

/* turn the system off (standby) or on (cold reset) */
int ewPowerOffThread(SceSize args, void *argp)
{
	SceUInt32 bits, timeout;
	int command = -1;
	uint32_t state;
	int intr, ret;

	(void)args;
	(void)argp;

	ewg.ew.spinlock = 0;
	SceSysconForDriver_E26488B9(SYSCON_SCRATCH_PAD(resumeContextPA), &command, sizeof(command));
	intr = SceCpuForDriver_D32ACE9E(&ewg.ew.spinlock);
	ewg.ew.powerMask = EW_BUTTON_POWER;
	ewg.ew.powerCallback = SceCtrlForDriver_FF9D4DD4(EW_BUTTON_POWER, ewPowerButton, 0, &state);
	if (state & EW_BUTTON_POWER)
		sceKernelSetEventFlag(ewg.ew.evf, EW_EVENT_POWER_PRESSED);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ewg.ew.spinlock, intr);

	for (;;) {
		sceKernelWaitEventFlag(ewg.ew.evf, EW_EVENT_POWER_PRESSED | EW_EVENT_UNPLUGGED
					| EW_EVENT_TIMER | EW_EVENT_DISPLAY_TIMER | EW_EVENT_CHARGED
					| EW_EVENT_SHUTDOWN,
					SCE_KERNEL_EVF_WAITMODE_OR |
					SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (bits & EW_EVENT_UNPLUGGED) {
			ScePowerForDriver_E4E3C316(5);
			goto standby;
		}
		if (bits & (EW_EVENT_TIMER | EW_EVENT_DISPLAY_TIMER | EW_EVENT_SHUTDOWN))
			goto standby;
		if (bits & EW_EVENT_CHARGED)
			break;
		/* held for 1.5 s: power on */
		timeout = 1500000;
		ret = sceKernelWaitEventFlag(ewg.ew.evf, EW_EVENT_POWER_RELEASED,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits,
					      &timeout);
		if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			break;
	}
	if (ewg.ew.mode != EW_MODE_DISPLAY)
		SceClockgenForDriver_5130682D();
	scePowerCancelRequest();
	scePowerRequestColdReset();
	SceKernelSuspendForDriver_2BB92967(0);
	return 0;

standby:
	if (ewg.ew.mode != EW_MODE_DISPLAY)
		SceClockgenForDriver_5130682D();
	scePowerCancelRequest();
	scePowerRequestStandby();
	SceKernelSuspendForDriver_2BB92967(0);
	return 0;
}

/* the alarm: give up waiting */
int ewTimer(void)
{
	sceKernelSetEventFlag(ewg.ew.evf, ewg.ew.mode == EW_MODE_DISPLAY ? EW_EVENT_DISPLAY_TIMER : EW_EVENT_TIMER);
	return 0;
}

/* ===== drawing ===== */







/* the never-taken brightness hooks when no display driver is present */
int ewNoDisplay(void)
{
	return 0;
}

void ewInitDisplay(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceDisplayFrameBuf fb;
	char text[0x100];
	const SceUInt8 *p = ewIconPpm;
	int i, j, ret;

	ret = SceDisplayForDriver_AF5EE5BE(0, 0, 0);
	if (ret < 0)
		goto fail;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x200000;		/* physically contiguous */
	ewg.ew.fbBlock = SceSysmemForDriver_C94850C9("FrameBuffer", EW_MEMTYPE_FB, 2 * EW_FB_SIZE, &opt);
	if (ewg.ew.fbBlock < 0)
		goto fail;
	SceSysmemForDriver_A841EDDA(ewg.ew.fbBlock, &ewg.ew.fb[0]);
	ewDrawSetTarget(ewg.ew.fb[0], EW_SCREEN_WIDTH, EW_SCREEN_HEIGHT, 0);
	ewDrawClear(0xff000000);
	ewg.ew.fb[1] = (char *)ewg.ew.fb[0] + EW_FB_SIZE;
	ewDrawSetTarget(ewg.ew.fb[1], EW_SCREEN_WIDTH, EW_SCREEN_HEIGHT, 0);
	ewDrawClear(0xff000000);
	SceCpuForDriver_103872A5(ewg.ew.fb[0], EW_FB_SIZE);
	fb.size = 0x18;
	fb.base = ewg.ew.fb[0];
	fb.pitch = EW_SCREEN_WIDTH;
	fb.pixelformat = 0;
	fb.width = EW_SCREEN_WIDTH;
	fb.height = EW_SCREEN_HEIGHT;
	SceDisplayForDriver_16466675(0, 0, &fb, 1);
	ewConsoleInit();

	ewg.ew.iconBlock = SceSysmemForDriver_C94850C9("TextureBuffer", EW_MEMTYPE_FB, 0x17f000, NULL);
	if (ewg.ew.iconBlock < 0)
		sceKernelExitThread(0);
	SceSysmemForDriver_A841EDDA(ewg.ew.iconBlock, &ewg.ew.icon);

	/* the icon is a PPM: "P6\n# comment\n90 70\n255\n" then RGB */
	i = 3;
	if (p[3] == '#')
		while (p[i++] != '\n')
			;
	for (j = 0; p[i + j] != ' '; j++)
		text[j] = p[i + j];
	text[j] = '\0';
	ewg.ew.iconWidth = 90;
	i += j + 1;
	for (j = 0; p[i] != '\n'; i++, j++)
		text[j] = p[i];
	text[j] = '\0';
	ewg.ew.iconHeight = 70;
	__builtin_memcpy(text, &p[i], 4);		/* "\n255": the maximum */
	memcpy(ewg.ew.icon, &p[i + 5], 90 * 70 * 3);

	ewg.ew.blank = 0;
	if (ksceOledSetBrightness) {
		ewg.ew.haveBrightness = 1;
		ewg.ew.setBrightness = ksceOledSetBrightness;
		ewg.ew.displayOn = SceOledForDriver_3C990F53;
	} else if (ksceLcdSetBrightness) {
		ewg.ew.haveBrightness = 1;
		ewg.ew.setBrightness = ksceLcdSetBrightness;
		ewg.ew.displayOn = SceLcdForDriver_8C965153;
	} else {
		ewg.ew.haveBrightness = 1;
		ewg.ew.setBrightness = (int (*)(int))ewNoDisplay;
		ewg.ew.displayOn = ewNoDisplay;
	}
	ewg.ew.brightness = 0x10000;
	ewg.ew.setBrightness(ewg.ew.brightness);
	return;

fail:
	sceKernelExitThread(0);
}

/* ===== main ===== */


/* the lowest clocks while the screen is up */
static const int ewClocks[4] = { 41, 55, 0, 83 };

/* collect the buttons of the buffered samples; returns those newly pressed */
static inline __attribute__((always_inline)) SceUInt32 ewReadButtons(void)
{
	SceUInt32 pressed = 0;
	int i, n;

	n = SceCtrlForDriver_9B96A1AA(0, ewg.ctrl, 8);
	ewg.buttons.pressed = 0;
	ewg.buttons.released = 0;
	for (i = 0; i < n; i++) {
		ewg.buttons.previous = ewg.buttons.current;
		ewg.buttons.current = ewg.ctrl[i].buttons;
		pressed |= ~ewg.buttons.previous & ewg.buttons.current;
		ewg.buttons.pressed = pressed;
	}
	return pressed;
}

static inline void ewSetBrightness(int brightness)
{
	ewg.ew.brightness = brightness;
	ewg.ew.setBrightness(brightness);
}

int ewMainThread(SceSize args, void *argp)
{
	SceDisplayFrameBuf frame;
	SceUInt32 held = 0, pressed, start = 0, deadline = 312500;
	SceUInt32 *fb;
	const SceUInt8 *rgb;
	int chargeEnable = 1, factor, charger, led = 0, count = 0;
	int x0, y0, i;

	(void)args;
	(void)argp;

	SceLedForDriver_EA24BE03(2, 0, NULL);
	sceRegMgrGetKeyInt("/CONFIG/USB", "usb_charge_enable", &chargeEnable);
	SceUdcdForDriver_2D91F8C3(chargeEnable);
	ScePowerForDriver_0CD21B1F(0);
	if (SceQafMgrForDriver_52B4E164() != 0) {
		ewReadButtons();
		held = ewg.buttons.current;
	}

	if (SceSysconForDriver_CF5B2F2F(&factor) < 0)
		factor = 0xf;
	if (held & EW_BUTTON_UP) {
		ewg.ew.mode = EW_MODE_MTP_DISPLAY;
		factor = 0xf;
	} else if (held & 0x200) {
		factor = 0xf;
		ewg.ew.mode = EW_MODE_MTP;
	} else {
		ewg.ew.mode = EW_MODE_MTP;
		if ((factor & 0x7f) != 0xf)	/* not woken by USB: just show the icon */
			goto display_only;
	}
	if (held & EW_BUTTON_PS) {
display_only:
		ewg.ew.mode = EW_MODE_DISPLAY;
	} else {
		mtpInit();
		mtpStart(1, ewFillDeviceStrings);
		if (ewg.ew.mode == EW_MODE_MTP) {
			SceDisplayForDriver_43347565(0);
			SceDisplayForDriver_43347565(1);
			goto clocks;
		}
	}
	ewInitDisplay();
	SceDisplayForDriver_43347565(1);
	if (SceDisplayForDriver_984C27E7() < 0)
		sceKernelExitThread(0);

clocks:
	if (ewg.ew.mode != EW_MODE_DISPLAY) {
		/* run slow and switch off what is not needed */
		__builtin_memcpy(ewg.ew.clocks, ewClocks, sizeof(ewg.ew.clocks));
		scePowerSetArmClockFrequency(ewg.ew.clocks[0]);
		scePowerSetBusClockFrequency(ewg.ew.clocks[1]);
		ScePowerForDriver_264C24FC(ewg.ew.clocks[2], ewg.ew.clocks[2]);
		scePowerSetGpuXbarClockFrequency(ewg.ew.clocks[3]);
		ScePervasiveForDriver_4F64DEF6(0);
		if (SceModulemgrForDriver_BBE1771C("SceDbgUsb") < 0)
			ScePervasiveForDriver_4F64DEF6(1);
		ScePervasiveForDriver_E0711D42();
		SceSysconForDriver_3FDD29D6(1, 0);
		SceHpremoteForDriver_7735C403();
		SceClockgenForDriver_F0A0242D();
		ksceClockgenWlanBtClkDisable();
		SceClockgenForDriver_82F39378();
		ScePowerForDriver_C743E392(0);
		SceGpioForDriver_372022A4(0, 10, 0);
		SceGpioForDriver_D454A584(0, 10);
		SceGpioForDriver_372022A4(0, 11, 0);
		SceGpioForDriver_D454A584(0, 11);
		for (i = 0; i < 3; i++) {
			ScePervasiveForDriver_821AE6C7(i);
			ScePervasiveForDriver_F861FEE4(i);
		}
		ScePervasiveForDriver_2FB5F88F(0);
		ScePervasiveForDriver_B8AF1249();
		ScePervasiveForDriver_51981FEE();
		ScePervasiveForDriver_146108D4(7);
		ScePervasiveForDriver_A3CF9844(7);
		ScePervasiveForDriver_146108D4(1);
		ScePervasiveForDriver_A3CF9844(1);
		ScePervasiveForDriver_91C80C41(1, 0);
		ScePervasiveForDriver_3D8A8712(2);
		ScePervasiveForDriver_4E43299D(2);
		ewg.ew.motionFlags = 0;
		ewg.ew.connected = 1;
		ewg.ew.charged = 0;
	} else {
		ewg.ew.connected = 1;
		ewg.ew.charged = 0;
		ewg.ew.motionFlags = 0;
	}
	if (ewg.ew.mode == EW_MODE_DISPLAY)
		ewg.ew.timer = SceThreadmgrForDriver_C58DF384("SceEnumWakeupAlarm", 8000000, ewTimer, NULL);

	for (;;) {
		if (ewg.ew.mode == EW_MODE_MTP) {
			sceKernelDelayThread(1000000);
		} else {
			SceKernelSuspendForDriver_E0489831(0);
			ewg.ew.setBrightness(ewg.ew.brightness);
			if (SceDisplayForDriver_46F186C3() < 0)
				sceKernelExitThread(0);
			count++;
			fb = ewg.ew.fb[count & 1];
			ewDrawSetTarget(fb, EW_SCREEN_WIDTH, EW_SCREEN_HEIGHT, 0);
			if (ewg.ew.blank) {
				/* leave the frame as it is */
			} else if (ewg.ew.mode == EW_MODE_DISPLAY) {
				/* the icon blinks: 0.19 s on of every 0.31 s */
				rgb = ewg.ew.icon;
				if (deadline < SceThreadmgrForDriver_47F6DE49()) {
					start = SceThreadmgrForDriver_47F6DE49();
					deadline = start + 312500;
				}
				if (ewg.ew.mode == EW_MODE_DISPLAY
				    && SceThreadmgrForDriver_47F6DE49() < start + 187500) {
					x0 = 480 - ewg.ew.iconWidth / 2;
					y0 = 272 - ewg.ew.iconHeight / 2;
					for (i = 0; i < ewg.ew.iconWidth * ewg.ew.iconHeight; i++, rgb += 3)
						ewDrawPixel(0xff000000 | rgb[0] | rgb[1] << 8 | rgb[2] << 16,
							    x0 + i % ewg.ew.iconWidth, y0 + i / ewg.ew.iconWidth);
				} else {
					ewDrawClear(0xff000000);
				}
			} else {
				ewDrawClear(0xff000000);
			}
			SceCpuForDriver_103872A5(fb, EW_FB_SIZE);
			frame.size = 0x18;
			frame.base = fb;
			frame.pitch = EW_SCREEN_WIDTH;
			frame.pixelformat = 0;
			frame.width = EW_SCREEN_WIDTH;
			frame.height = EW_SCREEN_HEIGHT;
			SceDisplayForDriver_16466675(0, 0, &frame, 1);
			if (ewg.ew.mode == EW_MODE_DISPLAY)
				continue;
		}

		{
			{
				pressed = ewReadButtons();
				if (SceQafMgrForDriver_52B4E164() != 0) {
					/* QA: Select blanks the screen, Start shuts down,
					   the d-pad sets the brightness */
					if (pressed & EW_BUTTON_SELECT) {
						led ^= 1;
						SceLedForDriver_EA24BE03(2, led, NULL);
						if (ewg.ew.mode == EW_MODE_MTP)
							goto motion;
						if (ewg.ew.displayOn() || !ewg.ew.blank)
							ewg.ew.blank = ~ewg.ew.blank;
					} else if (ewg.ew.mode == EW_MODE_MTP) {
						goto motion;
					}
					if (pressed & EW_BUTTON_START)
						sceKernelSetEventFlag(ewg.ew.evf, EW_EVENT_SHUTDOWN);
					if (ewg.ew.displayOn()) {
						if ((pressed & EW_BUTTON_UP) && ewg.ew.brightness < 0x100)
							ewSetBrightness(ewg.ew.brightness + 1);
						if ((pressed & EW_BUTTON_DOWN) && ewg.ew.brightness > 1)
							ewSetBrightness(ewg.ew.brightness - 1);
						if (pressed & EW_BUTTON_RIGHT)
							ewSetBrightness(ewg.ew.brightness <= 0xf0
									? ewg.ew.brightness + 0x10 : 0x100);
						if (pressed & EW_BUTTON_LEFT)
							ewSetBrightness(ewg.ew.brightness > 0x10
									? ewg.ew.brightness - 0x10 : 1);
					}
				}
				if (ewg.ew.mode != EW_MODE_DISPLAY) {
motion:
					if (ewg.ew.motionFlags != 1 && !(ewg.ew.motionFlags & 1)
					    && ksceMotionDevIsReady()) {
						SceMotionDevForDriver_031AF7A1();
						ewg.ew.motionFlags |= 1;
					}
				}
			}

			/* charging state */
			SceSysconForDriver_A2FE9BF9(&charger);
			if ((ksceUdcdGetDeviceState() & 2) && (charger & 0xf00) != 0x200) {
				if (!ewg.ew.connected) {
					if (ewg.ew.timer > 0) {
						sceKernelCancelAlarm(ewg.ew.timer);
						ewg.ew.timer = -1;
					}
					ewg.ew.connected = 1;
				}
				if ((charger & 0x12) == 0x10 && ewg.ew.charged != 1) {
					ewg.ew.charged = 1;
					sceKernelSetEventFlag(ewg.ew.evf, EW_EVENT_CHARGED);
				}
			} else if (ewg.ew.connected == 1) {
				/* unplugged: power off after a minute */
				ewg.ew.connected = 0;
				ewg.ew.timer = SceThreadmgrForDriver_C58DF384("SceEnumWakeupAlarm", 60000000,
								   ewTimer, NULL);
			}
		}
	}
	return 0;
}

int module_start(SceSize args, void *argp)
{
	SceUID main, powerOff;

	ewg.ew.evf = sceKernelCreateEventFlag("SceEnumWakeupPowerOff", 0x9000, 0, NULL);
	if (ewg.ew.evf < 0)
		return SCE_KERNEL_START_FAILED;
	main = sceKernelCreateThread("EnumWakeupMain", ewMainThread, 0x10000102, 0x20000, 0,
				      0x80000, NULL);
	if (main >= 0) {
		if (sceKernelStartThread(main, args, argp) >= 0) {
			powerOff = sceKernelCreateThread("EnumWakeupPowerOff", ewPowerOffThread,
							  0x10000102, 0x20000, 0, 0x80000, NULL);
			if (powerOff >= 0) {
				if (sceKernelStartThread(powerOff, args, argp) >= 0)
					return SCE_KERNEL_START_SUCCESS;
				sceKernelDeleteThread(powerOff);
			}
		}
		sceKernelDeleteThread(main);
	}
	sceKernelDeleteEventFlag(ewg.ew.evf);
	return SCE_KERNEL_START_FAILED;
}

/* provider of the device strings */
int ewFillDeviceStrings(MtpProvidedStrings *strings, int reserved)
{
	SceUInt32 version;
	int major, minor, retries = 20;

	(void)reserved;

	strncpy(strings->manufacturer, "Sony Interactive Entertainment Inc.",
		strnlen("Sony Interactive Entertainment Inc.", 0xff));
	strncpy(strings->model, "PlayStation(R)Vita", strnlen("PlayStation(R)Vita", 0xff));
	do {
		version = SceSysrootForDriver_67AAB627();
		if (version != 0xffffffff) {
			if (version == 0)
				goto names;
			/* BCD: 0x03650000 is 3.65 */
			minor = (version << 12 >> 28) + (version << 8 >> 28) * 10;
			major = (version << 4 >> 28) + (version >> 28) * 10;
			goto format;
		}
		sceKernelDelayThread(200000);
	} while (--retries);
	minor = 0xa5;
	major = 0xa5;
format:
	snprintf(ewg.version, 10, "%d.%02d", major, minor);
	mtpStrcpy(strings->deviceVersion, ewg.version);
names:
	strncpy(strings->friendlyName, "PlayStation(R)Vita", strnlen("PlayStation(R)Vita", 0xff));
	strncpy(strings->iconPath, "vs0:vsh/mtpresponder/DevIcon.fil",
		strnlen("vs0:vsh/mtpresponder/DevIcon.fil", 0xff));
	return version;
}
