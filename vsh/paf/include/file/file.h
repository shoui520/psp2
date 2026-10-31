/*
 * paf::File: the interface of paf's files (local, memory, HTTP, buffered).
 * BufferedFile is C++; the local, memory and HTTP files are still C (src/file/),
 * and their headers here are the C++ view of them.
 */
#ifndef PAF_FILE_FILE_H
#define PAF_FILE_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <datetime/datetime.h>
#include <src/paf_types.h>

namespace paf {

class FileStat {
public:
	uint16_t st_mode;
	uint32_t st_attr;
	int64_t st_size;
	datetime::DateTime st_ctime;
	datetime::DateTime st_atime;
	datetime::DateTime st_mtime;
};

class File {
public:
	enum Type {
		Type_None = 0,
		Type_Local = 1,
		Type_Mem = 2,
		Type_Http = 3
	};

	/* What Open() takes; each kind of file extends it. */
	class OpenArg {
	public:
		uint32_t file_type;
	};

	virtual int32_t GetFileType() const = 0;
	virtual int32_t GetCapability() const = 0;
	virtual int64_t GetFileSize() const = 0;
	virtual int32_t Open(const OpenArg *param) = 0;
	virtual int32_t OpenAsync(const OpenArg *param) = 0;
	virtual int32_t Close() = 0;
	virtual int32_t CloseAsync() = 0;
	virtual bool IsOpened() const = 0;
	virtual bool IsEof() = 0;
	virtual int32_t Abort() = 0;
	virtual int32_t Read(void *buf, size_t nbyte) = 0;
	virtual int32_t ReadAsync(void *buf, size_t nbyte) = 0;
	virtual int32_t Write(const void *buf, size_t nbyte) = 0;
	virtual int32_t WriteAsync(const void *buf, size_t nbyte) = 0;
	virtual int64_t Seek(int64_t offset, int32_t whence) = 0;
	virtual int32_t SeekAsync(int64_t offset, int32_t whence) = 0;
	virtual int32_t Flush() = 0;
	virtual int32_t FlushAsync() = 0;
	virtual int32_t WaitAsync(int64_t *result) = 0;
	virtual int32_t PollAsync(int64_t *result) = 0;

	virtual int32_t Getstat(FileStat *stat)
	{
		return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
	}

	virtual int32_t SetPriority(int32_t priority)
	{
		return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
	}

	virtual ~File()
	{
	}
};

}

#endif
