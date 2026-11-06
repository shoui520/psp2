/*
 * sceUpdaterCopyFile's copy (Sony's parallel_copy_file.cpp): the caller's thread reads the
 * source into one of two buffers while the consumer thread writes the other to the
 * destination. A side the caller gives no functions for is a file, opened, sized, read and
 * written here.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/threadmgr.h>
#include <updater_core/updater_core.h>
#include "updater_log.h"
#include "updater_memory.h"
#include "parallel_copy_file.h"

#define UPDATER_FILE_ID		0xbcc48a86

#define COPY_FILE_BUFFER_ALIGN		512
#define COPY_FILE_BUFFER_SIZE_DEFAULT	0x40000
#define COPY_FILE_MODE			0006

static int copyFileCallbackOpenForRead(void *arg, const char *path);
static int copyFileCallbackOpenForWrite(void *arg, const char *path);
static int copyFileCallbackClose(void *arg);
static SceInt32 copyFileCallbackGetSize(void *arg);
static SceSSize copyFileCallbackRead(void *arg, void *buf, SceSize size);
static SceSSize copyFileCallbackWrite(void *arg, const void *buf, SceSize size);
static void copyFileCallbackProgress(void *arg, SceUInt64 fileSize, SceUInt64 copied);

/* the option of a copy that gives none */
static SceUpdaterCopyFileOption s_defaultOption = {
	NULL, COPY_FILE_BUFFER_SIZE_DEFAULT, copyFileCallbackProgress,
	SCE_KERNEL_DEFAULT_PRIORITY_USER, 0,
};

ParallelCopyFileBuffer::ParallelCopyFileBuffer(SceUpdaterCopyFileTarget *dst,
					       SceUpdaterCopyFileOption *option)
{
	m_dst = dst;
	m_option = option;
	m_length = 0;
	m_buffer = NULL;
	m_buffer = g_updaterMemoryInfo.updaterCoreMemalign(COPY_FILE_BUFFER_ALIGN,
							   option->bufferSize);
}

ParallelCopyFileBuffer::~ParallelCopyFileBuffer()
{
	if (m_buffer != NULL) {
		g_updaterMemoryInfo.updaterCoreFree(m_buffer);
		m_buffer = NULL;
	}
}

/* writes the whole buffer to the destination */
int ParallelCopyFileBuffer::Operate(void)
{
	int ret = 0;
	SceInt32 written;
	SceInt32 size;
	SceSSize length;

	for (written = 0; written < (size = m_option->bufferSize); written += length) {
		length = m_dst->write(m_dst->arg, (SceUInt8 *)m_buffer + written, size - written);
		if (length < 0) {
			UPDATER_ERROR_FMT(281, "%s %d %d %d", m_dst->path, m_option->bufferSize,
					  length, written);
			return length;
		}
	}
	return ret;
}

ParallelCopyFile::ParallelCopyFile(SceUpdaterCopyFileTarget *src, SceUpdaterCopyFileTarget *dst,
				   SceUpdaterCopyFileOption *option)
{
	m_src = src;
	m_dst = dst;
	m_option = option;
	m_fileSize = 0;
	m_srcFd = -1;
	m_dstFd = -1;
}

ParallelCopyFile::~ParallelCopyFile()
{
}

/* checks the sides and fills in the defaults */
int ParallelCopyFile::setArgs(void)
{
	SceUpdaterCopyFileOption *option;

	if (m_src == NULL || m_dst == NULL || m_src->path == NULL || m_dst->path == NULL) {
		UPDATER_ERROR(111);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (m_src->open == NULL) {
		m_src->open = copyFileCallbackOpenForRead;
		m_src->close = copyFileCallbackClose;
		m_src->getSize = copyFileCallbackGetSize;
		m_src->read = copyFileCallbackRead;
		m_src->arg = &m_srcFd;
	}
	if (m_dst->open == NULL) {
		m_dst->open = copyFileCallbackOpenForWrite;
		m_dst->close = copyFileCallbackClose;
		m_dst->getSize = copyFileCallbackGetSize;
		m_dst->write = copyFileCallbackWrite;
		m_dst->arg = &m_dstFd;
	}
	option = m_option;
	if (option == NULL) {
		m_option = &s_defaultOption;
	} else {
		if (option->bufferSize == 0)
			option->bufferSize = COPY_FILE_BUFFER_SIZE_DEFAULT;
		if (option->callback == NULL)
			option->callback = copyFileCallbackProgress;
		if (option->cpuAffinityMask == 0)
			option->cpuAffinityMask = 0;
		if (option->threadPriority == 0)
			option->threadPriority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
	}
	return 0;
}

int ParallelCopyFile::CopyFile(void)
{
	SceInt64 fileSize;
	int ret;

	ret = setArgs();
	if (ret < 0) {
		UPDATER_ERROR_FMT(161, "%x", ret);
		return ret;
	}
	ret = SetUp();
	if (ret < 0) {
		UPDATER_ERROR_FMT(166, "%x", ret);
		return ret;
	}
	ret = m_src->open(m_src->arg, m_src->path);
	if (ret < 0) {
		UPDATER_ERROR_FMT(172, "%s : %x", m_src->path, ret);
		return ret;
	}
	ret = m_dst->open(m_dst->arg, m_dst->path);
	if (ret < 0) {
		UPDATER_ERROR_FMT(177, "%s : %x", m_dst->path, ret);
		m_src->close(m_src->arg);
		return ret;
	}
	fileSize = m_src->getSize(m_src->arg);
	m_fileSize = fileSize;
	if (fileSize < 0) {
		UPDATER_ERROR_FMT(184, "%x", (int)fileSize);
		return (int)fileSize;
	}
	ret = Execute(this);
	m_src->close(m_src->arg);
	m_dst->close(m_dst->arg);
	if (ret < 0) {
		UPDATER_ERROR_FMT(193, "%x", ret);
		return ret;
	}
	return 0;
}

int ParallelCopyFile::CreateElement(ParallelOperationElement **element)
{
	*element = new ParallelCopyFileBuffer(m_dst, m_option);
	return 0;
}

/* reads the source a buffer at a time, until its end */
int ParallelCopyFile::mainOperation(void *arg)
{
	ParallelCopyFileBuffer *buffer = NULL;
	SceInt64 copied = 0;
	SceSSize length;
	int ret;

	do {
		ret = m_freeChannel.Pop((void **)&buffer, 1);
		if (ret < 0)
			return ret;
		length = m_src->read(m_src->arg, buffer->GetBuffer(), m_option->bufferSize);
		if (length < 0) {
			UPDATER_ERROR_FMT(225, "%s %d : %x", m_src->path, m_option->bufferSize, length);
			return length;
		}
		copied += length;
		ret = m_fullChannel.Push(buffer, 1);
		if (ret < 0)
			return ret;
		m_option->callback(m_option->arg, m_fileSize, copied);
		if (copied >= m_fileSize)
			return ret;
	} while (length > 0 && ret == 0);
	return 0;
}

static int copyFileCallbackOpenForRead(void *arg, const char *path)
{
	SceUID fd;

	fd = sceIoOpen(path, SCE_O_RDONLY, COPY_FILE_MODE);
	if (fd < 0) {
		UPDATER_ERROR_FMT(40, "%s %o : %x", path, COPY_FILE_MODE, fd);
		return fd;
	}
	*(SceUID *)arg = fd;
	return 0;
}

static int copyFileCallbackOpenForWrite(void *arg, const char *path)
{
	SceUID fd;

	fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, COPY_FILE_MODE);
	if (fd < 0) {
		UPDATER_ERROR_FMT(54, "%s %o : %x", path, COPY_FILE_MODE, fd);
		return fd;
	}
	*(SceUID *)arg = fd;
	return 0;
}

static int copyFileCallbackClose(void *arg)
{
	return sceIoClose(*(SceUID *)arg);
}

/* the file's size, the position kept */
static SceInt32 copyFileCallbackGetSize(void *arg)
{
	SceUID fd = *(SceUID *)arg;
	SceOff position;
	SceOff size;

	position = sceIoLseek(fd, 0, SCE_SEEK_CUR);
	if (position < 0)
		return (SceInt32)position;
	size = sceIoLseek(fd, 0, SCE_SEEK_END);
	sceIoLseek(fd, position, SCE_SEEK_SET);
	return (SceInt32)size;
}

static SceSSize copyFileCallbackRead(void *arg, void *buf, SceSize size)
{
	return sceIoRead(*(SceUID *)arg, buf, size);
}

static SceSSize copyFileCallbackWrite(void *arg, const void *buf, SceSize size)
{
	return sceIoWrite(*(SceUID *)arg, buf, size);
}

static void copyFileCallbackProgress(void *arg, SceUInt64 fileSize, SceUInt64 copied)
{
}
