/*
 * location_log_writer.cpp: double-buffered asynchronous log writer thread and ring buffer
 * ("SceLocationDebugFlushThread", "SceLocationDebugBuffer%02d"; 0x8118AE80-0x8118B9A0).
 * The file's and internal names here are ours (PLACEHOLDER).
 */
#include "location_internal.h"
#include <std/stdio.h>
#include <std/string.h>
#include <std/stdlib.h>

/* (0x8118AE80) */
LocationLogWriter::LocationLogWriter(LocationLogger *logger)
{
	m_logger = logger;
	m_running = 0;
	m_bufMem = NULL;
	m_threadId = -1;
	m_writeIdx = 0;
	m_readIdx = 0;
	m_opSema = -1;
	m_freeSema = -1;
	m_flushEvent = -1;
	m_readMutexResult = -1;
	m_writeMutexResult = -1;
}

/* (0x8118AED8, 0x8118AEE6) */
LocationLogWriter::~LocationLogWriter()
{
}

/* (0x8118AEFC) */
int LocationLogWriter::DestroySync()
{
	int ret = 0;

	if (m_flushEvent >= 0) {
		int r = sceKernelDeleteEventFlag(m_flushEvent);

		if (r != 0)
			ret = r;
		m_flushEvent = -1;
	}
	if (m_freeSema >= 0) {
		int r = sceKernelWaitSema(m_freeSema, 2, NULL);

		if (r != 0)
			ret = r;
		r = sceKernelSignalSema(m_freeSema, 2);
		if (r != 0)
			ret = r;
		r = sceKernelDeleteSema(m_freeSema);
		if (r != 0)
			ret = r;
		m_freeSema = -1;
	}
	if (m_opSema >= 0) {
		int r = sceKernelWaitSema(m_opSema, 256, NULL);

		if (r != 0)
			ret = r;
		r = sceKernelSignalSema(m_opSema, 256);
		if (r != 0)
			ret = r;
		r = sceKernelDeleteSema(m_opSema);
		if (r != 0)
			ret = r;
		m_opSema = -1;
	}
	if (m_writeMutexResult == 0) {
		int r = sceKernelDeleteLwMutex(&m_writeMutex);

		if (r != 0)
			ret = r;
		m_writeMutexResult = -1;
	}
	if (m_readMutexResult == 0) {
		int r = sceKernelDeleteLwMutex(&m_readMutex);

		if (r != 0)
			ret = r;
		m_readMutexResult = -1;
	}
	return ret;
}

/* (0x8118AFDE) */
int LocationLogWriter::CreateSync()
{
	m_readMutexResult = sceKernelCreateLwMutex(&m_readMutex, "SceLocationDebugReadFromBuffer",
						   2, 0, NULL);
	if (m_readMutexResult != 0)
		return m_readMutexResult;

	m_writeMutexResult = sceKernelCreateLwMutex(&m_writeMutex, "SceLocationDebugWriteToBuffer",
						    2, 0, NULL);
	if (m_writeMutexResult != 0)
		return m_writeMutexResult;

	m_opSema = sceKernelCreateSema("SceLocationDebugOperationBuffer", 0, 256, 256, NULL);
	if (m_opSema < 0)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	m_freeSema = sceKernelCreateSema("SceLocationDebugFreeBuffer", 0, 2, 2, NULL);
	if (m_freeSema < 0)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	m_flushEvent = sceKernelCreateEventFlag("SceLocationDebugFlushEventFlag", 0, 0, NULL);
	if (m_flushEvent < 0)
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	return 0;
}

/* (0x8118B0AC) */
int LocationLogWriter::Start()
{
	int err = 0;

	m_bufMem = (char *)sce_paf_malloc(0x2000);
	if (m_bufMem == NULL) {
		err = SCE_LOCATION_ERROR_FATAL_ERROR;
		m_buffers[0].Term();
		m_buffers[1].Term();
		if (m_bufMem != NULL)
			sce_paf_free(m_bufMem);
		m_bufMem = NULL;
		return err;
	}

	for (int i = 0; i < 2 && err == 0; i++) {
		int r = m_buffers[i].Init((unsigned char)i);

		if (r != 0) {
			err = r;
		} else {
			m_buffers[i].m_data = m_bufMem + i * 0x1000;
		}
	}
	m_writeIdx = 0;
	m_readIdx = 0;
	if (err != 0) {
		m_buffers[0].Term();
		m_buffers[1].Term();
		if (m_bufMem != NULL)
			sce_paf_free(m_bufMem);
		m_bufMem = NULL;
		return err;
	}

	int r = CreateSync();

	if (r < 0) {
		DestroySync();
		m_buffers[0].Term();
		m_buffers[1].Term();
		if (m_bufMem != NULL)
			sce_paf_free(m_bufMem);
		m_bufMem = NULL;
		return r;
	}

	m_threadId = sceKernelCreateThread("SceLocationDebugFlushThread", FlushThreadEntry,
					   0x1000010A, 0x1000, 0, 0, NULL);
	if (m_threadId < 0) {
		r = SCE_LOCATION_ERROR_FATAL_ERROR;
	} else {
		LocationLogWriter *self = this;

		m_running = 1;
		r = sceKernelStartThread(m_threadId, sizeof(self), &self);
		if (r >= 0)
			return 0;
	}

	if (m_threadId >= 0) {
		m_running = 0;
		sceKernelCancelEventFlag(m_flushEvent, 0, NULL);
		sceKernelWaitThreadEnd(m_threadId, NULL, NULL);
		sceKernelDeleteThread(m_threadId);
		m_threadId = -1;
	}
	DestroySync();
	m_buffers[0].Term();
	m_buffers[1].Term();
	if (m_bufMem != NULL)
		sce_paf_free(m_bufMem);
	m_bufMem = NULL;
	return r;
}

/* (0x8118B24A) */
int LocationLogWriter::Stop()
{
	int ret = 0;

	if (m_threadId >= 0) {
		m_running = 0;
		int cancelRet = sceKernelCancelEventFlag(m_flushEvent, 0, NULL);
		int waitRet = sceKernelWaitThreadEnd(m_threadId, NULL, NULL);

		if (cancelRet != 0)
			waitRet = cancelRet;
		int delRet = sceKernelDeleteThread(m_threadId);

		if (delRet != 0)
			waitRet = delRet;
		m_threadId = -1;
		if (waitRet != 0)
			ret = waitRet;
	}

	int r = DestroySync();

	if (r != 0)
		ret = r;

	int bufErr = 0;

	for (int i = 0; i < 2; i++) {
		int br = m_buffers[i].Term();

		if (br != 0)
			bufErr = br;
	}
	if (m_bufMem != NULL)
		sce_paf_free(m_bufMem);
	m_bufMem = NULL;
	if (bufErr != 0)
		return bufErr;
	return ret;
}

/* (0x8118B2E6) */
int LocationLogWriter::WriteToBuffer(const char *data, SceSize len)
{
	int ret = 0;

	while (ret == 0 && len != 0) {
		int r = sceKernelWaitSema(m_freeSema, 1, NULL);

		if (r != 0)
			return r;
		r = m_buffers[m_writeIdx].Lock();
		if (r != 0)
			return r;

		SceSize avail = 0x1000 - m_buffers[m_writeIdx].m_used;
		SceSize chunk = (len > avail) ? avail : len;
		int ar = m_buffers[m_writeIdx].Append(data, chunk);
		unsigned char full = m_buffers[m_writeIdx].m_full;

		if (ar != 0)
			ret = ar;
		int ur = m_buffers[m_writeIdx].Unlock();

		if (ur != 0)
			ret = ur;
		data += chunk;
		len -= chunk;
		if (full == 1) {
			m_writeIdx = (unsigned char)((m_writeIdx + 1) % 2);
			r = sceKernelSetEventFlag(m_flushEvent, 1);
			if (r != 0)
				return r;
		} else {
			r = sceKernelSignalSema(m_freeSema, 1);
			if (r != 0)
				return r;
		}
	}
	return ret;
}

/* (0x8118B3D0) */
int LocationLogWriter::Write(const char *fmt, const char *tag, va_list args,
			     LocationLogParserList *parser)
{
	int ret = 0;
	int r = sceKernelWaitSema(m_opSema, 1, NULL);

	if (r != 0)
		return r;

	r = sceKernelLockLwMutex(&m_writeMutex, 1, NULL);
	if (r == 0) {
		int fmtErr = 0;
		SceDateTime time;

		sceRtcGetCurrentClock(&time, 0);

		int prefixLen = sce_paf_snprintf(m_formatBuf, 0x8000, "%02d:%02d:%02d.%03d,%s,",
						 time.hour, time.minute, time.second,
						 time.microsecond / 1000, tag);

		if (prefixLen < 0)
			fmtErr = SCE_LOCATION_ERROR_FATAL_ERROR;

		int msgLen = sce_paf_vsnprintf(m_formatBuf + prefixLen, (size_t)(0x8000 - prefixLen),
					       fmt, args);

		if (msgLen < 0)
			fmtErr = SCE_LOCATION_ERROR_FATAL_ERROR;
		if (fmtErr != 0)
			ret = fmtErr;

		if (parser != NULL)
			parser->Dispatch(m_formatBuf + prefixLen, m_logger);

		int wr = WriteToBuffer(m_formatBuf, sce_paf_strlen(m_formatBuf));

		if (wr != 0)
			ret = wr;

		if ((unsigned int)msgLen >= (unsigned int)(0x8000 - prefixLen)) {
			const char *warn = "[warning] Log may be cut to 32768 length.\n";
			int ww = WriteToBuffer(warn, sce_paf_strlen(warn));

			if (ww != 0)
				ret = ww;
		}

		int ur = sceKernelUnlockLwMutex(&m_writeMutex, 1);

		if (ur != 0)
			ret = ur;
	} else {
		ret = r;
	}

	int sr = sceKernelSignalSema(m_opSema, 1);

	if (sr != 0)
		return sr;
	return ret;
}

/* (0x8118B540) */
int LocationLogWriter::FlushOne()
{
	int ret = 0;
	int r = m_buffers[m_readIdx].Lock();

	if (r != 0)
		return r;

	LocationDebugBuffer *buf = &m_buffers[m_readIdx];

	if (buf->m_full == 1) {
		int wr = 0;
		SceSSize n = m_logger->WriteRaw(buf->m_data, buf->m_used);

		if (n < 0)
			wr = n;
		else
			buf->Clear();
		if (wr != 0)
			ret = wr;
		m_readIdx = (unsigned char)((m_readIdx + 1) % 2);
		int sr = sceKernelSignalSema(m_freeSema, 1);

		if (sr != 0)
			ret = sr;
	}

	int ur = buf->Unlock();

	if (ur != 0)
		return ur;
	return ret;
}

/* (0x8118B5DC) */
int LocationLogWriter::FlushAll()
{
	int ret = 0;
	int flushErr = 0;
	int r = sceKernelWaitSema(m_opSema, 1, NULL);

	if (r != 0)
		return r;

	int writeUnlockErr = sceKernelLockLwMutex(&m_writeMutex, 1, NULL);

	if (writeUnlockErr != 0) {
		ret = writeUnlockErr;
	} else {
		int lr = sceKernelLockLwMutex(&m_readMutex, 1, NULL);

		if (lr != 0) {
			ret = lr;
		} else {
			for (;;) {
				bool hasFull = false;

				for (int i = 0; i < 2; i++) {
					if (hasFull)
						break;
					hasFull = (m_buffers[i].m_full == 1);
				}
				if (!hasFull || ret != 0)
					break;
				lr = FlushOne();
				if (lr != 0)
					ret = lr;
			}
			if (lr == 0) {
				int br = m_buffers[m_writeIdx].Lock();

				if (br != 0) {
					flushErr = br;
				} else {
					LocationDebugBuffer *buf = &m_buffers[m_writeIdx];
					SceSSize n = m_logger->WriteRaw(buf->m_data, buf->m_used);

					if (n < 0)
						flushErr = n;
					else
						buf->Clear();
					int ur = m_buffers[m_writeIdx].Unlock();

					if (ur != 0)
						flushErr = ur;
				}
				if (flushErr != 0)
					ret = flushErr;
			}
			m_logger->SyncRaw();
			int ur = sceKernelUnlockLwMutex(&m_readMutex, 1);

			if (ur != 0)
				ret = ur;
		}
		writeUnlockErr = sceKernelUnlockLwMutex(&m_writeMutex, 1);
		if (writeUnlockErr != 0)
			ret = writeUnlockErr;
	}
	sceKernelSignalSema(m_opSema, 1);
	if (writeUnlockErr != 0)
		return writeUnlockErr;
	return ret;
}

/* (0x8118B716) */
int LocationLogWriter::FlushLoop()
{
	int ret = 0;

	while (ret == 0 && m_running == 1) {
		int r = sceKernelWaitEventFlag(m_flushEvent, 1, SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL,
					       NULL, NULL);

		if (r != 0)
			return r;
		r = sceKernelWaitSema(m_opSema, 1, NULL);
		if (r != 0)
			return r;
		r = sceKernelLockLwMutex(&m_readMutex, 1, NULL);
		if (r != 0) {
			ret = r;
		} else {
			for (;;) {
				bool hasFull = false;

				for (int i = 0; i < 2; i++) {
					if (hasFull)
						break;
					hasFull = (m_buffers[i].m_full == 1);
				}
				if (!hasFull || ret != 0)
					break;
				int fr = FlushOne();

				if (fr != 0) {
					ret = fr;
					int cleanErr = 0;

					if (m_freeSema >= 0) {
						int dr = sceKernelDeleteSema(m_freeSema);

						if (dr != 0)
							cleanErr = dr;
						m_freeSema = -1;
					}
					if (m_writeMutexResult == 0) {
						int dr = sceKernelDeleteLwMutex(&m_writeMutex);

						if (dr != 0)
							cleanErr = dr;
						m_writeMutexResult = -1;
					}
					if (m_readMutexResult == 0) {
						int dr = sceKernelDeleteLwMutex(&m_readMutex);

						if (dr != 0)
							cleanErr = dr;
						m_readMutexResult = -1;
					}
					for (int i = 0; i < 2; i++) {
						int dr = m_buffers[i].DeleteMutex();

						if (dr != 0)
							cleanErr = dr;
					}
					if (cleanErr != 0)
						ret = cleanErr;
					int tr = m_logger->m_timeoutTask.Schedule(0);

					if (tr != 0)
						ret = tr;
				}
			}
			m_logger->SyncRaw();
			int ur = sceKernelUnlockLwMutex(&m_readMutex, 1);

			if (ur != 0)
				ret = ur;
		}
		r = sceKernelSignalSema(m_opSema, 1);
		if (r != 0)
			return r;
	}
	return ret;
}

/* (0x8118B890) */
int LocationLogWriter::FlushThreadEntry(SceSize args, void *argp)
{
	LocationLogWriter *self = *(LocationLogWriter **)argp;

	return self->FlushLoop();
}

/* (0x8118B89C) */
__attribute__((noinline))
LocationDebugBuffer::LocationDebugBuffer()
{
	m_full = 0;
	m_data = NULL;
	m_mutexResult = 0;
	m_used = 0;
}

/* (0x8118B8B4) */
__attribute__((noinline))
int LocationDebugBuffer::Init(unsigned char index)
{
	int ret = 0;
	char name[32];

	if (sce_paf_snprintf(name, sizeof(name), "SceLocationDebugBuffer%02d", index) < 0)
		ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, name, 2, 0, NULL);
	if (m_mutexResult != 0)
		return m_mutexResult;
	return ret;
}

/* (0x8118B914) */
__attribute__((noinline))
int LocationDebugBuffer::Term()
{
	int ret = 0;

	m_data = NULL;
	if (m_mutexResult == 0) {
		ret = sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}
	return ret;
}

/* (0x8118B930) */
__attribute__((noinline))
int LocationDebugBuffer::DeleteMutex()
{
	int ret = 0;

	if (m_mutexResult == 0) {
		ret = sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}
	return ret;
}

/* (0x8118B94A) */
__attribute__((noinline))
int LocationDebugBuffer::Append(const void *data, SceSize len)
{
	if (data == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	sce_paf_memcpy(m_data + m_used, data, len);
	m_used += len;
	if (m_used >= 0x1000)
		m_full = 1;
	return 0;
}

/* (0x8118B97C) */
__attribute__((noinline))
void LocationDebugBuffer::Clear()
{
	m_full = 0;
	m_used = 0;
}

/* (0x8118B984) */
__attribute__((noinline))
int LocationDebugBuffer::Lock()
{
	return sceKernelLockLwMutex(&m_mutex, 1, NULL);
}

/* (0x8118B992) */
__attribute__((noinline))
int LocationDebugBuffer::Unlock()
{
	return sceKernelUnlockLwMutex(&m_mutex, 1);
}
