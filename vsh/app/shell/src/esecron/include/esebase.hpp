/*
 * esebase.hpp: the background download library's (Sony's esecron, vsh/app/shell/src/esecron)
 * base types: a task's paths, a growable array, a transfer's progress, the task list and the
 * number conversions of its param files. Sony's 0.990 prototype names the header and some of
 * the methods (initialize, format, insert); the types and the other names are ours
 * (PLACEHOLDER).
 */
#ifndef ESECRON_ESEBASE_HPP
#define ESECRON_ESEBASE_HPP

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>

/* the library's errors (PLACEHOLDER names) */
#define BGDL_ERROR_NOT_STARTED		0x80103A00	/* the manager is not made */
#define BGDL_ERROR_NO_UX0		0x80103A02	/* the task needs ux0: */
#define BGDL_ERROR_NO_STORAGE		0x80103A03	/* neither ux0: nor grw0: is there */
#define BGDL_ERROR_NO_APP		0x80103A07	/* the title has no livespace_db row */
#define BGDL_ERROR_NO_MEMORY		0x80103A08
#define BGDL_ERROR_NULL_TITLE		0x80103A09
#define BGDL_ERROR_INVALID_ARGUMENT	0x80103A0E
#define BGDL_ERROR_NO_KEY		0x80103A0F	/* the param file has no such key */
#define BGDL_ERROR_QUEUE_FULL		0x80103A19	/* 32 tasks listed (30 for a task of key 116) */
#define BGDL_ERROR_NO_TASK		0x80103A14	/* no task has the id */
#define BGDL_ERROR_NOT_LOADED		0x80103A1A	/* a task's result until its param file is read */

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

	/* the path set to a copy of src; left as it was when no memory is left */
	void assign(const char *src)
	{
		SceSize len = sce_paf_strlen(src);

		if (Reserve(len + 1) == 0) {
			sce_paf_memcpy(m_begin, src, len);
			m_begin[len] = '\0';
		}
	}

	int initialize(const char *device, int taskId, bool work);	/* (0x810FCD10) */
	int initialize(int taskId, SceSize size);			/* (0x810FCF36) */
	char *SetName(const char *name);				/* (0x810FCFEE) */
	int format(const char *format, ...);				/* (0x810FD00E) */
	char *ClearName(void);						/* (0x810FD106) */
	static bool Exists(const char *path);				/* (0x810FD12E) */
	bool Exists(void);						/* (0x810FD13C) */

	char *m_begin;
	char *m_end;
};

/* an entry already in the list with the same key */
#define BGDL_ERROR_ALREADY_EXISTS	0x80103A17
#define BGDL_ERROR_BUSY			0x80103A3C	/* the job dispatcher is not idle */
#define BGDL_ERROR_GAME_PLUGIN		0x80103A47	/* the game plugin returned 0x80010010 */
#define BGDL_ERROR_BAD_STORAGE		0x80103A38	/* the title's storage kind is not one a task can use */

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

class EseTask;

/* the manager's tasks, sorted by id, each id once (esetask.hpp) */
class BgdlTaskList : public BgdlArray<EseTask *> {
public:
	int insert(EseTask *task);			/* (0x810FD4EA) */
	void Erase(EseTask **pos);			/* (0x810FD5E6) */
	EseTask **Find(EseTask *task);		/* (0x810FD64E) the task with its id, or m_end */
	void Clear(void);				/* (0x810FD6BA) every task deleted */
};

int bgdlStringToInt(const paf::string &str);			/* (0x810FD328) */
int bgdlUIntToString(const SceUInt32 *value, paf::string *str);	/* (0x810FD366) */
int bgdlGetInt(const int *value);				/* (0x810FD412) */
int bgdlParseInt(const paf::string &str);			/* (0x810FD442) */
int bgdlFormatUInt(SceUInt32 value, paf::string *str);		/* (0x810FD454) */

#endif
