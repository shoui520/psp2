#include "ssmgr_internal.h"
#include <acmgr/acmgr.h>
#include <idstorage/idstorage.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/sysclib.h>
#include <rtc/rtc.h>
#include <sysmem/sblaimgr.h>

SceUID aimgrSmId;

const SceSelfAuthInfo aimgrSmAuthInfo = {
	.programAuthorityId = UINT64_C(0x2808000000000001),
	.capability = { { 0x00f000c000000080ULL, 0xffffffff00000000ULL } },
	.attribute = { {
		0x00c3000003800980ULL,
		0x0000008009800000ULL,
		0x0000000000000000ULL,
		0xffffffff00000000ULL,
	} }
};

int invoke_aimgr_secure_module(void)
{
	SelfAuthRequest authInfo;
	int ret;

	memset(&authInfo, 0, sizeof(authInfo));
	memcpy(&authInfo.caller, &aimgrSmAuthInfo, sizeof(aimgrSmAuthInfo));
	ret = SceSblACMgrForKernel_sceSblACMgrGetMediaType("os0:sm/aimgr_sm.self", &authInfo.mediaType);
	if (ret)
		return ret;
	authInfo.selfType = (authInfo.selfType & ~0xf) | 2;
	return sceSblSmCommStartSm(0, "os0:sm/aimgr_sm.self", 0,
		&authInfo, &aimgrSmId);
}

int stop_aimgr_secure_module(void)
{
	SceSblSmCommPair result;
	int ret;

	ret = sceSblSmCommStopSm(aimgrSmId, &result);
	if (!ret)
		ret = result.result;
	return ret;
}

int aimgrReadConsoleId(SceConsoleId *consoleId)
{
	SceConsoleId value;
	SceUInt32 serviceResult = 0;
	SceUInt32 result;
	int ret, callRet;

	if (!consoleId) {
		result = SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;
		goto out;
	}
	ret = invoke_aimgr_secure_module();
	if (ret)
		goto error;
	callRet = sceSblSmCommCallFunc(aimgrSmId, 1, &serviceResult,
				  &value, sizeof(value));
	ret = stop_aimgr_secure_module();
	if (ret)
		goto error;
	result = SCE_SBL_ERROR_AIMGR_SM_CALL;
	if (callRet == 0) {
		result = serviceResult;
		if (result == 0)
			*consoleId = value;
	}
out:
	return result;
error:
	result = ret;
	goto out;
}

int aimgrReadPscode2(ScePsCode *pscode)
{
	union {
		uint32_t words[128];
		ScePsCode pscode;
	} leaf;
	SceUInt32 serviceResult = 0;
	int ret, callRet;

	if (pscode) {
		if (sceIdStorageReadLeaf(68, leaf.words)) {
			ret = SCE_SBL_ERROR_AIMGR_IDSTORAGE;
		} else {
			ret = invoke_aimgr_secure_module();
			if (ret == 0) {
				callRet = sceSblSmCommCallFunc(aimgrSmId, 4,
					&serviceResult, leaf.words, sizeof(leaf));
				ret = stop_aimgr_secure_module();
				if (ret == 0) {
					ret = SCE_SBL_ERROR_AIMGR_SM_CALL;
					if (callRet == 0) {
						ret = serviceResult;
						if (ret == 0)
							*pscode = leaf.pscode;
					}
				}
			}
		}
	} else {
		ret = SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;
	}
	return ret;
}

int aimgrReadVisibleId(void *visibleId)
{
	uint32_t reversed[8];
	uint8_t value[32];
	SceUInt32 serviceResult = 0;
	int ret, stopRet;

	if (!visibleId)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;
	ret = invoke_aimgr_secure_module();
	if (ret)
		return ret;
	ret = sceSblSmCommCallFunc(aimgrSmId, 3, &serviceResult,
				  value, sizeof(value));
	stopRet = stop_aimgr_secure_module();
	if (stopRet)
		return stopRet;
	if (ret)
		return SCE_SBL_ERROR_AIMGR_SM_CALL;
	if (!serviceResult) {
		const uint8_t *source = value + sizeof(value);
		uint8_t *destination = (uint8_t *)reversed;

		do {
			*destination++ = *--source;
		} while (source != value);
		__builtin_memcpy(visibleId, reversed, sizeof(reversed));
	}
	return serviceResult;
}

/* Complete service-5 exchange, including the returned 512-byte passphrase. */
typedef struct AimgrPassPhraseRequest {
	uint64_t timestamp;
	uint32_t mode;
	uint32_t reserved;
	uint32_t parameters[4];
	uint8_t passPhrase[512];
} AimgrPassPhraseRequest;

int aimgrCreatePassPhrase(const uint32_t *parameters, void *passPhrase)
{
	AimgrPassPhraseRequest request;
	SceUInt32 serviceResult = 0;
	int (*volatile getCurrentSecureTick)(SceRtcTick *tick);
	SceRtcTick tick;
	int ret;

	if (!passPhrase || !parameters)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;
	memset(&request, 0, sizeof(request));
	request.mode = parameters[0];
	__builtin_memcpy(request.parameters, parameters + 2,
			 sizeof(request.parameters));
	if (sceIdStorageReadLeaf(68, request.passPhrase))
		return SCE_SBL_ERROR_AIMGR_IDSTORAGE;
	getCurrentSecureTick = SceRtcForDriver_401C0954;
	if (!getCurrentSecureTick)
		return SCE_SBL_ERROR_AIMGR_ILLEGAL_STATE;
	ret = SceRtcForDriver_401C0954(&tick);
	if (ret)
		return ret;
	request.timestamp = (tick.tick + UINT64_C(0xff23400100d44000)) / 1000000;
	ret = invoke_aimgr_secure_module();
	if (ret)
		return ret;
	ret = sceSblSmCommCallFunc(aimgrSmId, 5, &serviceResult,
				  &request, sizeof(request));
	if (ret)
		return ret;
	ret = stop_aimgr_secure_module();
	if (ret)
		return ret;
	if (serviceResult)
		return serviceResult;
	memcpy(passPhrase, request.passPhrase, sizeof(request.passPhrase));
	return 0;
}
