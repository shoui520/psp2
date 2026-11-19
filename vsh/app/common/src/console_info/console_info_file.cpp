/*
 * A console information file: opened on ur0:ci/file/ (made on first use) for the records
 * written to it, one lock for all of them.
 */

#include <scetypes.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <iofilemgr/iofilemgr_common.h>
#include <std/string.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <thread/thread.h>

#include "console_info_file.h"

#define CONSOLE_INFO_DIR		"ur0:ci/file/"
#define CONSOLE_INFO_DIR_LENGTH		12
#define CONSOLE_INFO_NAME_LENGTH	6	/* "1.dat" and its NUL */
/* LocalFile::Open's error for a path whose folder is missing (SCE_PAF_ERROR_FILE_NO_ENTRY) */
#define CONSOLE_INFO_ERROR_NO_ENTRY	((int)0x80AF5022)
/* the files' permissions: the system may read and write them */
#define CONSOLE_INFO_FILE_MODE		IO_STM_RWS

static bool s_consoleInfoDirMade = false;
static paf::thread::Mutex s_consoleInfoLock("SceConsoleInfo", 0);

ConsoleInfoFile::ConsoleInfoFile(const char *name, SceUInt8 recordSize)
{
	m_recordSize = recordSize;
	m_name = name;
	m_openCount = 0;
	m_keptOpen = 0;
}

int ConsoleInfoFile::Close()
{
	int ret;

	if (m_file.get() == NULL)
		return SCE_ERROR_ERRNO_ESTALE;
	if (m_openCount > 1) {
		m_openCount--;
		return 0;
	}
	ret = m_file->Close();
	m_file.reset();
	return ret;
}

ConsoleInfoFile::~ConsoleInfoFile()
{
	if (m_file.get() != NULL)
		Close();
}

int ConsoleInfoFile::Open()
{
	char path[CONSOLE_INFO_PATH_MAX + 1];
	int error;

	if (m_file.get() != NULL) {
		m_openCount++;
		return 0;
	}
	GetPath(path);
	error = 0;
	m_file = paf::LocalFile::Open(path, SCE_O_CREAT | SCE_O_RDWR, CONSOLE_INFO_FILE_MODE,
		&error);
	if (error == CONSOLE_INFO_ERROR_NO_ENTRY) {
		paf::Dir::CreateRecursive(CONSOLE_INFO_DIR);
		error = 0;
		m_file.reset();
		m_file = paf::LocalFile::Open(path, SCE_O_CREAT | SCE_O_RDWR,
			CONSOLE_INFO_FILE_MODE, &error);
	}
	if (error != 0) {
		m_file.reset();
		return error;
	}
	return 0;
}

/* ur0:ci/file/, made once */
static inline void makeConsoleInfoDir(void)
{
	paf::thread::MutexLock lock(&s_consoleInfoLock);

	if (s_consoleInfoDirMade)
		return;
	paf::Dir::CreateRecursive(CONSOLE_INFO_DIR);
	s_consoleInfoDirMade = true;
}

int ConsoleInfoFile::Write(int index, const void *data, int mode)
{
	int ret;

	makeConsoleInfoDir();
	paf::thread::MutexLock lock(&s_consoleInfoLock);
	switch (mode) {
	case CONSOLE_INFO_WRITE_ONCE:
		ret = Open();
		break;
	case CONSOLE_INFO_WRITE_KEEP_OPEN:
	case CONSOLE_INFO_WRITE_LAST:
		if (m_keptOpen == 1)
			goto write;
		ret = Open();
		m_keptOpen = 1;
		break;
	default:
		ret = SCE_ERROR_ERRNO_EINVAL;
		break;
	}
	if (ret < 0)
		return ret;
write:
	if (m_file.get() == NULL) {
		ret = SCE_ERROR_ERRNO_ESTALE;
	} else {
		ret = m_file->Seek((SceInt64)(index * m_recordSize), SCE_SEEK_SET);
		if (ret >= 0) {
			int written = m_file->Write(data, m_recordSize);

			ret = written < 0 ? written : 0;
		}
	}
	switch (mode) {
	case CONSOLE_INFO_WRITE_ONCE:
		Close();
		break;
	case CONSOLE_INFO_WRITE_KEEP_OPEN:
		break;
	case CONSOLE_INFO_WRITE_LAST:
		Close();
		m_keptOpen = 0;
		break;
	}
	return ret;
}

int ConsoleInfoFile::GetPath(char *path)
{
	sce_paf_memcpy(path, CONSOLE_INFO_DIR, CONSOLE_INFO_DIR_LENGTH);
	sce_paf_memcpy(path + CONSOLE_INFO_DIR_LENGTH, m_name, CONSOLE_INFO_NAME_LENGTH);
	path[CONSOLE_INFO_PATH_MAX] = '\0';
	return 0;
}
