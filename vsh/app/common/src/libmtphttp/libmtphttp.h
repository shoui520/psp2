/*
 * SceLibMtpHttp: the HTTP and SSL client the MTP responder runs over (vs0:vsh/common/
 * libmtphttp.suprx). Its exports mirror SceHttp's and SceSsl's (the wrapper keeps both sets in
 * parallel tables), so each takes its counterpart's arguments. No evidence names them: the
 * link names are the NIDs'.
 */
#ifndef LIBMTPHTTP_H
#define LIBMTPHTTP_H

#include <scetypes.h>
#include <libhttp.h>
#include <libssl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the SceSsl counterparts */
SceInt32 SceLibMtpHttp_D063DA59(SceSize poolSize);
SceInt32 SceLibMtpHttp_D35D8424(void);
SceInt32 SceLibMtpHttp_3E17312F(SceSslMemoryPoolStats *currentStat);

/* the SceHttp counterparts */
SceInt32 SceLibMtpHttp_D82F7256(SceSize poolSize);
SceInt32 SceLibMtpHttp_DF9A48DA(void);
SceInt32 SceLibMtpHttp_5B31E373(SceHttpMemoryPoolStats *currentStat);
SceInt32 SceLibMtpHttp_505331F6(const char *userAgent, SceInt32 httpVer, SceBool autoProxyConf);
SceInt32 SceLibMtpHttp_314CE923(SceInt32 tmplId);
SceInt32 SceLibMtpHttp_20FB4AF7(SceInt32 tmplId, const char *url, SceBool enableKeepalive);
SceInt32 SceLibMtpHttp_54F6A581(SceInt32 connId);
SceInt32 SceLibMtpHttp_6DB1F55B(SceInt32 connId, SceInt32 method, const char *url, SceULong64 contentLength);
SceInt32 SceLibMtpHttp_72684CF8(SceInt32 reqId);
SceInt32 SceLibMtpHttp_D2904E5A(SceInt32 reqId, const void *postData, SceSize size);
SceInt32 SceLibMtpHttp_1D6F97E9(SceInt32 reqId);
SceInt32 SceLibMtpHttp_7BA117AA(SceInt32 reqId, SceULong64 *contentLength);
SceInt32 SceLibMtpHttp_E6895A35(SceInt32 reqId, SceInt32 *statusCode);
SceInt32 SceLibMtpHttp_601596DC(SceInt32 reqId, void *data, SceSize size);
SceInt32 SceLibMtpHttp_0E6897BC(SceInt32 id, const char *name, const char *value, SceUInt32 mode);
SceInt32 SceLibMtpHttp_6A8BF9D5(SceInt32 id, const char *name);
SceInt32 SceLibMtpHttp_94B3A908(SceInt32 id, SceHttpAuthInfoCallback cbfunc, void *userArg);
SceInt32 SceLibMtpHttp_F42C62FF(SceInt32 id, SceUInt32 usec);
SceInt32 SceLibMtpHttp_CE45213C(SceInt32 id, SceUInt32 usec);
SceInt32 SceLibMtpHttp_76947F49(SceInt32 id, SceUInt32 usec);
SceInt32 SceLibMtpHttp_04A1183A(SceInt32 id, SceUInt32 sslFlags);
SceInt32 SceLibMtpHttp_A6A60DCF(SceInt32 id, SceUInt32 sslFlags);
SceInt32 SceLibMtpHttp_8DF9F8BF(SceInt32 id, SceBool enable, SceInt32 mode, const char *server, SceUInt16 port);

/* called right after the module is loaded, and right before it is unloaded */
SceInt32 SceLibMtpHttp_8D80C874(void);
SceInt32 SceLibMtpHttp_D92810E0(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBMTPHTTP_H */
