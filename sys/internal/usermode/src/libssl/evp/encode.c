/* SSLeay base64 codec with measured SSL-C changes in encode_sce.patch. */
#include "evp.h"
#include "../memory.h"

#define Malloc sslAllocate
#define Free sslFree
#define memcpy sslCopyMemory
#define memset sslFillMemory
#include "encode_sce.inc"

EVP_ENCODE_CTX *sslNewEncodeContext(void)
{
	EVP_ENCODE_CTX *ret;

	ret = (EVP_ENCODE_CTX *)Malloc(sizeof(EVP_ENCODE_CTX));
	if (ret == NULL)
		return NULL;
	memset(ret, 0, sizeof(EVP_ENCODE_CTX));
	return ret;
}

void sslFreeEncodeContext(EVP_ENCODE_CTX *ctx)
{
	Free(ctx);
}

int sslGetEncodeNewlineFlags(EVP_ENCODE_CTX *ctx)
{
	return ctx->newlineFlags;
}

void sslSetEncodeNewlineFlags(EVP_ENCODE_CTX *ctx, int flags)
{
	ctx->newlineFlags = flags;
}

void sslEncodeUpdateAndFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
			     unsigned char *in, int inl)
{
	EVP_EncodeUpdate(ctx, out, outl, in, inl);
	EVP_EncodeFinal(ctx, out, outl);
}

int sslDecodeUpdateAndFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
			    unsigned char *in, int inl)
{
	if (EVP_DecodeUpdate(ctx, out, outl, in, inl) == -1)
		return -1;
	return EVP_DecodeFinal(ctx, out, outl);
}
