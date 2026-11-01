/*
 * SceLibHttp's URIs: escaping and unescaping, parsing a URI into its elements (in a pool
 * the caller gives), building one from its elements, merging a relative URI with a base
 * one, and the copy of an element set a connection keeps.
 */

#include "http_internal.h"

/* what escaping does with a character: kept as it is, or written as "%XX" */
#define URI_CHAR_UNRESERVED	1
#define URI_CHAR_ESCAPE		3

#define U	URI_CHAR_UNRESERVED
#define E	URI_CHAR_ESCAPE

/*
 * The character classes, by SDK version: before 2.50 RFC 2396's unreserved characters
 * without its marks ('*' kept, '~' escaped), from 2.50 on RFC 3986's unreserved ones.
 */
static const signed char g_uriCharClassRfc2396[256] = {
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, U, E, E, U, U, E,	/*  !"#$%&'()*+,-./ */
	U, U, U, U, U, U, U, U, U, U, E, E, E, E, E, E,	/* 0123456789:;<=>? */
	E, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,	/* @ABCDEFGHIJKLMNO */
	U, U, U, U, U, U, U, U, U, U, U, E, E, E, E, U,	/* PQRSTUVWXYZ[\]^_ */
	E, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,	/* `abcdefghijklmno */
	U, U, U, U, U, U, U, U, U, U, U, E, E, E, E, E,	/* pqrstuvwxyz{|}~  */
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
};

static const signed char g_uriCharClass[256] = {
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, U, U, E,	/*  !"#$%&'()*+,-./ */
	U, U, U, U, U, U, U, U, U, U, E, E, E, E, E, E,	/* 0123456789:;<=>? */
	E, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,	/* @ABCDEFGHIJKLMNO */
	U, U, U, U, U, U, U, U, U, U, U, E, E, E, E, U,	/* PQRSTUVWXYZ[\]^_ */
	E, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,	/* `abcdefghijklmno */
	U, U, U, U, U, U, U, U, U, U, U, E, E, E, U, E,	/* pqrstuvwxyz{|}~  */
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
	E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
};

#undef U
#undef E

#define URI_SDK_RFC3986		0x2500000	/* the SDK version escaping follows RFC 3986 from */

static int uriIsHexDigit(int c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/* a hex digit's value, -1 for any other character */
static int uriHexValue(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

int sceHttpUriEscape(char *out, SceSize *require, SceSize prepare, const char *in)
{
	SceSize len = 0;
	int ret = 0;
	SceBool write = 1;
	const char *p;

	if (out == NULL) {
		if (require == NULL) {
			ret = SCE_HTTP_ERROR_INVALID_VALUE;
			goto out;
		}
		write = 0;
	}
	for (p = in; *p != '\0'; p++) {
		int class;

		if ((SceUInt32)_sceHttpGetSdkVersion() >= URI_SDK_RFC3986)
			class = g_uriCharClass[(unsigned char)*p];
		else
			class = g_uriCharClassRfc2396[(unsigned char)*p];
		if (class == URI_CHAR_ESCAPE) {
			len += 3;
			if (len > prepare && write == 1) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			if (write == 1) {
				out[0] = '%';
				out[1] = "0123456789ABCDEF"[(unsigned char)*p >> 4];
				out[2] = "0123456789ABCDEF"[*p & 0xf];
				out += 3;
			}
		} else {
			if (++len > prepare && write == 1) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			if (write == 1)
				*out++ = *p;
		}
	}
	len++;
	if (len > prepare && write == 1) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	if (write == 1)
		*out = '\0';
	if (require != NULL)
		*require = len;
out:
	return ret;
}

/*
 * Before 2.50, a character outside the old unreserved set that is not '+' makes the URI
 * invalid; '+' is kept as it is, not turned into a space.
 */
int sceHttpUriUnescape(char *out, SceSize *require, SceSize prepare, const char *in)
{
	SceBool write = 1;
	const char *p;
	SceSize len = 0;
	int ret = 0;
	int n;

	if (in == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
		goto out;
	}
	if (out == NULL) {
		if (require == NULL) {
			ret = SCE_HTTP_ERROR_INVALID_VALUE;
			goto out;
		}
		write = 0;
	}
	n = 0;
	for (p = in; *p != '\0'; p++) {
		if (*p == '%') {
			if (p[1] == '\0' || !uriIsHexDigit((unsigned char)p[1]) ||
			    p[2] == '\0' || !uriIsHexDigit((unsigned char)p[2]))
				goto invalid;
			n += 3;
		} else if ((SceUInt32)_sceHttpGetSdkVersion() >= URI_SDK_RFC3986) {
			n++;
		} else {
			if (*p != '+' && g_uriCharClassRfc2396[*p] != URI_CHAR_UNRESERVED)
				goto invalid;
			n++;
		}
	}
	if (n < 0) {
invalid:
		ret = SCE_HTTP_ERROR_INVALID_URL;
		goto out;
	}

	for (p = in; *p != '\0'; ) {
		if (++len > prepare && write == 1) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		if (*p == '%') {
			if (write == 1)
				*out++ = uriHexValue((unsigned char)p[1]) * 16 +
					 uriHexValue((unsigned char)p[2]);
			p += 3;
		} else {
			if (write == 1)
				*out++ = *p;
			p++;
		}
	}
	len++;
	if (len > prepare && write == 1) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	if (write == 1)
		*out = '\0';
	if (require != NULL)
		*require = len;
out:
	return ret;
}

/* the default port of a scheme: 443 for https, 80 for http, 0 for any other */
static void uriGetDefaultPort(const char *scheme, SceUShort16 *port)
{
	char lower[128];
	SceUShort16 value;
	char *p;
	int len;
	int i;

	if (scheme == NULL) {
		*port = 0;
		return;
	}
	len = sceClibStrnlen(scheme, 32);
	if (len > 32) {
		*port = 0;
		return;
	}
	p = lower;
	if (len > 0) {
		for (i = len; i != 0; i--)
			*p++ = sceClibTolower(*scheme++);
	}
	*p = '\0';
	if (sceClibStrncasecmp(lower, "HTTPS", sceClibStrnlen("HTTPS", 32)) == 0)
		value = 443;
	else if (sceClibStrncasecmp(lower, "HTTP", sceClibStrnlen("HTTP", 32)) == 0)
		value = 80;
	else if (sceClibStrncasecmp(lower, "TTP", sceClibStrnlen("TTP", 32)) == 0)
		value = 80;
	else
		value = 0;
	*port = value;
}

/*
 * The path src of size bytes (its NUL counted) into dst without its "./" and "../"
 * segments; a path that does not start with '/' is copied as it is.
 */
static int uriSweepPath(char *dst, const char *src, SceSize size)
{
	SceSize i;
	SceSize n;
	char *end;
	char *slash;

	if (size == 0)
		return 0;
	if (src[0] != '/') {
		sceClibMemcpy_safe(dst, src, size - 1);
		dst[size - 1] = '\0';
		return 0;
	}
	dst[0] = src[0];
	dst[1] = '\0';
	end = dst;
	for (i = 1; i < size - 1; ) {
		if (src[i] == '.' && src[i + 1] == '.' && src[i + 2] == '/') {
			if (end != dst) {
				*end = '\0';
				end = sceClibStrrchr(dst, '/');
				if (end != NULL)
					end[1] = '\0';
			}
			i += 3;
		} else if (src[i] == '.' && src[i + 1] == '/') {
			i += 2;
		} else {
			slash = sceClibStrchr(&src[i], '/');
			if (slash == NULL) {
				n = size - i - 1;
			} else {
				n = slash - &src[i] + 1;
				if (n > size - i - 1)
					n = size - i - 1;
			}
			sceClibMemcpy_safe(end + 1, &src[i], n);
			end += n;
			end[1] = '\0';
			i += n;
		}
	}
	return 0;
}

/* the characters of a URI's scheme, after its first one */
static int uriIsSchemeChar(unsigned char c)
{
	return isalnum(c) || c == '+' || c == '-' || c == '.';
}

/* the characters of a URI's userinfo */
static int uriIsUserinfoChar(unsigned char c)
{
	return isalnum(c) || c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
	       c == '*' || c == '\'' || c == '(' || c == ')' || c == '%' || c == ';' ||
	       c == ':' || c == '&' || c == '=' || c == '+' || c == '$' || c == ',';
}

/* the characters of a host name */
static int uriIsHostChar(unsigned char c)
{
	return isalnum(c) || c == '.' || c == '-' || c == '_';
}

/*
 * The parse, element by element: each one is copied into the pool after the one before it,
 * with its NUL. Without a pool (out or pool NULL) only the size needed is counted.
 */
int sceHttpUriParse(SceHttpUriElement *out, const char *srcUrl, void *pool, SceSize *require,
		    SceSize prepare)
{
	char *buf = pool;
	SceBool write = 1;
	SceBool hasPort = 0;
	SceUShort16 port = 0;
	SceShort16 noPort = 0;
	const char *p;
	const char *src;
	char *dst;
	SceSize schemeSize, userSize, hostSize, pathSize, querySize, fragmentSize;
	SceSize offset, pos, passOffset, passSize;
	SceSize userLen, passLen, need;
	SceBool hasPassword;
	int len;
	int ret;
	int c;
	int i;

	if (srcUrl == NULL)
		return SCE_HTTP_ERROR_INVALID_URL;
	if (pool != NULL && out != NULL) {
		sceClibMemset(out, 0, sizeof(*out));
	} else {
		if (require == NULL)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		write = 0;
	}

	/* scheme ":" */
	{
		SceSize max = sceClibStrnlen(srcUrl, 32);

		i = 0;
		p = srcUrl;
		c = *p;
		while (uriIsSchemeChar(c) && i != max) {
			c = *++p;
			i++;
		}
	}
	if (isdigit(*srcUrl) || srcUrl[i] != ':') {
		schemeSize = 1;
		if (write == 1) {
			if (prepare <= 1) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto scheme_done;
			}
			buf[0] = '\0';
		}
		ret = 0;
	} else {
		schemeSize = i + 1;
		if (write == 1) {
			if (schemeSize > prepare) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto scheme_done;
			}
			sceClibMemcpy_safe(buf, srcUrl, i);
			buf[i] = '\0';
		}
		ret = i + 1;
	}
scheme_done:
	if (ret < 0)
		return ret;
	if (write == 1)
		out->scheme = buf;

	/* "//" authority, or an opaque URI */
	pos = ret;
	p = &srcUrl[pos];
	dst = &buf[schemeSize];
	if (p != NULL) {
		int slashes = 0;

		if (srcUrl[pos] == '/') {
			const char *q = p;

			do
				slashes++;
			while (*++q == '/');
		}
		if (slashes >= 2) {
			p = &srcUrl[ret + 2];
			pos = ret + 2;
			goto userinfo;
		}
	}
	if (write == 1)
		out->opaque = 1;

userinfo:
	/* userinfo "@" */
	userLen = 0;
	hasPassword = 0;
	passOffset = 0;
	passSize = 0;
	if (p == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_URL;
		goto userinfo_done;
	}
	i = 0;
	c = *p;
	if (c == '\0')
		goto no_userinfo;
	src = p;
	while (c != '@') {
		if (!hasPassword && c == ':') {
			userLen = i;
			hasPassword = 1;
			passOffset = i + 1;
		} else if (!uriIsUserinfoChar(c)) {
			goto no_userinfo;
		}
		c = *++p;
		i++;
		if (c == '\0')
			goto no_userinfo;
	}
	if (hasPassword) {
		passSize = i - userLen;
		passLen = i + 1 - userLen - 2;
		need = passOffset + passSize;
	} else {
		userLen = i;
		passLen = 0;
		passOffset = i + 1;
		passSize = 1;
		need = i + 2;
	}
	if (write == 1) {
		if (prepare - schemeSize < need) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto userinfo_done;
		}
		sceClibMemcpy_safe(dst, src, userLen);
		dst[userLen] = '\0';
		sceClibMemcpy_safe(&dst[passOffset], &src[passOffset], passLen);
		dst[passLen + passOffset] = '\0';
	}
	ret = i + 1;
	goto userinfo_done;
no_userinfo:
	passOffset = 1;
	passSize = 1;
	if (write == 1) {
		if (prepare - schemeSize < 2) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto userinfo_done;
		}
		buf[schemeSize] = '\0';
		dst[1] = '\0';
	}
	ret = 0;
userinfo_done:
	if (ret < 0)
		return ret;
	if (write == 1) {
		out->username = dst;
		out->password = &buf[schemeSize + passOffset];
	}

	/* host */
	pos += ret;
	offset = passSize + passOffset + schemeSize;
	dst = &buf[offset];
	p = &srcUrl[pos];
	len = 0;
	if (p == NULL) {
		len = SCE_HTTP_ERROR_INVALID_URL;
		goto host_done;
	}
	c = srcUrl[pos];
	if (c != '.') {
		const char *q = p;

		while (uriIsHostChar(c)) {
			c = *++q;
			len++;
		}
	}
	if (write == 1) {
		if (len + 1 > prepare - offset) {
			len = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto host_done;
		}
		sceClibMemcpy_safe(dst, p, len);
		dst[len] = '\0';
	}
	hostSize = len + 1;
host_done:
	if (len < 0)
		return len;
	if (write == 1)
		out->hostname = dst;

	/* ":" port */
	pos += len;
	offset += hostSize;
	if (srcUrl[pos] == ':') {
		pos++;
		hasPort = 1;
	}
	p = &srcUrl[pos];
	if (hasPort == 1) {
		len = 0;
		port = 0;
		if (p != NULL) {
			const char *q = p;
			int value = 0;

			while (isdigit(*q)) {
				if ((SceSize)len >= 5) {
					len = SCE_HTTP_ERROR_INVALID_URL;
					goto port_done;
				}
				value = *q++ - '0' + value * 10;
				len++;
			}
			if (value <= 0x10000 && (p[len] == '/' || p[len] == '\0'))
				port = value;
			else
				len = SCE_HTTP_ERROR_INVALID_URL;
		} else {
			len = SCE_HTTP_ERROR_INVALID_URL;
		}
port_done:
		if (len < 0)
			return len;
		if (len != 0) {
			if (write == 1)
				out->port = port;
			pos += len;
			p = &srcUrl[pos];
		} else {
			noPort = 1;
		}
	}
	dst = &buf[offset];
	if ((hasPort == 0 || noPort == 1) && write == 1) {
		uriGetDefaultPort(out->scheme, &port);
		out->port = port;
	}

	/* path */
	if (p == NULL) {
		len = SCE_HTTP_ERROR_INVALID_URL;
		goto path_done;
	}
	len = 0;
	{
		const char *q = p;

		c = *q;
		while (c != '?' && c != '#' && c != '\0') {
			c = *++q;
			len++;
		}
	}
	if (write == 1) {
		if (len + 1 > prepare - offset) {
			len = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto path_done;
		}
		sceClibMemcpy_safe(dst, p, len);
		dst[len] = '\0';
		uriSweepPath(dst, p, len + 1);
	}
	pathSize = len + 1;
path_done:
	if (len < 0)
		return len;
	if (write == 1)
		out->path = dst;

	/* "?" query */
	pos += len;
	offset += pathSize;
	p = &srcUrl[pos];
	dst = &buf[offset];
	if (p == NULL) {
		len = SCE_HTTP_ERROR_INVALID_URL;
		goto query_done;
	}
	len = 0;
	{
		const char *q = p;

		c = srcUrl[pos];
		while (c != '#' && c != '\0') {
			c = *++q;
			len++;
		}
	}
	if (write == 1) {
		if (len + 1 > prepare - offset) {
			len = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto query_done;
		}
		sceClibMemcpy_safe(dst, p, len);
		dst[len] = '\0';
	}
	querySize = len + 1;
query_done:
	if (len < 0)
		return len;
	if (write == 1)
		out->query = dst;

	/* "#" fragment */
	p = &srcUrl[pos + len];
	offset += querySize;
	dst = &buf[offset];
	if (p == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_URL;
		goto fragment_done;
	}
	len = 0;
	if (*p != '\0') {
		const char *q = p;

		do
			len++;
		while (*++q != '\0');
	}
	if (write == 1) {
		if (len + 1 > prepare - offset) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto fragment_done;
		}
		sceClibMemcpy_safe(dst, p, len);
		dst[len] = '\0';
	}
	ret = len;
	fragmentSize = len + 1;
fragment_done:
	if (ret < 0)
		return ret;
	if (write == 1)
		out->fragment = dst;
	if (require != NULL)
		*require = fragmentSize + offset;
	return 0;
}

/*
 * The URI of the elements option names (SCE_HTTP_URI_BUILD_WITH_*): the size needed is
 * counted first and given back in require, then the URI is written when it fits. The port
 * is left out when it is the scheme's default one.
 */
int sceHttpUriBuild(char *out, SceSize *require, SceSize prepare, const SceHttpUriElement *srcElement,
		    SceUInt32 option)
{
	SceUShort16 defaultPort;
	char portStr[8];
	SceSize schemeLen = 0, userLen = 0, passLen = 0, hostLen = 0, portLen = 0;
	SceSize pathLen = 0, queryLen = 0, fragmentLen = 0;
	SceSize size = 0;
	SceSize n = 0;
	SceBool write = 1;
	SceBool hasUserinfo = 0;
	SceBool defaultPortOmitted = 0;
	int ret;

	if (srcElement == NULL)
		return SCE_HTTP_ERROR_INVALID_URL;
	if (out == NULL) {
		if (require == NULL)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		write = 0;
	}

	if ((option & SCE_HTTP_URI_BUILD_WITH_SCHEME) && srcElement->scheme != NULL) {
		schemeLen = sceClibStrnlen(srcElement->scheme, 32);
		size = schemeLen + 1;
	}
	if (srcElement->opaque != 1)
		size += 2;
	if ((option & SCE_HTTP_URI_BUILD_WITH_USERNAME) && srcElement->username != NULL &&
	    sceClibStrnlen(srcElement->username, 256) != 0) {
		userLen = sceClibStrnlen(srcElement->username, 256);
		hasUserinfo = 1;
		size += userLen + 1;
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_PASSWORD) && srcElement->password != NULL &&
	    sceClibStrnlen(srcElement->password, 256) != 0) {
		passLen = sceClibStrnlen(srcElement->password, 256);
		size += passLen + 1;
		if (hasUserinfo != 1) {
			hasUserinfo = 1;
			size += userLen + 1;
		}
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_HOSTNAME) && srcElement->hostname != NULL) {
		hostLen = sceClibStrnlen(srcElement->hostname, 255);
		size += hostLen;
	}
	if (option & SCE_HTTP_URI_BUILD_WITH_PORT) {
		if (srcElement->scheme != NULL) {
			uriGetDefaultPort(srcElement->scheme, &defaultPort);
			if ((defaultPort != 0 || sceClibStrncmp(srcElement->scheme, "mailto", 32) == 0) &&
			    defaultPort == srcElement->port) {
				defaultPortOmitted = 1;
				goto port_done;
			}
		}
		if (srcElement->port != 0) {
			sceClibSnprintf(portStr, 7, ":%d", srcElement->port);
			portLen = sceClibStrnlen(portStr, 7);
			size += portLen;
		}
	}
port_done:
	if ((option & SCE_HTTP_URI_BUILD_WITH_PATH) && srcElement->path != NULL) {
		pathLen = sceClibStrnlen(srcElement->path, 0x3fff);
		size += pathLen;
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_QUERY) && srcElement->query != NULL) {
		queryLen = sceClibStrnlen(srcElement->query, 0x3fff);
		size += queryLen;
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_FRAGMENT) && srcElement->fragment != NULL) {
		fragmentLen = sceClibStrnlen(srcElement->fragment, 0x3fff);
		size += fragmentLen;
	}
	size++;
	if (require != NULL)
		*require = size;
	ret = 0;
	if (!write)
		return ret;
	if (prepare < size)
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;

	if ((option & SCE_HTTP_URI_BUILD_WITH_SCHEME) && srcElement->scheme != NULL &&
	    sceClibStrnlen(srcElement->scheme, 32) != 0) {
		sceClibMemcpy_safe(out, srcElement->scheme, schemeLen);
		sceClibMemcpy_safe(&out[schemeLen], ":", 1);
		n = schemeLen + 1;
	}
	if (srcElement->opaque != 1) {
		sceClibMemcpy_safe(&out[n], "//", 2);
		n += 2;
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_USERNAME) && srcElement->username != NULL &&
	    sceClibStrnlen(srcElement->username, 256) != 0) {
		sceClibMemcpy_safe(&out[n], srcElement->username, userLen);
		n += userLen;
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_PASSWORD) && srcElement->password != NULL &&
	    sceClibStrnlen(srcElement->password, 256) != 0) {
		sceClibMemcpy_safe(&out[n], ":", 1);
		n++;
		sceClibMemcpy_safe(&out[n], srcElement->password, passLen);
		n += passLen;
	}
	if (hasUserinfo == 1)
		sceClibMemcpy_safe(&out[n++], "@", 1);
	if ((option & SCE_HTTP_URI_BUILD_WITH_HOSTNAME) && srcElement->hostname != NULL &&
	    sceClibStrnlen(srcElement->hostname, 255) != 0) {
		sceClibMemcpy_safe(&out[n], srcElement->hostname, hostLen);
		n += hostLen;
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_PORT) && defaultPortOmitted != 1) {
		sceClibMemcpy_safe(&out[n], portStr, portLen);
		n += portLen;
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_PATH) && srcElement->path != NULL) {
		sceClibMemcpy_safe(&out[n], srcElement->path, pathLen);
		n += pathLen;
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_QUERY) && srcElement->query != NULL) {
		sceClibMemcpy_safe(&out[n], srcElement->query, queryLen);
		n += queryLen;
	}
	if ((option & SCE_HTTP_URI_BUILD_WITH_FRAGMENT) && srcElement->fragment != NULL) {
		sceClibMemcpy_safe(&out[n], srcElement->fragment, fragmentLen);
		n += fragmentLen;
	}
	out[n] = '\0';
	return ret;
}

int sceHttpUriSweepPath(char *dst, const char *src, SceSize srcSize)
{
	if (srcSize == 0)
		return 0;
	if (dst == NULL || src == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return uriSweepPath(dst, src, srcSize);
}

/*
 * relativeUrl against url. A relative URL with an authority is taken as it is; otherwise
 * its path is appended to the directory of url's path (or replaces it when it starts with
 * '/'). Both are parsed into mergedUrl's tail, after the room the merged URI needs.
 */
int sceHttpUriMerge(char *mergedUrl, const char *url, const char *relativeUrl, SceSize *require,
		    SceSize prepare, SceUInt32 option)
{
	SceHttpUriElement element;
	SceSize urlRequire;
	SceSize relativeRequire;
	SceSize size;
	SceSize offset;
	SceSize pathOffset;
	char *path;
	char *slash;
	int ret;

	if (option != 0 || url == NULL || relativeUrl == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = sceHttpUriParse(NULL, url, NULL, &urlRequire, 0);
	if (ret < 0)
		return ret;
	ret = sceHttpUriParse(NULL, relativeUrl, NULL, &relativeRequire, 0);
	if (ret < 0)
		return ret;
	size = urlRequire + 2 * (sceClibStrnlen(url, 0x3fff) + sceClibStrnlen(relativeUrl, 0x3fff)) + 2;
	if (require != NULL)
		*require = size;
	if (mergedUrl == NULL)
		return 0;
	if (size > prepare)
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;

	offset = sceClibStrnlen(url, 0x3fff) + sceClibStrnlen(relativeUrl, 0x3fff) + 1;
	ret = sceHttpUriParse(&element, relativeUrl, &mergedUrl[offset], &relativeRequire,
			      prepare - offset);
	if (ret < 0)
		return ret;
	if (element.opaque == 0) {
		sceClibStrncpy(mergedUrl, relativeUrl, size);
		if (require != NULL)
			*require = sceClibStrnlen(relativeUrl, 0x3fff) + 1;
		return 0;
	}
	ret = sceHttpUriParse(&element, url, &mergedUrl[offset], &urlRequire, prepare - offset);
	if (ret < 0)
		return ret;
	pathOffset = urlRequire + offset;
	path = &mergedUrl[pathOffset];
	sceClibStrncpy(path, element.path, prepare - pathOffset);
	slash = sceClibStrrchr(path, '/');
	if (slash == NULL)
		sceClibStrncat(path, "/", 0x3fff);
	else
		slash[1] = '\0';
	if (*relativeUrl == '/')
		mergedUrl[pathOffset] = '\0';
	sceClibStrncat(path, relativeUrl, 0x3fff - sceClibStrnlen(path, 0x3fff));
	sceClibStrnlen(path, 0x3fff);
	element.path = path;
	ret = sceHttpUriBuild(mergedUrl, NULL,
			      prepare - sceClibStrnlen(relativeUrl, 0x3fff) - sceClibStrnlen(url, 0x3fff) - 1,
			      &element, SCE_HTTP_URI_BUILD_WITH_SCHEME | SCE_HTTP_URI_BUILD_WITH_HOSTNAME |
			      SCE_HTTP_URI_BUILD_WITH_PORT | SCE_HTTP_URI_BUILD_WITH_PATH |
			      SCE_HTTP_URI_BUILD_WITH_USERNAME | SCE_HTTP_URI_BUILD_WITH_PASSWORD);
	if (ret >= 0)
		ret = 0;
	return ret;
}

/* one element's string into buf (an empty one for NULL), counted in *require */
static int uriElementCopyString(char **dst, char *buf, SceSize size, const char *src,
				SceSize *require, SceBool write)
{
	SceSize len = 0;

	if (src != NULL && (len = sceClibStrnlen(src, 0xffffffff)) != 0) {
		if (require != NULL)
			*require += len + 1;
		if (write == 0)
			return 0;
		if (len + 1 > size)
			return SCE_HTTP_ERROR_OUT_OF_MEMORY;
		sceClibMemcpy_safe(buf, src, len);
		buf[len] = '\0';
	} else {
		if (require != NULL)
			(*require)++;
		if (write == 0)
			return 0;
		if (len + 1 > size)
			return SCE_HTTP_ERROR_OUT_OF_MEMORY;
		buf[0] = '\0';
	}
	*dst = buf;
	return 0;
}

/* the scheme, host, path, user name, password and port of src, with the strings in buf */
int _sceHttpUriElementCopy(SceHttpUriElement *dst, const SceHttpUriElement *src, char *buf,
			   SceSize size, SceSize *require)
{
	SceSize used = 0;
	SceBool write = 1;
	int ret;

	if (src == NULL)
		return SCE_HTTP_ERROR_INVALID_URL;
	if (buf == NULL || dst == NULL) {
		if (require == NULL)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		write = 0;
	}
	if (write) {
		sceClibMemset(dst, 0, sizeof(*dst));
		sceClibMemset(buf, 0, size);
	}
	ret = uriElementCopyString(write ? &dst->scheme : NULL, write ? &buf[used] : NULL,
				   write ? size - used : 0, src->scheme, &used, write);
	if (ret < 0)
		goto fail;
	ret = uriElementCopyString(write ? &dst->hostname : NULL, write ? &buf[used] : NULL,
				   write ? size - used : 0, src->hostname, &used, write);
	if (ret < 0)
		goto fail;
	ret = uriElementCopyString(write ? &dst->path : NULL, write ? &buf[used] : NULL,
				   write ? size - used : 0, src->path, &used, write);
	if (ret < 0)
		goto fail;
	ret = uriElementCopyString(write ? &dst->username : NULL, write ? &buf[used] : NULL,
				   write ? size - used : 0, src->username, &used, write);
	if (ret < 0)
		goto fail;
	ret = uriElementCopyString(write ? &dst->password : NULL, write ? &buf[used] : NULL,
				   write ? size - used : 0, src->password, &used, write);
	if (ret < 0)
		goto fail;
	if (write)
		dst->port = src->port;
	if (require != NULL)
		*require = used;
	return 0;

fail:
	if (write) {
		sceClibMemset(dst, 0, sizeof(*dst));
		sceClibMemset(buf, 0, size);
	}
	return ret;
}
