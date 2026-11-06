/* the shell's incoming dialog methods (incoming_dialog_ipc_client.cpp; the names are ours) */
#ifndef INCOMING_DIALOG_INCOMING_DIALOG_IPC_CLIENT_H
#define INCOMING_DIALOG_INCOMING_DIALOG_IPC_CLIENT_H

#include <scetypes.h>
#include "../../../../../vsh/lowlayer/src/ipmi/client.h"
#include "incoming_dialog.h"

int incomingDialogInitialize(IPMI::Client *client);
int incomingDialogFinish(IPMI::Client *client);
int incomingDialogOpen(IPMI::Client *client, const SceIncomingDialogParam *pParam,
		       const char *text);
int incomingDialogClose(IPMI::Client *client);
int incomingDialogGetStatus(IPMI::Client *client, int *pStatus);
int incomingDialogSwitchToDialog(IPMI::Client *client);

#endif
