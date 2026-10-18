#include <sm_comm/sm_comm.h>
#include <iofilemgr/iofilemgr.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "update_internal.h"

int SceSblACMgrForKernel_sceSblACMgrGetMediaType(const char *path, void *type);

typedef struct UpdateUtilSmPath {
	const char *sm_path;
	const char *remote_sm_path;
} UpdateUtilSmPath;

static const UpdateUtilSmPath updateSmPaths[] = {
	{ "os0:sm/update_service_sm.self", "host0:module/sbl/update_service_sm.self" },
	{ "os0:sm/spkg_verifier_sm_w_key_2.self",
	  "host0:module/sbl/spkg_verifier_sm_w_key_2.self" }
};

static const SceSelfAuthInfo updateSmAuthInfo = {
	.programAuthorityId = 0x2808000000000001ULL,
	.capability = {
		[0] = 0x80, [4] = 0xc0, [6] = 0xf0,
		[12] = 0xff, [13] = 0xff, [14] = 0xff, [15] = 0xff
	},
	.attribute = {
		[0] = 0x80, [1] = 0x09, [2] = 0x80, [3] = 0x03,
		[6] = 0xc3, [10] = 0x80, [11] = 0x09, [12] = 0x80,
		[28] = 0xff, [29] = 0xff, [30] = 0xff, [31] = 0xff
	}
};

int UpdateUtilSetupSm(unsigned int type, int *smId, int commandId)
{
	SceIoStat stat;
	SelfAuthRequest auth;
	const char *sm_path;
	int result, permission;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (!smId || *smId != -1 || type > 1) {
		sceKernelSetThreadAccessLevel(permission);
		return (int)0x800f0216;
	}
	sm_path = updateSmPaths[type].sm_path;
	if (sceIoGetstat(sm_path, &stat) || !sm_path) {
		result = (int)0x800f0216;
		goto restorePermission;
	}
	memset(&auth, 0, sizeof(auth));
	memcpy(&auth.caller, &updateSmAuthInfo, sizeof(auth.caller));
	result = SceSblACMgrForKernel_sceSblACMgrGetMediaType(sm_path, &auth.mediaType);
	if (result < 0)
		goto restorePermission;
	auth.selfType = (auth.selfType & ~15) | 2;
	result = SceSblSmCommForKernel_7863A0CC(0, sm_path, commandId, &auth, smId);
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	return result;
}
