/*
 * HttpFile's insides. A reader sends one request and reads its response: a
 * HttpFileHttpReader by itself, or a HttpFileRangeReader that opens one per byte
 * range it asks for. The readers share their connections through the file's
 * context, a pool of keep-alive connections. The request and response headers
 * are a HttpHeader, a map from name to values whose names compare without case.
 */
#ifndef PAF_FILE_HTTPFILE_READER_H
#define PAF_FILE_HTTPFILE_READER_H

#include <stddef.h>
#include <stdint.h>
#include <file/httpfile.h>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <std/map>
#include <std/string.h>
#include <thread/thread.h>
#include <src/paf_types.h>

#define SCE_PAF_ERROR_FILE_UNKNOWN_RESPONSE (-2135994284) /* 0x80AF5054 */
#define SCE_PAF_ERROR_FILE_INVALID_CONTENT_RANGE (-2135994283) /* 0x80AF5055 */
#define SCE_PAF_ERROR_FILE_STATUS_100 (-2135994224) /* 0x80AF5090 */
#define SCE_PAF_ERROR_FILE_STATUS_300 (-2135994208) /* 0x80AF50A0 */
#define SCE_PAF_ERROR_FILE_STATUS_400 (-2135994192) /* 0x80AF50B0 */
#define SCE_PAF_ERROR_FILE_STATUS_410 (-2135994176) /* 0x80AF50C0 */
#define SCE_PAF_ERROR_FILE_STATUS_420 (-2135994160) /* 0x80AF50D0 */
#define SCE_PAF_ERROR_FILE_STATUS_500 (-2135994144) /* 0x80AF50E0 */

namespace paf {

/* Header names, ordered without case (descending, as the map was built). */
class HttpHeaderCompare {
public:
	bool operator()(const string &left, const string &right) const
	{
		return sce_paf_strcasecmp(left.c_str(), right.c_str()) > 0;
	}
};

typedef map<string, vector<string>, HttpHeaderCompare> HttpHeader;

typedef int32_t (*HttpHeaderCallback)(const char *name, const char *value, void *userdata);

/* The status line of a response. */
class HttpResponse {
public:
	HttpResponse();
	HttpResponse(const char *line, size_t len);
	~HttpResponse();

	uint16_t code;
	string phrase;
	uint8_t major;
	uint8_t minor;
	bool valid;
};

/* What HttpFile::Open was asked, kept while the file is open. */
class HttpFileReaderParam {
public:
	HttpFileReaderParam()
	{
	}

	~HttpFileReaderParam()
	{
	}

	string scheme;
	string host;
	uint16_t port;
	string method;
	string version;
	string uri;
	string user;
	string passwd;
	HttpHeader header;
	common::SharedPtr<const Buffer> msg_body;
	uint32_t open_flag;
	uint32_t https_flag;
	HttpFile::Timeout timeout;
	int32_t accept_encoding_gzip;
	common::SharedPtr<HttpFileReaderContext> context;
	uint32_t template_option;
	size_t response_header_max_size;
};

/* A reader's request: the parameter's strings and a copy of its headers. */
class HttpFileReaderConfig {
public:
	HttpFileReaderConfig(const HttpFileReaderParam &param);

	HttpFileReaderConfig()
	{
	}

	HttpFileReaderConfig(const HttpFileReaderConfig &src)
	{
		*this = src;
	}

	~HttpFileReaderConfig()
	{
	}

	bool IsHttpsCheckEnabled() const;
	bool IsProxyIgnored() const;

	static common::SharedPtr<HttpFileReaderConfig> Create(const HttpFileReaderParam &param);
	static common::SharedPtr<HttpFileReaderConfig> Clone(
		common::SharedPtr<HttpFileReaderConfig> src);

	const char *scheme;
	const char *host;
	uint16_t port;
	const char *method;
	const char *version;
	const char *uri;
	const char *user;
	const char *passwd;
	HttpHeader header;
	common::SharedPtr<const Buffer> msg_body;
	uint32_t open_flag;
	uint32_t https_flag;
	HttpFile::Timeout timeout;
	int32_t accept_encoding_gzip;
	common::SharedPtr<HttpFileReaderContext> context;
	uint32_t template_option;
	size_t response_header_max_size;
};

/* What a connection was made for: a pooled one is reused only for the same. */
class HttpFileReaderConnectionParam {
public:
	int32_t http_version;
	string user_agent;
	string host;
	string scheme;
	uint16_t port;
	bool auto_proxy;
	bool enable_keepalive;
	uint32_t https_flag;
	uint32_t template_option;
	size_t response_header_max_size;
};

class HttpFileReaderConnection {
public:
	HttpFileReaderConnection(const HttpFileReaderConnectionParam &param);
	~HttpFileReaderConnection();

	static int32_t CreateHandles(const HttpFileReaderConnectionParam &param,
				     int32_t *template_id, int32_t *connection_id);

	HttpFileReaderConnectionParam param;
	int32_t m_template_id;
	int32_t m_connection_id;
	uint32_t m_https_flag;
};

/* The keep-alive connections, at most m_capacity of them. */
class HttpFileReaderContext {
public:
	HttpFileReaderContext(size_t capacity);

	~HttpFileReaderContext()
	{
	}

	static int32_t GetConnection(common::SharedPtr<HttpFileReaderContext> context,
				     const HttpFileReaderConnectionParam &param,
				     common::SharedPtr<HttpFileReaderConnection> &result);
	static int32_t ReturnConnection(common::SharedPtr<HttpFileReaderContext> context,
					common::SharedPtr<HttpFileReaderConnection> &connection);

	size_t m_capacity;
	list<common::SharedPtr<HttpFileReaderConnection> > m_connections;
	thread::Mutex m_mutex;
};

class HttpFileReader {
public:
	HttpFileReader(common::SharedPtr<HttpFileReaderConfig> config) : m_config(config)
	{
		m_result[0] = 0;
		m_result[1] = 0;
		m_result[2] = 0;
	}

	virtual ~HttpFileReader()
	{
	}

	virtual int32_t Open() = 0;
	virtual int32_t Close() = 0;
	virtual int32_t Read(void *buf, size_t nbyte) = 0;
	virtual int64_t Seek(int64_t offset, int32_t whence) = 0;
	virtual int32_t Abort() = 0;
	virtual int64_t GetFileSize() = 0;

	virtual HttpResponse *GetResponse()
	{
		return &m_response;
	}

	virtual HttpHeader *GetResponseHeaders()
	{
		return &m_response_headers;
	}

	/* The request headers HttpFile::OpenArg's user_agent and referer become. */
	static const char *const header_user_agent;
	static const char *const header_referer;

	common::SharedPtr<HttpFileReaderConfig> m_config;
	HttpHeader m_response_headers;
	HttpResponse m_response;
	int32_t m_result[3];
};

/* What HttpFileHttpReader::AddHeader adds a request header to. */
class HttpRequestHeaderParam {
public:
	int32_t id;
	int32_t mode;
};

class HttpFileHttpReader : public HttpFileReader {
public:
	HttpFileHttpReader(common::SharedPtr<HttpFileReaderConfig> config);
	virtual ~HttpFileHttpReader();

	virtual int32_t Open();
	virtual int32_t Close();
	virtual int32_t Read(void *buf, size_t nbyte);
	virtual int64_t Seek(int64_t offset, int32_t whence);
	virtual int32_t Abort();
	virtual int64_t GetFileSize();

	static int32_t AddHeader(const char *name, const char *value, void *userdata);

	common::SharedPtr<HttpFileReaderConnection> m_connection;
	int32_t m_request_id;
	int64_t m_position;
	int64_t m_content_length;
	thread::RWLock m_lock;
	bool m_aborted;
};

/* Reads by "Range: bytes=" requests while the server answers them. */
class HttpFileRangeReader : public HttpFileReader {
public:
	HttpFileRangeReader(common::SharedPtr<HttpFileReaderConfig> config);
	virtual ~HttpFileRangeReader();

	virtual int32_t Open();
	virtual int32_t Close();
	virtual int32_t Read(void *buf, size_t nbyte);
	virtual int64_t Seek(int64_t offset, int32_t whence);
	virtual int32_t Abort();
	virtual int64_t GetFileSize();

	virtual HttpResponse *GetResponse()
	{
		if (m_reader != NULL)
			return m_reader->GetResponse();
		return &m_response;
	}

	virtual HttpHeader *GetResponseHeaders()
	{
		if (m_reader != NULL)
			return m_reader->GetResponseHeaders();
		return &m_response_headers;
	}

	virtual bool FormatRange(char *dst, size_t max, size_t nbyte);

	bool m_range_enabled;
	HttpFileReader *m_reader;
	int64_t m_position;
	int64_t m_range_begin;
	int64_t m_range_end;
	int64_t m_content_length;
};

/* Runs one of a file's calls on its own thread; m_operation says which. */
class FileIoThread : public thread::Thread {
public:
	enum Operation {
		Operation_Open = 0,
		Operation_Close = 1,
		Operation_Read = 2,
		Operation_Seek = 4
	};

	FileIoThread(int32_t operation)
		: thread::Thread(SCE_KERNEL_DEFAULT_PRIORITY_USER, 0x4000, "ScePafFileIoThread", NULL),
		  m_operation(operation), m_result(0)
	{
	}

	virtual ~FileIoThread()
	{
	}

	int32_t m_operation;
	int64_t m_result;
};

/* A member function call on an object, with the arguments it is made with. */
template<class R, class T>
class MemberCall0 {
public:
	MemberCall0(T *object, R (T::*method)()) : m_object(object), m_method(method)
	{
	}

	R operator()()
	{
		return (m_object->*m_method)();
	}

	T *m_object;
	R (T::*m_method)();
};

template<class R, class T, class A1, class A2>
class MemberCall2 {
public:
	MemberCall2(T *object, R (T::*method)(A1, A2), A1 arg1, A2 arg2)
		: m_object(object), m_method(method), m_arg1(arg1), m_arg2(arg2)
	{
	}

	R operator()()
	{
		return (m_object->*m_method)(m_arg1, m_arg2);
	}

	T *m_object;
	R (T::*m_method)(A1, A2);
	A1 m_arg1;
	A2 m_arg2;
};

template<class Call>
class FileIoCallThread : public FileIoThread {
public:
	FileIoCallThread(int32_t operation, Call *call) : FileIoThread(operation), m_call(call)
	{
	}

	virtual ~FileIoCallThread()
	{
		delete m_call;
	}

	virtual void EntryFunction()
	{
		m_result = (*m_call)();
	}

	Call *m_call;
};

typedef FileIoCallThread<MemberCall0<int32_t, HttpFileReader> > HttpFileIoThread;
typedef FileIoCallThread<MemberCall2<int32_t, HttpFileReader, void *, size_t> > HttpFileReadIoThread;
typedef FileIoCallThread<MemberCall2<int64_t, HttpFileReader, int64_t, int32_t> > HttpFileSeekIoThread;

int32_t HttpFileResponseCodeToError(uint16_t code);
datetime::DateTime HttpFileParseDate(const char *text);

}

extern "C" {

/* The exported header and status-line functions keep their NIDs. */
void ScePafMisc_1156F852(paf::HttpHeader *self, const char *name, const char *value,
			 bool replace);
int32_t ScePafMisc_E21E796D(paf::HttpHeader *self, const char *name);
const char *ScePafMisc_82171187(const paf::HttpHeader *self, const char *name, int32_t index);
int32_t ScePafMisc_5D64A09C(const paf::HttpHeader *self, paf::HttpHeaderCallback callback,
			    void *userdata);
bool ScePafMisc_4EAE8284(const paf::HttpHeader *self, char *buf, size_t *size);
int32_t ScePafMisc_DB5A6E15(paf::HttpHeader *self, const char *line, size_t len);
int32_t ScePafMisc_DBF32F34(paf::HttpHeader *self, const char *header, size_t len);
void ScePafMisc_F39DF886(paf::HttpHeader *self);
bool ScePafMisc_37897945(const paf::HttpHeaderCompare *self, const paf::string &left,
			 const paf::string &right);

bool ScePafMisc_EA63635D(paf::HttpResponse *self, const char *line, size_t len);

int32_t ScePafMisc_ED1839C2(paf::HttpFile::OpenArg *self, int32_t value,
			    paf::HttpFile::OpenArg::OptionType type);
int32_t ScePafMisc_30A8AD46(int32_t value, paf::HttpFile::OpenArg::OptionType type);
int32_t ScePafMisc_C098F659(paf::HttpFile *self, int64_t *result);
void ScePafMisc_28BE7C38(paf::HttpFile *self,
			 const paf::common::SharedPtr<paf::HttpFileReaderContext> &context);
paf::common::SharedPtr<paf::HttpFileReaderContext> ScePafMisc_679B51BB(const paf::HttpFile *self);
paf::common::SharedPtr<paf::HttpFileReaderContext> ScePafMisc_84A50E1F(size_t capacity);
paf::common::SharedPtr<paf::HttpFile> ScePafMisc_BE023998(const char *path, uint32_t flag,
							   uint32_t mode, int32_t *error,
							   const void *userdata);

}

#endif
