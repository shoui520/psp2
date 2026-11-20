/*
 * The background download library's task paths: a task's folder, "ux0:bgdl/t/%08x" (19
 * characters), with room for a file name after it. No record names the class or its methods:
 * the names are ours (PLACEHOLDER).
 */
#ifndef SHELL_BGDL_PATH_H
#define SHELL_BGDL_PATH_H

#include <scetypes.h>
#include <std/string.h>

/* the library's errors (PLACEHOLDER names) */
#define BGDL_ERROR_NOT_STARTED		0x80103A00	/* the manager is not made */
#define BGDL_ERROR_NO_MEMORY		0x80103A08
#define BGDL_ERROR_INVALID_ARGUMENT	0x80103A0E

/* the length of a task's folder, "ux0:bgdl/t/%08x" */
#define BGDL_PATH_FOLDER_LENGTH		19

class BgdlTaskPath {
public:
	BgdlTaskPath() : m_begin(NULL), m_end(NULL)
	{
	}

	~BgdlTaskPath()
	{
		if (m_begin != NULL)
			delete[] m_begin;
	}

	/* the buffer made size bytes, the old contents kept */
	int Reserve(SceSize size)
	{
		if (size != 0) {
			char *buf = new char[size];

			if (buf == NULL)
				return BGDL_ERROR_NO_MEMORY;
			if (m_begin != NULL) {
				sce_paf_memset(buf, 0, size);
				sce_paf_memcpy(buf, m_begin, size);
				if (m_begin != NULL)
					delete[] m_begin;
			}
			m_begin = buf;
			m_end = buf + size;
		} else if (m_begin != NULL) {
			delete[] m_begin;
			m_begin = NULL;
			m_end = NULL;
		}
		return 0;
	}

	void Swap(BgdlTaskPath &other)
	{
		char *buf;

		buf = m_begin;
		m_begin = other.m_begin;
		other.m_begin = buf;
		buf = m_end;
		m_end = other.m_end;
		other.m_end = buf;
	}

	int SetFolder(const char *device, int taskId, bool work);	/* (0x810FCD10) */
	int SetTask(int taskId, SceSize size);				/* (0x810FCF36) */
	char *SetName(const char *name);				/* (0x810FCFEE) */
	int SetNameFormat(const char *format, ...);			/* (0x810FD00E) */
	char *ClearName(void);						/* (0x810FD106) */
	static bool Exists(const char *path);				/* (0x810FD12E) */
	bool Exists(void);						/* (0x810FD13C) */

	char *m_begin;
	char *m_end;
};

#endif
