/*
 * incoming_dialog_server.h: the incoming dialog server (impose_psmenu_util.cpp) and its IPMI
 * methods (incoming_dialog_server_ipc.cpp), which serve the applications' sceIncomingDialog
 * calls: one request per application, taken by the impose plugin's dialog. The file's and the
 * names are ours (PLACEHOLDER); the requests are in shell.h.
 */
#ifndef INCOMING_DIALOG_SERVER_H
#define INCOMING_DIALOG_SERVER_H

#include <scetypes.h>
#include <ipmi/session.h>
#include "../../../shell.h"

/* the methods, sceIncomingDialog's calls in its order (PLACEHOLDER names) */
#define INCOMING_DIALOG_METHOD_INITIALIZE	0x170000
#define INCOMING_DIALOG_METHOD_FINISH		0x170001
#define INCOMING_DIALOG_METHOD_OPEN		0x170002
#define INCOMING_DIALOG_METHOD_CLOSE		0x170003
#define INCOMING_DIALOG_METHOD_GET_STATUS	0x170004
#define INCOMING_DIALOG_METHOD_SWITCH_TO_DIALOG	0x170005

/* an application's calls, by its session; the Self variants for the shell's own process */
int shellIncomingDialogServerInitialize(IPMI::Session *session);	/* (0x8115AE10) */
int shellIncomingDialogServerInitializeSelf(void);			/* (0x8115AEBC) */
int shellIncomingDialogServerFinish(IPMI::Session *session);		/* (0x8115AEC6) */
int shellIncomingDialogServerFinishSelf(void);				/* (0x8115AF70) */
int shellIncomingDialogServerOpen(IPMI::Session *session, const SceIncomingDialogParam *dialogParam,
				  const char *param);			/* (0x8115AF7A) */
int shellIncomingDialogServerOpenSelf(const SceIncomingDialogParam *dialogParam); /* (0x8115B0A8) */
int shellIncomingDialogServerClose(IPMI::Session *session);		/* (0x8115B0B6) */
int shellIncomingDialogServerGetStatus(IPMI::Session *session, SceInt32 *state); /* (0x8115B12A) */
SceInt32 shellIncomingDialogServerGetStatusSelf(void);			/* (0x8115B19A) */
int shellIncomingDialogServerSwitchToDialog(IPMI::Session *session);	/* (0x8115B332) */

/* the server's handlers in the shell's table of them (0x81511F44) */
int shellIncomingDialogServerOnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
					  const IPMI::DataInfo *inList, SceUInt32 inCount,
					  IPMI::BufferInfo *outList, SceUInt32 outCount);
int shellIncomingDialogServerOnAsyncMethod(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *inList,
					   SceUInt32 inCount);
void shellIncomingDialogServerOnSessionClosed(IPMI::Session *session);

/* the methods answered (0x8115B498); no asynchronous one (0x8115B554, the shell's servers
   share it) */
int shellIncomingDialogServerSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
					  const IPMI::DataInfo *inList, SceUInt32 inCount,
					  IPMI::BufferInfo *outList, SceUInt32 outCount);
int shellIncomingDialogServerAsyncDispatch(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *inList,
					   SceUInt32 inCount);

#endif
