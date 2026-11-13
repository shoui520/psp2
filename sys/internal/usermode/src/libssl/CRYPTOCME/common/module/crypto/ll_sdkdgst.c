/* Filename and descriptive names are ours: the outer implementation of
 * CRYPTOCME's SHA-2 digests, which hands the data straight to the SDK's
 * block functions (their own buffering) in the inner state.
 */
#include "ll_obj.h"

static int sslInitSha256Sdk(SslDigestContext *context)
{
	return context->descriptor->innerImplementation->Init(
		context->innerState);
}

static int sslFinalSha256Sdk(SslDigestContext *context,
			     unsigned char *digest)
{
	return context->descriptor->innerImplementation->Output(
		context->innerState, digest);
}

static int sslUpdateSha256Sdk(SslDigestContext *context, const void *data,
			      int length)
{
	if (length <= 0)
		return 0;
	return context->descriptor->innerImplementation->Compress(
		context->innerState, data, length);
}

static int sslInitSha512Sdk(SslDigestContext *context)
{
	return context->descriptor->innerImplementation->Init(
		context->innerState);
}

static int sslFinalSha512Sdk(SslDigestContext *context,
			     unsigned char *digest)
{
	return context->descriptor->innerImplementation->Output(
		context->innerState, digest);
}

static int sslUpdateSha512Sdk(SslDigestContext *context, const void *data,
			      int length)
{
	if (length <= 0)
		return 0;
	return context->descriptor->innerImplementation->Compress(
		context->innerState, data, length);
}

const SslDigestImplementation sslSha256SdkImplementation = {
	sslInitSha256Sdk,
	sslUpdateSha256Sdk,
	sslFinalSha256Sdk,
	NULL,
	sslTransferSha256State,
	4,
	0
};

const SslDigestImplementation sslSha512SdkImplementation = {
	sslInitSha512Sdk,
	sslUpdateSha512Sdk,
	sslFinalSha512Sdk,
	NULL,
	sslTransferSha512State,
	4,
	0
};
