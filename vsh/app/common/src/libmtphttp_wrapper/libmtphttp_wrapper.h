/*
 * SceLibMtpHttpWrapper: one interface to SceHttp and SceSsl, or to SceLibMtpHttp's copies of
 * them. sceMtpHttpWrapperLoadStartModules() picks the set by its type argument, and every
 * other function forwards to the chosen set's entry. The error codes are ours.
 */
#ifndef LIBMTPHTTP_WRAPPER_H
#define LIBMTPHTTP_WRAPPER_H

#include <scetypes.h>
#include <libhttp.h>
#include <libssl.h>

#define SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED		(-2141952768)	/* 0x80546500 */
#define SCE_MTP_HTTP_WRAPPER_ERROR_ALREADY_STARTED	(-2141952767)	/* 0x80546501 */
#define SCE_MTP_HTTP_WRAPPER_ERROR_INVALID_TYPE		(-2141952766)	/* 0x80546502 */
#define SCE_MTP_HTTP_WRAPPER_ERROR_LOAD_FAILED		(-2141952765)	/* 0x80546503 */

/* the sets of functions the wrapper forwards to */
#define SCE_MTP_HTTP_WRAPPER_TYPE_HTTP			1	/* SceHttp */
#define SCE_MTP_HTTP_WRAPPER_TYPE_HTTPS			2	/* SceHttp and SceSsl */
#define SCE_MTP_HTTP_WRAPPER_TYPE_MTP_HTTP		3	/* SceLibMtpHttp's SceHttp */
#define SCE_MTP_HTTP_WRAPPER_TYPE_MTP_HTTPS		4	/* SceLibMtpHttp's SceHttp and SceSsl */

#ifdef __cplusplus
extern "C" {
#endif

SceInt32 sceMtpHttpWrapperLoadStartModules(SceUInt16 type);
SceInt32 sceMtpHttpWrapperStopUnloadModules(void);

SceInt32 sceMtpSslWrapperInit(SceSize poolSize);
SceInt32 sceMtpSslWrapperTerm(void);
SceInt32 sceMtpSslWrapperGetMemoryPoolStats(SceSslMemoryPoolStats *currentStat);

SceInt32 sceMtpHttpWrapperInit(SceSize poolSize);
SceInt32 sceMtpHttpWrapperTerm(void);
SceInt32 sceMtpHttpWrapperGetMemoryPoolStats(SceHttpMemoryPoolStats *currentStat);
SceInt32 sceMtpHttpWrapperCreateTemplate(const char *userAgent, SceInt32 httpVer, SceBool autoProxyConf);
SceInt32 sceMtpHttpWrapperDeleteTemplate(SceInt32 tmplId);
SceInt32 sceMtpHttpWrapperCreateConnectionWithURL(SceInt32 tmplId, const char *url, SceBool enableKeepalive);
SceInt32 sceMtpHttpWrapperDeleteConnection(SceInt32 connId);
SceInt32 sceMtpHttpWrapperCreateRequestWithURL(SceInt32 connId, SceInt32 method, const char *url,
					       SceULong64 contentLength);
SceInt32 sceMtpHttpWrapperDeleteRequest(SceInt32 reqId);
SceInt32 sceMtpHttpWrapperSendRequest(SceInt32 reqId, const void *postData, SceSize size);
SceInt32 sceMtpHttpWrapperAbortRequest(SceInt32 reqId);
SceInt32 sceMtpHttpWrapperGetResponseContentLength(SceInt32 reqId, SceULong64 *contentLength);
SceInt32 sceMtpHttpWrapperGetStatusCode(SceInt32 reqId, SceInt32 *statusCode);
SceInt32 sceMtpHttpWrapperReadData(SceInt32 reqId, void *data, SceSize size);
SceInt32 sceMtpHttpWrapperAddRequestHeader(SceInt32 id, const char *name, const char *value, SceUInt32 mode);
SceInt32 sceMtpHttpWrapperRemoveRequestHeader(SceInt32 id, const char *name);
SceInt32 sceMtpHttpWrapperSetAuthInfoCallback(SceInt32 id, SceHttpAuthInfoCallback cbfunc, void *userArg);
SceInt32 sceMtpHttpWrapperSetConnectTimeOut(SceInt32 id, SceUInt32 usec);
SceInt32 sceMtpHttpWrapperSetSendTimeOut(SceInt32 id, SceUInt32 usec);
SceInt32 sceMtpHttpWrapperSetRecvTimeOut(SceInt32 id, SceUInt32 usec);
SceInt32 sceMtpHttpsWrapperEnableOptionPrivate(SceInt32 id, SceUInt32 sslFlags);
SceInt32 sceMtpHttpsWrapperDisableOptionPrivate(SceInt32 id, SceUInt32 sslFlags);
SceInt32 sceMtpHttpWrapperSetProxy(SceInt32 id, SceBool enable, SceInt32 mode, const char *server, SceUInt16 port);

#ifdef __cplusplus
}
#endif

#endif /* LIBMTPHTTP_WRAPPER_H */
