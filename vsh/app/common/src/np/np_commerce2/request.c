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

extern SceUInt64 sceKernelGetSystemTimeWide(void);

/* the system time, in microseconds */
int npCommerce2GetSystemTime(SceUInt64 *time)
{
	*time = sceKernelGetSystemTimeWide();
	return 0;
}

/* deletes the connection of a template and clears it, so the next request connects anew */
int npCommerce2RenewConnection(int httpTemplate, int *httpConnection, int timeout)
{
	if (*httpConnection != 0)
		sceHttpDeleteConnection(*httpConnection);
	*httpConnection = 0;
	return 0;
}

/* puts the currency and country codes (three letters each) into a copy of the response header the
 * caller gives, which is at least 0x64 bytes long */
__attribute__((noinline))
int npCommerce2SetHeaderLocale(char *header, SceSize size, const char *currencyCode, const char *countryCode)
{
	char copy[0x64];
	int ret = 0;

	if (size >= 0x64) {
		sceClibMemcpy_safe(copy, header, 0x64);
		sceClibStrncpy(copy + 20, currencyCode, 4);
		copy[23] = '\0';
		sceClibStrncpy(copy + 24, countryCode, 4);
		copy[27] = '\0';
		sceClibMemcpy_safe(header, copy, 0x64);
	} else {
		ret = SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
	}
	return ret;
}

/* puts the length of the data (plus the four bytes before it) and a status into the response header the
 * caller gives */
__attribute__((noinline))
int npCommerce2SetHeaderStatus(char *header, SceSize size, SceUInt32 dataLength, SceUInt32 status)
{
	char copy[0x64];
	int ret = 0;

	if (size >= 0x64) {
		sceClibMemcpy_safe(copy, header, 0x64);
		*(SceUInt32 *)(copy + 8) = dataLength + 4;
		*(SceUInt32 *)(copy + 12) = status;
		sceClibMemcpy_safe(header, copy, 0x64);
	} else {
		ret = SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
	}
	return ret;
}

/* what a request's response gives the context: the currency and country codes (also put into the
 * response's header) of the kinds of response that carry them, and the response itself, kept when it fits */
int npCommerce2StoreResponse(NpCommerce2Context *ctx, NpCommerce2Request *req, void *buf, SceSize bufSize,
			     SceSize length)
{
	NpCommerce2Item header;
	NpCommerce2CountsResult counts;
	NpCommerce2ValuesItem values;
	SceUInt16 type = req->type;

	if (type == 66 || type == 1) {
		if (npCommerce2InitResult0x16(&header, (const char *)buf, length) >= 0 &&
		    npCommerce2ReadValuesItem0x5e(&header, &values) >= 0)
			goto copyCodes;
	} else if (type == 49) {
		if (npCommerce2InitCountsResult0x16(&counts, (const char *)buf, length) >= 0 &&
		    npCommerce2ReadValuesItem0x5e((const NpCommerce2Item *)&counts, &values) >= 0) {
copyCodes:
			sceClibStrncpy(ctx->currencyCode, values.textB, 4);
			ctx->currencyCode[3] = '\0';
			sceClibStrncpy(ctx->countryCode, values.textC, 4);
			ctx->countryCode[3] = '\0';
			npCommerce2SetHeaderLocale((char *)buf, bufSize, ctx->currencyCode, ctx->countryCode);
		}
	}
	type = req->type;
	if (type == 66 || type == 61 || type == 49 || type == 29 || type == 1) {
		if (length <= 0x400) {
			ctx->responseSize = length;
			sceClibMemcpy_safe(ctx->response, buf, length);
		} else {
			ctx->responseSize = 0;
		}
	}
	return 0;
}
