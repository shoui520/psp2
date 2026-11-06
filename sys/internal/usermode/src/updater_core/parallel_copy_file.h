/* ParallelCopyFile (parallel_copy_file.cpp): a file copy, its reads and writes on two threads. The class name is ours. */
#ifndef UPDATER_CORE_PARALLEL_COPY_FILE_H
#define UPDATER_CORE_PARALLEL_COPY_FILE_H

#include <scetypes.h>
#include <updater_core/updater_core.h>

class ParallelCopyFile {
public:
	ParallelCopyFile(const SceUpdaterCopyFileTarget *src, const SceUpdaterCopyFileTarget *dst,
			 const SceUpdaterCopyFileOption *option);
	~ParallelCopyFile();

	int CopyFile(void);

private:
	SceUInt8 m_state[212];
};

#endif
