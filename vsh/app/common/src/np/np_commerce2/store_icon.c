/*
 * SceNpCommerce2: the PS Store icon the shell shows over a title (SceShellSvc).
 */

#include "np_commerce2_internal.h"

int sceNpCommerce2ShowPsStoreIcon(int iconDisp)
{
	int shell;

	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	shell = SceShellSvc_B31E7F1C();
	return npCommerce2_8101EB8E(shell, iconDisp);
}

int sceNpCommerce2HidePsStoreIcon(void)
{
	int shell;

	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	shell = SceShellSvc_B31E7F1C();
	return npCommerce2_8101EBDE(shell);
}
