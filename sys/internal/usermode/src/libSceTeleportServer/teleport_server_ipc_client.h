/* the shell's Teleport method (teleport_server_ipc_client.cpp; the names are ours) */
#ifndef LIBSCETELEPORTSERVER_TELEPORT_SERVER_IPC_CLIENT_H
#define LIBSCETELEPORTSERVER_TELEPORT_SERVER_IPC_CLIENT_H

#include <scetypes.h>
#include <teleport/teleport_server_types.h>
#include "../../../../../vsh/lowlayer/src/ipmi/client.h"

/* an application's initial information as the shell takes it */
typedef struct TeleportServerInitialInfoRequest {
	SceUInt32 port;
	SceInt32 status;
	char extData[SCE_TELEPORT_SERVER_EXT_DATA_MAX_SIZE];
} TeleportServerInitialInfoRequest;

int teleportServerSendInitialInfo(IPMI::Client *client,
				  const TeleportServerInitialInfoRequest *pRequest,
				  SceUInt32 timeout);

#endif
