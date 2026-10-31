/*
 * paf::BufferedFile: reads and writes go through m_state's window on the file
 * (m_buffer's memory); the window is written back when dirty before the file
 * itself is read, written or moved.
 */
#include <file/bufferedfile.h>
#include <file/localfile.h>
#include <file/httpfile.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <kernel/iofilemgr.h>

namespace paf {

/* the file types GetFileType gives */
#define BUFFERED_FILE_TYPE	17
#define BUFFERED_READER_TYPE	18

/* the default buffer */
#define BUFFERED_FILE_BUFFER_ALIGN	64
#define BUFFERED_FILE_BUFFER_SIZE	4096

int64_t BufferedFile::GetFileSize() const
{
	if (m_file.get() == NULL)
		return SCE_PAF_ERROR_FILE_PROTECTED;
	return m_state->m_file_size;
}

int32_t BufferedFile::GetFileType() const
{
	return BUFFERED_FILE_TYPE;
}

int32_t BufferedFile::GetCapability() const
{
	thread::RMutex *mutex = const_cast<thread::RMutex *>(&m_lock_mutex);
	int32_t capability = 63;

	mutex->Lock();
	if (m_file.get() != NULL)
		capability = m_file->GetCapability() & 63;
	mutex->Unlock();
	return capability;
}

int32_t BufferedFile::Open(const File::OpenArg *param)
{
	return Open(param, common::SharedPtr<Buffer>());
}

int32_t BufferedFile::OpenAsync(const File::OpenArg *param)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

/* Writes the window to the file. */
static __attribute__((noinline)) int32_t WriteBuffer(common::SharedPtr<File> file, BufferedFileState *state)
{
	if (file.get() == NULL)
		return 0;
	if (state->m_count != 0) {
		if (file->Seek(state->m_buffer_start, SCE_SEEK_SET) != state->m_buffer_start)
			return SCE_PAF_ERROR_FILE_INVALID_OFFSET;

		size_t remaining = state->m_count;
		const uint8_t *addr = (const uint8_t *)state->m_addr;

		while (remaining != 0) {
			int32_t written = file->Write(addr, remaining);

			if (written < 0)
				return written;
			if (written == 0 || (uint32_t)written > remaining)
				return SCE_PAF_ERROR_FILE_INVALID_OFFSET;
			remaining -= written;
			addr += written;
		}
		state->m_file_position = state->m_buffer_start + state->m_count;
	}
	state->m_dirty = false;
	return 0;
}

static int32_t FlushDirty(common::SharedPtr<File> file, BufferedFileState *state)
{
	if (!state->m_dirty)
		return 0;
	return WriteBuffer(file, state);
}

/* Writes the window back and empties it at the file's position. */
static void ResetBuffer(common::SharedPtr<File> file, BufferedFileState *state)
{
	FlushDirty(file, state);
	state->m_count = 0;
	state->m_buffer_start = 0;
	state->m_position = 0;
	state->m_file_position = 0;
	state->m_file_size = 0;
	if (file.get() != NULL) {
		int64_t position = file->Seek(0, SCE_SEEK_CUR);

		if (position >= 0) {
			state->m_buffer_start = position;
			state->m_position = position;
			state->m_file_position = position;
		}

		int64_t size = file->GetFileSize();

		if (size >= 0)
			state->m_file_size = size;
	}
}

int32_t BufferedFile::Close()
{
	int32_t result = SCE_PAF_ERROR_FILE_PROTECTED;

	m_lock_mutex.Lock();

	BufferedFileState *state = m_state;

	if (m_file.get() != NULL) {
		FlushDirty(m_file, state);
		result = m_file->Close();
	}
	ResetBuffer(common::SharedPtr<File>(), state);
	m_lock_mutex.Unlock();
	return result;
}

int32_t BufferedFile::CloseAsync()
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

bool BufferedFile::IsOpened() const
{
	thread::RMutex *mutex = const_cast<thread::RMutex *>(&m_lock_mutex);

	mutex->Lock();
	if (m_file.get() != NULL) {
		bool opened = m_file->IsOpened();

		mutex->Unlock();
		return opened;
	}
	mutex->Unlock();
	return false;
}

bool BufferedFile::IsEof()
{
	m_lock_mutex.Lock();
	if (m_file.get() != NULL) {
		BufferedFileState *state = m_state;
		bool eof;

		if (state->m_position == state->m_file_position) {
			eof = m_file->IsEof();
		} else if (state->m_position <= state->m_file_position) {
			eof = false;
		} else {
			ResetBuffer(m_file, state);
			eof = m_file->IsEof();
		}
		m_lock_mutex.Unlock();
		return eof;
	}
	m_lock_mutex.Unlock();
	return true;
}

int32_t BufferedFile::Abort()
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFile::Read(void *buf, size_t nbyte)
{
	thread::RMutexLock lock(&m_lock_mutex);

	if (buf == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	if (m_file.get() == NULL)
		return SCE_PAF_ERROR_FILE_PROTECTED;
	if (!m_file->IsOpened())
		return SCE_PAF_ERROR_FILE_NOT_OPENED;

	BufferedFileState *state = m_state;
	int64_t offset = state->m_position - state->m_buffer_start;
	int64_t remaining = state->m_count - offset;

	if (remaining < 0)
		return SCE_PAF_ERROR_FILE_INVALID_OFFSET;

	const void *src = (const uint8_t *)state->m_addr + (uint32_t)offset;
	size_t available = remaining;

	if (nbyte <= (uint32_t)remaining) {
		sce_paf_memcpy(buf, src, nbyte);
		state->m_position += nbyte;
		return nbyte;
	}

	int32_t result = FlushDirty(m_file, state);

	if (result != 0)
		return result;

	size_t copied = 0;

	if (available != 0) {
		sce_paf_memcpy(buf, src, available);
		state->m_position += available;
		copied = available;
		buf = (uint8_t *)buf + available;
		nbyte -= available;
	}
	if (nbyte > state->m_capacity) {
		result = m_file->Read(buf, nbyte);
		if (result < 0)
			return result;

		int64_t position = state->m_file_position + result;

		state->m_count = 0;
		state->m_file_position = position;
		state->m_position = position;
		state->m_buffer_start = position;
		return (uint32_t)result + copied;
	}
	result = m_file->Read(state->m_addr, state->m_capacity);
	if (result < 0)
		return result;

	size_t received = nbyte;

	if ((uint32_t)result < nbyte)
		received = result;

	int64_t position = state->m_file_position;

	src = state->m_addr;
	state->m_buffer_start = position;
	state->m_count = result;
	state->m_file_position = position + result;
	state->m_position = position + received;
	sce_paf_memcpy(buf, src, received);
	return copied + received;
}

int32_t BufferedFile::ReadAsync(void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFile::Write(const void *buf, size_t nbyte)
{
	thread::RMutexLock lock(&m_lock_mutex);

	if (buf == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	if (m_file.get() == NULL)
		return SCE_PAF_ERROR_FILE_PROTECTED;
	if (!m_file->IsOpened())
		return SCE_PAF_ERROR_FILE_NOT_OPENED;

	BufferedFileState *state = m_state;
	int64_t offset = state->m_position - state->m_buffer_start;
	int64_t remaining = state->m_capacity - offset;

	if (remaining < 0)
		return SCE_PAF_ERROR_FILE_INVALID_OFFSET;
	if (nbyte <= (uint32_t)remaining) {
		sce_paf_memcpy((uint8_t *)state->m_addr + (uint32_t)offset, buf, nbyte);

		int64_t position = state->m_position + nbyte;
		int64_t start = state->m_buffer_start;

		state->m_position = position;

		int64_t size = state->m_file_size;

		if (position > start + state->m_count)
			state->m_count = position - start;
		state->m_dirty = true;
		if (size < position)
			size = position;
		state->m_file_size = size;
		return nbyte;
	}

	int32_t result = FlushDirty(m_file, state);

	if (result != 0)
		return result;
	if (state->m_position != state->m_file_position) {
		int64_t position = m_file->Seek(state->m_position, SCE_SEEK_SET);

		if (position < 0)
			return position;
		state->m_file_position = position;
	}
	if (nbyte > state->m_capacity) {
		result = m_file->Write(buf, nbyte);
		if (result < 0)
			return result;

		int64_t size = state->m_file_size;
		int64_t position = state->m_file_position + result;

		state->m_file_position = position;
		state->m_position = position;
		if (size < position)
			size = position;
		state->m_file_size = size;
		state->m_dirty = false;
		state->m_count = 0;
		state->m_buffer_start = position;
		return result;
	}
	sce_paf_memcpy(state->m_addr, buf, nbyte);

	int64_t position = state->m_file_position;

	state->m_buffer_start = position;
	state->m_count = nbyte;

	int64_t size = state->m_file_size;

	state->m_dirty = true;
	state->m_position = position + nbyte;
	if (size < position + nbyte)
		size = position + nbyte;
	state->m_file_size = size;
	return nbyte;
}

int32_t BufferedFile::WriteAsync(const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int64_t BufferedFile::Seek(int64_t offset, int32_t whence)
{
	thread::RMutexLock lock(&m_lock_mutex);

	if (m_file.get() == NULL)
		return SCE_PAF_ERROR_FILE_PROTECTED;
	if (!m_file->IsOpened())
		return SCE_PAF_ERROR_FILE_NOT_OPENED;

	BufferedFileState *state = m_state;
	int64_t position = 0;

	if (whence != SCE_SEEK_END) {
		position = offset;
		if (whence == SCE_SEEK_CUR)
			position += state->m_position;
		if (position < 0)
			return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		if (position >= state->m_buffer_start &&
		    position <= state->m_buffer_start + state->m_count) {
			state->m_position = position;
			return position;
		}
	}

	int32_t error = FlushDirty(m_file, state);

	if (error != 0)
		return error;
	if (whence == SCE_SEEK_CUR)
		offset = position - state->m_file_position;

	int64_t result = m_file->Seek(offset, whence);

	if (result >= 0) {
		int64_t size = state->m_file_size;

		state->m_count = 0;
		state->m_file_position = result;
		state->m_position = result;
		state->m_buffer_start = result;
		if (size < result)
			size = result;
		state->m_file_size = size;
	}
	return result;
}

int32_t BufferedFile::SeekAsync(int64_t offset, int32_t whence)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFile::Flush()
{
	m_lock_mutex.Lock();
	if (m_file.get() == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_PROTECTED;
	}
	if (!m_file->IsOpened()) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}

	BufferedFileState *state = m_state;
	int32_t result = FlushDirty(m_file, state);

	m_file->Flush();
	m_lock_mutex.Unlock();
	return result;
}

int32_t BufferedFile::FlushAsync()
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFile::WaitAsync(int64_t *result)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFile::PollAsync(int64_t *result)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

common::SharedPtr<File> BufferedFile::AttachFile(common::SharedPtr<File> file)
{
	m_lock_mutex.Lock();
	if (file.get() != NULL)
		ResetBuffer(file, m_state);

	common::SharedPtr<File> previous = FileHolder::AttachFile(file);

	m_lock_mutex.Unlock();
	return previous;
}

static common::SharedPtr<Buffer> CreateDefaultBuffer()
{
	common::SharedPtr<Buffer> buffer;
	void *addr = sce_paf_memalign(BUFFERED_FILE_BUFFER_ALIGN, BUFFERED_FILE_BUFFER_SIZE);

	if (addr != NULL)
		buffer = HeapAllocBuffer::Create(addr, BUFFERED_FILE_BUFFER_SIZE, NULL);
	return buffer;
}

int32_t BufferedFile::Open(const File::OpenArg *param, common::SharedPtr<Buffer> buffer)
{
	m_lock_mutex.Lock();
	if (IsOpened()) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_ALREADY_OPENED;
	}
	if (param == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}

	common::SharedPtr<File> file;

	if (param->file_type == File::Type_Local &&
	    (static_cast<const OpenArg *>(param)->flag & SCE_O_APPEND) == 0) {
		file = common::SharedPtr<LocalFile>(new LocalFile());
	} else if (param->file_type == File::Type_Http) {
		file = common::SharedPtr<HttpFile>(new HttpFile());
	} else {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
	}
	if (file.get() == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}
	if (buffer.get() == NULL)
		buffer = CreateDefaultBuffer();
	if (buffer.get() == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t result = file->Open(param);

	if (result != 0) {
		m_lock_mutex.Unlock();
		return result;
	}
	m_buffer = buffer;

	BufferedFileState *state = m_state;

	state->m_addr = buffer->GetAddr();
	state->m_capacity = buffer->GetSize();
	state->m_count = 0;
	state->m_buffer_start = 0;
	state->m_position = 0;
	state->m_file_position = 0;
	state->m_file_size = 0;
	state->m_dirty = false;
	AttachFile(file);
	m_lock_mutex.Unlock();
	return 0;
}

BufferedFile::BufferedFile() : m_lock_mutex("ScePafBufferedReaderCsLock", 0)
{
	m_state = new BufferedFileState;
}

common::SharedPtr<BufferedFile> BufferedFile::Open(common::SharedPtr<File> file, int32_t *error,
						   common::SharedPtr<Buffer> buffer)
{
	BufferedFile *self = NULL;

	if (buffer.get() == NULL)
		buffer = CreateDefaultBuffer();
	if (buffer.get() != NULL) {
		self = new BufferedFile();
		if (self != NULL) {
			BufferedFileState *state = self->m_state;

			if (state != NULL) {
				state->m_addr = buffer->GetAddr();
				state->m_capacity = buffer->GetSize();
				state->m_count = 0;
				state->m_buffer_start = 0;
				state->m_position = 0;
				state->m_file_position = 0;
				state->m_file_size = 0;
				state->m_dirty = false;
				self->m_buffer = buffer;
				self->AttachFile(file);
			} else {
				delete self;
				self = NULL;
			}
		}
	}
	if (error != NULL)
		*error = self != NULL ? 0 : SCE_PAF_ERROR_FILE_NO_MEMORY;
	return common::SharedPtr<BufferedFile>(self);
}

BufferedFile::~BufferedFile()
{
	m_lock_mutex.Lock();
	if (m_file.get() != NULL) {
		bool opened = m_file->IsOpened();

		m_lock_mutex.Unlock();
		if (opened)
			Close();
	} else {
		m_lock_mutex.Unlock();
	}
	delete m_state;
}

int32_t BufferedReader::GetFileType() const
{
	return BUFFERED_READER_TYPE;
}

int32_t BufferedReader::GetCapability() const
{
	thread::RMutex *mutex = const_cast<thread::RMutex *>(&m_lock_mutex);

	mutex->Lock();

	int32_t capability = BufferedFile::GetCapability();

	mutex->Unlock();
	return capability & 0x17;
}

int32_t FileHolder::Getstat(FileStat *stat)
{
	if (m_file.get() == NULL)
		return SCE_PAF_ERROR_FILE_PROTECTED;
	return m_file->Getstat(stat);
}

int32_t FileHolder::SetPriority(int32_t priority)
{
	if (m_file.get() == NULL)
		return SCE_PAF_ERROR_FILE_PROTECTED;
	return m_file->SetPriority(priority);
}

common::SharedPtr<File> FileHolder::AttachFile(common::SharedPtr<File> file)
{
	common::SharedPtr<File> previous = m_file;

	m_file = file;
	return previous;
}

common::SharedPtr<File> FileHolder::DetachFile()
{
	return AttachFile(common::SharedPtr<File>());
}

common::SharedPtr<File> FileHolder::GetFile() const
{
	return m_file;
}

}
