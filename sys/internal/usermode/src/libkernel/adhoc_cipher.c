#include "libkernel_backends.h"
#include <scetypes.h>



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
