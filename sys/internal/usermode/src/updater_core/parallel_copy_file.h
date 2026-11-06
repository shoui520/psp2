/*
 * ParallelCopyFile (parallel_copy_file.cpp): a file copy, its reads on the caller's thread and
 * its writes on the consumer thread. CopyFile, setArgs and mainOperation are the 0.995 build's
 * names; the class takes the file's.
 */
#ifndef UPDATER_CORE_PARALLEL_COPY_FILE_H
#define UPDATER_CORE_PARALLEL_COPY_FILE_H

#include <scetypes.h>
#include <updater_core/updater_core.h>
#include "parallel_operation_base.h"

/* a buffer the copy reads into and the consumer writes out (the name ours) */
class ParallelCopyFileBuffer : public ParallelOperationElement {
public:
	ParallelCopyFileBuffer(SceUpdaterCopyFileTarget *dst, SceUpdaterCopyFileOption *option);
	virtual ~ParallelCopyFileBuffer();

	virtual int Operate(void);

	void *GetBuffer(void)
	{
		return m_buffer;
	}

private:
	SceInt32 m_length;		/* set to 0; nothing reads it */
	void *m_buffer;
	SceUpdaterCopyFileTarget *m_dst;
	SceUpdaterCopyFileOption *m_option;
};

class ParallelCopyFile : public ParallelOperationBase {
public:
	ParallelCopyFile(SceUpdaterCopyFileTarget *src, SceUpdaterCopyFileTarget *dst,
			 SceUpdaterCopyFileOption *option);
	virtual ~ParallelCopyFile();

	int CopyFile(void);

protected:
	virtual int CreateElement(ParallelOperationElement **element);
	virtual int mainOperation(void *arg);

private:
	int setArgs(void);

	SceUpdaterCopyFileTarget *m_src;
	SceUpdaterCopyFileTarget *m_dst;
	SceUpdaterCopyFileOption *m_option;
	SceInt64 m_fileSize;
	SceUID m_srcFd;
	SceUID m_dstFd;
};

#endif
