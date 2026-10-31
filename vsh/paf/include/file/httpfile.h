/* paf::HttpFile: a file read over HTTP (C view in src/file/httpfile.h). */
#ifndef PAF_FILE_HTTPFILE_H
#define PAF_FILE_HTTPFILE_H

#include <file/file.h>
#include <common/shared_ptr.h>
#include <std/string>
#include <std/vector>
#include <thread/thread.h>

namespace paf {

class HttpFileReader;
class HttpFileReaderParam;
class HttpFileReaderContext;

class HttpFile : public File {
public:
	class Timeout {
	public:
		int32_t resolve_timeout;
		int32_t connect_timeout;
		int32_t send_timeout;
		int32_t recv_timeout;
	};

	class OpenArg : public File::OpenArg {
	public:
		OpenArg();

		~OpenArg()
		{
		}

		int32_t ParseUrl(const char *url);

		string scheme;
		string host;
		uint16_t port;
		string method;
		string uri;
		string version;
		string user_agent;
		string referer;
		string user;
		string passwd;
		vector<string> header;
		const void *msg_body;
		size_t msg_body_len;
		uint32_t https_flag;
		uint32_t open_flag;
		int32_t accept_encoding_gzip;
		Timeout timeout;
		uint32_t template_option;
		size_t response_header_max_size;
		uint16_t *response_code;
	};

	HttpFile();

	virtual int32_t GetFileType() const;
	virtual int32_t GetCapability() const;
	virtual int64_t GetFileSize() const;
	virtual int32_t Open(const File::OpenArg *param);
	virtual int32_t OpenAsync(const File::OpenArg *param);
	virtual int32_t Close();
	virtual int32_t CloseAsync();
	virtual bool IsOpened() const;
	virtual bool IsEof();
	virtual int32_t Abort();
	virtual int32_t Read(void *buf, size_t nbyte);
	virtual int32_t ReadAsync(void *buf, size_t nbyte);
	virtual int32_t Write(const void *buf, size_t nbyte);
	virtual int32_t WriteAsync(const void *buf, size_t nbyte);
	virtual int64_t Seek(int64_t offset, int32_t whence);
	virtual int32_t SeekAsync(int64_t offset, int32_t whence);
	virtual int32_t Flush();
	virtual int32_t FlushAsync();
	virtual int32_t WaitAsync(int64_t *result);
	virtual int32_t PollAsync(int64_t *result);
	virtual int32_t Getstat(FileStat *stat);
	virtual int32_t SetPriority(int32_t priority);
	virtual ~HttpFile();

private:
	bool m_is_opened;
	thread::RWLock m_lock_rwlock;
	thread::RMutex m_lock_mutex;
	thread::Thread *m_io_thread;
	HttpFileReader *m_http_reader;
	HttpFileReaderParam *m_reader_param;
	int32_t m_reader_result[3];
	common::SharedPtr<HttpFileReaderContext> m_reader_context;
};

}

#endif
