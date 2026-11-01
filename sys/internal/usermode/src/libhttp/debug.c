/*
 * SceLibHttp debug information: the memory pool, the connections and the requests printed
 * to the TTY, and the sockets a connection or request holds.
 */

#include <net/inet.h>

#include "http_internal.h"

int SceHttp_5B6A0ACE(void)
{
	return 0;
}

/* the requests: id, state, connection, scheme, host and path */
void SceHttp_FE60380A(void)
{
	HttpRequest **list = NULL;
	HttpRequest *req;
	int num = 0;
	int ret;
	int i;

	ret = _sceHttpGetObjectList(NULL, &num, HTTP_OBJECT_REQUEST);
	if (ret < 0) {
		sceClibPrintf("failed to _sceHttpGetObjectList()... %#x\n", ret);
		return;
	}
	sceClibPrintf("num. of current request = %d\n", num);
	if (num == 0)
		return;

	list = _sceHttpMalloc(NULL, num * sizeof(*list));
	if (list == NULL) {
		sceClibPrintf("out of memory...\n");
		goto out;
	}
	ret = _sceHttpGetObjectList((HttpObject **)list, &num, HTTP_OBJECT_REQUEST);
	if (ret < 0) {
		sceClibPrintf("failed to _sceHttpGetObjectList()... %#x\n", ret);
		goto out;
	}
	sceClibPrintf("id       status connId   \n");
	for (i = 0; i < num; i++) {
		req = list[i];
		sceClibPrintf("%8x   %4d  %8x\n", req->object.id, req->state,
			      req->connection->object.id);
		switch (req->connection->https) {
		case 0:
			sceClibPrintf("http  ");
			break;
		case 1:
			sceClibPrintf("https ");
			break;
		default:
			sceClibPrintf("-     ");
			break;
		}
		sceClibPrintf("%s %s\n", req->connection->hostname, req->path);
	}

out:
	if (list != NULL) {
		for (i = 0; i < num; i++)
			_sceHttpRequestRelease(list[i]);
		_sceHttpFree(NULL, list);
	}
}

/* the connections: id, socket, port, owner, state, scheme, host and address */
void SceHttp_B0E41827(void)
{
	HttpConnection **list;
	HttpConnection *conn;
	char addr[SCE_NET_INET_ADDRSTRLEN];
	int num = 0;
	int i;

	if (_sceHttpGetObjectList(NULL, &num, HTTP_OBJECT_CONNECTION) < 0)
		return;
	sceClibPrintf("num. of active connection = %d\n", num);
	if (num == 0)
		return;

	list = _sceHttpMalloc(NULL, num * sizeof(*list));
	if (list == NULL)
		return;
	if (_sceHttpGetObjectList((HttpObject **)list, &num, HTTP_OBJECT_CONNECTION) >= 0) {
		sceClibPrintf("id        sock  port leased4  stat \n");
		for (i = 0; i < num; i++) {
			conn = list[i];
			sceClibPrintf("%8x %5d %3d  %8x %4d ", conn->object.id, conn->sock,
				      conn->port, conn->owner, conn->state);
			switch (conn->https) {
			case 0:
				sceClibPrintf("http  ");
				break;
			case 1:
				sceClibPrintf("https ");
				break;
			default:
				sceClibPrintf("-     ");
				break;
			}
			sceNetInetNtop(SCE_NET_AF_INET, &conn->addr.sin_addr, addr, sizeof(addr));
			sceClibPrintf("%s(%s)\n", conn->hostname, addr);
		}
		sceClibPrintf("---------------------\n");
	}
	if (list != NULL) {
		for (i = 0; i < num; i++)
			_sceHttpConnectionRelease(list[i]);
		_sceHttpFree(NULL, list);
	}
}

/* the memory pool: in use now and at most */
void SceHttp_88116DB8(void)
{
	SceHttpMemoryPoolStats stats;

	sceHttpGetMemoryPoolStats(&stats);
	sceClibPrintf(" current      max\n");
	sceClibPrintf("%8x %8x\n", stats.currentInuseSize, stats.maxInuseSize);
}

/* all of it */
void SceHttp_0B0D8E7A(void)
{
	sceClibPrintf("**** libhttp debug information ****\n");
	sceClibPrintf("\nMEMORY POOL\n");
	SceHttp_88116DB8();
	sceClibPrintf("\nCONNECTION\n");
	SceHttp_B0E41827();
	sceClibPrintf("\nREQUEST\n");
	SceHttp_FE60380A();
}

int SceHttp_54847CFE(int connId, HttpConnectionSockInfo *info)
{
	HttpConnection *conn;
	int ret;

	if (info != NULL) {
		conn = NULL;
		ret = _sceHttpGetConnection(&conn, connId);
		if (ret >= 0) {
			info->resolverId = conn->resolverId;
			info->aborted = conn->aborted;
			info->sock = conn->sock;
			if (conn->state == 8 && _sceHttpConnectionIsConnected(conn) == 1)
				info->connected = 1;
			else
				info->connected = 0;
		}
		if (conn != NULL)
			_sceHttpConnectionRelease(conn);
	} else {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
	}
	return ret;
}

int SceHttp_80E6013D(int reqId, HttpRequestSockInfo *info)
{
	HttpRequest *req = NULL;
	HttpConnection *conn;
	int ret;

	ret = _sceHttpGetRequest(&req, reqId);
	if (ret >= 0) {
		ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
		if (ret >= 0) {
			conn = _sceHttpRequestGetConnection(req);
			info->sock = conn->sock;
			info->icmSock = req->icmSock;
			info->resolverId = conn->resolverId;
		}
	}
	if (req != NULL) {
		sceKernelUnlockLwMutex(&req->lock, 1);
		_sceHttpRequestRelease(req);
	}
	return ret;
}

/* sceHttpInit's debug set-up: nothing in these builds */
int _sceHttpDbgInit(void)
{
	return 0;
}
