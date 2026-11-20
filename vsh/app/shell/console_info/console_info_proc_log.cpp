/*
 * console_info_proc_log.cpp: the media information requests and the process event log they fill
 * (0x810F302C-0x810F34B2). A request becomes a SceMediaInfoWriterJob on the sender's queue; the
 * job adds the request's event to ur0:ci/m.log, a ring of 16384 events whose next index ur0:ci/m.cnt
 * keeps. The count is -1 while the shell runs, so a log a crash left unfinished is cleared at the
 * next start. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <std/string.h>
#include <file/localfile.h>
#include "shell.h"
#include "console_info.h"

using namespace paf;

/* the events the log holds before it wraps */
#define CONSOLE_INFO_PROC_LOG_SIZE	0x4000

/* PLACEHOLDER: ur0:ci/file's flag set when the log wrapped or was cleared */
#define CONSOLE_INFO_PROC_LOG_WRAPPED	3

/* the requests' types: the shell started, a title ended, the shell ended (the shell's own); a
   title's start, its suspend, its resume, its end */
#define SHELL_MEDIA_INFO_START		1
#define SHELL_MEDIA_INFO_END		2
#define SHELL_MEDIA_INFO_BOOT		3
#define SHELL_MEDIA_INFO_TITLE		4
#define SHELL_MEDIA_INFO_SUSPEND	5
#define SHELL_MEDIA_INFO_RESUME		6
#define SHELL_MEDIA_INFO_TITLE_END	7
#define SHELL_MEDIA_INFO_TITLE_EXIT	8

/* (0x815590D0, 0x815590B8) the log, and its next index */
static ConsoleInfoProcLog s_log;
static ConsoleInfoFile s_logCount(CONSOLE_INFO_PROC_LOG_COUNT, sizeof(int));

/* (0x810F302C) */
int consoleInfoMediaInfoCheck(const ShellMediaInfoRequest *request)
{
	switch (request->type) {
	case SHELL_MEDIA_INFO_START:
	case SHELL_MEDIA_INFO_END:
	case SHELL_MEDIA_INFO_BOOT:
		return 0;
	case SHELL_MEDIA_INFO_TITLE:
		break;
	case SHELL_MEDIA_INFO_SUSPEND:
	case SHELL_MEDIA_INFO_RESUME:
		if (request->attr[0] == 0)
			return SCE_ERROR_ERRNO_EINVAL;
		if (request->attr[1] != 1 && request->attr[1] != 2 && request->attr[1] != 3)
			return SCE_ERROR_ERRNO_EINVAL;
		/* fall through */
	case SHELL_MEDIA_INFO_TITLE_END:
	case SHELL_MEDIA_INFO_TITLE_EXIT:
		if (request->attr[2] != 1 && request->attr[2] != 2 &&
		    request->type != SHELL_MEDIA_INFO_START)
			return SCE_ERROR_ERRNO_EINVAL;
		break;
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (request->titleId[0] == '\0')
		return SCE_ERROR_ERRNO_EINVAL;
	return 0;
}

/* (0x810F3098) the request's job queued, or run at once when the shell ends while the queue is
   busy; a caller that waits waits until the job queued has run */
void shellMediaInfoWrite(const ShellMediaInfoRequest *request, bool wait)
{
	if (consoleInfoMediaInfoCheck(request) != 0)
		return;

	bool busy = false;

	if (consoleInfoSenderGetQueue(&busy) == NULL)
		return;

	MediaInfoWriterJob *job = new MediaInfoWriterJob(request, false);

	if (job == NULL)
		return;
	if (busy && request->type == SHELL_MEDIA_INFO_END) {
		job->Run();
		delete job;
		return;
	}

	common::SharedPtr<job::JobItem> item(job);

	if (consoleInfoSenderGetQueue(&busy)->Enqueue(item) != 0)
		return;
	if (wait)
		job->Wait();
}

/* (0x810F31F8) */
ConsoleInfoProcLog::ConsoleInfoProcLog()
	: ConsoleInfoFile(CONSOLE_INFO_PROC_LOG, sizeof(ConsoleInfoMiEvent)), m_next(0)
{
}

/* (0x810F321E) */
int consoleInfoProcLogClear(ConsoleInfoProcLog *log)
{
	s_logCount.Remove();
	log->Remove();
	log->m_next = 0;
	return consoleInfoFileWriteFlag(CONSOLE_INFO_PROC_LOG_WRAPPED, 0, 0);
}

/* (0x810F3248) an event written at the log's next index; the shell's start marks the count
   unfinished, its end writes it */
void consoleInfoProcLogAdd(ConsoleInfoProcLog *log, const ConsoleInfoMiEvent *event, int mode)
{
	if (event->state == SHELL_MEDIA_INFO_START) {
		log->m_next = 0;
		if (s_logCount.Read(0, &log->m_next, 1) != 0)
			log->m_next = 0;

		int unfinished = -1;

		if (log->m_next == -1) {
			consoleInfoProcLogClear(log);
			consoleInfoFileWriteFlag(CONSOLE_INFO_PROC_LOG_WRAPPED, 1, 0);
		}
		if (s_logCount.Write(0, &unfinished, 2) != 0)
			consoleInfoProcLogClear(log);
	}
	if (log->m_next >= CONSOLE_INFO_PROC_LOG_SIZE) {
		log->m_next = 0;
		consoleInfoFileWriteFlag(CONSOLE_INFO_PROC_LOG_WRAPPED, 1, 0);
	}
	if (log->Write(log->m_next, event, mode) == 0) {
		log->m_next++;
		if (event->state == SHELL_MEDIA_INFO_END)
			s_logCount.Write(0, &log->m_next, 2);
	}
}

/* (0x810F334E) the events in order: those after the next index (written before the log
   wrapped), then those before it */
int consoleInfoProcLogForEach(ConsoleInfoProcLog *log,
			      int (*func)(const ConsoleInfoMiEvent *event, void *arg), void *arg)
{
	ConsoleInfoMiEvent event;
	char path[CONSOLE_INFO_FILE_PATH_SIZE];
	int ret = 0;
	int first = 0;

	log->GetPath(path);

	SceInt64 size = LocalFile::GetFileSize(path);
	int index = log->m_next;
	unsigned int offset = index * sizeof(ConsoleInfoMiEvent);
	SceInt64 start = offset;

	if (size > start) {
		while (offset < size) {
			ret = log->Read(index, &event, 1);
			if (ret != 0)
				break;
			ret = func(&event, arg);
			if (ret != 0)
				break;
			offset += sizeof(ConsoleInfoMiEvent);
			index++;
		}
	}
	if (ret == 0) {
		for (index = first; index < log->m_next; index++) {
			ret = log->Read(index, &event, 1);
			if (ret != 0)
				break;
			ret = func(&event, arg);
			if (ret != 0)
				break;
		}
	}
	log->Read(0, &event, 2);
	return ret;
}

/* (0x810F3452) */
ConsoleInfoProcLog *consoleInfoGetProcLog(void)
{
	return &s_log;
}
