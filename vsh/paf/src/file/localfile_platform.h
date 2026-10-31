/* paf::LocalFilePsp2Impl: LocalFile's implementation on the Vita's file manager. */
#ifndef PAF_FILE_LOCALFILE_PLATFORM_H
#define PAF_FILE_LOCALFILE_PLATFORM_H

#include <file/localfile.h>
#include "../compiler.h"
#include <iofilemgr/iofilemgr.h>

namespace paf {

class LocalFilePsp2Impl : public LocalFileImpl {
public:
	/* What the pending asynchronous call is (m_async_type). */
	enum AsyncType {
		TYPE_NONE = 0,
		TYPE_OPEN = 1,
		TYPE_CLOSE = 2,
		TYPE_READ = 3,
		TYPE_WRITE = 4,
		TYPE_SEEK = 5,
		TYPE_FLUSH = 6
	};

	LocalFilePsp2Impl();
	virtual ~LocalFilePsp2Impl();

	virtual int32_t Open(const char *filename, uint32_t flag, uint32_t mode);
	virtual int32_t Close();
	virtual int32_t Read(void *buf, size_t nbyte);
	virtual int32_t Write(const void *buf, size_t nbyte);
	virtual int64_t Seek(int64_t offset, int32_t whence);
	virtual int32_t Flush();
	virtual int32_t OpenAsync(const char *filename, uint32_t flag, uint32_t mode);
	virtual int32_t CloseAsync();
	virtual int32_t ReadAsync(void *buf, size_t nbyte);
	virtual int32_t WriteAsync(const void *buf, size_t nbyte);
	virtual int32_t SeekAsync(int64_t offset, int32_t whence);
	virtual int32_t FlushAsync();
	virtual int32_t WaitAsync(int64_t *result);
	virtual int32_t PollAsync(int64_t *result);

	virtual bool IsOpened()
	{
		return m_is_opened;
	}

	virtual bool IsEof();
	virtual int32_t Abort();
	virtual int64_t GetFileSize();
	virtual int32_t Getstat(FileStat *stat);
	virtual int32_t SetPriority(int32_t priority);

	static int64_t GetFileSize(const char *path);
	static int32_t RemoveFile(const char *path);
	static int32_t RenameFile(const char *oldname, const char *newname);
	static int32_t Getstat(const char *path, LocalFileStat *stat);
	static int32_t Chstat(const char *path, LocalFileStat *stat, uint32_t cbit);

private:
	bool m_is_opened;
	SceUID m_fd;
	IoAsyncParam m_async;
	SceUID m_as_result;	/* the pending call's event, from sceIo*Async */
	uint32_t m_async_type;
};

}

/* The thread's default I/O priority for the files it opens; no Sony name. */
extern "C" int32_t ScePafMisc_A283AD47(int32_t priority);

#endif
