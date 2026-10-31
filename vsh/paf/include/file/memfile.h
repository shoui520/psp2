/* paf::MemFile: a file over a Buffer's memory; its asynchronous calls complete at once. */
#ifndef PAF_FILE_MEMFILE_H
#define PAF_FILE_MEMFILE_H

#include <file/file.h>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <thread/thread.h>

namespace paf {

class MemFile : public File {
public:
	class OpenArg : public File::OpenArg {
	public:
		OpenArg()
		{
			file_type = Type_Mem;
		}

		~OpenArg()
		{
		}

		common::SharedPtr<const Buffer> buffer;
	};

	MemFile();

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
	virtual ~MemFile();

	/* Read only over buffer or [addr, addr + size); the void * and Buffer kinds are writable. */
	static common::SharedPtr<MemFile> Open(common::SharedPtr<const Buffer> buffer, int32_t *error);
	static common::SharedPtr<MemFile> Open(const void *addr, size_t size, int32_t *error);
	static common::SharedPtr<MemFile> Open(void *addr, size_t size, int32_t *error);
	static common::SharedPtr<MemFile> Open(common::SharedPtr<Buffer> buffer, int32_t *error);

private:
	bool m_is_opened;
	bool m_is_writable;
	common::SharedPtr<const Buffer> m_buffer;
	const void *m_mem_addr;
	size_t m_mem_size;
	int64_t m_mem_pos;
	bool m_waiting_for_sync;	/* an asynchronous call's result waits for WaitAsync */
	int64_t m_io_result;
	thread::RMutex m_lock_mutex;
};

}

#endif
