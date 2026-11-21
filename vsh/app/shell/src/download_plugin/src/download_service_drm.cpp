/*
 * download_service_drm.cpp: the download service's requests and their DRM steps
 * (0x8113AC58-0x8113C278): an auto download's or a title's request handed to the next file,
 * the account server's download DRM form (start, finish and set: an HTTPS form post, built
 * like np_kdc's), the replies to a client's asynchronous methods and the two method
 * dispatchers of the "SceDownloadService" IPMI server. The file's name and every name in it
 * are ours (PLACEHOLDER): no record or message names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <rtc.h>
#include <vshbridge/vshbridge.h>
#include <regmgr/regmgr.h>
#include <libhttp.h>
#include <libhttp/https.h>
#include <libhttp/libhttp_url.h>
#include <ipmi/server.h>
#include <ipmi/session.h>

#include "../../esecron/include/esebase.hpp"
#include "../../esecron/include/esetask.hpp"
#include "download_plugin.h"
#include "download_service.h"

using namespace paf;

extern "C" {
int sceNpCommonGetNpEnviroment(char *environment);
int SceNpManager_DDF30B0E(char *loginId, void *password, void *reserved);
/* libhttp exports no SDK header declares */
int sceHttpsEnableOptionPrivate(int id, SceUInt32 sslFlags);
int sceHttpsDisableOptionPrivate(int id, SceUInt32 sslFlags);
}

/* the DRM form's errors (PLACEHOLDER names, np_kdc's) */
#define DRM_ERROR_NO_MEMORY		((int)0x80104F01)
#define DRM_ERROR_INVALID_ARG		((int)0x80104F02)
#define DRM_ERROR_BUFFER_TOO_SMALL	((int)0x80104F03)
#define DRM_ERROR_BUSY			((int)0x80104F04)
#define DRM_ERROR_BAD_RESPONSE		((int)0x80104F05)
#define DRM_ERROR_BAD_REASON		((int)0x80104F06)
#define DRM_ERROR_REFUSED		((int)0x80104F08)
#define DRM_ERROR_BAD_STATUS_CODE	((int)0x80104F09)
#define DRM_ERROR_SSL_UNTRUSTED		((int)0x80104F23)
#define DRM_ERROR_SSL_EXPIRED		((int)0x80104F24)
#define DRM_ERROR_SSL_NAME		((int)0x80104F25)
#define DRM_ERROR_SSL_UNKNOWN_CA	((int)0x80104F26)
#define DRM_ERROR_SSL_BAD_CERT		((int)0x80104F27)
#define DRM_ERROR_NOT_STARTED		((int)0x80104F28)
#define DRM_ERROR_SERVER_BASE		((int)0x80105000)

/* the methods' ids */
#define DOWNLOAD_SVC_METHOD_ASYNC_REPLY		0x12340022
#define DOWNLOAD_SVC_METHOD_TITLE_REPLY		0x12340024
#define DOWNLOAD_SVC_METHOD_CANCEL_REPLY	0x12340030
#define IPMI_ERROR_UNKNOWN_METHOD		((int)0x800205C1)	/* PLACEHOLDER name */

/* the form's fields */
enum {
	DRM_PARAM_LOGINID,
	DRM_PARAM_PASSWORD,
	DRM_PARAM_EPASSWORD,
	DRM_PARAM_CONSOLEID,
	DRM_PARAM_PRODUCTID,
	DRM_PARAM_PLATFORM,
	DRM_PARAM_C1,
	DRM_PARAM_ENABLEFLAG,
	DRM_NUM_PARAMS
};

/* the form's three requests */
enum {
	DRM_TYPE_START,
	DRM_TYPE_FINISH,
	DRM_TYPE_SET
};

struct DrmParamDesc {
	SceSize length;
	const char *pName;
};

struct DrmHeaderField {
	const char *pName;
	const char *pValue;
};

/* one field of the form: the name, then the value to escape */
struct DrmParam {
	DrmParam *pNext;
	const DrmParamDesc *pDesc;
	SceSize size;
	char value[1];
};

/* the form's fields, with the ones present */
struct DrmParamList {
	DrmParam *pHead;
	DrmParam *pTail;
	SceUInt32 mask;
};

/* the request: its connection, the form and the server's answer */
struct DrmRequest {
	SceInt32 connectionId;
	SceInt32 requestId;
	DrmParamList params;
	SceUInt32 flags;		/* PLACEHOLDER: only cleared */
	char *pBody;
	SceSize bodySize;
	SceUInt32 reserved[2];		/* PLACEHOLDER: only cleared */
	int reason;
	int refused;
};

/* what the console key is made from (PLACEHOLDER) */
struct DrmKeyRequest {
	SceUInt8 type;
	SceUInt8 reserved[31];
};

/* another file's (0x81425E88, PLACEHOLDER name): the console key, from the secure kernel */
int Bgdl_81425E88(const DrmKeyRequest *request, const SceRtcTick *tick, SceUInt8 *key,
		  char *out);

static const DrmParamDesc s_paramDescs[DRM_NUM_PARAMS] = {
	{ 8, "loginid=" },
	{ 10, "&password=" },
	{ 11, "&epassword=" },
	{ 11, "&consoleid=" },
	{ 11, "&productid=" },
	{ 10, "&platform=" },
	{ 4, "&c1=" },
	{ 12, "&enableflag=" },
};

static const DrmHeaderField s_requestHeaders[3] = {
	{ "Content-Type", "application/x-www-form-urlencoded" },
	{ "User-Agent", "My heart leaps up when I behold A rainbow in the sky" },
	{ "X-I-5-DL-Version", "1.0" },
};

static const DrmHeaderField s_versionHeader[1] = {
	{ "X-I-5-DL-Version", "1.0" },
};

static const DrmHeaderField s_contentHeader[1] = {
	{ "Content-Type", "application/x-i-5-drm" },
};

static DrmRequest *s_request;		/* (0x815448C0) the finish or set request under way */
static SceInt32 s_template;		/* (0x815448C4) */

DownloadServiceRequest::~DownloadServiceRequest()
{
}

paf::string *DownloadServiceRequest::GetString1(void)
{
	return &m_string1;
}

const SceUInt64 *DownloadServiceRequest::GetSize(void)
{
	return &m_size;
}

const SceUInt64 *DownloadServiceRequest::GetValue(void)
{
	return &m_value;
}

/* the id kept, when it fits */
void DownloadServiceRequest::SetId(const char *id)
{
	if (id != NULL && sce_paf_strlen(id) <= 48)
		m_id.assign(id, sce_paf_strlen(id));
	else
		m_id.clear();
}

void DownloadServiceRequest::SetKind(int kind)
{
	m_kind = kind;
}

void DownloadServiceRequest::SetUrl(const char *url)
{
	if (url != NULL && sce_paf_strlen(url) <= 256)
		m_url.assign(url, sce_paf_strlen(url));
	else
		m_url.clear();
}

void DownloadServiceRequest::SetName(const char *name)
{
	if (name != NULL && sce_paf_strlen(name) <= 512)
		m_name.assign(name, sce_paf_strlen(name));
	else
		m_name.clear();
}

void DownloadServiceRequest::SetSize(const SceUInt64 *size)
{
	m_size = *size;
}

void DownloadServiceRequest::SetValue(const SceUInt64 *value)
{
	m_value = *value;
}

int DownloadServiceRequest::Clear(void)
{
	m_id.clear();
	m_string1.clear();
	m_url.clear();
	m_iconUrl.clear();
	m_name.clear();
	m_string5.clear();
	m_size = 0;
	m_value = 0;
	m_flag = false;
	m_kind = 0;
	m_reserved = 0;
	return 0;
}

DownloadServiceRequest::DownloadServiceRequest(const char *id, int kind, const SceUInt64 *size,
					       const char *url, const SceUInt64 *value,
					       const char *name)
{
	m_id.clear();
	m_string1.clear();
	m_url.clear();
	m_iconUrl.clear();
	m_name.clear();
	m_string5.clear();
	m_kind = 0;
	m_reserved = 0;
	m_flag = false;
	m_size = 0;
	m_value = 0;
	SetId(id);
	m_kind = kind;
	m_size = *size;
	SetUrl(url);
	m_value = *value;
	SetName(name);
}

DownloadServiceRequest::DownloadServiceRequest(const char *id)
{
	m_id.clear();
	m_string1.clear();
	m_url.clear();
	m_iconUrl.clear();
	m_name.clear();
	m_string5.clear();
	m_kind = 0;
	m_reserved = 0;
	m_size = 0;
	m_value = 0;
	m_flag = false;
	SetId(id);
}

DownloadServiceRequest::DownloadServiceRequest()
{
	m_id.clear();
	m_string1.clear();
	m_url.clear();
	m_iconUrl.clear();
	m_name.clear();
	m_string5.clear();
	m_kind = 0;
	m_reserved = 0;
	m_size = 0;
	m_value = 0;
	m_flag = false;
}

int Bgdl_8113B08A(void)
{
	return 32;
}

SceSize Bgdl_8113B090(const IPMI::BufferInfo *info)
{
	return info->outSize;
}

/* an auto download: the setting turned on, the DRM start sent and the request queued */
int DownloadServiceRequest::Bgdl_8113B096(void)
{
	PodlCacheLock lock;
	int ret;

	ret = lock.Lock();
	if (ret >= 0) {
		int value = 0;

		ret = sceRegMgrGetKeyInt("/CONFIG/NP2", "auto_download", &value);
		if (ret >= 0
		    && (value == 1
			|| (ret = sceRegMgrSetKeyInt("/CONFIG/NP2", "auto_download", 1)) >= 0)
		    && (ret = Bgdl_8113E22C(m_id.c_str())) >= 0) {
			int err = Bgdl_8113C5B2(this, true);

			if (err >= 0)
				return 0;
			return err;
		}
	}
	return ret;
}

/* a title's request: its DRM set and the request queued */
int DownloadServiceRequest::Bgdl_8113B164(void)
{
	PodlCacheLock lock;
	int ret;

	ret = lock.Lock();
	if (ret >= 0 && (ret = Bgdl_8113E288(m_id.c_str())) >= 0) {
		int err = Bgdl_8113C9B4(this);

		if (err >= 0)
			return 0;
		return err;
	}
	return ret;
}

/* the account server's address for the environment the account is in */
__attribute__((noinline)) static int DrmGetUrl(int type, char **ppUrl)
{
	char environment[16];
	const char *pFormat;
	SceSize length;
	int ret;

	ret = sceNpCommonGetNpEnviroment(environment);
	if (ret < 0)
		return ret;
	if (type > 0) {
		if (type <= DRM_TYPE_FINISH) {
			pFormat = "https://account.%s.ac.playstation.net/ecomm/ingame/finishDownloadDRM";
		} else {
			if (type > DRM_TYPE_SET)
				return DRM_ERROR_INVALID_ARG;
			pFormat = "https://account.%s.ac.playstation.net/ecomm/ingame/setDownloadDRM";
		}
	} else {
		if (type < 0)
			return DRM_ERROR_INVALID_ARG;
		pFormat = "https://account.%s.ac.playstation.net/ecomm/ingame/startDownloadDRM";
	}
	length = sce_paf_strlen(pFormat);
	*ppUrl = (char *)sce_paf_malloc(length + 17);
	if (*ppUrl == NULL)
		return DRM_ERROR_NO_MEMORY;
	sce_paf_memset(*ppUrl, 0, length + 17);
	sce_paf_snprintf(*ppUrl, length + 16, pFormat, environment);
	return 0;
}

static int DrmCreateTemplate(void);
static int DrmDeleteTemplate(void);
static int DrmCreateRequest(DrmRequest **ppRequest, const char *pUrl);
static int DrmDeleteRequest(DrmRequest *pRequest);
static int DrmAddLoginId(DrmRequest *pRequest, const char *pLoginId);
static int DrmAddProductId(DrmRequest *pRequest, const char *pProductId);
static int DrmAddConsoleKey(DrmRequest *pRequest, const SceUInt8 *pKey);
static int DrmSend(DrmRequest *pRequest, const char *pUrl);
static int DrmAbort(DrmRequest *pRequest);
static int DrmAddEnableFlag(DrmRequest *pRequest, int flag);

/* a request of the type: its URL, connection, login and the console's key */
__attribute__((noinline)) static int DrmOpenRequest(int type, char **ppUrl, DrmRequest **ppRequest)
{
	SceRtcTick tick;
	SceUInt8 reserved[8];
	DrmKeyRequest keyRequest;
	SceUInt8 password[36];
	char out[64];
	char loginId[68];
	SceUInt8 consoleKey[128];
	int ret;

	sce_paf_memset(&tick, 0, sizeof(tick));
	sce_paf_memset(out, 0, sizeof(out));
	sce_paf_memset(&keyRequest, 0, sizeof(keyRequest));
	sce_paf_memset(consoleKey, 0, sizeof(consoleKey));
	ret = DrmGetUrl(type, ppUrl);
	if (ret < 0)
		return ret;
	ret = SceNpManager_DDF30B0E(loginId, password, reserved);
	if (ret < 0)
		return ret;
	if (type > 0) {
		if (type <= DRM_TYPE_FINISH) {
			keyRequest.type = 0x11;
		} else {
			if (type > DRM_TYPE_SET)
				return DRM_ERROR_INVALID_ARG;
			keyRequest.type = 0;
		}
	} else {
		if (type < 0)
			return DRM_ERROR_INVALID_ARG;
		keyRequest.type = 0x10;
	}
	ret = vshRtcGetCurrentSecureTick(&tick);
	if (ret == 0)
		ret = Bgdl_81425E88(&keyRequest, &tick, consoleKey, out);
	if (ret != 0) {
		if (ret >= 0)
			return ret;
	} else {
		ret = DrmCreateTemplate();
		if (ret < 0)
			return ret;
		ret = DrmCreateRequest(ppRequest, *ppUrl);
		if (ret >= 0
		    && (ret = DrmAddConsoleKey(*ppRequest, consoleKey)) >= 0
		    && (ret = DrmAddLoginId(*ppRequest, loginId)) >= 0)
			return 0;
	}
	if (*ppUrl != NULL) {
		sce_paf_free(*ppUrl);
		*ppUrl = NULL;
	}
	DrmDeleteTemplate();
	return ret;
}

/* the DRM finish sent for the product (path is not read) */
int Bgdl_8113B3A0(const char *path, const char *productId)
{
	char *url = NULL;
	int ret;

	ret = DrmOpenRequest(DRM_TYPE_FINISH, &url, &s_request);
	if (ret >= 0
	    && (ret = DrmAddProductId(s_request, productId)) >= 0
	    && (ret = DrmSend(s_request, url)) >= 0)
		ret = 0;
	if (s_request != NULL) {
		DrmDeleteRequest(s_request);
		DrmDeleteTemplate();
		s_request = NULL;
	}
	if (url != NULL)
		sce_paf_free(url);
	return ret;
}

/* the request under way stopped */
int Bgdl_8113B436(void)
{
	if (s_request != NULL)
		return DrmAbort(s_request);
	return DRM_ERROR_NOT_STARTED;
}

/* the DRM set sent for the product, with its flag */
int Bgdl_8113B456(const char *loginId, const char *productId, int flag)
{
	char *url = NULL;
	int ret;

	ret = DrmOpenRequest(DRM_TYPE_SET, &url, &s_request);
	if (ret >= 0
	    && (ret = DrmAddProductId(s_request, productId)) >= 0
	    && (ret = DrmAddEnableFlag(s_request, flag)) >= 0
	    && (ret = DrmSend(s_request, url)) >= 0)
		ret = 0;
	if (s_request != NULL) {
		DrmDeleteRequest(s_request);
		DrmDeleteTemplate();
		s_request = NULL;
	}
	if (url != NULL)
		sce_paf_free(url);
	return ret;
}

/* the form's body: each field's name, then its escaped value, into pOut (or only its size) */
__attribute__((noinline)) static int DrmBuildBody(DrmParamList *pList, char *pOut, SceSize prepare,
						 SceSize *pSize)
{
	const DrmParam *pParam;
	SceSize total = 0;
	SceSize require;
	SceSize used;
	int ret;

	for (pParam = pList->pHead; pParam != NULL; pParam = pParam->pNext) {
		require = pParam->pDesc->length;
		if (pOut != NULL) {
			if (prepare <= require)
				return DRM_ERROR_BUFFER_TOO_SMALL;
			sce_paf_strncpy(pOut, pParam->pDesc->pName, require);
			pOut += require;
			prepare -= require;
		}
		used = total + require;
		ret = sceHttpUriEscape(pOut, &require, prepare, pParam->value);
		total = used + --require;
		if (ret != 0)
			return ret;
		if (pOut != NULL) {
			pOut += require;
			prepare -= require;
		}
	}
	if (pSize != NULL)
		*pSize = total + 1;
	if (pOut != NULL && prepare != 0)
		*pOut = 0;
	return 0;
}

/* a header field of the answer the server must send */
static inline __attribute__((always_inline)) int DrmCheckHeader(const char *pHeader,
								SceSize headerSize,
								const DrmHeaderField *pFields)
{
	const char *pValue;
	SceSize valueSize;
	int ret;
	int i;

	for (i = 0; i < 1; i++) {
		ret = sceHttpParseResponseHeader(pHeader, headerSize, pFields[i].pName, &pValue,
						 &valueSize);
		if (ret < 0)
			return ret;
		if (sce_paf_strncmp(pValue, pFields[i].pValue, sce_paf_strlen(pFields[i].pValue)) != 0)
			return DRM_ERROR_BAD_RESPONSE;
	}
	return 0;
}

/* the form posted and the server's answer checked */
__attribute__((noinline)) static int DrmSendRequest(DrmRequest *pRequest, const char *pUrl)
{
	char *pHeader;
	SceSize headerSize;
	const char *pStatus;
	SceSize statusSize;
	char *pEnd;
	char *pValue;
	SceUInt64 contentLength;
	SceSize bodySize;
	char reasonText[16];
	int statusCode;
	int ret;
	int i;

	if (pRequest->pBody != NULL)
		return DRM_ERROR_BUSY;
	ret = DrmBuildBody(&pRequest->params, NULL, 0, &bodySize);
	if (ret >= 0) {
		pRequest->bodySize = (bodySize > 0x400) ? bodySize : 0x400;
		pRequest->pBody = (char *)sce_paf_malloc(pRequest->bodySize + 1);
		if (pRequest->pBody == NULL)
			return DRM_ERROR_NO_MEMORY;
		sce_paf_memset(pRequest->pBody, 0, pRequest->bodySize + 1);
		ret = DrmBuildBody(&pRequest->params, pRequest->pBody, pRequest->bodySize, &bodySize);
		if (ret >= 0) {
			ret = sceHttpCreateRequestWithURL(pRequest->connectionId, 1, pUrl,
							  (SceUInt64)bodySize - 1);
			if (ret >= 0) {
				pRequest->requestId = ret;
				for (i = 0; i < 3; i++) {
					ret = sceHttpAddRequestHeader(pRequest->requestId,
								      s_requestHeaders[i].pName,
								      s_requestHeaders[i].pValue, 0);
					if (ret < 0)
						goto fail;
				}
				ret = sceHttpSendRequest(pRequest->requestId, pRequest->pBody,
							 bodySize - 1);
				if (ret < 0) {
					if (ret == (int)0x80431075) {
						SceUInt32 detail;
						int errNum;

						ret = sceHttpsGetSslError(pRequest->requestId, &errNum,
									  &detail);
						if (ret >= 0) {
							ret = errNum;
							if (errNum == (int)0x80435060) {
								ret = DRM_ERROR_SSL_UNTRUSTED;
								if ((detail & 4) == 0) {
									ret = DRM_ERROR_SSL_EXPIRED;
									if ((detail & 0x20) == 0) {
										ret = DRM_ERROR_SSL_NAME;
										if ((detail & 2) == 0) {
											ret = DRM_ERROR_SSL_BAD_CERT;
											if ((detail & 1) != 0)
												ret = DRM_ERROR_SSL_UNKNOWN_CA;
										}
									}
								}
							}
						}
					}
					goto check;
				}
				ret = sceHttpGetStatusCode(pRequest->requestId, &statusCode);
				if (ret < 0)
					goto fail;
				if (statusCode != 200) {
					ret = DRM_ERROR_BAD_STATUS_CODE;
					goto fail;
				}
				ret = sceHttpGetAllResponseHeaders(pRequest->requestId, &pHeader,
								   &headerSize);
				if (ret < 0)
					goto fail;
				ret = DrmCheckHeader(pHeader, headerSize, s_versionHeader);
				if (ret < 0)
					goto fail;
				if (sceHttpParseResponseHeader(pHeader, headerSize, "X-I-5-DL-Status",
							       &pStatus, &statusSize) < 0) {
					ret = DRM_ERROR_BAD_RESPONSE;
					goto fail;
				}
				pEnd = sce_paf_private_strtok(pStatus, "\r\n");
				if (sce_paf_strncmp("OK", pStatus, 2) != 0) {
					SceSize length;
					char *pSemicolon;

					pRequest->refused = 1;
					pValue = sce_paf_private_strtok(pStatus, "reason=");
					if (pValue == NULL) {
						ret = DRM_ERROR_BAD_REASON;
						goto parsed;
					}
					pValue += 7;
					pSemicolon = sce_paf_strchr(pValue, ';');
					if (pSemicolon != NULL)
						pEnd = pSemicolon;
					length = pEnd - pValue;
					if (length > 15
					    || (sce_paf_memcpy(reasonText, pValue, length),
						reasonText[length] = 0,
						pRequest->reason = sce_paf_strtol(reasonText, NULL, 16),
						pRequest->reason == (int)0x80000000)
					    || pRequest->reason == 0x7FFFFFFF) {
						ret = DRM_ERROR_BAD_REASON;
						goto parsed;
					}
				} else {
					pRequest->refused = 0;
				}
				ret = 0;
parsed:
				if (ret < 0)
					goto fail;
				if (pRequest->refused == 0) {
					ret = sceHttpGetResponseContentLength(pRequest->requestId,
									      &contentLength);
					if (ret >= 0) {
						if (contentLength == 0)
							return 0;
						ret = DrmCheckHeader(pHeader, headerSize, s_contentHeader);
						if (ret >= 0)
							return 0;
					}
					goto fail;
				}
				if (pRequest->reason > 255) {
					ret = DRM_ERROR_REFUSED;
					goto fail;
				}
				ret = pRequest->reason | DRM_ERROR_SERVER_BASE;
check:
				if (ret >= 0)
					goto release;
			}
		}
fail:
		if (pRequest->requestId > 0) {
			sceHttpDeleteRequest(pRequest->requestId);
			pRequest->requestId = 0;
		}
release:
		if (pRequest->pBody != NULL) {
			sce_paf_free(pRequest->pBody);
			pRequest->pBody = NULL;
			pRequest->bodySize = 0;
		}
	}
	return ret;
}

__attribute__((noinline)) static int DrmCreateTemplate(void)
{
	int ret;
	int err;

	ret = sceHttpCreateTemplate("L'essentiel est invisible pour les yeux", 1, 0);
	s_template = ret;
	if (ret < 0)
		return ret;
	err = sceHttpSetConnectTimeOut(ret, 20000000);
	if (err < 0
	    || (err = sceHttpSetSendTimeOut(s_template, 20000000)) < 0
	    || (err = sceHttpSetRecvTimeOut(s_template, 60000000)) < 0
	    || (err = sceHttpsEnableOptionPrivate(s_template, 229)) < 0
	    || (err = sceHttpsDisableOptionPrivate(s_template, 26)) < 0) {
		if (s_template > 0) {
			sceHttpDeleteTemplate(s_template);
			s_template = -1;
		}
		return err;
	}
	return 0;
}

__attribute__((noinline)) static int DrmDeleteTemplate(void)
{
	int ret;

	if (s_template > 0) {
		ret = sceHttpDeleteTemplate(s_template);
		if (ret < 0)
			return ret;
		s_template = -1;
	}
	return 0;
}

__attribute__((noinline)) static int DrmCreateRequest(DrmRequest **ppRequest, const char *pUrl)
{
	DrmRequest *pRequest;
	int ret;

	pRequest = (DrmRequest *)sce_paf_malloc(sizeof(DrmRequest));
	*ppRequest = pRequest;
	if (pRequest == NULL)
		return DRM_ERROR_NO_MEMORY;
	sce_paf_memset(pRequest, 0, sizeof(DrmRequest));
	pRequest->params.pHead = NULL;
	pRequest->params.pTail = NULL;
	pRequest->params.mask = 0;
	ret = sceHttpCreateConnectionWithURL(s_template, pUrl, 1);
	if (ret >= 0) {
		pRequest->connectionId = ret;
		ret = 0;
		pRequest->pBody = NULL;
	}
	if (ret >= 0)
		return 0;
	return ret;
}

__attribute__((noinline)) static int DrmDeleteRequest(DrmRequest *pRequest)
{
	DrmParam *pParam;

	if (pRequest->requestId > 0) {
		sceHttpDeleteRequest(pRequest->requestId);
		pRequest->requestId = 0;
	}
	sceHttpDeleteConnection(pRequest->connectionId);
	if (pRequest->pBody != NULL) {
		sce_paf_free(pRequest->pBody);
		pRequest->pBody = NULL;
	}
	for (pParam = pRequest->params.pHead; pParam != NULL; pParam = pRequest->params.pHead) {
		pRequest->params.pHead = pParam->pNext;
		sce_paf_free(pParam);
	}
	sce_paf_free(pRequest);
	return 0;
}

/* a field of the form: '&' fields go to the end, loginid to the front */
__attribute__((noinline)) static int DrmAddParam(DrmParamList *pList, unsigned int index,
						const void *pValue, SceSize size)
{
	DrmParam *pParam;

	if (index >= DRM_NUM_PARAMS)
		return DRM_ERROR_INVALID_ARG;
	pParam = (DrmParam *)sce_paf_malloc(size + 13);
	if (pParam == NULL)
		return DRM_ERROR_NO_MEMORY;
	sce_paf_memset(pParam, 0, size + 13);
	pParam->pNext = NULL;
	pParam->size = size;
	pParam->pDesc = &s_paramDescs[index];
	sce_paf_memcpy(pParam->value, pValue, size);
	if (pList->pHead != NULL) {
		if (pParam->pDesc->pName[0] == '&') {
			pList->pTail->pNext = pParam;
			pList->pTail = pParam;
			pParam->pNext = NULL;
		} else {
			pParam->pNext = pList->pHead;
			pList->pHead = pParam;
		}
	} else {
		pList->pHead = pParam;
		pList->pTail = pParam;
	}
	pList->mask |= 1 << index;
	return 0;
}

__attribute__((noinline)) static int DrmAddLoginId(DrmRequest *pRequest, const char *pLoginId)
{
	int err = DrmAddParam(&pRequest->params, DRM_PARAM_LOGINID, pLoginId,
			      sce_paf_strlen(pLoginId));
	int ret = 0;

	if (err < 0)
		return err;
	return ret;
}

__attribute__((noinline)) static int DrmAddProductId(DrmRequest *pRequest, const char *pProductId)
{
	return DrmAddParam(&pRequest->params, DRM_PARAM_PRODUCTID, pProductId,
			   sce_paf_strlen(pProductId));
}

/* the console's 128 byte key in hex */
__attribute__((noinline)) static int DrmAddConsoleKey(DrmRequest *pRequest, const SceUInt8 *pKey)
{
	char hex[257];
	char *pOut;
	int err;
	int ret;
	int i;

	sce_paf_memset(hex, 0, 0x101);
	pOut = hex;
	for (i = 128; i != 0; i--) {
		sce_paf_snprintf(pOut, 3, "%02x", *pKey++);
		pOut += 2;
	}
	err = DrmAddParam(&pRequest->params, DRM_PARAM_C1, hex, 0x100);
	ret = 0;
	if (err < 0)
		ret = err;
	return ret;
}

__attribute__((noinline)) static int DrmSend(DrmRequest *pRequest, const char *pUrl)
{
	return DrmSendRequest(pRequest, pUrl);
}

__attribute__((noinline)) static int DrmAbort(DrmRequest *pRequest)
{
	return sceHttpAbortRequest(pRequest->requestId);
}

__attribute__((noinline)) static int DrmAddEnableFlag(DrmRequest *pRequest, int flag)
{
	char text[10];

	sce_paf_memset(text, 0, sizeof(text));
	sce_paf_snprintf(text, sizeof(text), "%d", flag);
	return DrmAddParam(&pRequest->params, DRM_PARAM_ENABLEFLAG, text, sce_paf_strlen(text));
}

/* an auto download job's reply */
int Bgdl_8113BC9A(IPMI::Session *session, SceUInt32 requestId, int result)
{
	return session->respondToAsyncMethodRequest(DOWNLOAD_SVC_METHOD_ASYNC_REPLY, requestId,
						    result, NULL, 0);
}

/* a title job's reply */
int Bgdl_8113BCBC(IPMI::Session *session, SceUInt32 requestId, int result)
{
	return session->respondToAsyncMethodRequest(DOWNLOAD_SVC_METHOD_TITLE_REPLY, requestId,
						    result, NULL, 0);
}

int Bgdl_8113BCDE(IPMI::Session *session, SceUInt32 requestId, int result)
{
	return session->respondToAsyncMethodRequest(DOWNLOAD_SVC_METHOD_CANCEL_REPLY, requestId,
						    result, NULL, 0);
}

/* the input and output buffers a method has */
#define IN_DATA(n)		(pInList[n].pData)
#define IN_WORD(n, i)		(((const SceUInt32 *)pInList[n].pData)[i])
#define OUT_BUFFER(n)		(pOutList[n].pBuffer)

/* a client's synchronous method run and answered */
int Bgdl_8113BCFE(IPMI::Session *session, SceUInt32 methodId, const IPMI::DataInfo *pInList,
		  SceUInt32 inCount, IPMI::BufferInfo *pOutList, SceUInt32 outCount)
{
	int ret;

	switch (methodId) {
	case 0x12340000:
		ret = Bgdl_81138FE4(session, IN_WORD(0, 0), (const BgdlTaskDetail *)IN_DATA(1));
		break;
	case 0x12340001:
		ret = Bgdl_81138F80(session, IN_WORD(0, 0), (BgdlTaskDetail *)IN_DATA(1),
				    (SceUInt32 *)OUT_BUFFER(0));
		pOutList[0].outSize = sizeof(SceUInt32);
		break;
	case 0x12340002:
		ret = Bgdl_8113903E(session, IN_WORD(0, 0));
		break;
	case 0x12340003:
		ret = Bgdl_8113906E(session, IN_WORD(0, 0), (BgdlTaskState *)OUT_BUFFER(0));
		pOutList[0].outSize = 80;
		break;
	case 0x12340004:
		ret = Bgdl_81139190(session, IN_WORD(0, 0));
		break;
	case 0x12340005:
		ret = Bgdl_811391C0(session, IN_WORD(0, 0));
		break;
	case 0x12340006:
		ret = Bgdl_811391F0(session, *(const bool *)IN_DATA(0));
		break;
	case 0x12340007:
		ret = Bgdl_81139216(session);
		break;
	case 0x12340008:
		ret = Bgdl_81139238(session);
		break;
	case 0x12340009:
		ret = Bgdl_8113925A(session, IN_WORD(0, 0), (BgdlTaskProgress *)OUT_BUFFER(0));
		pOutList[0].outSize = sizeof(BgdlTaskProgress);
		break;
	case 0x1234000A: {
		int *count = (int *)OUT_BUFFER(0);

		ret = Bgdl_81139318(session, IN_WORD(0, 0), IN_WORD(0, 1), count,
				    (BgdlTaskId *)OUT_BUFFER(1));
		pOutList[0].outSize = sizeof(int);
		pOutList[1].outSize = sizeof(BgdlTaskId) * *count;
		break;
	}
	case 0x1234000B: {
		int *count = (int *)OUT_BUFFER(0);

		ret = Bgdl_81139392(session, IN_WORD(0, 0), IN_WORD(0, 1), count,
				    (BgdlTaskId *)OUT_BUFFER(1));
		pOutList[0].outSize = sizeof(int);
		pOutList[1].outSize = sizeof(BgdlTaskId) * *count;
		break;
	}
	case 0x1234000C:
		ret = Bgdl_8113942C(session, IN_WORD(0, 0), IN_WORD(0, 1),
				    (BgdlTaskDetail *)OUT_BUFFER(0));
		pOutList[0].outSize = 3520;
		break;
	case 0x1234000D:
		ret = Bgdl_811394CC(session, (const BgdlServerQuery *)IN_DATA(0),
				    (const BgdlServerIds *)IN_DATA(1), (BgdlContentInfo *)OUT_BUFFER(0));
		pOutList[0].outSize = sizeof(BgdlContentInfo);
		break;
	case 0x1234000E:
		ret = Bgdl_81139536(session, (const SceUInt32 *)IN_DATA(0), (char *)OUT_BUFFER(0));
		pOutList[0].outSize = 256;
		break;
	case 0x1234000F:
		ret = Bgdl_8113961C(session, (const BgdlPackageRequest *)IN_DATA(0),
				    (BgdlPackageResult *)OUT_BUFFER(0));
		pOutList[0].outSize = 16;
		break;
	case 0x12340010:
		ret = Bgdl_81139680(session, IN_WORD(0, 0));
		break;
	case 0x12340011:
		ret = Bgdl_811396EE(session, (const BgdlContentRequest *)IN_DATA(0),
				    (const BgdlContentAccount *)IN_DATA(1),
				    (const BgdlContentInfo *)IN_DATA(2), (SceUInt32 *)OUT_BUFFER(0));
		pOutList[0].outSize = sizeof(SceUInt32);
		break;
	case 0x12340012:
		ret = Bgdl_81139766(session, (const BgdlDownloadRequest *)IN_DATA(0),
				    (BgdlContentInfo *)OUT_BUFFER(0), (SceUInt32 *)OUT_BUFFER(1));
		pOutList[0].outSize = sizeof(BgdlContentInfo);
		pOutList[1].outSize = sizeof(SceUInt32);
		break;
	case 0x12340013:
		ret = Bgdl_81139906(session, IN_WORD(0, 0), (BgdlPackageStatus *)OUT_BUFFER(0));
		pOutList[0].outSize = sizeof(BgdlPackageStatus);
		break;
	case 0x12340014:
		ret = Bgdl_81139AE8(session, IN_WORD(0, 0), IN_WORD(0, 1),
				    (BgdlPackageEntry *)OUT_BUFFER(0));
		pOutList[0].outSize = 296;
		break;
	case 0x12340015:
		ret = Bgdl_81139BD6(session, IN_WORD(0, 0), (const BgdlPackageRequest *)IN_DATA(1));
		break;
	case 0x12340016:
		ret = Bgdl_81139C0C(session, (const BgdlContentQuery *)IN_DATA(0));
		break;
	case 0x12340017:
		ret = Bgdl_81139C60(session, IN_WORD(0, 0), (BgdlDispatcherState *)OUT_BUFFER(0),
				    (int *)OUT_BUFFER(1));
		pOutList[0].outSize = sizeof(BgdlDispatcherState);
		pOutList[1].outSize = sizeof(int);
		break;
	case 0x12340018:
		ret = Bgdl_81139CBE(session, IN_WORD(0, 0));
		break;
	case 0x12340019:
		ret = Bgdl_81139E2A(session, IN_WORD(0, 0));
		break;
	case 0x1234001A:
		ret = Bgdl_81139CDC(session, IN_WORD(0, 0), (BgdlSpaceInfo *)OUT_BUFFER(0));
		pOutList[0].outSize = sizeof(BgdlSpaceInfo);
		break;
	case 0x1234001B:
		ret = Bgdl_81139E52(session, (const BgdlStorageQuery *)IN_DATA(0),
				    (bool *)OUT_BUFFER(0));
		pOutList[0].outSize = sizeof(bool);
		break;
	case 0x1234001C:
		ret = Bgdl_81139E92(session, (const FgdlTitleRequest *)IN_DATA(0),
				    (int)OUT_BUFFER(0));
		pOutList[0].outSize = sizeof(int);
		break;
	case 0x1234001D:
		ret = Bgdl_81139EFE(session, (int)IN_DATA(0), *(const SceInt64 *)IN_DATA(0));
		break;
	case 0x1234001E:
		ret = Bgdl_81139F46(session, IN_DATA(0));
		break;
	case 0x1234001F:
		ret = Bgdl_8113A76C(session, IN_DATA(0));
		break;
	case 0x12340020:
		ret = Bgdl_81139C36(session, (const BgdlContentQuery *)IN_DATA(0));
		break;
	case 0x12340029:
		ret = Bgdl_81139F58(session, IN_WORD(0, 0), OUT_BUFFER(0));
		pOutList[0].outSize = 8;
		break;
	case 0x1234002A:
		ret = Bgdl_81139F58(session, IN_WORD(0, 0), IN_DATA(1));
		break;
	case 0x1234002B:
		ret = Bgdl_81139F7E(session, (int)IN_DATA(0), (SceUInt64 *)OUT_BUFFER(0));
		pOutList[0].outSize = 40;
		break;
	case 0x1234002C:
		ret = Bgdl_81139FFE(session, (const BgdlPatchRequest *)IN_DATA(0));
		break;
	case 0x1234002D:
		ret = Bgdl_8113A0E2(session, (const BgdlMtpRequest *)IN_DATA(0),
				    (SceUInt32 *)OUT_BUFFER(0));
		pOutList[0].outSize = sizeof(SceUInt32);
		break;
	case 0x1234002E:
		ret = Bgdl_81409E2C(session);
		break;
	default:
		return IPMI_ERROR_UNKNOWN_METHOD;
	}
	return session->respondToSyncMethodRequest(ret, pOutList, outCount);
}

/* a client's asynchronous method queued; its job answers it */
int Bgdl_8113C220(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
		  const IPMI::DataInfo *pInList, SceUInt32 inCount)
{
	switch (methodId) {
	case 0x12340021:
		return Bgdl_8113A78C(session, requestId, (const DownloadServiceParam *)IN_DATA(0));
	case 0x12340023:
		return Bgdl_8113A982(session, requestId, (const char *)IN_DATA(0));
	case 0x12340025:
		return Bgdl_81409E2C(session, requestId, *(const bool *)IN_DATA(0));
	case 0x12340027:
		return Bgdl_81409E2C(session, requestId, *(const bool *)IN_DATA(0));
	case 0x1234002F:
		return Bgdl_8113A110(session, requestId);
	}
	return IPMI_ERROR_UNKNOWN_METHOD;
}
