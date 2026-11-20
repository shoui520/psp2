/*
 * esetask.cpp: the background download library's tasks and their manager (0x810FCD10 to the
 * file's static initialiser at 0x81110620). Sony's 0.990 prototype names the file
 * (vsh/app/shell/src/esecron/src/esetask.cpp) and some functions; the other names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <stdarg.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <dir/dir.h>
#include <timer/timer.h>

#include "../include/esebase.hpp"
#include "../include/esetask.hpp"
#include "src/livespace_db/lsdb_keys.h"

using namespace paf;

/* the task's param file (eseparam.cpp; PLACEHOLDER names: not written yet) */
int Bgdl_8112260C(SceUInt32 id, int key, void *value, SceSize size);		/* read */
int Bgdl_811233A4(SceUInt32 id, int key, const void *value, SceSize size);	/* write */
int Bgdl_8112278A(SceUInt32 id, int key, string *value);			/* read a string */

static BgdlManager *s_manager = NULL;		/* (0x81544788) */
static const char *s_np = "np";			/* (0x8154478C) PLACEHOLDER: read later */
static SceUInt64 s_handled = 0;			/* (0x81544790) the requests the thread took */
static SceUInt64 s_reserved0 = 0;		/* (0x81544798) PLACEHOLDER: read later */
static SceUInt64 s_requested = 0;		/* (0x815447A0) the requests made */
static SceUInt64 s_wakeups = 0;			/* (0x815447A8) */
static thread::RMutex s_packageMutex("SceBgdlRMutexPackage", 0);	/* (0x81563150) */
static thread::RMutex s_progressMutex("SceBgdlRMutexProgress", 0);	/* (0x81563170) */
static thread::RMutex s_updateMutex("SceBgdlRMutexUpdate", 0);		/* (0x81563190) guards s_manager */

/* (0x810FCD10) the task's folder on a device: its download folder, or its work folder */
int BgdlTaskPath::initialize(const char *device, int taskId, bool work)
{
	string path(device);
	int ret;

	if (!work)
		path += "bgdl/t";
	else
		path += "bgdl/w";
	char name[24] = "";

	sce_paf_snprintf(name, sizeof(name) - 1, "/%08x", taskId);
	path += name;
	ret = Reserve(BGDL_PATH_FOLDER_LENGTH + 1);
	if (ret != 0)
		return ret;
	sce_paf_snprintf(m_begin, m_end - m_begin, "%s", path.c_str());
	return 0;
}

/* (0x810FCF36) the task's download folder, -1 and ids from 0 on ux0:, the others on ur0: */
int BgdlTaskPath::initialize(int taskId, SceSize size)
{
	int ret;

	ret = Reserve(size);
	if (ret != 0)
		return ret;
	if (taskId == -2)
		sce_paf_snprintf(m_begin, size, "ur0:bgdl/t/%08x", taskId);
	else if (taskId == -1)
		sce_paf_snprintf(m_begin, size, "ux0:bgdl/t/%08x", taskId);
	else if (taskId >= 0)
		sce_paf_snprintf(m_begin, size, "ux0:bgdl/t/%08x", taskId);
	else
		sce_paf_snprintf(m_begin, size, "ur0:bgdl/t/%08x", taskId);
	return 0;
}

/* (0x810FCFEE) a file in the folder */
char *BgdlTaskPath::SetName(const char *name)
{
	sce_paf_strncpy(m_begin + BGDL_PATH_FOLDER_LENGTH, name, sce_paf_strlen(name) + 1);
	return m_begin;
}

/* (0x810FD00E) a file in the folder, by a format; the buffer grows to hold it */
int BgdlTaskPath::format(const char *format, ...)
{
	va_list ap;
	int room;
	int len;
	int ret;

	va_start(ap, format);
	room = m_end - m_begin - BGDL_PATH_FOLDER_LENGTH;
	len = sce_paf_vsnprintf(m_begin + BGDL_PATH_FOLDER_LENGTH, room, format, ap);
	if (len >= 0 && len < room)
		return 0;
	BgdlTaskPath path;

	ret = path.Reserve(len + room + 1);
	if (ret != 0)
		return ret;
	sce_paf_strncpy(path.m_begin, m_begin, path.m_end - path.m_begin);
	sce_paf_vsnprintf(path.m_begin + BGDL_PATH_FOLDER_LENGTH, len + 1, format, ap);
	Swap(path);
	return 0;
}

/* (0x810FD106) the folder alone */
char *BgdlTaskPath::ClearName(void)
{
	sce_paf_strncpy(m_begin + BGDL_PATH_FOLDER_LENGTH, "", sce_paf_strlen("") + 1);
	return m_begin;
}

/* (0x810FD12E) */
bool BgdlTaskPath::Exists(const char *path)
{
	if (path == NULL)
		return false;
	return Dir::IsExist(path);
}

/* (0x810FD13C) */
bool BgdlTaskPath::Exists(void)
{
	if (m_begin == NULL)
		return false;
	return Dir::IsExist(m_begin);
}


/* (0x810FD14C) */
void BgdlProgress::Start(SceUInt64 total, SceUInt64 base, SceUInt64 reserved, SceUInt64 interval)
{
	m_done = 0;
	m_startTime = 0;
	m_lastTime = 0;
	m_reportTime = 0;
	m_reserved0 = 0;
	m_reserved1 = 0;
	m_total = total;
	m_base = base;
	m_current = 0;
	m_interval = interval;
	m_startTime = get_time_internal();
	m_reportTime = m_startTime;
}

/* (0x810FD19A) 1 when an interval has passed since the last report */
int BgdlProgress::Update(SceUInt64 current)
{
	int ret = 0;
	SceUInt64 now;

	now = get_time_internal();
	m_lastTime = now;
	m_current = current;
	m_done = current - m_base;
	if (now - m_reportTime > m_interval) {
		ret = 1;
		m_reportTime = now;
	}
	return ret;
}

/* (0x810FD1F2) */
SceUInt64 BgdlProgress::GetSpeed(void)
{
	SceUInt64 speed = 0;
	SceUInt64 elapsed;

	if (m_total == (SceUInt64)-1 || m_total == 0)
		return speed;
	if (m_done == (SceUInt64)-1 || m_done == 0)
		return speed;
	elapsed = m_lastTime - m_startTime;
	if (elapsed == 0)
		return speed;
	speed = (SceUInt64)((double)m_done / (double)elapsed * 1000000.0);
	return speed;
}

/* (0x810FD2F0) */
SceUInt64 BgdlProgress::GetRemaining(void)
{
	SceUInt64 speed;

	speed = GetSpeed();
	if (speed > 0)
		return (m_total - m_current) / speed;
	return 0;
}

/* (0x810FD328) */
int bgdlStringToInt(const string &str)
{
	int value;

	sce_paf_memset(&value, 0, sizeof(value));
	if (str.length() != 0)
		return sce_paf_atoi(str.c_str());
	return value;
}

/* (0x810FD366) */
int bgdlUIntToString(const SceUInt32 *value, string *str)
{
	char buf[16] = "";
	SceUInt32 v;

	sce_paf_memset(&v, 0, sizeof(v));
	v = *value;
	sce_paf_snprintf(buf, sizeof(buf) - 1, "%u", v);
	str->assign(buf, sce_paf_strlen(buf));
	return 0;
}

/* (0x810FD412) */
int bgdlGetInt(const int *value)
{
	int v;

	sce_paf_memset(&v, 0, sizeof(v));
	v = *value;
	return v;
}

/* (0x810FD442) */
int bgdlParseInt(const string &str)
{
	if (str.length() != 0)
		return sce_paf_atoi(str.c_str());
	return 0;
}

/* (0x810FD454) */
int bgdlFormatUInt(SceUInt32 value, string *str)
{
	char buf[16] = "";

	sce_paf_snprintf(buf, sizeof(buf) - 1, "%u", value);
	str->assign(buf, sce_paf_strlen(buf));
	return 0;
}

/* the first of [first, last) whose id is not below the task's (Dinkumware's lower_bound) */
static inline EseTask **bgdlLowerBound(EseTask **first, EseTask **last, EseTask *const &task)
{
	int count = last - first;

	while (count > 0) {
		int half = count / 2;
		EseTask **mid = first + half;

		if ((*mid)->id < task->id) {
			first = ++mid;
			count -= half + 1;
		} else {
			count = half;
		}
	}
	return first;
}

/* (0x810FD4EA) */
int BgdlTaskList::insert(EseTask *task)
{
	BgdlArray<EseTask *> list;
	EseTask **first;
	EseTask **last;
	EseTask **dst;
	EseTask **it;
	int count;
	int half;
	int ret;

	ret = list.Reserve((char *)m_end - (char *)m_begin + sizeof(EseTask *));
	if (ret != 0)
		return ret;
	first = m_begin;
	last = m_end;
	it = first;
	count = ((char *)last - (char *)first) / (int)sizeof(EseTask *);
	while (count > 0) {
		half = count / 2;
		if (it[half]->id < task->id) {
			it += half + 1;
			count = count - half - 1;
		} else {
			count = half;
		}
	}
	if (it != last && !(task->id < (*it)->id))
		return BGDL_ERROR_ALREADY_EXISTS;
	dst = list.m_begin;
	while (first != it)
		*dst++ = *first++;
	*dst++ = task;
	while (it != last)
		*dst++ = *it++;
	Swap(list);
	return 0;
}

/* (0x810FD5E6) */
void BgdlTaskList::Erase(EseTask **pos)
{
	EseTask **it;

	if (pos >= m_end)
		return;
	for (it = pos + 1; it != m_end; it++)
		it[-1] = *it;
	Reserve((char *)m_end - (char *)m_begin - sizeof(EseTask *));
}

/* (0x810FD64E) */
EseTask **BgdlTaskList::Find(EseTask *task)
{
	EseTask **it;

	it = bgdlLowerBound(m_begin, m_end, task);
	if (it != m_end && task->id < (*it)->id)
		return m_end;
	return it;
}

/* (0x810FD6BA) */
void BgdlTaskList::Clear(void)
{
	EseTask **it;

	for (it = m_begin; it != m_end; it++) {
		if (*it != NULL) {
			delete *it;
			*it = NULL;
		}
	}
	Reserve(0);
}

/* (0x810FD758) the flags in mask made value's, once the thread has seen the request */
void BgdlManager::UpdateFlags(SceUInt32 value, SceUInt32 mask)
{
	SceUInt32 flags;
	SceUInt64 request;
	bool done;

	if (m_error != 0)
		return;
	flags = (m_flags & ~mask) | (value & mask);
	if (m_flags == flags)
		return;
	m_taskListMutex.Lock();
	request = ++s_requested;
	if (m_running != 0) {
		m_cancelMutex.Lock();
		if (!m_cancelled) {
			m_cancelled = true;
			if (m_cancelHandler != NULL)
				m_cancelHandler(m_cancelHandlerArg);
		}
		m_cancelMutex.Unlock();
	}
	m_taskListCond.Notify();
	m_taskListMutex.Unlock();
	for (;;) {
		m_taskListMutex.Lock();
		done = s_handled >= request;
		m_taskListMutex.Unlock();
		if (done)
			break;
		thread::Sleep(16);
	}
	m_taskListMutex.Lock();
	m_taskListMutex.Unlock();
	m_flags = flags;
	m_taskListMutex.Lock();
	s_wakeups++;
	m_taskListCond.Notify();
	m_taskListMutex.Unlock();
}

/* (0x810FD862) */
int bgdlManagerNotify(int value, int mask)
{
	s_updateMutex.Lock();
	if (s_manager != NULL) {
		s_manager->UpdateFlags(value, mask);
		s_updateMutex.Unlock();
		return 0;
	}
	s_updateMutex.Unlock();
	return BGDL_ERROR_NOT_STARTED;
}

/* (0x810FD8A4) */
int Job_EseSetPermission::Run()
{
	SceUInt32 mask = m_mask;
	SceUInt32 value = m_value;

	s_updateMutex.Lock();
	if (s_manager != NULL)
		s_manager->UpdateFlags(value, mask);
	s_updateMutex.Unlock();
	return 0;
}

/* (0x810FD8E0) */
BgdlManager *bgdlManagerGet(void)
{
	return s_manager;
}

/* (0x810FD8EC) */
bool BgdlManager::IsCancelled(void)
{
	bool cancelled;

	m_cancelMutex.Lock();
	cancelled = m_cancelled;
	m_cancelMutex.Unlock();
	return cancelled;
}

/* (0x810FD90C) */
void BgdlManager::NotifyListeners(BgdlEvent *event)
{
	BgdlListener *listener;

	m_listenersMutex.Lock();
	if (m_listenerCount != 0) {
		for (listener = m_listeners; listener != m_listeners + BGDL_LISTENER_MAX; listener++) {
			if (listener->func != NULL)
				listener->func(event, listener->arg);
		}
	}
	m_listenersMutex.Unlock();
}

/* (0x810FD94C) */
int EseTask::setRunStatus(int status)
{
	BgdlEvent event;
	int ret;

	if (m_ParamCache.runStatus == status)
		return 0;
	m_ParamCache.runStatus = status;
	ret = Bgdl_811233A4(id, 101, &status, sizeof(status));
	event.type = 4;
	event.id = id;
	event.param = m_ParamCache;
	event.result = result;
	event.reserved = 0;
	event.value = value;
	s_manager->NotifyListeners(&event);
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x810FDA08) */
int EseTask::setState(int state, int *current, bool force)
{
	BgdlEvent event;
	int saved = 0;
	int ret;

	ret = Bgdl_8112260C(id, 109, &saved, sizeof(saved));
	if (ret != 0)
		return ret;
	if (current != NULL)
		*current = saved;
	if (state > saved || force == true) {
		m_ParamCache.state = state;
		if (current != NULL)
			*current = state;
		ret = Bgdl_811233A4(id, 109, &state, sizeof(state));
		event.type = 3;
		event.id = id;
		event.result = 0;
		event.reserved = 0;
		event.value = 0;
		event.param = m_ParamCache;
		if (m_ParamCache.state == 4)
			event.value = value;
		s_manager->NotifyListeners(&event);
	}
	return ret;
}

/* (0x810FDAEC) the task's folder: the one its param file names (key 229), or its own */
int EseTask::GetFolder(BgdlTaskPath *path, bool fromParam)
{
	BgdlTaskPath folder;
	int ret;

	if (fromParam == true) {
		string name;

		ret = Bgdl_8112278A(id, 229, &name);
		if (ret == 0 && name.length() != 0) {
			const char *src = name.c_str();
			SceSize len = sce_paf_strlen(src);

			if (path->Reserve(len + 1) == 0) {
				sce_paf_memcpy(path->m_begin, src, len);
				path->m_begin[len] = '\0';
			}
			return 0;
		}
	}
	ret = folder.initialize(id, 52);
	if (ret != 0)
		return ret;
	path->Swap(folder);
	return 0;
}

/* (0x810FDC1C) */
EseTask *BgdlManager::FindTask(SceUInt32 id)
{
	EseTask **first = m_tasks.m_begin;
	EseTask **last = m_tasks.m_end;
	int count = last - first;

	while (count > 0) {
		int half = count / 2;
		EseTask **mid = first + half;

		if ((*mid)->id < id) {
			first = ++mid;
			count -= half + 1;
		} else {
			count = half;
		}
	}
	if (first != last && id < (*first)->id)
		first = last;
	if (first == last)
		return NULL;
	return *first;
}

/* (0x810FDC72) */
string bgdlTaskFolder(SceUInt32 id, bool fromParam)
{
	string folder("");

	if (s_manager != NULL) {
		EseTask *task = s_manager->FindTask(id);

		if (task != NULL) {
			BgdlTaskPath path;

			if (task->GetFolder(&path, fromParam) == 0)
				folder = path.m_begin;
		}
	}
	return folder;
}
