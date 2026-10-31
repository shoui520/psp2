/*
 * The connections an HttpFile's readers send their requests on, the pool that
 * keeps them alive between requests (HttpFileReaderContext), and the reader
 * that sends one request and reads its response (HttpFileHttpReader).
 */
#include <libhttp.h>
#include <kernel/iofilemgr.h>
#include <file/httpfile.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "httpfile_reader.h"

extern "C" {

int32_t SceHttp_43A7CC70(int32_t id, uint32_t option);
int32_t SceHttp_F30073EC(int32_t id, uint32_t flags);
SceInt32 sceHttpSetAcceptEncodingGZIPEnabled(SceInt32 id, SceBool isEnable);

}

namespace paf {

const char *const HttpFileReader::header_user_agent = "UserAgent";
const char *const HttpFileReader::header_referer = "Referer";

/* Where Seek reads the bytes it skips forward over. */
static uint8_t httpFileHttpReaderSeekBuffer[1024];

HttpFileReaderConnection::~HttpFileReaderConnection()
{
	if (m_connection_id > 0)
		sceHttpDeleteConnection(m_connection_id);
	m_connection_id = 0;
	if (m_template_id > 0)
		sceHttpDeleteTemplate(m_template_id);
	m_template_id = 0;
}

HttpFileReaderConnection::HttpFileReaderConnection(const HttpFileReaderConnectionParam &src)
{
	param.http_version = src.http_version;
	param.user_agent = src.user_agent;
	param.host = src.host;
	param.scheme = src.scheme;
	param.https_flag = src.https_flag;
	param.auto_proxy = src.auto_proxy;
	param.port = src.port;
	param.template_option = src.template_option;
	param.enable_keepalive = src.enable_keepalive;
	param.response_header_max_size = src.response_header_max_size;
	m_template_id = 0;
	m_connection_id = 0;
	m_https_flag = HttpFile::OpenArg::HTTPS_FLAG_DEFAULT &
		       ~HttpFile::OpenArg::HTTPS_FLAG_CLIENT_VERIFY;
}

int32_t HttpFileReaderConnection::CreateHandles(const HttpFileReaderConnectionParam &param,
						int32_t *template_id, int32_t *connection_id)
{
	int32_t id = sceHttpCreateTemplate(param.user_agent.c_str(), param.http_version,
					   param.auto_proxy);

	if (id <= 0)
		return id;
	*template_id = id;
	if (param.template_option != 0) {
		int32_t ret = SceHttp_43A7CC70(id, param.template_option);

		if (ret < 0)
			return ret;
		id = *template_id;
	}
	if (param.response_header_max_size != 0) {
		int32_t ret = sceHttpSetResponseHeaderMaxSize(id, param.response_header_max_size);

		if (ret < 0)
			return ret;
		id = *template_id;
	}
	id = sceHttpCreateConnection(id, param.host.c_str(), param.scheme.c_str(), param.port,
				     param.enable_keepalive);
	if (id <= 0) {
		if (*template_id > 0)
			sceHttpDeleteTemplate(*template_id);
		*template_id = 0;
		return id;
	}
	*connection_id = id;

	uint32_t enable = 0;
	uint32_t disable = 0;

	if (param.https_flag & HttpFile::OpenArg::HTTPS_FLAG_SERVER_VERIFY)
		enable |= HttpFile::OpenArg::HTTPS_FLAG_SERVER_VERIFY;
	else
		disable |= HttpFile::OpenArg::HTTPS_FLAG_SERVER_VERIFY;
	if (param.https_flag & HttpFile::OpenArg::HTTPS_FLAG_CLIENT_VERIFY)
		enable |= HttpFile::OpenArg::HTTPS_FLAG_CLIENT_VERIFY;
	else
		disable |= HttpFile::OpenArg::HTTPS_FLAG_CLIENT_VERIFY;
	if (param.https_flag & HttpFile::OpenArg::HTTPS_FLAG_CN_CHECK)
		enable |= HttpFile::OpenArg::HTTPS_FLAG_CN_CHECK;
	else
		disable |= HttpFile::OpenArg::HTTPS_FLAG_CN_CHECK;
	if (param.https_flag & HttpFile::OpenArg::HTTPS_FLAG_NOT_AFTER_CHECK)
		enable |= HttpFile::OpenArg::HTTPS_FLAG_NOT_AFTER_CHECK;
	else
		disable |= HttpFile::OpenArg::HTTPS_FLAG_NOT_AFTER_CHECK;
	if (param.https_flag & HttpFile::OpenArg::HTTPS_FLAG_NOT_BEFORE_CHECK)
		enable |= HttpFile::OpenArg::HTTPS_FLAG_NOT_BEFORE_CHECK;
	else
		disable |= HttpFile::OpenArg::HTTPS_FLAG_NOT_BEFORE_CHECK;
	if (param.https_flag & HttpFile::OpenArg::HTTPS_FLAG_KNOWN_CA_CHECK)
		enable |= HttpFile::OpenArg::HTTPS_FLAG_KNOWN_CA_CHECK;
	else
		disable |= HttpFile::OpenArg::HTTPS_FLAG_KNOWN_CA_CHECK;

	int32_t ret = sceHttpsEnableOption2(id, enable);

	if (ret < 0)
		return ret;
	ret = sceHttpsDisableOption2(*connection_id, disable);
	if (ret < 0)
		return ret;
	return 0;
}

int32_t HttpFileReaderContext::GetConnection(common::SharedPtr<HttpFileReaderContext> context,
					     const HttpFileReaderConnectionParam &param,
					     common::SharedPtr<HttpFileReaderConnection> &result)
{
	if (context.get() != NULL && param.enable_keepalive) {
		thread::Mutex *mutex = &context->m_mutex;

		mutex->Lock();
		for (list<common::SharedPtr<HttpFileReaderConnection> >::iterator it =
			     context->m_connections.begin();
		     it != context->m_connections.end(); ++it) {
			HttpFileReaderConnection *connection = it->get();

			if (connection != NULL && connection->param.port == param.port &&
			    connection->param.host == param.host &&
			    connection->param.scheme == param.scheme &&
			    connection->param.user_agent == param.user_agent &&
			    connection->param.http_version == param.http_version &&
			    connection->param.auto_proxy == param.auto_proxy &&
			    connection->param.https_flag == param.https_flag &&
			    connection->param.template_option == param.template_option &&
			    connection->param.response_header_max_size ==
				    param.response_header_max_size) {
				result = *it;
				context->m_connections.erase(it);
				mutex->Unlock();
				return 0;
			}
		}
		mutex->Unlock();
	}

	int32_t template_id = 0;
	int32_t connection_id = 0;
	int32_t ret = HttpFileReaderConnection::CreateHandles(param, &template_id, &connection_id);

	if (ret != 0)
		return ret;
	result = common::SharedPtr<HttpFileReaderConnection>(new HttpFileReaderConnection(param));
	if (result.get() != NULL) {
		result->m_template_id = template_id;
		result->m_connection_id = connection_id;
		return 0;
	}
	if (connection_id > 0)
		sceHttpDeleteConnection(connection_id);
	if (template_id > 0) {
		connection_id = 0;
		sceHttpDeleteTemplate(template_id);
	}
	return SCE_PAF_ERROR_FILE_NO_MEMORY;
}

int32_t HttpFileReaderContext::ReturnConnection(
	common::SharedPtr<HttpFileReaderContext> context,
	common::SharedPtr<HttpFileReaderConnection> &connection)
{
	if (context.get() != NULL && connection.get() != NULL &&
	    connection->param.enable_keepalive && context->m_capacity != 0) {
		thread::Mutex *mutex = &context->m_mutex;

		mutex->Lock();
		while (context->m_connections.size() >= context->m_capacity)
			context->m_connections.pop_front();
		context->m_connections.push_back(connection);
		mutex->Unlock();
	}
	connection.reset();
	return 0;
}

HttpFileHttpReader::HttpFileHttpReader(common::SharedPtr<HttpFileReaderConfig> config)
	: HttpFileReader(config), m_request_id(0), m_position(0),
	  m_content_length(SCE_PAF_ERROR_FILE_UNKNOWN_SIZE),
	  m_lock("ScePafHttpReaderLock", 0), m_aborted(false)
{
}

HttpFileHttpReader::~HttpFileHttpReader()
{
	m_lock.WriterLock();
	if (m_request_id > 0)
		sceHttpDeleteRequest(m_request_id);
	m_request_id = 0;
	HttpFileReaderContext::ReturnConnection(m_config->context, m_connection);
	m_lock.WriterUnlock();
}

int32_t HttpFileHttpReader::Open()
{
	m_lock.WriterLock();
	if (m_request_id > 0)
		sceHttpDeleteRequest(m_request_id);
	m_request_id = 0;
	m_connection.reset();
	m_lock.WriterUnlock();

	HttpFileReaderConfig *config = m_config.get();

	m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	m_position = 0;

	string user_agent;
	string referer;

	user_agent.assign(ScePafMisc_82171187(&config->header, HttpFileReader::header_user_agent, 0));
	ScePafMisc_E21E796D(&config->header, HttpFileReader::header_user_agent);
	referer.assign(ScePafMisc_82171187(&config->header, HttpFileReader::header_referer, 0));
	ScePafMisc_E21E796D(&config->header, HttpFileReader::header_referer);

	const char *http_version = config->version;
	int32_t version = SCE_HTTP_VERSION_1_0;

	if (sce_paf_strcmp("HTTP/1.0", http_version) != 0) {
		sce_paf_strcmp("HTTP/1.1", http_version);
		version = SCE_HTTP_VERSION_1_1;
	}

	const void *body = NULL;
	size_t body_size = 0;

	if (config->msg_body.get() != NULL) {
		body = config->msg_body->GetAddr();
		body_size = config->msg_body->GetSize();
	}

	m_lock.WriterLock();
	if (m_aborted) {
		m_aborted = false;
		m_lock.WriterUnlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}

	int32_t ret;

	{
		HttpFileReaderConnectionParam param;

		sce_paf_memset(&param, 0, sizeof(param));
		param.http_version = version;
		param.user_agent = user_agent;
		param.host.assign(config->host);
		param.scheme.assign(config->scheme);
		param.port = config->port;
		param.auto_proxy = !config->IsProxyIgnored();
		param.enable_keepalive = config->IsHttpsCheckEnabled();
		param.https_flag = config->https_flag;
		param.template_option = config->template_option;
		param.response_header_max_size = config->response_header_max_size;
		ret = HttpFileReaderContext::GetConnection(config->context, param, m_connection);
		if (ret != 0)
			goto unlock_error;
		m_request_id = sceHttpCreateRequestWithURL2(m_connection->m_connection_id,
							    config->method, config->uri,
							    (uint64_t)body_size);
		if (m_request_id <= 0) {
			ret = m_request_id;
			goto unlock_error;
		}
		{
			uint32_t flags = 0;

			if (config->open_flag & HttpFile::OpenArg::SUPPRESS_3G_ACCESS)
				flags = 1;
			if (config->open_flag & HttpFile::OpenArg::SUPPRESS_WIFI_ACCESS)
				flags |= 2;
			if (config->open_flag & HttpFile::OpenArg::HTTP_OPTION4)
				flags |= 4;
			SceHttp_F30073EC(m_request_id, flags);
			if (config->timeout.resolve_timeout >= 0)
				sceHttpSetResolveTimeOut(m_request_id, config->timeout.resolve_timeout);
			if (config->timeout.connect_timeout >= 0)
				sceHttpSetConnectTimeOut(m_request_id, config->timeout.connect_timeout);
			if (config->timeout.send_timeout >= 0)
				sceHttpSetSendTimeOut(m_request_id, config->timeout.send_timeout);
			if (config->timeout.recv_timeout >= 0)
				sceHttpSetRecvTimeOut(m_request_id, config->timeout.recv_timeout);

			HttpRequestHeaderParam header;

			header.id = m_request_id;
			header.mode = SCE_HTTP_HEADER_OVERWRITE;
			ScePafMisc_5D64A09C(&config->header, AddHeader, &header);
			if (config->accept_encoding_gzip == 1)
				sceHttpSetAcceptEncodingGZIPEnabled(m_request_id, 1);
			else if (config->accept_encoding_gzip == 0)
				sceHttpSetAcceptEncodingGZIPEnabled(m_request_id, 0);
		}
	}
	m_lock.WriterUnlock();
	{
		m_lock.ReaderLock();

		int32_t request_id = m_request_id;

		m_lock.ReaderUnlock();
		ret = sceHttpSendRequest(request_id, body, body_size);

		int32_t error;
		uint32_t detail;

		if (ret == SCE_HTTP_ERROR_PROXY) {
			m_result[0] = sceHttpGetLastErrno(request_id, &error);
			if (m_result[0] == 0)
				m_result[0] = error;
		} else {
			m_result[0] = 0;
		}
		if (ret == SCE_HTTP_ERROR_SSL) {
			m_result[1] = sceHttpsGetSslError(request_id, &error, &detail);
			if (m_result[1] == 0) {
				m_result[1] = error;
				m_result[2] = detail;
			}
		} else {
			m_result[1] = 0;
			m_result[2] = 0;
		}
	}
	ScePafMisc_F39DF886(&m_response_headers);
	{
		m_lock.ReaderLock();

		int32_t request_id = m_request_id;

		m_lock.ReaderUnlock();

		char *header;
		size_t header_size;

		if (sceHttpGetAllResponseHeaders(request_id, &header, &header_size) == 0) {
			ScePafMisc_EA63635D(&m_response, header, header_size);
			ScePafMisc_DBF32F34(&m_response_headers, header, header_size);
		}
	}
	if (ret != 0)
		goto error;
	m_position = 0;
	{
		m_lock.ReaderLock();

		int32_t request_id = m_request_id;

		m_lock.ReaderUnlock();

		uint64_t content_length;

		if (sceHttpGetResponseContentLength(request_id, &content_length) == 0) {
			m_content_length = content_length;
			if (m_content_length < 0)
				m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
		} else {
			m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
		}
	}
	return 0;

unlock_error:
	m_lock.WriterUnlock();
error:
	m_lock.WriterLock();
	if (m_request_id > 0)
		sceHttpDeleteRequest(m_request_id);
	m_request_id = 0;
	HttpFileReaderContext::ReturnConnection(m_config->context, m_connection);
	m_aborted = false;
	m_lock.WriterUnlock();
	return ret;
}

int32_t HttpFileHttpReader::Close()
{
	m_lock.WriterLock();
	if (m_request_id > 0)
		sceHttpDeleteRequest(m_request_id);
	m_request_id = 0;
	HttpFileReaderContext::ReturnConnection(m_config->context, m_connection);
	m_aborted = false;
	m_lock.WriterUnlock();
	m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	m_position = 0;
	return 0;
}

int32_t HttpFileHttpReader::Read(void *buf, size_t nbyte)
{
	m_lock.ReaderLock();

	int32_t request_id = m_request_id;

	m_lock.ReaderUnlock();

	int32_t ret = sceHttpReadData(request_id, buf, nbyte);

	if (ret == SCE_HTTP_ERROR_SSL) {
		int32_t error;
		uint32_t detail;

		m_result[1] = sceHttpsGetSslError(request_id, &error, &detail);
		if (m_result[1] == 0) {
			m_result[1] = error;
			m_result[2] = detail;
		}
	} else {
		m_result[1] = 0;
		m_result[2] = 0;
	}
	if (ret >= 0)
		m_position += ret;
	return ret;
}

int64_t HttpFileHttpReader::Seek(int64_t offset, int32_t whence)
{
	int64_t remaining = offset;

	if (whence == SCE_SEEK_END) {
		if (m_content_length < 0)
			return SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
		remaining = offset + m_content_length - m_position;
	} else if (whence == SCE_SEEK_SET) {
		remaining = offset - m_position;
	}
	if (remaining < 0) {
		int64_t position = remaining + m_position;

		if (position < 0)
			return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		Close();

		int32_t ret = Open();

		if (ret != 0)
			return ret;
		return Seek(position, SCE_SEEK_SET);
	}
	while (remaining > 0) {
		size_t size = sizeof(httpFileHttpReaderSeekBuffer);

		if (remaining < size)
			size = (size_t)remaining;

		int32_t ret = Read(httpFileHttpReaderSeekBuffer, size);

		if (ret < 0)
			return ret;
		if (ret == 0)
			return m_position;
		remaining -= ret;
	}
	return m_position;
}

int32_t HttpFileHttpReader::Abort()
{
	m_lock.WriterLock();

	int32_t request_id = m_request_id;

	m_aborted = true;

	int32_t ret = sceHttpAbortRequest(request_id);

	m_lock.WriterUnlock();
	return ret;
}

int64_t HttpFileHttpReader::GetFileSize()
{
	if (m_content_length >= 0)
		return m_content_length;
	return SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
}

int32_t HttpFileHttpReader::AddHeader(const char *name, const char *value, void *userdata)
{
	HttpRequestHeaderParam *param = (HttpRequestHeaderParam *)userdata;

	if (param == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	return sceHttpAddRequestHeader(param->id, name, value, param->mode);
}

}
