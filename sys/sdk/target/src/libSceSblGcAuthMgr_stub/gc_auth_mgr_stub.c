/*
 * The user side of SceSblGcAuthMgr's system calls, as the stub library links them into a
 * program (libmtp): each wrapper fills the sizes of its request block and makes the system
 * call of its name with a leading underscore (the library's and the file's names ours).
 */
#include <scetypes.h>
#include <gcauthmgr/gcauthmgr_user.h>

#include "gc_auth_mgr_stub.h"

int sceSblGcAuthMgrPcactGetChallenge(SceUInt mode, const void *input, void *output)
{
	struct PcActivationChallengeRequest request;

	request.inputSize = 32;
	request.outputSize = 128;
	return _sceSblGcAuthMgrPcactGetChallenge(mode, input, output, &request);
}

int sceSblGcAuthMgrPcactActivation(const void *activation, SceUInt unknownArgument)
{
	return _sceSblGcAuthMgrPcactActivation(activation, unknownArgument);
}

int sceSblGcAuthMgrPkgVry(const void *first, const void *second)
{
	struct PackageVerifyRequest request;

	request.firstSize = 20;
	request.secondSize = 40;
	return _sceSblGcAuthMgrPkgVry(first, second, &request);
}

int sceSblGcAuthMgrSclkGetData1(void *data)
{
	struct SecureClockRequest request;

	request.size = 48;
	return _sceSblGcAuthMgrSclkGetData1(data, &request);
}

int sceSblGcAuthMgrSclkSetData2(const void *data)
{
	struct SecureClockRequest request;

	request.size = 304;
	return _sceSblGcAuthMgrSclkSetData2(data, &request);
}

int sceSblGcAuthMgrAdhocBB224Init(void)
{
	return _sceSblGcAuthMgrAdhocBB224Init();
}

int sceSblGcAuthMgrAdhocBB224Shutdown(void)
{
	return _sceSblGcAuthMgrAdhocBB224Shutdown();
}

int sceSblGcAuthMgrAdhocBB224Auth1(struct AdhocBB224Context *context, void *output)
{
	struct AdhocAuthRequest request;

	request.dataSize = 208;
	return _sceSblGcAuthMgrAdhocBB224Auth1(context, output, &request);
}

int sceSblGcAuthMgrAdhocBB224Auth2(struct AdhocBB224Context *context, const void *input,
	void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 208;
	request.outputSize = 368;
	return _sceSblGcAuthMgrAdhocBB224Auth2(context, input, output, &request);
}

int sceSblGcAuthMgrAdhocBB224Auth3(struct AdhocBB224Context *context, const void *input,
	void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 368;
	request.outputSize = 304;
	return _sceSblGcAuthMgrAdhocBB224Auth3(context, input, output, &request);
}

int sceSblGcAuthMgrAdhocBB224Auth4(struct AdhocBB224Context *context, const void *input,
	void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 304;
	request.outputSize = 96;
	return _sceSblGcAuthMgrAdhocBB224Auth4(context, input, output, &request);
}

int sceSblGcAuthMgrAdhocBB224Auth5(struct AdhocBB224Context *context, const void *input)
{
	struct AdhocAuthRequest request;

	request.dataSize = 96;
	return _sceSblGcAuthMgrAdhocBB224Auth5(context, input, &request);
}

int sceSblGcAuthMgrAdhocBB224GetKeys(const struct AdhocBB224Context *context, void *firstKey,
	void *secondKey, void *thirdKey)
{
	struct AdhocKeysRequest request;

	request.thirdKey = thirdKey;
	request.firstKeySize = 16;
	request.secondKeySize = 16;
	request.thirdKeySize = 16;
	return _sceSblGcAuthMgrAdhocBB224GetKeys(context, firstKey, secondKey, &request);
}

int sceSblGcAuthMgrGetMediaIdType01(void *mediaId)
{
	struct AdhocAuthRequest request;

	request.dataSize = 32;
	return _sceSblGcAuthMgrGetMediaIdType01(mediaId, &request);
}

int sceSblGcAuthMgrAdhocBB160Init(void)
{
	return _sceSblGcAuthMgrAdhocBB160Init();
}

int sceSblGcAuthMgrAdhocBB160Shutdown(void)
{
	return _sceSblGcAuthMgrAdhocBB160Shutdown();
}

int sceSblGcAuthMgrAdhocBB160Auth1(struct AdhocBB160Context *context, void *output)
{
	struct AdhocAuthRequest request;

	request.dataSize = 160;
	return _sceSblGcAuthMgrAdhocBB160Auth1(context, output, &request);
}

int sceSblGcAuthMgrAdhocBB160Auth2(struct AdhocBB160Context *context, const void *input,
	void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 160;
	request.outputSize = 272;
	return _sceSblGcAuthMgrAdhocBB160Auth2(context, input, output, &request);
}

int sceSblGcAuthMgrAdhocBB160Auth3(struct AdhocBB160Context *context, const void *input,
	void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 272;
	request.outputSize = 256;
	return _sceSblGcAuthMgrAdhocBB160Auth3(context, input, output, &request);
}

int sceSblGcAuthMgrAdhocBB160Auth4(struct AdhocBB160Context *context, const void *input,
	void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 256;
	request.outputSize = 96;
	return _sceSblGcAuthMgrAdhocBB160Auth4(context, input, output, &request);
}

int sceSblGcAuthMgrAdhocBB160Auth5(struct AdhocBB160Context *context, const void *input)
{
	struct AdhocAuthRequest request;

	request.dataSize = 96;
	return _sceSblGcAuthMgrAdhocBB160Auth5(context, input, &request);
}

int sceSblGcAuthMgrAdhocBB160GetKeys(const struct AdhocBB160Context *context, void *firstKey,
	void *secondKey, void *thirdKey)
{
	struct AdhocKeysRequest request;

	request.thirdKey = thirdKey;
	request.firstKeySize = 16;
	request.secondKeySize = 16;
	request.thirdKeySize = 16;
	return _sceSblGcAuthMgrAdhocBB160GetKeys(context, firstKey, secondKey, &request);
}

int sceSblGcAuthMgrAdhocBB160UniCastEncrypt(struct AdhocBB160Context *context, void *data,
	SceSize size, void *authentication, SceUInt unknownArgument)
{
	struct AdhocUnicastEncryptRequest request;

	request.authentication = authentication;
	request.unknownArgument = unknownArgument;
	request.dataSize = size;
	request.authenticationSize = 32;
	return _sceSblGcAuthMgrAdhocBB160UniCastEncrypt(context, data, size, &request);
}

int sceSblGcAuthMgrAdhocBB160UniCastDecrypt(struct AdhocBB160Context *context, void *data,
	SceSize size, const void *authentication, SceUInt unknownFirstArgument,
	SceUInt unknownSecondArgument)
{
	struct AdhocDecryptRequest request;

	request.authentication = authentication;
	request.unknownFirstArgument = unknownFirstArgument;
	request.unknownSecondArgument = unknownSecondArgument;
	request.dataSize = size;
	request.authenticationSize = 32;
	return _sceSblGcAuthMgrAdhocBB160UniCastDecrypt(context, data, size, &request);
}

int sceSblGcAuthMgrAdhocBB160BroadCastEncrypt(void *data, SceSize size, void *authentication,
	SceUInt unknownArgument)
{
	struct AdhocBroadcastEncryptRequest request;

	request.unknownArgument = unknownArgument;
	request.dataSize = size;
	request.authenticationSize = 32;
	return _sceSblGcAuthMgrAdhocBB160BroadCastEncrypt(data, size, authentication, &request);
}

int sceSblGcAuthMgrAdhocBB160BroadCastDecrypt(struct AdhocBB160Context *context, void *data,
	SceSize size, const void *authentication, SceUInt unknownFirstArgument,
	SceUInt unknownSecondArgument)
{
	struct AdhocDecryptRequest request;

	request.authentication = authentication;
	request.unknownFirstArgument = unknownFirstArgument;
	request.unknownSecondArgument = unknownSecondArgument;
	request.dataSize = size;
	request.authenticationSize = 32;
	return _sceSblGcAuthMgrAdhocBB160BroadCastDecrypt(context, data, size, &request);
}

int sceSblGcAuthMgrMsSaveBBMacInit(struct MsSaveBBMacContext *context, SceUInt mode)
{
	return _sceSblGcAuthMgrMsSaveBBMacInit(context, mode);
}

int sceSblGcAuthMgrMsSaveBBMacUpdate(struct MsSaveBBMacContext *context, const void *data,
	SceSize size)
{
	return _sceSblGcAuthMgrMsSaveBBMacUpdate(context, data, size);
}

int sceSblGcAuthMgrMsSaveBBMacFinal(struct MsSaveBBMacContext *context, void *mac,
	const void *key)
{
	struct MsSaveBBMacRequest request;

	request.macSize = 16;
	request.keySize = 16;
	return _sceSblGcAuthMgrMsSaveBBMacFinal(context, mac, key, &request);
}

int sceSblGcAuthMgrMsSaveBBCipherInit(struct MsSaveBBCipherContext *context, SceUInt mode,
	SceUInt generationMode, void *data, const void *key)
{
	struct MsSaveBBCipherRequest request;

	request.data = data;
	request.key = key;
	request.dataSize = 16;
	request.keySize = 16;
	return _sceSblGcAuthMgrMsSaveBBCipherInit(context, mode, generationMode, &request);
}

int sceSblGcAuthMgrMsSaveBBCipherUpdate(struct MsSaveBBCipherContext *context, void *data,
	SceSize size)
{
	return _sceSblGcAuthMgrMsSaveBBCipherUpdate(context, data, size);
}

int sceSblGcAuthMgrMsSaveBBCipherFinal(struct MsSaveBBCipherContext *context)
{
	return _sceSblGcAuthMgrMsSaveBBCipherFinal(context);
}

int sceSblGcAuthMgrPsmactCreateC1(SceUInt mode, void *activation, void *response)
{
	struct PsmActivationRequest request;

	request.responseSize = 64;
	return _sceSblGcAuthMgrPsmactCreateC1(mode, activation, response, &request);
}

int sceSblGcAuthMgrPsmactVerifyR1(const void *activation, const void *response, void *output)
{
	struct PsmActivationRequest request;

	request.responseSize = 64;
	return _sceSblGcAuthMgrPsmactVerifyR1(activation, response, output, &request);
}
