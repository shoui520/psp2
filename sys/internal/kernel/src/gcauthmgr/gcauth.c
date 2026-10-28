/* Game card authentication: setup and the DrmBB cart secret exports. */
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <gcauthmgr/sblgcauthmgrdrmbb.h>
#include <gcauthmgr/sblgcauthmgrgcauth.h>
#include "gcauth.h"

int GcAuthInitializeGcAuth(void)
{
	gcAuthKeyId = 0;
	gcAuthGcAuthSemaphore = sceKernelCreateSema("SceGcAuth", 0, 1, 1, NULL);
	return gcAuthGcAuthSemaphore <= 0;
}

typedef struct GcAuthVerifyResponseData {
	unsigned char bytes[35];
} GcAuthVerifyResponseData;

typedef struct GcAuthVerifyInput {
	GcAuthCartSecretData cartSecret;
	GcAuthChallengeData challenge;
	GcAuthVerifyResponseData response;
} __attribute__((packed)) GcAuthVerifyInput;

typedef struct GcAuthSessionInput {
	GcAuthCartSecretData cartSecret;
	GcAuthCartSecretData response;
} __attribute__((packed)) GcAuthSessionInput;

typedef struct GcAuthChallengeInput {
	GcAuthCartSecretData cartSecret;
	GcAuthSessionKeyData sessionKey;
	GcAuthChallengeData challenge;
	unsigned char response[67];
} __attribute__((packed)) GcAuthChallengeInput;

typedef struct GcAuthDeriveInput {
	GcAuthCartSecretData cartSecret;
	GcAuthSessionKeyData sessionKey;
	GcAuthCartSecretData request;
	unsigned char response[67];
} __attribute__((packed)) GcAuthDeriveInput;

typedef union GcAuthKirkInput {
	GcAuthVerifyInput verify;
	GcAuthSessionInput session;
	GcAuthChallengeInput challenge;
	GcAuthDeriveInput derive;
} GcAuthKirkInput;

static inline __attribute__((always_inline)) int GcAuthExchange(SdifMmcContext *context, unsigned int command)
{
	if (sceMmcSendGeneralCmdData(context, &gcAuthCmd56Request, 512) != 0 ||
	    SceSdifForDriver_134E06C4(context, &gcAuthCmd56Response, 512) != 0 ||
	    gcAuthCmd56Response.command != command || gcAuthCmd56Response.error != 0)
		return -1;
	return 0;
}

int GcAuthCartAuthentication(const GcAuthCartAuthenticationRequest *request)
{
	unsigned int keyId = request->keyId;
	GcAuthKirkInput input;
	GcAuthChallengeData challenge;
	GcAuthCartSecretData signedRequest;
	GcAuthStatusBytes status;
	SdifMmcContext *context;
	SceUInt64 start;
	SceUInt64 end;
	unsigned int cartKeyId;
	int retries;
	int result;

	if (sceKernelWaitSema(gcAuthGcAuthSemaphore, 1, NULL) != 0)
		return (int)0x808a0701;
	if (keyId >= 0x10000) {
		result = (int)0x808a0702;
		goto clear;
	}
	context = gcAuthMmcContext = SceSdifForDriver_6A71987F(1);
	if (context == NULL) {
		if (SceSdifForDriver_22C82E79(1, &gcAuthMmcContext) != 0) {
			result = (int)0x808a0703;
			goto clear;
		}
		context = gcAuthMmcContext;
	}
	gcAuthCmd56Payload.code = 0xc4;
	gcAuthCmd56Payload.reserved = 0;
	gcAuthCmd56Payload.size = 3;
	GcAuthBuildCmd56Request(&gcAuthCmd56Payload, 3, 19, 49, &gcAuthCmd56Request);
	if (GcAuthExchange(context, 49) != 0 || gcAuthCmd56Response.status != 0) {
		result = (int)0x808a0704;
		goto clear;
	}
	if (GcAuthGetCmd56Status(gcAuthMmcContext, &status) != 0) {
		result = (int)0x808a0705;
		goto clear;
	}
	if (status.high != 0 || status.low == 0) {
		result = (int)0x808a0706;
		goto clear;
	}
	gcAuthCmd56Payload.reserved = status.high;
	context = gcAuthMmcContext;
	gcAuthCmd56Payload.size = 3;
	gcAuthCmd56Payload.code = 0xa1;
	GcAuthBuildCmd56Request(&gcAuthCmd56Payload, 3, 43, 2, &gcAuthCmd56Request);
	if (GcAuthExchange(context, 2) != 0)
		goto fail;
	if (gcAuthCmd56Response.status != 0) {
		memset(&gcAuthCartSecret, gcAuthCmd56Response.error, sizeof(gcAuthCartSecret));
		goto fail;
	}
	cartKeyId = gcAuthCmd56Response.data[3] | gcAuthCmd56Response.data[2] << 8;
	gcAuthKeyId = cartKeyId;
	if ((cartKeyId & ~0x8000) != 1)
		goto fail;
	gcAuthCmd56Payload.reserved = gcAuthCmd56Response.status;
	gcAuthCmd56Payload.data[0] = (unsigned char)(cartKeyId >> 8);
	gcAuthCmd56Payload.data[1] = (unsigned char)cartKeyId;
	gcAuthCmd56Payload.size = 21;
	gcAuthCartSecret = *(const GcAuthCartSecretData *)(gcAuthCmd56Response.data + 8);
	gcAuthCmd56Payload.code = 0xa2;
	if (GcAuthGenerateRandom16(gcAuthCmd56Payload.data + 2) != 0)
		goto fail;
	gcAuthCmd56Payload.data[2] |= 0x80;
	GcAuthBuildCmd56Request(&gcAuthCmd56Payload, 21, 35, 3, &gcAuthCmd56Request);
	if (GcAuthExchange(context, 3) != 0 || gcAuthCmd56Response.status != 0)
		goto fail;
	input.verify.cartSecret = gcAuthCartSecret;
	input.verify.challenge =
		*(const GcAuthChallengeData *)(gcAuthCmd56Request.payload + 5);
	input.verify.response =
		*(const GcAuthVerifyResponseData *)gcAuthCmd56Response.payload;
	if (GcAuthExecuteKirkCommand(NULL, &input, 27, sizeof(input.verify),
			gcAuthKeyId) != 0)
		goto fail;
	input.session.cartSecret = gcAuthCartSecret;
	input.session.response = *(const GcAuthCartSecretData *)gcAuthCmd56Response.data;
	if (GcAuthExecuteKirkCommand(&gcAuthCmd56Payload, &input, 28, sizeof(input.session),
			gcAuthKeyId) != 0)
		goto fail;
	gcAuthSessionKey = *(const GcAuthSessionKeyData *)gcAuthCmd56Payload.data;
	GcAuthBuildCmd56Request(&gcAuthCmd56Payload, 51, 3, 5, &gcAuthCmd56Request);
	if (GcAuthExchange(context, 5) != 0 || gcAuthCmd56Response.status != 0)
		goto fail;
	if (GcAuthGetCmd56Status(gcAuthMmcContext, &status) != 0) {
		result = (int)0x808a0705;
		goto clear;
	}
	if (status.low != 0 || status.high != 0) {
		result = (int)0x808a0706;
		goto clear;
	}
	for (retries = 3; ; ) {
		gcAuthCmd56Payload.code = 0xa4;
		gcAuthCmd56Payload.reserved = 0;
		gcAuthCmd56Payload.size = 19;
		cartKeyId = gcAuthKeyId;
		context = gcAuthMmcContext;
		if (GcAuthGenerateRandom16(gcAuthCmd56Payload.data) == 0) {
			gcAuthCmd56Payload.data[0] |= 0x80;
			GcAuthBuildCmd56Request(&gcAuthCmd56Payload, 19, 67, 7,
				&gcAuthCmd56Request);
			challenge = *(const GcAuthChallengeData *)(gcAuthCmd56Request.payload + 3);
			start = sceKernelGetSystemTimeWide();
			if (sceMmcSendGeneralCmdData(context, &gcAuthCmd56Request, 512) == 0 &&
			    SceSdifForDriver_134E06C4(context, &gcAuthCmd56Response, 512) == 0) {
				end = sceKernelGetSystemTimeWide();
				if (gcAuthCmd56Response.command != 7 ||
				    gcAuthCmd56Response.error != 0)
					result = -1;
				else if (gcAuthCmd56Response.status != 0)
					result = gcAuthCmd56Response.status;
				else {
					input.challenge.cartSecret = gcAuthCartSecret;
					input.challenge.sessionKey = gcAuthSessionKey;
					input.challenge.challenge = challenge;
					memcpy(input.challenge.response, gcAuthCmd56Response.payload,
						sizeof(input.challenge.response));
					result = GcAuthExecuteKirkCommand(NULL, &input, 29,
						sizeof(input.challenge), cartKeyId) != 0 ? -3 :
						gcAuthCmd56Response.status;
				}
				if (end >= start && end - start <= 50000 && result == 0)
					break;
			}
		}
		if (--retries == 0)
			goto fail;
	}
	context = gcAuthMmcContext;
	cartKeyId = gcAuthKeyId;
	if (GcAuthBuildSignedRequest(2, cartKeyId) != 0)
		goto fail;
	signedRequest = *(const GcAuthCartSecretData *)(gcAuthCmd56Request.payload + 3);
	if (GcAuthExchange(context, 17) != 0 || gcAuthCmd56Response.status != 0)
		goto fail;
	input.derive.cartSecret = gcAuthCartSecret;
	input.derive.sessionKey = gcAuthSessionKey;
	input.derive.request = signedRequest;
	memcpy(input.derive.response, gcAuthCmd56Response.payload,
		sizeof(input.derive.response));
	if (GcAuthExecuteKirkCommand(&gcAuthCartDerivedKey, &input, 31,
			sizeof(input.derive), cartKeyId) != 0 ||
	    gcAuthCmd56Response.status != 0)
		goto fail;
	result = GcAuthFinishCartAuthentication(gcAuthMmcContext, gcAuthKeyId);
	if (result == 0)
		goto out;
fail:
	result = (int)0x808a0707;
clear:
	SceSblGcAuthMgrDrmBBForDriver_BB451E83();
out:
	if (sceKernelSignalSema(gcAuthGcAuthSemaphore, 1) != 0)
		result = (int)0x808a0701;
	return result;
}

int SceSblGcAuthMgrGcAuthForDriver_68781760(SceUInt32 keyId)
{
	GcAuthCartAuthenticationRequest request;

	request.keyId = keyId;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthCartAuthentication, &request);
}

int SceSblGcAuthMgrDrmBBForDriver_BB70DDC0(void *secret)
{
	*(GcAuthCartSecretData *)secret = gcAuthCartKeys.secret;
	return 0;
}

int SceSblGcAuthMgrDrmBBForDriver_22FD5D23(const void *hash)
{
	const unsigned char *bytes = hash;
	const unsigned char *cart = gcAuthCartKeys.hash;
	unsigned int difference = 0;

	do
		difference |= *cart++ ^ *bytes++;
	while (cart != gcAuthCartKeys.hash + sizeof(gcAuthCartKeys.hash));
	if (difference != 0)
		return (int)0x808a040a;
	return 0;
}

int SceSblGcAuthMgrDrmBBForDriver_BB451E83(void)
{
	unsigned int index;

	for (index = 0; index < sizeof(gcAuthSessionKey); index++)
		gcAuthSessionKey.bytes[index] = 0;
	for (index = 0; index < sizeof(gcAuthCartSecret); index++)
		gcAuthCartSecret.bytes[index] = 0;
	for (index = 0; index < sizeof(gcAuthCartDerivedKey); index++)
		gcAuthCartDerivedKey.bytes[index] = 0;
	for (index = 0; index < sizeof(gcAuthCartKeys); index++)
		((unsigned char *)&gcAuthCartKeys)[index] = 0;
	return 0;
}
