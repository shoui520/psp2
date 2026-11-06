/*
 * The shell's incoming dialog methods over SceShellSvc's IPMI connection, each a synchronous
 * call that returns the call's error or the method's result. Sony's build keeps them out of the
 * module's file (after module_exit, not inlined); the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "incoming_dialog_ipc_client.h"

#define INCOMING_DIALOG_METHOD_INITIALIZE	0x170000
#define INCOMING_DIALOG_METHOD_FINISH		0x170001
#define INCOMING_DIALOG_METHOD_OPEN		0x170002
#define INCOMING_DIALOG_METHOD_CLOSE		0x170003
#define INCOMING_DIALOG_METHOD_GET_STATUS	0x170004
#define INCOMING_DIALOG_METHOD_SWITCH_TO_DIALOG	0x170005

int incomingDialogInitialize(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(INCOMING_DIALOG_METHOD_INITIALIZE, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int incomingDialogFinish(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(INCOMING_DIALOG_METHOD_FINISH, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int incomingDialogOpen(IPMI::Client *client, const SceIncomingDialogParam *pParam,
		       const char *text)
{
	IPMI::DataInfo in[2];
	int result = 0;
	SceSize length = 0;
	const char *s;
	int ret;

	in[0].pData = pParam;
	in[0].dataSize = sizeof(*pParam);
	in[1].pData = text;
	for (s = text; *s != '\0'; s++)
		length++;
	in[1].dataSize = length + 1;
	ret = client->invokeSyncMethod(INCOMING_DIALOG_METHOD_OPEN, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int incomingDialogClose(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(INCOMING_DIALOG_METHOD_CLOSE, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int incomingDialogGetStatus(IPMI::Client *client, int *pStatus)
{
	IPMI::BufferInfo out;
	int result = 0;
	int ret;

	out.pBuffer = pStatus;
	out.bufferSize = sizeof(*pStatus);
	ret = client->invokeSyncMethod(INCOMING_DIALOG_METHOD_GET_STATUS, NULL, 0, &result, &out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int incomingDialogSwitchToDialog(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(INCOMING_DIALOG_METHOD_SWITCH_TO_DIALOG, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
