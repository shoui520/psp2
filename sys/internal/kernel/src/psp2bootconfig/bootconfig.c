/*
 * ScePsp2BootConfig: the first kernel module the NSKBL starts.
 *
 * module_start loads the core kernel modules through the boot loader;
 * module_bootstart then starts them, in two groups around the boot loader's
 * own process/thread manager hand-over. Development kits also bring up the
 * debug channel (SDBG over SDIO, deci4p) right after the core group.
 */

#include <fw.h>
#include <stddef.h>
#include <kernel_boot_loader/kbl.h>
#include <sysmem/sysclib.h>

#define BOOTPARAM_FUNC0		0x2fc
#define BOOTPARAM_FUNC1		0x300
#define BOOTPARAM_VALUE0	0x304
#define BOOTPARAM_VALUE1	0x308

#define PANEL_LCD		0x9

static const char *const s_coreModules[] = {
	"sysmem.skprx",
	"excpmgr.skprx",
	"intrmgr.skprx",
	"buserror.skprx",
	"systimer.skprx",
	"acmgr.skprx",
	"threadmgr.skprx",
	"dmacmgr.skprx",
	"smsc_proxy.skprx",
	"authmgr.skprx",
	"processmgr.skprx",
	"iofilemgr.skprx",
	"modulemgr.skprx",
};

#if FW_DEVKIT
static const char *const s_debugModules[] = {
	"sdbgsdio.skprx",
	"deci4p_sdfmgr.skprx",
	"deci4p_sttyp.skprx",
};
static const char *const s_sdbgpModule[] = { "deci4p_sdbgp.skprx" };
static const char *const s_sdrfpModule[] = { "deci4p_sdrfp.skprx" };
#endif

/* slots 2 and 3 are filled in at boot */
static const char *const s_deviceModules[] = {
	"lowio.skprx",
	"syscon.skprx",
	NULL,			/* lcd.skprx or oled.skprx */
	NULL,			/* hdmi.skprx */
	"display.skprx",
	"sm_comm.skprx",
	"ss_mgr.skprx",
	"sdif.skprx",
	"msif.skprx",
	"gcauthmgr.skprx",
	"sdstor.skprx",
	"rtc.skprx",
	"exfatfs.skprx",
#if FW_DEVKIT
	"bsod.skprx",
#endif
	"sysstatemgr.skprx",
};

static SceUID s_deviceUid[ARRAY_SIZE(s_deviceModules)];
#if FW_DEVKIT
static SceUID s_sdbgpUid[1];
static SceUID s_sdrfpUid[1];
#endif
static SceUInt8 s_hardwareFlags[16];
#if FW_DEVKIT
static SceUID s_debugUid[ARRAY_SIZE(s_debugModules)];
#endif
static const char *s_device[ARRAY_SIZE(s_deviceModules)];
static SceUID s_coreUid[ARRAY_SIZE(s_coreModules)];

#if FW_DEVKIT
/* the debug channel needs dipsw 0xc1 and none of the three boot modes */
static inline __attribute__((always_inline)) int DebugChannelEnabled(void)
{
	return SceKblForKernel_sceKernelCheckDipsw(0xc1) &&
	       !sceSblAimgrIsCEX() &&
	       !SceKblForKernel_5945F065() &&
	       !SceKblForKernel_C3DDDE15();
}
#endif

int module_start(SceSize args, void *argp)
{
	SceKblForKernel_6D7A1F18(s_coreModules, s_coreUid, ARRAY_SIZE(s_coreModules), 0);

#if FW_DEVKIT
	if (DebugChannelEnabled()) {
		int flags = SceKblForKernel_sceKernelCheckDipsw(0xd2) != 0;

		SceKblForKernel_6D7A1F18(s_debugModules, s_debugUid,
					 ARRAY_SIZE(s_debugModules), flags);
		if (sceSblQafManagerIsAllowKernelDebug()) {
			SceKblForKernel_6D7A1F18(s_sdbgpModule, s_sdbgpUid, 1, flags);
			SceKblForKernel_6D7A1F18(s_sdrfpModule, s_sdrfpUid, 1, flags);
		}
	}
#endif

	__builtin_memcpy(s_device, s_deviceModules, sizeof(s_device));

	if (SceKblForKernel_B6D00D6D() == 0) {
		SceKblForKernel_D3A516D5(s_hardwareFlags);
		s_device[2] = (s_hardwareFlags[0] & PANEL_LCD) ? "lcd.skprx" : "oled.skprx";
	} else {
		s_device[2] = NULL;
	}

	if (SceKblForKernel_838466E9() == 0 ||
	    (sceSblAimgrIsCEX() == 0 && SceKblForKernel_5945F065() == 0))
		s_device[3] = "hdmi.skprx";

	SceKblForKernel_6D7A1F18(s_device, s_deviceUid, ARRAY_SIZE(s_device), 0);
	SceKblForKernel_261F2747();
	SceKblForKernel_79241ACF();
	return 0;
}

int module_bootstart(SceSize args, void *argp)
{
	char *param = argp;

	SceKblForKernel_DDB3A1A8(args);
	*(void **)(param + BOOTPARAM_FUNC0) = SceKblForKernel_08E9FAEB;
	*(void **)(param + BOOTPARAM_FUNC1) = SceKblForKernel_13A5ABEF;
	*(SceUInt32 *)(param + BOOTPARAM_VALUE0) = SceKblForKernel_9B868276();
	*(SceUInt32 *)(param + BOOTPARAM_VALUE1) = SceKblForKernel_C011935A();

	sceKernelBootBootModules(s_coreUid, ARRAY_SIZE(s_coreUid), 4, argp);

#if FW_DEVKIT
	if (DebugChannelEnabled()) {
		sceKernelBootBootModules(s_debugUid, ARRAY_SIZE(s_debugUid), 0, NULL);
		if (sceSblQafManagerIsAllowKernelDebug())
			sceKernelBootBootModules(s_sdbgpUid, 1, 0, NULL);
	}
#endif

	SceKblForKernel_sceKernelSysrootThreadMgrStartAfterProcess();
	SceKblForKernel_9F4F3F98();
	SceKblForKernel_sceKernelSysrootProcessmgrStart2();
	SceKblForKernel_sceKernelSysrootIofilemgrStart();

#if FW_DEVKIT
	if (sceSblQafManagerIsAllowKernelDebug() && DebugChannelEnabled())
		sceKernelBootBootModules(s_sdrfpUid, 1, 0, NULL);
#endif

	sceKernelBootBootModules(s_deviceUid, ARRAY_SIZE(s_deviceUid), 4, argp);
	return 0;
}
