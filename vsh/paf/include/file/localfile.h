/* paf::LocalFile: a file of the file system, through its platform's LocalFileImpl. */
#ifndef PAF_FILE_LOCALFILE_H
#define PAF_FILE_LOCALFILE_H

#include <file/file.h>
#include <common/shared_ptr.h>
#include <thread/thread.h>

namespace paf {

namespace memory {
class HeapAllocator;
}

/* A path's status: what LocalFile::Getstat and Chstat take. */
class LocalFileStat {
public:
	LocalFileStat()
		: st_ctime(0, 0, 0, 0, 0, 0, 0), st_atime(0, 0, 0, 0, 0, 0, 0),
		  st_mtime(0, 0, 0, 0, 0, 0, 0)
	{
	}

	~LocalFileStat()
	{
	}

	uint16_t st_mode;
	uint32_t st_attr;
	int64_t st_size;
	datetime::DateTime st_ctime;
	datetime::DateTime st_atime;
	datetime::DateTime st_mtime;
};

/* The platform's side of a LocalFile (LocalFilePsp2Impl on the Vita). */
class LocalFileImpl {
public:
	virtual ~LocalFileImpl()
	{
	}

	virtual int32_t Open(const char *filename, uint32_t flag, uint32_t mode) = 0;
	virtual int32_t Close() = 0;
	virtual int32_t Read(void *buf, size_t nbyte) = 0;
	virtual int32_t Write(const void *buf, size_t nbyte) = 0;
	virtual int64_t Seek(int64_t offset, int32_t whence) = 0;
	virtual int32_t Flush() = 0;
	virtual int32_t OpenAsync(const char *filename, uint32_t flag, uint32_t mode) = 0;
	virtual int32_t CloseAsync() = 0;
	virtual int32_t ReadAsync(void *buf, size_t nbyte) = 0;
	virtual int32_t WriteAsync(const void *buf, size_t nbyte) = 0;
	virtual int32_t SeekAsync(int64_t offset, int32_t whence) = 0;
	virtual int32_t FlushAsync() = 0;
	virtual int32_t WaitAsync(int64_t *result) = 0;
	virtual int32_t PollAsync(int64_t *result) = 0;
	virtual bool IsOpened() = 0;
	virtual bool IsEof() = 0;
	virtual int32_t Abort() = 0;
	virtual int64_t GetFileSize() = 0;
	virtual int32_t Getstat(FileStat *stat) = 0;
	virtual int32_t SetPriority(int32_t priority) = 0;
};

class LocalFile : public File {
public:
	class OpenArg : public File::OpenArg {
	public:
		const char *filename;
		uint32_t flag;
		uint32_t mode;
	};

	LocalFile();
	LocalFile(memory::HeapAllocator *allocator);

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

	static common::SharedPtr<File> Open(const char *path, uint32_t flag, uint32_t mode,
					    int32_t *error);
	/* An OpenUrl::OpenFunc: opens path unless *ignored is already set. */
	static common::SharedPtr<File> OpenUrl(const char *path, uint32_t flag, uint32_t mode,
					       int32_t *error, bool *ignored);
	static int32_t Getstat(const char *path, LocalFileStat *stat);
	static int64_t GetFileSize(const char *path);
	static int32_t RemoveFile(const char *path);
	static int32_t RenameFile(const char *oldname, const char *newname);
	static int32_t Chstat(const char *path, LocalFileStat *stat, uint32_t cbit);
	static bool Exists(const char *path);
	static bool IsFile(const char *path);
	static bool IsDirectory(const char *path);
	static int32_t SetThreadDefaultPriority(int32_t priority);

private:
	void CreateImpl();
	void DeleteImpl();

	thread::RMutex m_lock_mutex;
	LocalFileImpl *m_pimpl;
	memory::HeapAllocator *m_allocator;
};

}

#endif
