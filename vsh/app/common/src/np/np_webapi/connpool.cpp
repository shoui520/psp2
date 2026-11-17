/*
 * SceNpWebApi: a user context's pool of HTTP connections. The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiDestroyObject(void *object)
{
	if (object != NULL)
		return SceNpCommonPs4_CE924EB3(1, object);
	return 0;
}

int npWebApiConnectionPoolCloseAll(NpWebApiConnectionPool *pool)
{
	NpWebApiConnection *conn;
	NpWebApiConnection *next;
	NpWebApiConnection *node;
	NpWebApiConnection *prev;

	conn = pool->head;
	if (conn != NULL) {
		node = conn;
		do {
			next = conn->next;
			prev = NULL;
			if (node != NULL) {
				prev = NULL;
				do {
					if (node == conn)
						break;
					prev = node;
					node = node->next;
				} while (node != NULL);
			}
			if (node != NULL) {
				if (prev != NULL)
					prev->next = node->next;
				else
					pool->head = node->next;
				pool->count--;
			} else {
				npWebApiLibContextUnlock(pool->libContext);
			}
			if (conn->connId != 0) {
				sceHttpDeleteConnection(conn->connId);
				conn->connId = 0;
			}
			npWebApiLibContextDestroyPool(pool->libContext, conn);
			node = pool->head;
			conn = next;
		} while (next != NULL);
	}
	pool->count = 0;
	return 0;
}

int npWebApiConnectionPoolDeleteConnection(NpWebApiConnectionPool *pool, const char *name)
{
	NpWebApiConnection *conn;
	NpWebApiConnection *node;
	NpWebApiConnection *prev;

	if (name == NULL)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	npWebApiLibContextLock(pool->libContext);
	conn = pool->head;
	if (conn != NULL) {
		do {
			if (sceClibStrncmp(conn->name, name, sceClibStrnlen(name, 0xFFFFFFFF)) == 0)
				break;
			conn = conn->next;
		} while (conn != NULL);
		if (conn != NULL) {
			node = pool->head;
			prev = NULL;
			if (node != NULL) {
				while (node != conn) {
					prev = node;
					node = node->next;
					if (node == NULL)
						goto notFound;
				}
			}
			if (node != NULL) {
				if (prev != NULL)
					prev->next = node->next;
				else
					pool->head = node->next;
				pool->count--;
			} else {
notFound:
				npWebApiLibContextUnlock(pool->libContext);
			}
			if (conn->connId != 0) {
				sceHttpDeleteConnection(conn->connId);
				conn->connId = 0;
			}
			npWebApiLibContextDestroyPool(pool->libContext, conn);
		}
	}
	npWebApiLibContextUnlock(pool->libContext);
	return 0;
}

NpWebApiConnectionPool *npWebApiConnectionPoolConstruct(NpWebApiConnectionPool *pool, NpWebApiUserContext *userCtx)
{
	pool->vtable = g_connectionPoolVtable;
	npWebApiUserContextGetId(userCtx);
	pool->libContext = npWebApiUserContextGetLibContext(userCtx);
	pool->userContext = userCtx;
	pool->head = NULL;
	pool->count = 0;
	return pool;
}

NpWebApiConnectionPool *npWebApiConnectionPoolDestruct(NpWebApiConnectionPool *pool)
{
	NpWebApiLibContext *libCtx;

	libCtx = pool->libContext;
	pool->vtable = g_connectionPoolVtable;
	npWebApiLibContextLock(libCtx);
	npWebApiConnectionPoolCloseAll(pool);
	npWebApiLibContextUnlock(pool->libContext);
	pool->vtable = g_baseVtable;
	return pool;
}

int npWebApiConnectionPoolDeleteThis(NpWebApiConnectionPool *pool)
{
	NpWebApiLibContext *libCtx;

	libCtx = pool->libContext;
	pool->vtable = g_connectionPoolVtable;
	npWebApiLibContextLock(libCtx);
	npWebApiConnectionPoolCloseAll(pool);
	npWebApiLibContextUnlock(pool->libContext);
	pool->vtable = g_baseVtable;
	return SceNpCommonPs4_BB5282C8(pool);
}

/* stops what a request of this pool is waiting for, by what it waits on */
int npWebApiConnectionPoolAbortRequest(NpWebApiConnectionPool *pool, NpWebApiRequest *request,
	unsigned char flag)
{
	int result;

	result = 0;
	npWebApiLibContextLock(pool->libContext);
	if (npWebApiRequestGetField236(request) != 0) {
		if (flag != 0)
			result = SceNpCommonPs4_9AC24306();
		else
			result = SceNpCommonPs4_68D08C92();
	}
	if (npWebApiRequestGetField228(request) > 0)
		result = SceNpManager_737B2820();
	npWebApiLibContextUnlock(pool->libContext);
	return result;
}

int npWebApiPushObjectAbortRequest(NpWebApiPushObject *object, NpWebApiRequest *request)
{
	int result;

	result = 0;
	npWebApiLibContextLock(object->libContext);
	if (npWebApiRequestGetField236(request) != 0)
		result = SceNpCommonPs4_68D08C92(npWebApiRequestGetField236(request));
	npWebApiLibContextUnlock(object->libContext);
	return result;
}

/* the connection of the user context to a server (the one named, made on the first use) */
int npWebApiConnectionPoolGetConnection(NpWebApiConnectionPool *pool, const char *name, const char *url, int *pConnId)
{
	NpWebApiConnection *conn;
	NpWebApiConnection *node;
	NpWebApiLibContext *libCtx;
	int result;
	int created;

	result = 0;
	if (name == NULL)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	npWebApiLibContextLock(pool->libContext);
	for (conn = pool->head; conn != NULL; conn = conn->next) {
		if (sceClibStrncmp(conn->name, name, sceClibStrnlen(name, 0xFFFFFFFF)) == 0)
			break;
	}
	libCtx = pool->libContext;
	if (conn == NULL) {
		node = (NpWebApiConnection *)npWebApiLibContextPoolAlloc(libCtx, 72);
		if (node != NULL) {
			sceClibMemset(node, 0, 72);
			sceClibStrncpy(node->name, name, sceClibStrnlen(name, 0xFFFFFFFF));
			created = sceHttpCreateConnectionWithURL(npWebApiUserContextGetHttpTemplateId(pool->userContext), url, 1);
			if (created >= 0) {
				node->connId = created;
				conn = node;
				goto link;
			}
			result = created;
		} else {
			result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		}
		if (node != NULL)
			npWebApiLibContextDestroyPool(pool->libContext, node);
		goto fail;
link:
		if (result < 0)
			return result;
		if (pool->head != NULL)
			conn->next = pool->head;
		pool->head = conn;
		pool->count++;
		libCtx = pool->libContext;
		goto out;
fail:
		if (result < 0)
			return result;
	}
out:
	npWebApiLibContextUnlock(libCtx);
	if (pConnId != NULL)
		*pConnId = conn->connId;
	return result;
}

/* closes the connections of a pool under its library context's lock */
int npWebApiConnectionPoolCloseAllLocked(NpWebApiConnectionPool *pool)
{
	npWebApiLibContextLock(pool->libContext);
	npWebApiConnectionPoolCloseAll(pool);
	return npWebApiLibContextUnlock(pool->libContext);
}

/* closes the connections of every user context of every library context */
int npWebApiCloseAllConnections(void)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	int result;

	npWebApiGlobalLock();
	result = (int)&g_libContexts;
	for (libCtx = g_libContexts; libCtx != NULL; libCtx = libCtx->next) {
		SceNpCommonPs4_F5840D6E(&libCtx->lock);
		for (userCtx = libCtx->userContextHead; userCtx != NULL; userCtx = npWebApiUserContextNext(userCtx)) {
			if (npWebApiUserContextRequests(userCtx) != NULL)
				npWebApiConnectionPoolCloseAllLocked((NpWebApiConnectionPool *)npWebApiUserContextRequests(userCtx));
		}
		result = SceNpCommonPs4_4A1C1C3C(&libCtx->lock);
	}
	return npWebApiGlobalUnlock();
}

const void *g_connectionPoolVtable[] = {	/* 0x8100eb7c */
	(const void *)npWebApiConnectionPoolDestruct,
	(const void *)npWebApiConnectionPoolDeleteThis,
	NULL,
	NULL,
};
