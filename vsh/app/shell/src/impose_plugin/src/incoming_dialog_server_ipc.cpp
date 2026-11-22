/*
 * incoming_dialog_server_ipc.cpp: the incoming dialog server's IPMI methods (0x8115B498-
 * 0x8115B560): each of sceIncomingDialog's calls answered with the server's result. The file's
 * and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <ipmi/session.h>
#include "incoming_dialog_server.h"

/* the method is not the server's (PLACEHOLDER name) */
#define IPMI_ERROR_UNKNOWN_METHOD	((int)0x800205C1)

/* (0x8115B498) */
int shellIncomingDialogServerSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
					  const IPMI::DataInfo *inList, SceUInt32 inCount,
					  IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	int ret;

	switch (methodId) {
	case INCOMING_DIALOG_METHOD_INITIALIZE:
		ret = shellIncomingDialogServerInitialize(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	case INCOMING_DIALOG_METHOD_FINISH:
		ret = shellIncomingDialogServerFinish(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	case INCOMING_DIALOG_METHOD_OPEN:
		ret = shellIncomingDialogServerOpen(session,
						    (const SceIncomingDialogParam *)inList[0].pData,
						    (const char *)inList[1].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	case INCOMING_DIALOG_METHOD_CLOSE:
		ret = shellIncomingDialogServerClose(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	case INCOMING_DIALOG_METHOD_GET_STATUS:
		ret = shellIncomingDialogServerGetStatus(session, (SceInt32 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceInt32);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	case INCOMING_DIALOG_METHOD_SWITCH_TO_DIALOG:
		ret = shellIncomingDialogServerSwitchToDialog(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	default:
		return IPMI_ERROR_UNKNOWN_METHOD;
	}
}

/* (0x8115B554) */
int shellIncomingDialogServerAsyncDispatch(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *inList,
					   SceUInt32 inCount)
{
	return IPMI_ERROR_UNKNOWN_METHOD;
}
