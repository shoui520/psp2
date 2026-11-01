/*
 * SceLibHttp's objects: the templates, connections and requests, each kind in its own list
 * with its own range of ids, found by id under the library's lock and freed with their last
 * reference.
 */

#include "http_internal.h"

HttpObject *g_httpTemplateList = NULL;
HttpObject *g_httpConnectionList = NULL;
HttpObject *g_httpRequestList = NULL;
SceBool g_httpObjectInitialized = 1;

HttpObjectIds g_httpTemplateIds;
HttpObjectIds g_httpConnectionIds;
HttpObjectIds g_httpRequestIds;

/* adds an object to its kind's list; returns its new id */
int _sceHttpObjectRegister(HttpObject *object, SceUInt32 type)
{
	HttpObjectIds *ids;
	int max, base;
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	if (g_httpObjectInitialized == 0) {
		ret = SCE_HTTP_ERROR_BEFORE_INIT;
		goto out;
	}

	switch (type) {
	case HTTP_OBJECT_TEMPLATE:
		ids = &g_httpTemplateIds;
		object->next = g_httpTemplateList;
		max = 0x1fffffff;
		base = 0x10000000;
		if (g_httpTemplateList != NULL)
			g_httpTemplateList->prev = object;
		g_httpTemplateList = object;
		break;
	case HTTP_OBJECT_CONNECTION:
		ids = &g_httpConnectionIds;
		object->next = g_httpConnectionList;
		max = 0x2fffffff;
		base = 0x20000000;
		if (g_httpConnectionList != NULL)
			g_httpConnectionList->prev = object;
		g_httpConnectionList = object;
		break;
	case HTTP_OBJECT_REQUEST:
		ids = &g_httpRequestIds;
		object->next = g_httpRequestList;
		max = 0x3fffffff;
		base = 0x30000000;
		if (g_httpRequestList != NULL)
			g_httpRequestList->prev = object;
		g_httpRequestList = object;
		break;
	default:
		ids = NULL;
		break;
	}
	if (ids == NULL) {
		ret = SCE_HTTP_ERROR_BEFORE_INIT;
		goto out;
	}

	ret = ids->lastId + 1;
	ids->lastId = ret;
	ids->num++;
	if (ret > max) {
		ret = base + 1;
		ids->lastId = ret;
	}
	object->id = ret;

out:
	_sceHttpUnlock(0);
	return ret;
}

/* finds an object by id and takes a reference; with remove, unlinks it to be freed */
int _sceHttpFindObject(HttpObject **object, int id, SceUInt32 type, SceBool remove)
{
	HttpObject *obj = NULL;
	HttpObject *prev, *next;
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;

	switch (type) {
	case HTTP_OBJECT_TEMPLATE:
		obj = g_httpTemplateList;
		break;
	case HTTP_OBJECT_CONNECTION:
		obj = g_httpConnectionList;
		break;
	case HTTP_OBJECT_REQUEST:
		obj = g_httpRequestList;
		break;
	}
	for (; obj != NULL; obj = obj->next)
		if (obj->id == id)
			break;

	if (obj != NULL) {
		if (remove == 1) {
			prev = obj->prev;
			obj->deleted = 1;
			next = obj->next;
			if (prev != NULL) {
				prev->next = next;
				next = obj->next;
			} else {
				switch (type) {
				case HTTP_OBJECT_TEMPLATE:
					g_httpTemplateList = next;
					break;
				case HTTP_OBJECT_CONNECTION:
					g_httpConnectionList = next;
					break;
				case HTTP_OBJECT_REQUEST:
					g_httpRequestList = next;
					break;
				}
			}
			if (next != NULL)
				next->prev = obj->prev;
		}
		ret = 0;
		*object = obj;
		obj->refCount++;
	}

	_sceHttpUnlock(0);
	if (obj == NULL)
		return SCE_HTTP_ERROR_INVALID_ID;
	return ret;
}

/* the index-th object of a kind, with a reference when object is not NULL */
int _sceHttpGetObjectByIndex(HttpObject **object, int index, SceUInt32 type)
{
	HttpObject *obj = NULL;
	int ret;
	int i;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;

	switch (type) {
	case HTTP_OBJECT_TEMPLATE:
		obj = g_httpTemplateList;
		break;
	case HTTP_OBJECT_CONNECTION:
		obj = g_httpConnectionList;
		break;
	case HTTP_OBJECT_REQUEST:
		obj = g_httpRequestList;
		break;
	}
	for (i = 0; i < index; i++) {
		if (obj == NULL)
			break;
		obj = obj->next;
	}

	if (obj != NULL) {
		ret = 0;
		if (object != NULL) {
			*object = obj;
			obj->refCount++;
		}
	} else {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
	}

	_sceHttpUnlock(0);
	return ret;
}

/* with *num 0, counts the objects of a kind; else takes up to *num of them into list */
int _sceHttpGetObjectList(HttpObject **list, int *num, SceUInt32 type)
{
	HttpObject **templates = NULL;
	HttpObject **connections = NULL;
	HttpObject **requests = NULL;
	int ret = 0;
	int i;

	if (num == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;

	if (*num == 0) {
		for (i = 0; _sceHttpGetObjectByIndex(NULL, i, type) == 0; i++)
			;
		*num = i;
		return ret;
	}

	switch (type) {
	case HTTP_OBJECT_TEMPLATE:
		templates = list;
		break;
	case HTTP_OBJECT_CONNECTION:
		connections = list;
		break;
	case HTTP_OBJECT_REQUEST:
		requests = list;
		break;
	}
	for (i = 0; i < *num; i++) {
		switch (type) {
		case HTTP_OBJECT_TEMPLATE:
			ret = _sceHttpGetObjectByIndex(&templates[i], i, HTTP_OBJECT_TEMPLATE);
			break;
		case HTTP_OBJECT_CONNECTION:
			ret = _sceHttpGetObjectByIndex(&connections[i], i, type);
			break;
		case HTTP_OBJECT_REQUEST:
			ret = _sceHttpGetObjectByIndex(&requests[i], i, type);
			break;
		}
		if (ret != 0)
			break;
	}
	return ret;
}

int _sceHttpTakeObject(HttpObject **object, int id, int type)
{
	return _sceHttpFindObject(object, id, type, 1);
}

int _sceHttpGetTemplate(HttpTemplate **tmpl, int id)
{
	return _sceHttpFindObject((HttpObject **)tmpl, id, HTTP_OBJECT_TEMPLATE, 0);
}

int _sceHttpGetConnection(HttpConnection **conn, int id)
{
	return _sceHttpFindObject((HttpObject **)conn, id, HTTP_OBJECT_CONNECTION, 0);
}

int _sceHttpGetRequest(HttpRequest **req, int id)
{
	return _sceHttpFindObject((HttpObject **)req, id, HTTP_OBJECT_REQUEST, 0);
}

int _sceHttpObjectInit(void)
{
	g_httpTemplateList = NULL;
	g_httpTemplateIds.num = 0;
	g_httpTemplateIds.lastId = 0x10000000;
	g_httpConnectionList = NULL;
	g_httpConnectionIds.num = 0;
	g_httpConnectionIds.lastId = 0x20000000;
	g_httpRequestList = NULL;
	g_httpRequestIds.num = 0;
	g_httpRequestIds.lastId = 0x30000000;
	g_httpObjectInitialized = 1;
	return 0;
}

int _sceHttpTemplateRelease(HttpTemplate *tmpl)
{
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	tmpl->object.refCount--;
	if (tmpl->object.deleted == 1 && tmpl->object.refCount == 0) {
		_sceHttpTemplateClear(tmpl);
		_sceHttpFree(tmpl->freeObject, tmpl);
		g_httpTemplateIds.num--;
	}
	_sceHttpUnlock(0);
	return ret;
}

int _sceHttpConnectionRelease(HttpConnection *conn)
{
	void (*freeObject)(void *ptr);
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	conn->object.refCount--;
	if (conn->object.deleted == 1 && conn->object.refCount == 0) {
		freeObject = conn->tmpl.freeObject;
		_sceHttpConnectionDestroy(conn);
		_sceHttpFree(freeObject, conn);
		g_httpConnectionIds.num--;
	}
	_sceHttpUnlock(0);
	return ret;
}

/*
 * Drops a reference; the last one of a deleted request frees it with everything it holds.
 * The request itself is freed with the free function of the last child connection.
 */
int _sceHttpRequestReleaseEx(HttpRequest *req, SceBool *freed)
{
	void (*freeObject)(void *ptr);
	HttpConnection *child, *next;
	int ret;

	if (freed != NULL)
		*freed = 0;
	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	req->object.refCount--;
	if (req->object.deleted == 1 && req->object.refCount == 0) {
		_sceHttpTemplateClear(&req->tmpl);
		freeObject = req->tmpl.freeObject;
		if (req->path != NULL)
			_sceHttpFree(freeObject, req->path);
		if (req->query != NULL)
			_sceHttpFree(freeObject, req->query);
		if (req->fragment != NULL)
			_sceHttpFree(freeObject, req->fragment);
		if (req->responseHeader != NULL)
			_sceHttpFree(freeObject, req->responseHeader);
		if (req->url != NULL)
			_sceHttpFree(freeObject, req->url);
		if (req->allResponseHeaders != NULL)
			_sceHttpFree(freeObject, req->allResponseHeaders);
		if (req->requestHeader != NULL)
			_sceHttpRequestFreeHeader(req, &req->requestHeader);
		if (req->inflateStream != NULL) {
			ret = inflateEnd(req->inflateStream);
			_sceHttpFree(freeObject, req->inflateStream);
		}
		if (req->inflateBuffer != NULL)
			_sceHttpFree(freeObject, req->inflateBuffer);
		if (req->extra != NULL) {
			if (req->extra->method != NULL)
				_sceHttpFree(freeObject, req->extra->method);
			_sceHttpFree(freeObject, req->extra);
		}
		if ((SceUInt32)_sceHttpGetSdkVersion() < 0x3200000 && req->state != 19 &&
		    req->state != 18)
			_sceHttpConnectionShutdown(_sceHttpRequestGetConnection(req));
		for (child = req->childConnections; child != NULL; child = next) {
			freeObject = child->tmpl.freeObject;
			next = (HttpConnection *)child->object.next;
			_sceHttpConnectionDestroy(child);
			_sceHttpFree(freeObject, child);
		}
		sceKernelDeleteLwMutex(&req->lock);
		_sceHttpFree(freeObject, req);
		if (freed != NULL)
			*freed = 1;
		g_httpRequestIds.num--;
	}
	_sceHttpUnlock(0);
	return ret;
}

int _sceHttpRequestRelease(HttpRequest *req)
{
	return _sceHttpRequestReleaseEx(req, NULL);
}

/* unlinks the first object of a kind no one holds; SCE_HTTP_ERROR_BUSY and its id if held */
int _sceHttpTakeFirstObject(HttpObject **object, SceUInt32 type, int *id)
{
	HttpObject *obj = NULL;
	HttpObject *prev, *next;
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;

	switch (type) {
	case HTTP_OBJECT_TEMPLATE:
		obj = g_httpTemplateList;
		break;
	case HTTP_OBJECT_CONNECTION:
		obj = g_httpConnectionList;
		break;
	case HTTP_OBJECT_REQUEST:
		obj = g_httpRequestList;
		break;
	}
	*object = NULL;
	if (obj != NULL) {
		if (obj->refCount != 0) {
			if (id != NULL)
				*id = obj->id;
			ret = SCE_HTTP_ERROR_BUSY;
		} else {
			prev = obj->prev;
			obj->deleted = 1;
			next = obj->next;
			if (prev != NULL) {
				prev->next = next;
				next = obj->next;
			} else {
				switch (type) {
				case HTTP_OBJECT_TEMPLATE:
					g_httpTemplateList = next;
					break;
				case HTTP_OBJECT_CONNECTION:
					g_httpConnectionList = next;
					break;
				case HTTP_OBJECT_REQUEST:
					g_httpRequestList = next;
					break;
				}
			}
			if (next != NULL)
				next->prev = obj->prev;
			obj->refCount = 1;
			ret = 0;
			*object = obj;
		}
	}

	_sceHttpUnlock(0);
	return ret;
}

/* frees every object: the requests (aborting those in use), the connections, the templates */
int _sceHttpObjectTerm(void)
{
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	HttpTemplate *tmpl = NULL;
	int id;
	int ret, err;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	g_httpObjectInitialized = 0;
	_sceHttpUnlock(0);

	err = 0;
	while ((ret = _sceHttpTakeFirstObject((HttpObject **)&req, HTTP_OBJECT_REQUEST, &id)) != 0 ||
	       req != NULL) {
		if (ret == 0) {
			conn = _sceHttpRequestGetConnection(req);
			_sceHttpRequestReleaseEx(req, NULL);
			if (conn != NULL) {
				_sceHttpConnectionRelease(conn);
				conn = NULL;
			}
		} else if (ret == SCE_HTTP_ERROR_BUSY) {
			_sceHttpAbortRequest(id, 1);
			sceKernelDelayThread(1);
		} else if (ret < 0) {
			err = ret;
			break;
		}
	}

	while ((ret = _sceHttpTakeFirstObject((HttpObject **)&conn, HTTP_OBJECT_CONNECTION,
					      &id)) != 0 || conn != NULL) {
		if (ret == 0) {
			_sceHttpConnectionRelease(conn);
		} else if (ret == SCE_HTTP_ERROR_BUSY) {
			if (_sceHttpGetConnection(&conn, id) == 0) {
				_sceHttpConnectionAbort(conn);
				_sceHttpConnectionRelease(conn);
			}
			sceKernelDelayThread(1);
		} else if (ret < 0) {
			err = ret;
			break;
		}
	}

	while ((ret = _sceHttpTakeFirstObject((HttpObject **)&tmpl, HTTP_OBJECT_TEMPLATE, &id)) != 0 ||
	       tmpl != NULL) {
		if (ret == 0) {
			_sceHttpTemplateRelease(tmpl);
		} else if (ret == SCE_HTTP_ERROR_BUSY) {
			sceKernelDelayThread(1);
		} else if (ret < 0) {
			err = ret;
			break;
		}
	}
	return err;
}
