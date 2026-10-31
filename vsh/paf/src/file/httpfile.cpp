/*
 * paf::HttpFile: Open builds a reader from the OpenArg (CreateReader) and opens
 * it; the asynchronous calls run the reader's call on a FileIoThread and
 * CompleteAsync collects its result. The request and response headers are a
 * HttpHeader, kept by the exported functions at the end of this file.
 */
#include <file/httpfile.h>
#include <url/url.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/ctype.h>
#include "httpfile_reader.h"

namespace paf {

int32_t HttpFile::OpenArg::ParseUrl(Url *url)
{
	if (url == NULL || !ScePafMisc_0E846E5C(url))
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	scheme = url->GetSchemeString();
	host = url->GetHostString();
	port = (uint16_t)url->GetPort();
	user = url->GetUserString();
	passwd = url->GetPasswordString();
	uri = url->GetPathString();
	uri.append(url->GetQueryString());
	uri.append(url->GetAnchorString());
	uri.append(url->GetParamaterString());
	return 0;
}

static HttpFile::Timeout httpFileDefaultTimeout = { -1, -1, -1, -1 };

HttpFile::OpenArg::OpenArg()
	: scheme("HTTP"), port(80), method("GET"), version("HTTP/1.1"), msg_body(NULL),
	  msg_body_len(0), https_flag(HTTPS_FLAG_DEFAULT & ~HTTPS_FLAG_CLIENT_VERIFY), open_flag(0),
	  accept_encoding_gzip(2), timeout(httpFileDefaultTimeout), template_option(0),
	  response_header_max_size(0), response_code(NULL)
{
	file_type = Type_Http;
}

HttpFile::HttpFile()
	: m_is_opened(false), m_lock_rwlock("ScePafHttpFileRwLock", 0),
	  m_lock_mutex("ScePafHttpFileCsLock", 0), m_io_thread(NULL), m_http_reader(NULL),
	  m_reader_param(NULL)
{
	m_reader_result[0] = 0;
	m_reader_result[1] = 0;
	m_reader_result[2] = 0;
}

common::SharedPtr<HttpFile> HttpFile::Open(Url *url, int32_t *error, uint32_t flag)
{
	if (url == NULL) {
		*error = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		return common::SharedPtr<HttpFile>();
	}

	HttpFile *file = new HttpFile;

	if (file == NULL) {
		if (error != NULL)
			*error = SCE_PAF_ERROR_FILE_NO_MEMORY;
		return common::SharedPtr<HttpFile>();
	}

	OpenArg arg;

	arg.ParseUrl(url);
	arg.open_flag = flag;

	int32_t ret = file->Open(&arg);

	if (error != NULL)
		*error = ret;
	if (file->IsOpened())
		return common::SharedPtr<HttpFile>(file);
	delete file;
	return common::SharedPtr<HttpFile>();
}

common::SharedPtr<HttpFile> HttpFile::Open(const char *path, uint32_t flag, uint32_t mode,
					   int32_t *error)
{
	Url url((string(path)));

	return Open(&url, error, flag);
}

common::SharedPtr<HttpFile> HttpFile::Open(const char *url, int32_t *error, uint32_t flag)
{
	Url value((string(url)));

	return Open(&value, error, flag);
}

common::SharedPtr<HttpFile> HttpFile::Open(const wchar_t *url, int32_t *error, uint32_t flag)
{
	Url value((wstring(url)));

	return Open(&value, error, flag);
}

int32_t HttpFile::GetFileType() const
{
	return Type_Http;
}

int32_t HttpFile::GetCapability() const
{
	return 0x7bf7;
}

int64_t HttpFile::GetFileSize() const
{
	thread::RMutexLock lock(&m_lock_mutex);

	if (m_io_thread != NULL)
		return SCE_PAF_ERROR_FILE_BUSY;
	if (!m_is_opened)
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	return m_http_reader->GetFileSize();
}

/* Drops the reader and what it was made from. */
int32_t HttpFile::DeleteReader()
{
	delete m_http_reader;

	HttpFileReaderParam *param = m_reader_param;

	m_http_reader = NULL;
	delete param;
	m_reader_param = NULL;
	return 0;
}

int32_t HttpFileResponseCodeToError(uint16_t code)
{
	if (code <= 99) {
		/* not a status code */
	} else if (code <= 199) {
		switch (code) {
		case 100:
			return SCE_PAF_ERROR_FILE_STATUS_100 + 0;
		case 101:
			return SCE_PAF_ERROR_FILE_STATUS_100 + 1;
		case 102:
			return SCE_PAF_ERROR_FILE_STATUS_100 + 2;
		}
	} else if (code <= 299) {
		return 0;
	} else if (code <= 399) {
		switch (code) {
		case 300:
			return SCE_PAF_ERROR_FILE_STATUS_300 + 0;
		case 301:
			return SCE_PAF_ERROR_FILE_STATUS_300 + 1;
		case 302:
			return SCE_PAF_ERROR_FILE_STATUS_300 + 2;
		case 303:
			return SCE_PAF_ERROR_FILE_STATUS_300 + 3;
		case 304:
			return SCE_PAF_ERROR_FILE_STATUS_300 + 4;
		case 305:
			return SCE_PAF_ERROR_FILE_STATUS_300 + 5;
		case 307:
			return SCE_PAF_ERROR_FILE_STATUS_300 + 7;
		}
	} else if (code <= 499) {
		switch (code) {
		case 400:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 0;
		case 401:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 1;
		case 402:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 2;
		case 403:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 3;
		case 404:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 4;
		case 405:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 5;
		case 406:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 6;
		case 407:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 7;
		case 408:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 8;
		case 409:
			return SCE_PAF_ERROR_FILE_STATUS_400 + 9;
		case 410:
			return SCE_PAF_ERROR_FILE_STATUS_410 + 0;
		case 411:
			return SCE_PAF_ERROR_FILE_STATUS_410 + 1;
		case 412:
			return SCE_PAF_ERROR_FILE_STATUS_410 + 2;
		case 413:
			return SCE_PAF_ERROR_FILE_STATUS_410 + 3;
		case 414:
			return SCE_PAF_ERROR_FILE_STATUS_410 + 4;
		case 415:
			return SCE_PAF_ERROR_FILE_STATUS_410 + 5;
		case 416:
			return SCE_PAF_ERROR_FILE_STATUS_410 + 6;
		case 417:
			return SCE_PAF_ERROR_FILE_STATUS_410 + 7;
		case 422:
			return SCE_PAF_ERROR_FILE_STATUS_420 + 2;
		case 423:
			return SCE_PAF_ERROR_FILE_STATUS_420 + 3;
		case 424:
			return SCE_PAF_ERROR_FILE_STATUS_420 + 4;
		case 426:
			return SCE_PAF_ERROR_FILE_STATUS_420 + 6;
		}
	} else if (code <= 599) {
		switch (code) {
		case 500:
			return SCE_PAF_ERROR_FILE_STATUS_500 + 0;
		case 501:
			return SCE_PAF_ERROR_FILE_STATUS_500 + 1;
		case 502:
			return SCE_PAF_ERROR_FILE_STATUS_500 + 2;
		case 503:
			return SCE_PAF_ERROR_FILE_STATUS_500 + 3;
		case 504:
			return SCE_PAF_ERROR_FILE_STATUS_500 + 4;
		case 505:
			return SCE_PAF_ERROR_FILE_STATUS_500 + 5;
		case 507:
			return SCE_PAF_ERROR_FILE_STATUS_500 + 7;
		}
	}
	return SCE_PAF_ERROR_FILE_UNKNOWN_RESPONSE;
}

/*
 * Copies the request into m_reader_param and makes the reader for it: a range
 * reader when the open flags ask for byte ranges, a plain one otherwise.
 */
int32_t HttpFile::CreateReader(const OpenArg *arg)
{
	uint32_t range = arg->open_flag & (OpenArg::ENABLE_RANGE_GET | OpenArg::ENABLE_HALF_RANGE_GET);

	m_reader_param = new HttpFileReaderParam;
	if (m_reader_param == NULL)
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	if (arg->msg_body != NULL && arg->msg_body_len != 0) {
		if (range != 0) {
			void *data = sce_paf_malloc(arg->msg_body_len);

			if (data == NULL) {
				delete m_reader_param;
				m_reader_param = NULL;
				return SCE_PAF_ERROR_FILE_NO_MEMORY;
			}
			sce_paf_memcpy(data, arg->msg_body, arg->msg_body_len);
			m_reader_param->msg_body = HeapAllocBuffer::Create(data, arg->msg_body_len, NULL);
		} else {
			m_reader_param->msg_body = UnmanagedConstBuffer::Create(arg->msg_body,
										arg->msg_body_len);
		}
	}

	int32_t count = arg->header.size();
	HttpHeader *header = &m_reader_param->header;

	for (int32_t i = 0; i < count; i++)
		ScePafMisc_DB5A6E15(header, arg->header[i].c_str(), arg->header[i].size());
	if (ScePafMisc_82171187(header, HttpFileReader::header_user_agent, 0) == NULL && arg->user_agent.size() != 0)
		ScePafMisc_1156F852(header, HttpFileReader::header_user_agent, arg->user_agent.c_str(), true);
	if (ScePafMisc_82171187(header, HttpFileReader::header_referer, 0) == NULL && arg->referer.size() != 0)
		ScePafMisc_1156F852(header, HttpFileReader::header_referer, arg->referer.c_str(), true);
	m_reader_param->scheme = arg->scheme;
	m_reader_param->host = arg->host;
	m_reader_param->port = arg->port;
	m_reader_param->method = arg->method;
	m_reader_param->version = arg->version;
	m_reader_param->uri = arg->uri;
	m_reader_param->user = arg->user;
	m_reader_param->passwd = arg->passwd;
	m_reader_param->open_flag = arg->open_flag;
	m_reader_param->https_flag = arg->https_flag;
	m_reader_param->timeout = arg->timeout;
	m_reader_param->accept_encoding_gzip = arg->accept_encoding_gzip;
	m_reader_param->template_option = arg->template_option;
	m_reader_param->response_header_max_size = arg->response_header_max_size;
	m_reader_param->context = m_reader_context;

	common::SharedPtr<HttpFileReaderConfig> config =
		HttpFileReaderConfig::Create(*m_reader_param);

	if (range != 0)
		m_http_reader = new HttpFileRangeReader(config);
	else
		m_http_reader = new HttpFileHttpReader(config);
	if (m_http_reader == NULL) {
		delete m_reader_param;
		m_reader_param = NULL;
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}
	return 0;
}

/* Keeps the reader's last results, read back by the file's owner. */
void HttpFile::CopyReaderResult()
{
	HttpFileReader *reader = m_http_reader;

	if (reader != NULL) {
		m_reader_result[0] = reader->m_result[0];
		m_reader_result[1] = reader->m_result[1];
		m_reader_result[2] = reader->m_result[2];
	}
}

int32_t HttpFile::Open(const File::OpenArg *param)
{
	const OpenArg *arg = (const OpenArg *)param;
	int32_t result;

	m_lock_mutex.Lock();
	if (arg == NULL || GetFileType() != arg->file_type) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	if (m_io_thread != NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (m_is_opened) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_ALREADY_OPENED;
	}
	if (arg->response_code != NULL)
		*arg->response_code = 0;
	m_lock_rwlock.WriterLock();
	result = CreateReader(arg);
	m_lock_rwlock.WriterUnlock();
	if (result != 0) {
		m_lock_mutex.Unlock();
		return result;
	}
	m_lock_rwlock.ReaderLock();
	result = m_http_reader->Open();
	CopyReaderResult();
	m_lock_rwlock.ReaderUnlock();
	if (arg->response_code != NULL)
		*arg->response_code = m_http_reader->GetResponse()->code;
	if (result == 0 && !(arg->open_flag & OpenArg::SKIP_STATUS_CHECK))
		result = HttpFileResponseCodeToError(m_http_reader->GetResponse()->code);
	m_lock_rwlock.WriterLock();
	if (result != 0)
		DeleteReader();
	else
		m_is_opened = true;
	m_lock_rwlock.WriterUnlock();
	m_lock_mutex.Unlock();
	return result;
}

int32_t HttpFile::OpenAsync(const File::OpenArg *param)
{
	const OpenArg *arg = (const OpenArg *)param;
	thread::RMutexLock lock(&m_lock_mutex);

	if (arg == NULL || GetFileType() != arg->file_type)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	if (m_io_thread != NULL)
		return SCE_PAF_ERROR_FILE_BUSY;
	if (m_is_opened)
		return SCE_PAF_ERROR_FILE_ALREADY_OPENED;
	m_lock_rwlock.WriterLock();

	int32_t result = CreateReader(arg);

	/* Sony's keeps the writer lock when CreateReader fails. */
	if (result != 0)
		return result;

	HttpFileReader *reader = m_http_reader;

	m_io_thread = new HttpFileIoThread(FileIoThread::Operation_Open,
		new MemberCall0<int32_t, HttpFileReader>(reader, &HttpFileReader::Open));
	m_lock_rwlock.WriterUnlock();
	if (m_io_thread == NULL) {
		m_lock_rwlock.WriterLock();
		DeleteReader();
		m_lock_rwlock.WriterUnlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}
	result = m_io_thread->Start();
	if (result != 0) {
		m_lock_rwlock.WriterLock();
		delete m_io_thread;
		m_io_thread = NULL;
		DeleteReader();
		m_lock_rwlock.WriterUnlock();
	}
	return result;
}

int32_t HttpFile::Close()
{
	int32_t ret;

	m_lock_mutex.Lock();
	if (m_io_thread != NULL) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else if (m_is_opened) {
		m_lock_rwlock.WriterLock();
		ret = m_http_reader->Close();
		DeleteReader();
		m_is_opened = false;
		m_lock_rwlock.WriterUnlock();
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	m_lock_mutex.Unlock();
	return ret;
}

/* Starts m_io_thread, made by the caller under the writer lock. */
int32_t HttpFile::StartIoThread()
{
	if (m_io_thread == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t result = m_io_thread->Start();

	if (result != 0) {
		m_lock_rwlock.WriterLock();
		delete m_io_thread;
		m_io_thread = NULL;
		m_lock_rwlock.WriterUnlock();
	}
	m_lock_mutex.Unlock();
	return result;
}

int32_t HttpFile::CloseAsync()
{
	m_lock_mutex.Lock();
	if (m_io_thread != NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (!m_is_opened) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	m_lock_rwlock.WriterLock();

	HttpFileReader *reader = m_http_reader;

	m_io_thread = new HttpFileIoThread(FileIoThread::Operation_Close,
		new MemberCall0<int32_t, HttpFileReader>(reader, &HttpFileReader::Close));
	m_lock_rwlock.WriterUnlock();
	return StartIoThread();
}

bool HttpFile::IsOpened() const
{
	bool opened;

	m_lock_mutex.Lock();
	opened = m_is_opened;
	m_lock_mutex.Unlock();
	return opened;
}

bool HttpFile::IsEof()
{
	m_lock_mutex.Lock();
	m_lock_mutex.Unlock();
	return false;
}

int32_t HttpFile::Abort()
{
	int32_t ret;

	m_lock_rwlock.ReaderLock();
	if (m_http_reader != NULL)
		ret = m_http_reader->Abort();
	else
		ret = m_io_thread != NULL ? SCE_PAF_OK : SCE_PAF_ERROR_FILE_NOT_BUSY;
	m_lock_rwlock.ReaderUnlock();
	return ret;
}

int32_t HttpFile::Read(void *buf, size_t nbyte)
{
	int32_t ret;

	m_lock_mutex.Lock();
	if (buf == NULL) {
		ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	} else if (m_io_thread != NULL) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else if (m_is_opened) {
		ret = m_http_reader->Read(buf, nbyte);
		CopyReaderResult();
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t HttpFile::ReadAsync(void *buf, size_t nbyte)
{
	m_lock_mutex.Lock();
	if (buf == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	if (m_io_thread != NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (!m_is_opened) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	m_lock_rwlock.WriterLock();

	HttpFileReader *reader = m_http_reader;

	m_io_thread = new HttpFileReadIoThread(FileIoThread::Operation_Read,
		new MemberCall2<int32_t, HttpFileReader, void *, size_t>(reader,
			&HttpFileReader::Read, buf, nbyte));
	m_lock_rwlock.WriterUnlock();
	return StartIoThread();
}

int32_t HttpFile::Write(const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int32_t HttpFile::WriteAsync(const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int64_t HttpFile::Seek(int64_t offset, int32_t whence)
{
	int64_t ret;

	m_lock_mutex.Lock();
	if (m_io_thread != NULL) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else if (m_is_opened) {
		ret = m_http_reader->Seek(offset, whence);
		CopyReaderResult();
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t HttpFile::SeekAsync(int64_t offset, int32_t whence)
{
	m_lock_mutex.Lock();
	if (m_io_thread != NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (!m_is_opened) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	m_lock_rwlock.WriterLock();

	HttpFileReader *reader = m_http_reader;

	m_io_thread = new HttpFileSeekIoThread(FileIoThread::Operation_Seek,
		new MemberCall2<int64_t, HttpFileReader, int64_t, int32_t>(reader,
			&HttpFileReader::Seek, offset, whence));
	m_lock_rwlock.WriterUnlock();
	return StartIoThread();
}

int32_t HttpFile::Flush()
{
	m_lock_mutex.Lock();
	m_lock_mutex.Unlock();
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int32_t HttpFile::FlushAsync()
{
	m_lock_mutex.Lock();
	m_lock_mutex.Unlock();
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

}

using namespace paf;

/* Collects the finished asynchronous call's result; an open is checked as Open does. */
int32_t ScePafMisc_C098F659(HttpFile *self, int64_t *result)
{
	FileIoThread *thread = (FileIoThread *)self->m_io_thread;
	int64_t value = thread->m_result;
	int32_t operation = thread->m_operation;

	self->m_lock_rwlock.WriterLock();
	delete self->m_io_thread;

	HttpFileReader *reader = self->m_http_reader;

	self->m_io_thread = NULL;
	if (reader != NULL) {
		self->m_reader_result[0] = reader->m_result[0];
		self->m_reader_result[1] = reader->m_result[1];
		self->m_reader_result[2] = reader->m_result[2];
	}
	if (operation == FileIoThread::Operation_Open) {
		if (value == 0 && reader != NULL)
			value = HttpFileResponseCodeToError(reader->GetResponse()->code);
		if (value != 0) {
			self->m_is_opened = false;
			self->DeleteReader();
		} else {
			self->m_is_opened = true;
		}
	}
	self->m_lock_rwlock.WriterUnlock();
	if (result != NULL)
		*result = value;
	return 0;
}

namespace paf {

int32_t HttpFile::WaitAsync(int64_t *result)
{
	int32_t ret;

	m_lock_mutex.Lock();
	if (m_io_thread == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_BUSY;
	}
	m_io_thread->Join();
	ret = ScePafMisc_C098F659(this, result);
	m_lock_mutex.Unlock();
	return ret;
}

int32_t HttpFile::PollAsync(int64_t *result)
{
	int32_t ret;

	m_lock_mutex.Lock();
	if (m_io_thread == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_BUSY;
	}
	if (m_io_thread->IsAlive()) {
		m_lock_mutex.Unlock();
		return 1;
	}
	ret = ScePafMisc_C098F659(this, result);
	m_lock_mutex.Unlock();
	return ret;
}

int32_t HttpFile::GetResponseHeader(const char *name, string &value) const
{
	int32_t ret;

	m_lock_mutex.Lock();
	value = "";
	if (!m_is_opened || m_http_reader == NULL) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (m_io_thread != NULL) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else {
		value = ScePafMisc_82171187(m_http_reader->GetResponseHeaders(), name, 0);
		ret = SCE_PAF_OK;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t HttpFile::Getstat(FileStat *stat)
{
	thread::RMutexLock lock(&m_lock_mutex);

	if (stat == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	if (!m_is_opened || m_http_reader == NULL)
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	if (m_io_thread != NULL)
		return SCE_PAF_ERROR_FILE_BUSY;

	int64_t size = GetFileSize();

	if (size < 0)
		return (int32_t)size;
	stat->st_size = size;

	string value;
	int32_t ret = GetResponseHeader("Last-Modified", value);

	if (ret != 0)
		return ret;
	if (value.size() == 0)
		return SCE_PAF_ERROR_FILE_UNKNOWN_DATE;

	datetime::DateTime date = HttpFileParseDate(value.c_str());

	stat->st_ctime = date;
	stat->st_atime = date;
	stat->st_mtime = date;
	stat->st_attr = 0;
	stat->st_mode = 0;
	return ret;
}

int32_t HttpFile::GetResponseCode(uint16_t &code) const
{
	int32_t ret;

	m_lock_mutex.Lock();

	bool opened = m_is_opened;

	code = 0;
	if (opened && m_http_reader != NULL) {
		HttpResponse *response = m_http_reader->GetResponse();

		if (response->valid) {
			code = response->code;
			ret = SCE_PAF_OK;
		} else {
			ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		}
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t HttpFile::GetResponsePhrase(string &phrase) const
{
	int32_t ret;

	m_lock_mutex.Lock();
	phrase = "";
	if (!m_is_opened || m_http_reader == NULL) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (m_io_thread != NULL) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else {
		HttpResponse *response = m_http_reader->GetResponse();

		if (response->valid) {
			phrase = response->phrase.c_str();
			ret = SCE_PAF_OK;
		} else {
			ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		}
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t HttpFile::GetResponseVersion(uint8_t &major, uint8_t &minor) const
{
	int32_t ret;

	m_lock_mutex.Lock();
	minor = 0;
	major = 0;
	if (!m_is_opened || m_http_reader == NULL) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (m_io_thread != NULL) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else {
		HttpResponse *response = m_http_reader->GetResponse();

		if (response->valid) {
			major = response->major;
			/* Sony's gives the major version for both. */
			minor = response->major;
			ret = SCE_PAF_OK;
		} else {
			ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		}
	}
	m_lock_mutex.Unlock();
	return ret;
}

}

/* A context for files to share: at most capacity keep-alive connections. */
common::SharedPtr<HttpFileReaderContext> ScePafMisc_84A50E1F(size_t capacity)
{
	return common::SharedPtr<HttpFileReaderContext>(new HttpFileReaderContext(capacity));
}

void ScePafMisc_28BE7C38(HttpFile *self, const common::SharedPtr<HttpFileReaderContext> &context)
{
	self->m_reader_context = context;
}

common::SharedPtr<HttpFileReaderContext> ScePafMisc_679B51BB(const HttpFile *self)
{
	return self->m_reader_context;
}

namespace paf {

HttpFile::~HttpFile()
{
	if (m_io_thread != NULL) {
		m_io_thread->Cancel();
		m_io_thread->Join();
		delete m_io_thread;
	}
	m_io_thread = NULL;
	if (m_http_reader != NULL)
		m_http_reader->Close();
	DeleteReader();
}

}

/* The opener the file system calls for http: paths; userdata says whether it may. */
common::SharedPtr<HttpFile> ScePafMisc_BE023998(const char *path, uint32_t flag, uint32_t mode,
						 int32_t *error, const void *userdata)
{
	if (userdata == NULL || *(const bool *)userdata)
		return common::SharedPtr<HttpFile>();

	Url url((string(path)));

	return HttpFile::Open(&url, error, flag);
}

namespace paf {

int32_t HttpFile::OpenArg::ParseUrl(const char *url)
{
	Url value((string(url)));

	return ParseUrl(&value);
}

int32_t HttpFile::Getstat(const char *url, HttpFileStat *stat)
{
	if (stat == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;

	HttpFile *file = new HttpFile;

	if (file == NULL)
		return SCE_PAF_ERROR_FILE_NO_MEMORY;

	common::SharedPtr<HttpFile> owner(file);
	OpenArg arg;

	{
		Url value((string(url)));

		arg.ParseUrl(&value);
	}
	arg.method = "HEAD";

	int32_t ret = file->Open(&arg);

	if (ret != 0)
		return ret;
	ret = file->Getstat((FileStat *)stat);
	file->Close();
	return ret;
}

}

int32_t ScePafMisc_30A8AD46(int32_t value, HttpFile::OpenArg::OptionType type)
{
	if (value < 0)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	if (type & HttpFile::OpenArg::OptionType_ResolveTimeOut)
		httpFileDefaultTimeout.resolve_timeout = value / 31;
	if (type & HttpFile::OpenArg::OptionType_ConnectTimeOut)
		httpFileDefaultTimeout.connect_timeout = value;
	if (type & HttpFile::OpenArg::OptionType_SendTimeOut)
		httpFileDefaultTimeout.send_timeout = value;
	if (type & HttpFile::OpenArg::OptionType_RecvTimeOut)
		httpFileDefaultTimeout.recv_timeout = value;
	return SCE_PAF_OK;
}

namespace paf {

int32_t HttpFile::OpenArg::ParseUrl(const wchar_t *url)
{
	Url value((wstring(url)));

	return ParseUrl(&value);
}

int32_t HttpFile::OpenArg::AddRequestHeader(const char *name, const char *value)
{
	if (name == NULL)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	header.push_back(string(""));
	header[header.size() - 1].append(name);
	header[header.size() - 1].append(": ");
	header[header.size() - 1].append(value);
	return SCE_PAF_OK;
}

}

int32_t ScePafMisc_ED1839C2(HttpFile::OpenArg *self, int32_t value,
			    HttpFile::OpenArg::OptionType type)
{
	if (value < 0)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	if (type & HttpFile::OpenArg::OptionType_ResolveTimeOut)
		self->timeout.resolve_timeout = value / 31;
	if (type & HttpFile::OpenArg::OptionType_ConnectTimeOut)
		self->timeout.connect_timeout = value;
	if (type & HttpFile::OpenArg::OptionType_SendTimeOut)
		self->timeout.send_timeout = value;
	if (type & HttpFile::OpenArg::OptionType_RecvTimeOut)
		self->timeout.recv_timeout = value;
	return SCE_PAF_OK;
}

bool ScePafMisc_37897945(const HttpHeaderCompare *self, const string &left, const string &right)
{
	return sce_paf_strcasecmp(left.c_str(), right.c_str()) > 0;
}

/* Adds value to name's values; replace drops the ones it had. */
void ScePafMisc_1156F852(HttpHeader *self, const char *name, const char *value, bool replace)
{
	HttpHeader::iterator it = self->find(string(name));

	if (it == self->end()) {
		(*self)[string(name)].push_back(string(value));
	} else if (replace) {
		it->second.clear();
		it->second.push_back(string(value));
	} else {
		it->second.push_back(string(value));
	}
}

int32_t ScePafMisc_E21E796D(HttpHeader *self, const char *name)
{
	HttpHeader::iterator it = self->find(string(name));

	if (it == self->end())
		return 0;
	self->erase(it);
	return 1;
}

void ScePafMisc_F39DF886(HttpHeader *self)
{
	self->_Tidy();
}

const char *ScePafMisc_82171187(const HttpHeader *self, const char *name, int32_t index)
{
	HttpHeader::iterator it = const_cast<HttpHeader *>(self)->find(string(name));

	if (it == const_cast<HttpHeader *>(self)->end() || index >= (int32_t)it->second.size())
		return NULL;
	return it->second[index].c_str();
}

/* Calls callback with each name and value until it returns other than 0. */
int32_t ScePafMisc_5D64A09C(const HttpHeader *self, HttpHeaderCallback callback, void *userdata)
{
	if (callback == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;

	HttpHeader *header = const_cast<HttpHeader *>(self);

	for (HttpHeader::iterator it = header->begin(); it != header->end(); ++it) {
		int32_t count = it->second.size();

		for (int32_t i = 0; i < count; ++i) {
			int32_t result = callback(it->first.c_str(), it->second[i].c_str(), userdata);

			if (result != 0)
				return result;
		}
	}
	return 0;
}

/* Where ToString writes: the buffer, its room and how much is written. */
struct HttpHeaderString {
	char *buf;
	size_t max;
	size_t len;
};

static int32_t HttpHeaderToStringCallback(const char *name, const char *value, void *userdata);

/* The headers as "name: value\r\n" lines; size gives the room and gets the length. */
bool ScePafMisc_4EAE8284(const HttpHeader *self, char *buf, size_t *size)
{
	if (buf == NULL || size == NULL || *size == 0)
		return false;

	HttpHeaderString string = { buf, *size - 1, 0 };

	if (ScePafMisc_5D64A09C(self, HttpHeaderToStringCallback, &string) != 0)
		return false;
	buf[string.len] = '\0';
	*size = string.len + 1;
	return true;
}

/* One "name: value" line; 1 when it is one. */
int32_t ScePafMisc_DB5A6E15(HttpHeader *self, const char *line, size_t len)
{
	if (line == NULL || len < 2)
		return 0;

	string key;
	string value;
	size_t pos = 0;

	while (pos < len) {
		if (line[pos] == ':') {
			key.assign(line, pos);
			++pos;
			break;
		}
		if (!(sce_paf_look_ctype_table(line[pos]) & 0x1f) ||
		    (sce_paf_look_ctype_table(line[pos]) & 8))
			return 0;
		++pos;
	}
	if (key.size() == 0)
		return 0;
	while (pos < len && (sce_paf_look_ctype_table(line[pos]) & 8))
		++pos;
	value.assign(line + pos, len - pos);

	HttpHeader::iterator it = self->find(key);

	if (it == self->end())
		(*self)[key].push_back(value);
	else
		it->second.push_back(value);
	return 1;
}

/* A block of header lines, a line folded where the next starts with a blank; the lines it could not read. */
int32_t ScePafMisc_DBF32F34(HttpHeader *self, const char *header, size_t len)
{
	size_t pos = 0;
	size_t start = 0;
	int32_t errors = 0;
	bool terminated = false;

	ScePafMisc_F39DF886(self);
	if (header == NULL)
		return 0;
	if (len > 1) {
		do {
			terminated = false;
			if (header[pos] == '\r' && header[pos + 1] == '\n') {
				if (pos + 2 < len && (header[pos + 2] == ' ' || header[pos + 2] == '\t')) {
					pos += 3;
				} else {
					if (start < pos &&
					    ScePafMisc_DB5A6E15(self, header + start, pos - start))
						errors++;
					pos += 2;
					start = pos;
					terminated = true;
				}
			} else {
				++pos;
			}
		} while (pos + 1 < len);
	}
	if (!terminated && start < len && ScePafMisc_DB5A6E15(self, header + start, len - start))
		errors++;
	return errors;
}

paf::HttpResponse::HttpResponse()
{
	valid = false;
	minor = 0;
	major = 0;
	code = 0;
}

/* Reads a status line, "HTTP/<major>.<minor> <code><phrase>". */
bool ScePafMisc_EA63635D(HttpResponse *self, const char *line, size_t len)
{
	self->valid = false;
	self->phrase.assign(NULL, 0);
	self->major = 0;
	self->minor = 0;
	if (len < 9)
		return false;
	if (sce_paf_strncmp(line, "HTTP/", sce_paf_strlen("HTTP/")) != 0)
		return false;

	size_t pos = sce_paf_strlen("HTTP/");

	if (!(sce_paf_look_ctype_table(line[pos]) & 4))
		return false;
	while (sce_paf_look_ctype_table(line[pos]) & 4) {
		self->major = self->major * 10 + line[pos] - '0';
		++pos;
	}
	if (line[pos] != '.')
		return false;
	++pos;
	if (!(sce_paf_look_ctype_table(line[pos]) & 4))
		return false;
	while (sce_paf_look_ctype_table(line[pos]) & 4) {
		self->minor = self->minor * 10 + line[pos] - '0';
		++pos;
	}
	if (line[pos] != ' ')
		return false;
	++pos;
	if ((int32_t)pos < 0)
		return false;

	const char *status = line + pos;
	size_t remaining = len - pos;

	if (remaining < sce_paf_strlen("200 "))
		return false;
	if (!(sce_paf_look_ctype_table(status[0]) & 4) ||
	    !(sce_paf_look_ctype_table(status[1]) & 4) ||
	    !(sce_paf_look_ctype_table(status[2]) & 4))
		return false;
	self->code = status[0] * 100 + status[1] * 10 + status[2] - '0' * 111;
	pos = sce_paf_strlen("200");
	if ((int32_t)pos < 0)
		return false;

	const char *phrase = status + pos;
	size_t phrase_len;
	int32_t next = -1;

	remaining -= pos;
	for (size_t count = 0; count <= remaining; count++) {
		if (phrase[count] == '\n') {
			bool cr = count != 0 && phrase[count - 1] == '\r';

			phrase_len = cr ? count - 1 : count;
			next = count + 1;
			break;
		}
	}
	if (next < 0)
		return false;
	self->phrase.assign(phrase, phrase_len);
	self->valid = true;
	return true;
}

paf::HttpResponse::HttpResponse(const char *line, size_t len)
{
	valid = false;
	minor = 0;
	major = 0;
	code = 0;
	valid = ScePafMisc_EA63635D(this, line, len);
}

paf::HttpResponse::~HttpResponse()
{
}

static int32_t HttpHeaderToStringCallback(const char *name, const char *value, void *userdata)
{
	HttpHeaderString *string = (HttpHeaderString *)userdata;
	size_t name_len = sce_paf_strlen(name);
	size_t value_len = sce_paf_strlen(value);

	if (string->len + name_len + value_len + 4 > string->max)
		return SCE_PAF_ERROR_FILE_NO_MEMORY;

	char *line = string->buf + string->len;

	sce_paf_memcpy(line, name, name_len);
	line[name_len] = ':';
	line[name_len + 1] = ' ';
	sce_paf_memcpy(line + name_len + 2, value, value_len);
	line[name_len + 2 + value_len] = '\r';
	line[name_len + 2 + value_len + 1] = '\n';
	string->len += name_len + value_len + 4;
	return 0;
}
