/*
 * esetask.cpp: the background download library's tasks and their manager (0x810FCD10 to the
 * file's static initialiser at 0x81110620). Sony's 0.990 prototype names the file
 * (vsh/app/shell/src/esecron/src/esetask.cpp) and some functions; the other names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <dir/dir.h>
#include <timer/timer.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <vshbridge/vshbridge.h>

#include "../include/esebase.hpp"
#include "../include/esetask.hpp"
#include "../include/eseparam.hpp"

#include "shell.h"
#include "console_info/console_info.h"
#include "src/livespace_db/lsdb_keys.h"

using namespace paf;

/* the task's param file (eseparam.cpp; PLACEHOLDER names: not written yet) */
int Bgdl_8112260C(SceUInt32 id, int key, void *value, SceSize size);		/* read */
int Bgdl_811233A4(SceUInt32 id, int key, const void *value, SceSize size);	/* write */
int Bgdl_8112278A(SceUInt32 id, int key, string *value);			/* read a string */
int Bgdl_81122700(SceUInt32 id, int key, BgdlTaskPath *value);		/* read a string into a path */

int bgdlMtpRunCopy(const MtpBgCopyRunParam *param);	/* bgdl_mtp.cpp */
int bgdlMtpCancelCopy(SceUInt32 id);			/* bgdl_mtp.cpp */
int Bgdl_811120A0(BgdlTaskPath *path, bool recursive);	/* removes a folder (PLACEHOLDER) */
int Bgdl_81112112(string *path, bool recursive);

/* the next file's (0x8113A434): the object whose job queue runs the library's jobs */
class BgdlJobDispatcher {	/* PLACEHOLDER names */
public:
	static BgdlJobDispatcher *Bgdl_8113A434(void);
	int Bgdl_8113A4FC(common::SharedPtr<job::JobItem> &job);
};

int Bgdl_812A9954(const char *titleId, int kind);	/* PLACEHOLDER: another file's */

/* the next file's (0x8111076A): game_plugin held while ux0:pspemu is changed (PLACEHOLDER) */
class BgdlGamePluginLock {
public:
	BgdlGamePluginLock(bool lock);
	~BgdlGamePluginLock();

	bool m_locked;
};
bool Bgdl_81112284(const char *path);			/* the path is there (LocalFile::Getstat) */

/* the next file's (0x81110808): the Marlin licence folder made while the manager starts */
class BgdlLicenseFolder {	/* PLACEHOLDER names */
public:
	void Bgdl_81110808(void);
	void Bgdl_8111080E(void);
	void Bgdl_8111081C(void);

	bool m_active;
};

/* PLACEHOLDER name: the copy did not complete (libmtp's errors other than its 0x80546600 group) */
#define BGDL_ERROR_COPY_FAILED		0x80103A24

/* PLACEHOLDER name: the task's param file names no file (key 203 empty) */
#define BGDL_ERROR_NO_FILE		0x80103A1F

static BgdlManager *s_manager = NULL;		/* (0x81544788) */
static const char *s_np = "np";			/* (0x8154478C) PLACEHOLDER: read later */
static SceUInt64 s_handled = 0;			/* (0x81544790) the requests the thread took */
static SceUInt64 s_wakeupsHandled = 0;		/* (0x81544798) the wakeups the thread took */
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
	if (m_runningTask != NULL) {
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
			path->assign(name.c_str());
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

/* the task's folder as a string, empty when no such task is listed (inlined where it is used) */
static __attribute__((always_inline)) string taskFolder(SceUInt32 id, bool fromParam)
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

/* (0x810FDC72) */
string bgdlTaskFolder(SceUInt32 id, bool fromParam)
{
	return taskFolder(id, fromParam);
}

/*
 * (0x810FDD12) the task's file (key 203): in the folder its param file names
 * (key 229) when fromParam is set and one is named, else in its own folder
 */
int EseTask::getFilePath(BgdlTaskPath *path, bool fromParam)
{
	BgdlTaskPath file;
	BgdlTaskPath full;
	int ret;

	if (fromParam == true) {
		string folder;

		ret = Bgdl_8112278A(id, 229, &folder);
		if (ret == 0 && folder.length() != 0) {
			string name;
			const char *src;

			ret = Bgdl_8112278A(id, 203, &name);
			if (ret != 0)
				return ret;
			src = folder.c_str();
			if (src[sce_paf_strlen(src)] != '/')
				folder += "/";
			folder.append(name);
			path->assign(folder.c_str());
			return 0;
		}
	}
	ret = Bgdl_81122700(id, 203, &file);
	if (ret != 0)
		return ret;
	if (file.m_end == file.m_begin)
		return BGDL_ERROR_NO_FILE;
	ret = full.initialize(id, file.m_end - file.m_begin + 20);
	if (ret != 0)
		return ret;
	ret = full.format("/%s", file.m_begin);
	if (ret != 0)
		return ret;
	path->Swap(full);
	return 0;
}

/* (0x810FE038) */
bool BgdlManager::SetCancelHandler(void (*handler)(void *arg), void *arg)
{
	bool cancelled;

	m_cancelMutex.Lock();
	cancelled = m_cancelled;
	if (cancelled == false) {
		m_cancelHandler = handler;
		m_cancelHandlerArg = arg;
	}
	m_cancelMutex.Unlock();
	return cancelled == false;
}

static void bgdlManagerOnResume(int code, const void *data, SceSize size, void *arg);
static void bgdlMtpNotify(void *arg, const MtpBgCopyProgress *progress);
static int bgdlMtpAccept(void);

/* libmtp asks whether the copy is to stop (PLACEHOLDER name) */
class BgdlMtpCopyHost : public MtpBgCopyHost {
public:
	BgdlMtpCopyHost(EseTask *task) : m_task(task)
	{
	}

	virtual int isRequested()
	{
		{
			thread::RMutexLock lock(&s_manager->m_cancelMutex);
		}
		return s_manager->IsCancelled();
	}

	EseTask *m_task;
};

/* (0x810FE06C) an MTP task: libmtp copies its file, and the result goes to its param file */
int EseTask::copyMtp(int *runStatus)
{
	BgdlManager *manager = s_manager;
	bool done;
	int ret = 0;
	int current;

	manager->SetCancelHandler(NULL, NULL);
	current = 0;
	setState(1, &current, false);
	thread::Sleep(500);
	setState(2, &current, false);
	thread::Sleep(500);

	BgdlMtpCopyHost host(this);
	MtpBgCopyRunParam param;

	param.id = id;
	param.arg = this;
	param.notify = bgdlMtpNotify;
	param.accept = bgdlMtpAccept;
	param.host = &host;
	param.file = "";
	{
		BgdlTaskPath file;
		BgdlTaskPath folder;

		ret = getFilePath(&file, false);
		if (ret == 0 && file.m_end != file.m_begin)
			param.file = file.m_begin;
		param.path = taskFolder(id, false);
		ret = GetFolder(&folder, true);
		if (ret == 0)
			param.paramFolder = folder.m_begin;
	}

	ret = bgdlMtpRunCopy(&param);
	*runStatus = 0;
	switch (ret) {
	case 0:
	case (int)0x80546600:
	case (int)0x80546611:
	case (int)0x80546621:
		break;
	default:
		ret = BGDL_ERROR_COPY_FAILED;
		return ret;
	}

	done = false;
	if (ret == 0) {
		setState(4, NULL, false);
		done = true;
	}
	m_ParamCache.runStatus = *runStatus;
	m_ParamCache.key102 = done;
	m_ParamCache.ready = done;
	{
		EseParam file(id);

		if (file.openWrite() == 0 && file.write(101, runStatus, sizeof(*runStatus)) == 0 &&
		    file.write(104, &ret, sizeof(ret)) == 0 &&
		    file.write(102, &done, sizeof(done)) == 0)
			file.commit();
	}

	BgdlEvent event;

	event.type = 3;
	event.id = id;
	event.param = m_ParamCache;
	event.result = ret;
	event.reserved = 0;
	event.value = 0;
	s_manager->NotifyListeners(&event);
	return ret;
}

/*
 * (0x810FE448) runs the task by its type; a task that failed while it was to run again
 * waits up to 5 seconds, or until it is cancelled, before the next one runs
 */
int EseTask::invoke(void)
{
	SceUInt64 start = get_time_internal();
	int status;
	int ret;
	int next;

	ret = setRunStatus(2);
	status = ret;
	if (ret == 0) {
		next = 1;
		switch (m_ParamCache.type) {
		case 0:
			ret = execute(&next);
			break;
		case 1:
			ret = copyMtp(&next);
			break;
		default:
			ret = 0;
			next = 0;
			break;
		}
		if (next == 0) {
			result = ret;
			Bgdl_811233A4(id, 104, &ret, sizeof(ret));
		}
		setRunStatus(next);
		status = ret;
		if (status == 0)
			return 0;
	}
	if (status != 0 && m_ParamCache.runStatus == 1) {
		float deadline = (float)start + 5000000.0f;

		while (!s_manager->IsCancelled() && (float)get_time_internal() < deadline)
			thread::Sleep(16);
	}
	return status;
}

/* a task is done: no error, not to run again, key 113 set */
static inline bool taskIsReady(const EseTask *task)
{
	return task->result == 0 && task->m_ParamCache.runStatus == 0 && task->m_ParamCache.key113;
}

/*
 * (0x810FE5A4) the manager's list of its tasks and appmgr's background download queue made
 * again from the task list (only while ux0: is there)
 */
void BgdlManager::UpdateQueueStatus(void)
{
	EseTask **it;

	m_hasPending = false;
	for (it = m_tasks.m_begin; it != m_tasks.m_end; it++) {
		EseParamCache param = (*it)->m_ParamCache;

		if (param.key107 == 5 || param.key111 == 2) {
			m_hasPending = true;
			break;
		}
	}
	if (m_ux0Ready == false)
		return;

	m_taskListMutex.Lock();
	sce_paf_memset(&m_summaries, 0, sizeof(m_summaries));
	sce_paf_memset(m_queue, 0, sizeof(m_queue));
	{
		BgdlTaskPackage package;
		BgdlTaskSummary *entry = m_summaries.entries;
		int count = 0;

		for (it = m_tasks.m_begin; it != m_tasks.m_end; it++) {
			EseTask *task = *it;

			if (task->m_ParamCache.type != 0)
				continue;
			s_packageMutex.Lock();
			package = task->package;
			s_packageMutex.Unlock();
			if (package.key220.length() == 0)
				continue;

			entry->param.type = (*it)->m_ParamCache.type;
			entry->param.key107 = (*it)->m_ParamCache.key107;
			entry->param.runStatus = (*it)->m_ParamCache.runStatus;
			entry->param.key102 = (*it)->m_ParamCache.key102;
			entry->param.ready = (*it)->m_ParamCache.ready;
			entry->param.key108 = (*it)->m_ParamCache.key108;
			entry->param.state = (*it)->m_ParamCache.state;
			entry->param.key112 = (*it)->m_ParamCache.key112;
			entry->param.key113 = (*it)->m_ParamCache.key113;
			entry->param.key110 = (*it)->m_ParamCache.key110;
			entry->param.key111 = (*it)->m_ParamCache.key111;
			entry->param.kind = (*it)->m_ParamCache.kind;
			entry->param.key115 = (*it)->m_ParamCache.key115;
			entry->param.key116 = (*it)->m_ParamCache.key116;
			entry->param.reserved = (*it)->m_ParamCache.reserved;
			entry->ready = taskIsReady(*it);
			sce_paf_snprintf(entry->titleId, sizeof(entry->titleId), "%s",
					 package.key220.c_str());
			if (package.key217.length() != 0)
				sce_paf_snprintf(entry->contentId, sizeof(entry->contentId), "%s",
						 package.key217.c_str());
			if (count < BGDL_QUEUE_MAX) {
				if (package.key217.length() != 0)
					sce_paf_snprintf(m_queue[count].contentId,
							 sizeof(m_queue[count].contentId) - 1, "%s",
							 package.key217.c_str());
				if (entry->param.key107 == 9)
					m_queue[count].state = 1;
				else if (taskIsReady(*it))
					m_queue[count].state = 1;
				else
					m_queue[count].state = 0;
				switch (entry->param.key107) {
				case 12:
					m_queue[count].type = 4;
					break;
				case 9:
					m_queue[count].type = 3;
					break;
				case 8:
					m_queue[count].type = 1;
					break;
				case 7:
					m_queue[count].type = 2;
					break;
				default:
					m_queue[count].type = 0;
					break;
				}
			}
			count++;
			entry++;
		}
		vshAppMgrBgdlSetQueueStatus(m_queue);
	}
	m_taskListMutex.Unlock();
}

/*
 * (0x810FE8F0) a task removed: its MTP copy cancelled, or its PSP download in
 * ux0:pspemu/bgdl removed, then its folder, and the listeners told (event 1)
 */
int BgdlManager::deleteTask(SceUInt32 id, const EseParamCache *param)
{
	BgdlTaskPath folder;
	int ret;

	ret = folder.initialize(id, 20);
	if (ret != 0)
		return ret;
	{
		string path("");
		EseParam file(id);

		if (file.open() == 0) {
			if (file.read(229, &path) != 0 || path.length() == 0) {
				if (param->type == 1)
					bgdlMtpCancelCopy(id);
			} else if (sce_paf_strncmp(path.c_str(), "ux0:pspemu/bgdl", 15) == 0) {
				BgdlGamePluginLock lock(true);

				Bgdl_81112112(&path, true);
			}
		}
	}

	EseParam lock(id);

	ret = 0;
	if (folder.Exists() && (ret = Bgdl_811120A0(&folder, true)) != 0)
		return ret;

	BgdlEvent event;

	event.type = 1;
	event.id = id;
	event.param = *param;
	event.result = 0;
	event.reserved = 0;
	event.value = 0;
	NotifyListeners(&event);
	return ret;
}

/* (0x810FEA80) a finished task counted in ur0:ci by its kind (PLACEHOLDER name) */
void bgdlCountFinished(const EseTask *task)
{
	EseParamCache param;

	if (task == NULL)
		return;
	param = task->m_ParamCache;
	if (param.runStatus != 0)
		return;
	if (task->result != 0 && !param.key113)
		return;
	if (param.type != 0)
		return;
	switch (param.key107) {
	case 1:
		consoleInfoFileAddInt(41, 1, 0);
		break;
	case 2:
		consoleInfoFileAddInt(40, 1, 0);
		break;
	case 3:
	case 4:
		consoleInfoFileAddInt(39, 1, 0);
		break;
	case 6:
		switch (param.kind) {
		case 0:
			consoleInfoFileAddInt(32, 1, 0);
			break;
		case 1:
		case 2:
			consoleInfoFileAddInt(33, 1, 0);
			break;
		default:
			consoleInfoFileAddInt(38, 1, 0);
			break;
		}
		break;
	case 7:
	case 8:
		consoleInfoFileAddInt(38, 1, 0);
		break;
	default:
		consoleInfoFileAddInt(43, 1, 0);
		break;
	}
}

/* (0x810FEB22) a finished application or content download turns its auto download off */
void bgdlUnsetAutoDownload(const EseTask *task)
{
	SceUInt32 kind;

	if (task == NULL)
		return;
	kind = task->m_ParamCache.key107;
	if (kind != 12 && kind != 8 && kind != 7 && kind != 6)
		return;
	if (task->m_ParamCache.key107 == 8)
		return;

	BgdlTaskPackage package;

	s_packageMutex.Lock();
	package = task->package;
	s_packageMutex.Unlock();
	if (package.key217.length() != 0) {
		Job_EseUnsetAutoDownload *item =
			new Job_EseUnsetAutoDownload(package.key217.c_str());
		common::SharedPtr<job::JobItem> job(item);

		BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A4FC(job);
	}
}

/*
 * (0x810FED12) a task's run is over: it is removed when it ran without error and is done (or while
 * libmtp's copy may go on), otherwise counted and its follow-up jobs queued; the listeners
 * are told (event 3) and the queue made again
 */
int BgdlManager::finishTask(EseTask *task, int result)
{
	SceUInt32 id = task->id;
	SceUInt64 value = task->value;
	EseParamCache param;
	bool removed = false;
	bool notify = false;
	bool removeRunning = false;
	int ret = 0;

	m_taskListMutex.Lock();
	m_cancelMutex.Lock();
	if (m_cancelled)
		m_lastId = id - 1;
	m_cancelled = false;
	m_cancelHandler = NULL;
	m_cancelHandlerArg = NULL;
	m_cancelMutex.Unlock();
	m_runningTask = NULL;
	if ((result == 0 && task->m_ParamCache.key102) || m_removeRunning) {
		EseTask **pos = m_tasks.Find(task);

		if (pos != m_tasks.m_end) {
			task->m_ParamCache.ready = !m_removeRunning;
			param = task->m_ParamCache;
			bgdlCountFinished(task);
			delete *pos;
			m_tasks.Erase(pos);
			removed = true;
			notify = true;
		}
	} else if (result == 0) {
		task->m_ParamCache.ready = !task->m_ParamCache.key113;
		param = task->m_ParamCache;
		bgdlCountFinished(task);
		if (task->m_ParamCache.key110)
			bgdlUnsetAutoDownload(task);
		notify = true;
		if (task->m_ParamCache.key107 == 6 || task->m_ParamCache.key107 == 12) {
			Job_EseGDEvent *item = new Job_EseGDEvent(id);
			common::SharedPtr<job::JobItem> job(item);

			BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A4FC(job);
		}
		if (task->m_ParamCache.key107 == 8) {
			BgdlTaskPackage package;

			s_packageMutex.Lock();
			package = task->package;
			s_packageMutex.Unlock();
			if (package.key220.length() != 0)
				Bgdl_812A9954(package.key220.c_str(), 5);
		}
	}
	m_taskListMutex.Unlock();

	if (notify) {
		BgdlEvent event;

		event.type = 3;
		event.id = id;
		event.param = param;
		event.result = ret;
		event.reserved = ret;
		event.value = value;
		NotifyListeners(&event);
	}
	if (removed) {
		deleteTask(id, &param);
		m_taskListMutex.Lock();
		m_removeRunning = removeRunning;
		m_taskListMutex.Unlock();
	}
	UpdateQueueStatus();
	return ret;
}

/* (0x810FF102) */
bool BgdlManager::IsSuspended(void)
{
	return m_suspended;
}

/*
 * (0x810FF108) the next task to run, once every request is taken and tasks may run (flag 2):
 * the first of the lowest priority (key 108) that is to run, after the last one run, else
 * from the start of the list
 */
bool BgdlManager::NextTask(EseTask **next)
{
	EseTask *task = NULL;
	EseTask **first;
	EseTask **last;
	EseTask **it;
	int priority;

	if (s_manager->m_suspended == true)
		return false;
	s_handled = s_requested;
	s_wakeupsHandled = s_wakeups;
	if (s_requested > s_wakeups)
		return false;
	if ((m_flags & 2) == 0)
		return false;

	first = m_tasks.m_begin;
	last = m_tasks.m_end;
	for (priority = 0; priority <= 3; priority++) {
		for (it = m_tasks.m_begin; it != last; it++) {
			if ((*it)->m_ParamCache.key108 == priority && (*it)->m_ParamCache.runStatus == 1 &&
			    m_lastId < (*it)->id) {
				task = *it;
				break;
			}
		}
		if (task != NULL)
			break;
		for (it = m_tasks.m_begin; it != last; it++) {
			if ((*it)->m_ParamCache.key108 == priority && (*it)->m_ParamCache.runStatus == 1 &&
			    (*it)->id <= m_lastId) {
				task = *it;
				break;
			}
		}
		if (task != NULL)
			break;
	}
	if (task == NULL)
		return false;
	m_runningTask = task;
	m_lastId = task->id;
	*next = task;
	return true;
}

/* (0x810FF214) the tasks run one at a time, from the system's resume on, until it is stopped */
void BgdlManagerThread::EntryFunction()
{
	BgdlManager *manager = m_manager;

	if (manager == NULL)
		return;
	manager->UpdateQueueStatus();
	while (manager->m_resumed == false)
		thread::Sleep(1000);
	for (;;) {
		EseTask *task;

		manager->m_taskListMutex.Lock();
		task = NULL;
		while (manager->m_stopping == false && manager->NextTask(&task) == false)
			manager->m_taskListCond.Wait(0);
		manager->m_taskListMutex.Unlock();
		if (task == NULL)
			break;
		manager->finishTask(task, task->invoke());
	}
}

/* the first task whose id is not below id */
static inline EseTask **bgdlLowerBoundId(EseTask **first, EseTask **last, SceUInt32 id)
{
	int count = last - first;

	while (count > 0) {
		int half = count / 2;

		if ((*(first + half))->id >= id) {
			count = half;
		} else {
			first += half + 1;
			count -= half + 1;
		}
	}
	return first;
}

/* (0x810FF2AE) */
Job_EseRegistPatchByTitleID::Job_EseRegistPatchByTitleID(const BgdlTaskPackage &package,
							 const bool *flag)
	: paf::job::JobItem("Job_EseRegistPatchByTitleID", NULL), m_flag(false)
{
	m_package = package;
	m_flag = flag != NULL ? *flag : false;
}

/*
 * (0x810FF37C) a task removed by id: at once when it is not running, else the running task is
 * cancelled and the thread waited for (finishTask removes it)
 */
int BgdlManager::RemoveTask(SceUInt32 id)
{
	BgdlTaskPackage package;
	EseParamCache param;
	EseTask **pos;
	EseTask *task;
	bool marked;

	m_taskListMutex.Lock();
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end) {
		m_taskListMutex.Unlock();
		return 0;
	}
	m_taskListMutex.Unlock();

	task = *pos;
	marked = false;
	if (task->m_removing == false) {
		marked = true;
		task->m_removing = true;
	} else if (task->m_removing == true) {
		return 0;
	}

	m_taskListMutex.Lock();
	if (*pos != m_runningTask) {
		EseTask *removed = *pos;
		int ret;

		if (removed != NULL) {
			param = removed->m_ParamCache;
			s_packageMutex.Lock();
			package = removed->package;
			s_packageMutex.Unlock();
			delete *pos;
		}
		m_tasks.Erase(pos);
		m_taskListMutex.Unlock();
		ret = deleteTask(id, &param);
		if (param.key107 == 8 && ret == 0 && package.key220.length() != 0)
			Bgdl_812A9954(package.key220.c_str(), 2);
		UpdateQueueStatus();
		return 0;
	}

	m_removeRunning = true;
	m_cancelMutex.Lock();
	if (m_cancelled == false) {
		void (*handler)(void *arg) = m_cancelHandler;

		m_cancelled = true;
		if (handler != NULL)
			handler(m_cancelHandlerArg);
	}
	m_cancelMutex.Unlock();
	while (m_removeRunning) {
		m_taskListMutex.Unlock();
		thread::Sleep(200);
		m_taskListMutex.Lock();
	}
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end && marked == true)
		marked = false;
	m_taskListMutex.Unlock();
	if (marked == true)
		task->m_removing = false;
	return 0;
}

/* (0x810FF6CC) the running task's status, for the shell to show */
void BgdlManager::SetStatus(const BgdlTaskStatus *status)
{
	m_progressMutex.Lock();
	memcpy(&m_status, status, 60);
	m_progressMutex.Unlock();
}

/* (0x8110401E) */
BgdlManager::BgdlManager()
	: reserved0(0),
	  m_taskListMutex("SceBgdlMutexEseTaskList", 0),
	  m_taskListCond(m_taskListMutex, "SceBgdlCondEseTaskList", 0),
	  m_cancelMutex("SceBgdlMutexEseCancel", 0),
	  m_listenersMutex("SceBgdlMutexEseListeners", 0),
	  m_syncHandlerMutex("SceBgdlRMutexEseSyncHandler", 0),
	  m_progressMutex("SceBgdlRMutexProgerss", 0),
	  m_error(0x80103A15),
	  m_stopping(false),
	  m_removeRunning(false),
	  m_cancelled(false),
	  m_cancelHandler(NULL),
	  m_cancelHandlerArg(NULL),
	  m_runningTask(NULL),
	  m_lastId(0),
	  m_nextId(0),
	  m_nextInternalId(0x80000000),
	  m_flags(0),
	  m_ux0Ready(true),
	  reserved5(false),
	  m_resumed(false),
	  m_suspended(false),
	  m_copyResult(0),
	  m_hasPending(false),
	  m_listenerCount(0)
{
	sce_paf_memset(m_listeners, 0, sizeof(m_listeners));
	sce_paf_memset(reserved9, 0, sizeof(reserved9));
	sce_paf_memset(reserved10, 0, sizeof(reserved10));
	m_ux0Ready = Bgdl_81112284("ux0:");
	SceShell_7FE4E732(SHELL_EVENT_POWER, bgdlManagerOnResume, this);
	sce_paf_memset(m_queue, 0, sizeof(m_queue));
}

/* (0x811041EC) the manager made once: ur0:'s tasks, then ux0:'s when it is there */
void bgdlManagerCreate(bool clean, const int *quota)
{
	s_updateMutex.Lock();
	if (s_manager == NULL) {
		BgdlManager *manager = new BgdlManager;
		int ux0Quota;
		int ur0Quota;
		BgdlLicenseFolder license;

		s_manager = manager;
		ux0Quota = -1;
		ur0Quota = -2;
		if (quota != NULL) {
			ux0Quota = quota[0];
			ur0Quota = quota[1];
		}
		license.Bgdl_81110808();
		license.Bgdl_8111081C();
		license.Bgdl_8111080E();
		manager->prepare_device("ur0:", clean, ur0Quota);
		if (manager->m_ux0Ready == true && manager->prepare_device("ux0:", clean, ux0Quota) == 0)
			manager->Bgdl_811033C4();
		manager->Bgdl_81103224(clean);
	}
	s_updateMutex.Unlock();
}

/* (0x81110524) */
static void bgdlManagerOnResume(int code, const void *data, SceSize size, void *arg)
{
	if (s_manager != NULL)
		s_manager->m_resumed = true;
}

/* (0x81110538) libmtp's progress, kept in the task */
static void bgdlMtpNotify(void *arg, const MtpBgCopyProgress *progress)
{
	EseTask *task = (EseTask *)arg;

	if (task != NULL && progress != NULL) {
		BgdlTaskCopy copy;

		copy.name = progress->name;
		copy.progress = progress->progress;
		copy.number = progress->number;
		copy.total = progress->total;
		copy.remaining = progress->remaining;
		copy.reserved = progress->reserved;
		task->copyResult = 0;
		task->copy = copy;
	}
}

/* (0x8111060C) */
static int bgdlMtpAccept(void)
{
	if (s_manager == NULL)
		return 0;
	return s_manager->m_removeRunning;
}
