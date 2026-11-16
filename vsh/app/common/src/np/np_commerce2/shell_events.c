/*
 * SceNpCommerce2: the shell's events the library listens to. Init registers a callback for
 * each of 24 event IDs (two callbacks, one for the even IDs and one for the odd ones), Term
 * removes them again.
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_SHELL_EVENT_NUM	12

int g_shellReady = 0;				/* 0x81026090 */
int g_shellLockReady = 0;			/* 0x81026094 */

static const int s_enableEvents[NP_COMMERCE2_SHELL_EVENT_NUM] = {	/* 0x81026030 */
	0x18, 0x1A, 0x1C, 0x1E, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E,
};

static const int s_disableEvents[NP_COMMERCE2_SHELL_EVENT_NUM] = {	/* 0x81026060 */
	0x19, 0x1B, 0x1D, 0x1F, 0x21, 0x23, 0x25, 0x27, 0x29, 0x2B, 0x2D, 0x2F,
};

SceKernelLwMutexWork g_shellLock;		/* 0x810270B8 */

int npCommerce2_81017DCC(void)
{
	int ret;
	SceUInt32 i;

	if (g_shellReady != 0)
		return SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED;

	g_shellLockReady = 0;
	sceClibMemset(g_shellSlots, 0, sizeof(g_shellSlots));
	ret = sceKernelCreateLwMutex(&g_shellLock, "SceNpCommerce2", 2, 0, NULL);
	if (ret >= 0) {
		g_shellLockReady = 1;
		for (i = 0; i < NP_COMMERCE2_SHELL_EVENT_NUM; i++) {
			ret = SceShellSvc_D943CE15(s_enableEvents[i], npCommerce2ShellEnabled, 0);
			if (ret < 0)
				goto fail;
		}
		for (i = 0; i < NP_COMMERCE2_SHELL_EVENT_NUM; i++) {
			ret = SceShellSvc_D943CE15(s_disableEvents[i], npCommerce2ShellDisabled, 0);
			if (ret < 0)
				goto fail;
		}
		g_shellReady = 1;
		return 0;
	}
fail:
	for (i = 0; i < NP_COMMERCE2_SHELL_EVENT_NUM; i += 2) {
		SceShellSvc_7CBF442B(s_disableEvents[i]);
		SceShellSvc_7CBF442B(s_disableEvents[i + 1]);
	}
	for (i = 0; i < NP_COMMERCE2_SHELL_EVENT_NUM; i += 2) {
		SceShellSvc_7CBF442B(s_enableEvents[i]);
		SceShellSvc_7CBF442B(s_enableEvents[i + 1]);
	}
	if (g_shellLockReady == 1) {
		sceKernelDeleteLwMutex(&g_shellLock);
		g_shellLockReady = 0;
	}
	return ret;
}

void npCommerce2_81017EF8(int a1)
{
	SceUInt32 i;

	if (g_shellReady == 0)
		return;
	for (i = 0; i < NP_COMMERCE2_SHELL_EVENT_NUM; i += 2) {
		SceShellSvc_7CBF442B(s_disableEvents[i]);
		SceShellSvc_7CBF442B(s_disableEvents[i + 1]);
	}
	for (i = 0; i < NP_COMMERCE2_SHELL_EVENT_NUM; i += 2) {
		SceShellSvc_7CBF442B(s_enableEvents[i]);
		SceShellSvc_7CBF442B(s_enableEvents[i + 1]);
	}
	if (g_shellLockReady == 1) {
		sceKernelDeleteLwMutex(&g_shellLock);
		g_shellLockReady = 0;
	}
	g_shellReady = 0;
}
