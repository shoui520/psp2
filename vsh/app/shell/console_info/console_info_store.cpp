/*
 * console_info_store.cpp: ur0:ci/file/ fixed-size record store (1.dat, 4.dat,
 * r.dat) and SceConsoleInfoWriterJob asynchronous writers
 * (0x81376B04..0x813770FB).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <job/job.h>
#include <rtc/rtc.h>

#include "console_info.h"

#define CONSOLE_INFO_MODE_ONCE		0

#define CONSOLE_INFO_JOB_OP_WRITE	0
#define CONSOLE_INFO_JOB_OP_ADD		1

#define CONSOLE_INFO_FILE_INDEX_FLAG	0
#define CONSOLE_INFO_FILE_INDEX_UINT32	1
#define CONSOLE_INFO_FILE_INDEX_UINT64	2

#define CONSOLE_INFO_ERR_NO_MEMORY	(-2147418100)	/* 0x8001000C */
#define CONSOLE_INFO_ERR_NO_QUEUE	(-2147418080)	/* 0x80010020 */

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
__attribute__((noinline)) int shellUsageCounterAdd(int id, unsigned int count, int flags)
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
__attribute__((noinline)) int consoleInfoFileAddInt64(int index, SceUInt64 delta, int mode)
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
		switch (m_op) {
		case CONSOLE_INFO_JOB_OP_WRITE:
			s_consoleInfoFiles[m_fileIndex].Write(m_recordIndex, m_payload,
							      CONSOLE_INFO_MODE_ONCE);
			break;
		case CONSOLE_INFO_JOB_OP_ADD:
			if (m_fileIndex == CONSOLE_INFO_FILE_INDEX_UINT32) {
				SceUInt32 delta = *static_cast<const SceUInt32 *>(m_payload);
				shellUsageCounterAdd(m_recordIndex, delta, CONSOLE_INFO_MODE_ONCE);
			} else if (m_fileIndex == CONSOLE_INFO_FILE_INDEX_UINT64) {
				SceUInt64 delta = *static_cast<const SceUInt64 *>(m_payload);
				consoleInfoFileAddInt64(m_recordIndex, delta,
							CONSOLE_INFO_MODE_ONCE);
			}
			break;
		default:
			break;
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
static __attribute__((noinline)) int consoleInfoEnqueueJob(ConsoleInfoWriterJob *job)
{
	if (job == NULL)
		return CONSOLE_INFO_ERR_NO_MEMORY;

	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);
	paf::job::JobQueue *queue = paf::job::JobQueue::default_queue;
	if (queue == NULL)
		return CONSOLE_INFO_ERR_NO_QUEUE;

	int ret = queue->Enqueue(jobRef);
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x81376F7A) Enqueues an async 32-bit write or saturating add */
static __attribute__((noinline)) int consoleInfoEnqueueInt(int op, int index, SceUInt32 value)
{
	void *copy = operator new(sizeof(SceUInt32));
	if (copy == NULL)
		return CONSOLE_INFO_ERR_NO_MEMORY;
	sce_paf_memcpy(copy, &value, sizeof(SceUInt32));

	ConsoleInfoWriterJob *job = new ConsoleInfoWriterJob(
		CONSOLE_INFO_FILE_INDEX_UINT32, op, index, copy);
	return consoleInfoEnqueueJob(job);
}

/* (0x81377004) Enqueues an async 64-bit write or saturating add */
static __attribute__((noinline)) int consoleInfoEnqueueInt64(int op, int index, SceUInt64 value)
{
	void *copy = operator new(sizeof(SceUInt64));
	if (copy == NULL)
		return CONSOLE_INFO_ERR_NO_MEMORY;
	sce_paf_memcpy(copy, &value, sizeof(SceUInt64));

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
