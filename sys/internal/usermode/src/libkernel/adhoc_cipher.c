#include "libkernel_backends.h"
#include <psp2/types.h>

struct AdhocBB160Context;

struct AdhocUnicastEncryptRequest {
	void *authentication;
	SceUInt unknownArgument;
	SceSize dataSize;
	SceSize authenticationSize;
	SceUInt reserved[2];
};

struct AdhocDecryptRequest {
	const void *authentication;
	SceUInt unknownFirstArgument;
	SceUInt unknownSecondArgument;
	SceSize dataSize;
	SceSize authenticationSize;
	SceUInt reserved;
};

struct AdhocBroadcastEncryptRequest {
	SceUInt unknownArgument;
	SceSize dataSize;
	SceSize authenticationSize;
	SceUInt reserved;
};


/* GcAuthMgr owns the cryptographic operations and copies the full requests. */
int SceLibKernel_A9002567(struct AdhocBB160Context *context, void *data,
		SceSize size, void *authentication, SceUInt unknownArgument)
{
	struct AdhocUnicastEncryptRequest request;

	request.authentication = authentication;
	request.unknownArgument = unknownArgument;
	request.dataSize = size;
	request.authenticationSize = 32;
	return _sceSblGcAuthMgrAdhocBB160UniCastEncrypt(context, data, size,
						    &request);
}

int SceLibKernel_01E00CBF(struct AdhocBB160Context *context, void *data,
		SceSize size, const void *authentication,
		SceUInt unknownFirstArgument, SceUInt unknownSecondArgument)
{
	struct AdhocDecryptRequest request;

	request.authentication = authentication;
	request.unknownFirstArgument = unknownFirstArgument;
	request.unknownSecondArgument = unknownSecondArgument;
	request.dataSize = size;
	request.authenticationSize = 32;
	return _sceSblGcAuthMgrAdhocBB160UniCastDecrypt(context, data, size,
						    &request);
}

int SceLibKernel_88E72157(void *data, SceSize size, void *authentication,
		SceUInt unknownArgument)
{
	struct AdhocBroadcastEncryptRequest request;

	request.unknownArgument = unknownArgument;
	request.dataSize = size;
	request.authenticationSize = 32;
	return _sceSblGcAuthMgrAdhocBB160BroadCastEncrypt(data, size,
						      authentication, &request);
}

int SceLibKernel_E2984A54(struct AdhocBB160Context *context, void *data,
		SceSize size, const void *authentication,
		SceUInt unknownFirstArgument, SceUInt unknownSecondArgument)
{
	struct AdhocDecryptRequest request;

	request.authentication = authentication;
	request.unknownFirstArgument = unknownFirstArgument;
	request.unknownSecondArgument = unknownSecondArgument;
	request.dataSize = size;
	request.authenticationSize = 32;
	return _sceSblGcAuthMgrAdhocBB160BroadCastDecrypt(context, data, size,
						      &request);
}
