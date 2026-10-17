/* SceBbmc's reference-counted use of GPIO port 30. */
#include <stddef.h>
#include <sysmem/cpu.h>
#include <psp2kern/lowio/gpio.h>

static int g_bbmcGpioUsers;
static SceKernelSpinlock g_bbmcGpioLock;

/* name unknown: initialize the lock and configure the module's GPIO port. */
int bbmcInitializeGpio(void)
{
	int ret;

	g_bbmcGpioLock = 0;
	ret = ksceGpioSetPortMode(0, 30, 0);
	if (ret < 0)
		return ret;
	ksceGpioPortClear(0, 30);
	return 0;
}

/* name unknown: GPIO owner finalization is an explicit no-op. */
int bbmcFinalizeGpio(void)
{
	return 0;
}

/* name unknown: no-op before the manager completes its state transition. */
int bbmcGpioTransitionHook(void)
{
	return 0;
}

/* name unknown: reconfigure and clear the port without resetting the lock. */
int bbmcResetGpio(void)
{
	int ret = ksceGpioSetPortMode(0, 30, 0);

	if (ret < 0)
		return ret;
	ksceGpioPortClear(0, 30);
	return 0;
}

/* name unknown: report the old count and assert the port for its first user. */
int bbmcAcquireGpio(int *previous)
{
	SceCpuForDriver_BF82DEB2(&g_bbmcGpioLock);
	if (g_bbmcGpioUsers == 0)
		ksceGpioPortSet(0, 30);
	if (previous != NULL)
		*previous = g_bbmcGpioUsers;
	g_bbmcGpioUsers++;
	sceKernelSpinlockLowUnlock(&g_bbmcGpioLock);
	return 0;
}

/* name unknown: clear the port only when its final user releases it. */
int bbmcReleaseGpio(int *previous)
{
	int count;

	SceCpuForDriver_BF82DEB2(&g_bbmcGpioLock);
	count = g_bbmcGpioUsers;
	if (previous != NULL)
		*previous = count;
	if (count != 0) {
		g_bbmcGpioUsers = count - 1;
		if (count == 1)
			ksceGpioPortClear(0, 30);
	}
	sceKernelSpinlockLowUnlock(&g_bbmcGpioLock);
	return 0;
}

/* name unknown: return the user count while holding the GPIO owner lock. */
int bbmcGetGpioUsers(void)
{
	int count;

	SceCpuForDriver_BF82DEB2(&g_bbmcGpioLock);
	count = g_bbmcGpioUsers;
	sceKernelSpinlockLowUnlock(&g_bbmcGpioLock);
	return count;
}
