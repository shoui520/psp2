/*
 * SceTeleportServer (vs0:sys/external/libSceTeleportServer.suprx): an application launched by
 * Teleport answers with its initial information, which the library checks and passes to the
 * shell (SceShellSvc's IPMI service). No Sony record names its files; the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <teleport/teleport_server.h>
#include <libkernel/libkernel.h>
#include <sysmem/sysmem.h>
#include "../../../../../vsh/lowlayer/src/shell_svc/shell_svc_client.h"
#include "teleport_server_ipc_client.h"

/* the model capability of a system without Teleport (the name is ours) */
#define TELEPORT_SERVER_MODEL_CAPABILITY_UNSUPPORTED	10

extern "C" SceInt32 sceTeleportServerSendInitialInfo(const SceTeleportServerInitialInfo *info,
						     SceUInt32 timeout)
{
	TeleportServerInitialInfoRequest request;
	SceUInt32 i;
	int length;
	int ret;

	if (sceKernelCheckModelCapability(TELEPORT_SERVER_MODEL_CAPABILITY_UNSUPPORTED) == 1)
		return SCE_TELEPORT_ERROR_NOT_SUPPORTED_PLATFORM;
	if (info == NULL)
		return SCE_TELEPORT_ERROR_PARAM;
	for (i = 0; i < sizeof(info->reserved); i++) {
		if (info->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	if (info->status != SCE_TELEPORT_SERVER_STATUS_DEFAULT &&
	    info->status != SCE_TELEPORT_SERVER_STATUS_REFUSED)
		return SCE_TELEPORT_ERROR_PARAM;
	length = 0;
	if (info->extData != NULL) {
		for (length = 0; length < SCE_TELEPORT_SERVER_EXT_DATA_MAX_SIZE; length++) {
			if (info->extData[length] == '\0')
				break;
		}
		if (length == SCE_TELEPORT_SERVER_EXT_DATA_MAX_SIZE)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	sceClibMemset(&request, 0, sizeof(request));
	request.port = info->port;
	request.status = info->status;
	if (info->extData != NULL)
		sceClibStrncpy(request.extData, (const char *)info->extData, (SceSize)length);
	ret = teleportServerSendInitialInfo(SceShellSvc_B31E7F1C(), &request, timeout);
	if (ret == SCE_TELEPORT_ERROR_PARAM)
		return SCE_TELEPORT_ERROR_PARAM;
	if (ret != 0)
		return SCE_TELEPORT_ERROR_FATAL;
	return 0;
}
