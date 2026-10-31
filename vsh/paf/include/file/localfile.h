/* paf::LocalFile: a file of the file system (C view in src/file/localfile.h). */
#ifndef PAF_FILE_LOCALFILE_H
#define PAF_FILE_LOCALFILE_H

#include <file/file.h>
#include <common/shared_ptr.h>
#include <thread/thread.h>

namespace paf {

class LocalFileImpl;

namespace memory {
class HeapAllocator;
}

class LocalFile : public File {
public:
	class OpenArg : public File::OpenArg {
	public:
		const char *filename;
		uint32_t flag;
		uint32_t mode;
	};

	LocalFile();

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
	virtual ~LocalFile();

	static common::SharedPtr<LocalFile> Open(const char *path, uint32_t flag, uint32_t mode,
						 int32_t *error);

private:
	thread::RMutex m_lock_mutex;
	LocalFileImpl *m_pimpl;
	memory::HeapAllocator *m_allocator;
};

}

#endif
