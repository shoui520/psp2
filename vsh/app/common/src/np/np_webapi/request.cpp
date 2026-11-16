/*
 * SceNpWebApi: the request's small accessors (its reference count under the library context's lock,
 * its ID and the next request). The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiRequestRelease(NpWebApiRequest *request)
{
	NpWebApiLibContext *libCtx;

	npWebApiLibContextLock(request->libContext);
	libCtx = request->libContext;
	request->refCount--;
	return npWebApiLibContextUnlock(libCtx);
}

SceInt64 npWebApiRequestGetId(NpWebApiRequest *request)
{
	return request->requestId;
}

NpWebApiRequest *npWebApiRequestNext(NpWebApiRequest *request)
{
	return request->next;
}

int npWebApiRequestAddRef(NpWebApiRequest *request)
{
	NpWebApiLibContext *libCtx;

	npWebApiLibContextLock(request->libContext);
	libCtx = request->libContext;
	request->refCount++;
	return npWebApiLibContextUnlock(libCtx);
}

int npWebApiRequestHasOtherRefs(NpWebApiRequest *request)
{
	NpWebApiLibContext *libCtx;
	int others;

	npWebApiLibContextLock(request->libContext);
	others = request->refCount > 1;
	libCtx = request->libContext;
	npWebApiLibContextUnlock(libCtx);
	return others;
}
