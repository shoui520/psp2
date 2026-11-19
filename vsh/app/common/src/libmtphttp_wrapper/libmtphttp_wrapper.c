/*
 * SceLibMtpHttpWrapper: the HTTP and SSL calls of the MTP responder go through one table of
 * functions, chosen when the modules are loaded: SceHttp and SceSsl themselves (SceSysmodule
 * loads them), or SceLibMtpHttp's copies of them (loaded from vs0). Every export forwards to
 * the chosen table's entry and reports "not started" when no table is chosen.
 *
 * The error prints carry the line number of the original's source: each #line below sets it
 * to the line the original's message reports.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/modulemgr.h>
#include <libsysmodule.h>
#include <libhttp.h>
#include <libssl.h>
#include "libmtphttp_wrapper.h"
#include "../libmtphttp/libmtphttp.h"

extern int sceClibPrintf(const char *fmt, ...);

/* SceHttp's private exports, not in the SDK's header (the last one is sceHttpSetProxy's counterpart) */
extern SceInt32 sceHttpsEnableOptionPrivate(SceInt32 id, SceUInt32 sslFlags);
extern SceInt32 sceHttpsDisableOptionPrivate(SceInt32 id, SceUInt32 sslFlags);
extern SceInt32 SceHttp_53B722F2(SceInt32 id, SceBool enable, SceInt32 mode, const char *server, SceUInt16 port);

#define MTP_HTTP_WRAPPER_ERR(reason)	\
	sceClibPrintf("[MTPHTTPWRAPPER %s(%d)err] " reason "\n", __FUNCTION__, __LINE__)

typedef struct SslFunctions {
	SceInt32 (*init)(SceSize poolSize);
	SceInt32 (*term)(void);
	SceInt32 (*getMemoryPoolStats)(SceSslMemoryPoolStats *currentStat);
} SslFunctions;

typedef struct HttpFunctions {
	SceInt32 (*init)(SceSize poolSize);
	SceInt32 (*term)(void);
	SceInt32 (*getMemoryPoolStats)(SceHttpMemoryPoolStats *currentStat);
	SceInt32 (*createTemplate)(const char *userAgent, SceInt32 httpVer, SceBool autoProxyConf);
	SceInt32 (*deleteTemplate)(SceInt32 tmplId);
	SceInt32 (*createConnectionWithURL)(SceInt32 tmplId, const char *url, SceBool enableKeepalive);
	SceInt32 (*deleteConnection)(SceInt32 connId);
	SceInt32 (*createRequestWithURL)(SceInt32 connId, SceInt32 method, const char *url, SceULong64 contentLength);
	SceInt32 (*deleteRequest)(SceInt32 reqId);
	SceInt32 (*sendRequest)(SceInt32 reqId, const void *postData, SceSize size);
	SceInt32 (*abortRequest)(SceInt32 reqId);
	SceInt32 (*getResponseContentLength)(SceInt32 reqId, SceULong64 *contentLength);
	SceInt32 (*getStatusCode)(SceInt32 reqId, SceInt32 *statusCode);
	SceInt32 (*readData)(SceInt32 reqId, void *data, SceSize size);
	SceInt32 (*addRequestHeader)(SceInt32 id, const char *name, const char *value, SceUInt32 mode);
	SceInt32 (*removeRequestHeader)(SceInt32 id, const char *name);
	SceInt32 (*setAuthInfoCallback)(SceInt32 id, SceHttpAuthInfoCallback cbfunc, void *userArg);
	SceInt32 (*setConnectTimeOut)(SceInt32 id, SceUInt32 usec);
	SceInt32 (*setSendTimeOut)(SceInt32 id, SceUInt32 usec);
	SceInt32 (*setRecvTimeOut)(SceInt32 id, SceUInt32 usec);
	SceInt32 (*enableOptionPrivate)(SceInt32 id, SceUInt32 sslFlags);
	SceInt32 (*disableOptionPrivate)(SceInt32 id, SceUInt32 sslFlags);
	SceInt32 (*setProxy)(SceInt32 id, SceBool enable, SceInt32 mode, const char *server, SceUInt16 port);
} HttpFunctions;

typedef struct Wrapper {
	const SslFunctions *pSsl;
	const HttpFunctions *pHttp;
} Wrapper;

static const SslFunctions sslFunctions = {
	sceSslInit,
	sceSslTerm,
	sceSslGetMemoryPoolStats,
};

static const HttpFunctions httpFunctions = {
	sceHttpInit,
	sceHttpTerm,
	sceHttpGetMemoryPoolStats,
	sceHttpCreateTemplate,
	sceHttpDeleteTemplate,
	sceHttpCreateConnectionWithURL,
	sceHttpDeleteConnection,
	sceHttpCreateRequestWithURL,
	sceHttpDeleteRequest,
	sceHttpSendRequest,
	sceHttpAbortRequest,
	sceHttpGetResponseContentLength,
	sceHttpGetStatusCode,
	sceHttpReadData,
	sceHttpAddRequestHeader,
	sceHttpRemoveRequestHeader,
	sceHttpSetAuthInfoCallback,
	sceHttpSetConnectTimeOut,
	sceHttpSetSendTimeOut,
	sceHttpSetRecvTimeOut,
	sceHttpsEnableOptionPrivate,
	sceHttpsDisableOptionPrivate,
	SceHttp_53B722F2,
};

static const SslFunctions mtpSslFunctions = {
	SceLibMtpHttp_D063DA59,
	SceLibMtpHttp_D35D8424,
	SceLibMtpHttp_3E17312F,
};

static const HttpFunctions mtpHttpFunctions = {
	SceLibMtpHttp_D82F7256,
	SceLibMtpHttp_DF9A48DA,
	SceLibMtpHttp_5B31E373,
	SceLibMtpHttp_505331F6,
	SceLibMtpHttp_314CE923,
	SceLibMtpHttp_20FB4AF7,
	SceLibMtpHttp_54F6A581,
	SceLibMtpHttp_6DB1F55B,
	SceLibMtpHttp_72684CF8,
	SceLibMtpHttp_D2904E5A,
	SceLibMtpHttp_1D6F97E9,
	SceLibMtpHttp_7BA117AA,
	SceLibMtpHttp_E6895A35,
	SceLibMtpHttp_601596DC,
	SceLibMtpHttp_0E6897BC,
	SceLibMtpHttp_6A8BF9D5,
	SceLibMtpHttp_94B3A908,
	SceLibMtpHttp_F42C62FF,
	SceLibMtpHttp_CE45213C,
	SceLibMtpHttp_76947F49,
	SceLibMtpHttp_04A1183A,
	SceLibMtpHttp_A6A60DCF,
	SceLibMtpHttp_8DF9F8BF,
};

static Wrapper wrapper;
static SceInt32 wrapperType;
static SceUID wrapperModuleId = -1;

int module_start(SceSize argSize, const void *argp)
{
	return 0;
}

int module_stop(SceSize argSize, const void *argp)
{
	return 0;
}

SceInt32 sceMtpHttpWrapperLoadStartModules(SceUInt16 type)
{
	SceInt32 status = 0;
	SceUID modId;

	if (wrapper.pSsl != NULL || wrapper.pHttp != NULL) {
#line 152
		MTP_HTTP_WRAPPER_ERR("modules are already started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_ALREADY_STARTED;
	}
	switch (type) {
	case SCE_MTP_HTTP_WRAPPER_TYPE_HTTP:
		if (sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP) < 0) {
#line 160
			MTP_HTTP_WRAPPER_ERR("failed to load http modules");
			return SCE_MTP_HTTP_WRAPPER_ERROR_LOAD_FAILED;
		}
		wrapper.pHttp = &httpFunctions;
		break;
	case SCE_MTP_HTTP_WRAPPER_TYPE_HTTPS:
		if (sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS) < 0) {
#line 169
			MTP_HTTP_WRAPPER_ERR("failed to load https modules");
			return SCE_MTP_HTTP_WRAPPER_ERROR_LOAD_FAILED;
		}
		wrapper.pSsl = &sslFunctions;
		wrapper.pHttp = &httpFunctions;
		break;
	case SCE_MTP_HTTP_WRAPPER_TYPE_MTP_HTTP:
		modId = sceKernelLoadStartModule("vs0:vsh/common/libmtphttp.suprx", 0, NULL, 0, NULL, &status);
		if (modId < 0) {
#line 179
			MTP_HTTP_WRAPPER_ERR("failed to load mtp http modules");
			return SCE_MTP_HTTP_WRAPPER_ERROR_LOAD_FAILED;
		}
		SceLibMtpHttp_8D80C874();
		wrapperModuleId = modId;
		wrapper.pHttp = &mtpHttpFunctions;
		break;
	case SCE_MTP_HTTP_WRAPPER_TYPE_MTP_HTTPS:
		modId = sceKernelLoadStartModule("vs0:vsh/common/libmtphttp.suprx", 0, NULL, 0, NULL, &status);
		if (modId < 0) {
#line 192
			MTP_HTTP_WRAPPER_ERR("failed to load https modules");
			return SCE_MTP_HTTP_WRAPPER_ERROR_LOAD_FAILED;
		}
		SceLibMtpHttp_8D80C874();
		wrapperModuleId = modId;
		wrapper.pSsl = &mtpSslFunctions;
		wrapper.pHttp = &mtpHttpFunctions;
		break;
	default:
		return SCE_MTP_HTTP_WRAPPER_ERROR_INVALID_TYPE;
	}
	wrapperType = type;
	return 0;
}

SceInt32 sceMtpHttpWrapperStopUnloadModules(void)
{
	SceInt32 result = 0;
	SceInt32 status;

	if (wrapper.pSsl == NULL && wrapper.pHttp == NULL) {
#line 218
		MTP_HTTP_WRAPPER_ERR("modules are not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	if (wrapperType == SCE_MTP_HTTP_WRAPPER_TYPE_HTTP) {
		result = sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
	} else if (wrapperType == SCE_MTP_HTTP_WRAPPER_TYPE_HTTPS) {
		result = sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);
	} else if (wrapperType == SCE_MTP_HTTP_WRAPPER_TYPE_MTP_HTTP || wrapperType == SCE_MTP_HTTP_WRAPPER_TYPE_MTP_HTTPS) {
		status = 0;
		SceLibMtpHttp_D92810E0();
		result = sceKernelStopUnloadModule(wrapperModuleId, 0, NULL, 0, NULL, &status);
	}
	wrapper.pSsl = NULL;
	wrapper.pHttp = NULL;
	wrapperType = 0;
	wrapperModuleId = -1;
	return result;
}

SceInt32 sceMtpSslWrapperInit(SceSize poolSize)
{
	if (wrapper.pSsl == NULL) {
#line 246
		MTP_HTTP_WRAPPER_ERR("module for ssl is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pSsl->init(poolSize);
}

SceInt32 sceMtpSslWrapperTerm(void)
{
	if (wrapper.pSsl == NULL) {
#line 256
		MTP_HTTP_WRAPPER_ERR("module for ssl is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pSsl->term();
}

SceInt32 sceMtpSslWrapperGetMemoryPoolStats(SceSslMemoryPoolStats *currentStat)
{
	if (wrapper.pSsl == NULL) {
#line 267
		MTP_HTTP_WRAPPER_ERR("module for ssl is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pSsl->getMemoryPoolStats(currentStat);
}

SceInt32 sceMtpHttpWrapperInit(SceSize poolSize)
{
	if (wrapper.pHttp == NULL) {
#line 277
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->init(poolSize);
}

SceInt32 sceMtpHttpWrapperTerm(void)
{
	if (wrapper.pHttp == NULL) {
#line 288
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->term();
}

SceInt32 sceMtpHttpWrapperGetMemoryPoolStats(SceHttpMemoryPoolStats *currentStat)
{
	if (wrapper.pHttp == NULL) {
#line 299
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->getMemoryPoolStats(currentStat);
}

SceInt32 sceMtpHttpWrapperCreateTemplate(const char *userAgent, SceInt32 httpVer, SceBool autoProxyConf)
{
	if (wrapper.pHttp == NULL) {
#line 310
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->createTemplate(userAgent, httpVer, autoProxyConf);
}

SceInt32 sceMtpHttpWrapperDeleteTemplate(SceInt32 tmplId)
{
	if (wrapper.pHttp == NULL) {
#line 321
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->deleteTemplate(tmplId);
}

SceInt32 sceMtpHttpWrapperCreateConnectionWithURL(SceInt32 tmplId, const char *url, SceBool enableKeepalive)
{
	if (wrapper.pHttp == NULL) {
#line 332
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->createConnectionWithURL(tmplId, url, enableKeepalive);
}

SceInt32 sceMtpHttpWrapperDeleteConnection(SceInt32 connId)
{
	if (wrapper.pHttp == NULL) {
#line 343
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->deleteConnection(connId);
}

SceInt32 sceMtpHttpWrapperCreateRequestWithURL(SceInt32 connId, SceInt32 method, const char *url, SceULong64 contentLength)
{
	if (wrapper.pHttp == NULL) {
#line 354
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->createRequestWithURL(connId, method, url, contentLength);
}

SceInt32 sceMtpHttpWrapperDeleteRequest(SceInt32 reqId)
{
	if (wrapper.pHttp == NULL) {
#line 365
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->deleteRequest(reqId);
}

SceInt32 sceMtpHttpWrapperSendRequest(SceInt32 reqId, const void *postData, SceSize size)
{
	if (wrapper.pHttp == NULL) {
#line 376
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->sendRequest(reqId, postData, size);
}

SceInt32 sceMtpHttpWrapperAbortRequest(SceInt32 reqId)
{
	if (wrapper.pHttp == NULL) {
#line 387
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->abortRequest(reqId);
}

SceInt32 sceMtpHttpWrapperGetResponseContentLength(SceInt32 reqId, SceULong64 *contentLength)
{
	if (wrapper.pHttp == NULL) {
#line 398
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->getResponseContentLength(reqId, contentLength);
}

SceInt32 sceMtpHttpWrapperGetStatusCode(SceInt32 reqId, SceInt32 *statusCode)
{
	if (wrapper.pHttp == NULL) {
#line 409
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->getStatusCode(reqId, statusCode);
}

SceInt32 sceMtpHttpWrapperReadData(SceInt32 reqId, void *data, SceSize size)
{
	if (wrapper.pHttp == NULL) {
#line 420
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->readData(reqId, data, size);
}

SceInt32 sceMtpHttpWrapperAddRequestHeader(SceInt32 id, const char *name, const char *value, SceUInt32 mode)
{
	if (wrapper.pHttp == NULL) {
#line 431
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->addRequestHeader(id, name, value, mode);
}

SceInt32 sceMtpHttpWrapperRemoveRequestHeader(SceInt32 id, const char *name)
{
	if (wrapper.pHttp == NULL) {
#line 442
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->removeRequestHeader(id, name);
}

SceInt32 sceMtpHttpWrapperSetAuthInfoCallback(SceInt32 id, SceHttpAuthInfoCallback cbfunc, void *userArg)
{
	if (wrapper.pHttp == NULL) {
#line 453
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->setAuthInfoCallback(id, cbfunc, userArg);
}

SceInt32 sceMtpHttpWrapperSetConnectTimeOut(SceInt32 id, SceUInt32 usec)
{
	if (wrapper.pHttp == NULL) {
#line 464
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->setConnectTimeOut(id, usec);
}

SceInt32 sceMtpHttpWrapperSetSendTimeOut(SceInt32 id, SceUInt32 usec)
{
	if (wrapper.pHttp == NULL) {
#line 475
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->setSendTimeOut(id, usec);
}

SceInt32 sceMtpHttpWrapperSetRecvTimeOut(SceInt32 id, SceUInt32 usec)
{
	if (wrapper.pHttp == NULL) {
#line 486
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->setRecvTimeOut(id, usec);
}

SceInt32 sceMtpHttpsWrapperEnableOptionPrivate(SceInt32 id, SceUInt32 sslFlags)
{
	if (wrapper.pHttp == NULL) {
#line 498
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->enableOptionPrivate(id, sslFlags);
}

SceInt32 sceMtpHttpsWrapperDisableOptionPrivate(SceInt32 id, SceUInt32 sslFlags)
{
	if (wrapper.pHttp == NULL) {
#line 509
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->disableOptionPrivate(id, sslFlags);
}

SceInt32 sceMtpHttpWrapperSetProxy(SceInt32 id, SceBool enable, SceInt32 mode, const char *server, SceUInt16 port)
{
	if (wrapper.pHttp == NULL) {
#line 520
		MTP_HTTP_WRAPPER_ERR("module for http is not started");
		return SCE_MTP_HTTP_WRAPPER_ERROR_NOT_STARTED;
	}
	return wrapper.pHttp->setProxy(id, enable, mode, server, port);
}
