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
#include <std/ctype.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <dir/dir.h>
#include <file/localfile.h>
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
#include "src/livespace_db/livespace_db_internal.h"
#include <modulemgr/module_info.h>
#include <kernel/sysmem/memblock.h>
#include "../../../../game/game_plugin/game_plugin.h"

using namespace paf;

/* the task's param file (eseparam.cpp; PLACEHOLDER names: not written yet) */
int Bgdl_8112260C(SceUInt32 id, int key, void *value, SceSize size);		/* read */
int Bgdl_811233A4(SceUInt32 id, int key, const void *value, SceSize size);	/* write */
int Bgdl_8112278A(SceUInt32 id, int key, string *value);			/* read a string */
int Bgdl_81122700(SceUInt32 id, int key, BgdlTaskPath *value);		/* read a string into a path */

int bgdlMtpRunCopy(const MtpBgCopyRunParam *param);	/* bgdl_mtp.cpp */
int bgdlMtpCancelCopy(SceUInt32 id);			/* bgdl_mtp.cpp */
int bgdlMtpGetCopyProgress(SceUInt32 id, MtpBgCopyProgress *progress);	/* bgdl_mtp.cpp */
int Bgdl_811120A0(BgdlTaskPath *path, bool recursive);	/* removes a folder (PLACEHOLDER) */
int Bgdl_81112112(string *path, bool recursive);
int Bgdl_81123BC0(const char *titleId, LsdbAppInfoPtr *info);	/* PLACEHOLDER: the title's row */
int Bgdl_81113898(const LsdbAppInfoPtr *info, int *kind);	/* PLACEHOLDER: its storage kind */
int Bgdl_811121FE(const char *path, int mode);
int Bgdl_8111212C(const char *folder, const char *name);	/* PLACEHOLDER: folder/name removed */
SceUInt64 Bgdl_81112310(SceUInt64 size, SceUInt32 cluster);	/* size in whole clusters (PLACEHOLDER) */
int Bgdl_8111234C(const char *device, BgdlStorageInfo *info);	/* the device's space (PLACEHOLDER) */
int Bgdl_8111240E(const char *path, SceUInt64 size);		/* a file made that size (PLACEHOLDER) */
int Bgdl_81111724(const char *path, int flag, SceUInt64 reserved);	/* PLACEHOLDER: removes a file */
int Bgdl_81111CDC(const char *url, const char *folder, string *out, bool flag);	/* PLACEHOLDER */
string Bgdl_81113C72(SceUInt32 id, bool flag);			/* PLACEHOLDER: a game's folder */
int Bgdl_811137F6(bool mount);					/* PLACEHOLDER: grw0: */
void Bgdl_81122168(bool clean);					/* PLACEHOLDER */
extern "C" int sceNpCommonGetNpEnviroment(char *environment);
void Bgdl_81113890(int value);					/* PLACEHOLDER */

/* the PSP emulator's system software version (sceCompatGetPspSystemSoftwareVersion) */
extern "C" int SceCompatForVsh_9C107CC9(void);
extern "C" int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *info);

/*
 * Another file's (0x812A99AE on): the game update check as the shell links it, its context,
 * its parameters and what it finds (PLACEHOLDER names).
 */
struct BgdlUpdateParam {
	void *(*malloc)(size_t size);
	void (*free)(void *ptr);
	int mode;			/* 0; 1: a background patch; 2: an internal one */
	char titleId[16];
	char appVer[8];
	char contentId[48];
	SceUInt32 systemVersion;
	SceUInt32 pspVersion;
	char key235[13];		/* set by the check */
	char padding[7];
};

struct BgdlUpdateCore {
	void *work[3];
};

struct BgdlUpdateState {
	SceSize size;
	int state;			/* 2: a patch to get */
	char appVer[8];
};

struct BgdlUpdateInfo {
	char reserved[20];
	char name[384];
	SceUInt32 count;		/* the patch's files */
	SceUInt32 flags;		/* 0x80: not to get */
};

struct BgdlUpdateEntry {
	SceUInt64 size;
	char digest[20];
	char name[276];
};

int Bgdl_812A99AE(BgdlUpdateCore *core, int option, const BgdlUpdateParam *param);
int Bgdl_812A9BB0(BgdlUpdateCore *core, BgdlUpdateState *state, BgdlUpdateInfo *info);
int Bgdl_812A985C(BgdlUpdateCore *core, SceUInt32 index, BgdlUpdateEntry *entry);
int Bgdl_812A9708(BgdlUpdateCore *core);

/* a title a patch is looked for (PLACEHOLDER names) */
/* another file's (constructor 0x81114642): a package file's check (PLACEHOLDER) */
class BgdlPackageCheck {
public:
	BgdlPackageCheck();
	void Bgdl_81114AFC(int mode, int kind, const char *name, int arg3, int arg4, int arg5,
			   int arg6, int arg7);
	int Bgdl_8118FD0C(void);
	void Bgdl_81115C22(char *digest);

	int reserved0;
	int reserved1;
	string reserved2;
	string reserved3;
	string reserved4;
	int reserved5;
	int reserved6;
	char reserved7[32];
	string reserved8;
	bool reserved9;
	bool reserved10;
	string reserved11;
};		/* makes a folder (PLACEHOLDER) */

/* the next file's (0x8113A434): the object whose job queue runs the library's jobs */
/* the dispatcher's state, as Bgdl_8113A47C copies it (PLACEHOLDER) */
struct BgdlDispatcherState {
	int state;			/* 0: idle */
	int reserved[3];
};

class BgdlJobDispatcher {	/* PLACEHOLDER names */
public:
	static BgdlJobDispatcher *Bgdl_8113A434(void);
	void Bgdl_8113A47C(BgdlDispatcherState *state, bool second);
	int Bgdl_8113A4FC(common::SharedPtr<job::JobItem> &job);
};

int Bgdl_812A9954(const char *titleId, int kind);	/* PLACEHOLDER: another file's */

/* the next file's (0x8111076A): game_plugin held while ux0:pspemu is changed (PLACEHOLDER) */
class BgdlGamePluginLock {
public:
	BgdlGamePluginLock(bool lock);
	~BgdlGamePluginLock();
	void Lock(void);		/* (0x8111072E) */

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
/* the next file's (0x81110708): the time a block of code took (PLACEHOLDER names) */
class BgdlScopeTimer {
public:
	BgdlScopeTimer(const char *name) : m_name(name)
	{
		m_start = get_time_internal();
	}

	~BgdlScopeTimer()
	{
		m_end = get_time_internal();
		Report();
	}

	void Report(void);		/* (0x81110708) the time taken, in milliseconds */

	string m_name;
	SceUInt64 m_start;
	SceUInt64 m_end;
	SceUInt64 m_elapsed;
};

/* PLACEHOLDER name: a task found still running when its device was prepared */
#define BGDL_ERROR_INTERRUPTED		0x80103A44

/* an NP environment's PSM runtime server (PLACEHOLDER names) */
struct BgdlPsmServer {
	char env[17];			/* sceNpCommonGetNpEnviroment's name */
	char url[256];
};

#define BGDL_PSM_SERVER_MAX	7

/* (0x815099F8) PLACEHOLDER names: the PSM runtime's server for each NP environment */
static const BgdlPsmServer s_psmServers[BGDL_PSM_SERVER_MAX] = {
	{ "np", "http://psm-runtime.np.dl.playstation.net/psm-runtime/" },
	{ "mgmt", "http://psm-runtime.mgmt.dl.playstation.net/psm-runtime/" },
	{ "e1-np", "http://psm-runtime.e1-np.dl.playstation.net/psm-runtime/" },
	{ "sp-int", "http://psm-runtime.np.dl.playstation.net/psm-runtime/" },
	{ "prod-qa", "http://psm-runtime.np.dl.playstation.net/psm-runtime/" },
	{ "kurokawa", "http://192.168.92.250/download/psm/" },
	{ "", "" },
};
static const BgdlPsmServer *s_psmServer = &s_psmServers[0];	/* (0x8154478C) the console's */
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
			if (file.read(229, &path) == 0 && path.length() != 0) {
				if (sce_paf_strncmp(path.c_str(), "ux0:pspemu/bgdl", 15) == 0) {
					BgdlGamePluginLock lock(true);

					Bgdl_81112112(&path, true);
				}
			} else if (param->type == 1) {
				bgdlMtpCancelCopy(id);
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

/* a key the param file may leave out */
static inline bool bgdlKeyOptional(int ret)
{
	return ret == 0 || ret == (int)BGDL_ERROR_NO_KEY;
}

/* a key the param file may leave out or have empty */
static inline bool bgdlKeyOptionalOrEmpty(int ret)
{
	return ret == 0 || ret == (int)BGDL_ERROR_NO_KEY || ret == (int)BGDL_ERROR_INVALID_ARGUMENT;
}

/* (0x810FF6F4) the task's state read from its param file, its progress made from it */
int EseTask::Load(void)
{
	bool key102, key110, key112, key113, key115, key116;
	int type, key107, runStatus, taskResult, key108, state, key111, kind;
	int fileNumber, fileCount;
	int ret;

	result = BGDL_ERROR_NOT_LOADED;
	m_ParamCache.type = 15;
	m_ParamCache.key107 = 0;
	m_ParamCache.runStatus = 0;
	m_ParamCache.key102 = 0;
	m_ParamCache.ready = 0;
	m_ParamCache.key108 = 1;
	m_ParamCache.state = 0;
	m_ParamCache.key112 = 0;
	m_ParamCache.key113 = 0;
	m_ParamCache.key110 = 0;
	m_ParamCache.kind = 0;
	m_ParamCache.reserved = 0;

	EseParam file(id);

	ret = file.open();
	if (ret != 0)
		return ret;
	ret = file.read(100, &type, 4);
	if (ret != 0)
		return ret;
	ret = file.read(107, &key107, 4);
	if (ret != 0)
		return ret;
	ret = file.read(101, &runStatus, 4);
	if (ret != 0)
		return ret;
	ret = file.read(102, &key102, 1);
	if (ret != 0)
		return ret;
	ret = file.read(104, &taskResult, 4);
	if (ret != 0)
		return ret;
	key108 = 1;
	file.read(108, &key108, 4);
	state = 0;
	ret = file.read(109, &state, 4);
	if (!bgdlKeyOptional(ret))
		return ret;
	key110 = false;
	ret = file.read(110, &key110, 1);
	if (!bgdlKeyOptional(ret))
		return ret;
	key112 = false;
	ret = file.read(112, &key112, 1);
	if (!bgdlKeyOptional(ret))
		return ret;
	key113 = false;
	ret = file.read(113, &key113, 1);
	if (!bgdlKeyOptionalOrEmpty(ret))
		return ret;
	key111 = 7;
	ret = file.read(111, &key111, 4);
	if (!bgdlKeyOptional(ret))
		return ret;
	kind = 0;
	ret = file.read(114, &kind, 4);
	if (!bgdlKeyOptionalOrEmpty(ret))
		return ret;
	key115 = false;
	ret = file.read(115, &key115, 1);
	if (!bgdlKeyOptionalOrEmpty(ret))
		return ret;
	key116 = false;
	ret = file.read(116, &key116, 1);
	if (!bgdlKeyOptionalOrEmpty(ret))
		return ret;

	BgdlTaskPath key220;

	ret = file.read(220, &key220);
	if (ret == 0) {
		if (key220.m_end != key220.m_begin)
			package.key220 = key220.m_begin;
	} else if (ret != (int)BGDL_ERROR_NO_KEY && ret != (int)BGDL_ERROR_INVALID_ARGUMENT) {
		return ret;
	}
	{
		BgdlTaskPath key217;

		ret = file.read(217, &key217);
		if (ret == 0) {
			if (key217.m_end != key217.m_begin)
				package.key217 = key217.m_begin;
		} else if (ret != (int)BGDL_ERROR_NO_KEY && ret != (int)BGDL_ERROR_INVALID_ARGUMENT) {
			return ret;
		}
	}

	result = taskResult;
	m_ParamCache.type = type;
	m_ParamCache.key107 = key107;
	m_ParamCache.runStatus = runStatus;
	m_ParamCache.key102 = key102;
	m_ParamCache.key108 = key108;
	m_ParamCache.state = state;
	m_ParamCache.key110 = key110;
	m_ParamCache.key112 = key112;
	m_ParamCache.kind = kind;
	m_ParamCache.key115 = key115;
	m_ParamCache.key116 = key116;
	m_ParamCache.key113 = key113;
	m_ParamCache.ready = (taskResult | key113) == 0;
	m_ParamCache.key111 = key111;

	if (m_ParamCache.type == 0) {
		SceUInt64 size = 0;
		SceUInt64 current = 0;
		SceUInt64 filesTotal;
		SceUInt64 filesBase;
		int i;

		file.read(208, &current, 8);
		file.read(206, &size, 8);
		status.current = current;
		status.total = size;
		status.id = id;
		transfer.Start(size, current, 0, 512000);

		fileNumber = 0;
		file.read(500, &fileNumber, 4);
		fileCount = -1;
		file.read(501, &fileCount, 4);
		if (fileNumber == 0 || fileCount == -1)
			return 0;
		filesTotal = 0;
		ret = file.read(505, &filesTotal, 8);
		if (ret != 0)
			return ret;
		filesBase = 0;
		for (i = 0; i < fileCount; i++) {
			ret = file.read(5002 + 32 * i, &size, 8);
			if (ret != 0)
				return ret;
			filesBase += size;
		}
		status.fileCount = fileCount + 1;
		status.fileNumber = fileNumber;
		status.filesBase = filesBase;
		status.filesCurrent = filesBase;
		status.filesTotal = filesTotal;
		status.filesRemaining = 0;
		s_manager->SetStatus(&status);

		files.Start(filesTotal, filesBase, 0, 512000);
		status.id = id;
		status.current = current;
		if (transfer.Update(current))
			status.remaining = transfer.GetRemaining();
		if (status.fileNumber >= 2) {
			status.filesCurrent = status.filesBase + current;
			if (files.Update(status.filesCurrent))
				status.filesRemaining = files.GetRemaining();
		}
		s_manager->SetStatus(&status);
	} else if (m_ParamCache.type == 1) {
		MtpBgCopyProgress progress;

		if (bgdlMtpGetCopyProgress(id, &progress) == 0) {
			BgdlTaskCopy current;

			current.name = progress.name;
			current.progress = progress.progress;
			current.number = progress.number;
			current.total = progress.total;
			current.remaining = progress.remaining;
			current.reserved = progress.reserved;
			copyResult = 0;
			copy = current;
		}
	}
	return 0;
}

/* (0x810FFF22) a task read again from its param file; a task more urgent than the running one stops it */
int BgdlManager::ReloadTask(SceUInt32 id)
{
	EseTask **pos;
	EseTask *task;
	int ret;

	m_taskListMutex.Lock();
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_NO_TASK;
	}
	task = *pos;
	if (m_runningTask == task ||
	    (task->m_ParamCache.runStatus == 0 && task->m_ParamCache.key113)) {
		m_taskListMutex.Unlock();
		return 0;
	}
	ret = task->Load();
	if (ret != 0) {
		m_taskListMutex.Unlock();
		return ret;
	}
	if (task->m_ParamCache.runStatus != 0 || task->result != 0) {
		ret = task->setRunStatus(1);
		if (ret != 0) {
			m_taskListMutex.Unlock();
			return ret;
		}
		if (m_runningTask != NULL &&
		    m_runningTask->m_ParamCache.key108 > task->m_ParamCache.key108) {
			m_cancelMutex.Lock();
			if (m_cancelled == false) {
				void (*handler)(void *arg) = m_cancelHandler;

				m_cancelled = true;
				if (handler != NULL)
					handler(m_cancelHandlerArg);
			}
			m_cancelMutex.Unlock();
		}
	} else {
		BgdlEvent event;

		event.type = 3;
		event.id = id;
		event.param = task->m_ParamCache;
		event.result = 0;
		event.reserved = 0;
		event.value = 0;
		NotifyListeners(&event);
	}
	m_taskListCond.Notify();
	m_taskListMutex.Unlock();
	return 0;
}

/* (0x811000D4) */
int BgdlManager::AddTask(SceUInt32 *id, int type, SceUInt32 key111, const char *key105,
			 bool key115, bool key116)
{
	SceUInt32 count;
	SceUInt32 temp;
	SceUInt32 *next;
	int ret;
	int i;

	m_taskListMutex.Lock();
	count = m_tasks.m_end - m_tasks.m_begin;
	if ((key116 == true && count >= BGDL_QUEUE_MAX - 2) || count >= BGDL_QUEUE_MAX) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_QUEUE_FULL;
	}
	m_taskListMutex.Unlock();

	/* the keys are written in a work folder first */
	if (key111 > 1) {
		temp = (SceUInt32)-1;
		if (key111 == 2)
			temp = (SceUInt32)-2;
	} else {
		temp = (SceUInt32)-1;
	}
	if (*id >= (SceUInt32)-2 && temp != *id)
		temp = *id;
	{
		BgdlTaskPath work;

		ret = work.initialize(temp, 20);
		if (ret != 0)
			return ret;
		if (work.Exists())
			Bgdl_811120A0(&work, true);
		ret = Bgdl_811121FE(work.m_begin, 6);
		if (ret != 0)
			return ret;
	}
	{
		EseParam file(temp);
		int runStatus, key107, result, key108, state, kind;
		bool key102, key110, key112, key113;

		ret = file.openWrite();
		if (ret != 0)
			return ret;
		ret = file.write(100, &type, 4);
		if (ret != 0)
			return ret;
		runStatus = 0;
		ret = file.write(101, &runStatus, 4);
		if (ret != 0)
			return ret;
		key102 = false;
		ret = file.write(102, &key102, 1);
		if (ret != 0)
			return ret;
		key107 = 0;
		ret = file.write(107, &key107, 4);
		if (ret != 0)
			return ret;
		result = BGDL_ERROR_NOT_LOADED;
		ret = file.write(104, &result, 4);
		if (ret != 0)
			return ret;
		key108 = 1;
		ret = file.write(108, &key108, 4);
		if (ret != 0)
			return ret;
		state = 0;
		ret = file.write(109, &state, 4);
		if (ret != 0)
			return ret;
		key110 = false;
		ret = file.write(110, &key110, 1);
		if (ret != 0)
			return ret;
		key112 = false;
		ret = file.write(112, &key112, 1);
		if (ret != 0)
			return ret;
		key113 = false;
		ret = file.write(113, &key113, 1);
		if (ret != 0)
			return ret;
		kind = 0;
		ret = file.write(114, &kind, 4);
		if (ret != 0)
			return ret;
		ret = file.write(115, &key115, 1);
		if (ret != 0)
			return ret;
		ret = file.write(116, &key116, 1);
		if (ret != 0)
			return ret;
		ret = file.write(111, &key111, 4);
		if (ret != 0)
			return ret;
		if (key105 != NULL && sce_paf_strlen(key105) != 0) {
			ret = file.write(105, key105, sce_paf_strlen(key105) + 1);
			if (ret != 0)
				return ret;
		}
		ret = file.commit();
		if (ret != 0)
			return ret;
	}

	/* then the folder takes the next free id */
	next = &m_nextId;
	if (temp == (SceUInt32)-2)
		next = &m_nextInternalId;
	++*next;

	BgdlTaskPath folder;

	{
		BgdlTaskPath work;

		ret = work.initialize(temp, 20);
		if (ret != 0)
			return ret;
		for (i = 0; i < 16; i++) {
			char *from;
			char *to;

			ret = folder.initialize(*next, 20);
			if (ret != 0)
				return ret;
			if (folder.Exists())
				Bgdl_811120A0(&folder, true);
			from = work.ClearName();
			to = folder.ClearName();
			ret = LocalFile::RenameFile(from, to);
			if (ret == 0)
				break;
			++*next;
			thread::Sleep(10);
		}
		if (ret != 0)
			return ret;
	}

	EseTask *task = new EseTask(*next);

	if (task == NULL) {
		if (folder.Exists())
			Bgdl_811120A0(&folder, true);
		return BGDL_ERROR_NO_MEMORY;
	}
	ret = task->Load();
	if (ret != 0) {
		if (folder.Exists())
			Bgdl_811120A0(&folder, true);
		delete task;
		return ret;
	}

	BgdlEvent event;

	event.type = 0;
	event.id = *next;
	event.param = task->m_ParamCache;
	event.result = 0;
	event.reserved = 0;
	event.value = 0;
	m_taskListMutex.Lock();
	ret = m_tasks.insert(task);
	if (ret == 0)
		m_taskListCond.Notify();
	m_taskListMutex.Unlock();
	if (ret != 0) {
		if (folder.Exists())
			Bgdl_811120A0(&folder, true);
		delete task;
		return ret;
	}
	NotifyListeners(&event);
	*id = *next;
	return 0;
}

/* (0x8110088A) */
int BgdlManager::CheckStorage(const char *titleId, bool *grw0)
{
	int kind;
	int ret;

	*grw0 = false;
	if (titleId == NULL)
		return BGDL_ERROR_NULL_TITLE;

	LsdbAppInfoPtr info(new LsdbAppInfo());

	if (Bgdl_81123BC0(titleId, &info) != 0)
		return BGDL_ERROR_NO_APP;
	ret = Bgdl_81113898(&info, &kind);
	if (ret != 0)
		return ret;
	if (kind <= 1) {
		if (kind <= 0)
			return BGDL_ERROR_BAD_STORAGE;
		if (m_ux0Ready == true)
			return 0;
		if (Bgdl_81112284("grw0:")) {
			*grw0 = true;
			return 0;
		}
		return BGDL_ERROR_NO_STORAGE;
	}
	if (kind > 4)
		return BGDL_ERROR_BAD_STORAGE;
	if (m_ux0Ready != true)
		return BGDL_ERROR_NO_UX0;
	return 0;
}

/* the game plugin's function 38 (0x8100A00C; PLACEHOLDER: game_plugin.h does not name it yet) */
#define BGDL_GAME_PLUGIN_FUNC_38	38

/* (0x81100B0A) PLACEHOLDER name: 0, or BGDL_ERROR_BUSY while the dispatcher is not idle */
int bgdlDispatcherIdle(void)
{
	BgdlDispatcherState state;

	BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A47C(&state, false);
	if (state.state != 0 || state.state == 3)
		return BGDL_ERROR_BUSY;
	return 0;
}

/* (0x81100B4C) PLACEHOLDER name: the game plugin's function 38 */
int bgdlGamePluginCheck(void)
{
	Plugin *plugin = Plugin::Find("game_plugin");
	int ret;

	if (plugin == NULL) {
		ret = BGDL_ERROR_NO_MEMORY;
	} else {
		GamePluginInterface *game = (GamePluginInterface *)plugin->GetInterface(1);

		ret = BGDL_ERROR_NO_MEMORY;
		if (game != NULL)
			ret = ((int (*)(void))game->func[BGDL_GAME_PLUGIN_FUNC_38])();
	}
	if (ret == (int)0x80010010)
		return BGDL_ERROR_GAME_PLUGIN;
	return ret;
}

/* (0x81100B98) PLACEHOLDER name: the space the request's package files take on the device */
int bgdlPackageSize(const BgdlPackageRequest *request, const BgdlStorageInfo *info,
		    SceUInt64 *size)
{
	common::SharedPtr<File> list;
	int ret = 0;

	*size = 0;
	list = LocalFile::Open(request->listPath, SCE_O_RDONLY | 0x10000, 6, &ret);
	if (ret == 0) {
		BgdlFileCloser closer(list);
		BgdlPackageEntry entry;
		SceUInt32 i;

		for (i = 0; ; i++) {
			sce_paf_memset(&entry, 0, sizeof(entry));
			if (list->Read(&entry, sizeof(entry)) <= 0)
				break;
			*size += Bgdl_81112310(entry.size, info->clusterSize) + info->clusterSize;
		}
		if (request->fileCount > i)
			ret = BGDL_ERROR_LIST_SHORT;
		else
			ret = 0;
	}
	return ret;
}

/* (0x81100E8C) */
int BgdlManager::AddPackageTask(const BgdlPackageRequest *request, BgdlPackageResult *result)
{
	bool grw0 = false;
	bool key102 = false;
	int ret = 0;
	BgdlTaskPath folder;
	SceUInt64 needed = 0;
	SceUInt64 firstSize = 0;
	BgdlTaskPackage package;
	int check = 0;
	SceUInt32 id = (SceUInt32)-1;
	const char *device;

	Bgdl_81113890(1);
	if (request == NULL || result == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
		goto out;
	}
	if (request->titleId[0] == '\0' || sce_paf_strlen(request->titleId) == 0) {
		ret = BGDL_ERROR_NO_TITLE_ID;
		goto out;
	}
	if (request->listPath[0] == '\0') {
		ret = BGDL_ERROR_NULL_TITLE;
		goto out;
	}
	if (LocalFile::GetFileSize(request->listPath) <= 0) {
		ret = BGDL_ERROR_NO_LIST;
		goto out;
	}
	ret = CheckStorage(request->titleId, &grw0);
	if (ret == (int)BGDL_ERROR_NO_APP) {
		if (m_ux0Ready != true)
			goto out;
		ret = 0;
		grw0 = false;
	} else if (ret != 0) {
		goto out;
	}
	{
		BgdlStorageInfo info;

		if (grw0 == true) {
			ret = bgdlDispatcherIdle();
			if (ret != 0)
				goto out;
			device = "grw0:";
		} else {
			device = "ux0:";
		}
		if (grw0 == true) {
			Bgdl_811137F6(false);
			ret = bgdlGamePluginCheck();
			if (ret != 0)
				goto out;
		}
		ret = Bgdl_8111234C(device, &info);
		if (ret != 0)
			goto out;
		ret = bgdlPackageSize(request, &info, &needed);
		if (ret != 0)
			goto out;
		if (info.free < needed) {
			ret = BGDL_ERROR_UX0_FULL;
			if (grw0 == true)
				ret = BGDL_ERROR_GRW0_FULL;
			result->needed = needed;
			goto out;
		}
		id = bgdlWorkId(request->type, true);
		if (grw0 == true) {
			if (id == (SceUInt32)-1)
				id = (SceUInt32)-2;
			if (!Bgdl_81112284("grw0:")) {
				ret = BGDL_ERROR_NO_STORAGE;
				goto out;
			}
			ret = Bgdl_811137F6(true);
			if (ret != 0)
				goto out;
		}
		{
			string name("");

			if (request->name[0] != '\0')
				name = request->name;
			ret = AddTask(&id, 0, request->type, name.c_str(),
				      (request->flags & BGDL_REQUEST_FLAG_KEY115) != 0,
				      (request->flags & BGDL_REQUEST_FLAG_KEY116) != 0);
			if (ret != 0)
				goto done;
			ret = folder.initialize(id, 20);
			if (ret != 0)
				goto done;

			EseParam file(id);
			bool sized, key218, key205;
			int key501, key502, key114;

			ret = file.openWrite();
			if (ret != 0)
				goto done;
			sized = grw0 != true;
			ret = file.write(112, &sized, 1);
			if (ret != 0)
				goto done;
			key218 = request->type == 0;
			ret = file.write(218, &key218, 1);
			if (ret != 0)
				goto done;
			ret = file.write(102, &key102, 1);
			if (ret != 0)
				goto done;
			key205 = false;
			ret = file.write(205, &key205, 1);
			if (ret != 0)
				goto done;
			package.reserved2 = 0;
			package.reserved3 = 0;
			if (request->titleId[0] != '\0' && sce_paf_strlen(request->titleId) != 0) {
				ret = file.write(220, request->titleId, sce_paf_strlen(request->titleId) + 1);
				if (ret != 0)
					goto done;
				package.key220 = request->titleId;
			}
			if (request->contentId[0] != '\0' && sce_paf_strlen(request->contentId) != 0) {
				ret = file.write(217, request->contentId,
						 sce_paf_strlen(request->contentId) + 1);
				if (ret != 0)
					goto done;
				package.key217 = request->contentId;
			}
			if (request->key235[0] != '\0' && sce_paf_strlen(request->key235) != 0) {
				ret = file.write(235, request->key235, sce_paf_strlen(request->key235) + 1);
				if (ret != 0)
					goto done;
			}

			string icon;

			if (request->iconPath[0] != '\0') {
				ret = Bgdl_81111CDC(request->iconPath, folder.m_begin, &icon, true);
				if (ret != 0)
					icon.assign((const char *)NULL, 0);
			}
			if (icon.length() != 0) {
				ret = file.write(106, icon.c_str(), icon.length() + 1);
				if (ret != 0)
					goto done;
			}
			ret = file.write(503, &request->key503, 2);
			if (ret != 0)
				goto done;
			ret = file.write(504, &request->key504, 2);
			if (ret != 0)
				goto done;
			ret = file.write(500, &request->fileCount, 4);
			if (ret != 0)
				goto done;
			key501 = -1;
			ret = file.write(501, &key501, 4);
			if (ret != 0)
				goto done;
			key502 = 0;
			ret = file.write(502, &key502, 4);
			if (ret != 0)
				goto done;
			key114 = request->kind == 1;
			ret = file.write(114, &key114, 4);
			if (ret != 0)
				goto done;

			common::SharedPtr<File> list;

			list = LocalFile::Open(request->listPath, SCE_O_RDONLY | 0x10000, 6, &ret);
			if (ret != 0)
				goto done;

			BgdlFileCloser closer(list);
			char fileName[32] = { 0 };
			char number[8] = { 0 };
			string dest("");
			string gameFolder("");
			SceUInt64 extra = 0;
			SceUInt64 total = 0;
			BgdlGamePluginLock lock(false);
			BgdlPackageEntry entry;
			SceUInt32 i;
			int k;

			if (grw0 == true) {
				dest = "grw0:fgdl";
			} else {
				dest = folder.m_begin;
				extra = 0x2000000;
				if (request->kind == 1) {
					lock.Lock();
					gameFolder = Bgdl_81113C72(id, true);
					if (gameFolder.length() != 0)
						dest = gameFolder.c_str();
					else
						dest = "";
					if (dest.length() == 0) {
						ret = BGDL_ERROR_NO_GAME_FOLDER;
						goto done;
					}
				}
			}
			dest += "/";
			dest += "p";
			k = 0;
			for (i = 0; ; i++) {
				sce_paf_memset(&entry, 0, sizeof(entry));
				if (list->Read(&entry, sizeof(entry)) <= 0)
					break;
				total += entry.size;
				needed += Bgdl_81112310(entry.size + extra, info.clusterSize) +
					  info.clusterSize;
				ret = file.write(5002 + k, &entry.size, 8);
				if (ret != 0)
					break;
				ret = file.write(5000 + k, entry.name, sce_paf_strlen(entry.name) + 1);
				if (ret != 0)
					break;
				ret = file.write(5001 + k, entry.digest, 20);
				if (ret != 0)
					break;
				{
					BgdlPackageCheck packageCheck;
					char digest[32];

					packageCheck.Bgdl_81114AFC(grw0 ? 5 : 6, 2, entry.name, 0, 0, 0, 1, 0);
					check = packageCheck.Bgdl_8118FD0C();
					packageCheck.Bgdl_81115C22(digest);
					ret = file.write(5004 + k, &check, 4);
					if (ret == 0)
						ret = file.write(5005 + k, digest, 32);
					if (ret != 0)
						break;
				}
				sce_paf_snprintf(fileName, 31, "f_%04d_%02d.pkg", i, 0);
				ret = file.write(5003 + k, fileName, sce_paf_strlen(fileName) + 1);
				if (ret != 0)
					break;
				sce_paf_snprintf(number, 7, "%04d", i);
				{
					string path;

					path = dest;
					path += "/";
					path += number;
					if (Dir::IsExist(path.c_str()) == 1)
						Dir::RemoveRecursive(path.c_str());
					ret = Bgdl_811121FE(path.c_str(), 6);
					if (ret != 0)
						break;
					if (sized) {
						path += "/";
						path += fileName;
						Bgdl_8111240E(path.c_str(), entry.size + extra);
					}
					if (i == 0)
						firstSize = entry.size;
				}
				k += 32;
			}
			if (request->fileCount > i) {
				ret = BGDL_ERROR_LIST_SHORT;
				goto done;
			}
			ret = 0;
			if (gameFolder.length() != 0) {
				ret = file.write(229, gameFolder.c_str(),
						 sce_paf_strlen(gameFolder.c_str()) + 1);
				if (ret != 0)
					goto done;
			}
			ret = file.write(206, &firstSize, 8);
			if (ret != 0)
				goto done;

			SceUInt64 current = 0;

			ret = file.write(208, &current, 8);
			if (ret != 0)
				goto done;
			ret = file.write(505, &total, 8);
			if (ret != 0)
				goto done;
			if (grw0 == true) {
				int key108;

				ret = file.write(110, &grw0, 1);
				if (ret != 0)
					goto done;
				key108 = 0;
				ret = file.write(108, &key108, 4);
				if (ret != 0)
					goto done;

				string fgdl("grw0:fgdl");

				ret = file.write(229, fgdl.c_str(), sce_paf_strlen(fgdl.c_str()) + 1);
				if (ret != 0)
					goto done;
			}

			int key107 = 8;

			ret = file.write(107, &key107, 4);
			if (ret != 0)
				goto done;

			EseTask *task = FindTask(id);

			if (task == NULL) {
				ret = BGDL_ERROR_NO_TASK;
				goto done;
			}
			task->m_ParamCache.key107 = key107;
			task->m_ParamCache.key102 = key102;
			task->status.id = task->id;
			task->status.current = current;
			task->status.total = firstSize;
			task->transfer.Start(firstSize, current, 0, 512000);
			s_packageMutex.Lock();
			task->package = package;
			s_packageMutex.Unlock();
			if (grw0 == true)
				task->m_ParamCache.key110 = 1;
			ret = file.commit();
		}
done:
		;
	}
	if (ret == 0 && grw0 == false)
		ret = ReloadTask(id);
	if (ret == 0 && result != NULL)
		result->id = id;
out:
	if (ret != 0) {
		RemoveTask(id);
	} else {
		UpdateQueueStatus();
		if (package.key220.length() != 0)
			Bgdl_812A9954(package.key220.c_str(), 0);
	}
	if (grw0 == true) {
		if (ret != 0)
			Bgdl_811137F6(false);
		else
			ret = 1;
	}
	if (request != NULL && request->listPath[0] != '\0')
		Bgdl_81111724(request->listPath, 0, 29600);
	return ret;
}

/* (0x8110255E) PLACEHOLDER name */
int bgdlAddPackageTask(const BgdlPackageRequest *request, BgdlPackageResult *result)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->AddPackageTask(request, result);
}

/* (0x81102582) PLACEHOLDER name: the work folder of a device's tasks, "ux0:bgdl/w" or "ur0:bgdl/w" */
int bgdlWorkFolder(string *path, int device)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (device > 0 && device <= 2)
		*path = "ux0:bgdl";
	else
		*path = "ur0:bgdl";
	*path += "/";
	*path += "w";
	return 0;
}

/* (0x811026A0) PLACEHOLDER name: a device's patch list in its work folder */
int bgdlPatchFile(string *path, int device)
{
	int ret;

	ret = bgdlWorkFolder(path, device);
	if (ret != 0)
		return ret;
	*path += "/";
	switch (device) {
	case 1:
		*path += "patch_internal.bin";
		break;
	case 2:
		*path += "patch_bg.bin";
		break;
	default:
		*path += "patch.bin";
		break;
	}
	return ret;
}

/* (0x81102868) PLACEHOLDER name: the update check's parameters for a title */
int bgdlUpdateParamInit(const BgdlTaskPackage *title, const bool *disc, LsdbAppInfoPtr *app,
			int mode, BgdlUpdateParam *param)
{
	SceKernelSystemSwVersion version = { 0 };
	const char *appVer;
	const char *titleId;
	const char *contentId;
	int pspVersion;
	int ret;

	if (param == NULL)
		return -1;
	sce_paf_memset(param, 0, sizeof(*param));
	pspVersion = SceCompatForVsh_9C107CC9();
	sce_paf_memset(&version, 0, sizeof(version));
	version.size = sizeof(version);
	ret = sceKernelGetSystemSwVersion(&version);
	if (ret < 0)
		return ret;
	appVer = "01.00";
	titleId = title->key220.c_str();
	contentId = title->key217.c_str();
	if (*disc != true) {
		LsdbAppInfo *info = app->get();

		if (info != NULL) {
			LsdbAppInfo::iterator it = info->find(LSDB_KEY_APP_VER);

			if (it != info->end()) {
				if (it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL)
					appVer = it->second.text->c_str();
				else
					appVer = "";
			}
		}
	}
	if (titleId != NULL)
		sce_paf_strncpy(param->titleId, titleId, 13);
	if (appVer != NULL)
		sce_paf_strncpy(param->appVer, appVer, 6);
	if (contentId != NULL)
		sce_paf_strncpy(param->contentId, contentId, 48);
	switch (mode) {
	case 1:
		param->mode = 2;
		break;
	case 2:
		param->mode = 1;
		break;
	default:
		param->mode = 0;
		break;
	}
	param->systemVersion = version.version;
	param->pspVersion = pspVersion;
	param->malloc = sce_paf_malloc;
	param->free = sce_paf_free;
	return 0;
}

/* (0x811029E6) PLACEHOLDER name: a title's patch looked for, and a task made to get it */
int bgdlRegistPatch(const BgdlTaskPackage *title, const bool *disc, LsdbAppInfoPtr *app, int mode)
{
	int ret = 0;
	BgdlPackageRequest request;
	BgdlPackageResult result;
	BgdlUpdateParam param;
	BgdlUpdateCore core;
	BgdlUpdateState state;
	BgdlUpdateInfo info;

	sce_paf_memset(&request, 0, sizeof(request));
	sce_paf_memset(&result, 0, sizeof(result));
	ret = bgdlUpdateParamInit(title, disc, app, mode, &param);
	if (ret != 0)
		return ret;
	ret = 0;
	{
		int err = Bgdl_812A99AE(&core, 0, &param);

		if (err != 0)
			return err;
	}
	sce_paf_memset(&state, 0, sizeof(state));
	sce_paf_memset(&info, 0, sizeof(info));
	state.size = sizeof(state);
	ret = Bgdl_812A9BB0(&core, &state, &info);
	if (ret >= 0) {
		if (state.state != 2 || (info.flags & 0x80) != 0) {
			if (state.state > 3) {
				if (state.state > 4)
					ret = 1;
				else
					ret = BGDL_ERROR_PATCH_STATE4;
			} else if (state.state <= 2) {
				ret = 1;
			} else {
				ret = BGDL_ERROR_PATCH_STATE3;
			}
		} else {
			string path;
			char listPath[256];

			ret = bgdlPatchFile(&path, 1);
			if (ret != 0)
				return ret;
			sce_paf_memset(listPath, 0, sizeof(listPath));
			sce_paf_strncpy(listPath, path.c_str(), sizeof(listPath));

			common::SharedPtr<File> list = LocalFile::Open(listPath,
				SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6, &ret);

			if (ret == 0) {
				BgdlFileCloser closer(list);
				SceUInt32 i = 0;

				if (info.count != 0) {
					for (i = 0; ; ) {
						BgdlUpdateEntry update;
						BgdlPackageEntry entry;
						int written;

						sce_paf_memset(&update, 0, sizeof(update));
						ret = Bgdl_812A985C(&core, i, &update);
						sce_paf_memset(&entry, 0, sizeof(entry));
						entry.size = update.size;
						sce_paf_memcpy(entry.digest, update.digest, sizeof(entry.digest));
						sce_paf_snprintf(entry.name, 255, "%s", update.name);
						written = list->Write(&entry, sizeof(entry));
						if (written != sizeof(entry)) {
							ret = written;
							break;
						}
						if (++i >= info.count)
							break;
					}
				}
				if (ret == 0) {
					int major = 0;
					int minor = 0;
					int scale;
					int n;
					const char *p;

					request.fileCount = i;
					request.type = 3;
					request.key503 = 0;
					request.key504 = 0;
					p = state.appVer;
					scale = 1;
					n = 0;
					do {
						if (*p == '\0')
							break;
						n++;
						if (*p == '.')
							break;
						major = *p - '0' + major * scale;
						scale *= 10;
						p++;
						request.key503 = major;
					} while (n < 6);
					scale = 1;
					if (n < 6) {
						p = &state.appVer[n];
						do {
							if (*p == '\0' || *p == '.')
								break;
							minor = *p - '0' + minor * scale;
							n++;
							scale *= 10;
							p++;
							request.key504 = minor;
						} while (n < 6);
					}
					request.kind = mode == 2;
					request.flags &= ~(BGDL_REQUEST_FLAG_KEY115 | BGDL_REQUEST_FLAG_KEY116);
					sce_paf_memcpy(request.listPath, listPath, sizeof(request.listPath));
					if (param.titleId[0] != '\0')
						sce_paf_snprintf(request.titleId, 13, "%s", param.titleId);
					if (param.key235[0] != '\0')
						sce_paf_snprintf(request.key235, 13, "%s", param.key235);
					if (param.contentId[0] != '\0')
						sce_paf_snprintf(request.contentId, 51, "%s", param.contentId);
					if (sce_paf_strncmp(title->key220.c_str(), "PCSI00011", 10) == 0)
						sce_paf_snprintf(request.iconPath, 255, "%s",
								 "vs0:vsh/game/psm/runtime_icon.png");
					if (info.name[0] != '\0') {
						char *q = request.name;

						sce_paf_snprintf(request.name, 256, "%s", info.name);
						if (*q != '\0') {
							do {
								if (*q == '\n')
									*q = ' ';
							} while (*++q != '\0');
						}
					}
				}
			}
		}
	}
	Bgdl_812A9708(&core);
	if (ret == 0 && state.state == 2)
		return bgdlAddPackageTask(&request, &result);
	return ret;
}

/* (0x81102FFE) PLACEHOLDER name: an installed title's patch looked for in the background */
int bgdlRegistTitlePatch(const BgdlTaskPackage *title, const bool *disc)
{
	int ret;

	if (!Bgdl_81112284("ux0:"))
		return BGDL_ERROR_NO_UX0;
	if (title->key220.empty())
		return BGDL_ERROR_NULL_TITLE;
	ret = 0;

	LsdbAppInfoPtr info(new LsdbAppInfo());

	sce_paf_strncmp(title->key220.c_str(), "PCSI00011", 10);
	Bgdl_81123BC0(title->key220.c_str(), &info);
	bgdlRegistPatch(title, disc, &info, 1);
	return ret;
}

/* (0x8110313E) */
int Job_EseRegistPatchByTitleID::Run()
{
	bgdlRegistTitlePatch(&m_package, &m_flag);
	return 0;
}

/* (0x81103154) PLACEHOLDER name: bgdlRegistTitlePatch run by the job queue */
int bgdlRegistPatchByTitleID(const BgdlTaskPackage *package, const bool *disc)
{
	Job_EseRegistPatchByTitleID *item = new Job_EseRegistPatchByTitleID(*package, disc);
	common::SharedPtr<job::JobItem> job(item);

	BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A4FC(job);
	return 0;
}

/*
 * (0x81103224) PLACEHOLDER name: the patch lists made, the PSM runtime's server chosen for the
 * console's NP environment and the DRM install heap allocated
 */
void BgdlManager::Bgdl_81103224(bool clean)
{
	char env[17];
	SceKernelAllocMemBlockOpt opt;
	int i;

	Bgdl_811137F6(false);

	paf::string path("ur0:bgdl/w/patch.bin");

	Bgdl_8111240E(path.c_str(), 29600);
	if (m_ux0Ready == true) {
		paf::string ux0Path("ux0:bgdl/w/patch_internal.bin");

		Bgdl_8111240E(ux0Path.c_str(), 29600);
		ux0Path = "ux0:bgdl/w/patch_bg.bin";
		Bgdl_8111240E(ux0Path.c_str(), 29600);
	}
	Bgdl_81122168(clean);
	sce_paf_memset(env, 0, sizeof(env));
	s_psmServer = &s_psmServers[0];
	if (sceNpCommonGetNpEnviroment(env) == 0) {
		/* Sony's test reads the i-th character of the i-th name */
		for (i = 0; i < BGDL_PSM_SERVER_MAX && s_psmServers[i].env[i] != '\0'; i++) {
			if (sce_paf_strncmp(env, s_psmServers[i].env, sizeof(env)) == 0) {
				s_psmServer = &s_psmServers[i];
				break;
			}
		}
	}
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	opt.alignment = 0x1000;
	opt.reserved = 0;
	opt.strBaseBlockName = NULL;
	m_drmHeap = sceKernelAllocMemBlock("SceNpDrmInstallHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
					   0x28000, &opt);
	if (m_drmHeap < 0)
		m_drmHeap = -1;
}

/* (0x811033C4) PLACEHOLDER name: the folders of ux0:pspemu/bgdl no task names (key 229) removed */
void BgdlManager::Bgdl_811033C4(void)
{
	paf::Dir dir;
	SceUInt32 i;

	if (dir.Open("ux0:pspemu/bgdl") != 0)
		return;

	paf::vector<paf::string> orphans;

	for (;;) {
		paf::DirEnt ent;

		if (dir.Read(&ent) != 0 || ent.name.length() == 0)
			break;
		if (sce_paf_strcmp(ent.name.c_str(), ".") == 0 ||
		    sce_paf_strcmp(ent.name.c_str(), "..") == 0)
			continue;

		paf::string path("ux0:pspemu/bgdl");
		bool found = false;

		path.append("/" + ent.name);
		for (EseTask **pos = m_tasks.m_begin; pos != m_tasks.m_end; pos++) {
			paf::string folder("");

			if (*pos != NULL && Bgdl_8112278A((*pos)->id, 229, &folder) == 0 &&
			    path == folder) {
				found = true;
				break;
			}
		}
		if (!found)
			orphans.push_back(paf::string(path.c_str()));
	}
	if (orphans.size() != 0) {
		BgdlGamePluginLock lock(true);

		for (i = 0; i < orphans.size(); i++)
			Bgdl_81112112(&orphans[i], true);
	}
}

/* (0x81103722) the run status and the result kept and written (keys 101 and 104) */
int EseTask::setResult(int status, int taskResult)
{
	BgdlEvent event;
	int ret;

	m_ParamCache.runStatus = status;
	result = taskResult;
	{
		EseParam param(id);

		ret = param.openWrite();
		if (ret != 0)
			return ret;
		ret = param.write(101, &status, sizeof(status));
		if (ret != 0)
			return ret;
		ret = param.write(104, &taskResult, sizeof(taskResult));
		if (ret != 0)
			return ret;
		ret = param.commit();
		if (ret != 0)
			return ret;
	}
	event.type = 4;
	event.id = id;
	event.param = m_ParamCache;
	event.result = result;
	event.reserved = 0;
	event.value = value;
	s_manager->NotifyListeners(&event);
	return 0;
}

/*
 * (0x81103834) a device's folders made ("bgdl/w", "bgdl/t"; all removed first when clean), its
 * tasks loaded (32 at most) and its next task id set
 */
int BgdlManager::prepare_device(const char *device, bool clean, int quota)
{
	SceUInt32 maxId;
	int workId;
	int ret;
	int i;

	if (device == NULL)
		return BGDL_ERROR_INVALID_ARGUMENT;

	BgdlTaskPath taskPath;
	string path;

	{
		BgdlScopeTimer timer("prepare_device");

		workId = -1;
		if (sce_paf_strcmp(device, "ur0:") == 0)
			workId = -2;
		path = device;
		path += "bgdl";
		if (clean == true) {
			ret = Dir::RemoveRecursive(path.c_str());
			if (ret != 0)
				return ret;
		}
		path = device;
		path += "bgdl/w";
		if (BgdlTaskPath::Exists(path.c_str())) {
			if (taskPath.initialize(device, workId, true) == 0 && taskPath.Exists())
				Bgdl_811120A0(&taskPath, true);
		} else {
			ret = Bgdl_811121FE(path.c_str(), 6);
			if (ret != 0)
				return ret;
		}
		path = device;
		path += "bgdl/t";
		if (BgdlTaskPath::Exists(path.c_str())) {
			if (taskPath.initialize(device, workId, false) == 0 && taskPath.Exists())
				Bgdl_811120A0(&taskPath, true);
		} else {
			ret = Bgdl_811121FE(path.c_str(), 6);
			if (ret != 0)
				return ret;
		}
	}
	{
		BgdlScopeTimer timer("prepare_device");
		const char *folder;
		Dir dir;

		maxId = 0;
		if (workId == -2)
			maxId = 0x80000000;
		folder = path.c_str();
		if (dir.Open(folder) == 0) {
			for (;;) {
				DirEnt ent;
				const char *name;
				SceUInt32 id;
				EseTask *task;

				if (dir.Read(&ent) != 0 || ent.name.length() == 0)
					break;
				if (sce_paf_strcmp(ent.name.c_str(), ".") == 0 ||
				    sce_paf_strcmp(ent.name.c_str(), "..") == 0)
					continue;
				/* a task's folder is its id, 8 hex digits */
				name = ent.name.c_str();
				if (name == NULL || sce_paf_strlen(name) != 8)
					continue;
				for (i = 0; i < 8; i++) {
					if ((sce_paf_look_ctype_table(name[i]) & 0x44) == 0)
						break;
				}
				if (i < 8)
					continue;
				id = sce_paf_strtoul(name, NULL, 16);
				if (maxId < id)
					maxId = id;
				task = new EseTask(id);
				if (task == NULL)
					break;
				if (task->Load() != 0) {
					delete task;
					Bgdl_8111212C(folder, ent.name.c_str());
					continue;
				}
				if (task->m_ParamCache.runStatus == 2 &&
				    task->setResult(0, BGDL_ERROR_INTERRUPTED) != 0) {
					delete task;
					continue;
				}
				if (task->m_ParamCache.key110) {
					delete task;
					Bgdl_8111212C(folder, ent.name.c_str());
					Bgdl_811137F6(false);
					continue;
				}
				if (m_tasks.insert(task) != 0) {
					delete task;
					Bgdl_8111212C(folder, ent.name.c_str());
					continue;
				}
				if ((SceSize)(m_tasks.m_end - m_tasks.m_begin) >= 32)
					break;
			}
		}
	}
	if (workId == -2)
		m_nextInternalId = maxId;
	else
		m_nextId = maxId;
	if (!clean) {
		if (workId == -2) {
			if (quota != 0 && quota != -2 && m_nextInternalId < (SceUInt32)quota)
				m_nextInternalId = quota;
		} else if (quota != 0 && quota != -1 && m_nextId < (SceUInt32)quota) {
			m_nextId = quota;
		}
	}
	return 0;
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
