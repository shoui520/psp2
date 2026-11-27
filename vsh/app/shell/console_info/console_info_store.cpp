/*
 * console_info_store.cpp: ur0:ci/file/ fixed-size record store (1.dat, 4.dat,
 * r.dat), ConsoleInfoFile, and SceConsoleInfoWriterJob asynchronous writers
 * (0x81376B04..0x8137780B).
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <std/string.h>
#include <kernel/iofilemgr.h>
#include <iofilemgr/iofilemgr_common.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <job/job.h>
#include <rtc/rtc.h>
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

#define CONSOLE_INFO_JOB_OP_WRITE	0
#define CONSOLE_INFO_JOB_OP_ADD		1

#define CONSOLE_INFO_FILE_INDEX_FLAG	0
#define CONSOLE_INFO_FILE_INDEX_UINT32	1
#define CONSOLE_INFO_FILE_INDEX_UINT64	2

#define CONSOLE_INFO_ERR_NO_MEMORY	(-2147418100)	/* 0x8001000C */
#define CONSOLE_INFO_ERR_NO_QUEUE	(-2147418080)	/* 0x80010020 */

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
static int consoleInfoFileClose(ConsoleInfoFile *self)
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
static int consoleInfoFileOpen(ConsoleInfoFile *self)
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
int ConsoleInfoFile::Write(int index, const void *record, int mode)
{
	ensureConsoleInfoDir();
	paf::thread::MutexLock lock(&s_consoleInfoStoreMutex);

	int ret = 0;
	if (mode == CONSOLE_INFO_MODE_ONCE) {
		ret = consoleInfoFileOpen(this);
	} else if (mode == CONSOLE_INFO_MODE_KEEP_OPEN || mode == CONSOLE_INFO_MODE_CLOSE_AFTER) {
		if (m_opened != 1) {
			ret = consoleInfoFileOpen(this);
			m_opened = 1;
		}
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
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

	if (mode == CONSOLE_INFO_MODE_ONCE) {
		consoleInfoFileClose(this);
	} else if (mode == CONSOLE_INFO_MODE_CLOSE_AFTER) {
		consoleInfoFileClose(this);
		m_opened = 0;
	}
	return ret;
}

/* (0x81377634) Reads one fixed-size record at record index */
int ConsoleInfoFile::Read(int index, void *record, int mode)
{
	ensureConsoleInfoDir();
	paf::thread::MutexLock lock(&s_consoleInfoStoreMutex);

	int ret = 0;
	if (mode == CONSOLE_INFO_MODE_ONCE) {
		ret = consoleInfoFileOpen(this);
	} else if (mode == CONSOLE_INFO_MODE_KEEP_OPEN || mode == CONSOLE_INFO_MODE_CLOSE_AFTER) {
		if (m_opened != 1) {
			ret = consoleInfoFileOpen(this);
			m_opened = 1;
		}
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
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

	if (mode == CONSOLE_INFO_MODE_ONCE) {
		consoleInfoFileClose(this);
	} else if (mode == CONSOLE_INFO_MODE_CLOSE_AFTER) {
		consoleInfoFileClose(this);
		m_opened = 0;
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

/* (0x815C7538..0x815C757F, initialized at 0x81376D70) */
static ConsoleInfoFile s_consoleInfoFiles[3] = {
	ConsoleInfoFile("1.dat", 1),
	ConsoleInfoFile("4.dat", 4),
	ConsoleInfoFile("r.dat", 8)
};

/* (0x81376B04) Writes a 1-byte flag record to ur0:ci/file/1.dat */
int consoleInfoFileWriteFlag(int index, SceUInt8 value, int mode)
{
	return s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_FLAG].Write(index, &value, mode);
}

/* (0x81376B3C) Writes a 32-bit integer record to ur0:ci/file/4.dat */
int consoleInfoFileWriteInt(int index, SceUInt32 value, int mode)
{
	return s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_UINT32].Write(index, &value, mode);
}

/* (0x81376B74) Writes a 64-bit tick/counter record to ur0:ci/file/r.dat (-1 = current network tick) */
int consoleInfoFileWriteInt64(int index, SceUInt64 value, int mode)
{
	paf::rtc::Tick tick = value;
	if (tick == static_cast<SceUInt64>(-1) &&
	    paf::rtc::GetCurrentNetworkTick(&tick) < 0) {
		tick = 0;
	}
	return s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_UINT64].Write(index, &tick, mode);
}

/* (0x81376BDA) Saturating 32-bit increment on ur0:ci/file/4.dat */
int shellUsageCounterAdd(int id, unsigned int count, int flags)
{
	SceUInt32 current = 0;
	int ret = s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_UINT32].Read(id, &current, flags);
	if (ret < 0)
		return ret;

	SceUInt32 sum = current + count;
	if (count != 0 && count > sum)
		sum = 0xFFFFFFFFu;
	return s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_UINT32].Write(id, &sum, flags);
}

/* (0x81376C4A) Saturating 64-bit increment on ur0:ci/file/r.dat */
int consoleInfoFileAddInt64(int index, SceUInt64 delta, int mode)
{
	SceUInt64 current = 0;
	int ret = s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_UINT64].Read(index, &current, mode);
	if (ret < 0)
		return ret;

	SceUInt64 sum = current + delta;
	if (delta != 0 && delta > sum)
		sum = static_cast<SceUInt64>(-1);
	return s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_UINT64].Write(index, &sum, mode);
}

/* (0x81376CDE) Reads a 1-byte flag record from ur0:ci/file/1.dat */
int consoleInfoFileReadFlag(int index, SceUInt8 *value, int mode)
{
	*value = 0;
	return s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_FLAG].Read(index, value, mode);
}

/* (0x81376CF8) Reads a 32-bit record from ur0:ci/file/4.dat */
int consoleInfoFileReadInt(int index, SceUInt32 *value, int mode)
{
	*value = 0;
	return s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_UINT32].Read(index, value, mode);
}

/* (0x81376D16) Reads a 64-bit record from ur0:ci/file/r.dat */
int consoleInfoFileReadInt64(int index, SceUInt64 *value, int mode)
{
	*value = 0;
	return s_consoleInfoFiles[CONSOLE_INFO_FILE_INDEX_UINT64].Read(index, value, mode);
}

/* (vtable 0x8153DC5C, Run 0x81377090) Async writer job for ConsoleInfoFile */
class ConsoleInfoWriterJob : public paf::job::JobItem {
public:
	ConsoleInfoWriterJob(int fileIndex, int op, int recordIndex, void *payload)
		: paf::job::JobItem("SceConsoleInfoWriterJob", NULL),
		  m_fileIndex(fileIndex),
		  m_op(op),
		  m_recordIndex(recordIndex),
		  m_payload(payload)
	{
	}

	/* (0x81376DC8, 0x81376DE8) */
	virtual ~ConsoleInfoWriterJob()
	{
		operator delete(m_payload);
	}

	/* (0x81377090) */
	virtual int32_t Run()
	{
		if (m_op == CONSOLE_INFO_JOB_OP_WRITE) {
			s_consoleInfoFiles[m_fileIndex].Write(m_recordIndex, m_payload, CONSOLE_INFO_MODE_ONCE);
		} else if (m_op == CONSOLE_INFO_JOB_OP_ADD) {
			if (m_fileIndex == CONSOLE_INFO_FILE_INDEX_UINT32) {
				SceUInt32 delta = *static_cast<const SceUInt32 *>(m_payload);
				shellUsageCounterAdd(m_recordIndex, delta, CONSOLE_INFO_MODE_ONCE);
			} else if (m_fileIndex == CONSOLE_INFO_FILE_INDEX_UINT64) {
				SceUInt64 delta = *static_cast<const SceUInt64 *>(m_payload);
				consoleInfoFileAddInt64(m_recordIndex, delta, CONSOLE_INFO_MODE_ONCE);
			}
		}
		return 0;
	}

	virtual void Finish(int32_t result)
	{
		(void)result;
	}

private:
	int m_fileIndex;
	int m_op;
	int m_recordIndex;
	void *m_payload;
};

/* (0x81376E0C) Enqueues a ConsoleInfoWriterJob onto the default job queue */
static int consoleInfoEnqueueJob(ConsoleInfoWriterJob *job)
{
	if (job == NULL)
		return CONSOLE_INFO_ERR_NO_MEMORY;

	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);
	paf::job::JobQueue *queue = paf::job::JobQueue::default_queue;
	if (queue == NULL)
		return CONSOLE_INFO_ERR_NO_QUEUE;

	int ret = queue->Enqueue(jobRef);
	return (ret != 0) ? ret : 0;
}

/* (0x81376F7A) Enqueues an async 32-bit write or saturating add */
static __attribute__((noinline)) int consoleInfoEnqueueInt(int op, int index, SceUInt32 value)
{
	SceUInt32 *copy = static_cast<SceUInt32 *>(operator new(sizeof(SceUInt32)));
	if (copy == NULL)
		return CONSOLE_INFO_ERR_NO_MEMORY;
	*copy = value;

	ConsoleInfoWriterJob *job = new ConsoleInfoWriterJob(
		CONSOLE_INFO_FILE_INDEX_UINT32, op, index, copy);
	return consoleInfoEnqueueJob(job);
}

/* (0x81377004) Enqueues an async 64-bit write or saturating add */
static __attribute__((noinline)) int consoleInfoEnqueueInt64(int op, int index, SceUInt64 value)
{
	SceUInt64 *copy = static_cast<SceUInt64 *>(operator new(sizeof(SceUInt64)));
	if (copy == NULL)
		return CONSOLE_INFO_ERR_NO_MEMORY;
	*copy = value;

	ConsoleInfoWriterJob *job = new ConsoleInfoWriterJob(
		CONSOLE_INFO_FILE_INDEX_UINT64, op, index, copy);
	return consoleInfoEnqueueJob(job);
}

/* (0x81376D36) Async saturating 32-bit counter increment (mode unused) */
int consoleInfoFileAddInt(int index, SceUInt32 value, int mode)
{
	(void)mode;
	return consoleInfoEnqueueInt(CONSOLE_INFO_JOB_OP_ADD, index, value);
}

/* (0x81376D44) Async saturating 64-bit elapsed-time increment */
int consoleInfoFileAddTime(int index, SceUInt64 elapsedUs)
{
	return consoleInfoEnqueueInt64(CONSOLE_INFO_JOB_OP_ADD, index, elapsedUs);
}
