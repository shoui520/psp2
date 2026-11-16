/*
 * SceNpCommerce2: the checks before a request runs.
 */

#include "np_commerce2_internal.h"

/* whether the request may run: the memory the SSL and HTTP pools have left is enough for it, and the
 * account can take it (requests of the shell's modes and of kind 2 skip the account checks) */
int npCommerce2CheckRequest(NpCommerce2Context *ctx, NpCommerce2Request *req)
{
	SceSslMemoryPoolStats sslStats;
	SceHttpMemoryPoolStats httpStats;
	int created = 0;
	int signedIn;
	SceUInt32 mode = ctx->mode;
	SceUInt32 freeSize;
	int skip;
	int ret;

	if (mode > 6) {
		if (mode <= 8)
			goto skipChecks;
	} else if (mode > 4) {
skipChecks:
		skip = 1;
		ret = 0;
		goto checked;
	}
	if (req->type == 2) {
		skip = 1;
		ret = 0;
		goto checked;
	}
	if (SceNpManager_B24FC028(&signedIn) < 0 || signedIn == 0) {
		skip = 1;
		ret = 0;
		goto checked;
	}
	skip = 0;
	if (SceLibKernel_9F793F84() < 0x2000000) {
		ret = 0;
		goto checked;
	}
	ret = SceNpManager_A775A452();
	if (ret >= 0) {
		if (ret != 0) {
			ret = 0;
		} else {
			ret = NP_COMMERCE2_ERROR_NP_NOT_READY;
		}
	}
checked:
	if (ret < 0 || skip == 1)
		return ret;
	sceHttpSslIsCtxCreated(ctx->httpConnection, &created);
	ctx->sslCreated = created;
	ret = sceSslGetMemoryPoolStats(&sslStats);
	if (ret < 0)
		return ret;
	freeSize = sslStats.poolSize - sslStats.currentInuseSize;
	if (created == 1) {
		if (freeSize < 0xC800) {
			ctx->sslShortfall = freeSize - 51200;
			return NP_COMMERCE2_ERROR_SSL_MEMORY;
		}
	} else if (freeSize < 0x18000) {
		ctx->sslShortfall = freeSize - 98304;
		return NP_COMMERCE2_ERROR_SSL_MEMORY;
	}
	ret = sceHttpGetMemoryPoolStats(&httpStats);
	if (ret >= 0) {
		freeSize = httpStats.poolSize - httpStats.currentInuseSize;
		if (created == 1) {
			if (freeSize < 0x8000) {
				ctx->httpShortfall = freeSize - 0x8000;
				ret = NP_COMMERCE2_ERROR_HTTP_MEMORY;
			}
		} else if (freeSize < 0x9000) {
			ctx->httpShortfall = freeSize - 36864;
			ret = NP_COMMERCE2_ERROR_HTTP_MEMORY;
		}
	}
	return ret;
}
