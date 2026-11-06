/*
 * SceAppUtilExt: the application utility's calls for system applications: the PlayReady, Marlin
 * IPTV and PS Now calls (apputil_ext_api.c) and the video data drive (apputil_ext_video.c).
 *
 * Every call first checks that the caller's thread has the stack it needs and that SceAppUtil is
 * initialised.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <apputil/apputil_internal.h>
#include "apputil_ext_internal.h"

static SceUInt32 s_sdkVersion;

int module_start(SceSize args, const void *argp)
{
	s_sdkVersion = SceLibKernel_9F793F84();
	return initVideo();
}

int module_stop(SceSize args, const void *argp)
{
	return SCE_OK;
}

int module_exit(SceSize args, const void *argp)
{
	return SCE_OK;
}

void paramError(int code)
{
	sceClibPrintf("\n\n****** SceAppUtil Parameter Error : %d ******\n\n", code);
}

int checkStack(SceSize size)
{
	SceAppUtilInternalState state;
	int ret;

	if (size > sceKernelCheckThreadStack())
		return SCE_APPUTIL_ERROR_STACKSIZE_TOO_SHORT;
	ret = SceAppUtil_A3E0694F(&state);
	if (ret < 0)
		return ret;
	return SCE_OK;
}
