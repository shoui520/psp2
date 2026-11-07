/*
 * The common dialog's own API (sceCommonDialog*): the calls the manager
 * makes for every dialog. The file's name is ours.
 */
#include <scetypes.h>
#include <common_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

extern "C" int sceCommonDialogSetConfigParam(const CommonDialogConfig *configParam)
{
	return SetConfigParam(configParam);
}

extern "C" int sceCommonDialogUpdate(const SceCommonDialogUpdateParam *updateParam)
{
	return Update(updateParam);
}

extern "C" SceBool sceCommonDialogIsRunning(void)
{
	return IsRunning();
}

extern "C" SceUID sceCommonDialogGetWorkerThreadId(void)
{
	return GetWorkerThreadId();
}
