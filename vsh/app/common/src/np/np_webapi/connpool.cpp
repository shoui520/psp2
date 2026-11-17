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
