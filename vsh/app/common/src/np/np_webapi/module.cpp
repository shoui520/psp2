/*
 * SceNpWebApi: the module's start and stop, and the calls that set the library up. The names are
 * ours.
 */

#include "np_webapi_internal.h"

int npWebApiNewLibContext(SceSize poolSize, void *pool, const char *name, int flags)
{
	return npWebApiCreateLibContext(poolSize, pool, name, flags);
}

/* what the module starts with: the heap, the NP common library, the global lock, the push
   library; the shell is given the allocator it starts with */
int npWebApiModuleInit(const int *shellParam)
{
	int result;
	int mspace;
	int param[2];
	const int *allocator;

	mspace = 0;
	g_sdkVersion = SceLibKernel_9F793F84();
	if (!npWebApiIsShellProcess()) {
		result = npWebApiHeapCreate();
		if (result < 0)
			return result;
		result = npWebApiHeapGet((void **)&mspace);
		if (result < 0)
			goto destroyHeap;
		result = SceNpCommonPs4_0D1288D4((void *)mspace);
		if (result < 0)
			goto destroyHeap;
	}
	allocator = SceNpCommonPs4_53484B79();
	result = npWebApiLoadSystemParam();
	if (result < 0)
		goto undoNpCommon;
	result = npWebApiGlobalLockCreate();
	if (result < 0)
		goto undoNpCommon;
	if (!npWebApiIsShellProcess()) {
		result = SceNpCommonPs4_CC43F27D();
		if (result < 0)
			goto destroyGlobalLock;
		result = npPushInit(allocator);
		if (result < 0)
			goto undoCommon;
		result = SceNpCommonPs4_D84BF4B7(1);
		if (result < 0)
			goto undoPush;
		result = npPushRegisterCallback();
		if (result < 0)
			goto undoD84;
	}
	if (npWebApiIsShellProcess()) {
		sceClibMemset(param, 0, sizeof(param));
		if (shellParam == NULL) {
			result = NP_WEBAPI_ERROR_INVALID_ARGUMENT;
			goto undoCallback;
		}
		param[0] = shellParam[0];
		param[1] = shellParam[1];
		result = SceNpCommonPs4_6602CA8B(allocator, param);
		if (result < 0)
			goto undoCallback;
	} else {
		result = SceNpCommonPs4_5492CC76(0, allocator);
		if (result < 0)
			goto undoCallback;
		if (g_sdkVersion >= NP_WEBAPI_SDK_3_50)
			SceNpCommonPs4_E2805B06();
	}
	result = SceNpCommonPs4_E7831B1A();
	if (result >= 0) {
		result = SceNpCommonPs4_AA96CC6A();
		if (result >= 0)
			return result;
		SceNpCommonPs4_A2AEF12D();
	}
	if (npWebApiIsShellProcess())
		SceNpCommonPs4_7F68D169();
	else
		SceNpCommonPs4_F2D6BA27();
undoCallback:
	if (!npWebApiIsShellProcess())
		npPushUnregisterCallback();
undoD84:
	if (!npWebApiIsShellProcess())
		SceNpCommonPs4_1096EAD4(1);
undoPush:
	if (!npWebApiIsShellProcess())
		npPushTerm();
undoCommon:
	if (!npWebApiIsShellProcess())
		SceNpCommonPs4_8CE077C7();
destroyGlobalLock:
	npWebApiGlobalLockDestroy();
undoNpCommon:
	if (!npWebApiIsShellProcess())
		SceNpCommonPs4_33B5FE83();
destroyHeap:
	if (!npWebApiIsShellProcess())
		npWebApiHeapDestroy();
	return result;
}

int npWebApiModuleTerm(void)
{
	int result;

	SceNpCommonPs4_4EA26D7A();
	SceNpCommonPs4_A2AEF12D();
	if (g_sdkVersion >= NP_WEBAPI_SDK_3_50) {
		if (!npWebApiIsShellProcess())
			SceNpCommonPs4_019C2522();
	}
	SceNpCommonPs4_F2D6BA27();
	if (!npWebApiIsShellProcess()) {
		npPushUnregisterCallback();
		SceNpCommonPs4_1096EAD4(1);
		npPushTerm();
		SceNpCommonPs4_8CE077C7();
	}
	result = npWebApiGlobalLockDestroy();
	if (result >= 0 && !npWebApiIsShellProcess()) {
		SceNpCommonPs4_33B5FE83();
		npWebApiHeapDestroy();
	}
	return result;
}

extern "C" {

int module_start(SceSize arglen, const void *argp)
{
	int result;

	result = 0;
	if (!npWebApiIsShellProcess())
		result = npWebApiModuleInit(NULL);
	if (result < 0)
		return 2;
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	int result;

	result = 0;
	if (!npWebApiIsShellProcess())
		result = npWebApiModuleTerm();
	if (result < 0)
		return 1;
	return 0;
}

int SceNpWebApi_E83CE7F1(const int *shellParam)
{
	int result;

	result = 0;
	if (npWebApiIsShellProcess())
		return npWebApiModuleInit(shellParam);
	return result;
}

int SceNpWebApi_8D83A2D0(void)
{
	int result;

	result = 0;
	if (npWebApiIsShellProcess())
		return npWebApiModuleTerm();
	return result;
}

}
