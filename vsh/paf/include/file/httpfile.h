/*
 * paf::HttpFile: a file read over HTTP. Opening it sends the request through a
 * reader (src/file/httpfile_reader.h): a plain one, or one that asks for byte
 * ranges. The asynchronous calls run the reader on a FileIoThread.
 */
#ifndef PAF_FILE_HTTPFILE_H
#define PAF_FILE_HTTPFILE_H

#include <stddef.h>
#include <stdint.h>
#include <file/file.h>
#include <common/shared_ptr.h>
#include <std/string>
#include <std/vector>
#include <thread/thread.h>

namespace paf {

class Url;
class HttpFileReader;
class HttpFileReaderParam;
class HttpFileReaderContext;

class HttpFileStat {
public:
	int32_t st_mode;
	uint32_t st_attr;
	int64_t st_size;
	datetime::DateTime st_ctime;
	datetime::DateTime st_atime;
	datetime::DateTime st_mtime;
};

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
		enum HttpFileOpenFlag {
			SKIP_STATUS_CHECK = 0x800000,
			ENABLE_RANGE_GET = 0x1000000,
			ENABLE_HALF_RANGE_GET = 0x2000000,
			DISABLE_HTTPS_CHECK = 0x4000000,
			IGNORE_PROXY_SETTING = 0x8000000,
			HTTP_OPTION4 = 0x10000000,
			SUPPRESS_WIFI_ACCESS = 0x20000000,
			SUPPRESS_3G_ACCESS = 0x40000000
		};

		enum HttpsFlag {
			HTTPS_FLAG_SERVER_VERIFY = 1,
			HTTPS_FLAG_CLIENT_VERIFY = 2,
			HTTPS_FLAG_CN_CHECK = 4,
			HTTPS_FLAG_NOT_AFTER_CHECK = 8,
			HTTPS_FLAG_NOT_BEFORE_CHECK = 16,
			HTTPS_FLAG_KNOWN_CA_CHECK = 32,
			HTTPS_FLAG_DEFAULT = 63,
			HTTPS_FLAG_MASK = 63
		};

		enum OptionType {
			OptionType_ResolveTimeOut = 1,
			OptionType_ConnectTimeOut = 2,
			OptionType_SendTimeOut = 4,
			OptionType_RecvTimeOut = 8
		};

		OpenArg();

		~OpenArg()
		{
		}

		int32_t ParseUrl(Url *url);
		int32_t ParseUrl(const char *url);
		int32_t ParseUrl(const wchar_t *url);
		int32_t AddRequestHeader(const char *name, const char *value);

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
	virtual ~HttpFile();

	int32_t GetResponseHeader(const char *name, string &value) const;
	int32_t GetResponseCode(uint16_t &code) const;
	int32_t GetResponsePhrase(string &phrase) const;
	int32_t GetResponseVersion(uint8_t &major, uint8_t &minor) const;

	static common::SharedPtr<HttpFile> Open(Url *url, int32_t *error, uint32_t flag);
	static common::SharedPtr<HttpFile> Open(const char *path, uint32_t flag, uint32_t mode,
						int32_t *error);
	static common::SharedPtr<HttpFile> Open(const char *url, int32_t *error, uint32_t flag);
	static common::SharedPtr<HttpFile> Open(const wchar_t *url, int32_t *error, uint32_t flag);
	static int32_t Getstat(const char *url, HttpFileStat *stat);

	int32_t DeleteReader();
	int32_t CreateReader(const OpenArg *arg);

	inline __attribute__((always_inline)) void CopyReaderResult();
	inline __attribute__((always_inline)) int32_t StartIoThread();

	bool m_is_opened;
	thread::RWLock m_lock_rwlock;
	mutable thread::RMutex m_lock_mutex;
	thread::Thread *m_io_thread;
	HttpFileReader *m_http_reader;
	HttpFileReaderParam *m_reader_param;
	int32_t m_reader_result[3];
	common::SharedPtr<HttpFileReaderContext> m_reader_context;
};

}

#endif
