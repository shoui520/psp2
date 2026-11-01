/*
 * SceLibHttp over SSL: a connection's SSL context and handshake (through a proxy's
 * CONNECT first), its reads and writes, the certificates sceHttpsLoadCert loads, and the
 * SSL settings of templates, connections and requests.
 */

#include <net/errno.h>
#include <libssl/sslinternal.h>

#include "http_internal.h"

#define HTTPS_IO_MAX			1500		/* the most one SSL read or write moves */
#define HTTPS_ERROR_RETRY		-2143072155	/* 0x80435065 */
#define HTTPS_ERROR_NO_BIO		10008		/* the key's BIO could not be made */
#define HTTPS_SSL_OPTIONS		0x20001ffb	/* SceSslInternal_FBEB1FD7's */
#define HTTPS_SSL_OPTION_NO_SSLV3	0x02000000	/* with TLS only (SDK 3.50) */
#define HTTPS_SSL_MODE			0x40000		/* SceSslInternal_0DB6C206's */
#define HTTPS_SSL_CTRL_SESSION_CACHE	44		/* SceSslInternal_CC7F4211's, off */
#define HTTPS_CERT_FORMAT_DER		1
#define HTTPS_KEY_TYPE			6
#define HTTPS_KEY_FORMAT		0x1100
#define HTTPS_SSL_VALUE_KEY_LIBRARY	6		/* SceSslInternal_24C6257F's */
#define HTTPS_SSL_VALUE_CERT_LIBRARY	5
#define HTTPS_CERT_ISSUER		1		/* SceSslInternal_81FA5DBE's */

/* the alerts the handshake reports (SSL 3.0's) */
#define HTTPS_ALERT_CLOSE_NOTIFY		0
#define HTTPS_ALERT_UNEXPECTED_MESSAGE		10
#define HTTPS_ALERT_BAD_RECORD_MAC		20
#define HTTPS_ALERT_DECOMPRESSION_FAILURE	30
#define HTTPS_ALERT_HANDSHAKE_FAILURE		40
#define HTTPS_ALERT_NO_CERTIFICATE		41
#define HTTPS_ALERT_BAD_CERTIFICATE		42
#define HTTPS_ALERT_UNSUPPORTED_CERTIFICATE	43
#define HTTPS_ALERT_CERTIFICATE_REVOKED		44
#define HTTPS_ALERT_CERTIFICATE_EXPIRED		45
#define HTTPS_ALERT_CERTIFICATE_UNKNOWN		46
#define HTTPS_ALERT_ILLEGAL_PARAMETER		47

/* where the info callback is called from */
#define HTTPS_INFO_READ			0x04
#define HTTPS_INFO_WRITE		0x08
#define HTTPS_INFO_ALERT		0x4000

/* the debug messages, by number: an alert sent, an alert received */
#define HTTPS_DEBUG_ALERT_SENT		0x40
#define HTTPS_DEBUG_ALERT_RECEIVED	0x50

/* the default SSL flags of templates that set none (sceHttpsEnableOption's) */
SceUInt32 g_httpsSslFlags = HTTPS_FLAGS_SERVER_CHECKS | 0x40;

/* the message callback: nothing */
static void _sceHttpsSslMessageCallback(void)
{
}

/* a debug message by number, its messages compiled out (one break each, as Sony's table) */
__attribute__((noinline))
static int _sceHttpsDebugMessage(int id, int arg)
{
	switch (id) {
	case 16: break; case 17: break; case 18: break; case 19: break;
	case 20: break; case 21: break; case 22: break; case 23: break;
	case 24: break; case 25: break; case 26: break; case 27: break;
	case 28: break; case 29: break; case 30: break; case 31: break;
	case 32: break; case 33: break; case 34: break; case 35: break;
	case 36: break; case 37: break; case 38: break; case 39: break;
	case 40: break; case 41: break; case 42: break; case 43: break;
	case 44: break; case 45: break; case 46: break; case 47: break;
	case 48: break; case 49: break; case 50: break; case 51: break;
	case 52: break; case 53: break; case 54: break; case 55: break;
	case 56: break; case 57: break; case 58: break; case 59: break;
	case 60: break; case 61: break; case 62: break; case 63: break;
	case 64: break; case 65: break; case 66: break; case 67: break;
	case 68: break; case 69: break; case 70: break; case 71: break;
	case 72: break; case 73: break; case 74: break; case 75: break;
	case 76: break; case 77: break; case 78: break; case 79: break;
	case 80: break; case 81: break; case 82: break; case 83: break;
	case 84: break; case 85: break; case 86: break; case 87: break;
	case 88: break; case 89: break; case 90: break; case 91: break;
	case 92: break; case 93: break; case 94: break; case 95: break;
	case 96: break; case 97: break; case 98: break; case 99: break;
	case 100: break; case 101: break; case 102: break; case 103: break;
	case 104: break; case 105: break; case 106: break; case 107: break;
	case 108: break; case 109: break; case 110: break; case 111: break;
	case 112: break; case 113: break; case 114: break; case 115: break;
	case 116: break; case 117: break; case 118: break; case 119: break;
	case 120: break; case 121: break; case 122: break; case 123: break;
	case 124: break; case 125: break; case 126: break; case 127: break;
	case 128: break; case 129: break; case 130: break; case 131: break;
	case 132: break; case 133: break; case 134: break; case 135: break;
	case 136: break; case 137: break; case 138: break; case 139: break;
	case 140: break; case 141: break; case 142: break; case 143: break;
	case 144: break; case 145: break; case 146: break; case 147: break;
	case 148: break; case 149: break; case 150: break; case 151: break;
	case 152: break; case 153: break; case 154: break; case 155: break;
	case 156: break; case 157: break; case 158: break; case 159: break;
	case 160: break; case 161: break; case 162: break; case 163: break;
	case 164: break; case 165: break; case 166: break; case 167: break;
	case 168: break; case 169: break; case 170: break; case 171: break;
	case 172: break; case 173: break; case 174: break; case 175: break;
	case 176: break; case 177: break; case 178: break; case 179: break;
	case 180: break; case 181: break; case 182: break; case 183: break;
	case 184: break; case 185: break; case 186: break; case 187: break;
	case 188: break; case 189: break; case 190: break;
	}
	return 0;
}

/* the handshake's progress: its alerts, as debug messages */
static void _sceHttpsSslInfoCallback(void *ssl, int where, int ret)
{
	if ((where & HTTPS_INFO_ALERT) == 0)
		return;
	if ((where & HTTPS_INFO_WRITE) != 0) {
		switch ((SceUChar8)ret) {
		case HTTPS_ALERT_CLOSE_NOTIFY:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 0, 0);
			break;
		case HTTPS_ALERT_UNEXPECTED_MESSAGE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 1, 0);
			break;
		case HTTPS_ALERT_BAD_RECORD_MAC:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 2, 0);
			break;
		case HTTPS_ALERT_DECOMPRESSION_FAILURE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 3, 0);
			break;
		case HTTPS_ALERT_HANDSHAKE_FAILURE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 4, 0);
			break;
		case HTTPS_ALERT_NO_CERTIFICATE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 5, 0);
			break;
		case HTTPS_ALERT_BAD_CERTIFICATE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 6, 0);
			break;
		case HTTPS_ALERT_UNSUPPORTED_CERTIFICATE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 7, 0);
			break;
		case HTTPS_ALERT_CERTIFICATE_REVOKED:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 8, 0);
			break;
		case HTTPS_ALERT_CERTIFICATE_EXPIRED:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 9, 0);
			break;
		case HTTPS_ALERT_CERTIFICATE_UNKNOWN:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 10, 0);
			break;
		case HTTPS_ALERT_ILLEGAL_PARAMETER:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_SENT + 11, 0);
			break;
		}
	}
	if ((where & HTTPS_INFO_READ) != 0) {
		switch ((SceUChar8)ret) {
		case HTTPS_ALERT_CLOSE_NOTIFY:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 0, 0);
			break;
		case HTTPS_ALERT_UNEXPECTED_MESSAGE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 1, 0);
			break;
		case HTTPS_ALERT_BAD_RECORD_MAC:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 2, 0);
			break;
		case HTTPS_ALERT_DECOMPRESSION_FAILURE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 3, 0);
			break;
		case HTTPS_ALERT_HANDSHAKE_FAILURE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 4, 0);
			break;
		case HTTPS_ALERT_NO_CERTIFICATE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 5, 0);
			break;
		case HTTPS_ALERT_BAD_CERTIFICATE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 6, 0);
			break;
		case HTTPS_ALERT_UNSUPPORTED_CERTIFICATE:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 7, 0);
			break;
		case HTTPS_ALERT_CERTIFICATE_REVOKED:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 8, 0);
			break;
		case HTTPS_ALERT_CERTIFICATE_EXPIRED:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 9, 0);
			break;
		case HTTPS_ALERT_CERTIFICATE_UNKNOWN:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 10, 0);
			break;
		case HTTPS_ALERT_ILLEGAL_PARAMETER:
			_sceHttpsDebugMessage(HTTPS_DEBUG_ALERT_RECEIVED + 11, 0);
			break;
		}
	}
}

static void *_sceHttpsSslMalloc(void *arg, SceSize size);
static void *_sceHttpsSslRealloc(void *arg, void *ptr, SceSize size);
static void _sceHttpsSslFree(void *arg, void *ptr);
static int _sceHttpsLock(void *arg);
static int _sceHttpsUnlock(void *arg);
static void *_sceHttpsDefaultMalloc(void *arg, SceSize size);
static void *_sceHttpsDefaultRealloc(void *arg, void *ptr, SceSize size);
static void _sceHttpsDefaultFree(void *arg, void *ptr);
static int _sceHttpsSslClientCertCallback(void *ssl, void **certOut, void **keyOut);

/*
 * Puts SSL on the socket of req's connection: the connection's context first (made once,
 * with libhttp's callback and the client certificate), then a new SSL connection on sock.
 */
int _sceHttpsConnect(HttpRequest *req, int sock)
{
	HttpConnection *conn;
	HttpsSslContext *context;
	SceUInt64 seed;
	SceUInt32 options, sslFlags;
	void *lib, *bio;
	SceBool locked;
	int ret;

	lib = NULL;
	ret = sceSslInternalIsInitalized();
	if (ret < 0)
		return ret;
	conn = _sceHttpRequestGetConnection(req);
	if (conn->sslCtx == NULL) {
		ret = _sceHttpGetDefaultOption(22, &locked, sizeof(locked), NULL, 0);
		if (ret < 0)
			goto fail;
		ret = SceSslInternal_2185BB05(&lib);
		if (ret < 0)
			goto fail;
		conn->sslCtx = SceSslInternal_E746465E(lib, SceSslInternal_A374CD7B());
		if (conn->sslCtx == NULL) {
			ret = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_HTTP, SCE_HTTP_ERROR_SSL);
			goto fail;
		}
		seed = sceKernelGetSystemTimeWide();
		ret = SceSslInternal_67FB1CD3(conn->sslCtx, &seed, sizeof(seed));
		if (ret != 0) {
			ret = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_SSL, ret);
			goto fail;
		}
		options = HTTPS_SSL_OPTIONS;
		if ((SceUInt32)_sceHttpGetSdkVersion() >= 0x3500000 &&
		    conn->tmpl.sslVersion == SCE_HTTPS_TLSV1)
			options = HTTPS_SSL_OPTIONS | HTTPS_SSL_OPTION_NO_SSLV3;
		SceSslInternal_FBEB1FD7(conn->sslCtx, options);
		if (locked)
			conn->sslContext = _sceHttpMalloc(conn->tmpl.allocator.malloc,
							  sizeof(HttpsSslContext));
		else
			conn->sslContext = _sceHttpMalloc(conn->tmpl.allocator.malloc,
							  HTTPS_SSL_CONTEXT_SIZE);
		if (conn->sslContext == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto fail;
		}
		conn->sslContext->hostname = conn->hostname;
		context = conn->sslContext;
		sslFlags = conn->tmpl.sslFlags;
		if ((sslFlags & HTTPS_FLAGS_SET) == 0)
			sslFlags = g_httpsSslFlags;
		context->sslFlags = sslFlags;
		context->callback = conn->tmpl.sslCallback;
		context->userArg = conn->tmpl.sslCallbackUserArg;
		context->malloc = _sceHttpsSslMalloc;
		context->realloc = _sceHttpsSslRealloc;
		context->free = _sceHttpsSslFree;
		context->allocatorArg = &conn->tmpl;
		context->sslError = &conn->sslError;
		context->sslErrorDetail = &conn->sslErrorDetail;
		if (locked) {
			context->lock = _sceHttpsLock;
			context->unlock = _sceHttpsUnlock;
			context->sslFlags |= HTTPS_SSL_CONTEXT_LOCKED;
		}
		ret = SceHttp_A9A39BA1(conn->sslCtx, context);
		if (ret < 0)
			goto fail;
		SceSslInternal_9E38815D(conn->sslCtx, _sceHttpsSslClientCertCallback);
		SceSslInternal_CC7F4211(conn->sslCtx, HTTPS_SSL_CTRL_SESSION_CACHE, 0, NULL);
	}
	if (conn->ssl != NULL) {
		SceSslInternal_19B2BB58(conn->ssl);
		conn->ssl = NULL;
	}
	conn->ssl = SceSslInternal_D28834E6(conn->sslCtx);
	if (conn->ssl == NULL) {
		ret = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_HTTP, SCE_HTTP_ERROR_SSL);
		goto fail;
	}
	SceSslInternal_0DB6C206(conn->ssl, HTTPS_SSL_MODE);
	bio = SceSslInternal_FFE2ED51(sock, 0);
	SceSslInternal_4A1557A7(conn->ssl, bio, bio);
	SceSslInternal_F2C9B744(conn->ssl, _sceHttpsSslInfoCallback);
	SceSslInternal_F71DB06F(conn->ssl, _sceHttpsSslMessageCallback, NULL);
	SceSslInternal_CEE46B33(conn->ssl);
	return 0;

fail:
	if (conn->ssl != NULL)
		SceSslInternal_EC9FAF42(conn->ssl);
	return ret;
}

/* runs the handshake on req's connection, once more for each call while it would block */
int _sceHttpsHandshake(HttpRequest *req)
{
	HttpConnection *conn;
	int ret, err;

	conn = _sceHttpRequestGetConnection(req);
	ret = SceSslInternal_E5EDD91B(conn->ssl);
	switch (SceSslInternal_2F0FC3CD(conn->ssl, ret)) {
	case SSL_INTERNAL_ERROR_NONE:
		return 0;
	case SSL_INTERNAL_ERROR_SSL:
		err = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_HTTP, SCE_HTTP_ERROR_SSL);
		if (err == SCE_HTTP_ERROR_SSL && conn->sslError == 0)
			conn->sslError = SCE_HTTPS_ERROR_HANDSHAKE;
		return err;
	case SSL_INTERNAL_ERROR_WANT_READ:
		if (ret != SCE_NET_ERROR_EAGAIN)
			goto io;
		err = SCE_HTTP_ERROR_EAGAIN;
		if (req->tmpl.nonblock == 0 && (SceUInt32)_sceHttpGetSdkVersion() >= 0x1800000)
			return SCE_HTTP_ERROR_TIMEOUT;
		return err;
	case SSL_INTERNAL_ERROR_WANT_WRITE:
		err = SCE_HTTPS_ERROR_INTERNAL;
		goto fail;
	case SSL_INTERNAL_ERROR_SYSCALL:
		if ((SceUInt32)_sceHttpGetSdkVersion() >= 0x3200000 && ret == 0) {
			conn->sslError = SCE_HTTPS_ERROR_HANDSHAKE;
			err = SCE_HTTP_ERROR_SSL;
			goto fail;
		}
		if (*sceNetErrnoLoc() != SCE_NET_EAGAIN)
			goto io;
		err = SCE_HTTP_ERROR_EAGAIN;
		if (req->tmpl.nonblock == 0 && (SceUInt32)_sceHttpGetSdkVersion() >= 0x1800000)
			return SCE_HTTP_ERROR_TIMEOUT;
		return err;
	case SSL_INTERNAL_ERROR_ZERO_RETURN:
		if ((SceUInt32)_sceHttpGetSdkVersion() >= 0x3200000) {
			conn->sslError = SCE_HTTPS_ERROR_HANDSHAKE;
			err = SCE_HTTP_ERROR_SSL;
			goto fail;
		}
		if (*sceNetErrnoLoc() != SCE_NET_EAGAIN)
			goto io;
		err = SCE_HTTP_ERROR_EAGAIN;
		if (req->tmpl.nonblock == 0 && (SceUInt32)_sceHttpGetSdkVersion() >= 0x1800000)
			return SCE_HTTP_ERROR_TIMEOUT;
		return err;
	default:
		err = SCE_HTTPS_ERROR_INTERNAL;
		goto fail;
	}

io:
	conn->sslError = SCE_HTTPS_ERROR_IO;
	err = SCE_HTTP_ERROR_SSL;
	conn->sslErrorDetail = *sceNetErrnoLoc();
fail:
	if (conn->ssl != NULL)
		SceSslInternal_EC9FAF42(conn->ssl);
	return err;
}

/* sends the proxy a CONNECT for req's server and reads its answer, which must be 200 */
int _sceHttpRequestProxyConnect(HttpRequest *req, int sock)
{
	HttpConnection *conn;
	char *msg = NULL;
	int method, ret;

	conn = _sceHttpRequestGetConnection(req);
	if (req->state > 5) {
		if (req->state > 6) {
			ret = SCE_HTTP_ERROR_UNKNOWN;
			goto out;
		}
	} else {
		if (req->state <= 4) {
			ret = SCE_HTTP_ERROR_UNKNOWN;
			goto out;
		}
		method = req->method;
		req->method = SCE_HTTP_METHOD_CONNECT;
		ret = _sceHttpRequestCreateHeader(&msg, req);
		req->method = method;
		if (ret < 0)
			goto out;
		ret = _sceHttpRequestSend(req, conn, msg, ret - 1, 0, req->tmpl.nonblock, 1);
		if (ret < 0)
			goto out;
		conn->state = 6;
		req->state = 6;
	}
	ret = _sceHttpRequestRecvResponseHeader(req, conn);
	if (ret <= 0) {
		if (ret != SCE_HTTP_ERROR_EAGAIN)
			ret = SCE_HTTP_ERROR_BAD_RESPONSE;
		goto out;
	}
	if (req->response.statusCode != 200) {
		conn->sslError = SCE_HTTPS_ERROR_PROXY;
		ret = SCE_HTTP_ERROR_SSL;
		goto out;
	}
	_sceHttpRequestResetResponseHeader(req);
out:
	if (msg != NULL)
		_sceHttpFree(req->tmpl.allocator.free, msg);
	return ret;
}

/* what an SSL read or write's result ret means for conn */
__attribute__((noinline))
static int _sceHttpsSslResult(HttpConnection *conn, int ret)
{
	switch (SceSslInternal_2F0FC3CD(conn->ssl, ret)) {
	case SSL_INTERNAL_ERROR_NONE:
	case SSL_INTERNAL_ERROR_ZERO_RETURN:
		return 0;
	case SSL_INTERNAL_ERROR_SSL:
		return SCE_HTTP_ERROR_SSL;
	case SSL_INTERNAL_ERROR_WANT_READ:
	case SSL_INTERNAL_ERROR_WANT_WRITE:
		break;
	case SSL_INTERNAL_ERROR_SYSCALL:
		if (ret == 0)
			return 0;
		break;
	default:
		return SCE_HTTPS_ERROR_INTERNAL;
	}
	conn->sslError = SCE_HTTPS_ERROR_IO;
	conn->sslErrorDetail = *sceNetErrnoLoc();
	return SCE_HTTP_ERROR_SSL;
}

int _sceHttpsRecv(HttpConnection *conn, void *buf, SceSize len)
{
	int ret, err;

	ret = sceSslInternalIsInitalized();
	if (ret < 0)
		return ret;
	for (;;) {
		ret = SceSslInternal_4F781BB6(conn->ssl, buf, len > HTTPS_IO_MAX ? HTTPS_IO_MAX : len);
		if (ret == SCE_NET_ERROR_EAGAIN)
			break;
		err = _sceHttpsSslResult(conn, ret);
		if (err != HTTPS_ERROR_RETRY) {
			if (err < 0)
				return err;
			return ret;
		}
	}
	return ret;
}

int _sceHttpsSend(HttpConnection *conn, const void *buf, SceSize len)
{
	int ret, err;

	ret = sceSslInternalIsInitalized();
	if (ret < 0)
		return ret;
	for (;;) {
		ret = sceSslInternalWrite(conn->ssl, buf, len > HTTPS_IO_MAX ? HTTPS_IO_MAX : len);
		if (ret == SCE_NET_ERROR_EAGAIN)
			break;
		err = _sceHttpsSslResult(conn, ret);
		if (err != HTTPS_ERROR_RETRY) {
			if (err < 0)
				return err;
			return ret;
		}
	}
	return ret;
}

int _sceHttpsClose(HttpConnection *conn)
{
	int ret;

	ret = sceSslInternalIsInitalized();
	if (ret < 0)
		return ret;
	if (conn->sslCtx != NULL) {
		SceHttp_A4CF892E(conn->sslCtx, NULL);
		if (conn->sslContext != NULL) {
			_sceHttpFree(conn->tmpl.allocator.free, conn->sslContext);
			conn->sslContext = NULL;
		}
	}
	if (conn->ssl != NULL) {
		SceSslInternal_EC9FAF42(conn->ssl);
		SceSslInternal_19B2BB58(conn->ssl);
		conn->ssl = NULL;
	}
	if (conn->sslCtx != NULL) {
		SceSslInternal_EF9EBAC7(conn->sslCtx);
		conn->sslCtx = NULL;
	}
	return 0;
}

/*
 * The DER bytes of a PEM certificate or key: the base64 lines between the BEGIN and END
 * lines, decoded into buf (size bytes) or, without buf, only measured (with a byte more).
 */
__attribute__((noinline))
static int _sceHttpsPemDecode(const char *pem, void *buf, SceSize *outSize, SceSize size)
{
	const char *begin, *line, *end, *nl, *cr;
	SceSize len = 0, lineLen, next;
	_Bool write = buf != NULL;
	int ret;

	begin = sceClibStrchr(pem, '-');
	if (begin == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	line = sceClibStrchr(begin, '\n');
	if (line == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	end = sceClibStrchr(line, '-');
	if (end == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	for (line++; line < end; line = nl + 1) {
		nl = sceClibStrchr(line, '\n');
		if (nl == NULL)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		cr = sceClibStrchr(line, '\r');
		lineLen = nl - line;
		if (cr != NULL && cr < nl)
			lineLen--;
		next = lineLen / 4 * 3 + len;
		if (write) {
			if (size < next)
				return SCE_HTTP_ERROR_OUT_OF_SIZE;
			ret = sceBase64Decoder((char *)buf + len, line, lineLen);
			if (ret < 0)
				return SCE_HTTP_ERROR_INVALID_VALUE;
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

int _sceHttpsLoadCert(int caCertNum, const SceHttpsData **caList, const SceHttpsData *cert,
		      const SceHttpsData *privKey)
{
	SceSize size, derSize;
	char *pem;
	void *der;
	int i, ret;

	ret = sceSslInternalIsInitalized();
	if (ret < 0)
		return ret;
	ret = 0;
	if (caList != NULL && caCertNum > 0) {
		for (i = 0; i < caCertNum; i++, caList++) {
			pem = _sceHttpMalloc(NULL, (*caList)->size + 1);
			if (pem == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				der = NULL;
				goto fail;
			}
			sceClibMemcpy_safe(pem, (*caList)->ptr, (*caList)->size);
			pem[(*caList)->size] = '\0';
			if (_sceHttpsPemDecode(pem, NULL, &size, 0) < 0) {
				ret = SCE_HTTP_ERROR_SSL;
				der = NULL;
				goto fail;
			}
			der = _sceHttpMalloc(NULL, size);
			if (der == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto fail;
			}
			if (_sceHttpsPemDecode(pem, der, &derSize, size) < 0) {
				ret = SCE_HTTP_ERROR_SSL;
				goto fail;
			}
			ret = SceHttp_F8885CFD(der, derSize);
			if (ret < 0)
				goto fail;
			_sceHttpFree(NULL, pem);
			_sceHttpFree(NULL, der);
		}
	}
	if (cert != NULL) {
		pem = _sceHttpMalloc(NULL, cert->size + 1);
		if (pem == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			der = NULL;
			goto fail;
		}
		sceClibMemcpy_safe(pem, cert->ptr, cert->size);
		pem[cert->size] = '\0';
		if (_sceHttpsPemDecode(pem, NULL, &size, 0) < 0) {
			ret = SCE_HTTP_ERROR_SSL;
			der = NULL;
			goto fail;
		}
		der = _sceHttpMalloc(NULL, size);
		if (der == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto fail;
		}
		if (_sceHttpsPemDecode(pem, der, &derSize, size) < 0) {
			ret = SCE_HTTP_ERROR_SSL;
			goto fail;
		}
		ret = SceHttp_031EBD2A(der, derSize);
		if (ret < 0)
			goto fail;
		_sceHttpFree(NULL, pem);
		_sceHttpFree(NULL, der);
	}
	if (privKey != NULL)
		ret = SceHttp_802E3C30(privKey->ptr, privKey->size);
	return ret;

fail:
	if (pem != NULL)
		_sceHttpFree(NULL, pem);
	if (der != NULL)
		_sceHttpFree(NULL, der);
	return ret;
}

int _sceHttpsUnloadCert(void)
{
	SceHttp_0CC59EF6();
	if ((SceUInt32)_sceHttpGetSdkVersion() >= 0x1800000)
		SceHttp_42108FBE();
	return 0;
}

int _sceHttpsEnableOption(SceUInt32 sslFlags)
{
	int ret;

	ret = _sceHttpLock(0);
	if (ret >= 0) {
		g_httpsSslFlags |= sslFlags;
		_sceHttpUnlock(0);
	}
	return ret;
}

int _sceHttpsDisableOption(SceUInt32 sslFlags)
{
	int ret;

	ret = _sceHttpLock(0);
	if (ret >= 0) {
		g_httpsSslFlags &= ~sslFlags;
		_sceHttpUnlock(0);
	}
	return ret;
}

int _sceHttpsSetSslCallback(int id, SceHttpsCallback cbfunc, void *userArg)
{
	HttpTemplate *tmpl = NULL;
	HttpConnection *conn = NULL;
	int type, ret;

	ret = _sceHttpGetObjectType(id);
	if (ret >= 0) {
		type = ret;
		ret = _sceHttpLock(0);
		if (ret >= 0) {
			if (type <= HTTP_OBJECT_TEMPLATE) {
				ret = _sceHttpGetTemplate(&tmpl, id);
				if (ret >= 0) {
					tmpl->sslCallback = cbfunc;
					tmpl->sslCallbackUserArg = userArg;
				}
			} else if (type <= HTTP_OBJECT_CONNECTION) {
				ret = _sceHttpGetConnection(&conn, id);
				if (ret >= 0) {
					conn->tmpl.sslCallback = cbfunc;
					conn->tmpl.sslCallbackUserArg = userArg;
				}
			} else {
				ret = SCE_HTTP_ERROR_INVALID_ID;
			}
			_sceHttpUnlock(0);
		}
	}
	if (conn != NULL)
		_sceHttpConnectionRelease(conn);
	else if (tmpl != NULL)
		_sceHttpTemplateRelease(tmpl);
	return ret;
}

int _sceHttpsSetSslVersion(int id, int version, SceBool set)
{
	HttpTemplate *tmpl = NULL, *settings;
	HttpConnection *conn = NULL;
	SceUChar8 valid = set;
	int type, ret;

	ret = _sceHttpGetObjectType(id);
	if (ret >= 0) {
		type = ret;
		ret = _sceHttpLock(0);
		if (ret >= 0) {
			if (type <= HTTP_OBJECT_TEMPLATE) {
				ret = _sceHttpGetTemplate(&tmpl, id);
				settings = tmpl;
				if (ret < 0)
					goto unlock;
			} else if (type <= HTTP_OBJECT_CONNECTION) {
				ret = _sceHttpGetConnection(&conn, id);
				if (ret < 0)
					goto unlock;
				settings = tmpl = &conn->tmpl;
			} else {
				ret = SCE_HTTP_ERROR_INVALID_ID;
				goto unlock;
			}
			if (valid != 0) {
				ret = 0;
				settings->sslVersion = version;
			} else {
				ret = SCE_HTTP_ERROR_INVALID_VALUE;
			}
unlock:
			_sceHttpUnlock(0);
		}
	}
	if (conn != NULL)
		_sceHttpConnectionRelease(conn);
	else if (tmpl != NULL)
		_sceHttpTemplateRelease(tmpl);
	return ret;
}

/* the SSL layer's errors (cert.c's) as libhttp's */
static inline __attribute__((always_inline)) int _sceHttpsSslError(int err)
{
	switch (err) {
	case HTTPS_SSL_ERROR_INVALID_VALUE:
		return SCE_HTTP_ERROR_INVALID_VALUE;
	case HTTPS_SSL_ERROR_INVALID_ID:
		return SCE_HTTP_ERROR_INVALID_ID;
	case HTTPS_SSL_ERROR_OUT_OF_SIZE:
		return SCE_HTTP_ERROR_OUT_OF_SIZE;
	case HTTPS_SSL_ERROR_OUT_OF_MEMORY:
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;
	case HTTPS_SSL_ERROR_SSL:
		return SCE_HTTP_ERROR_SSL;
	case HTTPS_SSL_ERROR_CERT:
		return SCE_HTTPS_ERROR_CERT;
	case HTTPS_SSL_ERROR_INTERNAL:
		return SCE_HTTPS_ERROR_INTERNAL;
	}
	return err;
}

int _sceHttpsGetSslError(int id, int *errNum, SceUInt32 *detail)
{
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	int type, ret;

	ret = _sceHttpGetObjectType(id);
	if (ret < 0)
		return ret;
	type = ret;
	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	if (type <= HTTP_OBJECT_CONNECTION) {
		if (type > HTTP_OBJECT_TEMPLATE) {
			ret = _sceHttpGetConnection(&conn, id);
			if (ret >= 0) {
				*errNum = _sceHttpsSslError(conn->sslError);
				ret = 0;
				*detail = conn->sslErrorDetail;
			}
		} else {
			ret = SCE_HTTP_ERROR_INVALID_ID;
		}
	} else if (type > HTTP_OBJECT_REQUEST) {
		ret = SCE_HTTP_ERROR_INVALID_ID;
	} else {
		ret = _sceHttpGetRequest(&req, id);
		if (ret < 0)
			goto out;
		ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
		if (ret < 0)
			goto out;
		conn = _sceHttpRequestGetConnection(req);
		if (conn != NULL) {
			*errNum = _sceHttpsSslError(conn->sslError);
			ret = 0;
			*detail = conn->sslErrorDetail;
			conn = NULL;
		} else {
			ret = SCE_HTTP_ERROR_INVALID_ID;
		}
	}
	if (req != NULL)
		sceKernelUnlockLwMutex(&req->lock, 1);
out:
	if (conn != NULL)
		_sceHttpConnectionRelease(conn);
	if (req != NULL)
		_sceHttpRequestRelease(req);
	_sceHttpUnlock(0);
	return ret;
}

int _sceHttpsGetCaList(SceSslCert ***caCerts, SceInt32 *caNum)
{
	HttpsSslContext context;

	sceClibMemset(&context, 0, HTTPS_SSL_CONTEXT_SIZE);
	context.malloc = _sceHttpsDefaultMalloc;
	context.realloc = _sceHttpsDefaultRealloc;
	context.free = _sceHttpsDefaultFree;
	context.allocatorArg = NULL;
	return _sceHttpsSslError(_sceHttpsCertGetCaList(caCerts, caNum, &context));
}

int _sceHttpsFreeCaList(SceSslCert **caCerts, SceInt32 caNum)
{
	HttpsSslContext context;

	sceClibMemset(&context, 0, HTTPS_SSL_CONTEXT_SIZE);
	context.malloc = _sceHttpsDefaultMalloc;
	context.realloc = _sceHttpsDefaultRealloc;
	context.free = _sceHttpsDefaultFree;
	context.allocatorArg = NULL;
	return _sceHttpsSslError(_sceHttpsCertFreeCaList(caCerts, caNum, &context));
}

/*
 * Whether the connection id, or a request's connection, has its SSL context. A request's
 * connection is released at the end without having been taken (Sony's).
 */
int _sceHttpSslIsCtxCreated(int id, SceBool *created)
{
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	int type, ret;

	ret = _sceHttpGetObjectType(id);
	if (ret < 0)
		return ret;
	type = ret;
	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	if (type <= HTTP_OBJECT_CONNECTION) {
		if (type > HTTP_OBJECT_TEMPLATE) {
			ret = _sceHttpGetConnection(&conn, id);
			if (ret >= 0) {
				*created = conn->sslCtx != NULL;
				ret = 0;
			}
		} else {
			ret = SCE_HTTP_ERROR_INVALID_ID;
		}
	} else if (type <= HTTP_OBJECT_REQUEST) {
		ret = _sceHttpGetRequest(&req, id);
		if (ret < 0)
			goto out;
		ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
		if (ret < 0)
			goto out;
		conn = _sceHttpRequestGetConnection(req);
		if (conn != NULL) {
			*created = conn->sslCtx != NULL;
			ret = 0;
		} else {
			ret = SCE_HTTP_ERROR_INVALID_ID;
		}
	} else {
		ret = SCE_HTTP_ERROR_INVALID_ID;
	}
	if (req != NULL)
		sceKernelUnlockLwMutex(&req->lock, 1);
out:
	if (conn != NULL)
		_sceHttpConnectionRelease(conn);
	if (req != NULL)
		_sceHttpRequestRelease(req);
	_sceHttpUnlock(0);
	return ret;
}

void _sceHttpsInit(void)
{
}

void _sceHttpsTerm(void)
{
}

/*
 * The client certificate and its private key, when the server asks for one: only when the
 * certificate's issuer is one of the CAs the server names.
 */
static int _sceHttpsSslClientCertCallback(void *ssl, void **certOut, void **keyOut)
{
	void *cert = NULL, *certLib = NULL, *name = NULL, *keyLib = NULL, *ca = NULL;
	void *end = NULL, *key, *bio;
	SceSize certSize, keySize, pos;
	char *certBuf = NULL, *keyBuf;
	int caNum, i, ret, n;

	*certOut = NULL;
	*keyOut = NULL;
	if (SceSslInternal_24C6257F(ssl, HTTPS_SSL_VALUE_KEY_LIBRARY, &keyLib) != 0)
		goto fail;
	if (SceSslInternal_24C6257F(ssl, HTTPS_SSL_VALUE_CERT_LIBRARY, &certLib) != 0)
		goto fail;
	caNum = SceSslInternal_A4EA9DE0(ssl);
	if (_sceHttpsCertGetClientCert(NULL, 0, &certSize) < 0)
		goto fail;
	certBuf = _sceHttpMalloc(NULL, certSize);
	if (certBuf == NULL || _sceHttpsCertGetClientCert(certBuf, certSize, NULL) < 0)
		goto fail;
	if (cert != NULL) {
		SceSslInternal_144FA354(cert);
		cert = NULL;
	}
	if (SceSslInternal_29AAC768(certLib, 0, HTTPS_CERT_FORMAT_DER, certSize, certBuf, &end,
				    &cert) != 0)
		goto fail;

	key = NULL;
	ret = _sceHttpsCertGetPrivateKey(NULL, 0, &keySize);
	if (ret >= 0) {
		keyBuf = _sceHttpMalloc(NULL, keySize);
		bio = NULL;
		if (keyBuf != NULL) {
			ret = _sceHttpsCertGetPrivateKey(keyBuf, keySize, NULL);
			bio = NULL;
			if (ret >= 0) {
				bio = SceSslInternal_B8E6E249();
				if (bio == NULL) {
					ret = HTTPS_ERROR_NO_BIO;
				} else {
					for (pos = 0; pos < keySize; pos += n) {
						n = SceSslInternal_33AC7A16(bio, &keyBuf[pos],
									    keySize - pos);
						if (n < 0) {
							ret = n;
							goto read;
						}
					}
					ret = SceSslInternal_7D30F0A6(bio, keyLib, &key, HTTPS_KEY_TYPE,
								      HTTPS_KEY_FORMAT);
				}
			}
read:
			if (keyBuf != NULL)
				_sceHttpFree(NULL, keyBuf);
		}
		if (bio != NULL)
			SceSslInternal_C4D984C2(bio);
	}
	if (ret != 0) {
		if (key != NULL) {
			SceSslInternal_46358909(key);
			key = NULL;
		}
		goto fail;
	}
	if (key == NULL)
		goto fail;
	if (SceSslInternal_D68D8421(cert, key) != 0 &&
	    SceSslInternal_81FA5DBE(cert, HTTPS_CERT_ISSUER, &name) == 0 && caNum > 0) {
		for (i = 0; i < caNum; i++) {
			if (SceSslInternal_243DE290(ssl, i, &ca) != 0)
				break;
			if (SceSslInternal_568F0E80(ca, name) == 1) {
				*certOut = cert;
				*keyOut = key;
				ret = 1;
				goto out;
			}
		}
	}
	if (key != NULL)
		SceSslInternal_46358909(key);
fail:
	ret = 0;
	if (cert != NULL) {
		SceSslInternal_144FA354(cert);
		ret = 0;
		cert = NULL;
	}
out:
	if (certBuf != NULL)
		_sceHttpFree(NULL, certBuf);
	if (name != NULL)
		SceSslInternal_CE37B69E(name);
	return ret;
}

/* the connection's allocator, for the SSL layer (arg is the connection's template) */
static void *_sceHttpsSslMalloc(void *arg, SceSize size)
{
	return _sceHttpMalloc(((HttpTemplate *)arg)->allocator.malloc, size);
}

static void *_sceHttpsSslRealloc(void *arg, void *ptr, SceSize size)
{
	return _sceHttpRealloc(((HttpTemplate *)arg)->allocator.realloc, ptr, size);
}

static void _sceHttpsSslFree(void *arg, void *ptr)
{
	_sceHttpFree(((HttpTemplate *)arg)->allocator.free, ptr);
}

/* the library's lock, for the SSL layer */
static int _sceHttpsLock(void *arg)
{
	return _sceHttpLock(0);
}

static int _sceHttpsUnlock(void *arg)
{
	return _sceHttpUnlock(0);
}

/* the default allocator, for the SSL layer */
static void *_sceHttpsDefaultMalloc(void *arg, SceSize size)
{
	return _sceHttpMalloc(NULL, size);
}

static void *_sceHttpsDefaultRealloc(void *arg, void *ptr, SceSize size)
{
	return _sceHttpRealloc(NULL, ptr, size);
}

static void _sceHttpsDefaultFree(void *arg, void *ptr)
{
	_sceHttpFree(NULL, ptr);
}
