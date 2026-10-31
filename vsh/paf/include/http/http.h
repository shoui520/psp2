/*
 * paf::http: an HTTP client (a libhttp template made from the client's
 * settings) and its transactions (a connection and a request on it).
 */
#ifndef PAF_HTTP_HTTP_H
#define PAF_HTTP_HTTP_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <file/file.h>

struct MainThreadCall;
struct StateCheck;

namespace paf {
namespace http {

class Client {
public:
	/* A client's settings; the client keeps a copy. Timeouts of 0 take libhttp's defaults. */
	class Param {
	public:
		Param(const string &user_agent = string())
			: m_user_agent(user_agent), m_ssl_flags(1), m_resolve_timeout(0),
			  m_resolve_retry(0), m_connect_timeout(0), m_send_timeout(0),
			  m_recv_timeout(0), m_reserved_flag(true)
		{
		}

		Param(const Param &src)
			: m_user_agent(src.m_user_agent), m_ssl_flags(src.m_ssl_flags),
			  m_user_name(src.m_user_name), m_password(src.m_password),
			  m_reserved(src.m_reserved), m_resolve_timeout(src.m_resolve_timeout),
			  m_resolve_retry(src.m_resolve_retry),
			  m_connect_timeout(src.m_connect_timeout),
			  m_send_timeout(src.m_send_timeout), m_recv_timeout(src.m_recv_timeout),
			  m_reserved_flag(src.m_reserved_flag)
		{
		}

		~Param()
		{
		}

		string m_user_agent;
		uint32_t m_ssl_flags;		/* bits 1-3: the request's SSL options to disable */
		string m_user_name;		/* with m_password: basic authentication */
		string m_password;
		string m_reserved;
		uint32_t m_resolve_timeout;	/* microseconds */
		uint32_t m_resolve_retry;
		uint32_t m_connect_timeout;	/* seconds */
		uint32_t m_send_timeout;	/* seconds */
		uint32_t m_recv_timeout;	/* seconds */
		bool m_reserved_flag;
	};

	/*
	 * Exported under NIDs 2984E347 (C1) and 92DC3409 (C2): Sony's mangled names
	 * spell the parameter's type otherwise, and no search has found it.
	 */
	Client(const Param &param);
	~Client();

	Param m_param;
	int32_t m_template_id;
};

class Transaction {
public:
	Transaction(Client &client);
	~Transaction();

	int32_t Destroy();
	int32_t SetRequestHeader(const char *name, const char *value);
	int32_t GetResponseHeader(const char *name, string &value);
	int32_t SetAutoRedirect(bool enable);
	int32_t GetAutoRedirect(bool &enable);
	int32_t GetStatusCode(int &status_code);
	int32_t GetContentLength(uint64_t &content_length);
	int32_t GetURL(string &url);
	int32_t GetFileName(string &name);
	int32_t GetRealm(string &realm);
	int32_t GetMimeType(string &mime_type);
	int32_t SendRequest(const void *data, size_t size);
	int32_t ReceiveResponse(common::SharedPtr<Buffer> &buffer, uint32_t *size);
	int32_t DumpResponseHeaders();
	int32_t SetResponseHeaderMaxSize(uint32_t size);

	static int32_t AuthInfoCallback(int32_t request, int32_t auth_type, const char *realm,
					char *user_name, char *password, int32_t need_entity,
					unsigned char **entity_body, uint32_t *entity_size,
					int32_t *save, void *arg);
	static int32_t SslCallback(uint32_t verify_error, void *const cert[], int cert_num,
				   void *arg);

	Client *m_client;
	::MainThreadCall *m_abort_call;	/* aborts the request when its StateCheck says so */
	int32_t m_connection_id;
	int32_t m_request_id;
	uint32_t m_ssl_error_mask;	/* certificate errors that fail the connection */
	string m_url;
};

namespace url {

/*
 * Encode writes %XX for every byte but [A-Za-z0-9-._~], Decode reads it back;
 * the Form kinds write and read a space as '+'.
 */
string Encode(const char *src);
string Decode(const char *src);
string FormEncode(const char *src);
string FormDecode(const char *src);

}

}
}

/*
 * Exports whose Sony names no NID search has found: the
 * transaction's request setup, SSL option and abort calls, and the URL's file
 * name.
 */
extern "C" {
int32_t ScePafMisc_5BF60DE9(paf::http::Transaction *self, uint32_t flags);
int32_t ScePafMisc_2D83836A(paf::http::Transaction *self, const char *url, const char *method,
			    uint64_t content_length, StateCheck *check);
int32_t ScePafMisc_A86A5D0E(paf::http::Transaction *self, const char *url, const char *method,
			    StateCheck *check);
int32_t ScePafMisc_CA7A4BB2(const char *url, paf::string &name);
int32_t ScePafMisc_A7E7AF41(paf::http::Transaction *self);
int32_t ScePafMisc_23F9BC0A(paf::http::Transaction *self, uint32_t flags);
int32_t ScePafMisc_7EA133E0(paf::http::Transaction *self);
int32_t ScePafMisc_EE1CA56C(paf::http::Transaction *self,
			    paf::common::SharedPtr<paf::File> &file, uint64_t max_size);
int32_t ScePafMisc_4E787F5E(const char *url, const char *dir, paf::string *name);
}

#endif
