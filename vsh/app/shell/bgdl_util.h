/*
 * The background download library's helpers (bgdl_util.cpp, 0x810FD14C on): a transfer's
 * progress (bytes done, speed, time left), number and string conversions for its param
 * files, and a the manager's task list. No record names the file, the types or
 * the functions: the names are ours (PLACEHOLDER).
 */
#ifndef SHELL_BGDL_UTIL_H
#define SHELL_BGDL_UTIL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>

#include "bgdl_path.h"

/* an entry already in the list with the same key */
#define BGDL_ERROR_ALREADY_EXISTS	0x80103A17

/* a transfer's progress; times in microseconds (paf::get_time_internal) */
class BgdlProgress {
public:
	void Start(SceUInt64 total, SceUInt64 base, SceUInt64 reserved, SceUInt64 interval);	/* (0x810FD14C) */
	int Update(SceUInt64 current);			/* (0x810FD19A) */
	__attribute__((noinline)) SceUInt64 GetSpeed(void);	/* (0x810FD1F2) bytes a second */
	SceUInt64 GetRemaining(void);			/* (0x810FD2F0) seconds */

	SceUInt64 m_total;		/* the transfer's size; 0 or -1: unknown */
	SceUInt64 m_current;		/* the bytes done so far */
	SceUInt64 m_base;		/* the bytes done when it (re)started */
	SceUInt64 m_done;		/* the bytes done since then */
	SceUInt64 m_startTime;
	SceUInt64 m_lastTime;		/* the last update */
	SceUInt64 m_reportTime;		/* the last update Update() reported */
	SceUInt64 m_interval;		/* the time between two reports */
	int m_reserved0;		/* PLACEHOLDER: only cleared */
	int m_reserved1;
};

/* a growable array of T, its buffer m_begin to m_end, sized in bytes */
template <class T>
class BgdlArray {
public:
	BgdlArray() : m_begin(NULL), m_end(NULL)
	{
	}

	~BgdlArray()
	{
		if (m_begin != NULL)
			delete[] (char *)m_begin;
	}

	/* the buffer made size bytes, the old contents kept */
	int Reserve(SceSize size)
	{
		if (size != 0) {
			T *buf = (T *)new char[size];

			if (buf == NULL)
				return BGDL_ERROR_NO_MEMORY;
			if (m_begin != NULL) {
				sce_paf_memset(buf, 0, size);
				sce_paf_memcpy(buf, m_begin, size);
				if (m_begin != NULL)
					delete[] (char *)m_begin;
			}
			m_begin = buf;
			m_end = (T *)((char *)buf + size);
		} else if (m_begin != NULL) {
			delete[] (char *)m_begin;
			m_begin = NULL;
			m_end = NULL;
		}
		return 0;
	}

	void Swap(BgdlArray &other)
	{
		T *buf;

		buf = m_begin;
		m_begin = other.m_begin;
		other.m_begin = buf;
		buf = m_end;
		m_end = other.m_end;
		other.m_end = buf;
	}

	T *m_begin;
	T *m_end;
};

class BgdlTask;

/* the manager's tasks, sorted by id, each id once (bgdl_task.h) */
class BgdlTaskList : public BgdlArray<BgdlTask *> {
public:
	int Insert(BgdlTask *task);			/* (0x810FD4EA) */
	void Erase(BgdlTask **pos);			/* (0x810FD5E6) */
	BgdlTask **Find(BgdlTask *task);		/* (0x810FD64E) the task with its id, or m_end */
	void Clear(void);				/* (0x810FD6BA) every task deleted */
};

int bgdlStringToInt(const paf::string &str);			/* (0x810FD328) */
int bgdlUIntToString(const SceUInt32 *value, paf::string *str);	/* (0x810FD366) */
int bgdlGetInt(const int *value);				/* (0x810FD412) */
int bgdlParseInt(const paf::string &str);			/* (0x810FD442) */
int bgdlFormatUInt(SceUInt32 value, paf::string *str);		/* (0x810FD454) */

#endif
