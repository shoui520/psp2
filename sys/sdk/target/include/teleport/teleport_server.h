/*
 * teleport/teleport_server.h: an application's answer to a Teleport launch (SceTeleportServer).
 */
#ifndef _SCE_TELEPORT_SERVER_H_
#define _SCE_TELEPORT_SERVER_H_

#include <scetypes.h>
#include <teleport/teleport_error.h>
#include <teleport/teleport_server_types.h>

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

SceInt32 sceTeleportServerSendInitialInfo(const SceTeleportServerInitialInfo *info,
					  SceUInt32 timeout);

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _SCE_TELEPORT_SERVER_H_ */
