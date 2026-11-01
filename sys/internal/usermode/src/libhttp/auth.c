/*
 * SceLibHttp authentication: parsing a server's challenge (WWW-Authenticate,
 * Proxy-Authenticate) and building the Authorization header for Basic and Digest.
 */

#include "http_internal.h"

/*
 * Reads the value of one challenge parameter (name=value or name="value") into
 * value and valueLen; returns how far the parameter reaches.
 */
__attribute__((noinline))
SceSize _sceHttpAuthParseParam(const char *param, SceSize size, const char **value, SceSize *valueLen)
{
	SceSize i, pos;
	SceSize quotedLen = 0, plainLen = 0;

	*valueLen = 0;
	*value = NULL;
	for (i = 0; i < size && param[i] != '='; i++)
		;
	pos = i + 1;
	if (pos >= size)
		return pos;
	if (param[i + 1] == '"') {
		pos = i + 2;
		if (pos <= size) {
			*value = &param[pos];
			for (; pos < size && param[pos] != '"'; pos++)
				*valueLen = ++quotedLen;
			for (; pos < size && param[pos] != ',' && param[pos] != '\n'; pos++)
				;
		}
	} else {
		*value = &param[pos];
		for (; pos < size && param[pos] != ',' && param[pos] != '\n'; pos++)
			*valueLen = ++plainLen;
	}
	return pos;
}

/* Parses a challenge header's value; proxy is set for Proxy-Authenticate. */
__attribute__((noinline))
int _sceHttpAuthParseChallenge(HttpAuthChallenge *challenge, const char *header, SceSize size,
			   int proxy)
{
	const char *param, *stale = NULL;
	SceSize staleLen = 0;
	SceSize pos;
	int type = SCE_HTTP_AUTH_BASIC;
	int ret = 0;

	if (challenge == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	sceClibMemset(challenge, 0, sizeof(*challenge));
	if (header == NULL || *header == '\0')
		ret = SCE_HTTP_ERROR_BAD_RESPONSE;
	if (sceClibStrncasecmp(header, "Basic", sceClibStrnlen("Basic", 0xffff)) == 0) {
		if (proxy == 1)
			type = SCE_HTTP_AUTH_RESERVED0;
		challenge->type = type;
		pos = sceClibStrnlen("Basic", 0xffff);
	} else if (sceClibStrncasecmp(header, "Digest", sceClibStrnlen("Digest", 0xffff)) == 0) {
		type = SCE_HTTP_AUTH_DIGEST;
		if (proxy == 1)
			type = SCE_HTTP_AUTH_RESERVED1;
		challenge->type = type;
		pos = sceClibStrnlen("Digest", 0xffff);
	} else {
		return SCE_HTTP_ERROR_UNKNOWN_AUTH_TYPE;
	}

	while (pos < size) {
		param = &header[pos];
		if (sceClibStrncasecmp(param, "realm", sceClibStrnlen("realm", 0xffff)) == 0) {
			pos += _sceHttpAuthParseParam(param, size - pos, &challenge->realm,
						  &challenge->realmLen);
		} else if (sceClibStrncasecmp(param, "nonce", sceClibStrnlen("nonce", 0xffff)) == 0) {
			pos += _sceHttpAuthParseParam(param, size - pos, &challenge->nonce,
						  &challenge->nonceLen);
		} else if (sceClibStrncasecmp(param, "opaque", sceClibStrnlen("opaque", 0xffff)) == 0) {
			pos += _sceHttpAuthParseParam(param, size - pos, &challenge->opaque,
						  &challenge->opaqueLen);
		} else if (sceClibStrncasecmp(param, "stale", sceClibStrnlen("stale", 0xffff)) == 0) {
			pos += _sceHttpAuthParseParam(param, size - pos, &stale, &staleLen);
			if (sceClibStrncasecmp(stale, "true", sceClibStrnlen("true", 0xffff)) == 0)
				challenge->stale = 1;
			else if (sceClibStrncasecmp(stale, "false", sceClibStrnlen("false", 0xffff)) == 0)
				challenge->stale = 0;
		} else if (sceClibStrncasecmp(param, "algorithm",
					      sceClibStrnlen("algorithm", 0xffff)) == 0) {
			pos += _sceHttpAuthParseParam(param, size - pos, &challenge->algorithm,
						  &challenge->algorithmLen);
		} else if (sceClibStrncasecmp(param, "qop", sceClibStrnlen("qop", 0xffff)) == 0) {
			pos += _sceHttpAuthParseParam(param, size - pos, &challenge->qop,
						  &challenge->qopLen);
			/* "auth,auth-int" is taken as "auth" */
			if (challenge->qop != NULL &&
			    challenge->qopLen > sceClibStrnlen("auth", 0xffff) &&
			    sceClibStrncasecmp(challenge->qop, "auth",
					       sceClibStrnlen("auth", 0xffff)) == 0)
				challenge->qopLen = sceClibStrnlen("auth", 0xffff);
		} else if (sceClibStrncasecmp(param, "domain", sceClibStrnlen("domain", 0xffff)) == 0) {
			pos += _sceHttpAuthParseParam(param, size - pos, &challenge->domain,
						  &challenge->domainLen);
		} else {
			pos++;
		}
	}
	return ret;
}

/*
 * The Basic credentials ("Basic " and base64 of "username:password") into buf, or, without
 * buf, the size they need into needed.
 */
__attribute__((noinline))
int _sceHttpAuthBasic(const HttpAllocator *allocator, char *buf, SceSize *needed, SceSize size,
		  const char *username, const char *password)
{
	char *plain;
	SceSize len, encoded;
	SceBool write = 1;
	int ret = 0;

	if (username == NULL || password == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	if (buf != NULL)
		*buf = '\0';
	else
		write = 0;
	len = sceClibStrnlen(username, SCE_HTTP_USERNAME_MAX_SIZE) +
	      sceClibStrnlen(password, SCE_HTTP_PASSWORD_MAX_SIZE);
	encoded = 4 * ((len + 3) / 3) + 7;
	if (!write) {
		if (needed != NULL)
			*needed = encoded;
		return ret;
	}
	if (encoded > size)
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;
	plain = _sceHttpMalloc(allocator->malloc, len + 2);
	if (plain != NULL) {
		sceClibMemset(plain, 0, len + 2);
		sceClibSnprintf(plain, len + 2, "%s%s%s", username, ":", password);
		sceClibStrlcpy(buf, "Basic ", size);
		buf[sceBase64Encoder(buf + 6, plain, len + 1) + 6] = '\0';
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	if (plain != NULL)
		_sceHttpFree(allocator->free, plain);
	return ret;
}

/* An MD5 digest in lower-case hex. */
__attribute__((noinline))
void _sceHttpAuthHex(const SceUChar8 *digest, char *hex)
{
	SceUShort16 i = 0;
	SceUChar8 c;

	do {
		c = digest[i] >> 4;
		if (c > 9)
			hex[2 * i] = c + 'a' - 10;
		else
			hex[2 * i] = c + '0';
		c = digest[i] & 0xf;
		if (c > 9)
			hex[2 * i + 1] = c + 'a' - 10;
		else
			hex[2 * i + 1] = c + '0';
		i++;
	} while (i < SCE_MD5_DIGEST_SIZE);
	hex[2 * SCE_MD5_DIGEST_SIZE] = '\0';
}

/* One name="value", parameter of the Digest credentials; len gets its length. */
__attribute__((noinline))
int _sceHttpAuthDigestParam(char *buf, SceSize size, SceSize *len, const char *name, SceSize nameLen,
			const char *value, SceBool write)
{
	SceSize valueLen = sceClibStrnlen(value, size - (nameLen + 2));

	if (len != NULL)
		*len = nameLen + valueLen + 5;
	if (!write)
		return 0;
	if (nameLen + 2 + valueLen + 4 > size)
		return SCE_HTTP_ERROR_OUT_OF_SIZE;
	sceClibSnprintf(buf, size, "%s%s%s%s%s%s%s", name, "=", "\"", value, "\"", ",", " ");
	return 0;
}

/* the request method's name in the Digest computation */
static inline __attribute__((always_inline)) const char *_sceHttpAuthMethodName(int method)
{
	switch (method) {
	case SCE_HTTP_METHOD_GET:
		return "GET";
	case SCE_HTTP_METHOD_POST:
		return "POST";
	case SCE_HTTP_METHOD_HEAD:
		return "HEAD";
	case SCE_HTTP_METHOD_OPTIONS:
		return "OPTIONS";
	case SCE_HTTP_METHOD_PUT:
		return "PUT";
	case SCE_HTTP_METHOD_DELETE:
		return "DELETE";
	case SCE_HTTP_METHOD_TRACE:
		return "TRACE";
	case SCE_HTTP_METHOD_CONNECT:
		return "CONNECT";
	}
	return NULL;
}

/*
 * The Digest credentials (RFC 2617) into buf, or, without buf, the size they need into
 * needed: response = MD5(HA1:nonce:nc:cnonce:qop:HA2), HA1 = MD5(username:realm:password)
 * (md5-sess: MD5(HA1:nonce:cnonce)), HA2 = MD5(method:uri[:MD5(entity)] for auth-int).
 */
__attribute__((noinline))
int _sceHttpAuthDigest(char *buf, SceSize *needed, SceSize size, const char *username,
		   const char *password, const char *uri, int method, const void *entity,
		   SceSize entitySize, const HttpAuthDigest *digest)
{
	SceMd5Context entityContext, cnonceContext, ha1Context, context;
	SceUChar8 entityDigest[SCE_MD5_DIGEST_SIZE], cnonceDigest[SCE_MD5_DIGEST_SIZE];
	SceUChar8 ha1[SCE_MD5_DIGEST_SIZE], ha2[SCE_MD5_DIGEST_SIZE], response[SCE_MD5_DIGEST_SIZE];
	char entityHex[2 * SCE_MD5_DIGEST_SIZE + 1], cnonce[2 * SCE_MD5_DIGEST_SIZE + 1];
	char ha1Hex[2 * SCE_MD5_DIGEST_SIZE + 1], ha2Hex[2 * SCE_MD5_DIGEST_SIZE + 1];
	char responseHex[2 * SCE_MD5_DIGEST_SIZE + 1];
	char nonceCount[9];
	const char *name, *realm, *nonce, *algorithm, *qop;
	SceUInt32 time;
	SceSize len = 0, pos;
	SceBool write = buf != NULL;
	int ret;

	if (username == NULL || password == NULL || digest->realm == NULL || digest->qop == NULL ||
	    digest->nonce == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	name = _sceHttpAuthMethodName(method);
	if (name == NULL)
		return SCE_HTTP_ERROR_UNKNOWN_METHOD;

	if (entity != NULL) {
		sceMd5BlockInit(&entityContext);
		sceMd5BlockUpdate(&entityContext, entity, entitySize);
		sceMd5BlockResult(&entityContext, entityDigest);
		_sceHttpAuthHex(entityDigest, entityHex);
	}
	time = SceThreadmgr_47F6DE49();
	sceMd5BlockInit(&cnonceContext);
	sceMd5BlockUpdate(&cnonceContext, &time, sizeof(time));
	sceMd5BlockResult(&cnonceContext, cnonceDigest);
	_sceHttpAuthHex(cnonceDigest, cnonce);

	algorithm = digest->algorithm;
	realm = digest->realm;
	nonce = digest->nonce;
	sceMd5BlockInit(&ha1Context);
	sceMd5BlockUpdate(&ha1Context, username, sceClibStrnlen(username, SCE_HTTP_USERNAME_MAX_SIZE));
	sceMd5BlockUpdate(&ha1Context, ":", 1);
	sceMd5BlockUpdate(&ha1Context, realm, sceClibStrnlen(realm, 0xffff));
	sceMd5BlockUpdate(&ha1Context, ":", 1);
	sceMd5BlockUpdate(&ha1Context, password, sceClibStrnlen(password, SCE_HTTP_PASSWORD_MAX_SIZE));
	sceMd5BlockResult(&ha1Context, ha1);
	if (sceClibStrncasecmp(algorithm, "md5-sess", 9) == 0) {
		sceMd5BlockInit(&ha1Context);
		sceMd5BlockUpdate(&ha1Context, ha1, SCE_MD5_DIGEST_SIZE);
		sceMd5BlockUpdate(&ha1Context, ":", 1);
		sceMd5BlockUpdate(&ha1Context, nonce, sceClibStrnlen(nonce, 0xffff));
		sceMd5BlockUpdate(&ha1Context, ":", 1);
		sceMd5BlockUpdate(&ha1Context, cnonce, sceClibStrnlen(cnonce, 0xffff));
		sceMd5BlockResult(&ha1Context, ha1);
	}
	_sceHttpAuthHex(ha1, ha1Hex);
	sceClibSnprintf(nonceCount, sizeof(nonceCount), "%08x", digest->nonceCount);

	qop = digest->qop;
	nonce = digest->nonce;
	sceMd5BlockInit(&context);
	sceMd5BlockUpdate(&context, name, sceClibStrnlen(name, 7));
	sceMd5BlockUpdate(&context, ":", 1);
	sceMd5BlockUpdate(&context, uri, sceClibStrnlen(uri, 0x3fff));
	if (sceClibStrncasecmp(qop, "auth-int", 9) == 0) {
		sceMd5BlockUpdate(&context, ":", 1);
		sceMd5BlockUpdate(&context, entityHex, 2 * SCE_MD5_DIGEST_SIZE);
	}
	sceMd5BlockResult(&context, ha2);
	_sceHttpAuthHex(ha2, ha2Hex);

	sceMd5BlockInit(&context);
	sceMd5BlockUpdate(&context, ha1Hex, 2 * SCE_MD5_DIGEST_SIZE);
	sceMd5BlockUpdate(&context, ":", 1);
	sceMd5BlockUpdate(&context, nonce, sceClibStrnlen(nonce, 0xffff));
	sceMd5BlockUpdate(&context, ":", 1);
	/* without a qop Sony's never finishes the response digest */
	if (*qop != '\0') {
		sceMd5BlockUpdate(&context, nonceCount, sceClibStrnlen(nonceCount, 0xffff));
		sceMd5BlockUpdate(&context, ":", 1);
		sceMd5BlockUpdate(&context, cnonce, sceClibStrnlen(cnonce, 0xffff));
		sceMd5BlockUpdate(&context, ":", 1);
		sceMd5BlockUpdate(&context, qop, sceClibStrnlen(qop, 0xffff));
		sceMd5BlockUpdate(&context, ":", 1);
		sceMd5BlockUpdate(&context, ha2Hex, 2 * SCE_MD5_DIGEST_SIZE);
		sceMd5BlockResult(&context, response);
	}
	_sceHttpAuthHex(response, responseHex);

	if (write)
		sceClibStrlcpy(buf, "Digest ", size);
	ret = _sceHttpAuthDigestParam(buf + 7, size - 7, &len, "username", 8, username, write);
	if (ret < 0)
		return ret;
	pos = len + 7;
	ret = _sceHttpAuthDigestParam(buf + pos, size - pos, &len, "realm", 5, digest->realm, write);
	if (ret < 0)
		return ret;
	pos += len;
	ret = _sceHttpAuthDigestParam(buf + pos, size - pos, &len, "nonce", 5, digest->nonce, write);
	if (ret < 0)
		return ret;
	pos += len;
	ret = _sceHttpAuthDigestParam(buf + pos, size - pos, &len, "uri", 3, uri, write);
	if (ret < 0)
		return ret;
	pos += len;
	ret = _sceHttpAuthDigestParam(buf + pos, size - pos, &len, "qop", 3, digest->qop, write);
	if (ret < 0)
		return ret;
	pos += len;
	ret = _sceHttpAuthDigestParam(buf + pos, size - pos, &len, "nc", 2, nonceCount, write);
	if (ret < 0)
		return ret;
	pos += len;
	ret = _sceHttpAuthDigestParam(buf + pos, size - pos, &len, "cnonce", 6, cnonce, write);
	if (ret < 0)
		return ret;
	pos += len;
	ret = _sceHttpAuthDigestParam(buf + pos, size - pos, &len, "response", 8, responseHex, write);
	if (ret < 0)
		return ret;
	pos += len;
	if (digest->opaque != NULL && sceClibStrnlen(digest->opaque, 1) != 0) {
		ret = _sceHttpAuthDigestParam(buf + pos, size - pos, &len, "opaque", 6, digest->opaque,
					  write);
		if (ret < 0)
			return ret;
		pos += len;
	}
	if (write)
		buf[pos] = '\0';
	if (needed != NULL)
		*needed = pos + 1;
	return ret;
}

/*
 * The Digest state of a challenge, in one buffer of the allocator's: the algorithm and qop
 * the response is computed with, and copies of the challenge's strings.
 */
__attribute__((noinline))
int _sceHttpAuthDigestInit(const HttpAllocator *allocator, HttpAuthDigest *digest,
			   const HttpAuthChallenge *challenge)
{
	char *algorithm = NULL, *qop = NULL, *buf, *p;
	SceSize i = 0, qopLen, size, used, nameLen;
	SceBool md5Sess = 0, authInt = 0, auth = 0;
	int ret;

	if (challenge->algorithm != NULL) {
		algorithm = _sceHttpMalloc(allocator->malloc, challenge->algorithmLen + 1);
		if (algorithm == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibMemcpy_safe(algorithm, challenge->algorithm, challenge->algorithmLen);
		algorithm[challenge->algorithmLen] = '\0';
		if (sceClibStrncasecmp(algorithm, "md5-sess", sceClibStrnlen("md5-sess", 0xffff)) == 0) {
			ret = 0;
			qopLen = challenge->qopLen;
			md5Sess = 1;
		} else {
			ret = 0;
			if (sceClibStrncasecmp(algorithm, "md5", sceClibStrnlen("md5", 0xffff)) == 0)
				md5Sess = 0;
			qopLen = challenge->qopLen;
		}
	} else {
		ret = 0;
		qopLen = challenge->qopLen;
	}

	if (challenge->qop != NULL) {
		qop = _sceHttpMalloc(allocator->malloc, qopLen + 1);
		if (qop == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibMemcpy_safe(qop, challenge->qop, challenge->qopLen);
		qop[challenge->qopLen] = '\0';
		qopLen = challenge->qopLen;
		while (i < qopLen) {
			if (sceClibStrncasecmp(&qop[i], "auth-int", sceClibStrnlen("auth-int", 0xffff)) == 0) {
				qopLen = challenge->qopLen;
				authInt = 1;
			} else {
				if (sceClibStrncasecmp(&qop[i], "auth", sceClibStrnlen("auth", 0xffff)) == 0)
					auth = 1;
				qopLen = challenge->qopLen;
			}
			/* Sony's stops at a comma without stepping over it */
			for (; i < qopLen && qop[i] != ','; i++)
				;
		}
	}

	nameLen = 4;
	if (md5Sess == 1)
		nameLen = 9;
	if (authInt == 1)
		nameLen += 5;
	else if (auth == 1)
		nameLen += 9;
	size = challenge->realmLen + qopLen + challenge->opaqueLen + challenge->domainLen +
	       challenge->nonceLen + challenge->algorithmLen + nameLen + 3 + 3;
	buf = _sceHttpMalloc(allocator->malloc, size);
	if (buf == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	sceClibMemset(buf, 0, size);
	digest->algorithm = buf;
	if (md5Sess == 1) {
		sceClibStrlcpy(buf, "md5-sess", size);
		p = buf + 9;
		used = 9;
	} else {
		sceClibStrlcpy(buf, "md5", size);
		used = 4;
		p = buf + 4;
	}
	if (authInt == 1) {
		digest->qop = p;
		sceClibStrlcpy(p, "auth-int", size - used);
		used += 9;
		p = &buf[used];
	} else if (auth == 1) {
		digest->qop = p;
		sceClibStrlcpy(p, "auth", size - used);
		used += 5;
		p = &buf[used];
	} else {
		digest->qop = NULL;
	}
	digest->algorithm = p;
	sceClibMemcpy_safe(p, challenge->algorithm, challenge->algorithmLen);
	used += challenge->algorithmLen;
	p = &buf[used + 1];
	digest->domain = p;
	sceClibMemcpy_safe(p, challenge->domain, challenge->domainLen);
	used += challenge->domainLen;
	p = &buf[used + 2];
	digest->nonce = p;
	sceClibMemcpy_safe(p, challenge->nonce, challenge->nonceLen);
	used += challenge->nonceLen + 3;
	digest->opaque = &buf[used];
	sceClibMemcpy_safe(&buf[used], challenge->opaque, challenge->opaqueLen);
	used += challenge->opaqueLen;
	p = &buf[used + 1];
	digest->qop = p;
	sceClibMemcpy_safe(p, challenge->qop, challenge->qopLen);
	p = &buf[challenge->qopLen + 2 + used];
	digest->realm = p;
	sceClibMemcpy_safe(p, challenge->realm, challenge->realmLen);
	digest->buffer = buf;
	digest->nonceCount = 1;
	digest->allocator = allocator;
out:
	if (algorithm != NULL)
		_sceHttpFree(allocator->free, algorithm);
	if (qop != NULL)
		_sceHttpFree(allocator->free, qop);
	return ret;
}

__attribute__((noinline))
void _sceHttpAuthDigestTerm(HttpAuthDigest *digest)
{
	if (digest->buffer != NULL) {
		_sceHttpFree(digest->allocator->free, digest->buffer);
		digest->buffer = NULL;
	}
}

/*
 * The settings an id names, locked: a template's under the library's lock, a connection's
 * under its own, a request's under its mutex.
 */
#define HTTP_SETTINGS_BEGIN(id, tmpl, conn, req, settings, ret)			\
	do {									\
		ret = _sceHttpGetObjectType(id);				\
		if (ret < 0)							\
			goto out;						\
		if (ret <= HTTP_OBJECT_TEMPLATE) {				\
			ret = _sceHttpGetTemplate(&tmpl, id);			\
			if (ret < 0)						\
				goto out;					\
			ret = _sceHttpLock(0);					\
			settings = tmpl;					\
			if (ret < 0)						\
				goto out;					\
		} else if (ret <= HTTP_OBJECT_CONNECTION) {			\
			ret = _sceHttpGetConnection(&conn, id);			\
			if (ret < 0)						\
				goto out;					\
			ret = _sceHttpConnectionLock(conn, NULL, 0, NULL);	\
			if (ret < 0)						\
				goto out;					\
			settings = tmpl = &conn->tmpl;				\
		} else if (ret > HTTP_OBJECT_REQUEST) {				\
			ret = SCE_HTTP_ERROR_INVALID_ID;			\
			goto out;						\
		} else {							\
			ret = _sceHttpGetRequest(&req, id);			\
			if (ret < 0)						\
				goto out;					\
			ret = sceKernelLockLwMutex(&req->lock, 1, NULL);	\
			if (ret < 0)						\
				goto out;					\
			settings = tmpl = &req->tmpl;				\
		}								\
	} while (0)

#define HTTP_SETTINGS_END(tmpl, conn, req)					\
	do {									\
		if (req != NULL) {						\
			_sceHttpRequestRelease(req);				\
		} else if (conn != NULL) {					\
			_sceHttpConnectionUnlock(conn, NULL);			\
			_sceHttpConnectionRelease(conn);			\
		} else if (tmpl != NULL) {					\
			_sceHttpUnlock(0);					\
			_sceHttpTemplateRelease(tmpl);				\
		}								\
	} while (0)

__attribute__((noinline))
int _sceHttpSetAuthInfoCallback(int id, SceHttpAuthInfoCallback cbfunc, void *userArg)
{
	HttpTemplate *tmpl = NULL, *settings;
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	int ret;

	HTTP_SETTINGS_BEGIN(id, tmpl, conn, req, settings, ret);
	settings->authInfoCallback = cbfunc;
	ret = 0;
	tmpl->authInfoUserArg = userArg;
	if (req != NULL) {
		sceKernelUnlockLwMutex(&req->lock, 1);
		ret = 0;
	}
out:
	HTTP_SETTINGS_END(tmpl, conn, req);
	return ret;
}

__attribute__((noinline))
int _sceHttpSetAuthEnabled(int id, const SceBool *basic, const SceBool *digest)
{
	HttpTemplate *tmpl = NULL, *settings;
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	int ret;

	HTTP_SETTINGS_BEGIN(id, tmpl, conn, req, settings, ret);
	if (basic != NULL) {
		settings->basicAuthEnabled = *basic;
		settings = tmpl;
	}
	if (digest != NULL)
		settings->digestAuthEnabled = *digest;
	ret = 0;
	if (req != NULL) {
		sceKernelUnlockLwMutex(&req->lock, 1);
		ret = 0;
	}
out:
	HTTP_SETTINGS_END(tmpl, conn, req);
	return ret;
}

__attribute__((noinline))
int _sceHttpGetAuthEnabled(int id, SceBool *basic, SceBool *digest)
{
	HttpTemplate *tmpl = NULL, *settings;
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	int ret;

	HTTP_SETTINGS_BEGIN(id, tmpl, conn, req, settings, ret);
	if (basic != NULL)
		*basic = settings->basicAuthEnabled;
	if (digest != NULL)
		*digest = settings->digestAuthEnabled;
	ret = 0;
	if (req != NULL) {
		sceKernelUnlockLwMutex(&req->lock, 1);
		ret = 0;
	}
out:
	HTTP_SETTINGS_END(tmpl, conn, req);
	return ret;
}

/* parses url into uri, its strings in a buffer of the request's allocator */
static inline __attribute__((always_inline)) int _sceHttpAuthParseUrl(HttpRequest *req,
								    SceHttpUriElement *uri,
								    void **buffer, SceSize *require)
{
	int ret;

	ret = sceHttpUriParse(NULL, req->url, NULL, require, 0);
	if (ret >= 0) {
		*buffer = _sceHttpMalloc(req->tmpl.allocator.malloc, *require);
		if (*buffer != NULL)
			ret = sceHttpUriParse(uri, req->url, *buffer, require, *require);
		else
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	return ret;
}

/* a lookup key for the proxy the request's connection goes through */
static inline __attribute__((always_inline)) void _sceHttpAuthProxyKey(HttpAuthCacheKey *key,
								    HttpRequest *req)
{
	HttpConnection *conn;

	sceClibMemset(&key->proxy, 0, sizeof(key->proxy));
	conn = req->connection;
	key->proxy.hostname = conn->tmpl.proxyServer;
	key->proxy.port = conn->tmpl.proxyPort;
	key->proxy.scheme = "http";
}

/*
 * After a response: on 401 or 407, finds the credentials for the server's challenge (the
 * template's, the cache's or the application's callback's) and sets the request's
 * Authorization or Proxy-Authorization; 1 when the request is to be sent again.
 */
__attribute__((noinline))
int _sceHttpAuthCheckResponse(HttpRequest *req)
{
	HttpAuthChallenge challenge;
	HttpAuthDigest digest;
	HttpAuthCacheKey key;
	SceHttpUriElement uri;
	SceUChar8 *entity = NULL;
	SceSize entitySize = 0;
	SceBool save;
	SceSize prepare, require, valueLen;
	const char *fieldValue, *header;
	SceSize headerLen;
	SceUInt32 sdkVersion;
	int proxyChallenge;
	char *realm = NULL, *username, *password;
	void *uriBuffer;
	HttpAuthCacheEntry *entry;
	SceBool proxy = 0;
	SceBool needEntity;
	int status, ret;

	if (req->state < 15 && req->state != 6)
		return 0;
	status = req->statusCode;
	if (status != 407) {
		if (req->proxyAuthCache != NULL) {
			_sceHttpAuthCacheConfirm(req->proxyAuthCache);
			_sceHttpAuthCacheRelease(req->proxyAuthCache);
			req->proxyAuthCache = NULL;
			req->authRetries = 0;
			status = req->statusCode;
		}
		if (status != 401) {
			if (req->authCache != NULL) {
				_sceHttpAuthCacheConfirm(req->authCache);
				_sceHttpAuthCacheRelease(req->authCache);
				req->authCache = NULL;
			}
			return 0;
		}
	}

	header = req->responseHeader;
	headerLen = req->responseHeaderLen;
	proxyChallenge = 0;
	fieldValue = NULL;
	if (status == 401) {
		if (sceHttpParseResponseHeader(header, headerLen, "WWW-Authenticate", &fieldValue,
					       &valueLen) < 0) {
			ret = SCE_HTTP_ERROR_NOT_FOUND;
			goto parsed;
		}
	} else if (status == 407) {
		if (sceHttpParseResponseHeader(header, headerLen, "Proxy-Authenticate", &fieldValue,
					       &valueLen) < 0) {
			ret = SCE_HTTP_ERROR_NOT_FOUND;
			goto parsed;
		}
		proxyChallenge = 1;
	} else {
		ret = SCE_HTTP_ERROR_NOT_FOUND;
		goto parsed;
	}
	ret = _sceHttpAuthParseChallenge(&challenge, fieldValue, valueLen, proxyChallenge);
parsed:
	if (ret == SCE_HTTP_ERROR_NOT_FOUND) {
		if (req->authCache != NULL) {
			_sceHttpAuthCacheDelete(req->authCache);
			req->authCache = NULL;
		}
		if (req->proxyAuthCache != NULL) {
			_sceHttpAuthCacheDelete(req->proxyAuthCache);
			req->proxyAuthCache = NULL;
		}
		return 0;
	}
	sdkVersion = _sceHttpGetSdkVersion();
	status = req->statusCode;
	if (sdkVersion >= 0x2500000 && ret == SCE_HTTP_ERROR_UNKNOWN_AUTH_TYPE)
		return 0;
	if (ret < 0)
		return ret;

	needEntity = 0;
	if (challenge.stale == 1) {
		if (status == 407) {
			_sceHttpAuthCacheConfirm(req->proxyAuthCache);
			_sceHttpAuthCacheRelease(req->proxyAuthCache);
			req->proxyAuthCache = NULL;
			req->authRetries = 0;
		} else if (status == 401) {
			_sceHttpAuthCacheConfirm(req->authCache);
			_sceHttpAuthCacheRelease(req->authCache);
			req->authCache = NULL;
		}
	}
	if ((challenge.type == SCE_HTTP_AUTH_BASIC || challenge.type == SCE_HTTP_AUTH_DIGEST) &&
	    req->authCache != NULL) {
		_sceHttpAuthCacheDelete(req->authCache);
		req->authCache = NULL;
	}
	if ((challenge.type == SCE_HTTP_AUTH_RESERVED0 || challenge.type == SCE_HTTP_AUTH_RESERVED1) &&
	    req->proxyAuthCache != NULL) {
		_sceHttpAuthCacheDelete(req->proxyAuthCache);
		req->proxyAuthCache = NULL;
	}
	if ((SceUInt32)challenge.type > SCE_HTTP_AUTH_DIGEST) {
		if ((SceUInt32)challenge.type > SCE_HTTP_AUTH_RESERVED1)
			return 0;
		if (!req->tmpl.proxyAuthEnabled)
			return 0;
		proxy = 1;
	} else if (challenge.type != SCE_HTTP_AUTH_BASIC) {
		if (!req->tmpl.digestAuthEnabled)
			return 0;
	} else if (!req->tmpl.basicAuthEnabled) {
		return 0;
	}
	if (req->authRetries >= req->tmpl.maxAuthRetries)
		return 0;

	if (!proxy && req->tmpl.username != NULL && sceClibStrnlen(req->tmpl.username, 1) != 0 &&
	    req->tmpl.password != NULL && sceClibStrnlen(req->tmpl.password, 1) != 0) {
		/* the template's own credentials, tried once */
		if (req->authRetries != 0)
			return 0;
		username = req->tmpl.username;
		uriBuffer = NULL;
		password = req->tmpl.password;
		entry = NULL;
		ret = _sceHttpAuthParseUrl(req, &uri, &uriBuffer, &require);
		if (ret < 0)
			return ret;
		key.request = req;
		key.header = NULL;
		key.uri = &uri;
		ret = _sceHttpAuthCacheAdd(&key, &challenge, username, password, &entry, 0, 0);
		if (ret < 0) {
			_sceHttpFree(req->tmpl.allocator.free, uriBuffer);
			return ret;
		}
		req->authCache = entry;
		_sceHttpFree(req->tmpl.allocator.free, uriBuffer);
	} else {
		if (req->tmpl.authInfoCallback == NULL)
			return 0;
		realm = _sceHttpMalloc(req->tmpl.allocator.malloc, challenge.realmLen + 1);
		if (realm == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibMemcpy_safe(realm, challenge.realm, challenge.realmLen);
		realm[challenge.realmLen] = needEntity;
		uriBuffer = NULL;
		entry = NULL;
		ret = _sceHttpAuthParseUrl(req, &uri, &uriBuffer, &prepare);
		if (ret < 0)
			goto out;
		key.request = req;
		if (proxy == 1)
			_sceHttpAuthProxyKey(&key, req);
		key.header = NULL;
		key.uri = &uri;
		ret = _sceHttpAuthCacheFind(&key, entity, entitySize, challenge.type, realm, &entry,
					    &challenge, needEntity, proxy);
		if (ret < 0) {
			_sceHttpFree(req->tmpl.allocator.free, uriBuffer);
			goto out;
		}
		username = req->username;
		password = req->password;
		if (entry != NULL) {
			ret = _sceHttpAuthCacheGetCredentials(entry, req->username, req->password);
			if (ret < 0) {
				_sceHttpFree(req->tmpl.allocator.free, uriBuffer);
				goto out;
			}
		} else {
			if (req->tmpl.authInfoCallback(req->object.id, challenge.type, realm,
						       req->username, req->password, needEntity,
						       &entity, &entitySize, &save,
						       req->tmpl.authInfoUserArg) < 0) {
				_sceHttpFree(req->tmpl.allocator.free, uriBuffer);
				ret = 0;
				goto out;
			}
			key.request = req;
			if (proxy == 1)
				_sceHttpAuthProxyKey(&key, req);
			key.header = NULL;
			key.uri = &uri;
			ret = _sceHttpAuthCacheAdd(&key, &challenge, req->username, req->password,
						   &entry, save, proxy);
			if (ret < 0) {
				_sceHttpFree(req->tmpl.allocator.free, uriBuffer);
				goto out;
			}
		}
		if (proxy == 1)
			req->proxyAuthCache = entry;
		else
			req->authCache = entry;
		_sceHttpFree(req->tmpl.allocator.free, uriBuffer);
	}

	ret = _sceHttpRequestRemoveAuthHeaders(req, 1,
					      challenge.type == SCE_HTTP_AUTH_RESERVED0 ||
					      challenge.type == SCE_HTTP_AUTH_RESERVED1,
					      (SceUInt32)challenge.type <= SCE_HTTP_AUTH_DIGEST);
	if (ret < 0)
		goto out;
	if (challenge.type == SCE_HTTP_AUTH_BASIC || challenge.type == SCE_HTTP_AUTH_RESERVED0) {
		if (challenge.type != SCE_HTTP_AUTH_BASIC) {
			ret = _sceHttpRequestSetBasicAuth(req, username, password);
			sceClibMemset(username, 0, SCE_HTTP_USERNAME_MAX_SIZE);
			sceClibMemset(password, 0, SCE_HTTP_PASSWORD_MAX_SIZE);
		} else {
			ret = _sceHttpRequestSetBasicAuth(req, username, password);
		}
		if (ret < 0) {
			if (ret != SCE_HTTP_ERROR_OUT_OF_MEMORY)
				ret = 0;
			goto out;
		}
	} else if (challenge.type == SCE_HTTP_AUTH_DIGEST ||
		   challenge.type == SCE_HTTP_AUTH_RESERVED1) {
		ret = _sceHttpAuthDigestInit(&req->tmpl.allocator, &digest, &challenge);
		if (ret < 0) {
			if (ret != SCE_HTTP_ERROR_OUT_OF_MEMORY)
				ret = 0;
			goto out;
		}
		if (challenge.type == SCE_HTTP_AUTH_DIGEST) {
			ret = _sceHttpRequestSetDigestAuth(req, username, password, entity, entitySize,
							   &digest, needEntity);
		} else {
			ret = _sceHttpRequestSetDigestAuth(req, username, password, entity, entitySize,
							   &digest, 1);
			sceClibMemset(username, 0, SCE_HTTP_USERNAME_MAX_SIZE);
			sceClibMemset(password, 0, SCE_HTTP_PASSWORD_MAX_SIZE);
		}
		if (digest.buffer != NULL)
			_sceHttpFree(digest.allocator->free, digest.buffer);
		if (ret < 0) {
			if (ret != SCE_HTTP_ERROR_OUT_OF_MEMORY)
				ret = 0;
			goto out;
		}
	}
	ret = 1;
	req->authRetries++;
out:
	if (realm != NULL)
		_sceHttpFree(req->tmpl.allocator.free, realm);
	return ret;
}

/* Proxy-Authorization from the cache, before a request through a proxy is sent */
__attribute__((noinline))
int _sceHttpAuthAddProxyAuthorization(HttpRequest *req)
{
	HttpAuthCacheKey key;
	SceHttpUriElement uri;
	HttpAuthCacheEntry *entry = NULL;
	char *header = NULL;
	int ret;

	sceClibMemset(&uri, 0, sizeof(uri));
	uri.path = req->path;
	key.request = req;
	_sceHttpAuthProxyKey(&key, req);
	key.header = &header;
	key.uri = &uri;
	ret = _sceHttpAuthCacheLookup(&key, NULL, 0, &entry, 1, 1);
	if (ret >= 0 && entry != NULL)
		ret = _sceHttpAddRequestHeader(req->object.id, "Proxy-Authorization", *key.header, 0);
	if (header != NULL)
		_sceHttpAuthCacheFreeHeader(header);
	if (entry != NULL)
		_sceHttpAuthCacheRelease(entry);
	return ret;
}

/* Authorization from the cache, before a request is sent */
__attribute__((noinline))
int _sceHttpAuthAddAuthorization(HttpRequest *req)
{
	HttpAuthCacheKey key;
	SceHttpUriElement uri;
	SceSize require;
	HttpAuthCacheEntry *entry = NULL;
	char *header = NULL;
	void *uriBuffer = NULL;
	int ret;

	sceClibMemset(&uri, 0, sizeof(uri));
	ret = _sceHttpAuthParseUrl(req, &uri, &uriBuffer, &require);
	if (ret >= 0) {
		key.request = req;
		key.header = &header;
		key.uri = &uri;
		ret = _sceHttpAuthCacheLookup(&key, NULL, 0, &entry, 0, 0);
		if (ret >= 0 && entry != NULL)
			ret = _sceHttpAddRequestHeader(req->object.id, "Authorization", *key.header, 0);
	}
	if (uriBuffer != NULL)
		_sceHttpFree(req->tmpl.allocator.free, uriBuffer);
	if (header != NULL)
		_sceHttpAuthCacheFreeHeader(header);
	if (entry != NULL)
		_sceHttpAuthCacheRelease(entry);
	return ret;
}
