/*
 * SceLibHttp's private definitions: what its files share.
 */
#ifndef HTTP_INTERNAL_H
#define HTTP_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <libmd5.h>
#include <libbase64.h>
#include <net/socket.h>
#include <net/in.h>

/* SceAppMgr */
int sceAppMgrIsGameProgram(void);
int sceAppMgrIsNonGameProgram(void);

/* vitasdk: sceKernelGetSystemTimeLow */
extern SceUInt32 SceThreadmgr_47F6DE49(void);

/* sceClib's character classes */
#define isspace(c)	(sceClibLookCtypeTable(c) & SCE_CLIB_CTYPE_SPACE)
#define isdigit(c)	(sceClibLookCtypeTable(c) & SCE_CLIB_CTYPE_DIGIT)

/* the memory functions an object allocates with; NULL takes the library's pool */
typedef struct HttpAllocator {
	void *(*malloc)(SceSize size);
	void (*free)(void *ptr);
	void *(*realloc)(void *ptr, SceSize size);
} HttpAllocator;

void *_sceHttpMalloc(void *(*func)(SceSize size), SceSize size);
void _sceHttpFree(void (*func)(void *ptr), void *ptr);

/* the kinds of object an id names (its top four bits: 1, 2, 3) */
#define HTTP_OBJECT_TEMPLATE		0
#define HTTP_OBJECT_CONNECTION		1
#define HTTP_OBJECT_REQUEST		2

/* what every object starts with: its id, references and place in its kind's list */
typedef struct HttpObject {
	int id;
	int refCount;
	SceBool deleted;
	struct HttpObject *next;
	struct HttpObject *prev;
} HttpObject;

/* a header line of a template or request, in a list */
typedef struct HttpHeader {
	char *name;
	char *value;
	void (*free)(void *ptr);
	struct HttpHeader *next;
} HttpHeader;

/*
 * A template's settings (0xe4 bytes). A connection and a request start from a copy of
 * their parent's (_sceHttpTemplateCopy), so a setting is changed on any of the three.
 */
typedef struct HttpTemplate {
	HttpObject object;
	int httpVer;
	SceBool proxyEnabled;
	SceBool keepAlive;
	SceBool autoRedirect;
	SceBool cookieEnabled;
	SceBool basicAuthEnabled;
	SceBool digestAuthEnabled;
	SceBool proxyAuthEnabled;
	int reserved34;
	int autoProxyConf;
	SceBool legacySdk;
	SceBool nonblock;
	int resolveRetry;
	SceUInt32 resolveTimeout;
	SceUInt32 connectTimeout;
	SceUInt32 sendTimeout;
	SceUInt32 recvTimeout;
	int maxRedirects;
	int maxAuthRetries;
	SceSize responseHeaderMaxSize;
	SceSize recvBufferSize;
	int reserved68[2];
	int reserved70;
	char *proxyServer;
	char *username;
	char *password;
	SceHttpAuthInfoCallback authInfoCallback;
	void *authInfoUserArg;
	int reserved88[2];
	SceHttpCookieSendCallback cookieSendCallback;
	void *cookieSendUserArg;
	SceHttpCookieRecvCallback cookieRecvCallback;
	void *cookieRecvUserArg;
	int reservedA0[2];
	struct HttpHeader *headers;
	void *reservedAc;
	HttpAllocator allocator;
	void (*freeObject)(void *ptr);
	int reservedC0;
	int reservedC4[2];
	SceUShort16 proxyPort;
	SceUShort16 reservedCe;
	SceBool acceptEncodingGzipEnabled;
	int reservedD4;
	SceBool inflateGzipEnabled;
	int reservedDc;
	int reservedE0;
} HttpTemplate;
_Static_assert(sizeof(HttpTemplate) == 0xe4, "HttpTemplate");

/* a connection to one server (0x190 bytes) */
typedef struct HttpConnection {
	HttpObject object;
	SceUInt32 state;
	int reserved18;
	SceBool https;
	int owner;
	int sock;
	int savedSendTimeout;
	int resolverId;
	SceNetSockaddrIn addr;
	int reserved40[2];
	char *hostname;
	int reserved4c;
	int reserved50;
	SceUID sema;
	int reserved58[3];
	HttpTemplate tmpl;
	SceUShort16 port;
	SceUShort16 reserved14a;
	void *recvBuffer;
	SceSize recvBufferSize;
	int reserved154[3];
	int reserved160[10];
	SceBool aborted;			/* sceNetSocketAbort was called on sock */
	int reserved18c;
} HttpConnection;
_Static_assert(sizeof(HttpConnection) == 0x190, "HttpConnection");

/* a response's status line and the headers libhttp reads (0x58 bytes) */
typedef struct HttpResponse {
	SceULong64 contentLength;
	SceBool contentLengthKnown;
	SceBool chunked;
	SceBool close;				/* the server closes the connection after it */
	int httpMajorVer;
	int httpMinorVer;
	int statusCode;
	const char *reasonPhrase;
	SceSize reasonPhraseLen;
	const char *etag;
	SceSize etagLen;
	const char *server;
	SceSize serverLen;
	const char *date;
	SceSize dateLen;
	const char *lastModified;
	SceSize lastModifiedLen;
	const char *location;
	SceSize locationLen;
	const char *wwwAuthenticate;
	SceSize wwwAuthenticateLen;
} HttpResponse;
_Static_assert(sizeof(HttpResponse) == 0x58, "HttpResponse");

/* what sceHttpCreateRequest2 and sceHttpCreateRequestWithURL2 add to a request (0x20 bytes) */
typedef struct HttpRequestExtra {
	char *method;
	int reserved04[7];
} HttpRequestExtra;

/* a request (0x460 bytes) */
typedef struct HttpRequest {
	HttpObject object;
	int state;
	int method;
	int reserved1c[2];
	int authRetries;
	SceULong64 contentLength;
	int reserved30[6];
	struct HttpRequestExtra *extra;	/* method 8's name, ... */
	HttpTemplate tmpl;
	HttpConnection *connection;
	HttpConnection *childConnections;
	int reserved138[2];
	HttpResponse response;
	const char *connectionValue;		/* the [Proxy-]Connection header's value */
	SceSize connectionValueLen;
	SceBool gzipEncoded;			/* Content-Encoding: gzip */
	int reserved1a4;
	char *url;
	char *path;
	char *query;
	char *fragment;
	void *reserved1b8;
	char *responseHeader;
	void *reserved1c0;
	char username[SCE_HTTP_USERNAME_MAX_SIZE];
	char password[SCE_HTTP_PASSWORD_MAX_SIZE];
	struct HttpAuthCacheEntry *authCache;
	struct HttpAuthCacheEntry *proxyAuthCache;
	int reserved3cc[4];
	SceSize requestHeaderSize;		/* with its NUL */
	SceSize requestHeaderSent;
	SceSize responseHeaderSize;		/* of the responseHeader buffer */
	SceSize responseHeaderLen;
	int reserved3ec;
	int icmSock;			/* "SceLibHttpIcm" */
	int icmOption;
	SceKernelLwMutexWork lock;
	void *reserved418;
	int reserved41c[4];
	void *reserved42c;
	int reserved430[2];
	int responseHeaderState;		/* where the reader is in the CR LF CR LF */
	SceSize responseHeaderPos;
	int reserved440[8];
} HttpRequest;
_Static_assert(sizeof(HttpRequest) == 0x460, "HttpRequest");

/* a connection's sockets, for SceHttp_54847CFE */
typedef struct HttpConnectionSockInfo {
	SceBool connected;
	SceBool aborted;
	int sock;
	int resolverId;
} HttpConnectionSockInfo;

/* a request's sockets, for SceHttp_80E6013D */
typedef struct HttpRequestSockInfo {
	int sock;
	int icmSock;
	int resolverId;
} HttpRequestSockInfo;

/* a challenge of a WWW-Authenticate or Proxy-Authenticate header (0x38 bytes) */
typedef struct HttpAuthChallenge {
	int type;			/* SceHttpAuthType, +2 for a proxy */
	const char *realm;
	SceSize realmLen;
	const char *qop;
	SceSize qopLen;
	const char *nonce;
	SceSize nonceLen;
	const char *opaque;
	SceSize opaqueLen;
	const char *algorithm;
	SceSize algorithmLen;
	const char *domain;
	SceSize domainLen;
	SceBool stale;
} HttpAuthChallenge;

/* what a Digest response is computed from, in one buffer of the allocator's */
typedef struct HttpAuthDigest {
	const char *realm;
	const char *qop;
	const char *nonce;
	const char *opaque;
	const char *algorithm;
	const char *domain;
	SceUInt32 nonceCount;
	char *buffer;
	const HttpAllocator *allocator;
} HttpAuthDigest;

/* the credentials the authentication cache keeps for one realm (0x29c bytes) */
typedef struct HttpAuthCacheEntry {
	int refCount;
	int state;
	HttpAuthChallenge challenge;
	char *challengeBuffer;
	SceHttpUriElement uri;
	char *uriBuffer;
	SceUChar8 realmDigest[SCE_MD5_DIGEST_SIZE];
	char username[SCE_HTTP_USERNAME_MAX_SIZE];
	char password[SCE_HTTP_PASSWORD_MAX_SIZE];
	struct HttpAuthCacheEntry *prev;
	struct HttpAuthCacheEntry *next;
	SceBool save;
	int reserved[3];
} HttpAuthCacheEntry;
_Static_assert(sizeof(HttpAuthCacheEntry) == 0x29c, "HttpAuthCacheEntry");

/* what a lookup in the authentication cache is for */
typedef struct HttpAuthCacheKey {
	char **header;
	const SceHttpUriElement *uri;
	SceHttpUriElement proxy;
	int reserved;
	HttpRequest *request;
} HttpAuthCacheKey;

/* a cookie domain's reference taken while it is being deleted (no SDK name) */
#define HTTP_ERROR_COOKIE_DELETED	-2143088509	/* 0x80431083 */

/* a cookie (0x48 bytes) */
typedef struct HttpCookie {
	char *name;
	char *value;
	char *domain;
	char *path;
	char *comment;
	char *commentUrl;
	SceUShort16 *ports;
	struct HttpCookie *next;
	SceULong64 expires;			/* 0: for this session only */
	SceSize size;				/* what its strings take */
	SceUInt32 portCount;
	SceUInt32 version;
	int type;				/* the header it came in (_sceHttpCookieCreateList) */
	SceBool secure;
	SceBool discard;
	SceBool deleted;
	int reserved44;
} HttpCookie;
_Static_assert(sizeof(HttpCookie) == 0x48, "HttpCookie");

/*
 * The cookies of one domain (0x2c bytes): in a hash bucket by the domain's name, and in a list
 * from the least recently used domain (head) to the most recently used (tail).
 */
typedef struct HttpCookieDomain {
	struct HttpCookieDomain *next;
	struct HttpCookieDomain *prev;
	struct HttpCookieDomain *older;
	struct HttpCookieDomain *newer;
	HttpCookie *cookies;
	SceBool deleted;
	int reserved18;
	int refCount;
	SceUInt32 cookieCount;
	char *name;
	int reserved28;
} HttpCookieDomain;
_Static_assert(sizeof(HttpCookieDomain) == 0x2c, "HttpCookieDomain");

/* a name=value pair of a Set-Cookie header, in a list from the last to the first */
typedef struct HttpCookieAttr {
	char *name;
	char *value;
	struct HttpCookieAttr *next;
} HttpCookieAttr;

/* the cookies a request sends, sorted from the most specific (longest path) on */
typedef struct HttpCookieMatchEntry {
	HttpCookie *cookie;
	struct HttpCookieMatchEntry *prev;
	struct HttpCookieMatchEntry *next;
} HttpCookieMatchEntry;

typedef struct HttpCookieMatch {
	HttpCookieMatchEntry entries[10];
	struct HttpCookieMatch *next;
	int count;
	int reserved80;
} HttpCookieMatch;
_Static_assert(sizeof(HttpCookieMatch) == 0x84, "HttpCookieMatch");

/* a domain a lookup holds a reference to */
typedef struct HttpCookieDomainRef {
	HttpCookieDomain *domain;
	struct HttpCookieDomainRef *next;
} HttpCookieDomainRef;

/* auth.c */
SceSize _sceHttpAuthParseParam(const char *param, SceSize size, const char **value, SceSize *valueLen);
int _sceHttpAuthParseChallenge(HttpAuthChallenge *challenge, const char *header, SceSize size,
			       int proxy);
int _sceHttpAuthBasic(const HttpAllocator *allocator, char *buf, SceSize *needed, SceSize size,
		      const char *username, const char *password);
void _sceHttpAuthHex(const SceUChar8 *digest, char *hex);
int _sceHttpAuthDigestParam(char *buf, SceSize size, SceSize *len, const char *name,
			    SceSize nameLen, const char *value, SceBool write);
int _sceHttpAuthDigest(char *buf, SceSize *needed, SceSize size, const char *username,
		       const char *password, const char *uri, int method, const void *entity,
		       SceSize entitySize, const HttpAuthDigest *digest);
int _sceHttpAuthDigestInit(const HttpAllocator *allocator, HttpAuthDigest *digest,
			   const HttpAuthChallenge *challenge);
void _sceHttpAuthDigestTerm(HttpAuthDigest *digest);
int _sceHttpSetAuthInfoCallback(int id, SceHttpAuthInfoCallback cbfunc, void *userArg);
int _sceHttpSetAuthEnabled(int id, const SceBool *basic, const SceBool *digest);
int _sceHttpGetAuthEnabled(int id, SceBool *basic, SceBool *digest);
int _sceHttpAuthCheckResponse(HttpRequest *req);
int _sceHttpAuthAddProxyAuthorization(HttpRequest *req);
int _sceHttpAuthAddAuthorization(HttpRequest *req);

/* authcache.c */
void _sceHttpAuthCacheRemove(HttpAuthCacheEntry *entry);
int _sceHttpAuthCacheFlush(void);
int _sceHttpAuthCacheInit(void);
int _sceHttpAuthCacheTerm(void);
int _sceHttpAuthCacheRelease(HttpAuthCacheEntry *entry);
int _sceHttpAuthCacheDelete(HttpAuthCacheEntry *entry);
int _sceHttpAuthCacheConfirm(HttpAuthCacheEntry *entry);
int _sceHttpAuthCacheGetCredentials(const HttpAuthCacheEntry *entry, char *username,
				    char *password);
void _sceHttpAuthCacheFreeHeader(char *header);
int _sceHttpAuthCacheMakeHeader(HttpAuthCacheKey *key, const void *entity, SceSize entitySize,
				HttpAuthCacheEntry *entry);
int _sceHttpAuthChallengeCopy(HttpAuthChallenge *dst, char **buffer,
			      const HttpAuthChallenge *src);
int _sceHttpAuthCacheSearch(HttpAuthCacheKey *key, const void *entity, SceSize entitySize,
			    int type, const char *realm, HttpAuthCacheEntry **entry,
			    const HttpAuthChallenge *challenge, SceBool anyPath,
			    SceBool forChallenge, SceBool proxy);
int _sceHttpAuthCacheLookup(HttpAuthCacheKey *key, const void *entity, SceSize entitySize,
			    HttpAuthCacheEntry **entry, SceBool anyPath, SceBool proxy);
int _sceHttpAuthCacheFind(HttpAuthCacheKey *key, const void *entity, SceSize entitySize,
			  int type, const char *realm, HttpAuthCacheEntry **entry,
			  const HttpAuthChallenge *challenge, SceBool anyPath, SceBool proxy);
int _sceHttpAuthCacheAdd(HttpAuthCacheKey *key, const HttpAuthChallenge *challenge,
			 const char *username, const char *password, HttpAuthCacheEntry **entry,
			 SceBool save, SceBool proxy);

/* conn.c */
int _sceHttpConnectionCreate(HttpTemplate *tmpl, const char *hostname, const char *scheme,
			     const char *username, const char *password, SceUShort16 port,
			     SceBool keepAlive, SceBool internal, HttpRequest *parent);
int _sceHttpCreateConnectionWithURL(HttpTemplate *tmpl, int tmplId, const char *url, SceBool keepAlive,
				    SceBool internal, HttpRequest *parent);
int _sceHttpCreateConnection(int tmplId, const char *serverName, const char *scheme,
			     SceUShort16 port, SceBool keepAlive);
int _sceHttpDeleteConnection(int connId);
void _sceHttpConnectionDestroy(HttpConnection *conn);
int _sceHttpConnectionLock(HttpConnection *conn, const int *owner, SceBool nonblock,
			   SceUInt *timeout);
SceBool _sceHttpConnectionIsOwner(HttpConnection *conn, const int *owner);
SceBool _sceHttpConnectionIsActive(const HttpConnection *conn);
int _sceHttpConnectionUnlock(HttpConnection *conn, const int *owner);
int _sceHttpConnectionConnect(HttpRequest *req);

/* cookie.c */
SceSize _sceHttpCookieFreeFields(HttpCookie *cookie);
SceSize _sceHttpCookieFreeList(HttpCookie *cookie);
int _sceHttpCookieSerialize(void *buffer, SceSize bufferSize, SceSize *exportSize);
int _sceHttpCookieExport(void *buffer, SceSize bufferSize, SceSize *exportSize);
int _sceHttpCookieCreate(HttpCookie **out, const char *name, const char *value, const char *comment,
	const char *commentUrl, const char *domain, const char *path, const SceUShort16 *ports,
	SceUInt32 portCount, const SceULong64 *expires, SceUInt32 version, SceBool secure,
	SceBool discard, int type);
char *_sceHttpCookieGetBaseDomain(char *host);
int _sceHttpCookieDomainRelease(HttpCookieDomain *domain, SceBool lock);
int _sceHttpCookieAddList(HttpCookie *list, SceSize size, SceUInt32 count);
int _sceHttpCookieDeserialize(const void *buffer, SceSize bufferSize);
int _sceHttpCookieImport(const void *buffer, SceSize bufferSize);
void _sceHttpCookieMatchFree(HttpCookieMatch *match, HttpCookieDomainRef *refs);
int _sceHttpCookieMakeHeader(char *cookie, HttpCookieMatch *match, SceSize *required, SceSize prepared);
int _sceHttpCookieDomainMatch(const char *domain, const char *host);
int _sceHttpCookieMatchAdd(HttpCookieMatch *match, HttpCookie *cookie, const char *path, const char *scheme);
int _sceHttpCookieFind(HttpCookieMatch **match, char *host, const char *path, const char *scheme,
	HttpCookieDomainRef **refs);
int _sceHttpCookieGet(const char *url, char *cookie, SceSize prepared, SceSize *required);
int _sceHttpCookieFlush(void);
void _sceHttpCookieNop(void);
int _sceHttpCookieInit(void);
int _sceHttpCookieTerm(void);
int _sceHttpCookieParseAttrs(const char *header, SceSize headerLen, HttpCookieAttr **attrs,
	SceBool *secure, SceBool *discard, int type);
int _sceHttpCookieParsePorts(const char *list, SceUShort16 port, SceUShort16 *ports);
int _sceHttpCookieCreateList(HttpCookie **list, SceSize *size, SceUInt32 *count, HttpCookieAttr *attrs,
	char *host, const char *path, SceUShort16 port, SceBool secure, SceBool discard, int type);
int _sceHttpCookieParse(const char *header, SceSize headerLen, char *host, const char *path,
	SceUShort16 port, HttpCookie **list, SceUInt32 *count, SceSize *size);
int _sceHttpCookieParseHeader(const char *header, SceSize headerLen, char *host, const char *path,
	HttpCookie **list, SceUInt32 *count, SceSize *size);
int _sceHttpCookieAdd(HttpCookie *list, SceSize size, SceUInt32 count);
int _sceHttpCookieCallRecvCallback(HttpRequest *req, const char *url, const char *cookieHeader,
	SceSize headerLen);
int _sceHttpCookieCallSendCallback(HttpRequest *req, const char *url, const char *cookieHeader);
int _sceHttpCookieAddUrl(const char *url, const char *cookie, SceSize cookieLength);
SceUInt32 _sceHttpCookieGetMaxNumPerDomain(void);
int _sceHttpCookieSetMaxNumPerDomain(SceUInt32 num);
int _sceHttpCookieSetMaxNum(SceUInt32 num);
int _sceHttpCookieSetMaxSize(SceUInt32 size);
int _sceHttpCookieSetTotalMaxSize(SceUInt32 size);
int _sceHttpCookieGetStats(SceHttpCookieStats *stats);
void _sceHttpCookieNop2(void);
void _sceHttpCookieNop3(void);

/* debug.c */
int SceHttp_5B6A0ACE(void);
void SceHttp_FE60380A(void);
void SceHttp_B0E41827(void);
void SceHttp_88116DB8(void);
void SceHttp_0B0D8E7A(void);
int SceHttp_54847CFE(int connId, HttpConnectionSockInfo *info);
int SceHttp_80E6013D(int reqId, HttpRequestSockInfo *info);
int _sceHttpDbgInit(void);
/* headers */
int _sceHttpHeaderCreate(const HttpAllocator *allocator, HttpHeader **header, const char *name,
			 const char *value);
void _sceHttpHeaderFree(HttpHeader *header);
int _sceHttpHeaderCopy(const HttpAllocator *allocator, HttpHeader **copy, const HttpHeader *list);
int _sceHttpHeaderRemove(HttpHeader **list, const char *name);
SceSize _sceHttpRequestMakeHeader(HttpRequest *req, char *buf, SceSize size, SceBool countOnly);
int _sceHttpRequestCreateHeader(char **header, HttpRequest *req);
int _sceHttpRequestFreeHeader(HttpRequest *req, char **header);
int _sceHttpReadResponseHeader(HttpRequest *req);
int _sceHttpResponseParse(const char *header, SceSize len, HttpResponse *resp);
int _sceHttpRequestRecvResponseHeader(HttpRequest *req, HttpConnection *conn);
int _sceHttpRequestGetResponseHeader(HttpRequest *req);
int _sceHttpRemoveRequestHeader(int id, const char *name);
int _sceHttpGetTemplateById(int id, HttpTemplate **tmpl, HttpConnection **conn,
			    HttpRequest **req);
void _sceHttpRequestResetResponseHeader(HttpRequest *req);
int _sceHttpMakeHostHeader(const HttpAllocator *allocator, char **header, SceBool https,
			   const char *hostname, SceUShort16 port);

/* templates */
int _sceHttpTemplateCopy(const HttpTemplate *tmpl, HttpTemplate *copy);
int _sceHttpTemplateClear(HttpTemplate *tmpl);

/* the objects (object.c) */
int _sceHttpObjectRegister(HttpObject *object, int type);
int _sceHttpTakeObject(HttpObject **object, int id, int type);
int _sceHttpGetObjectType(int id);
int _sceHttpGetObjectList(HttpObject **list, int *num, int type);
int _sceHttpGetTemplate(HttpTemplate **tmpl, int id);
int _sceHttpGetConnection(HttpConnection **conn, int id);
int _sceHttpGetRequest(HttpRequest **req, int id);
int _sceHttpTemplateRelease(HttpTemplate *tmpl);
int _sceHttpConnectionRelease(HttpConnection *conn);
int _sceHttpRequestRelease(HttpRequest *req);
int _sceHttpLock(int which);
int _sceHttpUnlock(int which);
int _sceHttpGetSdkVersion(void);
SceULong64 _sceHttpStrToULong64(const char *s);

/* the library's pool */
void *_sceHttpDefaultMalloc(SceSize size);
void _sceHttpDefaultFree(void *ptr);

/* sockets */
int _sceHttpGetScheme(const char *scheme, SceBool *https);
int _sceHttpSocketSetSendTimeout(int sock, const SceNetSockaddrIn *addr, SceUInt32 timeout,
				 int *saved);
int _sceHttpSocketRestoreSendTimeout(int sock, int saved);
int _sceHttpSocketConnect(int sock, const SceNetSockaddrIn *addr, SceUInt32 timeout, int arg);
void _sceHttpConnectionClose(HttpConnection *conn);
int _sceHttpRecv(HttpRequest *req, void *buf, SceSize len, int flags);
int _sceHttpPeek(HttpRequest *req, void *buf, SceSize len, int flags);
SceBool _sceHttpConnectionIsConnected(HttpConnection *conn);
void _sceHttpEpollRemoveConnection(HttpConnection *conn);
int _sceHttpEpollWait(HttpRequest *req, int arg, int events);

/* requests */
int _sceHttpAddRequestHeader(int id, const char *name, const char *value, int mode);
int _sceHttpAddRequestHeaderRaw(int id, const char *header, int mode);
int _sceHttpRequestRemoveAuthHeaders(HttpRequest *req, SceBool remove, SceBool proxy,
				     SceBool www);
int _sceHttpRequestSetBasicAuth(HttpRequest *req, const char *username, const char *password,
				SceBool proxy);
int _sceHttpRequestSetDigestAuth(HttpRequest *req, const char *username, const char *password,
				 const void *entity, SceSize entitySize,
				 const HttpAuthDigest *digest, SceBool proxy);
int _sceHttpRequestGetPath(HttpRequest *req, char **path);
HttpConnection *_sceHttpRequestGetConnection(HttpRequest *req);

/* URIs */
int _sceHttpUriElementCopy(SceHttpUriElement *dst, const SceHttpUriElement *src, char *buf,
			   SceSize size, SceSize *require);

#endif /* HTTP_INTERNAL_H */
