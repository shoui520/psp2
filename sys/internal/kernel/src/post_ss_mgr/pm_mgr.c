/*
 * pm_mgr.c: SceSblPmMgr, the product mode. Turning it on or off runs
 * pm_sm.self in the secure processor; newer syscons keep the mode there,
 * older ones in the NVS.
 */

#include <sysmem/sblaimgr.h>
#include <kernel_boot_loader/kbl_param.h>
#include <syscon/syscon.h>
#include <sysmem/sysmem.h>
#include <intrmgr/syscall.h>
#include "post_ss_mgr_internal.h"
#include <post_ss_mgr/sblpmmgr.h>

#define PM_SM_SD_PATH		"sd0:sm/pm_sm_sd.self"

#define PM_MODE_ON		4	/* names unknown */
#define PM_MODE_OFF		5
#define PM_MODE_OFF_NVS		7

static SceUInt32 g_pmBaryonVersion;	/* name unknown */
static SceUInt32 g_pmSysconRevision;	/* name unknown */
SceUInt8 g_pmProductMode;		/* name unknown */
static SceUInt8 g_pmInitialized;	/* name unknown */

static inline SceUInt32 pmMgrGetBaryonVersion(void)	/* name unknown */
{
	if (g_pmBaryonVersion == 0)
		g_pmBaryonVersion = SceSysconForDriver_FF86F4C5();
	return g_pmBaryonVersion;
}

static inline SceUInt32 pmMgrGetSysconRevision(void)	/* name unknown */
{
	if (g_pmSysconRevision == 0)
		SceSysconForDriver_D2F456DC((uint8_t *)&g_pmSysconRevision);
	return g_pmSysconRevision;
}

/* whether pm_sm keeps the mode, rather than the NVS */
static inline int pmMgrHasSmMode(void)	/* name unknown */
{
	SceUInt32 baryon = pmMgrGetBaryonVersion();
	SceUInt32 revision = pmMgrGetSysconRevision();

	if (pmMgrIsNoSmMode() && (revision & 0xffffff) >= 0x3600)
		return 1;
	if (!pmMgrIsNoSmMode() && baryon > 0x90902)
		return 1;
	return 0;
}

int sceSblPmMgrGetProductModeFromNVS(SceUInt8 *mode)
{
	if (mode == NULL)
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	if (pmMgrHasSmMode())
		return pmMgrGetSmProductMode(mode);
	return SCE_SBL_ERROR_PM_UNSUPPORTED;
}

int sceSblPmMgrSetProductModeCommon(int mode, int useSm)
{
	SceUInt32 state, data[0x20 / 4];
	int ret, stop;

	if (g_pmProductMode == 1) {
		if (mode == PM_MODE_ON)
			return 0;
		SceKernelSuspendForDriver_4DF40893(0);
		stop = invoke_pm_secure_module(PM_SM_PATH);
		if (stop != 0) {
			SceKernelSuspendForDriver_2BB92967(0);
			return stop;
		}
	} else {
		if (g_pmProductMode == 0 && mode == PM_MODE_OFF)
			return 0;
		SceKernelSuspendForDriver_4DF40893(0);
		stop = invoke_pm_secure_module(PM_SM_PATH);
		if (stop != 0) {
			SceKernelSuspendForDriver_2BB92967(0);
			return stop;
		}
		if (mode == PM_MODE_ON) {
			ret = pm_auth_common(0xe, PM_SM_PATH);
			if (ret != 0) {
				SCE_PRINTF_WITH_INFO(0, 402, "STEP 8657(0x%08x)\n", ret);
				stop = stop_pm_secure_module();
				SceKernelSuspendForDriver_2BB92967(0);
				return stop != 0 ? stop : ret;
			}
			ret = pm_auth2_common(mode);
			if (ret != 0) {
				SCE_PRINTF_WITH_INFO(0, 417, "STEP 0d76(0x%08x)\n", ret);
				stop = stop_pm_secure_module();
				SceKernelSuspendForDriver_2BB92967(0);
				return stop != 0 ? stop : ret;
			}
		}
	}
	if (useSm) {
		state = 0;
		data[0] = 0;
		ret = pmSmGetMode(&state, data);
		if (ret == 0)
			ret = pmSmSetMode(&state, data, mode);
	} else {
		/* the NVS keeps the mode in its first 0x20 bytes */
		ret = nvs_read_data(0, data, sizeof(data));
		if (ret == 0) {
			ret = pmMgrSetNvsMode(mode, data);
			if (ret == 0)
				ret = nvs_write_data(0, data, sizeof(data));
		}
	}
	stop = stop_pm_secure_module();
	SceKernelSuspendForDriver_2BB92967(0);
	return stop != 0 ? stop : ret;
}

int sceSblPmMgrSetProductMode(int on)
{
	int mode;

	if (on & 1)
		mode = PM_MODE_ON;
	else
		mode = PM_MODE_OFF;

	if (pmMgrHasSmMode())
		return sceSblPmMgrSetProductModeCommon(mode, 1);
	return sceSblPmMgrSetProductModeCommon(mode, 0);
}

int SceSblPostSsMgrForDriver_FE92A318(SceUInt32 flags)
{
	if (flags != 0)
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	if (pmMgrHasSmMode())
		return sceSblPmMgrSetProductModeCommon(PM_MODE_OFF_NVS, 1);
	return sceSblPmMgrSetProductModeCommon(PM_MODE_OFF_NVS, 0);
}

int sceSblPmMgrAuthEtoI(void)
{
	int ret, stop;

	SceKernelSuspendForDriver_4DF40893(0);
	stop = invoke_pm_secure_module(PM_SM_SD_PATH);
	if (stop != 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return stop;
	}
	ret = pm_auth2_common(0xc);
	if (ret != 0)
		SCE_DBG_PRINTF(0, 505, "PM auth etoi: error (%d)\n", ret);
	stop = stop_pm_secure_module();
	SceKernelSuspendForDriver_2BB92967(0);
	return stop != 0 ? stop : ret;
}

/* the buttons held at boot (a 3-byte code) that turn the product mode on */
static inline int pmMgrIsProductModeKeyHeld(SceUInt32 wakeup, const SceUInt8 *key)	/* name unknown */
{
	if (wakeup == 0x20 && SceSblAIMgrForDriver_C6E83F34())
		return 1;
	if (!SceSblAIMgrForDriver_37A79140() && !SceSblAIMgrForDriver_FF5784B9() &&
	    !SceSblAIMgrForDriver_BB9D146B() && !SceSblAIMgrForDriver_E5E47FF7() &&
	    !SceSblAIMgrForDriver_05F79D4A()) {
		if (key[0] != 0x3f || key[1] != 0xab)
			return 0;
	} else if (key[0] != 0x36 || key[1] != 0xaf) {
		return 0;
	}
	return key[2] == 0xff;
}

int sceSblPmMgrInit(void)
{
	SceKblParam *kbl;
	SceUInt32 wakeup, hold;
	int ret;

	kbl = sceKernelSysrootGetKblParam();
	if (kbl == NULL) {
		g_pmInitialized = 1;
		return 0;
	}
	wakeup = kbl->wakeup_factor & 0x7f;
	g_pmProductMode = (kbl->boot_type_indicator_1 & BOOT_TYPE_PRODUCT_MODE) != 0;
	g_pmInitialized = 1;
	if (wakeup == 0x20 || wakeup == 0xb) {
		hold = kbl->hold_ctrl;
		if (pmMgrIsProductModeKeyHeld(wakeup, (const SceUInt8 *)&hold) &&
		    sceSblPmMgrSetProductMode(1) == 0) {
			SCE_PRINTF_WITH_INFO(0, 582, "SET A68A.\n");
			ret = sceSysconSetPowerMode(0, 0);
			if (ret != 0)
				SCE_DBG_PRINTF(0, 586, "sceSysconSetPowerMode() failure = 0x%x\n", ret);
			for (;;)
				;
		}
	}
	return 0;
}

int sceSblPmMgrSetProductModeOffForUser(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	ret = sceSblPmMgrSetProductMode(0);
	if (ret == 0)
		ret = SceSblPostSsMgrForDriver_FE92A318(0);
	sceSyscallLeave(state);
	return ret;
}

int sceSblPmMgrGetProductModeForUser(SceUInt8 *mode)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt8 value = 0;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (mode == NULL) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	value = g_pmProductMode;
	ret = sceKernelCopyToUser(mode, &value, sizeof(value));
	sceSyscallLeave(state);
	return ret;
}

int sceSblPmMgrGetProductModeFromNVSForUser(SceUInt8 *mode)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt8 value = 0;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (mode == NULL) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	ret = sceSblPmMgrGetProductModeFromNVS(&value);
	if (ret == 0)
		ret = sceKernelCopyToUser(mode, &value, sizeof(value));
	sceSyscallLeave(state);
	return ret;
}

int sceSblPmMgrGetCurrentMode(SceUInt8 *mode)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt8 value = 0;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (mode == NULL) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	value = g_pmProductMode;
	ret = sceKernelCopyToUser(mode, &value, sizeof(value));
	sceSyscallLeave(state);
	return ret;
}

int sceSblPmMgrAuthEtoIForUser(void)	/* name unknown */
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	ret = sceSblPmMgrAuthEtoI();
	sceSyscallLeave(state);
	return ret;
}
