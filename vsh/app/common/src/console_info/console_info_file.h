/* A console information file (console_info_file.cpp). */

#ifndef _CONSOLE_INFO_FILE_H
#define _CONSOLE_INFO_FILE_H

#include <scetypes.h>
#include <common/shared_ptr.h>
#include <file/file.h>

#include "console_info.h"

class ConsoleInfoFile {
public:
	ConsoleInfoFile(const char *name, SceUInt8 recordSize);
	virtual ~ConsoleInfoFile();
	/* its path, in a buffer of CONSOLE_INFO_PATH_MAX + 1 bytes */
	virtual int GetPath(char *path);

	int Open();
	int Close();
	/* writes record index from data (recordSize bytes) */
	int Write(int index, const void *data, int mode);

private:
	paf::common::SharedPtr<paf::File> m_file;
	const char *m_name;		/* in ur0:ci/file/ */
	SceUInt8 m_recordSize;
	SceUInt8 m_openCount;		/* the Opens beyond the first */
	int m_keptOpen;			/* opened by a CONSOLE_INFO_WRITE_KEEP_OPEN */
};

#define CONSOLE_INFO_PATH_MAX		64

#endif /* _CONSOLE_INFO_FILE_H */
