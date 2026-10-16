/*
 * SceTeleportServer (vs0:sys/external/libSceTeleportServer.suprx): an
 * application answers a Teleport launch with its initial information; the
 * library checks it and passes it to the shell over ShellSvc.
 */

#ifdef __SNC__
/* built without automatic inlining (SNC -Xautoinlinesize=0) */
#pragma control autoinlinesize=0
#endif

#include <psp2/types.h>
#include <sce/shellsvc.h>

/* SDK teleport/teleport_error.h, teleport_server_types.h */
#define SCE_TELEPORT_ERROR_FATAL			0x80108401
#define SCE_TELEPORT_ERROR_PARAM			0x80108402
#define SCE_TELEPORT_ERROR_NOT_SUPPORTED_PLATFORM	0x8010840a

#define SCE_TELEPORT_SERVER_STATUS_DEFAULT	0
#define SCE_TELEPORT_SERVER_STATUS_REFUSED	1
#define SCE_TELEPORT_SERVER_EXT_DATA_MAX_SIZE	128

typedef struct _SceTeleportServerInitialInfo {
	SceInt32 status;
	const SceChar8 *extData;
	SceUInt16 port;
	SceUChar8 reserved[54];
} SceTeleportServerInitialInfo;

#define SHELLSVC_REQUEST_TELEPORT_INITIAL_INFO	0x1a0000
#define MODEL_CAPABILITY_TELEPORT_UNSUPPORTED	10	/* name unknown */

/* what the shell receives (structure name unknown) */
typedef struct TeleportInitialInfoRequest {
	SceUInt32 port;
	SceInt32 status;
	char extData[SCE_TELEPORT_SERVER_EXT_DATA_MAX_SIZE];
} TeleportInitialInfoRequest;

int sceKernelCheckModelCapability(int capability);
void *sceClibMemset(void *dst, int c, SceSize len);
char *sceClibStrncpy(char *dst, const char *src, SceSize len);

/* name unknown */
static int sendInitialInfo(struct ShellSvcInterface *svc, TeleportInitialInfoRequest *request,
			   SceUInt32 timeout)
{
	ShellSvcRequest call = (ShellSvcRequest)svc->methods[5];
	struct ShellSvcBuffer in[2];
	int result = 0;
	int ret;

	in[0].address = &timeout;
	in[0].size = sizeof(timeout);
	in[1].address = request;
	in[1].size = sizeof(*request);
	ret = call(svc, SHELLSVC_REQUEST_TELEPORT_INITIAL_INFO, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

SceInt32 sceTeleportServerSendInitialInfo(const SceTeleportServerInitialInfo *info,
					  SceUInt32 timeout)
{
	TeleportInitialInfoRequest request;
	SceUInt32 i, length;
	const SceChar8 *p;
	int ret;

	if (sceKernelCheckModelCapability(MODEL_CAPABILITY_TELEPORT_UNSUPPORTED) == 1)
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
		p = info->extData;
		for (length = 0; length < SCE_TELEPORT_SERVER_EXT_DATA_MAX_SIZE; length++) {
			if (p[length] == '\0')
				break;
		}
		if (length == SCE_TELEPORT_SERVER_EXT_DATA_MAX_SIZE)
			return SCE_TELEPORT_ERROR_PARAM;
	}

	sceClibMemset(&request, 0, sizeof(request));
	request.port = info->port;
	request.status = info->status;
	if (info->extData != NULL)
		sceClibStrncpy(request.extData, (const char *)info->extData, length);
	ret = sendInitialInfo(SceShellSvc_B31E7F1C(), &request, timeout);
	if (ret == (int)SCE_TELEPORT_ERROR_PARAM)
		return SCE_TELEPORT_ERROR_PARAM;
	if (ret != 0)
		return SCE_TELEPORT_ERROR_FATAL;
	return 0;
}
