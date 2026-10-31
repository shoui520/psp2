#include <file/localfile.h>
#include <memory/heap_allocator.h>
#include <std/new>
#include "localfile_platform.h"

/* GetGlobalHeapAllocator, exported by its C name. */
extern "C" paf::memory::HeapAllocator *ScePafStdc_261CAF72(void);

namespace paf {

int32_t LocalFile::Getstat(const char *path, LocalFileStat *stat)
{
	return LocalFilePsp2Impl::Getstat(path, stat);
}

void LocalFile::CreateImpl()
{
	if (m_allocator != NULL) {
		void *buffer = m_allocator->sce_paf_memalign(8, sizeof(LocalFilePsp2Impl));

		if (buffer != NULL)
			m_pimpl = new (buffer) LocalFilePsp2Impl();
	}
}

LocalFile::LocalFile() : m_lock_mutex("ScePafLocalFileCsLock", 0)
{
	m_pimpl = NULL;
	m_allocator = ScePafStdc_261CAF72();
	CreateImpl();
}

common::SharedPtr<File> LocalFile::Open(const char *path, uint32_t flag, uint32_t mode,
					int32_t *error)
{
	LocalFile *file = new LocalFile();

	if (file == NULL) {
		if (error != NULL)
			*error = SCE_PAF_ERROR_FILE_NO_MEMORY;
		return common::SharedPtr<File>();
	}

	OpenArg arg;

	arg.file_type = Type_Local;
	arg.filename = path;
	arg.flag = flag;
	arg.mode = mode;

	int32_t ret = file->Open(&arg);

	if (error != NULL)
		*error = ret;
	if (file->IsOpened())
		return common::SharedPtr<File>(file);
	delete file;
	return common::SharedPtr<File>();
}

int32_t LocalFile::GetFileType() const
{
	return Type_Local;
}

int32_t LocalFile::GetCapability() const
{
	return 0x7fff;
}

int64_t LocalFile::GetFileSize() const
{
	if (m_pimpl == NULL)
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	return m_pimpl->GetFileSize();
}

int32_t LocalFile::Open(const File::OpenArg *param)
{
	m_lock_mutex.Lock();
	if (param == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	if (GetFileType() != param->file_type) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}

	const OpenArg *arg = (const OpenArg *)param;
	const char *filename = arg->filename;
	uint32_t flag = arg->flag;
	uint32_t mode = arg->mode;

	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->Open(filename, flag, mode);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::OpenAsync(const File::OpenArg *param)
{
	m_lock_mutex.Lock();
	if (param == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	if (GetFileType() != param->file_type) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}

	const OpenArg *arg = (const OpenArg *)param;
	const char *filename = arg->filename;
	uint32_t flag = arg->flag;
	uint32_t mode = arg->mode;

	if (IsOpened()) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_ALREADY_OPENED;
	}
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->OpenAsync(filename, flag, mode);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::Close()
{
	m_lock_mutex.Lock();
	if (IsOpened()) {
		if (m_pimpl == NULL) {
			m_lock_mutex.Unlock();
			return SCE_PAF_ERROR_FILE_NO_MEMORY;
		}

		int32_t ret = m_pimpl->Close();

		m_lock_mutex.Unlock();
		return ret;
	}
	m_lock_mutex.Unlock();
	return SCE_PAF_ERROR_FILE_NOT_OPENED;
}

int32_t LocalFile::CloseAsync()
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->CloseAsync();

	m_lock_mutex.Unlock();
	return ret;
}

bool LocalFile::IsOpened() const
{
	thread::RMutex *mutex = const_cast<thread::RMutex *>(&m_lock_mutex);

	mutex->Lock();
	if (m_pimpl == NULL) {
		mutex->Unlock();
		return false;
	}

	bool ret = m_pimpl->IsOpened();

	mutex->Unlock();
	return ret;
}

bool LocalFile::IsEof()
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return false;
	}

	bool ret = m_pimpl->IsEof();

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::Abort()
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->Abort();

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::Read(void *buf, size_t nbyte)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->Read(buf, nbyte);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::ReadAsync(void *buf, size_t nbyte)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->ReadAsync(buf, nbyte);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::Write(const void *buf, size_t nbyte)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->Write(buf, nbyte);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::WriteAsync(const void *buf, size_t nbyte)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->WriteAsync(buf, nbyte);

	m_lock_mutex.Unlock();
	return ret;
}

int64_t LocalFile::Seek(int64_t offset, int32_t whence)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int64_t ret = m_pimpl->Seek(offset, whence);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::SeekAsync(int64_t offset, int32_t whence)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->SeekAsync(offset, whence);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::Flush()
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->Flush();

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::FlushAsync()
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->FlushAsync();

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::WaitAsync(int64_t *result)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->WaitAsync(result);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::PollAsync(int64_t *result)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->PollAsync(result);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::Getstat(FileStat *stat)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->Getstat(stat);

	m_lock_mutex.Unlock();
	return ret;
}

int32_t LocalFile::SetPriority(int32_t priority)
{
	m_lock_mutex.Lock();
	if (m_pimpl == NULL) {
		m_lock_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}

	int32_t ret = m_pimpl->SetPriority(priority);

	m_lock_mutex.Unlock();
	return ret;
}

LocalFile::LocalFile(memory::HeapAllocator *allocator) : m_lock_mutex("ScePafLocalFileCsLock", 0)
{
	m_allocator = allocator;
	m_pimpl = NULL;
	CreateImpl();
}

void LocalFile::DeleteImpl()
{
	if (m_pimpl != NULL) {
		m_pimpl->~LocalFileImpl();
		if (m_allocator != NULL)
			m_allocator->sce_paf_free(m_pimpl);
		m_pimpl = NULL;
	}
}

LocalFile::~LocalFile()
{
	WaitAsync(NULL);
	Close();
	DeleteImpl();
}

common::SharedPtr<File> LocalFile::OpenUrl(const char *path, uint32_t flag, uint32_t mode,
					   int32_t *error, bool *ignored)
{
	if (ignored == NULL || *ignored)
		return common::SharedPtr<File>();
	return Open(path, flag, mode, error);
}

int64_t LocalFile::GetFileSize(const char *path)
{
	return LocalFilePsp2Impl::GetFileSize(path);
}

int32_t LocalFile::RemoveFile(const char *path)
{
	return LocalFilePsp2Impl::RemoveFile(path);
}

int32_t LocalFile::RenameFile(const char *oldname, const char *newname)
{
	return LocalFilePsp2Impl::RenameFile(oldname, newname);
}

int32_t LocalFile::Chstat(const char *path, LocalFileStat *stat, uint32_t cbit)
{
	return LocalFilePsp2Impl::Chstat(path, stat, cbit);
}

/* Whether path's status has any of mode's bits; no Sony name. */
static bool FileHasMode(const char *path, uint32_t mode)
{
	LocalFileStat stat;

	if (LocalFilePsp2Impl::Getstat(path, &stat) != 0)
		return false;
	return (stat.st_mode & mode) != 0;
}

bool LocalFile::Exists(const char *path)
{
	return FileHasMode(path, SCE_STM_FREG | SCE_STM_FDIR);
}

bool LocalFile::IsFile(const char *path)
{
	return FileHasMode(path, SCE_STM_FREG);
}

bool LocalFile::IsDirectory(const char *path)
{
	return FileHasMode(path, SCE_STM_FDIR);
}

int32_t LocalFile::SetThreadDefaultPriority(int32_t priority)
{
	return LocalFilePsp2Impl::SetThreadDefaultPriority(priority);
}

}

