#include "ssmgr_internal.h"
#include <psp2kern/kernel/acmgr.h>
#include <psp2kern/idstorage.h>
#include <sce/sm_comm.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/rtc.h>

SceUID aimgrSmId;

const SceSelfAuthInfo aimgrSmAuthInfo = {
	.program_authority_id = UINT64_C(0x2808000000000001),
	.capability = {
		0x80, 0, 0, 0, 0xc0, 0, 0xf0, 0,
		0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff
	},
	.attribute = {
		0x80, 0x09, 0x80, 0x03, 0, 0, 0xc3, 0,
		0, 0, 0x80, 0x09, 0x80, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff
	}
};
int ksceSblACMgrGetMediaType2(const char *path, uint32_t *mediaType);

int invoke_aimgr_secure_module(void)
{
	SceAuthInfo authInfo;
	int ret;

	memset(&authInfo, 0, sizeof(authInfo));
	memcpy(&authInfo.request, &aimgrSmAuthInfo, sizeof(aimgrSmAuthInfo));
	ret = ksceSblACMgrGetMediaType2("os0:sm/aimgr_sm.self", &authInfo.media_type);
	if (ret)
		return ret;
	authInfo.self_type = (authInfo.self_type & ~0xf) | 2;
	return ksceSblSmCommStartSmFromFile(0, "os0:sm/aimgr_sm.self", 0,
		&authInfo, &aimgrSmId);
}

int stop_aimgr_secure_module(void)
{
	SceSblSmCommPair result;
	int ret;

	ret = ksceSblSmCommStopSm(aimgrSmId, &result);
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
		result = 0x800f0c16;
		goto out;
	}
	ret = invoke_aimgr_secure_module();
	if (ret)
		goto error;
	callRet = ksceSblSmCommCallFunc(aimgrSmId, 1, &serviceResult,
				  &value, sizeof(value));
	ret = stop_aimgr_secure_module();
	if (ret)
		goto error;
	result = 0x800f0c28;
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
		if (ksceIdStorageReadLeaf(68, leaf.words)) {
			ret = 0x800f0c05;
		} else {
			ret = invoke_aimgr_secure_module();
			if (ret == 0) {
				callRet = ksceSblSmCommCallFunc(aimgrSmId, 4,
					&serviceResult, leaf.words, sizeof(leaf));
				ret = stop_aimgr_secure_module();
				if (ret == 0) {
					ret = 0x800f0c28;
					if (callRet == 0) {
						ret = serviceResult;
						if (ret == 0)
							*pscode = leaf.pscode;
					}
				}
			}
		}
	} else {
		ret = 0x800f0c16;
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
		return 0x800f0c16;
	ret = invoke_aimgr_secure_module();
	if (ret)
		return ret;
	ret = ksceSblSmCommCallFunc(aimgrSmId, 3, &serviceResult,
				  value, sizeof(value));
	stopRet = stop_aimgr_secure_module();
	if (stopRet)
		return stopRet;
	if (ret)
		return 0x800f0c28;
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
		return 0x800f0c16;
	memset(&request, 0, sizeof(request));
	request.mode = parameters[0];
	__builtin_memcpy(request.parameters, parameters + 2,
			 sizeof(request.parameters));
	if (ksceIdStorageReadLeaf(68, request.passPhrase))
		return 0x800f0c05;
	getCurrentSecureTick = ksceRtcGetCurrentSecureTick;
	if (!getCurrentSecureTick)
		return 0x800f0c29;
	ret = ksceRtcGetCurrentSecureTick(&tick);
	if (ret)
		return ret;
	request.timestamp = (tick.tick + UINT64_C(0xff23400100d44000)) / 1000000;
	ret = invoke_aimgr_secure_module();
	if (ret)
		return ret;
	ret = ksceSblSmCommCallFunc(aimgrSmId, 5, &serviceResult,
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
