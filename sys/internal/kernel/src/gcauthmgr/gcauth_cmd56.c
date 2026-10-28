/* Game card CMD56 exchanges: request framing and the card's signed answers. */
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <ss_mgr/ss_mgr.h>
#include "gcauth.h"

SceUID gcAuthGcAuthSemaphore;
SdifMmcContext *gcAuthMmcContext;
GcAuthSessionKeyData gcAuthSessionKey;
GcAuthCmd56Request gcAuthCmd56Request;
unsigned int gcAuthKeyId;
GcAuthCmd56Payload gcAuthCmd56Payload;
GcAuthCartSecretData gcAuthCartSecret;
GcAuthCartKeys gcAuthCartKeys;
GcAuthCmd56Response gcAuthCmd56Response;
GcAuthCmd56Response gcAuthCmd56SecondResponse;
GcAuthCartSecretData gcAuthCartDerivedKey;

static const GcAuthCmd56Key gcAuthCmd56RequestKey = { {
	0xdd, 0x10, 0x25, 0x44, 0x15, 0x23, 0xfd, 0xc0,
	0xf9, 0xe9, 0x15, 0x26, 0xdc, 0x2a, 0xe0, 0x84,
	0xa9, 0x03, 0xa2, 0x97, 0xd4, 0xbb, 0xf8, 0x52,
	0xd3, 0xd4, 0x94, 0x2c, 0x89, 0x03, 0xcc, 0x77,
} };

void GcAuthBuildCmd56Request(const GcAuthCmd56Payload *payload, unsigned int size,
		unsigned int responseSize, unsigned int command, GcAuthCmd56Request *request)
{
	memset(request, 0, sizeof(*request));
	request->key = gcAuthCmd56RequestKey;
	request->command[0] = (unsigned char)command;
	request->command[1] = (unsigned char)(command >> 8);
	request->command[2] = (unsigned char)(command >> 16);
	request->command[3] = (unsigned char)(command >> 24);
	request->size[0] = (unsigned char)size;
	request->size[1] = (unsigned char)(size >> 8);
	request->size[2] = (unsigned char)(size >> 16);
	request->size[3] = (unsigned char)(size >> 24);
	request->responseSize[0] = (unsigned char)responseSize;
	request->responseSize[1] = (unsigned char)(responseSize >> 8);
	request->responseSize[2] = (unsigned char)(responseSize >> 16);
	request->responseSize[3] = (unsigned char)(responseSize >> 24);
	memcpy(request->payload, payload, size);
}

typedef struct GcAuthGetCartSecretRequest {
	GcAuthCartSecretData *secret;
} GcAuthGetCartSecretRequest;

int GcAuthGetCartSecret(const GcAuthGetCartSecretRequest *request)
{
	int result;

	if (sceKernelWaitSema(gcAuthGcAuthSemaphore, 1, NULL) != 0)
		return (int)0x808a0701;
	if (request->secret == NULL)
		goto fail;
	gcAuthMmcContext = SceSdifForDriver_6A71987F(1);
	if (gcAuthMmcContext == NULL) {
		result = (int)0x808a0703;
		goto out;
	}
	*(GcAuthCartSecretData *)&gcAuthCmd56Payload = gcAuthCartSecret;
	result = GcAuthExecuteKirkCommand(&gcAuthCmd56Payload, &gcAuthCmd56Payload,
		35, 32, 0);
	if (result != 0)
		goto fail;
	*request->secret = *(GcAuthCartSecretData *)&gcAuthCmd56Payload;
	goto out;
fail:
	result = (int)0x808a0700;
out:
	if (sceKernelSignalSema(gcAuthGcAuthSemaphore, 1) != 0)
		return (int)0x808a0701;
	return result;
}

int GcAuthGetCmd56Status(SdifMmcContext *context, GcAuthStatusBytes *status)
{
	int result;

	gcAuthCmd56Payload.code = 0xc2;
	gcAuthCmd56Payload.reserved = 0;
	gcAuthCmd56Payload.size = 3;
	GcAuthBuildCmd56Request(&gcAuthCmd56Payload, 3, 5, 35, &gcAuthCmd56Request);
	result = sceMmcSendGeneralCmdData(context, &gcAuthCmd56Request, 512);
	if (result != 0)
		return result;
	result = SceSdifForDriver_134E06C4(context, &gcAuthCmd56Response, 512);
	if (result != 0)
		return result;
	if (gcAuthCmd56Response.command != 35 || gcAuthCmd56Response.error != 0)
		return -1;
	if (gcAuthCmd56Response.status != 0)
		return gcAuthCmd56Response.status;
	status->low = gcAuthCmd56Response.data[0];
	status->high = gcAuthCmd56Response.data[1];
	return gcAuthCmd56Response.status;
}

typedef struct GcAuthSignedRequestInput {
	GcAuthCartSecretData cartSecret;
	GcAuthSessionKeyData sessionKey;
	unsigned char type;
} GcAuthSignedRequestInput;

int GcAuthBuildSignedRequest(unsigned char type, unsigned int keyId)
{
	GcAuthSignedRequestInput input;
	int result;

	input.cartSecret = gcAuthCartSecret;
	input.sessionKey = gcAuthSessionKey;
	input.type = type;
	result = GcAuthExecuteKirkCommand(&gcAuthCmd56Payload, &input, 30, 81, keyId);
	if (result != 0)
		result = -1;
	else
		GcAuthBuildCmd56Request(&gcAuthCmd56Payload, 51, 67, 17, &gcAuthCmd56Request);
	return result;
}

int GcAuthGenerateRandom16(unsigned char *output)
{
	unsigned char random[64];
	int index;

	for (index = 0; index != 16; index++) {
		if (sceSblRngGenuineRandomNumber(random) != 0)
			return (int)0x808a0700;
		output[index] = random[0];
	}
	return 0;
}

typedef struct GcAuthFinishInput {
	GcAuthCartSecretData cartSecret;
	GcAuthSessionKeyData sessionKey;
	GcAuthCartSecretData request;
	unsigned char response[67];
	GcAuthChallengeData challenge;
	unsigned char secondResponse[83];
} __attribute__((packed)) GcAuthFinishInput;

int GcAuthFinishCartAuthentication(SdifMmcContext *context, unsigned int keyId)
{
	GcAuthFinishInput input;
	GcAuthCartSecretData request;
	GcAuthChallengeData challenge;
	int result;

	result = GcAuthBuildSignedRequest(3, keyId);
	if (result != 0)
		return result;
	request = *(const GcAuthCartSecretData *)(gcAuthCmd56Request.payload + 3);
	result = sceMmcSendGeneralCmdData(context, &gcAuthCmd56Request, 512);
	if (result != 0)
		return result;
	result = SceSdifForDriver_134E06C4(context, &gcAuthCmd56Response, 512);
	if (result != 0)
		return result;
	gcAuthCmd56Payload.reserved = 0;
	gcAuthCmd56Payload.code = 0xc1;
	gcAuthCmd56Payload.size = 19;
	result = GcAuthGenerateRandom16(gcAuthCmd56Payload.data);
	if (result != 0)
		return result;
	gcAuthCmd56Payload.data[0] |= 0x80;
	GcAuthBuildCmd56Request(&gcAuthCmd56Payload, 19, 83, 25, &gcAuthCmd56Request);
	challenge = *(const GcAuthChallengeData *)(gcAuthCmd56Request.payload + 3);
	result = sceMmcSendGeneralCmdData(context, &gcAuthCmd56Request, 512);
	if (result != 0)
		return result;
	result = SceSdifForDriver_134E06C4(context, &gcAuthCmd56SecondResponse, 512);
	if (result != 0)
		return result;
	if (gcAuthCmd56Response.command != 17 || gcAuthCmd56Response.error != 0)
		return -1;
	if (gcAuthCmd56Response.status != 0)
		return gcAuthCmd56Response.status;
	if (gcAuthCmd56SecondResponse.command != 25 || gcAuthCmd56SecondResponse.error != 0)
		return -1;
	if (gcAuthCmd56SecondResponse.status != 0)
		return gcAuthCmd56SecondResponse.status;
	input.cartSecret = gcAuthCartSecret;
	input.sessionKey = gcAuthSessionKey;
	input.request = request;
	memcpy(input.response, gcAuthCmd56Response.payload, sizeof(input.response));
	input.challenge = challenge;
	memcpy(input.secondResponse, gcAuthCmd56SecondResponse.payload,
		sizeof(input.secondResponse));
	if (GcAuthExecuteKirkCommand(&gcAuthCartKeys, &input, 32, sizeof(input), keyId) != 0)
		return -3;
	return gcAuthCmd56SecondResponse.status;
}
