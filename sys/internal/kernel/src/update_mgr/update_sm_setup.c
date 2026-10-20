#include <sm_comm/sm_comm.h>
#include <iofilemgr/iofilemgr.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "update_internal.h"
#include <acmgr/acmgr.h>

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
	.capability = { { 0x00f000c000000080ULL, 0xffffffff00000000ULL } },
	.attribute = { {
		0x00c3000003800980ULL,
		0x0000008009800000ULL,
		0x0000000000000000ULL,
		0xffffffff00000000ULL,
	} }
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
	result = sceSblSmCommStartSm(0, sm_path, commandId, &auth, smId);
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	return result;
}
