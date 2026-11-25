/*
 * SceCamImportDialogPlugin: SceXuiShellUtil power-tick and ShellUtil event helpers.
 */
#include <scetypes.h>
#include <thread/thread.h>
#include "xui_shell_util.h"

extern "C" {
typedef void (*SceShellUtilEventHandler)(int result, SceMode mode, void *userData);
SceInt32 sceKernelPowerTick(SceUInt32 type);
SceInt32 sceShellUtilInitEvents(int initMode);
SceInt32 sceShellUtilRegisterEventHandler(SceShellUtilEventHandler handler, void *userData);
SceInt32 SceShellUtil_5C5373B9(void);
}

using namespace paf::thread;

static Mutex s_shellUtilMutex("SceXuiShellUtilMutex", 0);
static Cond s_shellUtilLockCond(s_shellUtilMutex, "SceXuiShellUtilLockCond", 0);
static Cond s_shellUtilUnlockCond(s_shellUtilMutex, "SceXuiShellUtilUnlockCond", 0);

static void ShellUtilEventCallback(int result, SceMode mode, void *userData)
{
	(void)result;
	if ((int)userData != 1)
		return;
	if (mode == 1) {
		s_shellUtilLockCond.NotifyAll();
	} else if (mode == 2 || mode == 3) {
		s_shellUtilUnlockCond.NotifyAll();
	}
}

int XuiPowerTick(int flags)
{
	SceUInt32 tickType = (flags & 1) != 0 ? 1U : 0U;

	if ((flags & 6) != 0)
		tickType |= 6U;
	if ((flags & 4) != 0)
		tickType |= 4U;
	if (flags == 0)
		tickType = 0U;
	return sceKernelPowerTick(tickType);
}

int XuiShellUtilInit(void)
{
	int ret = sceShellUtilInitEvents(0);

	if (ret != 0)
		return ret;
	ret = sceShellUtilRegisterEventHandler(ShellUtilEventCallback, NULL);
	if (ret != 0)
		return ret;
	return 0;
}

int XuiShellUtilTerm(void)
{
	sceShellUtilRegisterEventHandler(NULL, NULL);
	return SceShellUtil_5C5373B9();
}
