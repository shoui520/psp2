#include <libhttp.h>
#include <http/http.h>
#include <std/new>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/stdio.h>
extern "C" {
#include "../common/main_thread_call.h"
#include "../paf_types.h"
}

/* The request's SSL options (Sony's names unknown; vitasdk: sceHttpsEnableOptionPrivate,
   sceHttpsDisableOptionPrivate). */
extern "C" int SceHttp_43A7CC70(int id, unsigned int ssl_flags);
extern "C" int SceHttp_F30073EC(int id, unsigned int ssl_flags);

using namespace paf;
using namespace paf::http;

static const struct {
	const char *name;
	int32_t method;
} s_method_table[] = {
	{ "GET", SCE_HTTP_METHOD_GET },
	{ "POST", SCE_HTTP_METHOD_POST },
	{ "HEAD", SCE_HTTP_METHOD_HEAD },
	{ "OPTIONS", SCE_HTTP_METHOD_OPTIONS },
	{ "PUT", SCE_HTTP_METHOD_PUT },
	{ "DELETE", SCE_HTTP_METHOD_DELETE },
	{ "TRACE", SCE_HTTP_METHOD_TRACE },
	{ "CONNECT", SCE_HTTP_METHOD_CONNECT },
};

/* paf's SSL option bits 1-3 as libhttp's 0-2. */
static inline uint32_t ToSslFlags(uint32_t flags)
{
	uint32_t ssl_flags = (flags & 2) != 0;

	if (flags & 4)
		ssl_flags |= 2;
	if (flags & 8)
		ssl_flags |= 4;
	return ssl_flags;
}

Client::Client(const Param &param) : m_param(param)
{
	m_template_id = -1;
	m_template_id = sceHttpCreateTemplate(param.m_user_agent.size() != 0 ?
					      param.m_user_agent.c_str() : "",
					      SCE_HTTP_VERSION_1_1, SCE_TRUE);
}

Client::~Client()
{
	if (m_template_id > 0)
		sceHttpDeleteTemplate(m_template_id);
}

Transaction::Transaction(Client &client)
	: m_client(&client), m_abort_call(NULL), m_connection_id(-1), m_request_id(-1),
	  m_ssl_error_mask(0x3f)
{
}

int32_t Transaction::Destroy()
{
	if (m_abort_call != NULL) {
		delete MainThreadCallDestroy(m_abort_call);
		m_abort_call = NULL;
	}
	if (m_request_id > 0) {
		sceHttpDeleteRequest(m_request_id);
		m_request_id = -1;
	}
	if (m_connection_id > 0) {
		sceHttpDeleteConnection(m_connection_id);
		m_connection_id = -1;
	}
	m_url.clear();
	return 0;
}

Transaction::~Transaction()
{
	Destroy();
}

extern "C" int32_t ScePafMisc_5BF60DE9(Transaction *self, uint32_t flags)
{
	return SceHttp_F30073EC(self->m_request_id, ToSslFlags(flags));
}

extern "C" int32_t ScePafMisc_2D83836A(Transaction *self, const char *url, const char *method,
				       uint64_t content_length, StateCheck *check)
{
	int32_t connection_id;
	int32_t ret;

	self->Destroy();
	connection_id = sceHttpCreateConnectionWithURL(self->m_client->m_template_id, url, SCE_TRUE);
	self->m_connection_id = connection_id;
	if (connection_id < 0)
		return connection_id;

	int32_t method_id = SCE_HTTP_METHOD_GET;

	for (uint32_t i = 0; i < sizeof(s_method_table) / sizeof(s_method_table[0]); i++) {
		if (sce_paf_strcmp(method, s_method_table[i].name) == 0) {
			method_id = s_method_table[i].method;
			break;
		}
	}
	ret = sceHttpCreateRequestWithURL(connection_id, method_id, url, content_length);
	self->m_request_id = ret;
	if (ret < 0)
		return ret;
	if (self->m_client->m_param.m_user_name.size() != 0 &&
	    self->m_client->m_param.m_password.size() != 0) {
		sceHttpSetAuthEnabled(ret, SCE_TRUE);
		sceHttpSetAuthInfoCallback(self->m_request_id,
					   (SceHttpAuthInfoCallback)Transaction::AuthInfoCallback,
					   self);
	}
	sceHttpsSetSslCallback(self->m_connection_id,
			       (SceHttpsCallback)Transaction::SslCallback, self);
	ret = SceHttp_F30073EC(self->m_request_id,
			       ToSslFlags(self->m_client->m_param.m_ssl_flags));
	if (ret < 0)
		return ret;

	const Client::Param &param = self->m_client->m_param;

	if (param.m_resolve_timeout != 0) {
		sceHttpSetResolveTimeOut(self->m_connection_id, param.m_resolve_timeout);
		sceHttpSetResolveRetry(self->m_connection_id, param.m_resolve_retry);
	} else {
		sceHttpSetResolveTimeOut(self->m_connection_id, 4000000);
		sceHttpSetResolveRetry(self->m_connection_id, 2);
	}
	if (param.m_connect_timeout != 0)
		sceHttpSetConnectTimeOut(self->m_request_id, param.m_connect_timeout * 1000000);
	else
		sceHttpSetConnectTimeOut(self->m_request_id, 30000000);
	if (param.m_send_timeout != 0)
		sceHttpSetSendTimeOut(self->m_request_id, param.m_send_timeout * 1000000);
	else
		sceHttpSetSendTimeOut(self->m_request_id, 30000000);
	if (param.m_recv_timeout != 0)
		sceHttpSetRecvTimeOut(self->m_request_id, param.m_recv_timeout * 1000000);
	else
		sceHttpSetRecvTimeOut(self->m_request_id, 30000000);
	if (check != NULL) {
		MainThreadCall *call = (MainThreadCall *)operator new(sizeof(MainThreadCall));

		self->m_abort_call = call != NULL ?
			MainThreadCallConstruct(call, check, (Function)ScePafMisc_7EA133E0, self) :
			NULL;
	}
	self->m_url.assign(url);
	return 0;
}

extern "C" int32_t ScePafMisc_A86A5D0E(Transaction *self, const char *url, const char *method,
				       StateCheck *check)
{
	return ScePafMisc_2D83836A(self, url, method, 0, check);
}

int32_t Transaction::SetRequestHeader(const char *name, const char *value)
{
	int32_t ret = sceHttpAddRequestHeader(m_request_id, name, value, SCE_HTTP_HEADER_OVERWRITE);

	if (ret < 0)
		return ret;
	return 0;
}

int32_t Transaction::GetResponseHeader(const char *name, string &value)
{
	char *header;
	SceSize header_size;
	const char *field_value;
	SceSize value_len;
	int32_t ret = sceHttpGetAllResponseHeaders(m_request_id, &header, &header_size);

	if (ret >= 0) {
		ret = sceHttpParseResponseHeader(header, header_size, name, &field_value,
						 &value_len);
		if (ret >= 0) {
			value.assign(field_value, value_len);
			ret = 0;
		}
	}
	return ret;
}

int32_t Transaction::SetAutoRedirect(bool enable)
{
	return sceHttpSetAutoRedirect(m_request_id, enable ? SCE_TRUE : SCE_FALSE);
}

int32_t Transaction::GetAutoRedirect(bool &enable)
{
	SceBool enabled = SCE_FALSE;
	int32_t ret = sceHttpGetAutoRedirect(m_request_id, &enabled);

	if (ret == 0 && enabled == SCE_TRUE)
		enable = true;
	return ret;
}

int32_t Transaction::GetStatusCode(int &status_code)
{
	return sceHttpGetStatusCode(m_request_id, &status_code);
}

int32_t Transaction::GetContentLength(uint64_t &content_length)
{
	return sceHttpGetResponseContentLength(m_request_id, &content_length);
}

int32_t Transaction::GetURL(string &url)
{
	url = m_url;
	return 0;
}

/*
 * The first (last) character of str that differs from any one character of
 * set: with more than one character in set, that is the first (last) one.
 */
static inline size_t FindFirstNotOf(const string &str, const string &set)
{
	for (size_t i = 0; i < str.size(); i++) {
		char ch = str.at(i);

		for (size_t j = 0; j < set.size(); j++) {
			if (ch != set.at(j))
				return i;
		}
	}
	return string::npos;
}

static inline size_t FindLastNotOf(const string &str, const string &set)
{
	if (str.size() != 0) {
		for (int i = str.size() - 1; i >= 0; i--) {
			char ch = str.at(i);

			for (size_t j = 0; j < set.size(); j++) {
				if (ch != set.at(j))
					return i;
			}
		}
	}
	return string::npos;
}

/* The first character of str that is one of set's. */
static inline size_t FindFirstOf(const string &str, const string &set)
{
	for (size_t i = 0; i < str.size(); i++) {
		char ch = str.at(i);

		for (size_t j = 0; j < set.size(); j++) {
			if (ch == set.at(j))
				return i;
		}
	}
	return string::npos;
}

/*
 * A name for a file from a server's: the part after the last '/', trimmed of
 * ". " ahead and " " behind (see FindFirstNotOf), with ":?*><|\\\"" replaced
 * by '_'.
 */
inline void ToFileName(string &name)
{
	string file(name.c_str());

	if (file.size() == 0)
		return;

	size_t pos = file.rfind('/');

	if (pos != string::npos)
		file.erase(0, pos + 1);
	pos = FindFirstNotOf(file, string(". "));
	if (pos == string::npos)
		file.clear();
	else
		file.erase(0, pos);
	pos = FindLastNotOf(file, string(" "));
	if (pos == string::npos)
		file.clear();
	else
		file.erase(pos + 1, string::npos);
	for (;;) {
		pos = FindFirstOf(file, string(":?*><|\\\""));
		if (pos == string::npos)
			break;
		file.at(pos) = '_';
	}
	name.assign(file.c_str());
}

extern "C" int32_t ScePafMisc_CA7A4BB2(const char *url, string &name)
{
	SceSize require;
	SceHttpUriElement element;
	int32_t ret;

	name.clear();

	common::SharedPtr<Buffer> pool;

	ret = sceHttpUriParse(NULL, url, NULL, &require, 0);
	if (ret >= 0) {
		pool = MallocBuffer::Allocate(require);
		if (pool.get() == NULL)
			return SCE_PAF_ERROR_HTTP_NOT_FOUND;
		ret = sceHttpUriParse(&element, url, pool->GetAddr(), NULL, pool->GetSize());
	}
	if (ret < 0)
		return ret;
	name.assign(element.path);
	ToFileName(name);
	if (name.size() != 0)
		return 0;
	return SCE_PAF_ERROR_HTTP_NOT_FOUND;
}

int32_t Transaction::GetFileName(string &name)
{
	char *header;
	SceSize header_size;
	const char *field_value;
	SceSize value_len;

	name.clear();
	{
		string value;

		if (sceHttpGetAllResponseHeaders(m_request_id, &header, &header_size) >= 0 &&
		    sceHttpParseResponseHeader(header, header_size, "Content-Disposition", &field_value,
					       &value_len) >= 0)
			value.assign(field_value, value_len);

		const char *found = sce_paf_private_strtok(value.c_str(), "filename=");

		if (found != NULL) {
			const char *begin = found + 9;
			const char *end;

			if (*begin == '"') {
				begin++;
				end = sce_paf_strchr(begin, '"');
				if (end != NULL)
					name.assign(begin, end - begin);
			} else {
				end = sce_paf_strchr(begin, ';');
				if (end != NULL)
					name.assign(begin, end - begin);
				else
					name.assign(begin);
			}
			ToFileName(name);
		}
		if (name.size() != 0)
			return 0;
	}

	string url(m_url);

	return ScePafMisc_CA7A4BB2(url.c_str(), name);
}

int32_t Transaction::GetRealm(string &realm)
{
	char *header;
	SceSize header_size;
	const char *field_value;
	SceSize value_len;

	realm.clear();

	string value;

	if (sceHttpGetAllResponseHeaders(m_request_id, &header, &header_size) >= 0 &&
	    sceHttpParseResponseHeader(header, header_size, "WWW-Authenticate", &field_value,
				       &value_len) >= 0)
		value.assign(field_value, value_len);

	const char *found = sce_paf_private_strtok(value.c_str(), "realm=");

	if (found != NULL) {
		const char *begin = found + 6;
		const char *end;

		if (*begin == '"') {
			begin++;
			end = sce_paf_strchr(begin, '"');
			if (end != NULL)
				realm.assign(begin, end - begin);
		} else {
			end = sce_paf_strchr(begin, ';');
			if (end != NULL)
				realm.assign(begin, end - begin);
			else
				realm.assign(begin);
		}
		ToFileName(realm);
	}
	if (realm.size() != 0)
		return 0;
	return SCE_PAF_ERROR_HTTP_NOT_FOUND;
}

int32_t Transaction::GetMimeType(string &mime_type)
{
	char *header;
	SceSize header_size;
	const char *field_value;
	SceSize value_len;
	int32_t ret;

	mime_type.clear();

	string value;

	ret = sceHttpGetAllResponseHeaders(m_request_id, &header, &header_size);
	if (ret >= 0) {
		ret = sceHttpParseResponseHeader(header, header_size, "Content-Type", &field_value,
						 &value_len);
		if (ret >= 0)
			value.assign(field_value, value_len);
	}
	if (ret < 0)
		return ret;

	size_t pos = value.find_first_of("; ");

	if (pos != string::npos)
		value.erase(pos, string::npos);
	if (value.find_first_of('/') == string::npos)
		return SCE_PAF_ERROR_HTTP_NOT_FOUND;
	mime_type.swap(value);
	return 0;
}

int32_t Transaction::SendRequest(const void *data, size_t size)
{
	return sceHttpSendRequest(m_request_id, data, size);
}

int32_t Transaction::ReceiveResponse(common::SharedPtr<Buffer> &buffer, uint32_t *size)
{
	int32_t request_id = m_request_id;
	void *addr = buffer->GetAddr();
	int32_t ret = sceHttpReadData(request_id, addr, buffer->GetSize());

	if (ret < 0)
		return ret;
	if (size != NULL)
		*size = ret;
	return 0;
}

extern "C" int32_t ScePafMisc_A7E7AF41(Transaction *self)
{
	return SCE_PAF_ERROR_HTTP_NOT_FOUND;
}

int32_t Transaction::DumpResponseHeaders()
{
	char *header;
	SceSize header_size;
	int32_t ret = sceHttpGetAllResponseHeaders(m_request_id, &header, &header_size);

	if (ret < 0)
		return ret;

	string headers(header, header_size);

	sce_paf_printf_dummy();
	return 0;
}

extern "C" int32_t ScePafMisc_23F9BC0A(Transaction *self, uint32_t flags)
{
	return SceHttp_43A7CC70(self->m_client->m_template_id, flags);
}

int32_t Transaction::SetResponseHeaderMaxSize(uint32_t size)
{
	return sceHttpSetResponseHeaderMaxSize(m_client->m_template_id, size);
}

extern "C" int32_t ScePafMisc_7EA133E0(Transaction *self)
{
	return sceHttpAbortRequest(self->m_request_id);
}

int32_t Transaction::AuthInfoCallback(int32_t request, int32_t auth_type, const char *realm,
				      char *user_name, char *password, int32_t need_entity,
				      unsigned char **entity_body, uint32_t *entity_size,
				      int32_t *save, void *arg)
{
	Transaction *self = (Transaction *)arg;

	if (self == NULL)
		return -1;
	if (auth_type != SCE_HTTP_AUTH_BASIC)
		return -1;

	const Client::Param &param = self->m_client->m_param;

	if (param.m_user_name.size() == 0 || param.m_password.size() == 0)
		return -1;
	sce_paf_snprintf(user_name, 255, "%s", param.m_user_name.c_str());
	sce_paf_snprintf(password, 255, "%s", param.m_password.c_str());
	*save = SCE_FALSE;
	return 0;
}

int32_t Transaction::SslCallback(uint32_t verify_error, void *const cert[], int cert_num,
				 void *arg)
{
	Transaction *self = (Transaction *)arg;

	if (self == NULL)
		return SCE_PAF_ERROR_HTTP_SSL_REJECTED;
	if (verify_error & self->m_ssl_error_mask)
		return SCE_PAF_ERROR_HTTP_SSL_REJECTED;
	return 0;
}
