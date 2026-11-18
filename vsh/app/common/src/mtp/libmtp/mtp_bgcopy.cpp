/*
 * The background copy: the copies NPXS10100, the background copy service, runs for the
 * host while nothing else uses the connection. The service registers its entry points
 * (SceLibMtp_67DB4C13); each copy it starts is an import the service manager keeps by its
 * ID, the current one taking the progress the service reports and the data it keeps in
 * files. An idle timer says whether the connection has been unused for ten seconds.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <std/string>
#include <std/stdio.h>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include "mtp_internal.h"

using paf::common::SharedPtr;
using paf::job::JobItem;
using paf::job::JobQueue;

/* the connection is idle after ten seconds without a task */
#define MTP_BGCOPY_IDLE_TIME		10000000

/* the idle timer's event: its time has come */
#define MTP_BGCOPY_TIMER_EVENT		0x8000
#define MTP_BGCOPY_TIMER_EVENT_HANDLER	1

/* what the service is asked to do */
#define MTP_BGCOPY_COMMAND_IMPORT	7
#define MTP_BGCOPY_REQUEST_FROM_HOST	0x2

/* the files the service's data is kept in */
#define MTP_BGCOPY_DATA_FILE		"bg_copy_data_%04d"
#define MTP_BGCOPY_DATA_FILE_EXT	".bin"

/* a request to the background copy service (its first entry point) */
struct MtpBgCopyServiceRequest {
	int command;			/* MTP_BGCOPY_COMMAND_* */
	SceUInt32 flags;		/* MTP_BGCOPY_REQUEST_* */
	int reserved;
	int kind;			/* the task's type, from 0 */
	SceUInt64 size;
	char name[257];
	char path[256];
	char padding[7];
};

/* a notice to the service (its own entry point) */
struct MtpBgCopyNotice {
	SceUInt8 reserved[4];
	int zero;
	int arg1;
	int arg2;
};

/* the service's entry points, as the shell registers them */
typedef int (*MtpBgCopyNotifyFunc)(MtpBgCopyNotice *notice);
typedef int (*MtpBgCopyRequestFunc)(MtpBgCopyServiceRequest *request, SceUInt32 *id);
typedef int (*MtpBgCopyIdFunc)(SceUInt32 id);
typedef int (*MtpBgCopyIdleFunc)(void);
typedef paf::string (*MtpBgCopyPathFunc)(SceUInt32 id);

#define MTP_BGCOPY_FUNC_NOTIFY		0
#define MTP_BGCOPY_FUNC_REQUEST		1
#define MTP_BGCOPY_FUNC_CANCEL		2
#define MTP_BGCOPY_FUNC_END		3
#define MTP_BGCOPY_FUNC_IS_IDLE		4
#define MTP_BGCOPY_FUNC_GET_PATH	5

#define MTP_BGCOPY_FUNC(index, type)	((type)g_bgCopyServiceFuncs.func[index])

/* waits for the idle timer: expired once its event has come */
class MtpBgCopyIdleTimerJob : public JobItem {
public:
	MtpBgCopyIdleTimerJob() : JobItem("SceMtpBgCopyIdleTimer", NULL)
	{
		m_expired = false;
		m_timer = -1;
	}

	virtual ~MtpBgCopyIdleTimerJob()
	{
		if (m_timer >= 0) {
			sceKernelDeleteTimer(m_timer);
			m_timer = -1;
		}
	}

	virtual void Cancel()
	{
		sceKernelCancelEvent(m_timer, NULL);
		sceKernelDeleteTimer(m_timer);
		m_timer = -1;
	}

	virtual int Run()
	{
		SceUInt32 pattern;
		int ret;

		m_expired = false;
		pattern = 0;
		for (;;) {
			ret = sceKernelWaitEvent(m_timer, MTP_BGCOPY_TIMER_EVENT, &pattern, NULL,
						 NULL);
			if (ret < 0)
				break;
			m_expired = true;
		}
		return ret;
	}

	virtual void Finish(int result)
	{
		if (m_timer >= 0) {
			sceKernelDeleteTimer(m_timer);
			m_timer = -1;
		}
	}

	SceUID m_timer;
	bool m_expired;
};

/* the idle timer's job and the queue it runs on */
class MtpBgCopyIdleTimer {
public:
	MtpBgCopyIdleTimer() : m_job(), m_queue(NULL) {}

	virtual ~MtpBgCopyIdleTimer()
	{
		if (m_queue != NULL) {
			delete m_queue;
			m_queue = NULL;
		}
	}

	SharedPtr<JobItem> m_job;
	JobQueue *m_queue;
};

static MtpBgCopyIdleTimer *s_idleTimer;

BgCopySrvMgr g_bgCopySrvMgr;

void mtpBgCopySetCurrentTask(SceUInt32 id)
{
	g_bgCopySrvMgr.setCurrentId(id);
}

int mtpBgCopyIdleTimerInitialize(void)
{
	MtpBgCopyIdleTimer *timer;
	MtpBgCopyIdleTimerJob *job;
	SceKernelSysClock interval;
	int ret;

	timer = new MtpBgCopyIdleTimer();
	s_idleTimer = timer;
	if (timer == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	if (timer->m_queue == NULL)
		timer->m_queue = new JobQueue("SceMtpBgCopyIdleTimerQueue", NULL);
	if (timer->m_queue == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	job = new MtpBgCopyIdleTimerJob();
	if (job == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	job->m_timer = sceKernelCreateTimer("SceMtpBgCopyIdleTimer", 0, NULL);
	if (job->m_timer < 0) {
		ret = job->m_timer;
	} else {
		interval.quad = MTP_BGCOPY_IDLE_TIME;
		ret = sceKernelSetTimerEvent(job->m_timer, MTP_BGCOPY_TIMER_EVENT_HANDLER,
					     &interval, 1);
		if (ret >= 0)
			ret = sceKernelStartTimer(job->m_timer);
	}
	if (ret < 0) {
		delete job;
		return ret;
	}
	timer->m_job.reset(job);
	timer->m_queue->Enqueue(timer->m_job);
	return 0;
}

void mtpBgCopyIdleTimerTerminate(void)
{
	MtpBgCopyIdleTimer *timer = s_idleTimer;
	MtpBgCopyIdleTimerJob *job;

	if (timer != NULL) {
		job = static_cast<MtpBgCopyIdleTimerJob *>(timer->m_job.get());
		if (job != NULL) {
			sceKernelCancelEvent(job->m_timer, NULL);
			sceKernelDeleteTimer(job->m_timer);
			job->m_timer = -1;
		}
		if (timer->m_queue != NULL) {
			timer->m_queue->WaitEmpty();
			if (timer->m_queue != NULL)
				delete timer->m_queue;
			timer->m_queue = NULL;
		}
	}
	if (s_idleTimer != NULL)
		delete s_idleTimer;
	s_idleTimer = NULL;
}

/* the idle time starts again, from the last whole interval */
int mtpBgCopyRestartIdleTimer(void)
{
	MtpBgCopyIdleTimerJob *job;
	SceUInt64 time;
	int ret;

	if (s_idleTimer == NULL)
		return 0;
	job = static_cast<MtpBgCopyIdleTimerJob *>(s_idleTimer->m_job.get());
	ret = 0;
	if (job != NULL) {
		job->m_expired = false;
		sceKernelStopTimer(job->m_timer);
		time = sceKernelGetTimerTimeWide(job->m_timer);
		ret = (int)sceKernelSetTimerTimeWide(job->m_timer,
			time / MTP_BGCOPY_IDLE_TIME * MTP_BGCOPY_IDLE_TIME);
		if (ret >= 0)
			sceKernelStartTimer(job->m_timer);
	}
	return ret;
}

int mtpBgCopyIsIdle(void)
{
	MtpBgCopyIdleTimerJob *job;

	if (s_idleTimer == NULL)
		return 0;
	job = static_cast<MtpBgCopyIdleTimerJob *>(s_idleTimer->m_job.get());
	if (job == NULL)
		return 0;
	return job->m_expired;
}

int mtpExportPauseSource(void *data)
{
	return g_bgCopySrvMgr.setPauseSource(data);
}

int mtpExportPauseTarget(void *data)
{
	return g_bgCopySrvMgr.setPauseTarget(data);
}

int mtpBgCopySetRestart(int restart)
{
	return g_bgCopySrvMgr.setRestart(restart != 0);
}

void *mtpExportGetPauseSource(void)
{
	return g_bgCopySrvMgr.getPauseSource();
}

void *mtpExportGetPauseTarget(void)
{
	return g_bgCopySrvMgr.getPauseTarget();
}

void mtpBgCopySetTaskPath(const paf::string *path)
{
	g_bgCopySrvMgr.setPath(*path);
}

int mtpBgCopyGetTaskInfo(SceUInt32 *id, int *type, void *path)
{
	*id = g_bgCopySrvMgr.getCurrentId();
	*type = g_bgCopySrvMgr.getType();
	g_bgCopySrvMgr.getPath((paf::string *)path);
	return 0;
}

/* the service starts the copy; the new import becomes the current one */
int mtpBgCopyRequestTask(const MtpBgCopyTask *task, void *path)
{
	MtpBgCopyServiceRequest request;
	SceUInt32 id;
	int ret;

	if (MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_REQUEST, MtpBgCopyRequestFunc) == NULL)
		return SCE_MTP_ERROR_DEVICE;
	sce_paf_memset(&request, 0, sizeof(request));
	sce_paf_memset(&id, 0, sizeof(id));
	request.command = MTP_BGCOPY_COMMAND_IMPORT;
	sce_paf_memset(&request.flags, 0, sizeof(request.flags));
	request.flags |= MTP_BGCOPY_REQUEST_FROM_HOST;
	sce_paf_strncpy(request.name, task->name, sizeof(request.name));
	sce_paf_memset(request.path, 0, sizeof(request.path));
	request.size = task->size;
	switch (task->type) {
	case 1:
		request.kind = 0;
		break;
	case 2:
		request.kind = 1;
		break;
	case 3:
		request.kind = 2;
		break;
	case 4:
		request.kind = 3;
		break;
	}
	ret = MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_REQUEST, MtpBgCopyRequestFunc)(&request, &id);
	if (ret != 0)
		return ret;

	BgCopyImportContext context;

	context.status.index = 0;
	context.status.total = 0;
	context.status.progress = 0;
	context.status.remaining = 0;
	context.status.result = 0;
	context.id = id;
	context.type = task->type;
	g_bgCopySrvMgr.addContext(context);
	g_bgCopySrvMgr.setCurrentId(id);
	if (MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_GET_PATH, MtpBgCopyPathFunc) != NULL)
		*(paf::string *)path =
			MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_GET_PATH, MtpBgCopyPathFunc)(id);
	return ret;
}

int mtpBgCopyEndTask(void)
{
	if (MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_END, MtpBgCopyIdFunc) == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_END, MtpBgCopyIdFunc)(
		g_bgCopySrvMgr.getCurrentId());
}

int mtpBgCopySetTaskInfo(const void *status)
{
	g_bgCopySrvMgr.setStatus((const MtpBgCopyTaskStatus *)status);
	return 0;
}

int mtpBgCopyGetTaskStatus(MtpBgCopyTaskStatus *status)
{
	return g_bgCopySrvMgr.getStatus(status);
}

int mtpBgCopyClearTaskStatus(void)
{
	return g_bgCopySrvMgr.clearStatus();
}

int mtpBgCopyNotifyService(int arg1, int arg2)
{
	MtpBgCopyNotice notice;

	if (MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_NOTIFY, MtpBgCopyNotifyFunc) == NULL)
		return SCE_MTP_ERROR_DEVICE;
	notice.arg1 = arg1;
	notice.arg2 = arg2;
	notice.zero = 0;
	return MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_NOTIFY, MtpBgCopyNotifyFunc)(&notice);
}

int mtpBgCopyCancelTask(SceUInt32 id)
{
	if (MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_CANCEL, MtpBgCopyIdFunc) == NULL)
		return SCE_MTP_ERROR_DEVICE;
	g_bgCopySrvMgr.removeContext(id);
	return MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_CANCEL, MtpBgCopyIdFunc)(id);
}

static int cancelService(SceUInt32 id);

/* every import not to be started again is canceled */
int mtpBgCopyCancelAll(void)
{
	g_bgCopySrvMgr.forEachRestart(cancelService);
	return 0;
}

int mtpBgCopyIsServiceIdle(void)
{
	if (MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_IS_IDLE, MtpBgCopyIdleFunc) == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_IS_IDLE, MtpBgCopyIdleFunc)();
}

/* the current import's data, kept in its directory as bg_copy_data_<index>.bin */
int mtpBgCopyWriteData(const void *data, SceSize size, int index)
{
	paf::string dir;
	paf::string file;
	char name[256];
	SceUInt32 id;
	SceUID fd;
	int ret;

	id = g_bgCopySrvMgr.getCurrentId();
	if (MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_GET_PATH, MtpBgCopyPathFunc) == NULL)
		return SCE_MTP_ERROR_DEVICE;
	dir = MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_GET_PATH, MtpBgCopyPathFunc)(id);
	sce_paf_snprintf(name, sizeof(name), MTP_BGCOPY_DATA_FILE, index);
	file = dir + "/" + name + MTP_BGCOPY_DATA_FILE_EXT;
	fd = sceIoOpen(file.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0006);
	if (fd < 0)
		return fd;
	ret = sceIoWrite(fd, data, size);
	sceIoClose(fd);
	return ret;
}

int mtpBgCopyReadData(void *data, SceUInt32 offset, SceSize size, int index, SceSize *read)
{
	paf::string dir;
	paf::string file;
	char name[256];
	SceIoStat stat;
	SceUID fd;
	SceOff pos;
	int ret;

	g_bgCopySrvMgr.getPath(&dir);
	sce_paf_snprintf(name, sizeof(name), MTP_BGCOPY_DATA_FILE, index);
	file = dir + "/" + name + MTP_BGCOPY_DATA_FILE_EXT;
	ret = sceIoGetstat(file.c_str(), &stat);
	if (ret != 0)
		return ret;
	if (read != NULL)
		*read = (SceSize)stat.st_size;
	if (size == 0)
		return 0;
	if (offset > stat.st_size)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	fd = sceIoOpen(file.c_str(), SCE_O_RDONLY, 0006);
	if (fd < 0)
		return fd;
	pos = sceIoLseek(fd, offset, SCE_SEEK_SET);
	if (pos < 0) {
		sceIoClose(fd);
		return (int)pos;
	}
	ret = sceIoRead(fd, data, size);
	sceIoClose(fd);
	return ret;
}

static int cancelService(SceUInt32 id)
{
	if (MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_CANCEL, MtpBgCopyIdFunc) == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return MTP_BGCOPY_FUNC(MTP_BGCOPY_FUNC_CANCEL, MtpBgCopyIdFunc)(id);
}
