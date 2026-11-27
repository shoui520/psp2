/*
 * console_info_file.cpp: ConsoleInfoFile fixed-size record file backing store
 * under ur0:ci/file/ (0x813770FC..0x8137780B).
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <std/string.h>
#include <kernel/iofilemgr.h>
#include <iofilemgr/iofilemgr_common.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <thread/thread.h>

#include "console_info.h"

#define CONSOLE_INFO_STORE_DIR		"ur0:ci/file/"
#define CONSOLE_INFO_STORE_DIR_LEN	12
#define CONSOLE_INFO_STORE_NAME_LEN	6
#define CONSOLE_INFO_STORE_NO_ENTRY	(-2135994334)	/* 0x80AF5022 */
#define CONSOLE_INFO_STORE_FILE_MODE	IO_STM_RWS

#define CONSOLE_INFO_MODE_ONCE		0
#define CONSOLE_INFO_MODE_KEEP_OPEN	1
#define CONSOLE_INFO_MODE_CLOSE_AFTER	2

/* (0x815489D8, 0x815C7580) Shared directory creation flag and store lock */
static bool s_consoleInfoDirReady = false;
static paf::thread::Mutex s_consoleInfoStoreMutex("SceConsoleInfo", 0);

static void ensureConsoleInfoDir(void)
{
	paf::thread::MutexLock lock(&s_consoleInfoStoreMutex);
	if (!s_consoleInfoDirReady) {
		paf::Dir::CreateRecursive(CONSOLE_INFO_STORE_DIR);
		s_consoleInfoDirReady = true;
	}
}

/* (0x813770FC, 0x8137711A) */
ConsoleInfoFile::ConsoleInfoFile(const char *name, SceUInt8 recordSize)
	: m_name(name),
	  m_recordSize(recordSize),
	  m_openCount(0),
	  m_opened(0)
{
}

/* (0x81377124) Internal ref-counted close helper */
static __attribute__((noinline)) int consoleInfoFileClose(ConsoleInfoFile *self)
{
	if (self->m_file.get() == NULL)
		return SCE_ERROR_ERRNO_ESTALE;
	if (self->m_openCount > 1) {
		self->m_openCount--;
		return 0;
	}
	int ret = self->m_file->Close();
	self->m_file.reset();
	return ret;
}

/* (0x8137722C) Internal ref-counted open helper */
static __attribute__((noinline)) int consoleInfoFileOpen(ConsoleInfoFile *self)
{
	if (self->m_file.get() != NULL) {
		self->m_openCount++;
		return 0;
	}

	char path[CONSOLE_INFO_FILE_PATH_SIZE];
	self->GetPath(path);

	int err = 0;
	self->m_file = paf::LocalFile::Open(path, SCE_O_CREAT | SCE_O_RDWR,
					    CONSOLE_INFO_STORE_FILE_MODE, &err);
	if (err == CONSOLE_INFO_STORE_NO_ENTRY) {
		paf::Dir::CreateRecursive(CONSOLE_INFO_STORE_DIR);
		err = 0;
		self->m_file.reset();
		self->m_file = paf::LocalFile::Open(path, SCE_O_CREAT | SCE_O_RDWR,
						    CONSOLE_INFO_STORE_FILE_MODE, &err);
	}
	if (err != 0) {
		self->m_file.reset();
		return err;
	}
	return 0;
}

/* (0x813771A4, 0x81377214) */
ConsoleInfoFile::~ConsoleInfoFile()
{
	if (m_file.get() != NULL)
		consoleInfoFileClose(this);
}

/* (0x813774FC) Writes one fixed-size record at record index */
__attribute__((noinline)) int ConsoleInfoFile::Write(int index, const void *record, int mode)
{
	ensureConsoleInfoDir();
	paf::thread::MutexLock lock(&s_consoleInfoStoreMutex);

	int ret = 0;
	switch (mode) {
	case CONSOLE_INFO_MODE_ONCE:
		ret = consoleInfoFileOpen(this);
		break;
	case CONSOLE_INFO_MODE_KEEP_OPEN:
	case CONSOLE_INFO_MODE_CLOSE_AFTER:
		if (m_opened != 1) {
			ret = consoleInfoFileOpen(this);
			m_opened = 1;
		}
		break;
	default:
		ret = SCE_ERROR_ERRNO_EINVAL;
		break;
	}
	if (ret < 0)
		return ret;

	if (m_file.get() == NULL) {
		ret = SCE_ERROR_ERRNO_ESTALE;
	} else {
		ret = m_file->Seek(static_cast<SceInt64>(index * m_recordSize), SCE_SEEK_SET);
		if (ret >= 0) {
			int written = m_file->Write(record, m_recordSize);
			ret = (written < 0) ? written : 0;
		}
	}

	switch (mode) {
	case CONSOLE_INFO_MODE_ONCE:
		consoleInfoFileClose(this);
		break;
	case CONSOLE_INFO_MODE_CLOSE_AFTER:
		consoleInfoFileClose(this);
		m_opened = 0;
		break;
	default:
		break;
	}
	return ret;
}

/* (0x81377634) Reads one fixed-size record at record index */
__attribute__((noinline)) int ConsoleInfoFile::Read(int index, void *record, int mode)
{
	ensureConsoleInfoDir();
	paf::thread::MutexLock lock(&s_consoleInfoStoreMutex);

	int ret = 0;
	switch (mode) {
	case CONSOLE_INFO_MODE_ONCE:
		ret = consoleInfoFileOpen(this);
		break;
	case CONSOLE_INFO_MODE_KEEP_OPEN:
	case CONSOLE_INFO_MODE_CLOSE_AFTER:
		if (m_opened != 1) {
			ret = consoleInfoFileOpen(this);
			m_opened = 1;
		}
		break;
	default:
		ret = SCE_ERROR_ERRNO_EINVAL;
		break;
	}
	if (ret < 0)
		return ret;

	if (m_file.get() == NULL) {
		ret = SCE_ERROR_ERRNO_ESTALE;
	} else {
		ret = m_file->Seek(static_cast<SceInt64>(index * m_recordSize), SCE_SEEK_SET);
		if (ret >= 0) {
			int bytesRead = m_file->Read(record, m_recordSize);
			ret = (bytesRead < 0) ? bytesRead : 0;
		}
	}

	switch (mode) {
	case CONSOLE_INFO_MODE_ONCE:
		consoleInfoFileClose(this);
		break;
	case CONSOLE_INFO_MODE_CLOSE_AFTER:
		consoleInfoFileClose(this);
		m_opened = 0;
		break;
	default:
		break;
	}
	return ret;
}

/* (0x8137776A) Removes the backing file if present */
int ConsoleInfoFile::Remove()
{
	char path[CONSOLE_INFO_FILE_PATH_SIZE];
	GetPath(path);
	int ret = paf::LocalFile::RemoveFile(path);
	if (ret == 0 || ret == CONSOLE_INFO_STORE_NO_ENTRY)
		return 0;
	return ret;
}

/* (0x813777B0) Formats ur0:ci/file/<name> into caller buffer */
void ConsoleInfoFile::GetPath(char *path)
{
	sce_paf_memcpy(path, CONSOLE_INFO_STORE_DIR, CONSOLE_INFO_STORE_DIR_LEN);
	sce_paf_memcpy(path + CONSOLE_INFO_STORE_DIR_LEN, m_name, CONSOLE_INFO_STORE_NAME_LEN);
	path[CONSOLE_INFO_FILE_PATH_SIZE - 1] = '\0';
}
