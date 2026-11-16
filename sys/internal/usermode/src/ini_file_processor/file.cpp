/*
 * SceIniFileProcessor: the file the processor reads and writes.
 */

#include "ini_internal.h"

IniFileBase::IniFileBase(IniAllocatorRef *allocator)
{
	IniAllocatorOps *ops;
	IniAllocatorOps *context;

	if (allocator != NULL) {
		ops = allocator->ops;
		context = (IniAllocatorOps *)allocator->context;
	} else {
		ops = iniDefaultAllocator();
		context = iniDefaultAllocator();
	}
	m_path.data = g_iniEmptyString;
	m_path.length = 0;
	m_path.ops = ops;
	m_path.context = context;
	m_mode = 0;
	m_allocator = allocator;
	m_error = 0;
	m_flags = 0;
}

IniFileBase::~IniFileBase()
{
	if (m_path.data != g_iniEmptyString)
		m_path.ops->free(m_path.data, m_path.context);
}

const IniString *IniFileBase::path()
{
	return &m_path;
}

int IniFileBase::openMode()
{
	return m_mode;
}

int IniFileBase::errorCode()
{
	return m_error;
}

void IniFileBase::unused20()
{
}

void IniFileBase::onError(int error)
{
	m_error = error;
}

IniFile::IniFile(IniAllocatorRef *allocator) : IniFileBase(allocator)
{
	m_fd = -1;
	m_reserved = 0;
	iniStringAssignChars(&m_path, NULL, 0);
}

IniFile::~IniFile()
{
	if (m_fd >= 0) {
		close();
		m_fd = -1;
	}
	iniStringAssignChars(&m_path, NULL, 0);
}

int IniFile::open(const char *path, int mode, int flags)
{
	int result = -2138832886;
	int openFlags = 1;
	int perm = 390;

	if ((flags & 0x100000) != 0)
		openFlags = 0x10001;
	if ((flags & 0x200000) != 0)
		perm = (SceUInt16)flags;
	if ((mode & 4) != 0) {
		openFlags = 0x603;
		result = -2138832885;
	} else if ((mode & 1) != 0) {
		openFlags = 0x203;
		result = -2138832885;
	}
	close();
	m_fd = sceIoOpen(path, openFlags, perm);
	if (m_fd >= 0) {
		SceSize length = 0;

		if (path != NULL)
			length = sceClibStrnlen(path, 0x100000);
		iniStringAssignChars(&m_path, path, length);
		m_mode = mode;
		m_flags = flags;
		return 0;
	}
	onError(m_fd);
	if (m_fd == -2147418110)
		return SCE_INI_FILE_PROCESSOR_ERROR_NO_FILE_NAME;
	return result;
}

int IniFile::read(void *buf, SceSize size, SceSSize *readSize)
{
	int result = 0;
	SceSSize got;

	if (m_fd < 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	if (buf == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	if (size == 0) {
		*(char *)buf = 0;
		return 0;
	}
	got = sceIoRead(m_fd, buf, size);
	if (got < 0) {
		onError(got);
		return -2138832884;
	}
	if (readSize != NULL)
		*readSize = got;
	return result;
}

int IniFile::write(const void *buf, SceSize size)
{
	SceSSize put;

	if (m_fd < 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	put = sceIoWrite(m_fd, buf, size);
	if (put >= 0) {
		if ((SceSize)put == size)
			return 0;
		return -2138832883;
	}
	onError(put);
	return -2138832883;
}

int IniFile::seek(SceOff offset, int origin)
{
	int whence;
	int result;

	if (m_fd < 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	if (origin > 1)
		whence = origin <= 2 ? 2 : 0;
	else
		whence = origin <= 0 ? 0 : 1;
	result = (int)sceIoLseek(m_fd, offset, whence);
	if (result < 0)
		onError(result);
	return 0;
}

int IniFile::close()
{
	int result;

	if (m_fd < 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	result = sceIoClose(m_fd);
	if (result < 0)
		onError(result);
	m_fd = -1;
	return 0;
}

int IniFile::sync()
{
	int result;

	if (m_fd < 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	result = sceIoSyncByFd(m_fd, 0);
	if (result < 0)
		onError(result);
	return 0;
}

int IniFile::stat()
{
	SceIoStat stat;
	int result;

	if (m_fd < 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	result = sceIoGetstatByFd(m_fd, &stat);
	if (result >= 0)
		return 0;
	onError(result);
	return -2138832884;
}

int IniFile::rewind()
{
	int result;

	if (m_fd < 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN;
	result = (int)sceIoLseek(m_fd, 0, 0);
	if (result < 0)
		onError(result);
	return 0;
}

SceOff IniFile::size()
{
	SceIoStat stat;
	int result;

	if (m_path.length == 0)
		return 0;
	result = sceIoGetstat(m_path.data, &stat);
	if (result >= 0)
		return stat.st_size;
	onError(result);
	return 0;
}

SceOff IniFile::tell()
{
	SceOff position;

	if (m_fd < 0)
		return 0;
	position = sceIoLseek(m_fd, 0, 1);
	if ((int)position < 0)
		onError((int)position);
	return position;
}

/* a string with nothing in it, made when it is first needed */
struct IniEmptyString : IniString {
	IniEmptyString()
	{
		data = g_iniEmptyString;
		length = 0;
		ops = iniDefaultAllocator();
		context = iniDefaultAllocator();
	}
	~IniEmptyString()
	{
		iniStringDestroy(this);
	}
};

const IniString *IniFile::pathOf()
{
	static IniEmptyString s_empty;

	if (m_fd >= 0)
		return &m_path;
	return &s_empty;
}

int IniFile::unused14()
{
	return 0;
}

int IniFile::unused15()
{
	return -2138832877;
}

/* makes this file the source (a temporary file) by removing it and renaming the source's file to its name */
int IniFile::replace(IniFileBase *source)
{
	const IniString *sourcePath = source->pathOf();
	IniString oldName;
	IniString file;
	IniString part1;
	IniString part2;
	int result;
	int removed;
	int renamed;

	oldName.data = g_iniEmptyString;
	oldName.length = 0;
	oldName.ops = sourcePath->ops;
	oldName.context = sourcePath->context;
	iniStringAssign(&oldName, sourcePath);
	file.data = g_iniEmptyString;
	file.length = 0;
	file.ops = m_path.ops;
	file.context = m_path.context;
	iniStringAssign(&file, &m_path);
	if (oldName.length != 0) {
		SceSize oldDevice = iniPathDeviceLength(oldName.data);
		SceSize fileDevice = iniPathDeviceLength(file.data);

		if (fileDevice != 0) {
			bool madeFirst = false;
			bool madeSecond = false;

			if (fileDevice == oldDevice) {
				iniStringSubstr(&part1, &file, 0, fileDevice);
				madeSecond = true;
				iniStringSubstr(&part2, &oldName, 0, oldDevice);
				madeFirst = true;
				if (*part1.data == *part2.data && sceClibStrcmp(part1.data, part2.data) == 0)
					madeSecond = madeSecond;
			}
			if (madeFirst || madeSecond) {
				if (part2.data != g_iniEmptyString)
					part2.ops->free(part2.data, part2.context);
			}
			if (madeSecond && part1.data != g_iniEmptyString)
				part1.ops->free(part1.data, part1.context);
		}
		close();
		source->close();
		removed = sceIoRemove(file.data);
		if (removed >= 0) {
			renamed = sceIoRename(oldName.data, file.data);
			if (renamed >= 0) {
				result = reopen();
				if (result == 0)
					sceIoRemove(oldName.data);
			} else {
				onError(renamed);
				result = -2138832883;
			}
		} else {
			onError(removed);
			result = -2138832885;
		}
	} else {
		result = SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	}
	if (file.data != g_iniEmptyString)
		file.ops->free(file.data, file.context);
	if (oldName.data != g_iniEmptyString)
		oldName.ops->free(oldName.data, oldName.context);
	return result;
}

int IniFile::reopen()
{
	return open(m_path.data, 3, m_flags);
}

