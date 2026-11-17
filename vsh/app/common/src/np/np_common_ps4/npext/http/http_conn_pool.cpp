/*
 * SceNpCommonPs4: HttpConnPool (npext/http/http_conn_pool.cpp), a list of HTTP connections kept for
 * reuse. A transaction takes a connection that matches its scheme, host and port, or the pool makes
 * one (and drops an idle one when it is full).
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_httpConnVtable[];		/* 0x8102ab38 */
extern const void *g_httpConnPoolVtable[];	/* 0x8102ab48 */

struct NpCommonPs4HttpConnPool *npCommonPs4HttpConnPoolConstruct(struct NpCommonPs4HttpConnPool *connPool,
	struct NpCommonPs4AllocatorWithError *pool)
{
	connPool->pool = pool;
	connPool->vtable = NP_COMMON_PS4_VTABLE(g_httpConnPoolVtable);
	connPool->connNumMax = 0;
	connPool->connNum = 0;
	SceNpCommonPs4_4266D870(&connPool->mutex);
	connPool->head = NULL;
	connPool->tail = NULL;
	return connPool;
}

__attribute__((noinline)) struct NpCommonPs4HttpConnPool *npCommonPs4HttpConnPoolDestruct(struct NpCommonPs4HttpConnPool *connPool)
{
	struct NpCommonPs4HttpConn *conn;
	const void *const *vtable;

	connPool->vtable = NP_COMMON_PS4_VTABLE(g_httpConnPoolVtable);
	for (conn = connPool->head; conn != NULL; conn = connPool->head) {
		if (conn->next != NULL)
			conn->next->prev = conn->prev;
		else
			connPool->tail = conn->prev;
		if (conn->prev != NULL)
			conn->prev->next = conn->next;
		else
			connPool->head = conn->next;
		vtable = conn->vtable;
		conn->next = NULL;
		conn->prev = NULL;
		((void (*)(struct NpCommonPs4HttpConn *))vtable[1])(conn);
	}
	SceNpCommonPs4_294F0B1A(&connPool->mutex);
	connPool->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return connPool;
}

int npCommonPs4HttpConnPoolDelete(struct NpCommonPs4HttpConnPool *connPool)
{
	npCommonPs4HttpConnPoolDestruct(connPool);
	return SceNpCommonPs4_BB5282C8(connPool);
}

int npCommonPs4HttpConnPoolInit(struct NpCommonPs4HttpConnPool *connPool, SceInt32 connNumMax)
{
	int result;

	if (connNumMax > 0) {
		result = SceNpCommonPs4_8AA7E995(&connPool->mutex, "SceNpHttpConnPool", 0);
		if (result < 0) {
			SceNpCommonPs4_63EED478(&connPool->mutex);
			return result;
		}
		connPool->connNumMax = connNumMax;
		connPool->connNum = 0;
		connPool->head = NULL;
		connPool->tail = NULL;
		return 0;
	}
	NP_COMMON_PS4_ASSERT(connNumMax > 0, "Init", "npext\\http\\http_conn_pool.cpp", 161, "connNumMax > 0");
	return NP_COMMON_PS4_ERROR_80559E11;
}

int npCommonPs4HttpConnInit(struct NpCommonPs4HttpConn *conn, SceInt32 templateId, const char *scheme,
	const char *serverName, SceUShort16 port)
{
	SceInt32 connId;
	char *schemeCopy;
	char *hostCopy;
	int result;

	if (conn->scheme != NULL)
		NP_COMMON_PS4_ASSERT(0, "Init", "npext\\http\\http_conn_pool.cpp", 50, "m_scheme == 0");
	if (conn->host != NULL)
		NP_COMMON_PS4_ASSERT(0, "Init", "npext\\http\\http_conn_pool.cpp", 51, "m_host == 0");
	hostCopy = NULL;
	result = sceHttpCreateConnection(templateId, serverName, scheme, port, 1);
	if (result >= 0) {
		connId = result;
		schemeCopy = npCommonPs4AllocatorDuplicateString((struct NpCommonPs4Allocator *)conn->pool, scheme);
		if (schemeCopy == NULL) {
			result = conn->pool->error;
		} else {
			hostCopy = npCommonPs4AllocatorDuplicateString((struct NpCommonPs4Allocator *)conn->pool, serverName);
			if (hostCopy == NULL) {
				result = conn->pool->error;
			} else {
				conn->connId = connId;
				conn->templateId = templateId;
				conn->scheme = schemeCopy;
				conn->host = hostCopy;
				conn->port = port;
				conn->inUse = 0;
				conn->invalid = 0;
				conn->reportedError = 0;
				conn->next = NULL;
				conn->prev = NULL;
				return 0;
			}
		}
		if (schemeCopy != NULL)
			SceNpCommonPs4_38CC9AF5((struct NpCommonPs4Allocator *)conn->pool, schemeCopy);
		if (hostCopy != NULL)
			SceNpCommonPs4_38CC9AF5((struct NpCommonPs4Allocator *)conn->pool, hostCopy);
		if (connId != 0)
			sceHttpDeleteConnection(connId);
	}
	return result;
}

struct NpCommonPs4HttpConn *npCommonPs4HttpConnDestruct(struct NpCommonPs4HttpConn *conn)
{
	conn->vtable = NP_COMMON_PS4_VTABLE(g_httpConnVtable);
	if (conn->scheme != NULL)
		SceNpCommonPs4_38CC9AF5((struct NpCommonPs4Allocator *)conn->pool, conn->scheme);
	if (conn->host != NULL)
		SceNpCommonPs4_38CC9AF5((struct NpCommonPs4Allocator *)conn->pool, conn->host);
	if (conn->connId != 0)
		sceHttpDeleteConnection(conn->connId);
	conn->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return conn;
}

int npCommonPs4HttpConnDelete(struct NpCommonPs4HttpConn *conn)
{
	npCommonPs4HttpConnDestruct(conn);
	return SceNpCommonPs4_BB5282C8(conn);
}

int npCommonPs4HttpConnPoolGetConnection(struct NpCommonPs4HttpConnPool *connPool, struct NpCommonPs4HttpTemplate *tmpl,
	const char *scheme, const char *serverName, SceUShort16 port, SceInt32 *pConnId)
{
	struct NpCommonPs4HttpConn *conn;
	struct NpCommonPs4HttpConn *idle;
	SceInt32 connId;
	int result;

	SceNpCommonPs4_A2A8493B(&connPool->mutex);
	for (conn = connPool->head; conn != NULL; conn = conn->next) {
		if (conn->inUse == 0 && conn->templateId == tmpl->templateId &&
		    sceClibStrncmp(conn->scheme, scheme, sceClibStrnlen(scheme, 0xFFFFFFFF) + 1) == 0 &&
		    sceClibStrncmp(conn->host, serverName, sceClibStrnlen(serverName + 1, 0xFFFFFFFF)) == 0 &&
		    conn->port == port)
			break;
	}
	if (conn != NULL) {
		if (conn->invalid != 0)
			NP_COMMON_PS4_ASSERT(0, "GetConnection", "npext\\http\\http_conn_pool.cpp", 328, "!conn->IsInvalid()");
		connId = conn->connId;
		conn->inUse = 1;
		goto done;
	}
	if (connPool->connNum >= connPool->connNumMax) {
		for (idle = connPool->head; idle != NULL; idle = idle->next) {
			if (idle->inUse == 0)
				break;
		}
		if (idle == NULL) {
			result = NP_COMMON_PS4_ERROR_80559E12;
			goto unlock;
		}
		if (idle->next != NULL)
			idle->next->prev = idle->prev;
		else
			connPool->tail = idle->prev;
		if (idle->prev != NULL)
			idle->prev->next = idle->next;
		else
			connPool->head = idle->next;
		idle->next = NULL;
		idle->prev = NULL;
		--connPool->connNum;
		((void (*)(struct NpCommonPs4HttpConn *))idle->vtable[1])(idle);
	}
	conn = NULL;
	idle = (struct NpCommonPs4HttpConn *)SceNpCommonPs4_D154D84F(sizeof(struct NpCommonPs4HttpConn),
		(struct NpCommonPs4Allocator *)connPool->pool);
	if (idle != NULL) {
		idle->pool = connPool->pool;
		idle->vtable = NP_COMMON_PS4_VTABLE(g_httpConnVtable);
		idle->connId = 0;
		idle->templateId = 0;
		idle->scheme = NULL;
		idle->host = NULL;
		idle->port = 0;
		idle->inUse = 0;
		idle->invalid = 0;
		idle->reportedError = 0;
		idle->next = NULL;
		idle->prev = NULL;
		conn = idle;
		result = npCommonPs4HttpConnInit(conn, tmpl->templateId, scheme, serverName, port);
		if (result >= 0) {
			conn->inUse = 1;
			conn->next = NULL;
			conn->prev = connPool->tail;
			if (connPool->tail != NULL)
				connPool->tail->next = conn;
			connId = connPool->connNum;
			if (connPool->head == NULL)
				connPool->head = conn;
			connPool->tail = conn;
			connPool->connNum = connId + 1;
			connId = conn->connId;
done:
			*pConnId = connId;
			SceNpCommonPs4_6BA5A164(&connPool->mutex);
			return 0;
		}
	} else {
		result = NP_COMMON_PS4_ERROR_80559E09;
	}
	if (conn != NULL)
		((void (*)(struct NpCommonPs4HttpConn *))conn->vtable[1])(conn);
unlock:
	SceNpCommonPs4_6BA5A164(&connPool->mutex);
	return result;
}

int npCommonPs4HttpConnPoolGetConnectionWithUrl(struct NpCommonPs4HttpConnPool *connPool,
	struct NpCommonPs4HttpTemplate *tmpl, const char *url, SceInt32 *pConnId)
{
	struct NpCommonPs4HttpUri uri;
	int result;
	int status;

	SceNpCommonPs4_EF053155(&uri, (struct NpCommonPs4Allocator *)connPool->pool);
	result = SceNpCommonPs4_A9EAFF5E(&uri, url);
	if (result >= 0) {
		status = npCommonPs4HttpConnPoolGetConnection(connPool, tmpl, uri.scheme, uri.hostname, uri.port, pConnId);
		SceNpCommonPs4_349710F4(&uri);
		if (status >= 0)
			return 0;
		return status;
	}
	SceNpCommonPs4_349710F4(&uri);
	return result;
}

int npCommonPs4HttpConnPoolRelease(struct NpCommonPs4HttpConnPool *connPool, SceInt32 connId)
{
	struct NpCommonPs4HttpConn *conn;
	const void *const *vtable;

	SceNpCommonPs4_A2A8493B(&connPool->mutex);
	for (conn = connPool->head; conn != NULL; conn = conn->next) {
		if (conn->connId == connId)
			break;
	}
	if (conn != NULL) {
		if (conn->invalid != 0) {
			if (conn->next != NULL)
				conn->next->prev = conn->prev;
			else
				connPool->tail = conn->prev;
			if (conn->prev != NULL)
				conn->prev->next = conn->next;
			else
				connPool->head = conn->next;
			vtable = conn->vtable;
			conn->next = NULL;
			--connPool->connNum;
			conn->prev = NULL;
			((void (*)(struct NpCommonPs4HttpConn *))vtable[1])(conn);
		} else {
			conn->inUse = 0;
		}
		SceNpCommonPs4_6BA5A164(&connPool->mutex);
		return 0;
	}
	SceNpCommonPs4_6BA5A164(&connPool->mutex);
	return NP_COMMON_PS4_ERROR_80559E13;
}

int npCommonPs4HttpConnPoolReportError(struct NpCommonPs4HttpConnPool *connPool, SceInt32 connId, int error)
{
	struct NpCommonPs4HttpConn *conn;

	SceNpCommonPs4_A2A8493B(&connPool->mutex);
	for (conn = connPool->head; conn != NULL; conn = conn->next) {
		if (conn->connId == connId)
			break;
	}
	if (conn != NULL) {
		conn->reportedError = error;
		SceNpCommonPs4_6BA5A164(&connPool->mutex);
		return 0;
	}
	SceNpCommonPs4_6BA5A164(&connPool->mutex);
	return NP_COMMON_PS4_ERROR_80559E13;
}

const void *g_httpConnVtable[] = {		/* 0x8102ab38 */
	NULL,
	NULL,
	(const void *)npCommonPs4HttpConnDestruct,
	(const void *)npCommonPs4HttpConnDelete,
};

const void *g_httpConnPoolVtable[] = {		/* 0x8102ab48 */
	NULL,
	NULL,
	(const void *)npCommonPs4HttpConnPoolDestruct,
	(const void *)npCommonPs4HttpConnPoolDelete,
};

}
