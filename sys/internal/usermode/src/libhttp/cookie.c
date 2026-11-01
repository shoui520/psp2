/*
 * SceLibHttp cookies: the Set-Cookie headers a response brings, kept per domain, the Cookie
 * header a request sends, and the cookie jar's export and import.
 */

#include <rtc.h>
#include <net/inet.h>

#include "http_internal.h"

/* the cookie jar */
static struct HttpCookieJar {
	SceSize totalMaxSize;
	SceSize totalSize;
	SceSize maxSize;			/* of a cookie's name and value */
	SceSize maxTotalSize;			/* the most totalSize has been */
	SceSize attrMaxSize;			/* of its other strings, on top */
	HttpCookieDomain *tail;
	HttpCookieDomain *head;
	HttpCookieDomain *domains[16];
	SceKernelLwMutexWork lock;
	SceUInt32 maxNumPerDomain;
	SceUInt32 maxNum;
	SceUInt32 maxInuseNum;
	SceUInt32 num;
	SceUInt32 removedNum;
	SceBool initialized;
} g_httpCookieJar = {
	.totalMaxSize = 81920,
	.maxSize = 4096,
	.attrMaxSize = 512,
	.maxNumPerDomain = 20,
	.maxNum = 300,
};

/* the export's records: a type byte, a 16-bit length and the data */
#define HTTP_COOKIE_RECORD_EXPIRES	1
#define HTTP_COOKIE_RECORD_DOMAIN	2
#define HTTP_COOKIE_RECORD_NAME		3
#define HTTP_COOKIE_RECORD_VALUE	4
#define HTTP_COOKIE_RECORD_PATH		5
#define HTTP_COOKIE_RECORD_COMMENT	6
#define HTTP_COOKIE_RECORD_COMMENT_URL	7
#define HTTP_COOKIE_RECORD_PORTS	8
#define HTTP_COOKIE_RECORD_PORT_COUNT	9
#define HTTP_COOKIE_RECORD_END		0xff

SceSize _sceHttpCookieFreeFields(HttpCookie *cookie)
{
	SceSize size = cookie->size;

	if (cookie->comment != NULL) {
		_sceHttpDefaultFree(cookie->comment);
		cookie->comment = NULL;
	}
	if (cookie->commentUrl != NULL) {
		_sceHttpDefaultFree(cookie->commentUrl);
		cookie->commentUrl = NULL;
	}
	if (cookie->domain != NULL) {
		_sceHttpDefaultFree(cookie->domain);
		cookie->domain = NULL;
	}
	if (cookie->name != NULL) {
		_sceHttpDefaultFree(cookie->name);
		cookie->name = NULL;
	}
	if (cookie->path != NULL) {
		_sceHttpDefaultFree(cookie->path);
		cookie->path = NULL;
	}
	if (cookie->value != NULL) {
		_sceHttpDefaultFree(cookie->value);
		cookie->value = NULL;
	}
	if (cookie->ports != NULL) {
		_sceHttpDefaultFree(cookie->ports);
		cookie->ports = NULL;
	}
	return size;
}

SceSize _sceHttpCookieFreeList(HttpCookie *cookie)
{
	HttpCookie *next;
	SceSize size = 0;

	while (cookie != NULL) {
		size += _sceHttpCookieFreeFields(cookie);
		next = cookie->next;
		_sceHttpDefaultFree(cookie);
		cookie = next;
	}
	return size;
}

/* writes a record's type and length at p */
static inline __attribute__((always_inline)) void _sceHttpCookiePutRecord(char *p, SceUChar8 type,
	SceUShort16 len)
{
	sceClibMemcpy_safe(p, &type, 1);
	sceClibMemcpy_safe(p + 1, &len, 2);
}

/*
 * Writes every cookie that outlives the session into buffer: its payload's size and MD5, then
 * the payload's records. Without a buffer, only says how big the export is.
 */
int _sceHttpCookieSerialize(void *buffer, SceSize bufferSize, SceSize *exportSize)
{
	char nowText[128], expiresText[128];
	SceMd5Context md5;
	SceUChar8 digest[SCE_MD5_DIGEST_SIZE];
	SceRtcTick now;
	SceKernelLwMutexWork *lock;
	HttpCookieDomain *domain;
	HttpCookie *cookie;
	char *data;
	SceSize size, len;
	SceUInt32 payloadSize;
	SceUShort16 index = 0;
	SceBool sizeOnly = 0;
	int ret = 0;

	if (buffer == NULL)
		sizeOnly = 1;
	else if (bufferSize < 20)
		return SCE_HTTP_ERROR_OUT_OF_SIZE;

	if (!g_httpCookieJar.initialized)
		return SCE_HTTP_ERROR_BEFORE_INIT;

	ret = sceRtcGetCurrentTick(&now);
	if (ret < 0)
		return ret;

	data = (char *)buffer + 20;
	lock = &g_httpCookieJar.lock;
	ret = sceKernelLockLwMutex(lock, 1, NULL);
	if (ret < 0)
		return ret;

	size = 0;
	for (domain = g_httpCookieJar.head; domain != NULL; domain = domain->newer) {
		if (domain->deleted == 1)
			continue;
		for (cookie = domain->cookies; cookie != NULL; cookie = cookie->next) {
			if (cookie->discard == 1 || cookie->deleted == 1 || cookie->expires == 0 ||
			    cookie->expires < now.tick) {
				sceRtcFormatRFC3339LocalTime(nowText, &now);
				sceRtcFormatRFC3339LocalTime(expiresText, (SceRtcTick *)&cookie->expires);
				continue;
			}

			if (!sizeOnly) {
				if (bufferSize < cookie->size + size + 38)
					break;
				_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_EXPIRES, 8);
			}
			if (!sizeOnly)
				sceClibMemcpy_safe(data + size + 3, &cookie->expires, 8);
			size += 11;

			if (!sizeOnly)
				_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_DOMAIN,
					sceClibStrnlen(domain->name, 0xFFFFFFFF));
			size += 3;
			if (!sizeOnly)
				sceClibMemcpy_safe(data + size, domain->name, sceClibStrnlen(domain->name, 0xFFFFFFFF));
			size += sceClibStrnlen(domain->name, 0xFFFFFFFF);

			if (cookie->name != NULL) {
				if (!sizeOnly) {
					_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_NAME,
						sceClibStrnlen(cookie->name, 0xFFFFFFFF));
					sceClibMemcpy_safe(data + size + 3, cookie->name,
						sceClibStrnlen(cookie->name, 0xFFFFFFFF));
				}
				size += 3 + sceClibStrnlen(cookie->name, 0xFFFFFFFF);
			}
			if (cookie->value != NULL) {
				if (!sizeOnly) {
					_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_VALUE,
						sceClibStrnlen(cookie->value, 0xFFFFFFFF));
					sceClibMemcpy_safe(data + size + 3, cookie->value,
						sceClibStrnlen(cookie->value, 0xFFFFFFFF));
				}
				size += 3 + sceClibStrnlen(cookie->value, 0xFFFFFFFF);
			}

			if (!sizeOnly) {
				_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_PATH,
					sceClibStrnlen(cookie->path, 0xFFFFFFFF));
				sceClibMemcpy_safe(data + size + 3, cookie->path,
					sceClibStrnlen(cookie->path, 0xFFFFFFFF));
			}
			size += 3 + sceClibStrnlen(cookie->path, 0xFFFFFFFF);

			if (cookie->comment != NULL) {
				if (!sizeOnly) {
					_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_COMMENT,
						sceClibStrnlen(cookie->comment, 0xFFFFFFFF));
					sceClibMemcpy_safe(data + size + 3, cookie->comment,
						sceClibStrnlen(cookie->comment, 0xFFFFFFFF));
				}
				size += 3 + sceClibStrnlen(cookie->comment, 0xFFFFFFFF);
			}
			if (cookie->commentUrl != NULL) {
				if (!sizeOnly) {
					_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_COMMENT_URL,
						sceClibStrnlen(cookie->commentUrl, 0xFFFFFFFF));
					sceClibMemcpy_safe(data + size + 3, cookie->commentUrl,
						sceClibStrnlen(cookie->commentUrl, 0xFFFFFFFF));
				}
				size += 3 + sceClibStrnlen(cookie->commentUrl, 0xFFFFFFFF);
			}

			if (cookie->ports != NULL) {
				len = 2 * cookie->portCount;
				if (!sizeOnly) {
					_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_PORTS,
						2 * cookie->portCount);
					sceClibMemcpy_safe(data + size + 3, cookie->ports, 2 * cookie->portCount);
					len = 2 * cookie->portCount;
				}
				size += 3 + len;
				if (!sizeOnly) {
					_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_PORT_COUNT, 4);
					sceClibMemcpy_safe(data + size + 3, &cookie->portCount, 4);
				}
				size += 7;
			}

			if (!sizeOnly)
				_sceHttpCookiePutRecord(data + size, HTTP_COOKIE_RECORD_END, index);
			size += 3;
		}
	}

	if (!sizeOnly) {
		payloadSize = size;
		sceMd5BlockInit(&md5);
		sceMd5BlockUpdate(&md5, data, payloadSize);
		sceMd5BlockResult(&md5, digest);
		sceClibMemcpy_safe(buffer, &payloadSize, 4);
		sceClibMemcpy_safe((char *)buffer + 4, digest, 16);
	}
	sceKernelUnlockLwMutex(lock, 1);
	if (exportSize != NULL)
		*exportSize = size + 20;
	return ret;
}

int _sceHttpCookieExport(void *buffer, SceSize bufferSize, SceSize *exportSize)
{
	return _sceHttpCookieSerialize(buffer, bufferSize, exportSize);
}

int _sceHttpCookieCreate(HttpCookie **out, const char *name, const char *value, const char *comment,
	const char *commentUrl, const char *domain, const char *path, const SceUShort16 *ports,
	SceUInt32 portCount, const SceULong64 *expires, SceUInt32 version, SceBool secure,
	SceBool discard, int type)
{
	HttpCookie *cookie;
	SceSize maxSize, attrMaxSize, size = 0, strings = 0, len;
	int ret;

	ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
	if (ret < 0)
		return ret;
	maxSize = g_httpCookieJar.maxSize;
	attrMaxSize = g_httpCookieJar.attrMaxSize;
	sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);

	cookie = _sceHttpDefaultMalloc(sizeof(*cookie));
	if (cookie == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto error;
	}
	sceClibMemset(cookie, 0, sizeof(*cookie));

	if (name != NULL) {
		size += sceClibStrnlen(name, 0xFFFFFFFF) + 1;
		strings++;
		if (size - strings > maxSize) {
			ret = SCE_HTTP_ERROR_OUT_OF_SIZE;
			goto error;
		}
		len = sceClibStrnlen(name, 0xFFFFFFFF);
		cookie->name = _sceHttpDefaultMalloc(len + 1);
		if (cookie->name == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		sceClibMemcpy_safe(cookie->name, name, len);
		cookie->name[len] = '\0';
	}
	if (value != NULL) {
		size += sceClibStrnlen(value, 0xFFFFFFFF) + 1;
		strings++;
		if (size - strings > maxSize) {
			ret = SCE_HTTP_ERROR_OUT_OF_SIZE;
			goto error;
		}
		len = sceClibStrnlen(value, 0xFFFFFFFF);
		cookie->value = _sceHttpDefaultMalloc(len + 1);
		if (cookie->value == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		sceClibMemcpy_safe(cookie->value, value, len);
		cookie->value[len] = '\0';
	}
	if (comment != NULL) {
		size += sceClibStrnlen(comment, 0xFFFFFFFF) + 1;
		strings++;
		if (size - strings > attrMaxSize + maxSize) {
			ret = SCE_HTTP_ERROR_OUT_OF_SIZE;
			goto error;
		}
		len = sceClibStrnlen(comment, 0xFFFFFFFF);
		cookie->comment = _sceHttpDefaultMalloc(len + 1);
		if (cookie->comment == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		sceClibMemcpy_safe(cookie->comment, comment, len);
		cookie->comment[len] = '\0';
	}
	if (commentUrl != NULL) {
		size += sceClibStrnlen(commentUrl, 0xFFFFFFFF) + 1;
		strings++;
		if (size - strings > attrMaxSize + maxSize) {
			ret = SCE_HTTP_ERROR_OUT_OF_SIZE;
			goto error;
		}
		len = sceClibStrnlen(commentUrl, 0xFFFFFFFF);
		cookie->commentUrl = _sceHttpDefaultMalloc(len + 1);
		if (cookie->commentUrl == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		sceClibMemcpy_safe(cookie->commentUrl, commentUrl, len);
		cookie->commentUrl[len] = '\0';
	}
	if (domain != NULL) {
		size += sceClibStrnlen(domain, 0xFFFFFFFF) + 1;
		strings++;
		if (size - strings > attrMaxSize + maxSize) {
			ret = SCE_HTTP_ERROR_OUT_OF_SIZE;
			goto error;
		}
		len = sceClibStrnlen(domain, 0xFFFFFFFF);
		cookie->domain = _sceHttpDefaultMalloc(len + 1);
		if (cookie->domain == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		sceClibMemcpy_safe(cookie->domain, domain, len);
		cookie->domain[len] = '\0';
	}
	if (path != NULL) {
		size += sceClibStrnlen(path, 0xFFFFFFFF) + 1;
		strings++;
		if (size - strings > attrMaxSize + maxSize) {
			ret = SCE_HTTP_ERROR_OUT_OF_SIZE;
			goto error;
		}
		len = sceClibStrnlen(path, 0xFFFFFFFF);
		cookie->path = _sceHttpDefaultMalloc(len + 1);
		if (cookie->path == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		ret = sceHttpUriSweepPath(cookie->path, path, sceClibStrnlen(path, 0xFFFFFFFF) + 1);
		if (ret < 0)
			goto error;
	}
	if (ports != NULL) {
		size += 2 * portCount;
		if (size - strings > attrMaxSize + maxSize) {
			ret = SCE_HTTP_ERROR_OUT_OF_SIZE;
			goto error;
		}
		cookie->ports = _sceHttpDefaultMalloc(2 * portCount);
		if (cookie->ports == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		sceClibMemcpy_safe(cookie->ports, ports, 2 * portCount);
		cookie->portCount = portCount;
	}

	cookie->expires = *expires;
	cookie->version = version;
	cookie->type = type;
	cookie->secure = secure;
	cookie->discard = discard;
	cookie->size = size;
	*out = cookie;
	return 0;

error:
	if (cookie != NULL) {
		_sceHttpCookieFreeFields(cookie);
		_sceHttpDefaultFree(cookie);
	}
	return ret;
}

/*
 * Counts host's labels, and finds its last one (tld) and how long that one and the one before
 * it are; -1 for an empty host.
 */
static inline __attribute__((always_inline)) int _sceHttpCookieCountLabels(const char *host,
	const char **tld, SceSize *tldLen, SceSize *secondLen)
{
	const char *p, *end;
	SceSize len1 = 0, len2 = 0;
	int labels;
	char c;

	if (host == NULL || sceClibStrnlen(host, 0xFFFFFFFF) == 0)
		return -1;

	end = &host[sceClibStrnlen(host, 0xFFFFFFFF)];
	p = end - 1;
	c = *p;
	if (c == '.' && p != host) {
		p = end - 2;
		c = *p;
	}
	while (c != '.') {
		if (p == host)
			break;
		c = *--p;
		len1++;
	}
	*tld = p + 1;
	if (c != '.') {
		len1++;
		*tld = p;
	}

	labels = 1;
	if (p != host) {
		for (;;) {
			if (*--p == '.') {
				labels = 2;
				break;
			}
			len2++;
			if (p == host)
				break;
		}
	}
	while (p != host) {
		if (*--p == '.')
			labels++;
	}
	if (*host != '.')
		labels++;

	*tldLen = len1;
	*secondLen = len2;
	return labels;
}

/* the part of host after its n-th dot from the end */
static inline __attribute__((always_inline)) char *_sceHttpCookieFindLabel(char *host, int n)
{
	char *p;

	if (host == NULL || sceClibStrnlen(host, 0xFFFFFFFF) == 0)
		return NULL;

	p = &host[sceClibStrnlen(host, 0xFFFFFFFF) - 1];
	while (p != host) {
		if (n == 0)
			break;
		if (*--p == '.')
			n--;
	}
	if (n == 0)
		return p + 1;
	if (n == 1 && *p != '.')
		return p;
	return NULL;
}

/*
 * The domain a cookie of host may be set for: its last two labels, or three under a country's
 * domain (a last label that is not one of the generic seven, or a short second label).
 */
char *_sceHttpCookieGetBaseDomain(char *host)
{
	const char *tld;
	SceSize tldLen, secondLen;
	int labels, need;

	labels = _sceHttpCookieCountLabels(host, &tld, &tldLen, &secondLen);
	if (labels < 0)
		return NULL;

	if (secondLen > 2) {
		need = 2;
	} else if (tldLen == 3) {
		need = 3;
		if (sceClibStrncasecmp(tld, "com", 3) == 0 || sceClibStrncasecmp(tld, "edu", 3) == 0 ||
		    sceClibStrncasecmp(tld, "net", 3) == 0 || sceClibStrncasecmp(tld, "org", 3) == 0 ||
		    sceClibStrncasecmp(tld, "gov", 3) == 0 || sceClibStrncasecmp(tld, "mil", 3) == 0 ||
		    sceClibStrncasecmp(tld, "int", 3) == 0)
			need = 2;
	} else {
		need = 3;
	}
	if (labels < need)
		return NULL;

	return _sceHttpCookieFindLabel(host, need);
}

/* the hash bucket of host's domain */
static inline __attribute__((always_inline)) SceUInt32 _sceHttpCookieHash(char *host)
{
	const char *p;
	SceSize len;
	SceUInt32 hash = 0;

	p = _sceHttpCookieGetBaseDomain(host);
	if (p == NULL)
		p = host;
	len = sceClibStrnlen(p, 0xFFFFFFFF);
	if (len != 0 && p[len - 1] == '.')
		len--;
	if (len != 0 && *p == '.')
		p++;
	while (len != 0) {
		hash += *p++;
		len--;
	}
	return hash & 0xF;
}

/* moves domain to the tail of the list of domains, as the most recently used */
static inline __attribute__((always_inline)) void _sceHttpCookieDomainUnlink(HttpCookieDomain *domain)
{
	HttpCookieDomain *newer = domain->newer, *older = domain->older;

	if (newer != NULL) {
		newer->older = older;
		if (older != NULL)
			domain->older->newer = domain->newer;
		else
			g_httpCookieJar.head = domain->newer;
	} else {
		g_httpCookieJar.tail = domain->older;
		if (older != NULL)
			older->newer = NULL;
		else
			g_httpCookieJar.head = NULL;
	}
}

static inline __attribute__((always_inline)) void _sceHttpCookieDomainAppend(HttpCookieDomain *domain)
{
	HttpCookieDomain *tail = g_httpCookieJar.tail;

	domain->older = tail;
	domain->newer = NULL;
	if (tail != NULL)
		tail->newer = domain;
	else
		g_httpCookieJar.head = domain;
	g_httpCookieJar.tail = domain;
}

static inline __attribute__((always_inline)) int _sceHttpCookieDomainRetain(HttpCookieDomain *domain)
{
	if (domain->deleted == 1)
		return HTTP_ERROR_COOKIE_DELETED;
	domain->refCount++;
	return 0;
}

/*
 * Drops a reference to domain. The last one frees the cookies marked deleted, and the domain
 * itself once it is deleted or has no cookie left.
 */
int _sceHttpCookieDomainRelease(HttpCookieDomain *domain, SceBool lock)
{
	HttpCookie *cookie, *next, *prev = NULL;
	SceSize removed = 0, freed = 0;
	SceUInt32 hash;
	int ret;

	if (domain == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;

	if (lock) {
		ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
		if (ret < 0)
			return ret;
	}

	if (--domain->refCount == 0) {
		if (domain->deleted == 0) {
			for (cookie = domain->cookies; cookie != NULL; cookie = next) {
				next = cookie->next;
				if (cookie->deleted == 1) {
					if (prev != NULL)
						prev->next = next;
					else
						domain->cookies = next;
					removed += _sceHttpCookieFreeFields(cookie);
					_sceHttpDefaultFree(cookie);
				} else {
					prev = cookie;
				}
			}
			g_httpCookieJar.totalSize -= removed;
			if (domain->cookies == NULL)
				domain->deleted = 1;
		}
		if (domain->deleted == 1) {
			_sceHttpCookieDomainUnlink(domain);

			if (domain->next != NULL)
				domain->next->prev = domain->prev;
			if (domain->prev != NULL)
				domain->prev->next = domain->next;
			else {
				hash = _sceHttpCookieHash(domain->name);
				g_httpCookieJar.domains[hash] = domain->next;
			}
			domain->next = NULL;
			domain->prev = NULL;

			for (cookie = domain->cookies; cookie != NULL; cookie = next) {
				freed += _sceHttpCookieFreeFields(cookie);
				next = cookie->next;
				_sceHttpDefaultFree(cookie);
			}
			g_httpCookieJar.totalSize -= freed;
			if (domain->name != NULL)
				_sceHttpDefaultFree(domain->name);
			_sceHttpDefaultFree(domain);
		}
	}

	if (lock)
		sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);
	return 0;
}

/* compares two cookies' specificity: their paths' lengths, then their domains' */
static inline __attribute__((always_inline)) int _sceHttpCookieCompare(const HttpCookie *a,
	const HttpCookie *b)
{
	SceSize lenA = sceClibStrnlen(a->path, 0xFFFFFFFF), lenB = sceClibStrnlen(b->path, 0xFFFFFFFF);
	int diff = lenA - lenB;

	if (lenA == lenB)
		diff = sceClibStrnlen(a->domain, 0xFFFFFFFF) - sceClibStrnlen(b->domain, 0xFFFFFFFF);
	return diff;
}

/*
 * Adds the count cookies of list (one domain's, size bytes): makes room first, by dropping the
 * least recently used domains and, past the domain's limit, its expired and oldest cookies;
 * a cookie of list replaces the domain's cookie of the same name and path.
 */
int _sceHttpCookieAddList(HttpCookie *list, SceSize size, SceUInt32 count)
{
	HttpCookieDomain *domain, *prev = NULL, **bucket;
	HttpCookie *cookie, *old, *last;
	SceKernelLwMutexWork *lock;
	SceRtcTick now;
	SceUInt32 hash, i, n, num;
	int over, ret;

	if (list == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;

	hash = _sceHttpCookieHash(list->domain);
	lock = &g_httpCookieJar.lock;
	ret = sceKernelLockLwMutex(lock, 1, NULL);
	if (ret < 0)
		return ret;

	if (sceKernelLockLwMutex(lock, 1, NULL) >= 0) {
		if (g_httpCookieJar.totalMaxSize - g_httpCookieJar.totalSize < size ||
		    g_httpCookieJar.maxNum <= g_httpCookieJar.num || g_httpCookieJar.maxNum - g_httpCookieJar.num < count) {
			n = 10;
			if (count > 10)
				n = count;
			domain = g_httpCookieJar.head;
			for (i = 0; i < n; i++) {
				if (domain == NULL)
					break;
				if (_sceHttpCookieDomainRetain(domain) < 0) {
					n++;
				} else {
					num = domain->cookieCount;
					domain->deleted = 1;
					g_httpCookieJar.num -= num;
					g_httpCookieJar.removedNum += num;
					if (num == 0)
						n++;
					_sceHttpCookieDomainRelease(domain, 0);
				}
				domain = domain->newer;
			}
		}
		sceKernelUnlockLwMutex(lock, 1);
	}

	if (g_httpCookieJar.totalMaxSize - g_httpCookieJar.totalSize < size)
		goto unlock;

	bucket = &g_httpCookieJar.domains[hash];
	for (domain = *bucket; domain != NULL; domain = domain->next) {
		if (sceClibStrncmp(domain->name, list->domain, sceClibStrnlen(domain->name, 0xFFFFFFFF) + 1) == 0)
			break;
		prev = domain;
	}

	if (domain != NULL) {
		_sceHttpCookieDomainRetain(domain);
		over = domain->cookieCount + count - g_httpCookieJar.maxNumPerDomain;
		if (over > 0) {
			cookie = domain->cookies;
			if ((SceUInt32)_sceHttpGetSdkVersion() >= 0x1500000) {
				ret = sceRtcGetCurrentTick(&now);
				if (ret < 0)
					goto unlock;
				for (; cookie != NULL && over > 0; cookie = cookie->next) {
					if (cookie->deleted != 1 && cookie->expires < now.tick) {
						cookie->deleted = 1;
						domain->cookieCount--;
						g_httpCookieJar.num--;
						g_httpCookieJar.removedNum++;
						if (--over == 0)
							break;
					}
				}
			}
			for (cookie = domain->cookies; cookie != NULL && over > 0; cookie = cookie->next) {
				if (cookie->deleted != 1) {
					cookie->deleted = 1;
					g_httpCookieJar.num--;
					domain->cookieCount--;
					g_httpCookieJar.removedNum++;
					if (--over == 0)
						break;
				}
			}
		}

		domain->cookieCount += count;
		old = domain->cookies;
		if (old != NULL) {
			do {
				for (cookie = list; cookie != NULL; cookie = cookie->next) {
					if (cookie->deleted == 1)
						continue;
					if (cookie->name == NULL && old->name == NULL)
						break;
					if (cookie->value == NULL && old->value == NULL)
						break;
					if (cookie->name != NULL && old->name != NULL &&
					    sceClibStrncmp(cookie->name, old->name,
						sceClibStrnlen(cookie->name, 0xFFFFFFFF) + 1) == 0 &&
					    sceClibStrncmp(cookie->path, old->path,
						sceClibStrnlen(cookie->path, 0xFFFFFFFF) + 1) == 0)
						break;
				}
				if (cookie != NULL) {
					old->deleted = 1;
					domain->cookieCount--;
					g_httpCookieJar.num--;
					g_httpCookieJar.removedNum++;
				}
				last = old;
				old = old->next;
			} while (old != NULL);
			last->next = list;
		} else {
			domain->cookies = list;
		}

		_sceHttpCookieDomainUnlink(domain);
		_sceHttpCookieDomainAppend(domain);
		_sceHttpCookieDomainRelease(domain, 1);
	} else {
		domain = _sceHttpDefaultMalloc(sizeof(*domain));
		if (domain == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto unlock;
		}
		sceClibMemset(domain, 0, sizeof(*domain));
		n = sceClibStrnlen(list->domain, 0xFFFFFFFF);
		domain->name = _sceHttpDefaultMalloc(n + 1);
		if (domain->name == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto unlock;
		}
		sceClibMemcpy_safe(domain->name, list->domain, n);
		domain->name[n] = '\0';
		domain->cookies = list;
		domain->cookieCount = count;
		if (prev != NULL) {
			prev->next = domain;
			domain->prev = prev;
		} else {
			*bucket = domain;
		}
		_sceHttpCookieDomainAppend(domain);
	}

	g_httpCookieJar.totalSize += size;
	g_httpCookieJar.num += count;
	if (g_httpCookieJar.maxInuseNum < g_httpCookieJar.num)
		g_httpCookieJar.maxInuseNum = g_httpCookieJar.num;
	if (g_httpCookieJar.maxTotalSize < g_httpCookieJar.totalSize)
		g_httpCookieJar.maxTotalSize = g_httpCookieJar.totalSize;
	ret = 0;

unlock:
	sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* adds the cookies of an export, checked by its MD5, that have not expired */
int _sceHttpCookieDeserialize(const void *buffer, SceSize bufferSize)
{
	SceMd5Context md5;
	SceUChar8 digest[SCE_MD5_DIGEST_SIZE], expected[SCE_MD5_DIGEST_SIZE];
	SceRtcTick now;
	HttpCookie cookie, *created = NULL;
	const char *data = (const char *)buffer + 20;
	SceUInt32 remain;
	SceUChar8 type;
	SceUShort16 len;
	SceBool expired = 0;
	int ret;

	sceClibMemset(&cookie, 0, sizeof(cookie));
	if (!g_httpCookieJar.initialized) {
		ret = SCE_HTTP_ERROR_BEFORE_INIT;
		goto out;
	}

	ret = sceRtcGetCurrentTick(&now);
	if (ret < 0)
		goto out;

	remain = *(const SceUInt32 *)buffer;
	if (bufferSize < remain + 20)
		goto out;
	sceClibMemcpy_safe(expected, (const char *)buffer + 4, 16);
	sceMd5BlockInit(&md5);
	sceMd5BlockUpdate(&md5, data, remain);
	sceMd5BlockResult(&md5, digest);
	if (sceClibMemcmp(expected, digest, 16) != 0)
		goto out;

	while (remain != 0) {
		if (remain < 3)
			goto out;
		sceClibMemcpy_safe(&type, data, 1);
		sceClibMemcpy_safe(&len, data + 1, 2);
		remain -= 3;
		data += 3;
		if (remain < len)
			goto out;

		switch (type) {
		case 0:
			break;
		case HTTP_COOKIE_RECORD_EXPIRES:
			if (len != 8) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemcpy_safe(&cookie.expires, data, 8);
			if (cookie.expires < now.tick)
				expired = 1;
			break;
		case HTTP_COOKIE_RECORD_DOMAIN:
			if (cookie.domain != NULL)
				_sceHttpDefaultFree(cookie.domain);
			cookie.domain = _sceHttpDefaultMalloc(len + 1);
			if (cookie.domain == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemcpy_safe(cookie.domain, data, len);
			cookie.domain[len] = '\0';
			break;
		case HTTP_COOKIE_RECORD_NAME:
			if (cookie.name != NULL)
				_sceHttpDefaultFree(cookie.name);
			cookie.name = _sceHttpDefaultMalloc(len + 1);
			if (cookie.name == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemcpy_safe(cookie.name, data, len);
			cookie.name[len] = '\0';
			break;
		case HTTP_COOKIE_RECORD_VALUE:
			if (cookie.value != NULL)
				_sceHttpDefaultFree(cookie.value);
			cookie.value = _sceHttpDefaultMalloc(len + 1);
			if (cookie.value == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemcpy_safe(cookie.value, data, len);
			cookie.value[len] = '\0';
			break;
		case HTTP_COOKIE_RECORD_PATH:
			if (cookie.path != NULL)
				_sceHttpDefaultFree(cookie.path);
			cookie.path = _sceHttpDefaultMalloc(len + 1);
			if (cookie.path == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemcpy_safe(cookie.path, data, len);
			cookie.path[len] = '\0';
			break;
		case HTTP_COOKIE_RECORD_COMMENT:
			if (cookie.comment != NULL)
				_sceHttpDefaultFree(cookie.comment);
			cookie.comment = _sceHttpDefaultMalloc(len + 1);
			if (cookie.comment == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemcpy_safe(cookie.comment, data, len);
			cookie.comment[len] = '\0';
			break;
		case HTTP_COOKIE_RECORD_COMMENT_URL:
			if (cookie.commentUrl != NULL)
				_sceHttpDefaultFree(cookie.commentUrl);
			cookie.commentUrl = _sceHttpDefaultMalloc(len + 1);
			if (cookie.commentUrl == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemcpy_safe(cookie.commentUrl, data, len);
			cookie.commentUrl[len] = '\0';
			break;
		case HTTP_COOKIE_RECORD_PORTS:
			if (cookie.ports != NULL)
				_sceHttpDefaultFree(cookie.ports);
			cookie.ports = _sceHttpDefaultMalloc(len + 1);
			if (cookie.ports == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemcpy_safe(cookie.ports, data, len);
			break;
		case HTTP_COOKIE_RECORD_PORT_COUNT:
			if (len != 4)
				goto out;
			sceClibMemcpy_safe(&cookie.portCount, data, 4);
			break;
		case HTTP_COOKIE_RECORD_END:
			if (expired != 1 && cookie.domain != NULL &&
			    (cookie.name != NULL || cookie.value != NULL)) {
				ret = _sceHttpCookieCreate(&created, cookie.name, cookie.value, cookie.comment,
					cookie.commentUrl, cookie.domain, cookie.path, cookie.ports,
					cookie.portCount, &cookie.expires, cookie.version, cookie.secure, expired, 0);
				if (ret < 0)
					goto out;
				ret = _sceHttpCookieAddList(created, created->size, 1);
				if (ret < 0)
					_sceHttpCookieFreeFields(created);
			}
			_sceHttpCookieFreeFields(&cookie);
			sceClibMemset(&cookie, 0, sizeof(cookie));
			expired = 0;
			created = NULL;
			break;
		}
		data += len;
		remain -= len;
	}
	ret = 0;

out:
	_sceHttpCookieFreeFields(&cookie);
	return ret;
}

int _sceHttpCookieImport(const void *buffer, SceSize bufferSize)
{
	int ret;

	sceHttpCookieFlush();
	ret = _sceHttpCookieDeserialize(buffer, bufferSize);
	if (ret == SCE_HTTP_ERROR_OUT_OF_MEMORY)
		return 0;
	return ret;
}

void _sceHttpCookieMatchFree(HttpCookieMatch *match, HttpCookieDomainRef *refs)
{
	HttpCookieMatch *nextMatch;
	HttpCookieDomainRef *nextRef;

	while (match != NULL) {
		nextMatch = match->next;
		_sceHttpDefaultFree(match);
		match = nextMatch;
	}
	while (refs != NULL) {
		nextRef = refs->next;
		_sceHttpCookieDomainRelease(refs->domain, 1);
		_sceHttpDefaultFree(refs);
		refs = nextRef;
	}
}

/*
 * Writes the Cookie header of the cookies of match that have not expired into cookie
 * (prepared bytes); without a buffer, only says how big it is.
 */
int _sceHttpCookieMakeHeader(char *cookie, HttpCookieMatch *match, SceSize *required, SceSize prepared)
{
	HttpCookieMatchEntry *entry, *e;
	const HttpCookie *c;
	SceRtcTick now;
	SceSize size = 1, len, n, rest;
	SceUInt32 count = 0;
	SceBool write = 0;
	int ret;

	if (cookie != NULL) {
		*cookie = '\0';
		write = 1;
	} else if (required == NULL) {
		return SCE_HTTP_ERROR_INVALID_VALUE;
	}

	ret = sceRtcGetCurrentTick(&now);
	if (ret < 0)
		return ret;

	entry = &match->entries[0];
	for (e = entry->prev; e != NULL; e = e->prev)
		entry = e;

	for (; entry != NULL; entry = entry->next) {
		c = entry->cookie;
		if (c->expires < now.tick && c->expires != 0)
			continue;

		len = 0;
		if (c->name != NULL) {
			if (c->value != NULL) {
				n = sceClibStrnlen(c->name, 0xFFFFFFFF);
				len = n + sceClibStrnlen(c->value, 0xFFFFFFFF) + 3;
			} else {
				len = sceClibStrnlen(c->name, 0xFFFFFFFF) + 2;
			}
		} else if (c->value != NULL) {
			len = sceClibStrnlen(c->value, 0xFFFFFFFF) + 2;
		} else {
			goto next;
		}

		if (write == 1) {
			if (prepared < size + len)
				return SCE_HTTP_ERROR_OUT_OF_SIZE;
			rest = len;
			if (c->name != NULL) {
				if (c->value != NULL) {
					sceClibStrncat(cookie, c->name, len);
					n = sceClibStrnlen(c->name, 0xFFFFFFFF);
					sceClibStrncat(cookie, "=", len - n);
					n++;
					sceClibStrncat(cookie, c->value, len - n);
					rest = len - (n + sceClibStrnlen(c->value, 0xFFFFFFFF));
				} else {
					sceClibStrncat(cookie, c->name, len);
					rest = len - sceClibStrnlen(c->name, 0xFFFFFFFF);
				}
			} else if (c->value != NULL) {
				sceClibStrncat(cookie, c->value, len);
				rest = len - sceClibStrnlen(c->value, 0xFFFFFFFF);
			}
			sceClibStrncat(cookie, "; ", rest);
		}
next:
		size += len;
		count++;
	}

	if (write == 1 && cookie != NULL && sceClibStrnlen(cookie, 0xFFFFFFFF) > 2)
		cookie[sceClibStrnlen(cookie, 0xFFFFFFFF) - 2] = '\0';
	if (required != NULL)
		*required = size;
	if (count == 0)
		ret = SCE_HTTP_ERROR_NOT_FOUND;
	return ret;
}

/* 0 when a cookie for domain may be sent to host */
int _sceHttpCookieDomainMatch(const char *domain, const char *host)
{
	SceNetInAddr addr;
	SceSize domainLen, hostLen;
	int ret;

	domainLen = sceClibStrnlen(domain, 0xFFFFFFFF);
	hostLen = sceClibStrnlen(host, 0xFFFFFFFF);
	if (domainLen == 0 || hostLen == 0)
		return -1;

	if (domain[domainLen - 1] == '.')
		domainLen--;
	if (host[hostLen - 1] == '.')
		hostLen--;

	if (sceNetInetPton(SCE_NET_AF_INET, host, &addr) == 1) {
		if (domainLen == hostLen)
			ret = sceClibStrncasecmp(domain, host, domainLen + 1);
		else
			ret = -1;
	} else if (*domain == '.') {
		if (domainLen - 1 == hostLen)
			ret = sceClibStrncasecmp(domain + 1, host, hostLen);
		else if (domainLen <= hostLen)
			ret = sceClibStrncasecmp(domain, &host[hostLen - domainLen], domainLen);
		else
			ret = -1;
	} else if (domainLen == hostLen) {
		ret = sceClibStrncasecmp(domain, host, domainLen);
	} else {
		ret = -1;
	}
	return ret;
}

/* adds the cookies of a domain that path and scheme may send to match, in their order */
int _sceHttpCookieMatchAdd(HttpCookieMatch *match, HttpCookie *cookie, const char *path, const char *scheme)
{
	HttpCookieMatch *last, *chunk;
	HttpCookieMatchEntry *entry, *head, *p;
	SceUInt32 n;
	int ret = 0, diff;

	for (last = match; last->next != NULL; last = last->next)
		;

	for (; cookie != NULL; cookie = cookie->next) {
		if (last->count > 9) {
			chunk = _sceHttpDefaultMalloc(sizeof(*chunk));
			if (chunk == NULL)
				return SCE_HTTP_ERROR_OUT_OF_MEMORY;
			sceClibMemset(chunk, 0, sizeof(*chunk));
			last->next = chunk;
			last = chunk;
		}
		if (cookie->deleted == 1)
			continue;

		if (path != NULL && sceClibStrnlen(path, 0xFFFFFFFF) != 0)
			diff = sceClibStrncasecmp(cookie->path, path, sceClibStrnlen(cookie->path, 0xFFFFFFFF));
		else
			diff = sceClibStrncasecmp(cookie->path, "/", sceClibStrnlen(cookie->path, 0xFFFFFFFF));
		if (diff != 0)
			continue;
		if (cookie->secure == 1 &&
		    sceClibStrncasecmp(scheme, "https", sceClibStrnlen("https", 0xFFFFFFFF) + 1) != 0)
			continue;

		n = last->count;
		entry = &last->entries[n];
		entry->cookie = cookie;
		head = &match->entries[0];
		if (entry != head) {
			diff = _sceHttpCookieCompare(head->cookie, cookie);
			if (diff < 0) {
				for (p = head; p->prev != NULL; p = p->prev) {
					if (_sceHttpCookieCompare(p->prev->cookie, entry->cookie) >= 0)
						break;
				}
				entry->next = p;
				entry->prev = p->prev;
				p->prev = entry;
				n = last->count;
				if (entry->prev != NULL)
					entry->prev->next = entry;
			} else if (diff > 0) {
				for (p = head; p->next != NULL; p = p->next) {
					if (_sceHttpCookieCompare(p->next->cookie, entry->cookie) <= 0)
						break;
				}
				entry->prev = p;
				entry->next = p->next;
				p->next = entry;
				n = last->count;
				if (entry->next != NULL)
					entry->next->prev = entry;
			} else {
				entry->next = head;
				entry->prev = head->prev;
				head->prev = entry;
				n = last->count;
				if (entry->prev != NULL)
					entry->prev->next = entry;
			}
		}
		last->count = n + 1;
	}
	return ret;
}

/*
 * Collects the cookies host, path and scheme may send, holding a reference to each domain they
 * come from (refs); *match stays NULL when there is none.
 */
int _sceHttpCookieFind(HttpCookieMatch **match, char *host, const char *path, const char *scheme,
	HttpCookieDomainRef **refs)
{
	HttpCookieDomain *domain, **bucket;
	HttpCookieDomainRef *ref;
	HttpCookieMatch *found = NULL;
	SceUInt32 hash;
	SceBool first = 1;
	int ret;

	if (match == NULL || refs == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
		goto error;
	}
	*match = NULL;
	*refs = NULL;

	hash = _sceHttpCookieHash(host);
	ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
	if (ret < 0)
		goto error;

	bucket = &g_httpCookieJar.domains[hash];
	domain = *bucket;
	if (domain != NULL) {
		found = _sceHttpDefaultMalloc(sizeof(*found));
		if (found == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto unlock;
		}
		sceClibMemset(found, 0, sizeof(*found));
		do {
			if (first == 1) {
				domain = *bucket;
				first = 0;
			}
			if (domain == NULL)
				break;
			while (_sceHttpCookieDomainMatch(domain->name, host) != 0 || domain->deleted == 1) {
				domain = domain->next;
				if (domain == NULL)
					goto next;
			}

			ret = 0;
			domain->refCount++;
			ref = _sceHttpDefaultMalloc(sizeof(*ref));
			if (ref != NULL) {
				ref->domain = domain;
				ref->next = (*refs != NULL) ? *refs : NULL;
				*refs = ref;
			} else {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			}
			if (ret < 0) {
				_sceHttpCookieDomainRelease(domain, 1);
				goto unlock;
			}

			_sceHttpCookieDomainUnlink(domain);
			_sceHttpCookieDomainAppend(domain);
			_sceHttpCookieMatchAdd(found, domain->cookies, path, scheme);
next:
			if (domain == NULL)
				break;
			domain = domain->next;
		} while (domain != NULL);
		if (found->entries[0].cookie != NULL)
			*match = found;
	}
	ret = (*match != NULL) ? 0 : SCE_HTTP_ERROR_NOT_FOUND;

unlock:
	sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);
	if (ret >= 0)
		return ret;

error:
	if (refs != NULL && (found != NULL || *refs != NULL)) {
		_sceHttpCookieMatchFree(found, *refs);
		*refs = NULL;
	}
	return ret;
}

/* the Cookie header for url */
int _sceHttpCookieGet(const char *url, char *cookie, SceSize prepared, SceSize *required)
{
	SceHttpUriElement uri;
	HttpCookieMatch *match;
	HttpCookieDomainRef *refs;
	SceSize require;
	void *pool;
	int ret;

	sceClibMemset(&uri, 0, sizeof(uri));
	ret = sceHttpUriParse(NULL, url, NULL, &require, 0);
	if (ret < 0)
		return ret;

	pool = _sceHttpDefaultMalloc(require);
	if (pool != NULL) {
		ret = sceHttpUriParse(&uri, url, pool, NULL, require);
		if (ret >= 0) {
			match = NULL;
			refs = NULL;
			if (_sceHttpCookieFind(&match, uri.hostname, uri.path, uri.scheme, &refs) >= 0 &&
			    match != NULL)
				ret = _sceHttpCookieMakeHeader(cookie, match, required, prepared);
			else
				ret = SCE_HTTP_ERROR_NOT_FOUND;
			if (match != NULL || refs != NULL)
				_sceHttpCookieMatchFree(match, refs);
		}
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	if (pool != NULL)
		_sceHttpDefaultFree(pool);
	return ret;
}

int _sceHttpCookieFlush(void)
{
	HttpCookieDomain *domain, *next;
	SceUInt32 i, num;
	int ret;

	if (!g_httpCookieJar.initialized)
		return SCE_HTTP_ERROR_BEFORE_INIT;

	ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < 16; i++) {
		for (domain = g_httpCookieJar.domains[i]; domain != NULL; domain = next) {
			ret = _sceHttpCookieDomainRetain(domain);
			next = domain->next;
			if (ret >= 0) {
				num = domain->cookieCount;
				domain->deleted = 1;
				g_httpCookieJar.num -= num;
				_sceHttpCookieDomainRelease(domain, 0);
			}
		}
	}

	sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);
	return 0;
}

void _sceHttpCookieNop(void)
{
}

int _sceHttpCookieInit(void)
{
	int ret;

	if (g_httpCookieJar.initialized)
		return SCE_HTTP_ERROR_ALREADY_INITED;

	ret = sceKernelCreateLwMutex(&g_httpCookieJar.lock, "SceLibHttpCookie",
		SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret < 0)
		return ret;

	g_httpCookieJar.initialized = 1;
	g_httpCookieJar.totalMaxSize = 81920;
	g_httpCookieJar.totalSize = 0;
	g_httpCookieJar.maxSize = 4096;
	g_httpCookieJar.attrMaxSize = 512;
	if ((SceUInt32)_sceHttpGetSdkVersion() >= 0x1800000) {
		g_httpCookieJar.maxNumPerDomain = 50;
		g_httpCookieJar.maxNum = 3000;
	} else {
		g_httpCookieJar.maxNumPerDomain = 20;
		g_httpCookieJar.maxNum = 300;
	}
	g_httpCookieJar.tail = NULL;
	g_httpCookieJar.head = NULL;
	return 0;
}

int _sceHttpCookieTerm(void)
{
	HttpCookieDomain *domain, *next;
	SceUInt32 i;

	if (g_httpCookieJar.initialized != 1)
		return SCE_HTTP_ERROR_BEFORE_INIT;

	for (i = 0; i < 16; i++) {
		for (domain = g_httpCookieJar.domains[i]; domain != NULL; domain = next) {
			next = domain->next;
			g_httpCookieJar.totalSize -= _sceHttpCookieFreeList(domain->cookies);
			if (domain->name != NULL)
				_sceHttpDefaultFree(domain->name);
			_sceHttpDefaultFree(domain);
		}
		g_httpCookieJar.domains[i] = NULL;
	}

	g_httpCookieJar.initialized = 0;
	g_httpCookieJar.totalSize = 0;
	g_httpCookieJar.maxInuseNum = 0;
	g_httpCookieJar.num = 0;
	g_httpCookieJar.maxTotalSize = 0;
	sceKernelDeleteLwMutex(&g_httpCookieJar.lock);
	return 0;
}

/* frees a list of name=value pairs */
static void _sceHttpCookieAttrFree(HttpCookieAttr *attr)
{
	HttpCookieAttr *next;

	while (attr != NULL) {
		if (attr->name != NULL) {
			_sceHttpDefaultFree(attr->name);
			attr->name = NULL;
		}
		if (attr->value != NULL) {
			_sceHttpDefaultFree(attr->value);
			attr->value = NULL;
		}
		next = attr->next;
		attr->next = NULL;
		_sceHttpDefaultFree(attr);
		attr = next;
	}
}

/* whether the token at p (rest bytes) is word, alone or before a ';' or a space */
static inline __attribute__((always_inline)) SceBool _sceHttpCookieIsFlag(const char *p, SceSize rest,
	const char *word)
{
	SceSize n = sceClibStrnlen(word, 0xFFFFFFFF);

	if (n > rest || sceClibStrncasecmp(p, word, n) != 0)
		return 0;
	return n == rest || p[n] == ';' || isspace(p[n]);
}

/*
 * Splits a Set-Cookie header into its name=value pairs (last first); the secure flag, and
 * Set-Cookie2's discard, are returned apart.
 */
int _sceHttpCookieParseAttrs(const char *header, SceSize headerLen, HttpCookieAttr **attrs,
	SceBool *secure, SceBool *discard, int type)
{
	HttpCookieAttr *attr = NULL, *prev = NULL;
	SceSize pos = 0, nameEnd, nameLen, valuePos, valueLen;
	SceBool noValue;
	const char *p;
	char *q;

	*secure = 0;
	*discard = 0;
	if (headerLen == 0)
		headerLen = sceClibStrnlen(header, 0xFFFFFFFF);

	while (pos < headerLen) {
		nameLen = 0;
		noValue = 0;
		valueLen = 0;
		p = &header[pos];

		if (_sceHttpCookieIsFlag(p, headerLen - pos, "secure")) {
			*secure = 1;
			while (pos < headerLen && header[pos] != ';')
				pos++;
			if (pos < headerLen)
				pos++;
			continue;
		}
		if (type == 2 && _sceHttpCookieIsFlag(p, headerLen - pos, "discard")) {
			*discard = 1;
			while (pos < headerLen && header[pos] != ';')
				pos++;
			if (pos < headerLen)
				pos++;
			continue;
		}

		while (pos < headerLen && isspace(header[pos]))
			pos++;
		nameEnd = pos;
		while (nameEnd < headerLen && header[nameEnd] != '=' && header[nameEnd] != ';') {
			nameEnd++;
			nameLen++;
		}

		if (nameEnd == headerLen || header[nameEnd] == ';') {
			if (prev != NULL) {
				pos = nameEnd + 1;
				if (nameEnd == headerLen)
					pos = nameEnd;
				goto skip;
			}
			noValue = 1;
		}

		attr = _sceHttpDefaultMalloc(sizeof(*attr));
		if (attr == NULL)
			goto oom;
		sceClibMemset(attr, 0, sizeof(*attr));
		attr->next = prev;
		if (nameLen != 0) {
			attr->name = _sceHttpDefaultMalloc(nameLen + 1);
			if (attr->name == NULL)
				goto error;
			sceClibMemcpy_safe(attr->name, &header[pos], nameLen);
			attr->name[nameLen] = '\0';
		}

		if (noValue == 1) {
			valuePos = nameEnd;
		} else {
			valuePos = nameEnd + 1;
			if (nameEnd == headerLen)
				valuePos = nameEnd;
			while (valuePos < headerLen && isspace(header[valuePos]))
				valuePos++;
			while (valuePos + valueLen < headerLen && header[valuePos + valueLen] != ';')
				valueLen++;
		}
		if (valueLen != 0 || (nameLen != 0 && noValue == 0)) {
			attr->value = _sceHttpDefaultMalloc(valueLen + 1);
			if (attr->value == NULL)
				goto error;
			sceClibMemcpy_safe(attr->value, &header[valuePos], valueLen);
			attr->value[valueLen] = '\0';
		}

		pos = valuePos + valueLen;
		if (pos < headerLen && header[pos] == ';')
			pos++;
		if (attr->name != NULL && sceClibStrnlen(attr->name, 0xFFFFFFFF) != 0) {
			q = &attr->name[sceClibStrnlen(attr->name, 0xFFFFFFFF) - 1];
			while (isspace(*q))
				*q-- = '\0';
		}
		if (attr->value != NULL && sceClibStrnlen(attr->value, 0xFFFFFFFF) != 0) {
			q = &attr->value[sceClibStrnlen(attr->value, 0xFFFFFFFF) - 1];
			while (isspace(*q))
				*q-- = '\0';
		}
skip:
		while (pos < headerLen && isspace(header[pos]))
			pos++;
		prev = attr;
	}

	*attrs = attr;
	return 0;

error:
	if (attr != NULL) {
		_sceHttpCookieAttrFree(attr);
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
oom:
	_sceHttpCookieAttrFree(prev);
	return SCE_HTTP_ERROR_OUT_OF_MEMORY;
}

/* the unsigned number at s, of up to max digits */
static inline __attribute__((always_inline)) SceUInt32 _sceHttpCookieStrToUInt(const char *s, SceUInt32 max)
{
	SceUInt32 value = 0, i = 0;

	while (*s != '\0') {
		if (isspace(*s) || i >= max)
			break;
		value = 10 * value + (*s++ - '0');
		i++;
	}
	return value;
}

/*
 * Reads a Port attribute's list of ports into ports (when not NULL): their number when port is
 * one of them, else 0.
 */
int _sceHttpCookieParsePorts(const char *list, SceUShort16 port, SceUShort16 *ports)
{
	char number[8];
	const char *p = list, *start;
	SceUInt32 n;
	int value, count = 0, ret = 0;
	SceBool found = 0, store = 0;
	char c;

	if (ports != NULL)
		store = 1;
	c = *p;
	if (c == '"')
		c = *++p;

	while (c != '\0' || c == '"') {
		while (isspace(c))
			c = *++p;
		if (isdigit(*p)) {
			n = 0;
			start = p;
			for (c = *p; isdigit(c); c = *++p)
				n++;
			if (n > 5)
				return ret;
			sceClibMemcpy_safe(number, start, n);
			number[n] = ret;
			value = _sceHttpCookieStrToUInt(number, 10);
			if (value > 0xFFFF)
				return ret;
			if (value == port)
				found = 1;
			if (store == 1)
				*ports = value;
			ports++;
			count++;
		}
		for (c = *p; c != ','; c = *++p) {
			if (c == '\0' || c == '"')
				break;
		}
	}

	if (found)
		return count;
	return ret;
}

/*
 * Makes the cookies of a Set-Cookie header's pairs (attrs, last first) for a response of host
 * and path; type tells the header's rules (4: RFC 6265's, 2: Set-Cookie2's, 1: RFC 2109's,
 * 0: Netscape's). Returns the cookies' size.
 */
int _sceHttpCookieCreateList(HttpCookie **list, SceSize *size, SceUInt32 *count, HttpCookieAttr *attrs,
	char *host, const char *path, SceUShort16 port, SceBool secure, SceBool discard, int type)
{
	SceNetInAddr addr;
	SceRtcTick expires;
	HttpCookieAttr *pairs = NULL, *pair;
	HttpCookie *created = NULL, *cookie, *next;
	SceUShort16 *ports = NULL;
	SceUInt32 portCount = 0, version = 0;
	SceLong64 maxAge = 0, value64, sign64;
	SceBool maxAgeSeen = 0;
	char *defaultPath, *dotDomain = NULL, *name, *value, *domainAttr = NULL, *pathAttr = NULL;
	char *comment = NULL, *commentUrl = NULL, *domain;
	const char *slash, *src;
	SceSize len, i, n;
	int sign, intValue, ret = 0;

	if (list == NULL || size == NULL || count == NULL || attrs == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	*size = 0;
	*count = 0;
	expires.tick = 0;
	cookie = NULL;

	slash = sceClibStrrchr(path, '/');
	if (slash != NULL) {
		src = path;
		len = slash - path + 1;
	} else {
		src = "/";
		len = sceClibStrnlen("/", 0xFFFFFFFF);
	}
	defaultPath = _sceHttpDefaultMalloc(len + 1);
	if (defaultPath != NULL) {
		sceClibMemcpy_safe(defaultPath, src, len);
		defaultPath[len] = '\0';
	}
	if (defaultPath == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	for (; attrs != NULL; attrs = attrs->next) {
		name = attrs->name;
		value = attrs->value;

		if (attrs->next == NULL) {
			/* the first pair is the cookie's */
			pair = _sceHttpDefaultMalloc(sizeof(*pair));
			if (pair == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto error;
			}
			sceClibMemset(pair, 0, sizeof(*pair));
			if (pairs != NULL)
				pair->next = pairs;
			pairs = pair;
			if (name != NULL) {
				len = sceClibStrnlen(name, 0xFFFFFFFF);
				pair->name = _sceHttpDefaultMalloc(len + 1);
				if (pair->name == NULL) {
					ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
					goto error;
				}
				sceClibMemcpy_safe(pair->name, name, len);
				pair->name[len] = '\0';
			}
			if (value != NULL) {
				len = sceClibStrnlen(value, 0xFFFFFFFF);
				pair->value = _sceHttpDefaultMalloc(len + 1);
				if (pair->value == NULL) {
					ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
					goto error;
				}
				sceClibMemcpy_safe(pair->value, value, len);
				pair->value[len] = '\0';
			}
			continue;
		}
		if (name == NULL)
			continue;

		if (type == 1 || type == 2) {
			if (sceClibStrncasecmp(name, "Comment", 8) == 0) {
				comment = value;
				continue;
			}
			if (sceClibStrncasecmp(name, "Max-Age", 8) == 0) {
				if (value != NULL) {
					i = 0;
					sign = 1;
					intValue = 0;
					if (*value == '-') {
						sign = -1;
						i = 1;
					}
					while (value[i] != '\0') {
						if (isspace(value[i]) || i >= 21)
							break;
						intValue = 10 * intValue + (value[i] - '0');
						i++;
					}
					maxAge = sign * intValue;
					ret = sceRtcGetCurrentTick(&expires);
					if (ret < 0)
						goto error;
					ret = sceRtcTickAddSeconds(&expires, &expires, maxAge);
					if (ret < 0)
						goto error;
				}
				continue;
			}
			if (sceClibStrncasecmp(name, "Version", 8) == 0) {
				if (value != NULL)
					version = _sceHttpCookieStrToUInt(value, 10);
				continue;
			}
		}

		if (type == 4 && sceClibStrncasecmp(name, "Max-Age", 8) == 0) {
			if (value == NULL)
				continue;
			n = sceClibStrnlen(value, 0xFFFFFFFF);
			if (n == 1) {
				if (!isdigit(*value))
					continue;
			} else {
				if (!isdigit(*value) && *value != '-')
					continue;
				if (*value == '-' && !isdigit(value[1]))
					continue;
				for (i = 1; i < n; i++) {
					if (!isdigit(value[i])) {
						value[i] = '\0';
						break;
					}
				}
			}
			if (maxAgeSeen == 0) {
				i = 0;
				sign64 = 1;
				value64 = 0;
				if (*value == '-') {
					sign64 = -1;
					i = 1;
				}
				while (value[i] != '\0') {
					if (isspace(value[i]) || i >= 21)
						break;
					value64 = 10 * value64 + (value[i] - '0');
					i++;
				}
				maxAge = sign64 * value64;
				if (maxAge < 0)
					maxAge = 0;
				maxAgeSeen = 1;
				ret = sceRtcGetCurrentTick(&expires);
				if (ret < 0)
					goto error;
				ret = sceRtcTickAddSeconds(&expires, &expires, maxAge);
				if (ret < 0)
					goto error;
			}
		}

		if (type == 2) {
			if (sceClibStrncasecmp(name, "CommentURL", 11) == 0) {
				commentUrl = value;
				continue;
			}
			if (sceClibStrncasecmp(name, "Port", 5) == 0) {
				if (value == NULL)
					continue;
				ret = _sceHttpCookieParsePorts(value, port, NULL);
				if (ret > 0) {
					ports = _sceHttpDefaultMalloc(2 * ret);
					if (ports != NULL) {
						ret = _sceHttpCookieParsePorts(value, port, ports);
						if (ret > 0)
							goto ports;
					} else {
						ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
					}
					if (ports != NULL) {
						_sceHttpDefaultFree(ports);
						ports = NULL;
					}
				}
ports:
				if (ret <= 0) {
					ret = SCE_HTTP_ERROR_NOT_FOUND;
					goto error;
				}
				portCount = ret;
				continue;
			}
		} else if (type == 0) {
			if (sceClibStrncasecmp(name, "expires", 8) == 0) {
				if (value != NULL && maxAge == 0) {
					ret = sceRtcParseDateTime(&expires, value);
					if (ret < 0)
						expires.tick = 0;
				}
				continue;
			}
		} else if (type == 4 && sceClibStrncasecmp(name, "expires", 8) == 0) {
			if (value != NULL && expires.tick == 0) {
				ret = sceRtcParseDateTime(&expires, value);
				if (ret < 0)
					expires.tick = 0;
			}
			continue;
		}

		if (type == 4 && sceClibStrncasecmp(name, "secure", 7) == 0) {
			if (value == NULL || sceClibStrnlen(value, 0xFFFFFFFF) == 0)
				secure = 1;
		} else if (sceClibStrncasecmp(name, "domain", 7) == 0) {
			if (value != NULL) {
				if (_sceHttpCookieGetBaseDomain(value) == NULL)
					goto error;
				domainAttr = value;
			}
		} else if (sceClibStrncasecmp(name, "path", 5) == 0) {
			if (value != NULL)
				pathAttr = value;
		}
	}

	if (pathAttr == NULL)
		pathAttr = defaultPath;
	domain = domainAttr;
	if (domain == NULL)
		domain = host;
	if (*domain != '.' && sceNetInetPton(SCE_NET_AF_INET, domain, &addr) != 1) {
		len = sceClibStrnlen(domain, 0xFFFFFFFF);
		dotDomain = _sceHttpDefaultMalloc(len + 2);
		if (dotDomain == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		*dotDomain = '.';
		sceClibMemcpy_safe(dotDomain + 1, domain, len);
		dotDomain[len + 1] = '\0';
		domain = dotDomain;
	}
	if (_sceHttpCookieDomainMatch(domain, host) != 0) {
		ret = SCE_HTTP_ERROR_PARSE_HTTP_INVALID_VALUE;
		goto error;
	}

	for (pair = pairs; pair != NULL; pair = pair->next) {
		ret = _sceHttpCookieCreate(&cookie, pair->name, pair->value, comment, commentUrl, domain,
			pathAttr, ports, portCount, &expires.tick, version, secure, discard, type);
		if (ret < 0)
			goto error;
		++*count;
		*size += cookie->size;
		if (created != NULL)
			cookie->next = created;
		created = cookie;
	}
	*list = created;

	_sceHttpCookieAttrFree(pairs);
	if (ports != NULL)
		_sceHttpDefaultFree(ports);
	if (defaultPath != NULL)
		_sceHttpDefaultFree(defaultPath);
	if (dotDomain != NULL)
		_sceHttpDefaultFree(dotDomain);
	return *size;

error:
	_sceHttpCookieAttrFree(pairs);
	for (cookie = created; cookie != NULL; cookie = next) {
		_sceHttpCookieFreeFields(cookie);
		next = cookie->next;
		_sceHttpDefaultFree(cookie);
	}
	if (ports != NULL)
		_sceHttpDefaultFree(ports);
	if (defaultPath != NULL)
		_sceHttpDefaultFree(defaultPath);
out:
	if (dotDomain != NULL)
		_sceHttpDefaultFree(dotDomain);
	return ret;
}

/* makes the cookies of a response's Set-Cookie header */
int _sceHttpCookieParse(const char *header, SceSize headerLen, char *host, const char *path,
	SceUShort16 port, HttpCookie **list, SceUInt32 *count, SceSize *size)
{
	HttpCookieAttr *attrs;
	SceBool secure, discard;
	int ret;

	if (header == NULL || host == NULL || path == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	if (list == NULL || count == NULL || size == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	*size = 0;
	*count = 0;
	attrs = NULL;
	*list = NULL;

	ret = _sceHttpCookieParseAttrs(header, headerLen, &attrs, &secure, &discard, 0);
	if (ret >= 0)
		ret = _sceHttpCookieCreateList(list, size, count, attrs, host, path, port, secure,
			discard, 4);
	if (ret >= 0) {
		if (attrs != NULL)
			_sceHttpCookieAttrFree(attrs);
		return ret;
	}

	if (attrs != NULL)
		_sceHttpCookieAttrFree(attrs);
	if (*list != NULL) {
		_sceHttpCookieFreeList(*list);
		*list = NULL;
	}
	return ret;
}

int _sceHttpCookieParseHeader(const char *header, SceSize headerLen, char *host, const char *path,
	HttpCookie **list, SceUInt32 *count, SceSize *size)
{
	int ret;

	ret = _sceHttpCookieParse(header, headerLen, host, path, 0, list, count, size);
	if (ret != SCE_HTTP_ERROR_BEFORE_INIT && ret != SCE_HTTP_ERROR_INVALID_VALUE &&
	    ret != SCE_HTTP_ERROR_OUT_OF_MEMORY)
		return 0;
	return ret;
}

int _sceHttpCookieAdd(HttpCookie *list, SceSize size, SceUInt32 count)
{
	int ret = SCE_HTTP_ERROR_INVALID_VALUE;

	if (list != NULL)
		return _sceHttpCookieAddList(list, size, count);
	return ret;
}

int _sceHttpCookieCallRecvCallback(HttpRequest *req, const char *url, const char *cookieHeader,
	SceSize headerLen)
{
	int id = req->object.id;
	SceBool enabled = req->tmpl.cookieEnabled;
	SceHttpCookieRecvCallback callback = req->tmpl.cookieRecvCallback;
	void *userArg = req->tmpl.cookieRecvUserArg;
	int ret = SCE_HTTP_ERROR_INVALID_VALUE;

	if (enabled) {
		ret = 0;
		if (callback != NULL)
			ret = callback(id, url, cookieHeader, headerLen, userArg);
	}
	return ret;
}

int _sceHttpCookieCallSendCallback(HttpRequest *req, const char *url, const char *cookieHeader)
{
	int id = req->object.id;
	int ret = SCE_HTTP_ERROR_INVALID_VALUE;
	SceBool enabled = req->tmpl.cookieEnabled;
	SceHttpCookieSendCallback callback = req->tmpl.cookieSendCallback;
	void *userArg = req->tmpl.cookieSendUserArg;

	if (enabled) {
		ret = 0;
		if (callback != NULL)
			ret = callback(id, url, cookieHeader, userArg);
	}
	return ret;
}

/* sceHttpAddCookie: the cookies of a Set-Cookie header, as if url's response had brought it */
int _sceHttpCookieAddUrl(const char *url, const char *cookie, SceSize cookieLength)
{
	SceHttpUriElement uri;
	HttpCookie *list;
	SceUInt32 count;
	SceSize size, require;
	void *pool;
	int ret = 0, err;

	if (url == NULL)
		return ret;

	ret = sceHttpUriParse(NULL, url, NULL, &require, 0);
	if (ret < 0)
		return ret;
	pool = _sceHttpDefaultMalloc(require);
	if (pool == NULL)
		return ret;

	ret = sceHttpUriParse(&uri, url, pool, NULL, require);
	if (ret >= 0) {
		err = _sceHttpCookieParse(cookie, cookieLength, uri.hostname, uri.path, uri.port, &list,
			&count, &size);
		if (err >= 0) {
			if (list != NULL)
				ret = _sceHttpCookieAddList(list, size, count);
			else
				ret = SCE_HTTP_ERROR_INVALID_VALUE;
			err = ret;
		}
		if (err < 0) {
			_sceHttpCookieFreeList(list);
			ret = err;
		}
	}
	_sceHttpDefaultFree(pool);
	return ret;
}

SceUInt32 _sceHttpCookieGetMaxNumPerDomain(void)
{
	SceUInt32 num;
	int ret;

	ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
	if (ret < 0)
		return ret;
	num = g_httpCookieJar.maxNumPerDomain;
	sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);
	return num;
}

int _sceHttpCookieSetMaxNumPerDomain(SceUInt32 num)
{
	int ret;

	ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
	if (ret < 0)
		return ret;
	g_httpCookieJar.maxNumPerDomain = num;
	sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);
	return ret;
}

int _sceHttpCookieSetMaxNum(SceUInt32 num)
{
	int ret;

	ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
	if (ret < 0)
		return ret;
	g_httpCookieJar.maxNum = num;
	sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);
	return ret;
}

int _sceHttpCookieSetMaxSize(SceUInt32 size)
{
	int ret;

	ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
	if (ret < 0)
		return ret;
	g_httpCookieJar.maxSize = size;
	sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);
	return ret;
}

int _sceHttpCookieSetTotalMaxSize(SceUInt32 size)
{
	int ret;

	ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
	if (ret < 0)
		return ret;
	g_httpCookieJar.totalMaxSize = size;
	sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);
	return ret;
}

int _sceHttpCookieGetStats(SceHttpCookieStats *stats)
{
	int ret;

	if (stats == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;

	ret = sceKernelLockLwMutex(&g_httpCookieJar.lock, 1, NULL);
	if (ret >= 0) {
		stats->currentInuseSize = g_httpCookieJar.totalSize;
		stats->currentInuseNum = g_httpCookieJar.num;
		stats->maxInuseSize = g_httpCookieJar.maxTotalSize;
		stats->maxInuseNum = g_httpCookieJar.maxInuseNum;
		stats->removedNum = g_httpCookieJar.removedNum;
		sceKernelUnlockLwMutex(&g_httpCookieJar.lock, 1);
	}
	return ret;
}

void _sceHttpCookieNop2(void)
{
}

void _sceHttpCookieNop3(void)
{
}
