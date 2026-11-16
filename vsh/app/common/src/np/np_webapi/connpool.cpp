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
			npWebApiLibContextDestroyPool(pool->libContext);
			node = pool->head;
			conn = next;
		} while (next != NULL);
	}
	pool->count = 0;
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
