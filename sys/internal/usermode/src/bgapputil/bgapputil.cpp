/*
 * SceBgAppUtil (vs0:sys/external/bgapputil.suprx): starts the caller's background application
 * through the shell (SceShellSvc's IPMI service). No Sony record names its files or internal
 * functions; the file's name is the module's.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "../../../../../vsh/lowlayer/src/shell_svc/shell_svc_client.h"
#include "bgapputil_ipc_client.h"

/* the only flags value: none (no SDK header names the error) */
#define SCE_BG_APP_UTIL_ERROR_INVALID_ARGUMENT	-2146409215	/* 0x80106501 */

/* the ShellSvc event the shell sets when the background application has started */
#define BG_APP_UTIL_SHELL_EVENT	5

#define BG_APP_UTIL_ASSERT(result) \
	do { \
		if ((result) != 0) { \
			sceClibPrintf("[SceBgAppUtil] ASSERTION FAILURE (0x%x)\n", (result)); \
			__breakpoint(0); \
		} \
	} while (0)

extern "C" int sceBgAppUtilStartBgApp(SceUInt32 flags)
{
	int requestId;
	IPMI::EventFlagInfo eventFlag;
	int ret;

	if (flags != 0)
		return SCE_BG_APP_UTIL_ERROR_INVALID_ARGUMENT;
	ret = SceShellSvc_A0B067AC(&eventFlag, BG_APP_UTIL_SHELL_EVENT);
	BG_APP_UTIL_ASSERT(ret);
	ret = bgAppUtilRequestStart(SceShellSvc_B31E7F1C(), &requestId, &eventFlag);
	if (ret != 0)
		return ret;
	ret = SceShellSvc_8D4F9F2E(BG_APP_UTIL_SHELL_EVENT, NULL);
	BG_APP_UTIL_ASSERT(ret);
	return bgAppUtilGetStartResult(SceShellSvc_B31E7F1C(), requestId);
}

extern "C" int module_start(SceSize args, const void *argp)
{
	return 0;
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	return 0;
}

extern "C" int module_exit(SceSize args, const void *argp)
{
	return 0;
}
