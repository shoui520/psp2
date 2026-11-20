/*
 * The HTTP component: the context's SceHttp template and connection, and its one request
 * (Sony's file: its checks name component/http/component_http.cpp)
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include "../../common/activity_debug.h"
#include "../../common/activity_simple_lock.h"
#include "../../np_activity.h"
#include "../utility/component_utility.h"
#include "component_http.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "component\\http\\component_http.cpp"

/* SceHttp's private SSL options (the SDK does not declare them) */
extern "C" int sceHttpsEnableOptionPrivate(int id, SceUInt32 sslFlags);
extern "C" int sceHttpsDisableOptionPrivate(int id, SceUInt32 sslFlags);
extern "C" int sceHttpSetAcceptEncodingGZIPEnabled(int id, SceBool enable);

/* the template's SSL options: on, as np_utility sets them, and the validity checks off */
#define HTTP_SSL_OPTIONS_ENABLED	0x165
#define HTTP_SSL_OPTIONS_DISABLED	(SCE_HTTPS_FLAG_NOT_AFTER_CHECK | SCE_HTTPS_FLAG_NOT_BEFORE_CHECK)

/* the activity server's digest authentication */
#define HTTP_AUTH_REALM			"c7y-activity"
#define HTTP_AUTH_USERNAME		"c7y-activity01"
#define HTTP_AUTH_PASSWORD_LENGTH	32
#define HTTP_AUTH_PASSWORD_KEY		0x7F

/* the password, each byte XORed with HTTP_AUTH_PASSWORD_KEY */
static const char s_authPassword[HTTP_AUTH_PASSWORD_LENGTH] = {
	0x47, 0x1e, 0x02, 0x4f, 0x35, 0x4d, 0x14, 0x53, 0x49, 0x38, 0x43, 0x3f, 0x01, 0x54, 0x43, 0x09,
	0x18, 0x1a, 0x3b, 0x16, 0x0d, 0x19, 0x11, 0x15, 0x1a, 0x39, 0x4e, 0x27, 0x13, 0x12, 0x12, 0x02,
};

/* SceHttp's abort as the library's */
static inline int convertError(int ret)
{
	if (ret == SCE_HTTP_ERROR_ABORTED)
		ret = NP_ACTIVITY_ERROR_ABORTED;
	return ret;
}

/* a failed SceHttp call's result as the library's: its error converted, else 0 */
static inline int convertResult(int ret)
{
	int result = 0;

	if (ret < 0)
		result = convertError(ret);
	return result;
}

ComponentHttp::ComponentHttp() :
	m_buffer(NULL), m_bufferSize(0), m_protocol(UTILITY_PROTOCOL_NONE),
	m_secureProtocol(UTILITY_PROTOCOL_NONE), m_templateId(0), m_connectionId(0), m_requestId(0),
	m_contentLength(0)
{
}

ComponentHttp::~ComponentHttp()
{
}

int ComponentHttp::SetBuffer(void *buffer, SceSize size)
{
	m_bufferSize = size;
	m_buffer = buffer;
	return 0;
}

int ComponentHttp::GetProtocol()
{
	return m_protocol;
}

static SceInt32 authInfoCallback(SceInt32 request, SceHttpAuthType authType, const char *realm,
	char *username, char *password, SceBool needEntity, SceUChar8 **entityBody,
	SceSize *entitySize, SceBool *save, void *userArg);

int ComponentHttp::Open(int protocol, const char *userAgent, const char *host,
	SceUShort16 port, int keepAlive)
{
	const char *scheme;
	int id;
	int ret;

	if (userAgent == NULL || host == NULL) {
#line 187
		ACTIVITY_CHECK("Open");
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	}

	if (m_templateId == 0) {
		id = sceHttpCreateTemplate(userAgent, SCE_HTTP_VERSION_1_1, SCE_TRUE);
		if (id <= 0)
			return convertResult(id);
		m_templateId = id;
		sceHttpsEnableOptionPrivate(id, HTTP_SSL_OPTIONS_ENABLED);
		sceHttpsDisableOptionPrivate(m_templateId, HTTP_SSL_OPTIONS_DISABLED);
		sceHttpSetAcceptEncodingGZIPEnabled(m_templateId, SCE_FALSE);
	}
	if (m_connectionId != 0) {
		ret = NP_ACTIVITY_ERROR_INVALID_STATE;
		sceHttpDeleteTemplate(m_templateId);
		m_templateId = 0;
		return ret;
	}

	m_protocol = protocol;
	if (protocol != UTILITY_PROTOCOL_HTTP) {
		scheme = "http";
		if (protocol == UTILITY_PROTOCOL_HTTPS)
			scheme = "https";
	} else {
		scheme = "http";
	}
	id = sceHttpCreateConnection(m_templateId, host, scheme, port, keepAlive);
	if (id <= 0) {
		ret = convertResult(id);
		sceHttpDeleteTemplate(m_templateId);
		m_templateId = 0;
		return ret;
	}
	m_connectionId = id;

	ret = sceHttpSetAuthInfoCallback(id, authInfoCallback, NULL);
	if (ret >= 0) {
		if (ret > 0)
			ret = 0;
		return ret;
	}
	ret = convertResult(ret);
	sceHttpDeleteConnection(m_connectionId);
	m_connectionId = 0;
	sceHttpDeleteTemplate(m_templateId);
	m_templateId = 0;
	return ret;
}

int ComponentHttp::Close()
{
	int ret;

	m_protocol = UTILITY_PROTOCOL_NONE;
	m_secureProtocol = UTILITY_PROTOCOL_NONE;
	if (m_connectionId != 0) {
		sceHttpDeleteConnection(m_connectionId);
		m_connectionId = 0;
	}
	if (m_templateId == 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	ret = sceHttpDeleteTemplate(m_templateId);
	m_templateId = 0;
	return convertError(ret);
}

inline int ComponentHttp::getRequestId()
{
	ActivitySimpleLock lock(GetMutex());

	return m_requestId;
}

inline void ComponentHttp::setRequestId(int requestId)
{
	ActivitySimpleLock lock(GetMutex());

	m_requestId = requestId;
}

int ComponentHttp::SendRequest(int method, const char *url, SceSize urlLength,
	const void *postData, SceSize size)
{
	int requestId;
	int statusCode;
	int ret;

	if (m_templateId == 0 || m_connectionId == 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;

	if (method != HTTP_METHOD_GET) {
		if (method != HTTP_METHOD_POST)
			return NP_ACTIVITY_ERROR_INVALID_STATE;
		requestId = sceHttpCreateRequestWithURL(m_connectionId, SCE_HTTP_METHOD_POST, url,
			size);
	} else {
		requestId = sceHttpCreateRequestWithURL(m_connectionId, SCE_HTTP_METHOD_GET, url, 0);
	}
	if (requestId <= 0)
		return convertResult(requestId);
	setRequestId(requestId);

	if (CheckAbort(requestId) == NP_ACTIVITY_ERROR_ABORTED) {
		sceHttpDeleteRequest(requestId);
		setRequestId(0);
		return NP_ACTIVITY_ERROR_ABORTED;
	}

	if (method != HTTP_METHOD_GET)
		ret = sceHttpSendRequest(requestId, postData, size);
	else
		ret = sceHttpSendRequest(requestId, NULL, 0);
	if (ret < 0) {
		ret = convertError(ret);
		sceHttpDeleteRequest(requestId);
		setRequestId(0);
		return ret;
	}
	if (CheckAbort(ret) == NP_ACTIVITY_ERROR_ABORTED) {
		sceHttpDeleteRequest(requestId);
		setRequestId(0);
		return NP_ACTIVITY_ERROR_ABORTED;
	}

	statusCode = 0;
	if (sceHttpGetStatusCode(requestId, &statusCode) < 0) {
		sceHttpDeleteRequest(requestId);
		setRequestId(0);
	}
	m_contentLength = 0;
	if (statusCode != SCE_HTTP_STATUS_CODE_OK) {
		sceHttpDeleteRequest(requestId);
		setRequestId(0);
		return NP_ACTIVITY_ERROR_SERVER_ERROR;
	}

	ret = sceHttpGetResponseContentLength(requestId, &m_contentLength);
	if (ret == SCE_HTTP_ERROR_NO_CONTENT_LENGTH) {
		m_contentLength = 0;
		return 0;
	}
	if (ret >= 0) {
		if (ret > 0)
			ret = 0;
		return ret;
	}
	ret = convertError(ret);
	sceHttpDeleteRequest(requestId);
	setRequestId(0);
	return ret;
}

int ComponentHttp::ReadData(void **data, SceSize *size, int *done)
{
	char *buffer;
	SceSize remaining;
	SceSize total;
	int requestId;
	int complete;
	int read;
	int ret = 0;

	requestId = getRequestId();
	if (requestId <= 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;

	total = 0;
	buffer = (char *)m_buffer;
	complete = 0;
	remaining = m_bufferSize;
	for (;;) {
		ret = CheckAbort(ret);
		if (ret == NP_ACTIVITY_ERROR_ABORTED)
			return ret;
		read = sceHttpReadData(requestId, buffer, remaining);
		if (read > 0)
			total += read;
		if (read < 0)
			return convertError(read);
		if (read == 0) {
			*done = 1;
			complete = 1;
		} else if (total >= m_bufferSize) {
			complete = 1;
		}
		buffer += read;
		remaining -= read;
		if (complete == 1) {
			*data = m_buffer;
			*size = total;
			return CheckAbort(ret);
		}
	}
}

int ComponentHttp::EndRequest()
{
	int requestId;
	int ret;

	requestId = getRequestId();
	if (requestId <= 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	ret = sceHttpDeleteRequest(requestId);
	setRequestId(0);
	if (ret >= 0)
		return ret;
	return convertError(ret);
}

void ComponentHttp::SetAbort()
{
	ActivitySimpleLock lock(GetMutex());

	m_aborted = 1;
	if (m_requestId > 0)
		sceHttpAbortRequest(m_requestId);
}

/* answers the activity server's digest authentication */
static SceInt32 authInfoCallback(SceInt32 request, SceHttpAuthType authType, const char *realm,
	char *username, char *password, SceBool needEntity, SceUChar8 **entityBody,
	SceSize *entitySize, SceBool *save, void *userArg)
{
	int i;

	if (save != NULL)
		*save = SCE_FALSE;
	if (authType != SCE_HTTP_AUTH_DIGEST || realm == NULL)
		return NP_ACTIVITY_ERROR_SERVER_ERROR;
	if (sceClibStrncmp(realm, HTTP_AUTH_REALM,
	    sceClibStrnlen(HTTP_AUTH_REALM, 0x7FFFFFFF) + 1) != 0)
		return NP_ACTIVITY_ERROR_SERVER_ERROR;
	if (username == NULL)
		return NP_ACTIVITY_ERROR_SERVER_ERROR;
	sceClibStrncpy(username, HTTP_AUTH_USERNAME,
		sceClibStrnlen(HTTP_AUTH_USERNAME, 0x7FFFFFFF) + 1);
	if (password == NULL)
		return NP_ACTIVITY_ERROR_SERVER_ERROR;
	for (i = 0; i < HTTP_AUTH_PASSWORD_LENGTH; i++)
		password[i] = s_authPassword[i] ^ HTTP_AUTH_PASSWORD_KEY;
	password[HTTP_AUTH_PASSWORD_LENGTH] = '\0';
	return 0;
}
