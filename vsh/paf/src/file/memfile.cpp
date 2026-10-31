#include <file/memfile.h>
#include <std/new>
#include <std/string.h>

namespace paf {

MemFile::MemFile()
	: m_is_opened(false), m_is_writable(false), m_buffer(), m_mem_addr(NULL), m_mem_size(0),
	  m_mem_pos(0), m_waiting_for_sync(false), m_io_result(0),
	  m_lock_mutex("ScePafMemFileCsLock", 0)
{
}

common::SharedPtr<MemFile> MemFile::Open(common::SharedPtr<const Buffer> buffer, int32_t *error)
{
	MemFile *file = new MemFile();

	if (file == NULL) {
		if (error != NULL)
			*error = SCE_PAF_ERROR_FILE_NO_MEMORY;
		return common::SharedPtr<MemFile>();
	}

	OpenArg arg;

	arg.buffer = buffer;

	int32_t ret = file->Open(&arg);

	if (error != NULL)
		*error = ret;
	if (file->IsOpened())
		return common::SharedPtr<MemFile>(file);
	delete file;
	return common::SharedPtr<MemFile>();
}

common::SharedPtr<MemFile> MemFile::Open(const void *addr, size_t size, int32_t *error)
{
	if (addr == NULL) {
		if (error != NULL)
			*error = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		return common::SharedPtr<MemFile>();
	}

	common::SharedPtr<UnmanagedConstBuffer> buffer = UnmanagedConstBuffer::Create(addr, size);

	if (buffer.get() != NULL)
		return Open(common::SharedPtr<const Buffer>(buffer), error);
	if (error != NULL)
		*error = SCE_PAF_ERROR_FILE_NO_MEMORY;
	return common::SharedPtr<MemFile>();
}

common::SharedPtr<MemFile> MemFile::Open(void *addr, size_t size, int32_t *error)
{
	common::SharedPtr<MemFile> file = Open((const void *)addr, size, error);

	if (file.get() != NULL)
		file->m_is_writable = true;
	return file;
}

common::SharedPtr<MemFile> MemFile::Open(common::SharedPtr<Buffer> buffer, int32_t *error)
{
	common::SharedPtr<MemFile> file = Open(common::SharedPtr<const Buffer>(buffer), error);

	if (file.get() != NULL)
		file->m_is_writable = true;
	return file;
}

int32_t MemFile::GetFileType() const
{
	return Type_Mem;
}

int32_t MemFile::GetCapability() const
{
	return 0x7bf7;
}

int64_t MemFile::GetFileSize() const
{
	thread::RMutex &mutex = const_cast<thread::RMutex &>(m_lock_mutex);
	int64_t result;

	mutex.Lock();
	if (IsOpened())
		result = m_mem_size;
	else
		result = SCE_PAF_ERROR_FILE_NOT_OPENED;
	mutex.Unlock();
	return result;
}

int32_t MemFile::Open(const File::OpenArg *param)
{
	int32_t ret;

	m_lock_mutex.Lock();
	ret = OpenAsync(param);
	if (ret == 0) {
		int64_t result;

		ret = WaitAsync(&result);
		if (ret == 0)
			ret = (int32_t)result;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t MemFile::OpenAsync(const File::OpenArg *param)
{
	const OpenArg *arg = static_cast<const OpenArg *>(param);
	thread::RMutexLock lock(&m_lock_mutex);

	if (IsOpened())
		return SCE_PAF_ERROR_FILE_ALREADY_OPENED;
	if (m_waiting_for_sync)
		return SCE_PAF_ERROR_FILE_BUSY;
	if (arg == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	if (GetFileType() != arg->file_type)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;

	common::SharedPtr<const Buffer> buffer = arg->buffer;

	if (buffer.get() == NULL || buffer->GetAddr() == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	m_buffer = buffer;
	m_mem_addr = m_buffer->GetAddr();
	m_mem_size = m_buffer->GetSize();
	m_is_writable = false;
	m_mem_pos = 0;
	m_waiting_for_sync = true;
	m_is_opened = true;
	m_io_result = 0;
	return 0;
}

int32_t MemFile::Close()
{
	int32_t ret;

	m_lock_mutex.Lock();
	ret = CloseAsync();
	if (ret == 0) {
		int64_t result;

		ret = WaitAsync(&result);
		if (ret == 0)
			ret = (int32_t)result;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t MemFile::CloseAsync()
{
	int32_t ret;

	m_lock_mutex.Lock();
	if (!IsOpened()) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (m_waiting_for_sync) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else {
		m_io_result = 0;
		m_waiting_for_sync = true;
		ret = 0;
	}
	m_lock_mutex.Unlock();
	return ret;
}

bool MemFile::IsOpened() const
{
	thread::RMutex &mutex = const_cast<thread::RMutex &>(m_lock_mutex);
	bool result;

	mutex.Lock();
	result = m_is_opened;
	mutex.Unlock();
	return result;
}

bool MemFile::IsEof()
{
	bool result;

	m_lock_mutex.Lock();
	result = m_mem_pos == m_mem_size;
	m_lock_mutex.Unlock();
	return result;
}

int32_t MemFile::Abort()
{
	m_lock_mutex.Lock();
	if (m_waiting_for_sync)
		m_io_result = 0;
	m_waiting_for_sync = false;
	m_lock_mutex.Unlock();
	return 0;
}

int32_t MemFile::Read(void *buf, size_t nbyte)
{
	int32_t ret;

	m_lock_mutex.Lock();
	ret = ReadAsync(buf, nbyte);
	if (ret == 0) {
		int64_t result;

		ret = WaitAsync(&result);
		if (ret == 0)
			ret = (int32_t)result;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t MemFile::ReadAsync(void *buf, size_t nbyte)
{
	m_lock_mutex.Lock();
	if (!IsOpened()) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	if (m_waiting_for_sync) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (buf == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	m_waiting_for_sync = true;
	if (IsEof()) {
		m_io_result = 0;
		m_lock_mutex.Unlock();
		return 0;
	}

	int64_t position = m_mem_pos;
	size_t count = nbyte;

	if (position + nbyte > m_mem_size) {
		int64_t remaining = m_mem_size - position;

		if (remaining < 0 || remaining > nbyte) {
			m_lock_mutex.Unlock();
			return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		}
		count = remaining;
	}
	sce_paf_memcpy(buf, (const char *)m_mem_addr + position, count);
	m_io_result = count;
	m_mem_pos += count;
	m_lock_mutex.Unlock();
	return 0;
}

int32_t MemFile::Write(const void *buf, size_t nbyte)
{
	int32_t ret;

	m_lock_mutex.Lock();
	ret = WriteAsync(buf, nbyte);
	if (ret == 0) {
		int64_t result;

		ret = WaitAsync(&result);
		if (ret == 0)
			ret = (int32_t)result;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t MemFile::WriteAsync(const void *buf, size_t nbyte)
{
	m_lock_mutex.Lock();
	if (!IsOpened()) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	if (m_waiting_for_sync) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (buf == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	if (!m_is_writable) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_PROTECTED;
	}
	m_waiting_for_sync = true;
	if (IsEof()) {
		m_io_result = 0;
		m_lock_mutex.Unlock();
		return 0;
	}

	int64_t position = m_mem_pos;
	size_t count = nbyte;

	if (position + nbyte > m_mem_size) {
		int64_t remaining = m_mem_size - position;

		if (remaining < 0 || remaining > nbyte) {
			m_lock_mutex.Unlock();
			return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		}
		count = remaining;
	}
	/* A writable file's memory came in as a void * or a Buffer. */
	sce_paf_memcpy((char *)m_mem_addr + position, buf, count);
	m_io_result = count;
	m_mem_pos += count;
	m_lock_mutex.Unlock();
	return 0;
}

int64_t MemFile::Seek(int64_t offset, int32_t whence)
{
	int32_t ret;

	m_lock_mutex.Lock();
	ret = SeekAsync(offset, whence);
	if (ret == 0) {
		int64_t result;

		ret = WaitAsync(&result);
		if (ret == 0)
			ret = (int32_t)result;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t MemFile::SeekAsync(int64_t offset, int32_t whence)
{
	int32_t ret = 0;
	int64_t position;

	m_lock_mutex.Lock();
	if (!IsOpened()) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
		goto done;
	}
	if (m_waiting_for_sync) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
		goto done;
	}
	switch (whence) {
	case 0:
		position = offset;
		break;
	case 1:
		position = (int64_t)((uint64_t)m_mem_pos + (uint64_t)offset);
		break;
	case 2:
		position = (int64_t)((uint64_t)m_mem_size + (uint64_t)offset);
		break;
	default:
		ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		goto done;
	}
	if (position < 0) {
		ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		goto done;
	}
	if (position > m_mem_size) {
		ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		goto done;
	}
	m_mem_pos = position;
	m_io_result = position;
	m_waiting_for_sync = true;
done:
	m_lock_mutex.Unlock();
	return ret;
}

int32_t MemFile::Flush()
{
	m_lock_mutex.Lock();
	m_lock_mutex.Unlock();
	return 0;
}

int32_t MemFile::FlushAsync()
{
	m_lock_mutex.Lock();
	m_lock_mutex.Unlock();
	return 0;
}

int32_t MemFile::WaitAsync(int64_t *result)
{
	int32_t ret;

	m_lock_mutex.Lock();
	if (m_waiting_for_sync) {
		int64_t value = m_io_result;

		m_waiting_for_sync = false;
		*result = value;
		ret = 0;
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_BUSY;
	}
	m_lock_mutex.Unlock();
	return ret;
}

int32_t MemFile::PollAsync(int64_t *result)
{
	int32_t ret;

	m_lock_mutex.Lock();
	if (!IsOpened()) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (m_waiting_for_sync) {
		int64_t value = m_io_result;

		m_waiting_for_sync = false;
		*result = value;
		ret = 0;
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_BUSY;
	}
	m_lock_mutex.Unlock();
	return ret;
}

MemFile::~MemFile()
{
}

}
