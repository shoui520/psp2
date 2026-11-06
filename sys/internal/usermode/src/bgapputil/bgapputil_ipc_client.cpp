/*
 * The shell's background application methods over SceShellSvc's IPMI connection. Sony's build
 * keeps them out of the module's file (after module_exit, not inlined); the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "bgapputil_ipc_client.h"

#define BG_APP_UTIL_METHOD_START	0x180000
#define BG_APP_UTIL_METHOD_START_RESULT	0x180001

int bgAppUtilRequestStart(IPMI::Client *client, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag)
{
	int ret;

	ret = client->invokeAsyncMethod(BG_APP_UTIL_METHOD_START, (const IPMI::DataInfo *)NULL, 0,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int bgAppUtilGetStartResult(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(BG_APP_UTIL_METHOD_START_RESULT, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
