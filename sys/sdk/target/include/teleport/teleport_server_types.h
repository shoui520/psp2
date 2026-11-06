/*
 * teleport/teleport_server_types.h: what an application sends the Teleport client when it is
 * launched (SDK names).
 */
#ifndef _SCE_TELEPORT_SERVER_TYPES_H_
#define _SCE_TELEPORT_SERVER_TYPES_H_

#include <scetypes.h>

#define SCE_TELEPORT_SERVER_STATUS_DEFAULT	0
#define SCE_TELEPORT_SERVER_STATUS_REFUSED	1

#define SCE_TELEPORT_SERVER_EXT_DATA_MAX_SIZE	128

typedef struct _SceTeleportServerInitialInfo {
	SceInt32 status;
	const SceChar8 *extData;	/* SCE_TELEPORT_SERVER_EXT_DATA_MAX_SIZE, the NUL included */
	SceUInt16 port;
	SceUChar8 reserved[54];
} SceTeleportServerInitialInfo;

#endif /* _SCE_TELEPORT_SERVER_TYPES_H_ */
