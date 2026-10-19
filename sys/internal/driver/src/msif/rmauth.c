/* Sony's rmauth.c: removable-media secure-module requests. */
#include <skeleton.h>
#include "rmauth.h"
#include <sysmem/sysroot.h>
#include <string.h>
#include <sysmem/suspend.h>

struct MsifRmAuthRequest {
	uint8_t mediaId[16];
	uint32_t generation;
	uint8_t serviceData[12];
};

_Static_assert(sizeof(struct MsifRmAuthRequest) == 32,
	"removable-media secure request size");

const SceSelfAuthInfo gMsifSmAuthInfo = {
	.programAuthorityId = 0x2808000000000001ULL,
	.capability = { { 0x00f000c000000080ULL, 0xffffffff00000000ULL } },
	.attribute = { {
		0x00c3000003800980ULL,
		0x0000008009800000ULL,
		0x0000000000000000ULL,
		0xffffffff00000000ULL,
	} },
};

SceSblSmCommId gMsifSmId;

int invoke_rm_auth_secure_module(void)
{
	SelfAuthRequest auth;
	struct SysrootSelfInfo selfInfo;

	memset(&auth, 0, sizeof(auth));
	memcpy(&auth.caller, &gMsifSmAuthInfo, sizeof(auth.caller));
	selfInfo.selfData = NULL;
	auth.mediaType = 2;
	selfInfo.selfSize = 0;
	selfInfo.size = sizeof(selfInfo);
	if (SceSysrootForKernel_F10AB792(SCE_KERNEL_SYSROOT_SELF_INDEX_RMAUTH_SM,
		&selfInfo) < 0)
		return 0x800f0016;
	auth.selfType = (auth.selfType & ~0xf) | 2;
	return SceSblSmCommForKernel_039C73B1(0, selfInfo.selfData,
		selfInfo.selfSize, 0, &auth, &gMsifSmId);
}

int call_get_key_master_gen_no(uint32_t *generation)
{
	struct MsifRmAuthRequest request;
	SceSblSmCommPair stopResult;
	uint32_t serviceResult;
	int callResult;
	int result;

	if (!generation)
		return 0x800f1916;
	SceKernelSuspendForDriver_4DF40893(0);
	serviceResult = 0;
	callResult = invoke_rm_auth_secure_module();
	if (callResult) {
		result = callResult;
	} else {
		if (sceSblSmCommCallFunc(gMsifSmId, 1, &serviceResult, &request,
			sizeof(request)))
			serviceResult = 0x800f1928;
		callResult = sceSblSmCommStopSm(gMsifSmId, &stopResult);
		if (callResult) {
			result = callResult;
		} else {
			result = serviceResult;
			*generation = request.generation;
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int call_set_ind_key(const uint8_t *mediaId)
{
	struct MsifRmAuthRequest request;
	SceSblSmCommPair stopResult;
	uint32_t serviceResult;
	int result;

	if (!mediaId)
		return 0x800f1916;
	SceKernelSuspendForDriver_4DF40893(0);
	__builtin_memcpy(request.mediaId, mediaId, sizeof(request.mediaId));
	serviceResult = 0;
	result = invoke_rm_auth_secure_module();
	if (result)
		goto unlock;
	if (sceSblSmCommCallFunc(gMsifSmId, 2, &serviceResult, &request,
		sizeof(request)))
		serviceResult = 0x800f1928;
	result = sceSblSmCommStopSm(gMsifSmId, &stopResult);
	if (!result)
		result = serviceResult;
unlock:
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}
