#include <libhttp.h>
#include <http/http.h>
#include <file/localfile.h>
#include <kernel/iofilemgr.h>
#include <std/algorithm>
#include <std/ctype.h>
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

/* What url::Encode writes for each byte. */
static const char *const s_encode_table[256] = {
	"%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
	"%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
	"%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
	"%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
	"%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27",
	"%28", "%29", "%2A", "%2B", "%2C", "-", ".", "%2F",
	"0", "1", "2", "3", "4", "5", "6", "7",
	"8", "9", "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
	"%40", "A", "B", "C", "D", "E", "F", "G",
	"H", "I", "J", "K", "L", "M", "N", "O",
	"P", "Q", "R", "S", "T", "U", "V", "W",
	"X", "Y", "Z", "%5B", "%5C", "%5D", "%5E", "_",
	"%60", "a", "b", "c", "d", "e", "f", "g",
	"h", "i", "j", "k", "l", "m", "n", "o",
	"p", "q", "r", "s", "t", "u", "v", "w",
	"x", "y", "z", "%7B", "%7C", "%7D", "~", "%7F",
	"%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
	"%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
	"%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
	"%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
	"%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
	"%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
	"%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
	"%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
	"%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
	"%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
	"%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
	"%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
	"%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
	"%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
	"%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
	"%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF",
};

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

/*
 * Reads the response into file, in reads of up to 16 KiB; fails when the
 * response is longer than max_size or shorter than its Content-Length.
 */
extern "C" int32_t ScePafMisc_EE1CA56C(Transaction *self, common::SharedPtr<File> &file,
				       uint64_t max_size)
{
	if (file.get() == NULL)
		return SCE_PAF_ERROR_HTTP_NOT_FOUND;

	uint64_t content_length = (uint64_t)-1;

	sceHttpGetResponseContentLength(self->m_request_id, &content_length);
	if (max_size != (uint64_t)-1 && content_length != (uint64_t)-1 &&
	    content_length > max_size)
		return SCE_PAF_ERROR_HTTP_NOT_FOUND;

	uint64_t size = 0;
	int32_t ret = 0;
	common::SharedPtr<Buffer> buffer;

	while (size < max_size) {
		const uint64_t max_read = 0x4000;
		uint64_t nbyte = min(max_size - size, max_read);

		if (buffer.get() == NULL || nbyte != buffer->GetSize()) {
			buffer = MallocBuffer::Allocate(nbyte);
			if (buffer.get() == NULL)
				return SCE_PAF_ERROR_HTTP_NOT_FOUND;
		}

		int32_t read = sceHttpReadData(self->m_request_id, buffer->GetAddr(),
					       buffer->GetSize());
		File *dst = file.get();

		if (read < 0)
			return read;
		if (read == 0)
			break;
		size += (uint32_t)read;

		int32_t written = dst->Write(buffer->GetAddr(), read);

		if (written < 0)
			return written;
	}
	if (size == max_size)
		return SCE_PAF_ERROR_HTTP_NOT_FOUND;
	if (content_length != (uint64_t)-1 && content_length != size)
		return SCE_PAF_ERROR_HTTP_NOT_FOUND;
	return ret;
}

/* Downloads url into dir under the server's file name, which name receives. */
extern "C" int32_t ScePafMisc_4E787F5E(const char *url, const char *dir, string *name)
{
	Client client((Client::Param()));
	Transaction transaction(client);
	int32_t error;

	error = ScePafMisc_A86A5D0E(&transaction, url, "GET", NULL);
	if (error < 0)
		return error;
	error = transaction.SendRequest(NULL, 0);
	if (error < 0)
		return error;

	string file_name;

	error = transaction.GetFileName(file_name);
	if (error < 0)
		return error;
	if (dir == NULL)
		dir = "";

	string path(dir);

	if (path.size() != 0 && path.at(path.size() - 1) != '/')
		path.append(1, '/');
	path.append(file_name);
	{
		common::SharedPtr<File> file = LocalFile::Open(path.c_str(),
							       SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
							       SCE_STM_RWU, &error);

		if (file.get() == NULL)
			return error;
		error = ScePafMisc_EE1CA56C(&transaction, file, (uint64_t)-1);
		file->Close();
	}
	if (error != 0) {
		LocalFile::RemoveFile(path.c_str());
		return error;
	}
	if (name != NULL)
		*name = file_name;
	return 0;
}

namespace paf {
namespace http {
namespace url {

string Encode(const char *src)
{
	string dst;

	if (src != NULL) {
		const unsigned char *pos = (const unsigned char *)src;

		while (*pos != '\0') {
			dst += s_encode_table[*pos];
			pos++;
		}
	}
	return dst;
}

string Decode(const char *src)
{
	string dst;

	if (src != NULL) {
		char hex[3] = { 0, 0, 0 };
		const char *pos = src;

		for (int ch = *pos; *pos != '\0'; ch = *pos) {
			if (ch == '%' && (sce_paf_look_ctype_table(pos[1]) & 0x44) &&
			    (sce_paf_look_ctype_table(pos[2]) & 0x44)) {
				hex[0] = pos[1];
				hex[1] = pos[2];
				dst.append(1, (char)sce_paf_strtoul(hex, NULL, 16));
				pos += 3;
			} else {
				dst.append(1, ch);
				pos++;
			}
		}
	}
	return dst;
}

string FormEncode(const char *src)
{
	string dst;

	if (src != NULL) {
		const unsigned char *pos = (const unsigned char *)src;

		while (*pos != '\0') {
			if (*pos == ' ')
				dst.append(1, '+');
			else
				dst += s_encode_table[*pos];
			pos++;
		}
	}
	return dst;
}

string FormDecode(const char *src)
{
	string dst;

	if (src != NULL) {
		char hex[3] = { 0, 0, 0 };
		const char *pos = src;

		for (int ch = *pos; *pos != '\0'; ch = *pos) {
			if (ch == '%' && (sce_paf_look_ctype_table(pos[1]) & 0x44) &&
			    (sce_paf_look_ctype_table(pos[2]) & 0x44)) {
				hex[0] = pos[1];
				hex[1] = pos[2];
				dst.append(1, (char)sce_paf_strtoul(hex, NULL, 16));
				pos += 3;
			} else {
				pos++;
				if (ch == '+')
					dst.append(1, ' ');
				else
					dst.append(1, ch);
			}
		}
	}
	return dst;
}

}
}
}
