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
#include <net/epoll.h>

/* SceAppMgr */
int sceAppMgrIsGameProgram(void);
int sceAppMgrIsNonGameProgram(void);

/* SceModulemgr, SceRegMgrForGame */
int sceKernelIsCalledFromSysModule(const void *addr);
int sceRegMgrSystemParamGetInt(int id, int *value);

/* vitasdk: sceKernelGetSystemTimeLow */
extern SceUInt32 SceThreadmgr_47F6DE49(void);
SceUInt64 sceKernelGetSystemTimeWide(void);

/* sceClib's character classes */
#define isspace(c)	(sceClibLookCtypeTable(c) & SCE_CLIB_CTYPE_SPACE)
#define isdigit(c)	(sceClibLookCtypeTable(c) & SCE_CLIB_CTYPE_DIGIT)

/* a header's name and value, as sceHttpRequestGetAllHeaders lists them */
typedef struct HttpHeaderPair {
	const char *name;
	const char *value;
} HttpHeaderPair;

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

/* a kind's ids: the last one given and how many objects there are */
typedef struct HttpObjectIds {
	int lastId;
	int num;
} HttpObjectIds;

/* an allocation in a list, with the function that frees it (_sceHttpFreeList) */
typedef struct HttpAllocation {
	void *ptr;
	void (*free)(void *ptr);
	struct HttpAllocation *next;
} HttpAllocation;

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
	SceBool tcpNoDelay;			/* TCP_NODELAY on its sockets */
	int autoProxyConf;
	SceBool legacySdk;
	SceBool nonblock;
	int resolveRetry;
	SceUInt32 resolveTimeout;
	SceUInt32 connectTimeout;
	SceUInt32 recvTimeout;
	SceUInt32 sendTimeout;
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
	SceHttpRedirectCallback redirectCallback;
	void *redirectUserArg;
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
	int reservedC4;
	SceHttpEpollHandle epoll;		/* sceHttpSetEpoll's */
	SceUShort16 proxyPort;
	SceUInt8 sockFlags;			/* SceHttp_F30073EC's; bit 0: HTTP_SO_PRIVATE_FLAGS 8 */
	SceUInt8 reservedCf;
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
	int resolveState;			/* _sceHttpRequestResolve's */
	SceUID sema;
	SceBool reused;				/* a kept-alive connection a request sends on again */
	int reserved5c[2];
	HttpTemplate tmpl;
	SceUShort16 port;
	SceUShort16 reserved14a;
	void *recvBuffer;
	SceSize recvBufferSize;
	SceSize recvPos;			/* the next unread byte of recvBuffer */
	SceSize recvLen;			/* the bytes in it */
	SceBool recvEof;			/* the peer closed */
	int sslError;				/* the SSL failure (0x80435xxx) behind SCE_HTTP_ERROR_SSL */
	int reserved164[9];
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
	int lastError;				/* the error that ended it (state 19) */
	SceUInt32 redirectCount;
	int authRetries;
	SceULong64 contentLength;
	SceULong64 sentLength;			/* of the body, against contentLength */
	SceULong64 receivedLength;		/* of the body, against response.contentLength */
	SceBool followOnly303;			/* only a 303 is followed (nothing here sets it) */
	SceBool aborted;			/* sceHttpAbortRequest was called */
	struct HttpRequestExtra *extra;	/* method 8's name, ... */
	HttpTemplate tmpl;
	HttpConnection *connection;
	HttpConnection *childConnections;
	int reserved138;
	char *redirectLocation;			/* _sceHttpRequestCheckRedirect's */
	HttpResponse response;
	const char *connectionValue;		/* the [Proxy-]Connection header's value */
	SceSize connectionValueLen;
	SceBool gzipEncoded;			/* Content-Encoding: gzip */
	int reserved1a4;
	char *url;
	char *path;
	char *query;
	char *fragment;
	char *requestHeader;			/* _sceHttpRequestCreateHeader's */
	char *responseHeader;
	char *allResponseHeaders;		/* sceHttpGetAllResponseHeaders' copy */
	char username[SCE_HTTP_USERNAME_MAX_SIZE];
	char password[SCE_HTTP_PASSWORD_MAX_SIZE];
	struct HttpAuthCacheEntry *authCache;
	struct HttpAuthCacheEntry *proxyAuthCache;
	const void *postData;			/* sceHttpSendRequest's, being sent */
	SceSize postSize;
	SceSize postSent;
	SceSize chunkRemaining;			/* of the chunk being read */
	SceSize requestHeaderSize;		/* with its NUL */
	SceSize requestHeaderSent;
	SceSize responseHeaderSize;		/* of the responseHeader buffer */
	SceSize responseHeaderLen;
	SceSize allResponseHeadersSize;
	int icmSock;			/* "SceLibHttpIcm" */
	int icmOption;
	SceKernelLwMutexWork lock;
	void *inflateBuffer;			/* a gzip body's input, as received */
	SceSize inflateBufferSize;
	SceSize inflateBufferLen;
	SceBool bodyClosed;			/* the server closed before the body ended */
	SceBool bodyEnd;			/* the last chunk was read */
	struct z_stream_s *inflateStream;	/* a gzip body's */
	SceBool inflateFinished;		/* the gzip stream is at its end */
	SceSize inflateTotalOut;		/* inflateStream's total_out, as counted */
	int responseHeaderState;		/* where the reader is in the CR LF CR LF */
	SceSize responseHeaderPos;
	SceUInt32 chunkState;			/* _sceHttpRequestReadChunked's */
	char chunkHex[12];			/* a chunk's size line */
	int chunkHexLen;
	int lineState;				/* _sceHttpRequestSkipLine's */
	SceSize lineLen;
	int reserved45c;
} HttpRequest;
_Static_assert(sizeof(HttpRequest) == 0x460, "HttpRequest");

/* a request on an epoll handle (sceHttpSetEpoll) */
typedef struct HttpEpollEntry {
	int id;
	void *userArg;
	struct HttpEpollEntry *next;
} HttpEpollEntry;

/* what an SceHttpEpollHandle points to (0x58 bytes) */
typedef struct HttpEpoll {
	int reserved0[2];
	SceNetId epollId;
	HttpEpollEntry *entries;
	SceKernelLwMutexWork listLock;		/* "SceLibHttpEhList", over entries */
	SceBool aborted;			/* sceHttpAbortWaitRequest was called */
	int reserved34;
	SceKernelLwMutexWork lock;		/* "SceLibHttpEh" */
} HttpEpoll;
_Static_assert(sizeof(HttpEpoll) == 0x58, "HttpEpoll");

/* what a request puts on its epoll handle */
#define HTTP_EPOLL_SOCKET	0		/* its connection's socket */
#define HTTP_EPOLL_RESOLVER	1		/* its connection's resolver */
#define HTTP_EPOLL_ICM		2		/* its "SceLibHttpIcm" socket */
#define HTTP_EPOLL_CONNECTION	3		/* a connection's socket, for the connection itself */
#define HTTP_EPOLL_ICM_EVENTS	0x40000		/* the events of an ICM socket (no SDK name) */
#define HTTP_SO_ICM_OPTION	0x10000		/* sceHttpSetIcmOption's bit 0 (no SDK name) */

/* an SceHttpNBEvent's eventDetail: where the request is (the SDK calls it reserved) */
#define HTTP_NB_DETAIL_SENDING		0x00000001	/* before its response header */
#define HTTP_NB_DETAIL_RESPONSE		0x00000002	/* its response header is in */
#define HTTP_NB_DETAIL_BODY		0x00000004	/* a body to read, still coming */
#define HTTP_NB_DETAIL_BODY_DONE	0x00000008	/* a body, all received */
#define HTTP_NB_DETAIL_ERROR		0x80000000	/* it failed (state 19) */

/* a 301 the redirect cache remembers (0x44 bytes) */
typedef struct HttpRedirectCacheEntry {
	SceUInt32 hits;
	int refCount;
	SceUChar8 urlDigest[SCE_MD5_DIGEST_SIZE];	/* of url */
	char *url;
	int method;
	SceUChar8 key[16];			/* compared when a lookup gives one */
	char *location;				/* where url moved to */
	SceBool removed;			/* freed when the last reference goes */
	struct HttpRedirectCacheEntry *prev;	/* in the list, the most recently used first */
	struct HttpRedirectCacheEntry *next;
	struct HttpRedirectCacheEntry *nextFree;
} HttpRedirectCacheEntry;
_Static_assert(sizeof(HttpRedirectCacheEntry) == 0x44, "HttpRedirectCacheEntry");

#define HTTP_REDIRECT_CACHE_ENTRIES	16

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
int _sceHttpCookieInit(void *reserved0, void *reserved1);
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
/* request.c */
int _sceHttpRequestAddCookieHeader(HttpRequest *req);
int _sceHttpMakeUserAgent(char *buf, const char *name, SceSize *size);
int _sceHttpRequestGetAllHeaders(int reqId, HttpHeaderPair *headers, SceUInt32 *num,
				 char *contentLength, SceSize *contentLengthSize);
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
int _sceHttpObjectRegister(HttpObject *object, SceUInt32 type);
int _sceHttpFindObject(HttpObject **object, int id, SceUInt32 type, SceBool remove);
int _sceHttpGetObjectByIndex(HttpObject **object, int index, SceUInt32 type);
int _sceHttpTakeFirstObject(HttpObject **object, SceUInt32 type, int *id);
int _sceHttpRequestReleaseEx(HttpRequest *req, SceBool *freed);
int _sceHttpTakeObject(HttpObject **object, int id, int type);
int _sceHttpGetObjectType(int id);
int _sceHttpGetObjectList(HttpObject **list, int *num, SceUInt32 type);
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
void _sceHttpGetSwVersion(char *version);

/* the library's pool */
void *_sceHttpDefaultMalloc(SceSize size);
void _sceHttpDefaultFree(void *ptr);

/* sockets */
int _sceHttpGetScheme(const char *scheme, SceBool *https);
int _sceHttpSocketSetSendTimeout(int sock, const SceNetSockaddrIn *addr, SceUInt32 timeout,
				 int *saved);
int _sceHttpSocketRestoreSendTimeout(int sock, int saved);
int _sceHttpSocketConnect(int sock, const SceNetSockaddrIn *addr, SceUInt32 timeout, int arg);
void *_sceHttpResolverAllocate(SceSize size, SceNetId rid, const char *name, void *user);
void _sceHttpResolverFree(void *ptr, SceNetId rid, const char *name, void *user);
int _sceHttpRequestEpollRemove(HttpRequest *req, SceUInt32 kind);
int _sceHttpResolverGetError(SceNetId rid);
int _sceHttpRequestResolverDestroy(HttpRequest *req);
int _sceHttpRequestEpollAdd(HttpRequest *req, SceUInt32 kind, SceUInt32 events);
int _sceHttpRequestResolve(HttpRequest *req);
int _sceHttpRequestResolverAbort(HttpRequest *req);
int _sceHttpConnectionClose(HttpConnection *conn);
int _sceHttpConnectionOpenSocket(HttpConnection *conn);
int _sceHttpRequestIcmOpen(HttpRequest *req);
int _sceHttpRequestIcmAbort(HttpRequest *req);
int _sceHttpRequestIcmClose(HttpRequest *req);
int _sceHttpTemplateGetProxyConfig(HttpRequest *req, HttpTemplate *tmpl);
int _sceHttpEpollFind(SceHttpEpollHandle eh, int id, HttpEpollEntry **entry);
int _sceHttpEpollWait(SceHttpEpollHandle eh, SceNetEpollEvent *events, int maxevents, int timeout,
		      SceBool cb);
int _sceHttpEpollAbort(SceHttpEpollHandle eh);
int _sceHttpEpollModify(void *object, SceUInt32 kind, SceUInt32 events);
void _sceHttpUriNormalize(const HttpAllocator *allocator, char *url);
int _sceHttpSchemeGetType(const char *scheme);
SceBool _sceHttpStrNotEmpty(const char *s);
char *_sceHttpRequestGetUrl(HttpRequest *req);
int _sceHttpRequestCreate(HttpConnection *conn, int method, const char *path, const char *query,
			  const char *fragment, const char *username, const char *password,
			  SceULong64 contentLength);
int _sceHttpRequestSend(HttpRequest *req, HttpConnection *conn, const void *msg, SceSize len,
			int flags, SceBool nonblock, SceBool noDelay);
int _sceHttpRequestCompleteEx(HttpRequest *req, SceBool close, SceBool end, SceBool main,
			      SceBool failed);
int _sceHttpRequestComplete(HttpRequest *req, SceBool close, SceBool end);
int _sceHttpRequestProxyConnect(HttpRequest *req, int sock);
int _sceHttpsConnect(HttpRequest *req, int sock);
int _sceHttpsHandshake(HttpRequest *req);
SceBool _sceHttpStrHasLineBreak(const char *s);
int _sceHttpRequestReadBody(HttpRequest *req, char *buf, SceSize len, SceBool *closed,
			    SceBool *end);

/* redirect.c */
int _sceHttpRequestCheckRedirect(HttpRequest *req);
int _sceHttpRequestRedirect(HttpRequest *req, const char *location);
int _sceHttpRequestFollowRedirect(HttpRequest *req);
int _sceHttpRequestRedirectFromCache(HttpRequest *req);
HttpRedirectCacheEntry *_sceHttpRedirectCacheSearch(const char *url, int method, const void *key);
int _sceHttpRedirectCacheLookupRequest(HttpRequest *req, HttpRedirectCacheEntry **entry);
int _sceHttpRedirectCacheLookup(const char *url, int method, const void *key,
				HttpRedirectCacheEntry **entry);
int _sceHttpRedirectCacheAdd(const char *url, int method, const void *key, const char *location);
int _sceHttpRedirectCacheRelease(HttpRedirectCacheEntry *entry);
int _sceHttpTemplateSetProxyServer(HttpTemplate *tmpl, const char *server, SceSize len);
/* SceNetInternal's: an address from its text form; an ICM socket started */
int SceNetInternal_916EA798(int af, const char *src, void *dst);
int SceNetInternal_93F2FF08(SceNetId s, int flags);
int _sceHttpRequestRecvBuffered(HttpRequest *req, char *buf, SceSize len, int flags, SceBool peek,
				SceBool direct);
int _sceHttpRecvDirect(HttpRequest *req, void *buf, SceSize len, int flags);
int _sceHttpRecv(HttpRequest *req, void *buf, SceSize len, int flags);
int _sceHttpRequestReadChunked(HttpRequest *req, char *buf, SceSize len, int flags, SceBool *end,
			       SceSize *chunkSize);
int _sceHttpSocketSetOptions(int sock, const HttpTemplate *tmpl);
int _sceHttpSocketPeek(int sock, void *buf, SceSize len);
int _sceHttpConnectionSend(HttpConnection *conn, const void *msg, SceSize len, int flags,
			   SceBool nonblock, SceBool noDelay);
int _sceHttpRequestSkipLine(HttpRequest *req, int flags);
void _sceHttpsClose(HttpConnection *conn);
int _sceHttpsRecv(HttpConnection *conn, void *buf, SceSize len);
int _sceHttpsSend(HttpConnection *conn, const void *buf, SceSize len);
#define HTTP_SO_PRIVATE_FLAGS	0x40000		/* a socket option no SDK header names */
int _sceHttpPeek(HttpRequest *req, void *buf, SceSize len, int flags);
SceBool _sceHttpConnectionIsConnected(HttpConnection *conn);
int _sceHttpConnectionShutdown(HttpConnection *conn);

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

/* http.c: what the exports call */
#define HTTP_METHOD_EXTRA	8		/* a method of its own name */
#define HTTPS_FLAGS_ALL		0x3f		/* the SCE_HTTPS_FLAG_* */
#define HTTPS_FLAGS_PRIVATE	0x1ff		/* and the system's own */
#define HTTPS_FLAGS_SERVER_CHECKS	(SCE_HTTPS_FLAG_SERVER_VERIFY | SCE_HTTPS_FLAG_CN_CHECK | \
					 SCE_HTTPS_FLAG_KNOWN_CA_CHECK)
#define HTTPS_FLAGS_SET		0x80000000	/* a template's flags were set */
#define HTTPS_ERROR_DISABLE_OPTION_NOT_ALLOWED	-2143072149	/* 0x8043506B */
#define HTTP_REGMGR_SSL_VERIFY_DISABLE	0x563bfe	/* the system parameter DisableOption needs */
int _sceHttpCreateLock(void);
int _sceHttpDeleteLock(void);
int _sceHttpPoolCreate(SceSize size);
int _sceHttpPoolDestroy(void);
typedef int (*HttpPoolStatsFunc)(SceSize *poolSize, SceSize *maxInuseSize, SceSize *currentInuseSize);
int _sceHttpPoolGetStats(HttpPoolStatsFunc func, SceHttpMemoryPoolStats *stats);
int _sceHttpDefaultGetStats(SceSize *poolSize, SceSize *maxInuseSize, SceSize *currentInuseSize);
void *_sceHttpDefaultRealloc(void *ptr, SceSize size);
void *_sceHttpRealloc(void *(*func)(void *ptr, SceSize size), void *ptr, SceSize size);
int _sceHttpFreeList(HttpAllocation *list);
void *_sceHttpZalloc(void *opaque, unsigned int items, unsigned int size);
void _sceHttpZfree(void *opaque, void *ptr);
int _sceHttpConnectionAbort(HttpConnection *conn);
struct z_stream_s;
int inflateEnd(struct z_stream_s *strm);
int _sceHttpObjectInit(void);
int _sceHttpObjectTerm(void);
int _sceHttpRedirectCacheInit(void);
int _sceHttpRedirectCacheTerm(void);
int SceHttp_E0825488(void *(*mallocFunc)(SceSize size), void (*freeFunc)(void *ptr));
int SceHttp_11C9BCE8(void);
void _sceHttpsInit(void);
int _sceHttpCreateTemplate(HttpTemplate *tmpl, const char *userAgent, int httpVer,
			   SceBool autoProxyConf);
int _sceHttpDeleteTemplate(int tmplId);
int _sceHttpDeleteConnection(int connId);
int _sceHttpCreateRequestWithURL(int connId, int method, const char *url,
				 SceULong64 contentLength);
int _sceHttpCreateRequest(int connId, int method, const char *path, SceULong64 contentLength);
int _sceHttpDeleteRequest(int reqId);
int _sceHttpAbortRequest(int reqId, SceBool force);
int _sceHttpSendRequest(int reqId, const void *postData, SceSize size);
int _sceHttpReadData(int reqId, void *data, SceSize size);
int _sceHttpGetLastErrno(int reqId, int *errNum);
int _sceHttpRequestSetMethod(int reqId, const char *method);
int _sceHttpRequestWaitResponse(HttpRequest *req);
int _sceHttpRemoveHeaderField(HttpRequest *req, const char *header, SceSize headerSize,
			      const char *name, SceSize nameLen, char **out, SceSize *outSize);
int _sceHttpSetOption(int id, int option, const void *value, SceSize size, void *arg);
int _sceHttpGetOption(int id, int option, void *value, SceSize size, void *arg, SceSize argSize);
int _sceHttpSetDefaultOption(int option, const void *value, SceSize size, void *arg,
			     SceSize argSize);
int _sceHttpWaitRequest(SceHttpEpollHandle eh, SceHttpNBEvent *nbev, int maxevents, int timeout,
			SceBool cb);
int _sceHttpAbortWaitRequest(SceHttpEpollHandle eh);
int _sceHttpCreateEpoll(SceHttpEpollHandle *eh);
int _sceHttpEpollAdd(SceHttpEpollHandle eh, int id, void *userArg);
int _sceHttpEpollRemove(SceHttpEpollHandle eh, int id);
int _sceHttpDestroyEpoll(SceHttpEpollHandle eh);
int _sceHttpRequestAbortNonblock(HttpRequest *req);
int _sceHttpRedirectCacheFlush(void);
int _sceHttpsLoadCert(int caCertNum, const SceHttpsData **caList, const SceHttpsData *cert,
		      const SceHttpsData *privKey);
int _sceHttpsUnloadCert(void);
int _sceHttpsEnableOption(SceUInt32 sslFlags);
int _sceHttpsDisableOption(SceUInt32 sslFlags);
int _sceHttpsSetSslCallback(int id, SceHttpsCallback cbfunc, void *userArg);
int _sceHttpsSetSslVersion(int id, int version, SceBool set);
int _sceHttpsGetSslError(int id, int *errNum, SceUInt32 *detail);
int _sceHttpsGetCaList(SceSslCert ***caCerts, SceInt32 *caNum);
int _sceHttpsFreeCaList(SceSslCert **caCerts, SceInt32 caNum);
int _sceHttpSslIsCtxCreated(int id, SceBool *created);

/* the exports no SDK header declares */
int sceHttpAbortRequestForce(int reqId);
int sceHttpAddRequestHeaderRaw(int id, const char *header, int mode);
int sceHttpSetEpollId(int id, int epollId, void *userArg);
int sceHttpGetEpollId(int id, int *epollId, void **userArg);
int sceHttpsEnableOptionPrivate(int id, SceUInt32 sslFlags);
int sceHttpsDisableOptionPrivate(int id, SceUInt32 sslFlags);
int sceHttpSetIcmOption(int id, int option);
int sceHttpGetIcmOption(int id, int *option);
int sceHttpRequestGetAllHeaders(int reqId, HttpHeaderPair *headers, SceUInt32 *num,
				char *contentLength, SceSize *contentLengthSize);
int sceHttpSslIsCtxCreated(int id, SceBool *created);
int sceHttpSetAcceptEncodingGZIPEnabled(int id, SceBool enable);
int sceHttpGetAcceptEncodingGZIPEnabled(int id, SceBool *enable);
int sceHttpSetDefaultAcceptEncodingGZIPEnabled(SceBool enable);
int SceHttp_43A7CC70(int id, int value);
int SceHttp_175685B8(int id, int version);
int SceHttp_F30073EC(int id, SceUInt8 value);
int SceHttp_30F5D11F(int id, int *value);
int SceHttp_53B722F2(int id, SceBool enable, int mode, const char *server, SceUShort16 port);
int SceHttp_C498765A(int id, int value);
int SceHttp_855D5254(int value);

#endif /* HTTP_INTERNAL_H */
