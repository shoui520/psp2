/*
 * SceIncomingDialog (vs0:sys/external/incoming_dialog.suprx): an application's incoming call
 * or invitation dialog, shown by the shell (SceShellSvc's IPMI service, its
 * SceIncomingDialogServer). No Sony record names its files or internal functions; the file's
 * name is the module's.
 */
#include <stddef.h>
#include <scetypes.h>
#include "../../../../../vsh/lowlayer/src/shell_svc/shell_svc_client.h"
#include "incoming_dialog.h"
#include "incoming_dialog_ipc_client.h"

extern "C" int sceIncomingDialogInitialize(int initType)
{
	if (initType != 0)
		return SCE_INCOMING_DIALOG_ERROR_INVALID_ARGUMENT;
	return incomingDialogInitialize(SceShellSvc_B31E7F1C());
}

extern "C" int sceIncomingDialogFinish(void)
{
	return incomingDialogFinish(SceShellSvc_B31E7F1C());
}

extern "C" int sceIncomingDialogOpen(const SceIncomingDialogParam *pParam)
{
	const char *text;

	if (pParam == NULL)
		return SCE_INCOMING_DIALOG_ERROR_INVALID_ARGUMENT;
	text = "";
	if (pParam->sdkVersion >= INCOMING_DIALOG_TEXT_SDK_VERSION)
		text = pParam->text;
	return incomingDialogOpen(SceShellSvc_B31E7F1C(), pParam, text);
}

extern "C" int sceIncomingDialogClose(void)
{
	return incomingDialogClose(SceShellSvc_B31E7F1C());
}

extern "C" int sceIncomingDialogGetStatus(void)
{
	int status = 0;

	incomingDialogGetStatus(SceShellSvc_B31E7F1C(), &status);
	return status;
}

extern "C" int sceIncomingDialogSwitchToDialog(void)
{
	return incomingDialogSwitchToDialog(SceShellSvc_B31E7F1C());
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
