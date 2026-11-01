/*
 * SceLibHttp's redirects: following a 3xx response's Location (on the same connection, or
 * on a new one to another server), and the cache of 301s, which sends a request for a URL
 * that moved permanently straight to where it moved.
 */

#include "http_internal.h"

static struct {
	SceKernelLwMutexWork lock;
	HttpRedirectCacheEntry entries[HTTP_REDIRECT_CACHE_ENTRIES];
	HttpRedirectCacheEntry *free;
	HttpRedirectCacheEntry *head;
	HttpRedirectCacheEntry *tail;
	SceUInt8 initialized;
} g_httpRedirectCache;

/*
 * Whether the response is a redirect to follow: 1 with the location taken (and passed to
 * the redirect callback, which may refuse it), 0 when it is not followed.
 */
int _sceHttpRequestCheckRedirect(HttpRequest *req)
{
	char *location;
	int code;
	int ret;

	if (req->state < 15)
		return 0;
	if (req->tmpl.autoRedirect != 1)
		return 0;
	code = req->response.statusCode;
	if (code != SCE_HTTP_STATUS_CODE_MULTIPLE_CHOICES &&
	    code != SCE_HTTP_STATUS_CODE_MOVED_PERMANENTLY && code != SCE_HTTP_STATUS_CODE_FOUND &&
	    code != SCE_HTTP_STATUS_CODE_SEE_OTHER && code != SCE_HTTP_STATUS_CODE_TEMPORARY_REDIRECT)
		return 0;
	if (req->redirectCount >= (SceUInt32)req->tmpl.maxRedirects ||
	    (req->followOnly303 == 1 && code != SCE_HTTP_STATUS_CODE_SEE_OTHER))
		return 0;
	if (code == SCE_HTTP_STATUS_CODE_SEE_OTHER && req->method != SCE_HTTP_METHOD_HEAD)
		req->method = SCE_HTTP_METHOD_GET;
	if (req->response.location == NULL)
		return SCE_HTTP_ERROR_BAD_RESPONSE;

	location = _sceHttpMalloc(req->tmpl.allocator.malloc, req->response.locationLen + 1);
	if (location != NULL) {
		sceClibMemcpy_safe(location, req->response.location, req->response.locationLen);
		location[req->response.locationLen] = '\0';
		if (req->tmpl.redirectCallback == NULL ||
		    req->tmpl.redirectCallback(req->object.id, req->response.statusCode, &req->method,
					       location, req->tmpl.redirectUserArg) >= 0) {
			req->redirectLocation = location;
			return 1;
		}
		ret = 0;
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	if (location != NULL)
		_sceHttpFree(req->tmpl.allocator.free, location);
	return ret;
}

/* a relative location's path, joined to the current one's directory and swept */
static inline char *_sceHttpRedirectJoinPath(HttpRequest *req, char *base, const char *current)
{
	SceHttpUriElement unused;
	char *joined;
	char *swept = NULL;
	const char *slash;
	SceSize size;

	sceClibMemset(&unused, 0, sizeof(unused));
	if (sceClibStrnlen(base, 0x3fff) != 0 && base[0] == '/') {
		size = sceClibStrnlen(base, 0x3fff) + 1;
		joined = _sceHttpMalloc(req->tmpl.allocator.malloc, size);
		if (joined == NULL)
			goto fail;
		sceClibStrlcpy(joined, base, size);
	} else {
		slash = sceClibStrrchr(current, '/');
		if (slash != NULL) {
			size = slash + sceClibStrnlen(base, 0x3fff) - current + 2;
			joined = _sceHttpMalloc(req->tmpl.allocator.malloc, size);
			if (joined == NULL)
				goto fail;
			sceClibMemcpy_safe(joined, current, slash - current + 1);
			joined[slash - current + 1] = '\0';
			sceClibStrlcat(joined, base, size);
		} else {
			size = sceClibStrnlen(current, 0x3fff) + sceClibStrnlen(base, 0x3fff) + 1;
			joined = _sceHttpMalloc(req->tmpl.allocator.malloc, size);
			if (joined == NULL)
				goto fail;
			sceClibStrlcpy(joined, current, size);
			sceClibStrlcat(joined, base, size);
		}
	}
	swept = _sceHttpMalloc(req->tmpl.allocator.malloc, size);
	if (swept != NULL &&
	    sceHttpUriSweepPath(swept, joined, sceClibStrnlen(joined, size) + 1) >= 0) {
		_sceHttpFree(req->tmpl.allocator.free, joined);
		return swept;
	}
	_sceHttpFree(req->tmpl.allocator.free, joined);
fail:
	if (swept != NULL)
		_sceHttpFree(req->tmpl.allocator.free, swept);
	return NULL;
}

/*
 * Points the request at location: a relative one is resolved against the request's path,
 * one on the same server keeps the connection, one on another server gets a new connection
 * (and a POST to http from https becomes a GET). A 301 is remembered in the cache.
 */
__attribute__((noinline)) int _sceHttpRequestRedirect(HttpRequest *req, const char *location)
{
	HttpConnection *conn;
	SceHttpUriElement uri;
	SceSize require;
	char *hostHeader = NULL;
	char *oldUrl = NULL;
	char *newUrl;
	void *pool = NULL;
	char *query = NULL;
	char *fragment = NULL;
	char *path = NULL;
	char *base = NULL;
	char *current = NULL;
	char *url;
	int https;
	int ret;

	if (req->response.statusCode == SCE_HTTP_STATUS_CODE_MOVED_PERMANENTLY) {
		oldUrl = _sceHttpRequestGetUrl(req);
		if (oldUrl == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
	}
	conn = _sceHttpRequestGetConnection(req);
	ret = sceHttpUriParse(NULL, location, NULL, &require, 0);
	if (ret < 0)
		goto out;
	pool = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
	if (pool == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	ret = sceHttpUriParse(&uri, location, pool, NULL, require);
	if (ret < 0)
		goto out;
	if (uri.port == 0)
		uri.port = conn->port;
	if (_sceHttpStrNotEmpty(uri.query)) {
		require = sceClibStrnlen(uri.query, 0x3fff) + 1;
		query = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
		if (query == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibMemcpy_safe(query, uri.query, require);
	}
	if (_sceHttpStrNotEmpty(uri.fragment)) {
		require = sceClibStrnlen(uri.fragment, 0x3fff) + 1;
		fragment = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
		if (fragment == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibMemcpy_safe(fragment, uri.fragment, require);
	}
	if (uri.opaque != 1 && req->path != NULL) {
		_sceHttpFree(req->tmpl.allocator.free, req->path);
		req->path = NULL;
	}
	if (req->query != NULL) {
		_sceHttpFree(req->tmpl.allocator.free, req->query);
		req->query = NULL;
	}
	if (req->fragment != NULL) {
		_sceHttpFree(req->tmpl.allocator.free, req->fragment);
		req->fragment = NULL;
	}

	if (uri.opaque == 1) {
		/* relative: against the request's own path */
		ret = sceHttpUriBuild(NULL, &require, 0, &uri,
				      SCE_HTTP_URI_BUILD_WITH_HOSTNAME | SCE_HTTP_URI_BUILD_WITH_PATH);
		if (ret < 0)
			goto out;
		base = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
		if (base == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		ret = sceHttpUriBuild(base, NULL, require, &uri,
				      SCE_HTTP_URI_BUILD_WITH_HOSTNAME | SCE_HTTP_URI_BUILD_WITH_PATH);
		if (ret < 0)
			goto out;
		_sceHttpFree(req->tmpl.allocator.free, pool);
		pool = NULL;
		sceClibMemset(&uri, 0, sizeof(uri));
		ret = sceHttpUriParse(NULL, req->path, NULL, &require, 0);
		if (ret < 0)
			goto out;
		pool = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
		if (pool == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		ret = sceHttpUriParse(&uri, req->path, pool, NULL, require);
		if (ret < 0)
			goto out;
		if (req->path != NULL) {
			_sceHttpFree(req->tmpl.allocator.free, req->path);
			req->path = NULL;
		}
		ret = sceHttpUriBuild(NULL, &require, 0, &uri,
				      SCE_HTTP_URI_BUILD_WITH_HOSTNAME | SCE_HTTP_URI_BUILD_WITH_PATH);
		if (ret < 0)
			goto out;
		current = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
		if (current == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		ret = sceHttpUriBuild(current, NULL, require, &uri,
				      SCE_HTTP_URI_BUILD_WITH_HOSTNAME | SCE_HTTP_URI_BUILD_WITH_PATH);
		if (ret < 0)
			goto out;
		path = _sceHttpRedirectJoinPath(req, base, current);
		if (path == NULL)
			goto out;
		_sceHttpFree(req->tmpl.allocator.free, current);
		_sceHttpFree(req->tmpl.allocator.free, base);
		req->path = path;
		req->query = query;
		req->fragment = fragment;
	} else if (sceClibStrncasecmp(uri.hostname, conn->hostname, 0xff) == 0 &&
		   (sceClibStrnlen(uri.scheme, 0x20) == 0 ||
		    _sceHttpSchemeGetType(uri.scheme) == conn->https) &&
		   conn->port == uri.port) {
		/* the same server */
		require = sceClibStrnlen(uri.path, 0x3fff) + 1;
		path = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
		if (path == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibStrlcpy(path, uri.path, require);
		_sceHttpFree(req->tmpl.allocator.free, pool);
		req->path = path;
		req->query = query;
		req->fragment = fragment;
		pool = NULL;
	} else {
		/* another server */
		if (req->method == SCE_HTTP_METHOD_POST &&
		    req->response.statusCode != SCE_HTTP_STATUS_CODE_TEMPORARY_REDIRECT &&
		    conn->https == 1 && _sceHttpSchemeGetType(uri.scheme) == 0)
			req->method = SCE_HTTP_METHOD_GET;
		require = sceClibStrnlen(uri.path, 0x3fff) + 1;
		path = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
		if (path == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibStrlcpy(path, uri.path, require);
		req->path = path;
		path = NULL;
		req->query = query;
		req->fragment = fragment;
		query = NULL;
		fragment = NULL;
		if (sceClibStrnlen(uri.scheme, 0x20) != 0)
			https = _sceHttpSchemeGetType(uri.scheme);
		else
			https = conn->https;
		ret = _sceHttpMakeHostHeader(&req->tmpl.allocator, &hostHeader, https, uri.hostname,
					     uri.port);
		if (ret < 0)
			goto out;
		_sceHttpAddRequestHeader(req->object.id, "Host", hostHeader, SCE_HTTP_HEADER_OVERWRITE);
		_sceHttpFree(req->tmpl.allocator.free, hostHeader);
		hostHeader = NULL;
		if (sceClibStrnlen(uri.scheme, 0x20) != 0) {
			ret = _sceHttpCreateConnectionWithURL(&req->tmpl, 0, location,
							      req->tmpl.keepAlive, 1, req);
		} else {
			/* "//host/path": the scheme of the connection */
			require = sceClibStrnlen(location, 0xffffffff) + 7;
			url = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
			if (url == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemset(url, 0, require);
			if (https == 0)
				sceClibStrncpy(url, "http", require);
			if (https == 1)
				sceClibStrncpy(url, "https", require);
			sceClibStrlcat(url, ":", require);
			sceClibStrlcat(url, location, require);
			ret = _sceHttpCreateConnectionWithURL(&req->tmpl, 0, url, req->tmpl.keepAlive, 1,
							      req);
			_sceHttpFree(req->tmpl.allocator.free, url);
		}
		if (ret < 0)
			goto out;
	}

	base = NULL;
	query = NULL;
	fragment = NULL;
	path = NULL;
	current = NULL;
	if (req->response.statusCode == SCE_HTTP_STATUS_CODE_MOVED_PERMANENTLY) {
		newUrl = _sceHttpRequestGetUrl(req);
		if (newUrl != NULL) {
			ret = _sceHttpRedirectCacheAdd(oldUrl, req->method, NULL, newUrl);
			_sceHttpFree(req->tmpl.allocator.free, newUrl);
		} else {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		}
	}
out:
	if (oldUrl != NULL)
		_sceHttpFree(req->tmpl.allocator.free, oldUrl);
	if (pool != NULL)
		_sceHttpFree(req->tmpl.allocator.free, pool);
	if (path != NULL)
		_sceHttpFree(req->tmpl.allocator.free, path);
	if (query != NULL)
		_sceHttpFree(req->tmpl.allocator.free, query);
	if (fragment != NULL)
		_sceHttpFree(req->tmpl.allocator.free, fragment);
	if (current != NULL)
		_sceHttpFree(req->tmpl.allocator.free, current);
	if (base != NULL)
		_sceHttpFree(req->tmpl.allocator.free, base);
	if (hostHeader != NULL)
		_sceHttpFree(req->tmpl.allocator.free, hostHeader);
	return ret;
}

/* goes to the location _sceHttpRequestCheckRedirect took: 1 */
int _sceHttpRequestFollowRedirect(HttpRequest *req)
{
	int ret;

	ret = _sceHttpRequestRemoveAuthHeaders(req, 1, 1, 1);
	if (ret < 0)
		return ret;
	ret = _sceHttpRequestRedirect(req, req->redirectLocation);
	if (ret < 0)
		return ret;
	req->redirectCount++;
	if (req->redirectLocation != NULL) {
		_sceHttpFree(req->tmpl.allocator.free, req->redirectLocation);
		req->redirectLocation = NULL;
	}
	return 1;
}

/* a request for a URL the cache knows moved goes to where it moved, a chain of them at once */
int _sceHttpRequestRedirectFromCache(HttpRequest *req)
{
	HttpRedirectCacheEntry *entry = NULL;
	HttpRedirectCacheEntry *next = NULL;
	HttpRedirectCacheEntry *last;
	SceUInt32 count;
	int ret, r;

	ret = _sceHttpRedirectCacheLookupRequest(req, &entry);
	if (ret < 0)
		goto out;
	last = NULL;
	while (entry != NULL) {
		count = req->redirectCount;
		if (count + 1 > (SceUInt32)req->tmpl.maxRedirects) {
			_sceHttpRedirectCacheRelease(entry);
			entry = last;
			break;
		}
		if (req->tmpl.redirectCallback != NULL) {
			r = req->tmpl.redirectCallback(req->object.id,
						       SCE_HTTP_STATUS_CODE_MOVED_PERMANENTLY,
						       &req->method, entry->location,
						       req->tmpl.redirectUserArg);
			count = req->redirectCount;
			if (r < 0) {
				_sceHttpRedirectCacheRelease(entry);
				entry = last;
				ret = 0;
				break;
			}
		}
		req->redirectCount = count + 1;
		if (last != NULL)
			_sceHttpRedirectCacheRelease(last);
		ret = _sceHttpRedirectCacheLookup(entry->location, req->method, NULL, &next);
		if (ret < 0)
			break;
		last = entry;
		entry = next;
	}
	if (entry == NULL && last != NULL)
		entry = last;
	if (entry != NULL)
		ret = _sceHttpRequestRedirect(req, entry->location);
out:
	if (entry != NULL)
		_sceHttpRedirectCacheRelease(entry);
	return ret;
}

static inline void _sceHttpRedirectCacheReset(void)
{
	int i;

	g_httpRedirectCache.free = &g_httpRedirectCache.entries[0];
	for (i = 0; i < HTTP_REDIRECT_CACHE_ENTRIES - 1; i++)
		g_httpRedirectCache.entries[i].nextFree = &g_httpRedirectCache.entries[i + 1];
	g_httpRedirectCache.entries[HTTP_REDIRECT_CACHE_ENTRIES - 1].nextFree = NULL;
}

int _sceHttpRedirectCacheInit(void)
{
	int ret;

	if (g_httpRedirectCache.initialized == 1)
		return SCE_HTTP_ERROR_ALREADY_INITED;
	sceClibMemset(g_httpRedirectCache.entries, 0, sizeof(g_httpRedirectCache.entries));
	ret = sceKernelCreateLwMutex(&g_httpRedirectCache.lock, "SceLibHttp",
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret < 0)
		return ret;
	_sceHttpRedirectCacheReset();
	g_httpRedirectCache.head = NULL;
	g_httpRedirectCache.tail = NULL;
	g_httpRedirectCache.initialized = 1;
	return 0;
}

int _sceHttpRedirectCacheTerm(void)
{
	HttpRedirectCacheEntry *entry;

	if (g_httpRedirectCache.initialized != 1)
		return SCE_HTTP_ERROR_BEFORE_INIT;
	for (entry = g_httpRedirectCache.head; entry != NULL; entry = entry->next) {
		_sceHttpDefaultFree(entry->location);
		_sceHttpDefaultFree(entry->url);
	}
	sceKernelDeleteLwMutex(&g_httpRedirectCache.lock);
	g_httpRedirectCache.initialized = 0;
	sceClibMemset(g_httpRedirectCache.entries, 0, sizeof(g_httpRedirectCache.entries));
	g_httpRedirectCache.free = NULL;
	g_httpRedirectCache.head = NULL;
	g_httpRedirectCache.tail = NULL;
	return 0;
}

int _sceHttpRedirectCacheFlush(void)
{
	HttpRedirectCacheEntry *entry;
	int ret;

	if (g_httpRedirectCache.initialized != 1)
		return SCE_HTTP_ERROR_BEFORE_INIT;
	ret = sceKernelLockLwMutex(&g_httpRedirectCache.lock, 1, NULL);
	if (ret < 0)
		return ret;
	for (entry = g_httpRedirectCache.head; entry != NULL; entry = entry->next) {
		_sceHttpDefaultFree(entry->location);
		_sceHttpDefaultFree(entry->url);
	}
	sceClibMemset(g_httpRedirectCache.entries, 0, sizeof(g_httpRedirectCache.entries));
	/* before SDK 3.00 a flush leaves the cache without entries */
	if ((SceUInt32)_sceHttpGetSdkVersion() >= 0x3000000)
		_sceHttpRedirectCacheReset();
	else
		g_httpRedirectCache.free = NULL;
	g_httpRedirectCache.head = NULL;
	g_httpRedirectCache.tail = NULL;
	sceKernelUnlockLwMutex(&g_httpRedirectCache.lock, 1);
	return ret;
}

static inline void _sceHttpRedirectCacheUnlink(HttpRedirectCacheEntry *entry)
{
	if (entry->prev != NULL) {
		entry->prev->next = entry->next;
		if (entry->next != NULL)
			entry->next->prev = entry->prev;
		else
			g_httpRedirectCache.tail = entry->prev;
	} else {
		g_httpRedirectCache.head = entry->next;
		if (entry->next != NULL)
			entry->next->prev = NULL;
		else
			g_httpRedirectCache.tail = NULL;
	}
}

static inline void _sceHttpRedirectCachePushFront(HttpRedirectCacheEntry *entry)
{
	HttpRedirectCacheEntry *head;

	entry->prev = NULL;
	head = g_httpRedirectCache.head;
	entry->next = head;
	if (head != NULL)
		head->prev = entry;
	else
		g_httpRedirectCache.tail = entry;
	g_httpRedirectCache.head = entry;
}

/* takes an entry off the list and back to the free ones */
static inline void _sceHttpRedirectCacheFreeEntry(HttpRedirectCacheEntry *entry)
{
	_sceHttpRedirectCacheUnlink(entry);
	_sceHttpDefaultFree(entry->url);
	_sceHttpDefaultFree(entry->location);
	sceClibMemset(entry, 0, sizeof(*entry));
	entry->nextFree = g_httpRedirectCache.free;
	g_httpRedirectCache.free = entry;
}

/* with the lock held: the entry for url, referenced and moved to the front */
__attribute__((noinline)) HttpRedirectCacheEntry *_sceHttpRedirectCacheSearch(const char *url, int method, const void *key)
{
	SceMd5Context ctx;
	SceUChar8 digest[SCE_MD5_DIGEST_SIZE];
	HttpRedirectCacheEntry *entry;
	SceSize len;

	len = sceClibStrnlen(url, 0xffffffff);
	sceMd5BlockInit(&ctx);
	sceMd5BlockUpdate(&ctx, url, len);
	sceMd5BlockResult(&ctx, digest);
	for (entry = g_httpRedirectCache.head; entry != NULL; entry = entry->next) {
		if (sceClibMemcmp(entry->urlDigest, digest, sizeof(digest)) == 0 &&
		    sceClibStrncmp(entry->url, url, sceClibStrnlen(entry->url, 0xffffffff) + 1) == 0 &&
		    entry->method == method &&
		    (key == NULL || sceClibMemcmp(entry->key, key, sizeof(entry->key)) == 0) &&
		    entry->removed != 1)
			break;
	}
	if (entry != NULL) {
		entry->hits++;
		entry->refCount++;
		_sceHttpRedirectCacheUnlink(entry);
		_sceHttpRedirectCachePushFront(entry);
	}
	return entry;
}

__attribute__((noinline)) int _sceHttpRedirectCacheLookupRequest(HttpRequest *req, HttpRedirectCacheEntry **entry)
{
	char *url;
	int ret = 0;

	*entry = NULL;
	url = _sceHttpRequestGetUrl(req);
	if (url != NULL) {
		ret = sceKernelLockLwMutex(&g_httpRedirectCache.lock, 1, NULL);
		if (ret >= 0) {
			*entry = _sceHttpRedirectCacheSearch(url, req->method, NULL);
			sceKernelUnlockLwMutex(&g_httpRedirectCache.lock, 1);
		}
		_sceHttpFree(req->tmpl.allocator.free, url);
	}
	return ret;
}

__attribute__((noinline)) int _sceHttpRedirectCacheLookup(const char *url, int method, const void *key,
				HttpRedirectCacheEntry **entry)
{
	HttpRedirectCacheEntry *found;
	int ret;

	ret = sceKernelLockLwMutex(&g_httpRedirectCache.lock, 1, NULL);
	if (ret < 0)
		return ret;
	found = _sceHttpRedirectCacheSearch(url, method, key);
	sceKernelUnlockLwMutex(&g_httpRedirectCache.lock, 1);
	*entry = found;
	if (entry == NULL)
		return SCE_HTTP_ERROR_NOT_FOUND;
	return ret;
}

/*
 * Remembers that url moved to location: an entry for url to another location is removed
 * first, and when no entry is free the least recently used unreferenced one is reused.
 */
__attribute__((noinline)) int _sceHttpRedirectCacheAdd(const char *url, int method, const void *key, const char *location)
{
	SceMd5Context ctx;
	HttpRedirectCacheEntry *entry;
	SceSize len;
	int ret, r;

	r = sceKernelLockLwMutex(&g_httpRedirectCache.lock, 1, NULL);
	ret = r;
	if (r < 0)
		return r;
	entry = _sceHttpRedirectCacheSearch(url, method, key);
	if (entry != NULL) {
		if (sceClibStrncmp(entry->location, location,
				   sceClibStrnlen(entry->location, 0xffffffff) + 1) == 0) {
			/* already known */
			if (--entry->refCount == 0 && entry->removed == 1)
				_sceHttpRedirectCacheFreeEntry(entry);
			goto out;
		}
		entry->removed = 1;
		if (entry->refCount == 0)
			_sceHttpRedirectCacheFreeEntry(entry);
		if (--entry->refCount == 0 && entry->removed == 1)
			_sceHttpRedirectCacheFreeEntry(entry);
	}
	while (g_httpRedirectCache.free == NULL) {
		for (entry = g_httpRedirectCache.tail; entry != NULL; entry = entry->prev) {
			if (entry->refCount == 0)
				break;
		}
		if (entry == NULL)
			goto out;
		entry->removed = 1;
		_sceHttpRedirectCacheFreeEntry(entry);
	}
	entry = g_httpRedirectCache.free;
	g_httpRedirectCache.free = g_httpRedirectCache.free->nextFree;
	len = sceClibStrnlen(url, 0xffffffff);
	entry->url = _sceHttpDefaultMalloc(len + 1);
	if (entry->url == NULL) {
		sceClibMemset(entry, 0, sizeof(*entry));
		entry->nextFree = g_httpRedirectCache.free;
		g_httpRedirectCache.free = entry;
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	sceClibMemcpy_safe(entry->url, url, len);
	entry->url[len] = '\0';
	len = sceClibStrnlen(location, 0xffffffff);
	entry->location = _sceHttpDefaultMalloc(len + 1);
	if (entry->location == NULL) {
		_sceHttpDefaultFree(entry->url);
		sceClibMemset(entry, 0, sizeof(*entry));
		entry->nextFree = g_httpRedirectCache.free;
		g_httpRedirectCache.free = entry;
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	sceClibMemcpy_safe(entry->location, location, len);
	entry->location[len] = '\0';
	len = sceClibStrnlen(url, 0xffffffff);
	sceMd5BlockInit(&ctx);
	sceMd5BlockUpdate(&ctx, url, len);
	sceMd5BlockResult(&ctx, entry->urlDigest);
	entry->method = method;
	if (key != NULL)
		sceClibMemcpy_safe(entry->key, key, sizeof(entry->key));
	_sceHttpRedirectCachePushFront(entry);
out:
	sceKernelUnlockLwMutex(&g_httpRedirectCache.lock, 1);
	return ret;
}

__attribute__((noinline)) int _sceHttpRedirectCacheRelease(HttpRedirectCacheEntry *entry)
{
	int ret;

	ret = sceKernelLockLwMutex(&g_httpRedirectCache.lock, 1, NULL);
	if (ret < 0)
		return ret;
	if (--entry->refCount == 0 && entry->removed == 1)
		_sceHttpRedirectCacheFreeEntry(entry);
	sceKernelUnlockLwMutex(&g_httpRedirectCache.lock, 1);
	return 0;
}
