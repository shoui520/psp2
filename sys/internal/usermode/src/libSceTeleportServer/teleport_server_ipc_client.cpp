/*
 * The shell's Teleport method over SceShellSvc's IPMI connection: a synchronous call that returns
 * the call's error or the method's result. Sony's build links it before the module's file; the
 * file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "teleport_server_ipc_client.h"

#define TELEPORT_SERVER_METHOD_SEND_INITIAL_INFO	0x1A0000

int teleportServerSendInitialInfo(IPMI::Client *client,
				  const TeleportServerInitialInfoRequest *pRequest,
				  SceUInt32 timeout)
{
	int result = 0;
	SceUInt32 time = timeout;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &time;
	in[0].dataSize = sizeof(time);
	in[1].pData = pRequest;
	in[1].dataSize = sizeof(*pRequest);
	ret = client->invokeSyncMethod(TELEPORT_SERVER_METHOD_SEND_INITIAL_INFO, in, 2, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
