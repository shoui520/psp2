/*
 * The product mode helpers pm_mgr.c calls (file name unknown): whether the
 * syscon keeps the mode for pm_sm, and the mode as booted or as pm_sm keeps it.
 */

#include "post_ss_mgr_internal.h"

#define PM_SM_PATH		"os0:sm/pm_sm.self"

#define SYSCON_STATUS_NO_SM_MODE	0x40	/* name unknown */

static int g_pmNoSmMode;		/* name unknown */
static int g_pmNoSmModeKnown;		/* name unknown */

/* whether the syscon cannot keep the mode for pm_sm (cached) */
int pmMgrIsNoSmMode(void)	/* name unknown */
{
	int status, noSmMode;

	if (g_pmNoSmModeKnown)
		return g_pmNoSmMode;
	status = SceSysconForDriver_EBE3262C();
	if (status >= 0) {
		noSmMode = status & SYSCON_STATUS_NO_SM_MODE;
		if (noSmMode == 0) {
			g_pmNoSmMode = noSmMode;
			g_pmNoSmModeKnown = 1;
			return noSmMode;
		}
	}
	g_pmNoSmMode = 1;
	g_pmNoSmModeKnown = 1;
	return 1;
}

int pmMgrGetProductMode(SceUInt8 *mode)	/* name unknown */
{
	if (mode == NULL)
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	*mode = g_pmProductMode;
	return 0;
}

/* the mode pm_sm keeps */
int pmMgrGetSmProductMode(SceUInt8 *mode)	/* name unknown */
{
	SceUInt32 state = 0;
	SceUInt32 data;
	int ret, stop;

	SceKernelSuspendForDriver_4DF40893(0);
	ret = invoke_pm_secure_module(PM_SM_PATH);
	if (ret != 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	ret = pmSmGetMode(&state, &data);
	if (ret == 0)
		*mode = state;
	stop = stop_pm_secure_module();
	SceKernelSuspendForDriver_2BB92967(0);
	return stop != 0 ? stop : ret;
}
