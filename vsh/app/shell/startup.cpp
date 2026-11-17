/*
 * The shell's startup settings: read once at start, before the framework is made. The start
 * waits for a standby wake-up to finish, reads whether the initial setup has been done, the
 * power management mode, the IDU (demo unit) mode and the home screen settings kept in the id
 * storage, then makes the services the framework needs and turns the display on. The file's
 * and every name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <thread/thread.h>
#include <system/system.h>
#include <kernel/processmgr.h>
#include <appmgr/appmgr.h>
#include <av_config/av_config.h>
#include <power/power.h>
#include <post_ss_mgr/sblpmmgr.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include <graphics/drawobj/effect_path.h>

#include "shell.h"

using namespace paf;

/* the id storage leaf of the home screen settings, and its flags */
#define SHELL_STARTUP_IDS_LEAF		278
#define SHELL_STARTUP_IDS_PAGE_COLOR	0x01	/* pageColor holds the pages' colour (pagec) */
#define SHELL_STARTUP_IDS_ICWVP		0x02
#define SHELL_STARTUP_IDS_PRESET_THEME	0x04	/* pd0 has a theme */
#define SHELL_STARTUP_MAX_PAGE_COLOR	31

/* the IDU mode: off, on, and left (the demo ended) */
#define SHELL_IDU_MODE_OFF		0
#define SHELL_IDU_MODE_ON		1
#define SHELL_IDU_MODE_LEFT		2

/* /CONFIG/SPECIFIC idu_mode's values (registry) */
#define SHELL_REG_IDU_MODE_OFF		1
#define SHELL_REG_IDU_MODE_ON		2

/* the power switch modes and the time the standby button is held (us) */
#define SHELL_POWER_SW_MODE_IDU		1
#define SHELL_POWER_SW_MODE_DEFAULT	4
#define SHELL_STANDBY_PUSH_TIME		2000000
#define SHELL_STANDBY_PUSH_TIME_IDU	10000000
#define SHELL_STANDBY_PUSH_TIME_TV	5000000

/* the wake-up factor's cause bits, and the cause of a wake-up to go to standby again */
#define SHELL_WAKEUP_FACTOR_MASK	0x7F
#define SHELL_WAKEUP_FACTOR_STANDBY	1

/* the shell event sent when the IDU mode is left (value only) */
#define SHELL_EVENT_IDU_MODE_LEFT	8

/* the power management mode of a unit under power management (value only) */
#define SHELL_PM_MODE_1			1

/* paf's model type of the PlayStation TV (value only) */
#define SHELL_MODEL_TYPE_1		1

/* the id storage leaf's home screen settings: its flags and the pages' colour (unaligned) */
typedef struct ShellStartupIdStorage {
	SceUInt8 flags;
	SceUInt8 pageColor[2];
	SceUInt8 reserved;
} __attribute__((aligned(4))) ShellStartupIdStorage;

static int s_initialize = 1;
static bool s_isPmMode = false;
static int s_iduMode = SHELL_IDU_MODE_OFF;
static int s_pageColor = 0;
static bool s_icwvp = false;
static bool s_hasPresetTheme = false;

int shellShutterSoundInit(void)
{
	return shellShutterSoundStart();
}

int shellStartupGetInitialize(void)
{
	return s_initialize;
}

bool shellStartupNeedsInitialSetup(void)
{
	return (s_initialize & 1) == 0;
}

bool shellStartupIsPmMode(void)
{
	return s_isPmMode;
}

int shellStartupGetIduMode(void)
{
	return s_iduMode;
}

int shellStartupGetPageColor(void)
{
	return s_pageColor;
}

bool shellStartupGetIcwvp(void)
{
	return s_icwvp;
}

/* no test mode in this build */
bool shellStartupIsTestMode(void)
{
	return false;
}

bool shellStartupHasPresetTheme(void)
{
	return s_hasPresetTheme;
}

/* the IDU mode's settings: in it, the display at its brightest, no auto suspend and no power
   saving, and the standby button held longer */
static void shellStartupApplyIduMode(void)
{
	int regIduMode = SHELL_REG_IDU_MODE_OFF;
	int pushTime = SHELL_STANDBY_PUSH_TIME;
	int swMode = SHELL_POWER_SW_MODE_DEFAULT;

	if (s_iduMode == SHELL_IDU_MODE_ON) {
		unsigned int maxBrightness;
		int latency;

		if (sceAVConfigGetDisplayMaxBrightness(&maxBrightness) == 0)
			sceAVConfigSetDisplayBrightness(maxBrightness);
		sceAVConfigSetAutoSuspend(0);
		if (sceRegMgrGetKeyInt("/CONFIG/SPECIFIC", "idu_video_latency", &latency) == 0)
			sceAVConfigSetAutoSuspend2(latency);
		sceRegMgrSetKeyInt("/CONFIG/SHELL", "notify_ps_button", 0);
		sceRegMgrSetKeyInt("/CONFIG/POWER_SAVING", "suspend_interval", 0);
		sceRegMgrSetKeyInt("/CONFIG/DISPLAY", "brightness", maxBrightness);
		pushTime = SHELL_STANDBY_PUSH_TIME_IDU;
		swMode = SHELL_POWER_SW_MODE_IDU;
		regIduMode = SHELL_REG_IDU_MODE_ON;
	} else {
		sceAVConfigClearAutoSuspend2();
	}
	sceRegMgrSetKeyInt("/CONFIG/SPECIFIC", "idu_mode", regIduMode);
	vshPowerSetPowerSwMode(swMode);
	vshPowerSetStandbyButtonPushTime(pushTime);
}

/* leaves the IDU mode: its settings undone and the shell's listeners told */
bool shellStartupLeaveIduMode(void)
{
	if (s_iduMode != SHELL_IDU_MODE_ON)
		return false;
	s_iduMode = SHELL_IDU_MODE_LEFT;
	shellStartupApplyIduMode();
	shellEventServerSend(SHELL_EVENT_IDU_MODE_LEFT, NULL, 0);
	return true;
}

int shellStartupInit(void)
{
	int ret = 0;
	int err;
	ShellStartupIdStorage ids;
	SceUInt8 pmMode;

	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	thread::Sleep(33);
	vshPowerSetPowerSwMode(SHELL_POWER_SW_MODE_DEFAULT);
	shellRtcSetAlarmTick(NULL, 1);
	if ((vshPowerGetWakeupFactor() & SHELL_WAKEUP_FACTOR_MASK) == SHELL_WAKEUP_FACTOR_STANDBY) {
		scePowerRequestStandby();
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		for (;;)
			thread::Sleep(100);
	}

	err = sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "initialize", &s_initialize);
	if (err != 0)
		ret = err;
	err = sceSblPmMgrGetCurrentMode(&pmMode);
	if (err == 0 && pmMode == SHELL_PM_MODE_1)
		s_isPmMode = true;
	if (err != 0)
		ret = err;
	if (system::Model::GetType() == SHELL_MODEL_TYPE_1)
		vshPowerSetStandbyButtonPushTime(SHELL_STANDBY_PUSH_TIME_TV);
	if (vshSysconIsIduMode() == 1) {
		s_iduMode = SHELL_IDU_MODE_ON;
		shellStartupApplyIduMode();
	}
	err = shellRtcUtilSync(false);
	if (err != 0)
		ret = err;

	if ((int)vshIdStorageLookup(SHELL_STARTUP_IDS_LEAF, 0, &ids, sizeof(ids)) >= 0 &&
	    ids.flags != 0) {
		SceUInt8 flags = ids.flags;
		int pageColor = ids.pageColor[0] | (ids.pageColor[1] << 8);

		if ((flags & SHELL_STARTUP_IDS_PAGE_COLOR) && pageColor <= SHELL_STARTUP_MAX_PAGE_COLOR)
			s_pageColor = pageColor;
		if (flags & SHELL_STARTUP_IDS_ICWVP)
			s_icwvp = true;
		if (flags & SHELL_STARTUP_IDS_PRESET_THEME)
			s_hasPresetTheme = true;
	}

	shellErrorDialogInit();
	shellAppEventInit();
	shellNetworkMgrCreate(!s_isPmMode);
	sceAppMgrDeclareShellProcess2("NPXS19999");
	sceAppMgrSetDisplayMergeConf(128, 256);
	shellAccessibilityApply();
	sceAVConfigDisplayOn();
	sceAppMgrRestoreDisplaySettingForShell();
	return ret;
}
