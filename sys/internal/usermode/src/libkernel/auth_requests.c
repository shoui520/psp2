#include "libkernel_backends.h"
#include <psp2/types.h>

struct PackageVerifyRequest {
	SceSize firstSize;
	SceSize secondSize;
	SceUInt reserved[2];
};

struct SecureClockRequest {
	SceSize size;
	SceUInt reserved;
};

struct PcActivationChallengeRequest {
	SceSize inputSize;
	SceSize outputSize;
	SceUInt reserved[2];
};

struct PsmActivationRequest {
	SceSize responseSize;
	SceUInt reserved;
};

struct AdhocBB224Context;
struct AdhocBB160Context;

struct AdhocAuthRequest {
	SceSize dataSize;
	SceUInt reserved;
};

struct AdhocAuthExchangeRequest {
	SceSize inputSize;
	SceSize outputSize;
	SceUInt reserved[2];
};

struct AdhocKeysRequest {
	void *thirdKey;
	SceSize firstKeySize;
	SceSize secondKeySize;
	SceSize thirdKeySize;
	SceUInt reserved[2];
};


/* GcAuthMgr copies the complete requests; reserved words remain unwritten. */
int SceLibKernel_12C7CD2B(const void *first, const void *second)
{
	struct PackageVerifyRequest request;

	request.firstSize = 20;
	request.secondSize = 40;
	return _sceSblGcAuthMgrPkgVry(first, second, &request);
}

int SceLibKernel_93A6570E(void *data)
{
	struct SecureClockRequest request;

	request.size = 48;
	return _sceSblGcAuthMgrSclkGetData1(data, &request);
}

int SceLibKernel_E088B0D0(const void *data)
{
	struct SecureClockRequest request;

	request.size = 304;
	return _sceSblGcAuthMgrSclkSetData2(data, &request);
}

int SceLibKernel_C8082804(void)
{
	return _sceSblGcAuthMgrAdhocBB224Init();
}

int SceLibKernel_9B28E1AF(void)
{
	return _sceSblGcAuthMgrAdhocBB160Init();
}

int SceLibKernel_9557D15C(SceUInt mode, const void *input, void *output)
{
	struct PcActivationChallengeRequest request;

	request.inputSize = 32;
	request.outputSize = 128;
	return _sceSblGcAuthMgrPcactGetChallenge(mode, input, output, &request);
}

int sceSblGcAuthMgrPcactActivation(const void *activation,
		SceUInt unknownArgument)
{
	return _sceSblGcAuthMgrPcactActivation(activation, unknownArgument);
}

int SceLibKernel_622A81E6(SceUInt mode, void *activation, void *response)
{
	struct PsmActivationRequest request;

	request.responseSize = 64;
	return _sceSblGcAuthMgrPsmactCreateC1(mode, activation, response, &request);
}

int SceLibKernel_EB6DA895(const void *activation, const void *response,
		void *output)
{
	struct PsmActivationRequest request;

	request.responseSize = 64;
	return _sceSblGcAuthMgrPsmactVerifyR1(activation, response, output, &request);
}

int SceLibKernel_211BEDE8(struct AdhocBB224Context *context, void *output)
{
	struct AdhocAuthRequest request;

	request.dataSize = 208;
	return _sceSblGcAuthMgrAdhocBB224Auth1(context, output, &request);
}

int SceLibKernel_EFD76235(struct AdhocBB160Context *context, void *output)
{
	struct AdhocAuthRequest request;

	request.dataSize = 160;
	return _sceSblGcAuthMgrAdhocBB160Auth1(context, output, &request);
}

int SceLibKernel_3B595E1D(struct AdhocBB160Context *context, const void *input)
{
	struct AdhocAuthRequest request;

	request.dataSize = 96;
	return _sceSblGcAuthMgrAdhocBB160Auth5(context, input, &request);
}

int SceLibKernel_DEAD6277(struct AdhocBB224Context *context,
		const void *input, void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 208;
	request.outputSize = 368;
	return _sceSblGcAuthMgrAdhocBB224Auth2(context, input, output, &request);
}

int SceLibKernel_104D802F(struct AdhocBB224Context *context,
		const void *input, void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 368;
	request.outputSize = 304;
	return _sceSblGcAuthMgrAdhocBB224Auth3(context, input, output, &request);
}

int SceLibKernel_65126005(struct AdhocBB224Context *context,
		const void *input, void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 304;
	request.outputSize = 96;
	return _sceSblGcAuthMgrAdhocBB224Auth4(context, input, output, &request);
}

int SceLibKernel_4AC7EFC9(struct AdhocBB224Context *context, const void *input)
{
	struct AdhocAuthRequest request;

	request.dataSize = 96;
	return _sceSblGcAuthMgrAdhocBB224Auth5(context, input, &request);
}

int SceLibKernel_1F4DF829(struct AdhocBB160Context *context,
		const void *input, void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 160;
	request.outputSize = 272;
	return _sceSblGcAuthMgrAdhocBB160Auth2(context, input, output, &request);
}

int SceLibKernel_C362ECD6(struct AdhocBB160Context *context,
		const void *input, void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 272;
	request.outputSize = 256;
	return _sceSblGcAuthMgrAdhocBB160Auth3(context, input, output, &request);
}

int SceLibKernel_6314CAA3(struct AdhocBB160Context *context,
		const void *input, void *output)
{
	struct AdhocAuthExchangeRequest request;

	request.inputSize = 256;
	request.outputSize = 96;
	return _sceSblGcAuthMgrAdhocBB160Auth4(context, input, output, &request);
}

int SceLibKernel_AC57B6A4(const struct AdhocBB224Context *context,
		void *firstKey, void *secondKey, void *thirdKey)
{
	struct AdhocKeysRequest request;

	request.thirdKey = thirdKey;
	request.firstKeySize = 16;
	request.secondKeySize = 16;
	request.thirdKeySize = 16;
	return _sceSblGcAuthMgrAdhocBB224GetKeys(context, firstKey, secondKey,
					     &request);
}

int SceLibKernel_84C75DC3(const struct AdhocBB160Context *context,
		void *firstKey, void *secondKey, void *thirdKey)
{
	struct AdhocKeysRequest request;

	request.thirdKey = thirdKey;
	request.firstKeySize = 16;
	request.secondKeySize = 16;
	request.thirdKeySize = 16;
	return _sceSblGcAuthMgrAdhocBB160GetKeys(context, firstKey, secondKey,
					     &request);
}

int SceLibKernel_F7027E6A(void *mediaId)
{
	struct AdhocAuthRequest request;

	request.dataSize = 32;
	return _sceSblGcAuthMgrGetMediaIdType01(mediaId, &request);
}
