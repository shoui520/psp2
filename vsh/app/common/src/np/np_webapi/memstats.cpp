/*
 * SceNpWebApi: the memory pool statistics of a library context. The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiLibContextGetMemoryPoolStats(NpWebApiLibContext *ctx, SceNpWebApiMemoryPoolStats *stats)
{
	int result;

	result = 0;
	if (stats != NULL) {
		sceClibMemset(stats, 0, 16);
		result = npWebApiMspaceGetStats(ctx->heapBlock->mspace, (SceSize *)stats);
	}
	return result;
}

int npWebApiGetMemoryPoolStats(int libCtxId, SceNpWebApiMemoryPoolStats *stats)
{
	NpWebApiLibContext *libCtx;
	int result;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	if (libCtx != NULL) {
		result = npWebApiLibContextGetMemoryPoolStats(libCtx, stats);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}
