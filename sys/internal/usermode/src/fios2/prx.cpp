/*
 * prx.cpp: the module's start and stop, and sceFiosVprintf, where every message of the
 * library goes.
 */
#include "fios2_internal.h"

extern "C" {

int module_start(SceSize args, const void *argp)
{
	fiosLibraryInit();
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, const void *argp)
{
	if (sceFiosIsInitialized(NULL))
		sceFiosTerminate();
	fiosLibraryTerm();
	return SCE_KERNEL_STOP_SUCCESS;
}

int module_exit(SceSize args, const void *argp)
{
	if (sceFiosIsInitialized(NULL))
		sceFiosTerminate();
	fiosLibraryTerm();
	return SCE_KERNEL_STOP_SUCCESS;
}

int sceFiosVprintf(const char *pFormat, va_list ap)
{
	char buf[512];
	int ret;

	if (g_FiosParams.pVprintf != NULL) {
		ret = g_FiosParams.pVprintf(pFormat, ap);
	} else {
		ret = fiosVsnprintf(buf, sizeof(buf), pFormat, ap);
		sceClibPrintf("%s", buf);
	}
	return ret;
}

} /* extern "C" */
