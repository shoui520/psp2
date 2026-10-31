/*
 * paf::BufferedFile: a file read and written through a memory buffer in front
 * of another file (a LocalFile or an HttpFile it opens itself, or any file
 * attached to it). BufferedReader is the read-only kind.
 */
#ifndef PAF_FILE_BUFFEREDFILE_H
#define PAF_FILE_BUFFEREDFILE_H

#include <stddef.h>
#include <stdint.h>
#include <file/file.h>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <thread/thread.h>

namespace paf {

/* A file that works on another file it holds. */
class FileHolder : public File {
public:
	FileHolder()
	{
	}

	virtual int32_t Getstat(FileStat *stat);
	virtual int32_t SetPriority(int32_t priority);

	virtual ~FileHolder()
	{
	}

	/* Holds file, and returns the file held before. */
	virtual common::SharedPtr<File> AttachFile(common::SharedPtr<File> file);
	virtual common::SharedPtr<File> DetachFile();
	virtual common::SharedPtr<File> GetFile() const;

protected:
	common::SharedPtr<File> m_file;
};

/* The buffer's window on the file. */
class BufferedFileState {
public:
	void *m_addr;
	size_t m_capacity;
	size_t m_count;
	int64_t m_buffer_start;
	int64_t m_position;
	int64_t m_file_position;
	int64_t m_file_size;
	bool m_dirty;
};

class BufferedFile : public FileHolder {
public:
	class OpenArg : public File::OpenArg {
	public:
		const char *filename;
		uint32_t flag;
		uint32_t mode;
	};

	BufferedFile();

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
	virtual ~BufferedFile();

	virtual common::SharedPtr<File> AttachFile(common::SharedPtr<File> file);
	virtual int32_t Open(const File::OpenArg *param, common::SharedPtr<Buffer> buffer);

	static common::SharedPtr<BufferedFile> Open(common::SharedPtr<File> file, int32_t *error,
						    common::SharedPtr<Buffer> buffer);

protected:
	thread::RMutex m_lock_mutex;
	common::SharedPtr<Buffer> m_buffer;
	BufferedFileState *m_state;
};

/* A BufferedFile that cannot write. */
class BufferedReader : public BufferedFile {
public:
	virtual int32_t GetFileType() const;
	virtual int32_t GetCapability() const;

	virtual int32_t Write(const void *buf, size_t nbyte)
	{
		return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
	}

	virtual int32_t Flush()
	{
		return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
	}

	virtual ~BufferedReader()
	{
	}

	virtual int32_t Open(const File::OpenArg *param, common::SharedPtr<Buffer> buffer)
	{
		return BufferedFile::Open(param, buffer);
	}
};

}

#endif
