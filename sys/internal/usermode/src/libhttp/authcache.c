/*
 * SceLibHttp authentication cache: the credentials that answered a server's or a proxy's
 * challenge, kept per realm so the next request sends them without being asked again.
 */

#include "http_internal.h"

SceKernelLwMutexWork g_httpAuthCacheLock = {{0}};
SceUInt32 g_httpAuthCacheMax = 4;
SceUInt32 g_httpAuthCacheCount = 0;
HttpAuthCacheEntry *g_httpAuthCacheHead = NULL;
HttpAuthCacheEntry *g_httpAuthCacheTail = NULL;
SceBool g_httpAuthCacheInitialized = 0;

/* takes entry out of the cache's list */
static inline __attribute__((always_inline)) void _sceHttpAuthCacheUnlink(HttpAuthCacheEntry *entry)
{
	HttpAuthCacheEntry *prev = entry->prev, *next = entry->next;

	if (prev != NULL) {
		prev->next = next;
		if (next != NULL)
			entry->next->prev = entry->prev;
		else
			g_httpAuthCacheTail = entry->prev;
	} else {
		g_httpAuthCacheHead = entry->next;
		if (next != NULL)
			next->prev = NULL;
		else
			g_httpAuthCacheTail = NULL;
	}
}

/* frees entry once nothing holds it */
__attribute__((noinline))
void _sceHttpAuthCacheRemove(HttpAuthCacheEntry *entry)
{
	if (entry->refCount != 0)
		return;
	_sceHttpAuthCacheUnlink(entry);
	if (entry->uriBuffer != NULL) {
		_sceHttpDefaultFree(entry->uriBuffer);
		sceClibMemset(&entry->uri, 0, sizeof(entry->uri));
	}
	if (entry->challengeBuffer != NULL) {
		_sceHttpDefaultFree(entry->challengeBuffer);
		sceClibMemset(&entry->challenge, 0, sizeof(entry->challenge));
	}
	_sceHttpDefaultFree(entry);
	if (g_httpAuthCacheCount != 0)
		g_httpAuthCacheCount--;
}

__attribute__((noinline))
int _sceHttpAuthCacheFlush(void)
{
	HttpAuthCacheEntry *entry, *next;
	int ret;

	if (g_httpAuthCacheInitialized != 1)
		return SCE_HTTP_ERROR_BEFORE_INIT;
	ret = sceKernelLockLwMutex(&g_httpAuthCacheLock, 1, NULL);
	if (ret < 0)
		return ret;
	for (entry = g_httpAuthCacheHead; entry != NULL; entry = next) {
		next = entry->next;
		_sceHttpAuthCacheRemove(entry);
	}
	sceKernelUnlockLwMutex(&g_httpAuthCacheLock, 1);
	return 0;
}

__attribute__((noinline))
int _sceHttpAuthCacheInit(void)
{
	int ret;

	if (g_httpAuthCacheInitialized == 1)
		return SCE_HTTP_ERROR_ALREADY_INITED;
	ret = sceKernelCreateLwMutex(&g_httpAuthCacheLock, "SceLibHttpAuthCache", 2, 0, NULL);
	if (ret < 0)
		return ret;
	g_httpAuthCacheMax = 4;
	g_httpAuthCacheCount = 0;
	g_httpAuthCacheHead = NULL;
	g_httpAuthCacheTail = NULL;
	g_httpAuthCacheInitialized = 1;
	return 0;
}

__attribute__((noinline))
int _sceHttpAuthCacheTerm(void)
{
	HttpAuthCacheEntry *entry, *next;

	if (g_httpAuthCacheInitialized != 1)
		return SCE_HTTP_ERROR_BEFORE_INIT;
	for (entry = g_httpAuthCacheHead; entry != NULL; entry = next) {
		next = entry->next;
		_sceHttpAuthCacheUnlink(entry);
		if (entry->uriBuffer != NULL)
			_sceHttpDefaultFree(entry->uriBuffer);
		if (entry->challengeBuffer != NULL)
			_sceHttpDefaultFree(entry->challengeBuffer);
		_sceHttpDefaultFree(entry);
	}
	sceKernelDeleteLwMutex(&g_httpAuthCacheLock);
	g_httpAuthCacheTail = NULL;
	g_httpAuthCacheInitialized = 0;
	g_httpAuthCacheHead = NULL;
	return 0;
}

/* drops a request's hold on entry */
__attribute__((noinline))
int _sceHttpAuthCacheRelease(HttpAuthCacheEntry *entry)
{
	int ret;

	if (entry == NULL)
		return 0;
	ret = sceKernelLockLwMutex(&g_httpAuthCacheLock, 1, NULL);
	if (ret < 0)
		return ret;
	if (entry->refCount != 0)
		entry->refCount--;
	return sceKernelUnlockLwMutex(&g_httpAuthCacheLock, 1);
}

/* drops a request's hold on entry and frees it if it was the last (the server refused it) */
__attribute__((noinline))
int _sceHttpAuthCacheDelete(HttpAuthCacheEntry *entry)
{
	int ret;

	if (entry == NULL)
		return 0;
	ret = sceKernelLockLwMutex(&g_httpAuthCacheLock, 1, NULL);
	if (ret < 0)
		return ret;
	if (entry->refCount != 0)
		entry->refCount--;
	_sceHttpAuthCacheRemove(entry);
	return sceKernelUnlockLwMutex(&g_httpAuthCacheLock, 1);
}

/* the server took entry's credentials: they are sent unasked from now on */
__attribute__((noinline))
int _sceHttpAuthCacheConfirm(HttpAuthCacheEntry *entry)
{
	int ret;

	if (entry == NULL)
		return 0;
	ret = sceKernelLockLwMutex(&g_httpAuthCacheLock, 1, NULL);
	if (ret < 0)
		return ret;
	entry->state = 2;
	return sceKernelUnlockLwMutex(&g_httpAuthCacheLock, 1);
}

__attribute__((noinline))
int _sceHttpAuthCacheGetCredentials(const HttpAuthCacheEntry *entry, char *username,
				    char *password)
{
	int ret = 0;

	if (entry != NULL) {
		ret = sceKernelLockLwMutex(&g_httpAuthCacheLock, 1, NULL);
		if (ret >= 0) {
			sceClibMemcpy_safe(username, entry->username, SCE_HTTP_USERNAME_MAX_SIZE);
			sceClibMemcpy_safe(password, entry->password, SCE_HTTP_PASSWORD_MAX_SIZE);
			sceKernelUnlockLwMutex(&g_httpAuthCacheLock, 1);
		}
	}
	return ret;
}

__attribute__((noinline))
void _sceHttpAuthCacheFreeHeader(char *header)
{
	_sceHttpDefaultFree(header);
}

/* the Authorization header entry's credentials make for key's request, into *key->header */
__attribute__((noinline))
int _sceHttpAuthCacheMakeHeader(HttpAuthCacheKey *key, const void *entity, SceSize entitySize,
				HttpAuthCacheEntry *entry)
{
	HttpAuthDigest digest;
	SceSize size = 0;
	char *path = NULL, *header;
	int type = entry->challenge.type;
	int ret;

	if (type == SCE_HTTP_AUTH_BASIC || type == SCE_HTTP_AUTH_RESERVED0) {
		ret = _sceHttpAuthBasic(NULL, NULL, &size, 0, entry->username, entry->password);
		if (ret < 0)
			goto out;
		header = _sceHttpDefaultMalloc(size);
		if (header == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		ret = _sceHttpAuthBasic(&key->request->tmpl.allocator, header, &size, size,
					entry->username, entry->password);
		if (ret < 0)
			goto fail;
	} else if (type == SCE_HTTP_AUTH_DIGEST || type == SCE_HTTP_AUTH_RESERVED1) {
		ret = _sceHttpAuthDigestInit(&key->request->tmpl.allocator, &digest, &entry->challenge);
		if (ret < 0)
			goto out;
		ret = _sceHttpRequestGetPath(key->request, &path);
		if (ret < 0)
			goto out;
		ret = _sceHttpAuthDigest(NULL, &size, 0, entry->username, entry->password, path,
					 key->request->method, entity, entitySize, &digest);
		if (ret < 0)
			goto out;
		header = _sceHttpDefaultMalloc(size);
		if (header == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		ret = _sceHttpAuthDigest(header, &size, size, entry->username, entry->password, path,
					 key->request->method, entity, entitySize, &digest);
		if (ret < 0) {
fail:
			_sceHttpDefaultFree(header);
			goto out;
		}
		_sceHttpAuthDigestTerm(&digest);
	} else {
		ret = SCE_HTTP_ERROR_UNKNOWN;
		goto out;
	}
	if (*key->header != NULL)
		_sceHttpDefaultFree(*key->header);
	*key->header = header;
out:
	if (path != NULL)
		_sceHttpFree(key->request->tmpl.allocator.free, path);
	return ret;
}

/* a copy of src with its strings in one buffer of the library's pool */
__attribute__((noinline))
int _sceHttpAuthChallengeCopy(HttpAuthChallenge *dst, char **buffer, const HttpAuthChallenge *src)
{
	char *p;

	p = _sceHttpDefaultMalloc(src->realmLen + src->qopLen + src->nonceLen + src->opaqueLen +
				  src->algorithmLen + src->domainLen);
	if (p == NULL)
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;
	*buffer = p;
	if (src->realm != NULL) {
		sceClibMemcpy_safe(p, src->realm, src->realmLen);
		dst->realm = p;
	}
	dst->realmLen = src->realmLen;
	p += src->realmLen;
	if (src->qop != NULL) {
		sceClibMemcpy_safe(p, src->qop, src->qopLen);
		dst->qop = p;
	}
	dst->qopLen = src->qopLen;
	p += src->qopLen;
	if (src->nonce != NULL) {
		sceClibMemcpy_safe(p, src->nonce, src->nonceLen);
		dst->nonce = p;
	}
	dst->nonceLen = src->nonceLen;
	p += src->nonceLen;
	if (src->opaque != NULL) {
		sceClibMemcpy_safe(p, src->opaque, src->opaqueLen);
		dst->opaque = p;
	}
	dst->opaqueLen = src->opaqueLen;
	p += src->opaqueLen;
	if (src->algorithm != NULL) {
		sceClibMemcpy_safe(p, src->algorithm, src->algorithmLen);
		dst->algorithm = p;
	}
	dst->algorithmLen = src->algorithmLen;
	p += src->algorithmLen;
	if (src->domain != NULL) {
		sceClibMemcpy_safe(p, src->domain, src->domainLen);
		dst->domain = p;
	}
	dst->domainLen = src->domainLen;
	dst->type = src->type;
	dst->stale = src->stale;
	return 0;
}

/*
 * The entry for key's server (or proxy) whose path is the longest prefix of the request's,
 * moved to the front of the list and held. For a new challenge (forChallenge) it must be of
 * the challenge's type and realm, and takes the challenge; otherwise only confirmed entries
 * count, and the Authorization header is made from it.
 */
__attribute__((noinline))
int _sceHttpAuthCacheSearch(HttpAuthCacheKey *key, const void *entity, SceSize entitySize,
			    int type, const char *realm, HttpAuthCacheEntry **entry,
			    const HttpAuthChallenge *challenge, SceBool anyPath,
			    SceBool forChallenge, SceBool proxy)
{
	SceMd5Context context;
	SceUChar8 realmDigest[SCE_MD5_DIGEST_SIZE];
	HttpAuthCacheEntry *e, *best, *head;
	const char *path, *keyPath;
	SceSize len, keyLen;
	char *old;
	int ret;

	*entry = NULL;
	ret = sceKernelLockLwMutex(&g_httpAuthCacheLock, 1, NULL);
	if (ret < 0)
		return ret;
	best = NULL;
	for (e = g_httpAuthCacheHead; e != NULL; e = e->next) {
		if (!forChallenge && e->state != 2)
			continue;
		if (proxy) {
			if (e->uri.port != key->proxy.port)
				continue;
			if (sceClibStrncmp(e->uri.hostname, key->proxy.hostname,
					   sceClibStrnlen(key->proxy.hostname, 0xffffffff) + 1) != 0)
				continue;
		} else {
			if (sceClibStrncmp(e->uri.scheme, key->uri->scheme,
					   sceClibStrnlen(e->uri.scheme, 0xffffffff) + 1) != 0)
				continue;
			if (e->uri.port != key->uri->port)
				continue;
			if (sceClibStrncmp(e->uri.hostname, key->uri->hostname,
					   sceClibStrnlen(key->uri->hostname, 0xffffffff) + 1) != 0)
				continue;
		}
		if (forChallenge) {
			if (type != e->challenge.type)
				continue;
			sceClibMemset(realmDigest, 0, sizeof(realmDigest));
			len = sceClibStrnlen(realm, 0xffffffff);
			if (realm != NULL && len != 0) {
				sceMd5BlockInit(&context);
				sceMd5BlockUpdate(&context, realm, len);
				sceMd5BlockResult(&context, realmDigest);
			}
			if (sceClibMemcmp(e->realmDigest, realmDigest, sizeof(realmDigest)) != 0)
				continue;
		}
		if (!proxy && !(anyPath && forChallenge)) {
			/* the entry's path must be a prefix of the request's */
			path = e->uri.path;
			keyPath = key->uri->path;
			len = sceClibStrnlen(path, 0xffffffff);
			if (path != NULL && len != 0) {
				keyLen = sceClibStrnlen(keyPath, 0xffffffff);
				if (keyPath == NULL || keyLen == 0 || len > keyLen ||
				    sceClibMemcmp(path, keyPath, len) != 0)
					continue;
			}
		}
		if (best == NULL || sceClibStrnlen(e->uri.path, 0xffffffff) >=
				    sceClibStrnlen(best->uri.path, 0xffffffff))
			best = e;
	}
	if (best == NULL)
		goto found;

	_sceHttpAuthCacheUnlink(best);
	head = g_httpAuthCacheHead;
	best->prev = NULL;
	best->next = head;
	if (head != NULL)
		head->prev = best;
	else
		g_httpAuthCacheTail = best;
	g_httpAuthCacheHead = best;
	if (challenge != NULL) {
		old = best->challengeBuffer;
		ret = _sceHttpAuthChallengeCopy(&best->challenge, &best->challengeBuffer, challenge);
		if (ret < 0) {
			best->challengeBuffer = old;
			goto out;
		}
		_sceHttpDefaultFree(old);
	}
	if (forChallenge || (ret = _sceHttpAuthCacheMakeHeader(key, entity, entitySize, best)) >= 0) {
found:
		*entry = best;
		if (best != NULL)
			best->refCount++;
	}
out:
	sceKernelUnlockLwMutex(&g_httpAuthCacheLock, 1);
	return ret;
}

/* the confirmed credentials for key's request, with its Authorization header */
__attribute__((noinline))
int _sceHttpAuthCacheLookup(HttpAuthCacheKey *key, const void *entity, SceSize entitySize,
			    HttpAuthCacheEntry **entry, SceBool anyPath, SceBool proxy)
{
	return _sceHttpAuthCacheSearch(key, entity, entitySize, 4, NULL, entry, NULL, anyPath, 0,
				       proxy);
}

/* the credentials kept for a new challenge */
__attribute__((noinline))
int _sceHttpAuthCacheFind(HttpAuthCacheKey *key, const void *entity, SceSize entitySize,
			  int type, const char *realm, HttpAuthCacheEntry **entry,
			  const HttpAuthChallenge *challenge, SceBool anyPath, SceBool proxy)
{
	return _sceHttpAuthCacheSearch(key, entity, entitySize, type, realm, entry, challenge,
				       anyPath, 1, proxy);
}

/*
 * Keeps username and password for challenge, for key's server (proxy: the proxy's), held by
 * the caller. A full cache frees one entry nothing holds instead, and keeps nothing.
 */
__attribute__((noinline))
int _sceHttpAuthCacheAdd(HttpAuthCacheKey *key, const HttpAuthChallenge *challenge,
			 const char *username, const char *password, HttpAuthCacheEntry **entry,
			 SceBool save, SceBool proxy)
{
	SceMd5Context context;
	HttpAuthCacheEntry *e, *next, *head;
	SceSize size, len;
	char *buf, *slash;
	int ret;

	*entry = NULL;
	ret = sceKernelLockLwMutex(&g_httpAuthCacheLock, 1, NULL);
	if (ret < 0)
		return ret;
	if (g_httpAuthCacheCount >= g_httpAuthCacheMax) {
		for (e = g_httpAuthCacheHead; e != NULL; e = next) {
			next = e->next;
			if (e->refCount == 0) {
				_sceHttpAuthCacheRemove(e);
				break;
			}
		}
		ret = 0;
		goto out;
	}
	e = _sceHttpDefaultMalloc(sizeof(*e));
	if (e == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	sceClibMemset(e, 0, sizeof(*e));
	size = 0;
	ret = _sceHttpUriElementCopy(NULL, proxy ? &key->proxy : key->uri, NULL, 0, &size);
	if (ret < 0)
		return ret;
	buf = _sceHttpDefaultMalloc(size);
	if (buf == NULL) {
		_sceHttpDefaultFree(e);
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	ret = _sceHttpUriElementCopy(&e->uri, proxy ? &key->proxy : key->uri, buf, size, NULL);
	if (ret < 0)
		goto fail;
	e->uriBuffer = buf;
	/* the path's directory: the space the credentials are for */
	slash = sceClibStrrchr(e->uri.path, '/');
	if (slash != NULL)
		slash[1] = '\0';
	ret = _sceHttpAuthChallengeCopy(&e->challenge, &e->challengeBuffer, challenge);
	if (ret < 0) {
fail:
		_sceHttpDefaultFree(e);
		_sceHttpDefaultFree(buf);
		goto out;
	}
	len = sceClibStrnlen(username, 0xffffffff);
	sceClibMemcpy_safe(e->username, username, len);
	e->username[len] = '\0';
	len = sceClibStrnlen(password, 0xffffffff);
	sceClibMemcpy_safe(e->password, password, len);
	e->password[len] = '\0';
	if (challenge->realmLen != 0) {
		sceMd5BlockInit(&context);
		sceMd5BlockUpdate(&context, challenge->realm, challenge->realmLen);
		sceMd5BlockResult(&context, e->realmDigest);
	}
	e->state = 0;
	e->save = save;
	e->prev = NULL;
	head = g_httpAuthCacheHead;
	e->next = head;
	if (head != NULL)
		head->prev = e;
	else
		g_httpAuthCacheTail = e;
	g_httpAuthCacheHead = e;
	g_httpAuthCacheCount++;
	*entry = e;
	e->refCount++;
out:
	sceKernelUnlockLwMutex(&g_httpAuthCacheLock, 1);
	return ret;
}
