/*
 * SceLibHttp's SSL checks: the server certificate's names (subjectAltName dNSNames, else
 * the subject's commonName, with wildcards), the chain check the SSL layer calls back, and
 * the CA list (the CAs sceHttpsLoadCert loaded, then the system's).
 */

#include <fw.h>
#include <sceerror.h>
#include <libssl.h>
#include <libssl/sslinternal.h>
#include <libssl/object.h>

#include "http_internal.h"

/* DER */
#define DER_CLASS_MASK			0xc0
#define DER_CLASS_CONTEXT		0x80
#define DER_CONSTRUCTED			0x20
#define DER_TAG_MASK			0x1f
#define DER_LENGTH_LONG			0x80
#define DER_LENGTH_MASK			0x7f
#define GENERAL_NAME_DNS		2		/* GeneralName's dNSName [2] */

#define HTTPS_SSL_ITEM_SUBJECT_ALT_NAME	0x4003		/* SceSslInternal_64BDF1BB's */
#define HTTPS_SSL_ITEM_GENERAL_NAMES	0x8002		/* SceSslInternal_93EAE08A's */
#define HTTPS_SSL_NAME_COUNT		10		/* SceSslInternal_91C2531C's */
#define HTTPS_SSL_NAME_ENTRY		11
#define HTTPS_SSL_AVA_TYPE		1		/* SceSslInternal_B5A099D2's */
#define HTTPS_SSL_AVA_VALUE		2
#define HTTPS_SSL_OID_NAME_SIZE		24

#define HTTPS_CA_SLACK			15000		/* added to a CA's size for its PEM text */
#define HTTPS_FLAG_SYSTEM_CA_ONLY	0x100		/* only the system's first store */

/* a DER item the SSL layer hands out */
typedef struct HttpsSslData {
	SceSize len;
	const SceUChar8 *data;
} HttpsSslData;

/* an attribute's type or value */
typedef struct HttpsSslAvaPart {
	int type;
	SceSize len;
	const char *data;
} HttpsSslAvaPart;

/* one attribute of a name, by index */
typedef struct HttpsSslNameEntry {
	int index;
	void *ava;
} HttpsSslNameEntry;

/* where _sceHttpsSslGetCaList is: the loaded CAs (phase 0), then the system's (phase 2) */
typedef struct HttpsSslCaIterator {
	int store;		/* the system store */
	int index;		/* the loaded CA's index, or the system CA's bit */
	int end;		/* 1 when the current store is done */
	int phase;
	void *certLib;
} HttpsSslCaIterator;

static void *_sceHttpsSslContextMalloc(HttpsSslContext *context, SceSize size);
static void _sceHttpsSslContextFree(HttpsSslContext *context, void *ptr);
static int _sceHttpsSslCertError(int library, int error, int other);
static SceUInt32 _sceHttpsSslGetSdkVersion(void);

/*
 * The dNSNames of a GeneralNames sequence: their count to *count when name is NULL, else
 * the *count'th one.
 */
__attribute__((noinline))
static int _sceHttpsSslGetDnsName(const SceUChar8 *der, SceSize size, int *count,
				  const SceUChar8 **name, SceSize *nameLen)
{
	const SceUChar8 *p;
	SceUChar8 header[2];
	SceUInt32 len, lenBytes, longLen, i;
	SceSize pos, end;
	int index, ret = 0;

	sceClibMemcpy_safe(header, der, sizeof(header));
	pos = 2;
	index = 0;
	p = der + 2;
	if ((header[0] & DER_CLASS_MASK) != 0 || (header[0] & DER_CONSTRUCTED) == 0) {
		ret = HTTPS_SSL_ERROR_CERT;
		goto out;
	}
	longLen = header[1] & DER_LENGTH_LONG;
	lenBytes = header[1] & DER_LENGTH_MASK;
	if ((header[1] & DER_LENGTH_LONG) != 0) {
		len = 0;
		for (i = 0; i < lenBytes; i = (SceUChar8)(i + 1))
			len = *p++ | len << 8;
		pos = lenBytes + 2;
	} else {
		len = lenBytes;
	}
	end = size - (longLen != 0 ? lenBytes : 0);
	if (end - 2 != len) {
		ret = HTTPS_SSL_ERROR_CERT;
		goto out;
	}
	do {
		sceClibMemcpy_safe(header, p, sizeof(header));
		pos += 2;
		p += 2;
		if ((header[0] & DER_CLASS_MASK) != DER_CLASS_CONTEXT) {
			ret = HTTPS_SSL_ERROR_CERT;
			goto out;
		}
		lenBytes = header[1] & DER_LENGTH_MASK;
		if ((header[1] & DER_LENGTH_LONG) != 0) {
			len = 0;
			for (i = 0; i < lenBytes; i = (SceUChar8)(i + 1))
				len = *p++ | len << 8;
			pos += lenBytes;
		} else {
			len = lenBytes;
		}
		if ((header[0] & DER_TAG_MASK) == GENERAL_NAME_DNS) {
			if (name != NULL && *count == index) {
				*name = p;
				*nameLen = len;
				goto out;
			}
			index++;
		}
		p += len;
		pos += len;
	} while (end > pos);
	if (name == NULL && count != NULL)
		*count = index;
out:
	return ret;
}

/* the first c in s's n bytes, before any NUL */
static inline __attribute__((always_inline)) const char *_sceHttpsStrnchr(const char *s, SceSize n,
									  char c)
{
	SceSize i;

	if (s == NULL)
		return NULL;
	for (i = 0; i < n; i++) {
		if (s[i] == c)
			return &s[i];
		if (s[i] == '\0')
			return NULL;
	}
	return NULL;
}

/*
 * Whether host matches the certificate's name (len bytes, not terminated): 0 when it does.
 * A '*' matches within one label, and a trailing dot on either is ignored.
 */
__attribute__((noinline))
static int _sceHttpsSslMatchName(const char *pattern, SceSize len, const char *host)
{
	const char *part, *star, *next, *dot;
	SceSize hostLen, pos, partLen, n, literal, skip;
	int ret;

	hostLen = sceClibStrnlen(host, 0xffffffff);
	if (pattern[len - 1] == '.')
		len--;
	if (host[hostLen - 1] == '.')
		hostLen--;
	pos = 0;
	for (;;) {
		part = pattern + pos;
		partLen = len - pos;
		star = _sceHttpsStrnchr(part, partLen, '*');
		if (star == NULL)
			break;
		n = star - part;
		if (star != part) {
			ret = sceClibStrncasecmp(part, host, n);
			if (ret != 0)
				return ret;
			host += n;
			hostLen -= n;
		}
		pos = pos + n + 1;
		part = pattern + pos;
		partLen = len - pos;
		next = _sceHttpsStrnchr(part, partLen, '*');
		if (next != NULL) {
			literal = next - star - 1;
			if (literal != 0) {
				if (hostLen >= literal) {
					while (sceClibStrncasecmp(part, host, literal) != 0) {
						hostLen--;
						host++;
						if (hostLen < literal)
							goto check;
					}
					host += literal;
					pos += literal;
					hostLen -= literal;
				}
check:
				if (hostLen < literal)
					return -1;
			}
		} else {
			if (partLen > hostLen)
				return -1;
			skip = hostLen - partLen;
			dot = _sceHttpsStrnchr(host, hostLen, '.');
			if (dot != NULL && skip > (SceSize)(dot - host))
				return -1;
			host += skip;
			hostLen -= skip;
		}
		if (len < pos)
			return -1;
	}
	if (partLen == hostLen)
		return sceClibStrncasecmp(part, host, partLen);
	return -1;
}

/*
 * Whether certs[0] names host (with SCE_HTTPS_FLAG_CN_CHECK; without it, always): its
 * subjectAltName dNSNames, or its subject's commonName when it has no dNSName.
 */
__attribute__((noinline))
static SceBool _sceHttpsSslCheckName(void *sslLib, void **certs, const char *host,
				     SceUInt32 sslFlags, int *err)
{
	HttpsSslData names;
	HttpsSslNameEntry entry;
	HttpsSslAvaPart type, value;
	SceUChar8 oid[16];
	char typeName[HTTPS_SSL_OID_NAME_SIZE];
	const SceUChar8 *name;
	SceSize nameLen;
	void *ext = NULL, *subject = NULL;
	int count, index, entries, i, found, ret, r;
	SceUInt32 cnCheck = sslFlags & SCE_HTTPS_FLAG_CN_CHECK;
	SceBool match;

	*err = 0;
	if ((sslFlags & SCE_HTTPS_FLAG_CN_CHECK) == 0) {
		match = SCE_TRUE;
		goto out;
	}
	r = SceSslInternal_BF99D664(sslLib, -1, &ext);
	if (r != 0) {
		*err = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_SSL, r);
		match = SCE_FALSE;
		goto out;
	}
	ret = SceSslInternal_64BDF1BB(certs[0], HTTPS_SSL_ITEM_SUBJECT_ALT_NAME, ext);
	if (ret != 0) {
		if (ret != SSL_INTERNAL_ERROR_NOT_FOUND) {
			*err = _sceHttpsSslCertError(SSL_INTERNAL_LIBRARY_SSL, ret,
						     HTTPS_SSL_ERROR_CERT);
			match = SCE_FALSE;
			goto out;
		}
		goto common_name;
	}
	r = SceSslInternal_93EAE08A(ext, HTTPS_SSL_ITEM_GENERAL_NAMES, &names);
	if (r != 0) {
		*err = _sceHttpsSslCertError(SSL_INTERNAL_LIBRARY_SSL, r, HTTPS_SSL_ERROR_CERT);
		match = SCE_FALSE;
		goto out;
	}
	r = _sceHttpsSslGetDnsName(names.data, names.len, &count, NULL, NULL);
	if (r < 0) {
		match = SCE_FALSE;
		*err = r;
		goto out;
	}
	index = 0;
	for (; index < count; index++) {
		r = _sceHttpsSslGetDnsName(names.data, names.len, &index, &name, &nameLen);
		if (r < 0) {
			match = SCE_FALSE;
			*err = r;
			goto out;
		}
		if (_sceHttpsSslMatchName((const char *)name, nameLen, host) == 0) {
			match = SCE_TRUE;
			goto out;
		}
	}
common_name:
	match = SCE_FALSE;
	if (ret != SSL_INTERNAL_ERROR_NOT_FOUND && (ret | count) != 0)
		goto out;
	r = SceSslInternal_08179EBD(certs[0], 1, &subject);
	if (r != 0)
		*err = _sceHttpsSslCertError(SSL_INTERNAL_LIBRARY_SSL, r, HTTPS_SSL_ERROR_CERT);
	match = SCE_FALSE;
	if (subject == NULL || cnCheck == 0)
		goto out;
	r = SceSslInternal_91C2531C(subject, HTTPS_SSL_NAME_COUNT, &entries);
	if (r != 0) {
		r = _sceHttpsSslCertError(SSL_INTERNAL_LIBRARY_SSL, r, HTTPS_SSL_ERROR_CERT);
		goto done;
	}
	found = 0;
	for (i = 0; i < entries; i++) {
		entry.index = i;
		r = SceSslInternal_91C2531C(subject, HTTPS_SSL_NAME_ENTRY, &entry);
		if (r != 0) {
			r = _sceHttpsSslCertError(SSL_INTERNAL_LIBRARY_SSL, r, HTTPS_SSL_ERROR_CERT);
			goto done;
		}
		r = SceSslInternal_B5A099D2(entry.ava, HTTPS_SSL_AVA_TYPE, &type);
		if (r != 0) {
			SceSslInternal_17BF2367(entry.ava);
			r = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_SSL, r);
			goto done;
		}
		r = SceSslInternal_4D80D3F7(oid, 1, type.data, type.len);
		if (r != 0)
			goto ava_error;
		if (SceSslInternal_3DE4F3DB(oid, 0, sizeof(typeName), typeName) != 0)
			sceClibStrncpy(typeName, "<UNKNOWN>", sizeof(typeName));
		r = SceSslInternal_B5A099D2(entry.ava, HTTPS_SSL_AVA_VALUE, &value);
		if (r != 0) {
ava_error:
			SceSslInternal_17BF2367(entry.ava);
			r = _sceHttpsSslCertError(SSL_INTERNAL_LIBRARY_SSL, r, HTTPS_SSL_ERROR_CERT);
			goto done;
		}
		if (sceClibStrncmp(typeName, "commonName", sizeof("commonName")) == 0 &&
		    _sceHttpsSslMatchName(value.data, value.len, host) == 0)
			found = 1;
		r = SceSslInternal_17BF2367(entry.ava);
		if (r != 0) {
			r = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_SSL, r);
			goto done;
		}
	}
	r = -1;
	if (found == 1)
		r = 0;
done:
	match = r == 0;
out:
	if (ext != NULL) {
		SceSslInternal_F030A0DE(ext);
		ext = NULL;
	}
	if (subject != NULL)
		SceSslInternal_CE37B69E(subject);
	return match;
}

/* the DER bytes of a PEM CA (the SSL layer's errors) */
__attribute__((noinline))
static int _sceHttpsSslPemDecode(const char *pem, void *buf, SceSize *outSize, SceSize size)
{
	const char *begin, *line, *end, *nl, *cr;
	SceSize len = 0, lineLen, next;
	_Bool write = buf != NULL;
	int ret;

	begin = sceClibStrchr(pem, '-');
	if (begin == NULL)
		return HTTPS_SSL_ERROR_INVALID_VALUE;
	line = sceClibStrchr(begin, '\n');
	if (line == NULL)
		return HTTPS_SSL_ERROR_INVALID_VALUE;
	end = sceClibStrchr(line, '-');
	if (end == NULL)
		return HTTPS_SSL_ERROR_INVALID_VALUE;
	for (line++; line < end; line = nl + 1) {
		nl = sceClibStrchr(line, '\n');
		if (nl == NULL)
			return HTTPS_SSL_ERROR_INVALID_VALUE;
		cr = sceClibStrchr(line, '\r');
		lineLen = nl - line;
		if (cr != NULL && cr < nl)
			lineLen--;
		next = lineLen / 4 * 3 + len;
		if (write) {
			if (size < next)
				return HTTPS_SSL_ERROR_OUT_OF_SIZE;
			ret = sceBase64Decoder((char *)buf + len, line, lineLen);
			if (ret < 0)
				return HTTPS_SSL_ERROR_INVALID_VALUE;
			next = ret + len;
		}
		len = next;
	}
	if (outSize != NULL) {
		if (write)
			*outSize = len;
		else
			*outSize = len + 1;
	}
	return 0;
}

/*
 * The next CA certificate to *cert (NULL at the end): the loaded ones, then the system's
 * (from sslStore when given, since 3.65; else store by store).
 */
__attribute__((noinline))
static int _sceHttpsSslNextCa(HttpsSslCaIterator *it, void **cert, HttpsSslContext *context,
			      void *sslStore)
{
	char *buf, *der = NULL;
	const char **pem;
	void *end;
	SceSize size, derSize, pemSize, len;
	char unused[8];
	SceBool systemOnly;
	int found, index, last, ret, r;

	if (it->phase <= 1) {
		if (it->phase != 0)
			goto system;
		derSize = 0;
		if (_sceHttpsCertIsInitialized() == 1) {
			ret = _sceHttpsCertGetCa(0, it->index, NULL, 0, NULL, &size);
			if (ret == HTTPS_SSL_ERROR_INVALID_ID) {
				buf = NULL;
				it->end = 1;
			} else {
				if (ret < 0)
					goto loaded;
				buf = _sceHttpsSslContextMalloc(context, size + HTTPS_CA_SLACK);
				if (buf == NULL) {
					ret = HTTPS_SSL_ERROR_OUT_OF_MEMORY;
					goto free_loaded;
				}
				ret = _sceHttpsCertGetCa(0, it->index, buf, size, unused, &derSize);
				if (ret < 0)
					goto free_loaded;
				r = SceSslInternal_29AAC768(it->certLib, 0, 1, derSize, buf, &end, cert);
				if (r != 0) {
					ret = _sceHttpsSslCertError(SSL_INTERNAL_LIBRARY_SSL, r,
								    HTTPS_SSL_ERROR_CERT);
					goto free_loaded;
				}
			}
		} else {
			buf = NULL;
			it->end = 1;
		}
		ret = 0;
free_loaded:
		if (buf != NULL)
			_sceHttpsSslContextFree(context, buf);
loaded:
		if (ret < 0)
			return ret;
		if (it->end != 1) {
			it->index++;
			return ret;
		}
system:
		it->index = 0;
		it->end = 0;
		it->phase = 2;
	} else if (it->phase > 2) {
		return HTTPS_SSL_ERROR_INTERNAL;
	}

	found = 0;
	systemOnly = SCE_FALSE;
	pemSize = 0;
	len = 0;
	ret = 0;
#if FW_SINCE(0x03650000)
	if (sslStore != NULL) {
		systemOnly = (context->sslFlags & HTTPS_FLAG_SYSTEM_CA_ONLY) != 0;
		r = SceSslInternal_3746501F(sslStore, NULL, NULL, 0, &pemSize, systemOnly);
		if (r == 0) {
			*cert = NULL;
			return 0;
		}
		if (r < 0)
			return r;
		it->end = 0;
	} else
#endif
	{
		do {
			if (it->store == 0 || it->end == 1) {
				if ((context->sslFlags & HTTPS_FLAG_SYSTEM_CA_ONLY) != 0)
					it->store = 1;
				else
					it->store = it->store + 1;
				index = 1;
			} else {
				index = it->index << 1;
			}
			it->index = index;
			r = sceSslInternalGetCertificateAuthority(it->store, index, NULL, NULL, 0,
								  &pemSize);
			if (r == SCE_ERROR_ERRNO_ENOTSUP) {
				if (it->end == 1 ||
				    (context->sslFlags & HTTPS_FLAG_SYSTEM_CA_ONLY) != 0) {
					*cert = NULL;
					return 0;
				}
				last = 1;
			} else {
				if (r < 0)
					return r;
				found = 1;
				last = 0;
			}
			it->end = last;
		} while (found == 0);
	}

	buf = _sceHttpsSslContextMalloc(context, pemSize + HTTPS_CA_SLACK);
	if (buf == NULL) {
		ret = HTTPS_SSL_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
#if FW_SINCE(0x03650000)
	if (sslStore != NULL) {
		ret = SceSslInternal_3746501F(sslStore, &pem, buf, pemSize, &len, systemOnly);
		if (ret < 0)
			goto fail;
	} else
#endif
	{
		ret = sceSslInternalGetCertificateAuthority(it->store, it->index, &pem, buf, pemSize,
							    &len);
		if (ret < 0)
			goto fail;
	}
	if (ret != 1) {
		ret = HTTPS_SSL_ERROR_INTERNAL;
		goto fail;
	}
	if (_sceHttpsSslPemDecode(*pem, NULL, &pemSize, 0) < 0) {
		ret = HTTPS_SSL_ERROR_INTERNAL;
		goto fail;
	}
	der = _sceHttpsSslContextMalloc(context, pemSize);
	if (der == NULL) {
		ret = HTTPS_SSL_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	if (_sceHttpsSslPemDecode(*pem, der, &len, pemSize) < 0) {
		ret = HTTPS_SSL_ERROR_INTERNAL;
		goto fail;
	}
	_sceHttpsSslContextFree(context, buf);
	r = SceSslInternal_29AAC768(it->certLib, 0, 1, len, der, &end, cert);
	if (r != 0)
		ret = _sceHttpsSslCertError(SSL_INTERNAL_LIBRARY_SSL, r, HTTPS_SSL_ERROR_CERT);
	else
		ret = 0;
	goto free_der;
fail:
	if (buf != NULL)
		_sceHttpsSslContextFree(context, buf);
free_der:
	if (der != NULL)
		_sceHttpsSslContextFree(context, der);
	return ret;
}

int _sceHttpsSslGetCaList(SceSslCert ***caCerts, SceInt32 *caNum, HttpsSslContext *context)
{
	HttpsSslCaIterator it;
	SceSslCert **list, **grown;
	void *lib = NULL, *cert = NULL, *certLib = NULL, *sslCtx, *ssl;
	SceInt32 num;
	int ret;

	ret = sceSslInternalIsInitalized();
	if (ret < 0)
		return ret;
	ret = SceSslInternal_2185BB05(&lib);
	if (ret < 0)
		return ret;
	sslCtx = SceSslInternal_E746465E(lib, SceSslInternal_AC1DBE40());
	if (sslCtx == NULL) {
		ret = HTTPS_SSL_ERROR_OUT_OF_MEMORY;
		sslCtx = NULL;
		goto out;
	}
	ssl = SceSslInternal_D28834E6(sslCtx);
	if (ssl == NULL) {
		ret = HTTPS_SSL_ERROR_OUT_OF_MEMORY;
		ssl = NULL;
		goto close;
	}
	if (SceSslInternal_24C6257F(ssl, SSL_INTERNAL_VALUE_CERT_LIBRARY, &certLib) != 0) {
		ret = HTTPS_SSL_ERROR_SSL;
		goto close;
	}
	sceClibMemset(&it, 0, sizeof(it));
	it.certLib = certLib;
	list = NULL;
	num = 0;
	for (;;) {
		ret = _sceHttpsSslNextCa(&it, &cert, context, NULL);
		if (ret < 0)
			break;
		if (cert == NULL) {
			*caNum = num;
			*caCerts = list;
			goto close;
		}
		grown = context->realloc(context->allocatorArg, list, (num + 1) * sizeof(*list));
		if (grown == NULL) {
			ret = HTTPS_SSL_ERROR_OUT_OF_MEMORY;
			break;
		}
		list = grown;
		list[num] = cert;
		num++;
	}
	if (list != NULL)
		context->free(context->allocatorArg, list);
close:
	if (ssl != NULL) {
		SceSslInternal_EC9FAF42(ssl);
		SceSslInternal_19B2BB58(ssl);
	}
out:
	if (sslCtx != NULL)
		SceSslInternal_EF9EBAC7(sslCtx);
	return ret;
}

int _sceHttpsSslFreeCaList(SceSslCert **caCerts, SceInt32 caNum, HttpsSslContext *context)
{
	int ret = 0;
	int i;

	if (caNum <= 0)
		return HTTPS_SSL_ERROR_INVALID_VALUE;
	for (i = 0; i < caNum; i++)
		SceSslInternal_144FA354(caCerts[i]);
	context->free(context->allocatorArg, caCerts);
	return ret;
}

/* the context's allocator, when it has one */
__attribute__((noinline))
static void *_sceHttpsSslContextMalloc(HttpsSslContext *context, SceSize size)
{
	void *ptr = NULL;

	if (context->malloc != NULL)
		ptr = context->malloc(context->allocatorArg, size);
	return ptr;
}

__attribute__((noinline))
static void _sceHttpsSslContextFree(HttpsSslContext *context, void *ptr)
{
	if (context->free != NULL)
		context->free(context->allocatorArg, ptr);
}

/* the SSL layer's error, kept when it is an invalid value or out of memory, else other */
__attribute__((noinline))
static int _sceHttpsSslCertError(int library, int error, int other)
{
	int ret;

	ret = SceSslInternal_461C5990(library, error);
	if (ret == SCE_SSL_ERROR_INVALID_VALUE || ret == SCE_SSL_ERROR_OUT_OF_MEMORY)
		return ret;
	return other;
}

__attribute__((noinline))
static SceUInt32 _sceHttpsSslGetSdkVersion(void)
{
	return SceLibKernel_9F793F84();
}
