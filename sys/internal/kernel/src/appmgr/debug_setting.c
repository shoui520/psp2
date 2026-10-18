/* Registry-backed application-manager debug settings. */

#include "debug_setting.h"
#include <regmgr/regmgr.h>
#include <intrmgr/syscall.h>
#include <psp2kern/kernel/threadmgr.h>

extern SceUID g_appmgrAppMutex;

int g_appmgrTrcCheckNotify = -1;
int g_appmgrDebugUpgradable = -1;
int g_appmgrFakeNoMemoryCard = -1;
int g_appmgrShowDebugInfo = -1;
int g_appmgrButtonAssign = 1;
void *g_appmgrDebugSettingContext;
AppmgrDebugSettingCallback g_appmgrDebugSettingCallback;

int appmgrInitializeDebugSettings(void)
{
	int value = 0;
	int showDebugInfo = 0;
	int result;

	if (ksceRegMgrSystemParamGetInt(0x00146e23, &value) >= 0)
		showDebugInfo = value;
	g_appmgrShowDebugInfo = showDebugInfo;
	result = sceRegMgrGetKeyInt("/CONFIG/SHELL", "trc_check_notify", &value);
	g_appmgrTrcCheckNotify = result >= 0 ? value : 0;
	result = ksceRegMgrSystemParamGetInt(0x00229142, &value);
	if (result < 0) {
		g_appmgrButtonAssign = 1;
		return 0;
	}
	if (value == 0)
		g_appmgrButtonAssign = 0;
	else
		g_appmgrButtonAssign = 1;
	return result;
}

int appmgrGetButtonAssign(void)
{
	return g_appmgrButtonAssign;
}

int appmgrInitializeFakeNoMemoryCard(void)
{
	int value = 0;
	int result = 0;

	if (g_appmgrFakeNoMemoryCard < 0) {
		result = sceRegMgrGetKeyInt("/CONFIG/GAME", "fake_no_memory_card", &value);
		if (result >= 0) {
			g_appmgrFakeNoMemoryCard = value;
			result = 0;
		}
	}
	return result;
}

int appmgrInitializeDebugUpgradable(void)
{
	int value = 0;
	int result = 0;

	if (g_appmgrDebugUpgradable < 0) {
		result = sceRegMgrGetKeyInt("/CONFIG/NP", "debug_upgradable", &value);
		if (result >= 0) {
			g_appmgrDebugUpgradable = value;
			result = 0;
		}
	}
	return result;
}

int appmgrGetFakeNoMemoryCard(void)
{
	int value = g_appmgrFakeNoMemoryCard;

	if (value == 1)
		return value;
	return value == 2 ? 2 : 0;
}

int appmgrGetDebugUpgradable(void)
{
	int value = g_appmgrDebugUpgradable;

	return value < 0 ? 0 : value;
}

int ksceAppMgrDebugSettingNotifyUpdate(int setting)
{
	SceUInt32 value = 0;
	SceUInt32 syscallState = sceSyscallEnter();
	int result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);

	if (result >= 0) {
		switch (setting) {
		case 0:
			if (sceRegMgrGetKeyInt("/CONFIG/GAME", "fake_no_memory_card",
					      (int *)&value) == 0 && value <= 2)
				g_appmgrFakeNoMemoryCard = value;
			if (g_appmgrDebugSettingCallback)
				g_appmgrDebugSettingCallback(0, g_appmgrFakeNoMemoryCard,
							g_appmgrDebugSettingContext);
			result = 0;
			break;
		case 1:
			if (sceRegMgrGetKeyInt("/CONFIG/NP", "debug_upgradable",
					      (int *)&value) == 0)
				g_appmgrDebugUpgradable = value;
			if (g_appmgrDebugSettingCallback)
				g_appmgrDebugSettingCallback(1, g_appmgrDebugUpgradable,
							g_appmgrDebugSettingContext);
			result = 0;
			break;
		case 2:
			if (ksceRegMgrSystemParamGetInt(0x00229142, (int *)&value) < 0 || value != 0)
				g_appmgrButtonAssign = 1;
			else
				g_appmgrButtonAssign = 0;
			result = 0;
			break;
		case 3:
			if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "trc_check_notify",
					      (int *)&value) == 0)
				g_appmgrTrcCheckNotify = value;
			if (g_appmgrDebugSettingCallback)
				g_appmgrDebugSettingCallback(3, g_appmgrTrcCheckNotify,
							g_appmgrDebugSettingContext);
			result = 0;
			break;
		case 4:
			if (ksceRegMgrSystemParamGetInt(0x00146e23, (int *)&value) == 0)
				g_appmgrShowDebugInfo = value;
			if (g_appmgrDebugSettingCallback)
				g_appmgrDebugSettingCallback(4, g_appmgrShowDebugInfo,
							g_appmgrDebugSettingContext);
			result = 0;
			break;
		default:
			result = (int)0x80800001;
			break;
		}
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	}
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrSetDebugSettingCallback(AppmgrDebugSettingCallback callback, void *context)
{
	g_appmgrDebugSettingCallback = callback;
	g_appmgrDebugSettingContext = context;
	return 0;
}

int appmgrGetFakeSdSlotBroken(void)
{
	SceUInt32 value = 0;
	int result = ksceRegMgrSystemParamGetInt(0x00424500, (int *)&value);

	if (result < 0)
		return result;
	if (value > 256)
		return (int)0x80800044;
	return value - 1;
}

int appmgrIsShowDebugInfoEnabled(void)
{
	return g_appmgrShowDebugInfo > 0;
}

int appmgrIsTrcCheckNotifyEnabled(void)
{
	return g_appmgrTrcCheckNotify > 0;
}
